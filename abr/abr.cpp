/*
 *   Copyright 2022 RDK Management
 *
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
 */
// #define DEBUG_ENABLED 1
/***************************************************
 * @file abr.cpp
 * @brief Handles operations on Hybrid ABR functionalities
 ***************************************************/

#include "abr.h"
#include <vector>
#include <map>
#include <string>
#include <cstdio>
#include <cmath>
#include <chrono>
#include <stdarg.h>
#include <sys/time.h>
#include <algorithm>
#include "AampLogManager.h"

#define DEFAULT_ABR_CHUNK_CACHE_LENGTH	10					/**< Default ABR chunk cache length */
#define DEFAULT_ABR_ELAPSED_MILLIS_FOR_ESTIMATE	100			/**< Duration(ms) to check Chunk Speed */
#define MAX_LOW_LATENCY_DASH_ABR_SPEEDSTORE_SIZE 10

ABRManager::AampAbrConfig eAAMPAbrConfig;

long ABRManager::mPersistBandwidth = 0;
long long ABRManager::mPersistBandwidthUpdatedTime = 0;

/**
 * @brief Get initial profile index, choose the medium profile or
 * the profile whose bitrate >= the default bitrate.
 */
int ABRManager::getInitialProfileIndex(bool chooseMediumProfile, const std::string& periodId) {
	
	std::lock_guard<std::mutex> lock(mProfileLock);
	int profileCount = getProfileCount();
	int desiredProfileIndex = INVALID_PROFILE;
	
	
	if( profileCount==0 )
	{
		AAMPLOG_WARN( "No profiles found" );
		return desiredProfileIndex;
	}
	
	if (chooseMediumProfile && profileCount > 1) {
		// get the mid profile from the sorted list
		SortedBWProfileListIter iter = mSortedBWProfileList[periodId].begin();
		std::advance(iter, static_cast<int>(mSortedBWProfileList[periodId].size() / 2));
		desiredProfileIndex = iter->second;
	} else {
		SortedBWProfileListIter iter;
		desiredProfileIndex = mSortedBWProfileList[periodId].begin()->second;
		for (iter = mSortedBWProfileList[periodId].begin(); iter != mSortedBWProfileList[periodId].end(); ++iter) {
			if (iter->first > mDefaultInitBitrate) {
				break;
			}
			// Choose the profile whose bitrate < default bitrate
			desiredProfileIndex = iter->second;
		}
	}
	if (INVALID_PROFILE == desiredProfileIndex) {
		desiredProfileIndex = mSortedBWProfileList[periodId].begin()->second;
		AAMPLOG_WARN("Got invalid profile index, choose the first index = %d and profileCount = %d and defaultBitrate = %ld", desiredProfileIndex, profileCount, mDefaultInitBitrate);
	} else {
		AAMPLOG_MIL("Get initial profile index = %d, bitrate = %ld and defaultBitrate = %ld", desiredProfileIndex, mProfiles[desiredProfileIndex].bandwidthBitsPerSecond, mDefaultInitBitrate);
	}
	return desiredProfileIndex;
}

/**
 * @brief Update the lowest / desired profile index
 *    by the profile info.
 */
void ABRManager::updateProfile() {
	/**
	 * @brief A temporary structure of iframe track info
	 */
	struct IframeTrackInfo {
		long bandwidth;
		int idx;
	};
	
	std::unique_lock<std::mutex> lock(mProfileLock);
	int profileCount = getProfileCount();
	
	// todo: replace with Use std::vector<IframeTrackInfo>
	struct IframeTrackInfo *iframeTrackInfo = new struct IframeTrackInfo[profileCount];
	bool is4K = false;
	
	int iframeTrackIdx = -1;
	// Construct iframe track info
	for (int i = 0; i < profileCount; i++) {
		if (mProfiles[i].isIframeTrack) {
			iframeTrackIdx++;
			iframeTrackInfo[iframeTrackIdx].bandwidth = mProfiles[i].bandwidthBitsPerSecond;
			iframeTrackInfo[iframeTrackIdx].idx = i;
		}
	}
	lock.unlock();
	
	// Exists iframe track
	if(iframeTrackIdx >= 0) {
		// Sort the iframe track array by bandwidth ascendingly
		// TODO: use std::sort
		for (int i = 0; i < iframeTrackIdx; i++) {
			for (int j = 0; j < iframeTrackIdx - i; j++) {
				if (iframeTrackInfo[j].bandwidth > iframeTrackInfo[j+1].bandwidth) {
					struct IframeTrackInfo temp = iframeTrackInfo[j];
					iframeTrackInfo[j] = iframeTrackInfo[j+1];
					iframeTrackInfo[j+1] = temp;
				}
			}
		}
		
		// Exist 4K video?
		int highestProfileIdx = iframeTrackInfo[iframeTrackIdx].idx;
		if(mProfiles[highestProfileIdx].height > HEIGHT_4K
		   || mProfiles[highestProfileIdx].width > WIDTH_4K) {
			is4K = true;
		}
		
		if (mDefaultIframeBitrate > 0) {
			mLowestIframeProfile = mDesiredIframeProfile = iframeTrackInfo[0].idx;
			for (int cnt = 0; cnt <= iframeTrackIdx; cnt++) {
				// find the track less than default bw set, apply to both desired and lower ( for all speed of trick)
				if(iframeTrackInfo[cnt].bandwidth >= mDefaultIframeBitrate) {
					break;
				}
				mDesiredIframeProfile = iframeTrackInfo[cnt].idx;
			}
		} else {
			if(is4K) {
				// Get the default profile of 4k video, apply same bandwidth of video to iframe also
				int desiredProfileIndexNonIframe = profileCount / 2;
				int desiredProfileNonIframeBW = (int)mProfiles[desiredProfileIndexNonIframe].bandwidthBitsPerSecond ;
				mDesiredIframeProfile = mLowestIframeProfile = 0;
				for (int cnt = 0; cnt <= iframeTrackIdx; cnt++) {
					// if bandwidth matches, apply to both desired and lower ( for all speed of trick)
					if(iframeTrackInfo[cnt].bandwidth == desiredProfileNonIframeBW) {
						mDesiredIframeProfile = mLowestIframeProfile = iframeTrackInfo[cnt].idx;
						break;
					}
				}
				// if matching bandwidth not found with video, then pick the middle profile for iframe
				if((!mDesiredIframeProfile) && (iframeTrackIdx >= 1)) {
					int desiredTrackIdx = (int) (iframeTrackIdx / 2) + (iframeTrackIdx % 2);
					mDesiredIframeProfile = mLowestIframeProfile = iframeTrackInfo[desiredTrackIdx].idx;
				}
			} else {
				//Keeping old logic for non 4K streams
				for (int cnt = 0; cnt <= iframeTrackIdx; cnt++) {
					if (mLowestIframeProfile == INVALID_PROFILE) {
						// first pick the lowest profile available
						mLowestIframeProfile = mDesiredIframeProfile = iframeTrackInfo[cnt].idx;
						continue;
					}
					// if more profiles available, stored second best to desired profile
					mDesiredIframeProfile = iframeTrackInfo[cnt].idx;
					break; // select first-advertised
				}
			}
		}
	}
	delete[] iframeTrackInfo;
	
#if defined(DEBUG_ENABLED)
	AAMPLOG_MIL("Update profile info, mDesiredIframeProfile = %d, mLowestIframeProfile = %d", mDesiredIframeProfile, mLowestIframeProfile);
#endif
}

