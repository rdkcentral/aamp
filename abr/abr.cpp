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

/***************************************************
 * @file abr.cpp
 * @brief Handles operations on Hybrid ABR functionalities
 ***************************************************/

#include "abr.h"
#include "RollingMedianOutlierEstimator.h"
#include "HarmonicEwmaEstimator.h"
#include "BandwidthEstimatorBase.h"
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

//#define DEBUG_ENABLED

#define DEFAULT_ABR_ELAPSED_MILLIS_FOR_ESTIMATE	100			/**< Duration(ms) to check Chunk Speed */
#define MAX_LOW_LATENCY_DASH_ABR_SPEEDSTORE_SIZE 10

ABRManager::AampAbrConfig eAAMPAbrConfig;

BitsPerSecond ABRManager::mPersistBandwidth = 0;
long long ABRManager::mPersistBandwidthUpdatedTime = 0;

ABRManager::ABRManager()
	: bLowLatencyStartABR(false),
	  bLowLatencyServiceConfigured(false),
	  mProfiles(),
	  mSortedBWProfileList(),
	  mProfileLock(),
	  mBandwidthState(),
	  mBandwidthEstimationAlgorithm(BANDWIDTH_ESTIMATION_ALGORITHM_ROLLING_MEDIAN_OUTLIER),
	  mBandwidthEstimator(),
	  mBandwidthEstimatorLock()
{
	SelectBandwidthEstimationAlgorithm(mBandwidthEstimationAlgorithm);
}

ABRManager::~ABRManager() = default;

void ABRManager::SelectBandwidthEstimationAlgorithm(BandwidthEstimationAlgorithm type)
{
	std::lock_guard<std::mutex> lock(mBandwidthEstimatorLock);
	mBandwidthEstimationAlgorithm = type;

	switch (type)
	{
		case BANDWIDTH_ESTIMATION_ALGORITHM_HARMONIC_EWMA:
			mBandwidthEstimator.reset(new HarmonicEwmaEstimator());
			break;

		case BANDWIDTH_ESTIMATION_ALGORITHM_ROLLING_MEDIAN_OUTLIER:
		default:
		{
			mBandwidthEstimator.reset(new RollingMedianOutlierEstimator());
		}
		break;
	}
	AAMPLOG_WARN("Setting ABR Bandwidth Estimator type to %s", mBandwidthEstimator->GetNetworkEstimatorName());

	// This is to initialize the bandwidth state from the newly created estimator
	(void)UpdateBandwidthStateFromEstimatorLocked();
}

BandwidthEstimationAlgorithm ABRManager::GetBandwidthEstimationAlgorithm() const
{
	std::lock_guard<std::mutex> lock(mBandwidthEstimatorLock);
	return mBandwidthEstimationAlgorithm;
}

void ABRManager::AddBandwidthSample(BitsPerSecond downloadbps, bool lowLatencyMode)
{
	std::lock_guard<std::mutex> lock(mBandwidthEstimatorLock);
	if (mBandwidthEstimator)
	{
		mBandwidthEstimator->AddBandwidthSample(downloadbps, lowLatencyMode);
		(void)UpdateBandwidthStateFromEstimatorLocked();
	}
}

void ABRManager::ReportDownloadComplete(
	BitsPerSecond downloadbps,
	bool lowLatencyMode,
	const DownloadMetrics &metrics)
{
	std::lock_guard<std::mutex> lock(mBandwidthEstimatorLock);
	if (!mBandwidthEstimator)
	{
		return;
	}
	if (downloadbps > 0)
	{
		mBandwidthEstimator->AddBandwidthSample(downloadbps, lowLatencyMode);
	}
	mBandwidthEstimator->UpdateDownloadMetrics(metrics);
	(void)UpdateBandwidthStateFromEstimatorLocked();
}

void ABRManager::ReportDownloadProgress(
	BitsPerSecond downloadbps,
	bool lowLatencyMode,
	const DownloadProgressInfo &progressInfo)
{
	std::lock_guard<std::mutex> lock(mBandwidthEstimatorLock);
	if (!mBandwidthEstimator)
	{
		return;
	}
	mBandwidthEstimator->UpdateDownloadProgress(progressInfo);
	if (downloadbps > 0)
	{
		mBandwidthEstimator->AddBandwidthSample(downloadbps, lowLatencyMode);
	}
	(void)UpdateBandwidthStateFromEstimatorLocked();
}