/**
 *  @brief According to the given bandwidth, return the best matched
 *  profile index.
 */
int ABRManager::getBestMatchedProfileIndexByBandWidth(int bandwidth) {
	
	std::lock_guard<std::mutex> lock(mProfileLock);
	// a) Check if network bandwidth changed from starting bandwidth
	// b) Check if netwwork bandwidth is different from persisted bandwidth( needed for first time reporting)
	// find the profile for the newbandwidth
	int desiredProfileIndex = 0;
	int profileCount = getProfileCount();
	for (int i = 0; i < profileCount; i++) {
		const ProfileInfo& profile = mProfiles[i];
		if (!profile.isIframeTrack) {
			if (profile.bandwidthBitsPerSecond == bandwidth) {
				// Good case, most manifest url will have same bandwidth in fragment file with configured profile bandwidth
				desiredProfileIndex = i;
				break;
			} else if (profile.bandwidthBitsPerSecond < bandwidth) {
				// fragment file name bandwidth doesn't match the profile bandwidth, will be always less
				if((i+1) == profileCount) {
					desiredProfileIndex = i;
					break;
				}
				else
					desiredProfileIndex = (i + 1);
			}
		}
	}
#if defined(DEBUG_ENABLED)
	AAMPLOG_MIL("Get best matched profile index = %d bitrate = %ld", desiredProfileIndex,
				(profileCount > desiredProfileIndex && desiredProfileIndex != INVALID_PROFILE) ? mProfiles[desiredProfileIndex].bandwidthBitsPerSecond : 0);
#endif
	return desiredProfileIndex;
}

/**
 *  @brief Ramp down the profile one step to get the profile index of a lower bitrate.
 */
int ABRManager::getRampedDownProfileIndex(int currentProfileIndex, const std::string& periodId) {
	
	std::lock_guard<std::mutex> lock(mProfileLock);
	// Clamp the param to avoid overflow
	int profileCount = getProfileCount();
	
	if (currentProfileIndex >= profileCount) {
		AAMPLOG_WARN("Invalid profileIndex %d exceeds the current profile count %d", currentProfileIndex, profileCount);
		currentProfileIndex--;
	}
	
	int desiredProfileIndex = currentProfileIndex;
	if (profileCount == 0) {
		AAMPLOG_WARN("No profiles found" );
		return desiredProfileIndex;
	}
	
	long currentBandwidth = mProfiles[currentProfileIndex].bandwidthBitsPerSecond;
	SortedBWProfileListIter iter = mSortedBWProfileList[periodId].find(currentBandwidth);
	if (iter == mSortedBWProfileList[periodId].end()) {
		AAMPLOG_WARN("The current bitrate %ld is not in the profile list", currentBandwidth);
		return desiredProfileIndex;
	}
	if (iter == mSortedBWProfileList[periodId].begin()) {
		desiredProfileIndex = iter->second;
	} else {
		// get the prev profile . This is sorted list, so no worry of getting wrong profile
		std::advance(iter, -1);
		desiredProfileIndex = iter->second;
	}
	
#if defined(DEBUG_ENABLED)
	AAMPLOG_MIL("Ramped down profile index = %d bitrate = %ld", desiredProfileIndex, mProfiles[desiredProfileIndex].bandwidthBitsPerSecond);
#endif
	return desiredProfileIndex;
}

/**
 *  @brief Ramp Up the profile one step to get the profile index of a upper bitrate.
 */
int ABRManager::getRampedUpProfileIndex(int currentProfileIndex, const std::string& periodId) {
	
	std::lock_guard<std::mutex> lock(mProfileLock);
	// Clamp the param to avoid overflow
	int profileCount = getProfileCount();
	int desiredProfileIndex = currentProfileIndex;
	
	if (profileCount == 0 || currentProfileIndex >= profileCount) {
		AAMPLOG_WARN("No profiles/input profile %d more than profileCount %d", currentProfileIndex, profileCount);
		return desiredProfileIndex;
	}
	
	long currentBandwidth = mProfiles[currentProfileIndex].bandwidthBitsPerSecond;
	SortedBWProfileListIter iter = mSortedBWProfileList[periodId].find(currentBandwidth);
	if (iter == mSortedBWProfileList[periodId].end()) {
		AAMPLOG_WARN("The current bitrate %ld is not in the profile list", currentBandwidth);
		return desiredProfileIndex;
	}
	
	if(std::next(iter) != mSortedBWProfileList[periodId].end())
	{
		std::advance(iter, 1);
		desiredProfileIndex = iter->second;
	}
	
#if defined(DEBUG_ENABLED)
	AAMPLOG_MIL("Ramped up profile index = %d bitrate = %ld", desiredProfileIndex, mProfiles[desiredProfileIndex].bandwidthBitsPerSecond);
#endif
	return desiredProfileIndex;
}


/**
 *  @brief Get UserData of profile
 */
int ABRManager::getUserDataOfProfile(int currentProfileIndex) {
	
	std::lock_guard<std::mutex> lock(mProfileLock);
	int userData = -1;
	int profileCount = getProfileCount();
	if (profileCount == 0 || currentProfileIndex >= profileCount) {
		AAMPLOG_WARN("No profiles/input profile %d more than profileCount %d", currentProfileIndex, profileCount);
		return userData;
	}
	userData = mProfiles[currentProfileIndex].userData;
	return userData;
}


/**
 *  @brief Check if the bitrate of currentProfileIndex reaches to the lowest.
 */
bool ABRManager::isProfileIndexBitrateLowest(int currentProfileIndex, const std::string& periodId) {
	
	std::lock_guard<std::mutex> lock(mProfileLock);
	// Clamp the param to avoid overflow
	int profileCount = getProfileCount();
	
	if (currentProfileIndex >= profileCount) {
		AAMPLOG_WARN("Invalid profileIndex %d exceeds the current profile count %d", currentProfileIndex, profileCount);
		currentProfileIndex--;
	}
	
	
	// If there is no profiles list, then it means `currentProfileIndex` always reaches to
	// the lowest.
	if (profileCount == 0) {
		AAMPLOG_WARN( "No profiles found" );
		return true;
	}
	
	long currentBandwidth = mProfiles[currentProfileIndex].bandwidthBitsPerSecond;
	SortedBWProfileListIter iter = mSortedBWProfileList[periodId].find(currentBandwidth);
	return iter == mSortedBWProfileList[periodId].begin();
}

/**
 *  @brief Do ABR by ramping bitrate up/down according to the current
 *         network status. Returns the profile index with the bitrate matched with
 *         the current bitrate.
 */
int ABRManager::getProfileIndexByBitrateRampUpOrDown(int currentProfileIndex, long currentBandwidth, long networkBandwidth, int nwConsistencyCnt, const std::string& periodId) {
	
	std::lock_guard<std::mutex> lock(mProfileLock);
	// Clamp the param to avoid overflow
	int profileCount = getProfileCount();
	
	if (currentProfileIndex >= profileCount) {
		AAMPLOG_WARN("Invalid profileIndex %d exceeds the current profile count %d", currentProfileIndex, profileCount);
		currentProfileIndex--;
	}
	
	
	int desiredProfileIndex = currentProfileIndex;
	if (networkBandwidth == -1) {
		// If the network bandwidth is not available, just reset the profile change up/down count.
#if defined(DEBUG_ENABLED)
		AAMPLOG_MIL("No network bandwidth info available, not changing profile[%d]", currentProfileIndex);
#endif
		mAbrProfileChangeUpCount = 0;
		mAbrProfileChangeDownCount = 0;
		return desiredProfileIndex;
	}
	if(networkBandwidth > currentBandwidth) {
		// if networkBandwidth > is more than current bandwidth
		SortedBWProfileListIter iter;
		SortedBWProfileListIter currIter = mSortedBWProfileList[periodId].find(currentBandwidth);
		SortedBWProfileListIter storedIter = mSortedBWProfileList[periodId].end();
		for (iter = currIter; iter != mSortedBWProfileList[periodId].end(); ++iter) {
			// This is sort List
			if (networkBandwidth >= iter->first) {
				desiredProfileIndex = iter->second;
				storedIter = iter;
			} else {
				break;
			}
		}
		
		// No need to jump one profile for one network bw increase
		if (storedIter != mSortedBWProfileList[periodId].end() && (currIter->first < storedIter->first) && std::distance(currIter, storedIter) == 1) {
			mAbrProfileChangeUpCount++;
			// if same profile holds good for next 3*2 fragments
			if (mAbrProfileChangeUpCount < nwConsistencyCnt) {
				desiredProfileIndex = currentProfileIndex;
			} else {
				mAbrProfileChangeUpCount = 0;
			}
		} else {
			mAbrProfileChangeUpCount = 0;
		}
		mAbrProfileChangeDownCount = 0;
#if defined(DEBUG_ENABLED)
		AAMPLOG_MIL("Ramp up profile index = %d, bitrate = %ld networkBandwidth = %ld", desiredProfileIndex,
					(profileCount > desiredProfileIndex && desiredProfileIndex != INVALID_PROFILE) ? mProfiles[desiredProfileIndex].bandwidthBitsPerSecond : 0, networkBandwidth);
#endif
	} else {
		// if networkBandwidth < than current bandwidth
		SortedBWProfileListRevIter revIter;
		SortedBWProfileListIter currIter = mSortedBWProfileList[periodId].find(currentBandwidth);
		SortedBWProfileListIter storedIter = mSortedBWProfileList[periodId].end();
		for (revIter = mSortedBWProfileList[periodId].rbegin(); revIter != mSortedBWProfileList[periodId].rend(); ++revIter) {
			// This is sorted List
			if (networkBandwidth >= revIter->first) {
				desiredProfileIndex = revIter->second;
				// convert from reverse iter to forward iter
				storedIter = revIter.base();
				storedIter--;
				break;
			}
		}
		
		// we didn't find a profile which can be supported in this bandwidth
		if (revIter == mSortedBWProfileList[periodId].rend()) {
			desiredProfileIndex = mSortedBWProfileList[periodId].begin()->second;
			AAMPLOG_WARN("Didn't find a profile which supports bandwidth[%ld], min bandwidth available [%ld]. Set profile to lowest!", networkBandwidth, mSortedBWProfileList[periodId].begin()->first);
		}
		
		// No need to jump one profile for small  network change
		if (storedIter != mSortedBWProfileList[periodId].end() && (currIter->first > storedIter->first) && std::distance(storedIter, currIter) == 1) {
			mAbrProfileChangeDownCount++;
			// if same profile holds good for next 3*2 fragments
			if(mAbrProfileChangeDownCount < nwConsistencyCnt) {
				desiredProfileIndex = currentProfileIndex;
			} else {
				mAbrProfileChangeDownCount = 0;
			}
		} else {
			mAbrProfileChangeDownCount = 0;
		}
		mAbrProfileChangeUpCount = 0;
#if defined(DEBUG_ENABLED)
		AAMPLOG_MIL("Ramp down profile index = %d, bitrate = %ld networkBandwidth = %ld",
					desiredProfileIndex,
					(profileCount > desiredProfileIndex && desiredProfileIndex != INVALID_PROFILE) ? mProfiles[desiredProfileIndex].bandwidthBitsPerSecond : 0, networkBandwidth);
#endif
	}
	
	if (currentProfileIndex != desiredProfileIndex) {
		AAMPLOG_MIL("currBW:%ld NwBW=%ld currProf:%d desiredProf:%d Period ID:%s",
					currentBandwidth, networkBandwidth,
					currentProfileIndex, desiredProfileIndex, periodId.c_str());
	}
	
	return desiredProfileIndex;
}