BitsPerSecond ABRManager::UpdateBandwidthStateFromEstimatorLocked()
{
	if (!mBandwidthEstimator)
	{
		mBandwidthState.availableBandwidth = static_cast<BitsPerSecond>(-1);
		return mBandwidthState.availableBandwidth;
	}

	const BitsPerSecond estimate = mBandwidthEstimator->GetBandwidthBitsPerSecond();
	mBandwidthState.availableBandwidth = estimate;
	if (estimate != static_cast<BitsPerSecond>(-1))
	{
		mBandwidthState.networkBandwidth = estimate;
	}
	return estimate;
}

void ABRManager::SetInitialBandwidthForProfile(BitsPerSecond bitsPerSecond, bool trickPlay, int profile)
{
	(void)trickPlay;
	(void)profile;

	std::lock_guard<std::mutex> lock(mBandwidthEstimatorLock);
	mBandwidthState.availableBandwidth = bitsPerSecond;
	mBandwidthState.networkBandwidth = bitsPerSecond;

	if (mBandwidthEstimator)
	{
		mBandwidthEstimator->Reset();
		if (bitsPerSecond > 0)
		{
			mBandwidthEstimator->AddBandwidthSample(bitsPerSecond, false);
		}
		(void)UpdateBandwidthStateFromEstimatorLocked();
	}
}

void ABRManager::ResetCurrentlyAvailableBandwidth()
{
	std::lock_guard<std::mutex> lock(mBandwidthEstimatorLock);
	if (mBandwidthEstimator)
	{
		mBandwidthEstimator->ResetCurrentlyAvailableBandwidth();
	}
}

BitsPerSecond ABRManager::GetCurrentlyAvailableBandwidth()
{
	std::lock_guard<std::mutex> lock(mBandwidthEstimatorLock);
	return UpdateBandwidthStateFromEstimatorLocked();
}

BitsPerSecond ABRManager::GetNetworkBandwidth()
{
	std::lock_guard<std::mutex> lock(mBandwidthEstimatorLock);
	(void)UpdateBandwidthStateFromEstimatorLocked();
	return mBandwidthState.networkBandwidth;
}

bool ABRManager::HasBandwidthEstimator() const
{
	std::lock_guard<std::mutex> lock(mBandwidthEstimatorLock);
	return (mBandwidthEstimator != nullptr);
}

double ABRManager::GetPredictedDownloadTimeSeconds(std::size_t segmentSizeBytes) const
{
	std::lock_guard<std::mutex> lock(mBandwidthEstimatorLock);
	if (!mBandwidthEstimator)
	{
		return 0.0;
	}
	return mBandwidthEstimator->GetPredictedDownloadTimeSeconds(segmentSizeBytes);
}

/**
 * @brief Get initial profile index, choose the medium profile or
 * the profile whose bitrate >= the default bitrate.
 */
int ABRManager::getInitialProfileIndex(bool chooseMediumProfile, const std::string& periodId)
{
	std::lock_guard<std::mutex> lock(mProfileLock);
	size_t profileCount = mProfiles.size();
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
		AAMPLOG_WARN("Got invalid profile index, choose the first index = %d and profileCount = %zu and defaultBitrate = %" BITSPERSECOND_FORMAT, desiredProfileIndex, profileCount, mDefaultInitBitrate);
	} else {
		AAMPLOG_MIL("Get initial profile index = %d, bitrate = %" BITSPERSECOND_FORMAT " and defaultBitrate = %" BITSPERSECOND_FORMAT, desiredProfileIndex, mProfiles[desiredProfileIndex].bandwidthBitsPerSecond, mDefaultInitBitrate);
	}
	return desiredProfileIndex;
}

/**
 * @brief Update the lowest / desired profile index
 *    by the profile info.
 */