/**
 *  @brief Get bandwidth of profile
 */
long ABRManager::getBandwidthOfProfile(int profileIndex) {
	
	std::lock_guard<std::mutex> lock(mProfileLock);
	// Clamp the param to avoid overflow
	int profileCount = getProfileCount();
	
	if( profileCount == 0 )
	{
		AAMPLOG_WARN( "No profiles found" );
		return 0;
	}
	if (profileIndex >= profileCount){
		AAMPLOG_WARN("Invalid profileIndex %d exceeds the current profile count %d", profileIndex, profileCount);
		profileIndex--;
	}
	
	
	return mProfiles[profileIndex].bandwidthBitsPerSecond;
}

/**
 *  @brief Get the index of max bandwidth
 */
int ABRManager::getMaxBandwidthProfile(const std::string& periodId) {
	
	std::lock_guard<std::mutex> lock(mProfileLock);
	int profileCount = getProfileCount();
	if( profileCount==0 )
	{
		AAMPLOG_WARN( "No profiles found" );
		return 0;
	}
	return mSortedBWProfileList[periodId].size()?mSortedBWProfileList[periodId].rbegin()->second:0;
}

// Getters/Setters
/**
 * @fn getProfileCount
 *
 * @return The number of profiles
 */
int ABRManager::getProfileCount() const {
	return static_cast<int>(mProfiles.size());
}
/**
 *  @brief Get the number of profiles
 */
int ABRManager::getProfileCount() {
	std::lock_guard<std::mutex> lock(mProfileLock);
	return getProfileCount();
}

/**
 *  @brief Set the default init bitrate
 */
void ABRManager::setDefaultInitBitrate(long defaultInitBitrate) {
	mDefaultInitBitrate = defaultInitBitrate;
}


/**
 *  @brief Get the lowest iframe profile index.
 */
int ABRManager::getLowestIframeProfile() const {
	return mLowestIframeProfile;
}

/**
 *  @brief Get the desired iframe profile index.
 */
int ABRManager::getDesiredIframeProfile() const {
	return mDesiredIframeProfile;
}

/**
 *  @brief Add new profile info into the manager
 */
void ABRManager::addProfile(ABRManager::ProfileInfo profile) {
	
	std::lock_guard<std::mutex> lock(mProfileLock);
	mProfiles.push_back(profile);
	int profileCount = getProfileCount();
	int idx = profileCount - 1;
	addSortedBWProfileList(mProfiles[idx], idx);
}

/**
 * @brief Add new profile info to sorted BW list
 * @param[in] profileInfo profile info
 * @param[in] idx profile index in list
 */
void ABRManager::addSortedBWProfileList(const ABRManager::ProfileInfo &profileInfo, int idx)
{
	if (!profileInfo.isIframeTrack) {
		mSortedBWProfileList[profileInfo.periodId][profileInfo.bandwidthBitsPerSecond] = idx;
#if defined(DEBUG_ENABLED)
		AAMPLOG_MIL("Period ID: %s", profileInfo.periodId.c_str());
		AAMPLOG_MIL("bw:%ld idx:%d", profileInfo.bandwidthBitsPerSecond, idx);
#endif
	}
	
}

/**
 * @fn removeProfiles
 * @param[in] vector of profile bitrates to remove from ABR data
 * @param[in] currentProfileIndex
 * @param[in] period Id empty string by default, Period-Id of profiles
 * @return modified profileIndex
 */
int ABRManager::removeProfiles(std::vector<long> profileBPS, int currentProfileIndex, const std::string& periodId) {
	
	std::lock_guard<std::mutex> lock(mProfileLock);
	int modifiedProfileIndex = INVALID_PROFILE;
	int profileCount = getProfileCount();
	
	if( profileCount == 0 )
	{
		AAMPLOG_WARN( "No profiles found" );
		return modifiedProfileIndex;
	}
	
	if (currentProfileIndex >= profileCount) {
		AAMPLOG_WARN("Invalid profileIndex %d exceeds the current profile count %d", currentProfileIndex, profileCount);
		currentProfileIndex--;
	}
	long currentBandwidth = mProfiles[currentProfileIndex].bandwidthBitsPerSecond;
	for (auto &profileBWToRemove : profileBPS)
	{
		for(auto profile = mProfiles.begin(); profile != mProfiles.end();)
		{
			if(profile->periodId == periodId && profile->bandwidthBitsPerSecond == profileBWToRemove) {
				profile = mProfiles.erase(profile);
				// We expect only unique BW entries
				break;
			} else {
				profile++;
			}
		}
	}
#if defined(DEBUG_ENABLED)
	AAMPLOG_MIL("profileCount after removing profiles orig:%d and new:%d", profileCount, getProfileCount());
#endif
	
	mSortedBWProfileList.clear();
	// Get new profile count
	profileCount = getProfileCount();
	for(int idx = 0; idx < profileCount; idx++) {
		addSortedBWProfileList(mProfiles[idx], idx);
		if(currentBandwidth == mProfiles[idx].bandwidthBitsPerSecond) {
			modifiedProfileIndex = idx;
		}
	}
	
	if (modifiedProfileIndex == INVALID_PROFILE) {
		AAMPLOG_MIL("Unable to find the currentProfileIndex in the modified profiles, currentProfileIndex:%d currBW:%ld period ID:%s", currentProfileIndex, currentBandwidth, periodId.c_str());
	}
	return modifiedProfileIndex;
}

/**
 *  @brief Clear profiles
 */
void ABRManager::clearProfiles() {
	
	std::lock_guard<std::mutex> lock(mProfileLock);
	mProfiles.clear();
	if (mSortedBWProfileList.size()) {
		mSortedBWProfileList.erase(mSortedBWProfileList.begin(),mSortedBWProfileList.end());
		mSortedBWProfileList.clear();
	}
}

/**
 *  @brief Set the default iframe bitrate
 */
void ABRManager::setDefaultIframeBitrate(long defaultIframeBitrate) {
	mDefaultIframeBitrate = defaultIframeBitrate;
}

/**
 *  @brief Get the lowest bitrate pointing index
 */
int  ABRManager::getProfileIndexForLowestBandwidth()
{
	std::lock_guard<std::mutex> lock(mProfileLock);
	int profileCount = getProfileCount();
	if( profileCount==0 )
	{
		AAMPLOG_WARN( "No profiles found" );
		return 0;
	}
	int index = 0;
	auto &profileMap = mSortedBWProfileList.begin()->second;
	if (!profileMap.empty())
	{
		index = profileMap.begin()->second;
	}
	return index;
}
/**
 *  @brief Get the best matched profile index by bandwidth using sorted list
 */
int ABRManager::getClosestProfileIndexByBandwidth( long inputBandwidth )
{
	std::lock_guard<std::mutex> lock(mProfileLock);
	// Use the first period's map
	if (!mSortedBWProfileList.empty())
	{
		int bestIdx = INVALID_PROFILE;
		auto& profileMap = mSortedBWProfileList.begin()->second;
		for (std::map<long, int>::const_iterator it = profileMap.begin(); it != profileMap.end(); ++it)
		{
			if (it->first > inputBandwidth)
			{
				break;
			}
			
			bestIdx = it->second;
#if defined(DEBUG_ENABLED)
			AAMPLOG_MIL("Matched bw:%ld idx:%d", it->first, it->second);
#endif
		}
		if( bestIdx == INVALID_PROFILE)
		{
			/* If the bandwidth of the current period is greater than the previous period, just return the initial profile index, having
			 *  lowest bandwidth
			 */
			bestIdx = profileMap.begin()->second;
		}
		return bestIdx;
	}
	else
	{
		//return 0th index for safer side
		return 0;
	}
}

/**
 * @struct SpeedCache
 * @brief Stores the information for cache speed
 */

struct SpeedCache
{
	long last_sample_time_val;
	long prev_dlnow;
	long prevSampleTotalDownloaded;
	long totalDownloaded;
	long speed_now;
	long start_val;
	bool bStart;
	
	double totalWeight;
	double weightedBitsPerSecond;
	std::vector< std::pair<double,long> > mChunkSpeedData;
	
	SpeedCache() : last_sample_time_val(0), prev_dlnow(0), prevSampleTotalDownloaded(0), totalDownloaded(0), speed_now(0), start_val(0), bStart(false), totalWeight(0), weightedBitsPerSecond(0), mChunkSpeedData()
	{
	}
};

/** @brief Read Config values
 *  @return none
 */