void ABRManager::updateProfile()
{
	/**
	 * @brief A temporary structure of iframe track info
	 */
	struct IframeTrackInfo {
		BitsPerSecond bandwidth;
		int idx;
	};
	
	std::unique_lock<std::mutex> lock(mProfileLock);
	size_t profileCount = mProfiles.size();
	
	std::vector<IframeTrackInfo> iframeTrackInfo;
	bool is4K = false;
	
	// Construct iframe track info
	for (int i = 0; i < (int)profileCount; i++) {
		if (mProfiles[i].isIframeTrack) {
			IframeTrackInfo info { mProfiles[i].bandwidthBitsPerSecond, i };
			iframeTrackInfo.push_back(info);
		}
	}
	lock.unlock();
	
	// Exists iframe track
	size_t iframeTrackCount = iframeTrackInfo.size();
	if( iframeTrackCount )
	{
		std::sort(iframeTrackInfo.begin(), iframeTrackInfo.end(), [](const IframeTrackInfo& a, const IframeTrackInfo& b) {
			return a.bandwidth < b.bandwidth; // ascending order by bandwidth
		});
		
		const IframeTrackInfo &back = iframeTrackInfo.back();
		if(mProfiles[back.idx].height > HEIGHT_FULL_HD || mProfiles[back.idx].width > WIDTH_FULL_HD)
		{
			is4K = true;
		}
		
		if (mDefaultIframeBitrate > 0) {
			mLowestIframeProfile = mDesiredIframeProfile = iframeTrackInfo[0].idx;
			for (size_t cnt = 0; cnt < iframeTrackCount; cnt++) {
				// find the track less than default bw set, apply to both desired and lower ( for all speed of trick)
				if(iframeTrackInfo[cnt].bandwidth >= mDefaultIframeBitrate) {
					break;
				}
				mDesiredIframeProfile = iframeTrackInfo[cnt].idx;
			}
		} else {
			if(is4K) {
				// Get the default profile of 4k video, apply same bandwidth of video to iframe also
				int desiredProfileIndexNonIframe = (int)profileCount / 2;
				int desiredProfileNonIframeBW = (int)mProfiles[desiredProfileIndexNonIframe].bandwidthBitsPerSecond ;
				mDesiredIframeProfile = mLowestIframeProfile = 0;
				for (size_t cnt = 0; cnt < iframeTrackCount; cnt++) {
					// if bandwidth matches, apply to both desired and lower ( for all speed of trick)
					if(iframeTrackInfo[cnt].bandwidth == desiredProfileNonIframeBW) {
						mDesiredIframeProfile = mLowestIframeProfile = iframeTrackInfo[cnt].idx;
						break;
					}
				}
				// if matching bandwidth not found with video, then pick the middle profile for iframe
				if((!mDesiredIframeProfile) && (iframeTrackCount >= 1)) {
					int desiredTrackIdx = (int) (iframeTrackCount / 2) + (iframeTrackCount % 2);
					mDesiredIframeProfile = mLowestIframeProfile = iframeTrackInfo[desiredTrackIdx].idx;
				}
			} else {
				//Keeping old logic for non 4K streams
				for (size_t cnt = 0; cnt < iframeTrackCount; cnt++) {
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
	
#if defined(DEBUG_ENABLED)
	AAMPLOG_MIL("Update profile info, mDesiredIframeProfile = %d, mLowestIframeProfile = %d", mDesiredIframeProfile, mLowestIframeProfile);
#endif
}

/**
 *  @brief According to the given bandwidth, return the best matched
 *  profile index.
 */
int ABRManager::getBestMatchedProfileIndexByBandWidth(int bandwidth)
{
	// a) Check if network bandwidth changed from starting bandwidth
	// b) Check if netwwork bandwidth is different from persisted bandwidth( needed for first time reporting)
	// find the profile for the newbandwidth
	int desiredProfileIndex = 0;
	std::lock_guard<std::mutex> lock(mProfileLock);
	size_t profileCount = mProfiles.size();
	for (int i = 0; i < (int)profileCount; i++) {
		const ProfileInfo& profile = mProfiles[i];
		if (!profile.isIframeTrack) {
			if (profile.bandwidthBitsPerSecond == bandwidth) {
				// Good case, most manifest url will have same bandwidth in fragment file with configured profile bandwidth
				desiredProfileIndex = i;
				break;
			} else if (profile.bandwidthBitsPerSecond < bandwidth) {
				// fragment file name bandwidth doesn't match the profile bandwidth, will be always less
				if (static_cast<size_t>(i + 1) == profileCount) {
					desiredProfileIndex = i;
					break;
				}
				else
					desiredProfileIndex = (i + 1);
			}
		}
	}
#if defined(DEBUG_ENABLED)
	AAMPLOG_MIL("Get best matched profile index = %d bitrate = %" BITSPERSECOND_FORMAT,
				desiredProfileIndex, (desiredProfileIndex != INVALID_PROFILE &&	static_cast<size_t>(desiredProfileIndex) < profileCount) ?
				mProfiles[desiredProfileIndex].bandwidthBitsPerSecond : 0);
#endif
	return desiredProfileIndex;
}

/**
 *  @brief Ramp down the profile one step to get the profile index of a lower bitrate.
 */
int ABRManager::getRampedDownProfileIndex(int currentProfileIndex, const std::string& periodId)
{
	std::lock_guard<std::mutex> lock(mProfileLock);
	size_t profileCount = mProfiles.size();
	
	// Clamp the param to avoid overflow
	if (static_cast<size_t>(currentProfileIndex) >= profileCount) {
		AAMPLOG_WARN("Invalid profileIndex %d exceeds the current profile count %zu", currentProfileIndex, profileCount);
		currentProfileIndex--;
	}
	
	int desiredProfileIndex = currentProfileIndex;
	if (profileCount == 0) {
		AAMPLOG_WARN("No profiles found" );
		return desiredProfileIndex;
	}
	
	BitsPerSecond currentBandwidth = mProfiles[currentProfileIndex].bandwidthBitsPerSecond;
	SortedBWProfileListIter iter = mSortedBWProfileList[periodId].find(currentBandwidth);
	if (iter == mSortedBWProfileList[periodId].end()) {
		AAMPLOG_WARN("The current bitrate %" BITSPERSECOND_FORMAT " is not in the profile list", currentBandwidth);
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
	AAMPLOG_MIL("Ramped down profile index = %d bitrate = %" BITSPERSECOND_FORMAT, desiredProfileIndex, mProfiles[desiredProfileIndex].bandwidthBitsPerSecond);
#endif
	return desiredProfileIndex;
}

/**
 *  @brief Ramp Up the profile one step to get the profile index of a upper bitrate.
 */
int ABRManager::getRampedUpProfileIndex(int currentProfileIndex, const std::string& periodId)
{
	std::lock_guard<std::mutex> lock(mProfileLock);
	// Clamp the param to avoid overflow
	size_t profileCount = mProfiles.size();
	int desiredProfileIndex = currentProfileIndex;
	
	if (profileCount == 0 || static_cast<size_t>(currentProfileIndex) >= profileCount) {
		AAMPLOG_WARN("No profiles/input profile %d more than profileCount %zu", currentProfileIndex, profileCount);
		return desiredProfileIndex;
	}
	
	BitsPerSecond currentBandwidth = mProfiles[currentProfileIndex].bandwidthBitsPerSecond;
	SortedBWProfileListIter iter = mSortedBWProfileList[periodId].find(currentBandwidth);
	if (iter == mSortedBWProfileList[periodId].end()) {
		AAMPLOG_WARN("The current bitrate %" BITSPERSECOND_FORMAT " is not in the profile list", currentBandwidth);
		return desiredProfileIndex;
	}
	
	if(std::next(iter) != mSortedBWProfileList[periodId].end())
	{
		std::advance(iter, 1);
		desiredProfileIndex = iter->second;
	}
	
#if defined(DEBUG_ENABLED)
	AAMPLOG_MIL("Ramped up profile index = %d bitrate = %" BITSPERSECOND_FORMAT, desiredProfileIndex, mProfiles[desiredProfileIndex].bandwidthBitsPerSecond);
#endif
	return desiredProfileIndex;
}

/**
 *  @brief Get UserData of profile
 */
int ABRManager::getUserDataOfProfile(int currentProfileIndex)
{
	std::lock_guard<std::mutex> lock(mProfileLock);
	int userData = -1;
	size_t profileCount = mProfiles.size();
	if (profileCount == 0 || static_cast<size_t>(currentProfileIndex) >= profileCount) {
		AAMPLOG_WARN("No profiles/input profile %d more than profileCount %zu", currentProfileIndex, profileCount);
	}
	else
	{
		userData = mProfiles[currentProfileIndex].userData;
	}
	return userData;
}

/**
 *  @brief Check if the bitrate of currentProfileIndex reaches to the lowest.
 */
bool ABRManager::isProfileIndexBitrateLowest(int currentProfileIndex, const std::string& periodId)
{
	std::lock_guard<std::mutex> lock(mProfileLock);
	size_t profileCount = mProfiles.size();
	
	if (static_cast<size_t>(currentProfileIndex) >= profileCount) {
		AAMPLOG_WARN("Invalid profileIndex %d exceeds the current profile count %zu", currentProfileIndex, profileCount);
		currentProfileIndex--;
	}
	
	// If there is no profiles list, then it means `currentProfileIndex` always reaches to
	// the lowest.
	if (profileCount == 0) {
		AAMPLOG_WARN( "No profiles found" );
		return true;
	}
	
	BitsPerSecond currentBandwidth = mProfiles[currentProfileIndex].bandwidthBitsPerSecond;
	SortedBWProfileListIter iter = mSortedBWProfileList[periodId].find(currentBandwidth);
	return iter == mSortedBWProfileList[periodId].begin();
}

/**
 *  @brief Do ABR by ramping bitrate up/down according to the current
 *         network status. Returns the profile index with the bitrate matched with
 *         the current bitrate.
 */
int ABRManager::getProfileIndexByBitrateRampUpOrDown(int currentProfileIndex, BitsPerSecond currentBandwidth, BitsPerSecond networkBandwidth, int nwConsistencyCnt, const std::string& periodId)
{
	std::lock_guard<std::mutex> lock(mProfileLock);
	// Clamp the param to avoid overflow
	size_t profileCount = mProfiles.size();
	
	if (static_cast<size_t>(currentProfileIndex) >= profileCount) {
		AAMPLOG_WARN("Invalid profileIndex %d exceeds the current profile count %zu", currentProfileIndex, profileCount);
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
		AAMPLOG_MIL("Ramp up profile index = %d, bitrate = %" BITSPERSECOND_FORMAT " networkBandwidth = %" BITSPERSECOND_FORMAT,
					desiredProfileIndex, (desiredProfileIndex != INVALID_PROFILE &&	static_cast<size_t>(desiredProfileIndex) < profileCount) ?
					mProfiles[desiredProfileIndex].bandwidthBitsPerSecond : 0, networkBandwidth);
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
			AAMPLOG_WARN("Didn't find a profile which supports bandwidth[%" BITSPERSECOND_FORMAT "], min bandwidth available [%" BITSPERSECOND_FORMAT "]. Set profile to lowest!", networkBandwidth, mSortedBWProfileList[periodId].begin()->first);
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
		AAMPLOG_MIL("Ramp down profile index = %d, bitrate = %" BITSPERSECOND_FORMAT " networkBandwidth = %" BITSPERSECOND_FORMAT,
					desiredProfileIndex, (desiredProfileIndex != INVALID_PROFILE &&	static_cast<size_t>(desiredProfileIndex) < profileCount) ?
						mProfiles[desiredProfileIndex].bandwidthBitsPerSecond : 0, networkBandwidth);
#endif
	}
	
	if (currentProfileIndex != desiredProfileIndex) {
		AAMPLOG_MIL("currBW:%" BITSPERSECOND_FORMAT " NwBW=%" BITSPERSECOND_FORMAT " currProf:%d desiredProf:%d Period ID:%s",
					currentBandwidth, networkBandwidth,
					currentProfileIndex, desiredProfileIndex, periodId.c_str());
	}
	
	return desiredProfileIndex;
}

/**
 *  @brief Get bandwidth of profile
 */
BitsPerSecond ABRManager::getBandwidthOfProfile(int profileIndex)
{
	std::lock_guard<std::mutex> lock(mProfileLock);
	// Clamp the param to avoid overflow
	size_t profileCount = mProfiles.size();
	
	if( profileCount == 0 )
	{
		AAMPLOG_WARN( "No profiles found" );
		return 0;
	}
	if (static_cast<size_t>(profileIndex) >= profileCount){
		AAMPLOG_WARN("Invalid profileIndex %d exceeds the current profile count %zu", profileIndex, profileCount);
		profileIndex--;
	}
	
	return mProfiles[profileIndex].bandwidthBitsPerSecond;
}

/**
 *  @brief Get the index of max bandwidth
 */
int ABRManager::getMaxBandwidthProfile(const std::string& periodId)
{
	std::lock_guard<std::mutex> lock(mProfileLock);
	size_t profileCount = mProfiles.size();
	if( profileCount==0 )
	{
		AAMPLOG_WARN( "No profiles found" );
		return 0;
	}
	return mSortedBWProfileList[periodId].size()?mSortedBWProfileList[periodId].rbegin()->second:0;
}

// Getters/Setters

int ABRManager::getProfileCount( void )
{
	std::lock_guard<std::mutex> lock(mProfileLock);
	return (int)mProfiles.size();
}

/**
 *  @brief Set the default init bitrate
 */
void ABRManager::setDefaultInitBitrate(BitsPerSecond defaultInitBitrate)
{
	mDefaultInitBitrate = defaultInitBitrate;
}

/**
 *  @brief Get the lowest iframe profile index.
 */
int ABRManager::getLowestIframeProfile() const
{
	return mLowestIframeProfile;
}

/**
 *  @brief Get the desired iframe profile index.
 */
int ABRManager::getDesiredIframeProfile() const
{
	return mDesiredIframeProfile;
}

/**
 *  @brief Add new profile info into the manager
 */
void ABRManager::addProfile(const ABRManager::ProfileInfo &profile)
{
	std::lock_guard<std::mutex> lock(mProfileLock);
	addSortedBWProfileList(profile, (int)mProfiles.size() );
	mProfiles.push_back(profile);
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
		AAMPLOG_MIL("bw:%" BITSPERSECOND_FORMAT " idx:%d", profileInfo.bandwidthBitsPerSecond, idx);
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
int ABRManager::removeProfiles(std::vector<BitsPerSecond> profileBPS, int currentProfileIndex, const std::string& periodId)
{
	std::lock_guard<std::mutex> lock(mProfileLock);
	int modifiedProfileIndex = INVALID_PROFILE;
	size_t profileCount = mProfiles.size();
	
	if( profileCount == 0 )
	{
		AAMPLOG_WARN( "No profiles found" );
		return modifiedProfileIndex;
	}
	
	if (static_cast<size_t>(currentProfileIndex) >= profileCount) {
		AAMPLOG_WARN("Invalid profileIndex %d exceeds the current profile count %zu", currentProfileIndex, profileCount);
		currentProfileIndex--;
	}
	BitsPerSecond currentBandwidth = mProfiles[currentProfileIndex].bandwidthBitsPerSecond;
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
	AAMPLOG_MIL("profileCount after removing profiles orig:%zu and new:%zu", profileCount, mProfiles.size() );
#endif
	profileCount = mProfiles.size();
	
	mSortedBWProfileList.clear();
	// Get new profile count
	for(int idx = 0; idx < (int)profileCount; idx++) {
		addSortedBWProfileList(mProfiles[idx], idx);
		if(currentBandwidth == mProfiles[idx].bandwidthBitsPerSecond) {
			modifiedProfileIndex = idx;
		}
	}
	
	if (modifiedProfileIndex == INVALID_PROFILE) {
		AAMPLOG_MIL("Unable to find the currentProfileIndex in the modified profiles, currentProfileIndex:%d currBW:%" BITSPERSECOND_FORMAT " period ID:%s", currentProfileIndex, currentBandwidth, periodId.c_str());
	}
	return modifiedProfileIndex;
}

/**
 *  @brief Clear profiles
 */
void ABRManager::clearProfiles()
{
	std::lock_guard<std::mutex> lock(mProfileLock);
	mProfiles.clear();
	mSortedBWProfileList.clear();
}

/**
 *  @brief Set the default iframe bitrate
 */
void ABRManager::setDefaultIframeBitrate(BitsPerSecond defaultIframeBitrate)
{
	mDefaultIframeBitrate = defaultIframeBitrate;
}

/**
 *  @brief Get the lowest bitrate pointing index
 */
int ABRManager::getProfileIndexForLowestBandwidth()
{
	std::lock_guard<std::mutex> lock(mProfileLock);
	size_t profileCount = mProfiles.size();
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
int ABRManager::getClosestProfileIndexByBandwidth( BitsPerSecond inputBandwidth )
{
	std::lock_guard<std::mutex> lock(mProfileLock);
	// Use the first period's map
	if (!mSortedBWProfileList.empty())
	{
		int bestIdx = INVALID_PROFILE;
		auto& profileMap = mSortedBWProfileList.begin()->second;
		for (std::map<BitsPerSecond, int>::const_iterator it = profileMap.begin(); it != profileMap.end(); ++it)
		{
			if (it->first > inputBandwidth)
			{
				break;
			}
			
			bestIdx = it->second;
#if defined(DEBUG_ENABLED)
			AAMPLOG_MIL("Matched bw:%" BITSPERSECOND_FORMAT " idx:%d", it->first, it->second);
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
	eAAMPAbrConfig.bandwidthEstimatorType = mAampAbrConfig->bandwidthEstimatorType;
	
	//Logging Level
	
	eAAMPAbrConfig.infologging     =  mAampAbrConfig->infologging;
	eAAMPAbrConfig.debuglogging    = mAampAbrConfig->debuglogging;
	eAAMPAbrConfig.tracelogging    = mAampAbrConfig->tracelogging;
	eAAMPAbrConfig.warnlogging     = mAampAbrConfig->warnlogging;
	AAMPLOG_MIL("ABRCacheLife %d, ABRCacheLength %d, ABRSkipDuration %d, ABRNwConsistency %d, ABRThresholdSize %d, ABRMaxBuffer %d, ABRMinBuffer %d ABRCacheOutlier %d ABRBufferCounter %d ABRBandwidthEstimator %d",eAAMPAbrConfig.abrCacheLife,eAAMPAbrConfig.abrCacheLength,eAAMPAbrConfig.abrSkipDuration,eAAMPAbrConfig.abrNwConsistency,eAAMPAbrConfig.abrThresholdSize,eAAMPAbrConfig.abrMaxBuffer,eAAMPAbrConfig.abrMinBuffer,eAAMPAbrConfig.abrCacheOutlier,eAAMPAbrConfig.abrBufferCounter,eAAMPAbrConfig.bandwidthEstimatorType);

	if (eAAMPAbrConfig.bandwidthEstimatorType < BANDWIDTH_ESTIMATION_ALGORITHM_MAX)
	{
		const auto ConfigureEstimator = [&]()
		{
			if(mBandwidthEstimator)
			{
				BandwidthEstimatorConfig config;
				config.mAbrCacheLife = eAAMPAbrConfig.abrCacheLife;
				config.mAbrCacheLength = eAAMPAbrConfig.abrCacheLength;
				config.mAbrCacheOutlier = eAAMPAbrConfig.abrCacheOutlier;
				config.mLowLatencyCacheLength = DEFAULT_ABR_CHUNK_CACHE_LENGTH;
				mBandwidthEstimator->SetConfig(config);
			}
		};

		const auto newEstimatorType = static_cast<BandwidthEstimationAlgorithm>(eAAMPAbrConfig.bandwidthEstimatorType);
		bool needNewEstimator = false;
		{
			std::lock_guard<std::mutex> lock(mBandwidthEstimatorLock);
			// Check if we need to create a new estimator
			needNewEstimator = (!mBandwidthEstimator ||	(mBandwidthEstimationAlgorithm != newEstimatorType));
			if (!needNewEstimator)
			{
				// Just reconfigure existing estimator
				ConfigureEstimator();
			}
		}

		if (needNewEstimator)
		{
			// Create new estimator and configure it
			SelectBandwidthEstimationAlgorithm(newEstimatorType);
			// Protect mBandwidthEstimator configuration change
			std::lock_guard<std::mutex> lock(mBandwidthEstimatorLock);
			ConfigureEstimator();
		}
	}
}

/**
 * @brief function to update downloadbps value when video segment size is greater than abrthreshold size
 * @return downloadbps
 */

BitsPerSecond ABRManager::CheckAbrThresholdSize(int bufferlen, int downloadTimeMs, BitsPerSecond currentProfilebps, int fragmentDurationMs, CurlAbortReason abortReason)
{
	BitsPerSecond downloadbps = currentProfilebps;
	if( downloadTimeMs )
	{
		// Use 64-bit arithmetic and multiply first to avoid precision loss:
		// bits per second = (bytes * 8 * 1000) / ms = (bytes * 8000) / ms
		downloadbps = static_cast<BitsPerSecond>(
			(static_cast<long long>(bufferlen) * 8000LL) / downloadTimeMs);
		
		// extra coding to avoid picking lower profile
		// Avoid this reset for Low bandwidth timeout cases
		if(
		   downloadbps < currentProfilebps &&
		   fragmentDurationMs &&
		   downloadTimeMs < fragmentDurationMs/2 &&
		   (abortReason != eCURL_ABORT_REASON_LOW_BANDWIDTH_TIMEDOUT))
		{
			downloadbps = currentProfilebps;
		}
	}
	return downloadbps;
}

/*
 * @brief Function for ABR check for each segment download
 * @return bool true if profilechange needed else false
 */
bool ABRManager::CheckProfileChange(double totalFetchedDuration, int currProfileIndex, BitsPerSecond availBW)
{
	bool checkProfileChange = true;
	BitsPerSecond currBW = getBandwidthOfProfile(currProfileIndex);
	//Avoid doing ABR during initial buffering which will affect tune times adversely
	if ( totalFetchedDuration > 0 && totalFetchedDuration < eAAMPAbrConfig.abrSkipDuration)
	{
		AAMPLOG_TRACE("TotalFetchedDuration %lf ", totalFetchedDuration);
		//For initial fragment downloads, check available bw is less than default bw
		//If available BW is less than current selected one, we need ABR
		if (availBW > 0 && availBW < currBW)
		{
			AAMPLOG_WARN("Changing profile due to low available bandwidth(%" BITSPERSECOND_FORMAT ") than default(%" BITSPERSECOND_FORMAT ")!! ", availBW, currBW);
			
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
	BitsPerSecond currentBandwidth = getBandwidthOfProfile(currProfileIndex);
	BitsPerSecond newBandwidth     = getBandwidthOfProfile(newProfileIndex);
	AAMPLOG_INFO("currProfileIndex %d newProfileIndex %d currentBandwidth %" BITSPERSECOND_FORMAT " newBandwidth %" BITSPERSECOND_FORMAT " bufferValue %lf, minBufferNeeded %lf", currProfileIndex, newProfileIndex, currentBandwidth, newBandwidth, bufferValue, minBufferNeeded);
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

void ABRManager::CheckRampupFromSteadyState(int currProfileIndex,int &newProfileIndex,BitsPerSecond nwBandwidth,double bufferValue,BitsPerSecond newBandwidth,BitrateChangeReason &mhBitrateReason,int &mMaxBufferCountCheck,const std::string& periodId)
{
	if (nwBandwidth <= 0)
	{
		AAMPLOG_WARN("nwBandwidth is %" BITSPERSECOND_FORMAT ", skipping rampup check", nwBandwidth);
		return;
	}
	int abrThreshold = (int)((newBandwidth - nwBandwidth) * 100) / (int)nwBandwidth;
	AAMPLOG_INFO("currProfileIndex %d newProfileIndex %d nwBandwidth %" BITSPERSECOND_FORMAT " bufferValue %lf newBandwidth %" BITSPERSECOND_FORMAT " threshold %d", currProfileIndex, newProfileIndex, nwBandwidth, bufferValue, newBandwidth, abrThreshold);
	int nProfileIdx = getRampedUpProfileIndex(currProfileIndex,periodId);
	// switch to new profile only on bitrate difference is less than 30 percentage
	if(abrThreshold >= 0 && abrThreshold <= 30)
		newProfileIndex = nProfileIdx;
	if(newProfileIndex  != currProfileIndex)
	{
		AAMPLOG_WARN("attempted rampup from steady state currProfileIndex %d newProfileIndex %d bufferValue %lf threshold %d", currProfileIndex, newProfileIndex, bufferValue, abrThreshold);
		mRampupFromSteadyStateLoop = (++mRampupFromSteadyStateLoop >4)?1:mRampupFromSteadyStateLoop;
		mMaxBufferCountCheck =  pow(eAAMPAbrConfig.abrBufferCounter,mRampupFromSteadyStateLoop);
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
 */
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
bool ABRManager::IsABRDataGoodToEstimate(long time_diff)
{
	return time_diff >= DEFAULT_ABR_ELAPSED_MILLIS_FOR_ESTIMATE;
}

/**
 * @brief to Update the ChunkSpeedData based on low latency ABR speedstoreSize
 * @params speedcache struct
 * @params estimated-bps
 * @return None
 */
void ABRManager::CheckLLDashABRSpeedStoreSize(struct SpeedCache *speedcache,BitsPerSecond &bitsPerSecond,long time_now,long total_dl_diff,long time_diff,long currentTotalDownloaded)
{
	speedcache->last_sample_time_val = time_now;
	//speed @ bits per second
	speedcache->speed_now = (total_dl_diff*8000L)/time_diff;
	
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
BitsPerSecond ABRManager::FragmentfailureRampdown(int currentBuffer, int currentProfileIndex)
{
	double bufferPercentage = ((double)currentBuffer / eAAMPAbrConfig.abrMaxBuffer) * 100;
	BitsPerSecond desiredProfilebw = 0;
	BitsPerSecond currentbw = getBandwidthOfProfile(currentProfileIndex);
	std::vector<ProfileInfo> availableProfiles = mProfiles;
	int i = (int)availableProfiles.size();
	if( i>0 )
	{
		std::sort(availableProfiles.begin(), availableProfiles.end(), [](const ProfileInfo& a, const ProfileInfo& b) {
			return a.bandwidthBitsPerSecond < b.bandwidthBitsPerSecond;
		});
		// Iterate over profiles in descending order of bandwidth
		double k = 100.0/availableProfiles[i-1].bandwidthBitsPerSecond;
		while( i-- )
		{
			double profilePercentage = availableProfiles[i].bandwidthBitsPerSecond*k;
			AAMPLOG_WARN("Index: %d, bandwidth %" BITSPERSECOND_FORMAT ", profile percentage %lf, buffer percentage %lf", i, availableProfiles[i].bandwidthBitsPerSecond, profilePercentage, bufferPercentage);
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
	}
	return desiredProfilebw;
}