void ABRManager::ReadPlayerConfig(AampAbrConfig *mAampAbrConfig)
{
	eAAMPAbrConfig.abrCacheLife     =  mAampAbrConfig->abrCacheLife;
	eAAMPAbrConfig.abrCacheLength   =  mAampAbrConfig->abrCacheLength;
	eAAMPAbrConfig.abrSkipDuration  =  mAampAbrConfig->abrSkipDuration;
	eAAMPAbrConfig.abrNwConsistency =  mAampAbrConfig->abrNwConsistency;
	eAAMPAbrConfig.abrThresholdSize =  mAampAbrConfig->abrThresholdSize;
	eAAMPAbrConfig.abrMaxBuffer     =  mAampAbrConfig->abrMaxBuffer;
	eAAMPAbrConfig.abrMinBuffer     =  mAampAbrConfig->abrMinBuffer;
	eAAMPAbrConfig.abrCacheOutlier  =  mAampAbrConfig->abrCacheOutlier;
	eAAMPAbrConfig.abrBufferCounter =  mAampAbrConfig->abrBufferCounter;
	
	//Logging Level
	
	eAAMPAbrConfig.infologging     =  mAampAbrConfig->infologging;
	eAAMPAbrConfig.debuglogging    = mAampAbrConfig->debuglogging;
	eAAMPAbrConfig.tracelogging    = mAampAbrConfig->tracelogging;
	eAAMPAbrConfig.warnlogging     = mAampAbrConfig->warnlogging;
	AAMPLOG_MIL("ABRCacheLife %d, ABRCacheLength %d, ABRSkipDuration %d, ABRNwConsistency %d, ABRThresholdSize %d, ABRMaxBuffer %d, ABRMinBuffer %d ABRCacheOutlier %d ABRBufferCounter %d ",eAAMPAbrConfig.abrCacheLife,eAAMPAbrConfig.abrCacheLength,eAAMPAbrConfig.abrSkipDuration,eAAMPAbrConfig.abrNwConsistency,eAAMPAbrConfig.abrThresholdSize,eAAMPAbrConfig.abrMaxBuffer,eAAMPAbrConfig.abrMinBuffer,eAAMPAbrConfig.abrCacheOutlier,eAAMPAbrConfig.abrBufferCounter);
}


/**
 * @brief function to update downloadbps value when video segment size is greater than abrthreshold size
 * @return downloadbps
 */

long ABRManager::CheckAbrThresholdSize(int bufferlen, int downloadTimeMs, long currentProfilebps, int fragmentDurationMs, CurlAbortReason abortReason)
{
	long downloadbps = ((long)(bufferlen / downloadTimeMs)*8000); // FIXME!
	// extra coding to avoid picking lower profile
	// Avoid this reset for Low bandwidth timeout cases
	if(downloadbps < currentProfilebps && fragmentDurationMs && downloadTimeMs < fragmentDurationMs/2 && (abortReason != eCURL_ABORT_REASON_LOW_BANDWIDTH_TIMEDOUT))
	{
		downloadbps = currentProfilebps;
	}
	return downloadbps;
}

/**
 * @brief Function to Update Persisted Recent Download Statistics Based on Cache Length
 * @return none
 */
void ABRManager::UpdateABRBitrateDataBasedOnCacheLength(std::vector < std::pair<long long,long> > &mAbrBitrateData,long downloadbps,bool LowLatencyMode)
{
	mAbrBitrateData.push_back(std::make_pair(ABRGetCurrentTimeMS(), downloadbps));
	//AAMPLOG_WARN("CacheSz[%d]ConfigSz[%d] Storing Size [%d] bps[%ld]",mAbrBitrateData.size(),abrCacheLength, buffer->len, ((long)(buffer->len / downloadTimeMS)*8000));
	if(LowLatencyMode)
	{
		if(mAbrBitrateData.size() > DEFAULT_ABR_CHUNK_CACHE_LENGTH)
			mAbrBitrateData.erase(mAbrBitrateData.begin());
	}
	else
	{
		if(mAbrBitrateData.size() > eAAMPAbrConfig.abrCacheLength)
			mAbrBitrateData.erase(mAbrBitrateData.begin());
	}
}

/**
 * @brief Function to Update Persisted Recent Download Statistics Based on abrCacheLife
 * @return none
 */
void ABRManager::UpdateABRBitrateDataBasedOnCacheLife(std::vector < std::pair<long long,long> > &mAbrBitrateData, std::vector< long> &tmpData)
{
	std::vector< std::pair<long long,long> >::iterator bitrateIter;
	long long presentTime = ABRGetCurrentTimeMS();
	for (bitrateIter = mAbrBitrateData.begin(); bitrateIter != mAbrBitrateData.end();)
	{
		//AAMPLOG_WARN("Sz[%d] TimeCheck Pre[%lld] Sto[%lld] diff[%lld] bw[%ld] ",mAbrBitrateData.size(),presentTime,(*bitrateIter).first,(presentTime - (*bitrateIter).first),(long)(*bitrateIter).second);
		if ((bitrateIter->first <= 0) || (presentTime - bitrateIter->first > eAAMPAbrConfig.abrCacheLife))
		{
			bitrateIter = mAbrBitrateData.erase(bitrateIter);
		}
		else
		{
			tmpData.push_back(bitrateIter->second);
			bitrateIter++;
		}
	}
}

/**
 * @brief Function to Update Persisted Recent Download Statistics Based on ABRCacheOutlier and calculate bw
 * @return Available bandwidth in bps
 */
long ABRManager::UpdateABRBitrateDataBasedOnCacheOutlier(std::vector< long> &tmpData)
{
	long avg = 0;
	long ret = -1;
	std::vector< long>::iterator tmpDataIter;
	long medianbps=0;
	int abrOutlierDiffBytes;
	
	std::sort(tmpData.begin(),tmpData.end());
	if (tmpData.size() %2)
	{
		medianbps = tmpData.at(tmpData.size()/2);
	}
	else
	{
		long m1 = tmpData.at(tmpData.size()/2);
		long m2 = tmpData.at(tmpData.size()/2)+1;
		medianbps = (m1+m2)/2;
	}
	
	long diffOutlier = 0;
	avg = 0;
	abrOutlierDiffBytes = eAAMPAbrConfig.abrCacheOutlier ;
	for (tmpDataIter = tmpData.begin();tmpDataIter != tmpData.end();)
	{
		diffOutlier = (*tmpDataIter) > medianbps ? (*tmpDataIter) - medianbps : medianbps - (*tmpDataIter);
		if (diffOutlier > abrOutlierDiffBytes)
		{
			//AAMPLOG_WARN("Outlier found[%ld]>[%ld] erasing ....",diffOutlier,abrOutlierDiffBytes);
			tmpDataIter = tmpData.erase(tmpDataIter);
		}
		else
		{
			avg += (*tmpDataIter);
			tmpDataIter++;
		}
	}
	if (tmpData.size())
	{
		//AAMPLOG_WARN("NwBW with newlogic size[%d] avg[%ld] ",tmpData.size(), avg/tmpData.size());
		ret = (avg/tmpData.size());
		//Store the PersistBandwidth and UpdatedTime on ABRManager
		//Bitrate Update only for foreground player
	}
	else
	{
		//AAMPLOG_WARN("No prior data available for abr, return -1 ");
		ret = -1;
	}
	return ret;
	
}
/*
 * @brief Function for ABR check for each segment download
 * @return bool true if profilechange needed else false
 */

bool ABRManager::CheckProfileChange(double totalFetchedDuration, int currProfileIndex, long availBW)
{
	bool checkProfileChange = true;
	long currBW = getBandwidthOfProfile(currProfileIndex);
	//Avoid doing ABR during initial buffering which will affect tune times adversely
	if ( totalFetchedDuration > 0 && totalFetchedDuration < eAAMPAbrConfig.abrSkipDuration)
	{
		AAMPLOG_TRACE("TotalFetchedDuration %lf ", totalFetchedDuration);
		//For initial fragment downloads, check available bw is less than default bw
		//If available BW is less than current selected one, we need ABR
		if (availBW > 0 && availBW < currBW)
		{
			AAMPLOG_WARN("Changing profile due to low available bandwidth(%ld) than default(%ld)!! ", availBW, currBW);
			
		}
		else
		{
			checkProfileChange = false;
		}
	}
	return checkProfileChange;
	
}

/**
 *  @brief Get Desired Profile based on Buffer availability
 *
 */

void ABRManager::GetDesiredProfileOnBuffer(int currProfileIndex,int &newProfileIndex,double bufferValue,double minBufferNeeded,const std::string& periodId)
{
	long currentBandwidth = getBandwidthOfProfile(currProfileIndex);
	long newBandwidth     = getBandwidthOfProfile(newProfileIndex);
	AAMPLOG_INFO("currProfileIndex %d newProfileIndex %d currentBandwidth %ld newBandwidth %ld bufferValue %lf, minBufferNeeded %lf", currProfileIndex, newProfileIndex, currentBandwidth, newBandwidth, bufferValue, minBufferNeeded);
	if(bufferValue > 0 )
	{
		if(newBandwidth > currentBandwidth)
		{
			// Rampup attempt . check if buffer availability is good before profile change
			// else retain current profile
			if(bufferValue < eAAMPAbrConfig.abrMaxBuffer)
				newProfileIndex = currProfileIndex;
		}
		else if (! GetLowLatencyServiceConfigured())
		{
			// Rampdown attempt. check if buffer availability is good before profile change
			// Also if delta of current profile to new profile is 1, then ignore the change
			// if bigger rampdown, then adjust to new profile
			// else retain current profile
			if(bufferValue > minBufferNeeded && getRampedDownProfileIndex(currProfileIndex,periodId) == newProfileIndex)
				newProfileIndex = currProfileIndex;
		}
	}
}

/**
 *  @brief Get Desired Profile on steady state while rampup
 */

void ABRManager::CheckRampupFromSteadyState(int currProfileIndex,int &newProfileIndex,long nwBandwidth,double bufferValue,long newBandwidth,BitrateChangeReason &mhBitrateReason,int &mMaxBufferCountCheck,const std::string& periodId)
{
	int abrThreshold = (int)((newBandwidth - nwBandwidth) * 100) / (int)nwBandwidth;
	AAMPLOG_INFO("currProfileIndex %d newProfileIndex %d nwBandwidth %ld bufferValue %lf newBandwidth %ld threshold %d", currProfileIndex, newProfileIndex, nwBandwidth, bufferValue, newBandwidth, abrThreshold);
	int nProfileIdx = getRampedUpProfileIndex(currProfileIndex,periodId);
	// switch to new profile only on bitrate difference is less than 30 percentage
	if(abrThreshold >= 0 && abrThreshold <= 30)
		newProfileIndex = nProfileIdx;
	if(newProfileIndex  != currProfileIndex)
	{
		static int loop = 1;
		AAMPLOG_WARN("attempted rampup from steady state currProfileIndex %d newProfileIndex %d bufferValue %lf threshold %d", currProfileIndex, newProfileIndex, bufferValue, abrThreshold);
		loop = (++loop >4)?1:loop; // FIXME
		mMaxBufferCountCheck =  pow(eAAMPAbrConfig.abrBufferCounter,loop);
		mhBitrateReason = eAAMP_BITRATE_CHANGE_BY_BUFFER_FULL;
	}
}

/**
 * @brief Get Desired Profile on steady state while rampdown
 */

void ABRManager::CheckRampdownFromSteadyState(int currProfileIndex, int &newProfileIndex,BitrateChangeReason &mBitrateReason,int mABRLowBufferCounter,const std::string& periodId)
{
	AAMPLOG_INFO("currProfileIndex %d newProfileIndex %d mABRLowBufferCounter %d", currProfileIndex, newProfileIndex, mABRLowBufferCounter);
	if(mABRLowBufferCounter >= eAAMPAbrConfig.abrBufferCounter)
	{
		newProfileIndex = getRampedDownProfileIndex(currProfileIndex,periodId);
		if(newProfileIndex  != currProfileIndex)
		{
			mBitrateReason = eAAMP_BITRATE_CHANGE_BY_BUFFER_EMPTY;
			AAMPLOG_WARN("Attempted rampdown from steady state ->currProf:%d newProf:%d", currProfileIndex,newProfileIndex);
		}
	}
}

/**
 * @brief function to get currenttime in ms
 *
 **/

long long ABRManager::ABRGetCurrentTimeMS(void)
{
	struct timeval t;
	gettimeofday(&t, NULL);
	return (long long)(t.tv_sec*1e3 + t.tv_usec*1e-3);
}


/**
 *  @brief Get Low Latency ABR Start Status
 */
bool ABRManager::GetLowLatencyStartABR()
{
	return (this->bLowLatencyStartABR);
}

/**
 *  @brief Set Low Latency ABR Start Status
 */
void ABRManager::SetLowLatencyStartABR(bool bStart)
{
	this->bLowLatencyStartABR = bStart;
}


/**
 *  @brief Get Low Latency Service Configuration Status
 */
bool ABRManager::GetLowLatencyServiceConfigured()
{
	return (this->bLowLatencyServiceConfigured);
}

/**
 *  @brief Set Low Latency Service Configuration Status
 */
void ABRManager::SetLowLatencyServiceConfigured(bool bConfig)
{
	this->bLowLatencyServiceConfigured = bConfig;
}

/**
 * @brief Check if it is Good to capture speed sample
 * @param time_diff Time Diff
 * @retval bool Good to Estimate
 */
bool ABRManager::IsABRDataGoodToEstimate(long time_diff) {
	
	return time_diff >= DEFAULT_ABR_ELAPSED_MILLIS_FOR_ESTIMATE;
}


/**
 * @brief to Update the ChunkSpeedData based on low latency ABR speedstoreSize
 * @params speedcache struct
 * @params estimated-bps
 * @return None
 */
void ABRManager::CheckLLDashABRSpeedStoreSize(struct SpeedCache *speedcache,long &bitsPerSecond,long time_now,long total_dl_diff,long time_diff,long currentTotalDownloaded)
{
	speedcache->last_sample_time_val = time_now;
	//speed @ bits per second
	speedcache->speed_now = ((long)(total_dl_diff / time_diff)* 8000);
	
	double weight = std::sqrt((double)total_dl_diff);
	speedcache->weightedBitsPerSecond += weight * speedcache->speed_now;
	speedcache->totalWeight += weight;
	speedcache->mChunkSpeedData.push_back(std::make_pair(weight, speedcache->speed_now));
	
	if(speedcache->mChunkSpeedData.size() > MAX_LOW_LATENCY_DASH_ABR_SPEEDSTORE_SIZE)
	{
		speedcache->totalWeight -= (speedcache->mChunkSpeedData.at(0).first);
		speedcache->weightedBitsPerSecond -= (speedcache->mChunkSpeedData.at(0).first * speedcache->mChunkSpeedData.at(0).second);
		speedcache->mChunkSpeedData.erase(speedcache->mChunkSpeedData.begin());
		//Speed Data Count is good to estimate bps
		bitsPerSecond = speedcache->weightedBitsPerSecond/speedcache->totalWeight;
	}
	
	speedcache->prevSampleTotalDownloaded = currentTotalDownloaded;
}

/**
 * @brief - Fn to rampdown during fragment download failure based on buffer
 * @param - current available buffer
 * @return - desired profile based on buffer
 */
long ABRManager::FragmentfailureRampdown(int currentBuffer, int currentProfileIndex)
{
	double bufferPercentage = ((double)currentBuffer / eAAMPAbrConfig.abrMaxBuffer) * 100;
	long desiredProfilebw = 0;
	long currentbw = getBandwidthOfProfile(currentProfileIndex);
	std::vector<ProfileInfo> availableProfiles = getProfileInfo();
	int len = (int)availableProfiles.size() - 1;
	std::sort(availableProfiles.begin(), availableProfiles.end(), [](const ProfileInfo& a, const ProfileInfo& b) {
		return a.bandwidthBitsPerSecond < b.bandwidthBitsPerSecond;
	});
	// Iterate over profiles in descending order of bandwidth
	for (int i = (int)availableProfiles.size() -1  ;i >= 0 ; i--)
	{
		double profilePercentage = ((double)(availableProfiles[i].bandwidthBitsPerSecond) / availableProfiles[len].bandwidthBitsPerSecond) * 100.0;
		AAMPLOG_WARN("Index: %d, bandwidth %d, profile percentage %lf, buffer percentage %lf",i,(int)availableProfiles[i].bandwidthBitsPerSecond,profilePercentage,bufferPercentage);
		// Check if profile bandwidth percentage is less than buffer percentage, and it should be a rampdown
		if (profilePercentage < bufferPercentage && (availableProfiles[i].bandwidthBitsPerSecond < currentbw))
		{
			desiredProfilebw  = availableProfiles[i].bandwidthBitsPerSecond;
			break;
		}
	}
	
	if (desiredProfilebw == 0)
	{
		// If no profile found, then return the lowest profile. Usually happens when bufferPercentage is very low or already at lowest profile
		desiredProfilebw = availableProfiles[0].bandwidthBitsPerSecond;
	}
	return desiredProfilebw;
}
