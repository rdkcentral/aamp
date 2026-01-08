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
 * @file abr.h
 * @brief Handles operations on Hybrid ABR functionalities
 ***************************************************/
#ifndef ABR_H
#define ABR_H

#include <iostream>
#include <vector>
#include <map>
#include <string>
#include <cstdio>
#include <mutex>
#include "AampMediaType.h"

class ABRManager
{
public:
	~ABRManager() = default;
	
	struct ProfileInfo {
		/**
		 * @brief Is iframe track
		 */
		bool isIframeTrack;
		
		/**
		 * @brief Bandwidth / second (Bitrate)
		 */
		BitsPerSecond bandwidthBitsPerSecond;
		
		/**
		 * @brief Width of resolution (optional)
		 */
		int width;
		
		/**
		 * @brief Height of resolution (optional)
		 */
		int height;
		
		/**
		 * @brief period-Id of profiles (optional)
		 */
		std::string periodId;
		
		/**
		 * @brief profileIndex or PeriodIndex (optional)
		 */
		int userData;
	};
	
	/**
	 * @brief Persist Network Bandwidth
	 */
	static BitsPerSecond mPersistBandwidth;
	
	/**
	 * @brief Persist Network Bandwidth Updated Time
	 */
	
	static long long mPersistBandwidthUpdatedTime;
	
	
	/**
	 * @brief Invalid profile index
	 */
	static const int INVALID_PROFILE = -1;
	
	
	/**
	 * @fn getInitialProfileIndex
	 *
	 * @param chooseMediumProfile Boolean flag, true means
	 * to choose the medium profile, otherwise to choose the profile whose
	 * bitrate >= the default bitrate.
	 * @param periodId empty string by default, Period-Id of the profiles
	 * added to ABR map
	 *
	 * @return The initial profile index
	 */
	int getInitialProfileIndex(bool chooseMediumProfile, const std::string& periodId= std::string());
	
	/**
	 * @fn updateProfile
	 * @return void
	 */
	void updateProfile();
	
	/**
	 * @fn getBestMatchedProfileIndexByBandWidth
	 * @param bandWidth  The given bandwidth
	 * @return the best matched profile index
	 */
	int getBestMatchedProfileIndexByBandWidth(int bandwidth);
	
	/**
	 * @fn getRampedDownProfileIndex
	 *
	 * @param currentProfileIndex The current profile index
	 * @param periodId empty string by default, Period-Id of profiles
	 *
	 * @return the profile index of a lower bitrate (one step)
	 */
	int getRampedDownProfileIndex(int currentProfileIndex, const std::string& periodId= std::string());
	
	/**
	 * @fn getRampedUpProfileIndex
	 *
	 * @param currentProfileIndex The current profile index
	 * @param periodId empty string by default, Period-Id of profiles
	 *
	 * @return the profile index of a upper bitrate (one step)
	 */
	int getRampedUpProfileIndex(int currentProfileIndex, const std::string& periodId= std::string());
	
	/**
	 * @fn isProfileIndexBitrateLowest
	 *
	 * @param currentProfileIndex The current profile index
	 * @param periodId empty string by default, Period-Id of profiles
	 *
	 * @return True means it reaches to the lowest, otherwise, it doesn't.
	 */
	bool isProfileIndexBitrateLowest(int currentProfileIndex, const std::string& periodId= std::string());
	
	/**
	 * @fn getProfileIndexByBitrateRampUpOrDown
	 *
	 * @param currentProfileIndex The current profile index
	 * @param currentBandwidth The current band width
	 * @param networkBandwidth The current available bandwidth (network bandwidth)
	 * @param nwConsistencyCnt Network consistency count, used for bitrate ramping up/down
	 * @param periodId empty string by default, Period-Id of profiles
	 * @return int Profile index
	 */
	int getProfileIndexByBitrateRampUpOrDown(int currentProfileIndex, BitsPerSecond currentBandwidth, BitsPerSecond networkBandwidth, int nwConsistencyCnt = DEFAULT_ABR_NW_CONSISTENCY_COUNT, const std::string& periodId= std::string());
	
	/**
	 * @fn getBandwidthOfProfile
	 *
	 * @param profileIndex The profile index
	 * @return bandwidth of the profile
	 */
	BitsPerSecond getBandwidthOfProfile(int profileIndex);
	
	/**
	 * @brief Get profile of bandwidth
	 *
	 * @param bandwidth The bandwidth
	 * @return int index of the bandwidth
	 */
	int getProfileOfBandwidth(BitsPerSecond bandwidth);
	
	/**
	 * @fn getMaxBandwidthProfile
	 *
	 * @param periodId empty string by default, Period-Id of profiles
	 *
	 * @return int index of the max bandwidth
	 */
	int getMaxBandwidthProfile(const std::string& periodId = std::string());
	
	/**
	 * @fn getProfileIndexForLowestBandwidth
	 *
	 * @return int index for lowest bitrate
	 */
	int getProfileIndexForLowestBandwidth();
	/**
	 * @fn getClosestProfileIndexByBandwidth
	 *
	 * @return int index for best matched bitrate
	 */
	int getClosestProfileIndexByBandwidth( BitsPerSecond inputBandwidth );
	
	// Getters/Setters
	
	/**
	 * @fn getProfileCount
	 *
	 * @return The number of profiles
	 */
	int getProfileCount();
	
	/**
	 * @fn setDefaultInitBitrate
	 *
	 * @param defaultInitBitrate Default init bitrate
	 */
	void setDefaultInitBitrate(BitsPerSecond defaultInitBitrate);
	
	/**
	 * @fn getLowestIframeProfile
	 *
	 * @return the lowest iframe profile index.
	 */
	int getLowestIframeProfile() const;
	
	/**
	 * @fn getDesiredIframeProfile
	 *
	 * @return the desired iframe profile index.
	 */
	int getDesiredIframeProfile() const;
	
	/**
	 * @fn addProfile
	 * @param profile The profile info
	 */
	void addProfile(const ProfileInfo &profile);
	
	/**
	 * @fn clearProfiles
	 * @return void
	 */
	void clearProfiles();
	
	/**
	 * @fn removeProfiles
	 * @param[in] vector of profile bitrates to remove from ABR data
	 * @param[in] currentProfileIndex
	 * @param[in] period Id empty string by default, Period-Id of profiles
	 * @return modified ProfileIndex
	 */
	int removeProfiles(std::vector<BitsPerSecond> profileBPS, int currentProfileIndex, const std::string& periodId = std::string());
	
	/**
	 * @fn setDefaultIframeBitrate
	 *
	 * @param defaultIframeBitrate Default iframe bitrate
	 */
	void setDefaultIframeBitrate(BitsPerSecond defaultIframeBitrate);
	/**
	 * @fn getUserDataOfProfile
	 *
	 * @param profileIndex The profile index
	 * @return int userdata / period index
	 */
	int getUserDataOfProfile(int profileIndex);
	/**
	 * @brief Set the Persist Network Bandwidth
	 *
	 * @param network bitrate
	 */
	static void setPersistBandwidth(BitsPerSecond bitrate){mPersistBandwidth = bitrate;}
	/**
	 * @brief Get Persisted Network Bandwidth
	 *
	 * @return  bandwidth
	 */
	static BitsPerSecond getPersistBandwidth() { return mPersistBandwidth;}
	
	/*
	 * @brief Configuration related to AampABR
	 */
	struct AampAbrConfig
	{
		/**
		 * @brief Adaptive bitrate cache life in seconds
		 */
		int abrCacheLife;
		
		/**
		 * @brief Adaptive bitrate cache length
		 */
		int abrCacheLength;
		
		/**
		 * @brief Initial duration for ABR skip
		 */
		int abrSkipDuration;
		/**
		 * @brief Adaptive bitrate network consistency
		 */
		int abrNwConsistency;
		
		/**
		 * @brief AAMP ABR threshold size
		 */
		int abrThresholdSize;
		
		/**
		 * @brief Maximum ABR Buffer for Rampup
		 */
		int abrMaxBuffer;
		
		/**
		 * @brief Minimum ABR Buffer for Rampdown
		 */
		int abrMinBuffer;
		
		/**
		 * @brief Adaptive bitrate outlier, if values goes beyond this
		 */
		int abrCacheOutlier;
		
		/**
		 * @brief Counter value used for steady state rampup/rampdown
		 */
		int abrBufferCounter;
		/**
		 * @brief Enables Info logging
		 */
		bool infologging;
		
		/**
		 * @brief Enables Trace logging
		 */
		bool tracelogging;
		
		/**
		 * @brief Enables Warn Logging
		 */
		bool warnlogging;
		
		/**
		 * @brief Enables Debug Logging
		 */
		bool debuglogging;
		
		// Constructor to initialize all members
		AampAbrConfig()
		: abrCacheLife(0), abrCacheLength(0), abrSkipDuration(0), abrNwConsistency(0),
		abrThresholdSize(0), abrMaxBuffer(0), abrMinBuffer(0), abrCacheOutlier(0),
		abrBufferCounter(0), infologging(false), tracelogging(false),
		warnlogging(false), debuglogging(false) {}
	};
	
	/**
	 * @brief Http Header Type
	 */
	enum CurlAbortReason
	{
		eCURL_ABORT_REASON_NONE = 0,
		eCURL_ABORT_REASON_STALL_TIMEDOUT,
		eCURL_ABORT_REASON_START_TIMEDOUT,
		eCURL_ABORT_REASON_LOW_BANDWIDTH_TIMEDOUT,
		eCURL_ABORT_REASON_FIRST_CHUNK_SLOW
	};
	
	/**
	 * @brief Different reasons for bitrate change
	 */
	typedef enum
	{
		eAAMP_BITRATE_CHANGE_BY_ABR = 0,
		eAAMP_BITRATE_CHANGE_BY_RAMPDOWN = 1,
		eAAMP_BITRATE_CHANGE_BY_TUNE = 2,
		eAAMP_BITRATE_CHANGE_BY_SEEK = 3,
		eAAMP_BITRATE_CHANGE_BY_TRICKPLAY = 4,
		eAAMP_BITRATE_CHANGE_BY_BUFFER_FULL = 5,
		eAAMP_BITRATE_CHANGE_BY_BUFFER_EMPTY = 6,
		eAAMP_BITRATE_CHANGE_BY_FOG_ABR = 7,
		eAAMP_BITRATE_CHANGE_BY_OTA = 8,
		eAAMP_BITRATE_CHANGE_BY_HDMIIN = 9,
		eAAMP_BITRATE_CHANGE_MAX = 10
	} BitrateChangeReason;
	
	/**
	 * @brief Read Config values
	 * @param AampAbrConfig struct
	 * @return none
	 */
	void ReadPlayerConfig(AampAbrConfig *mAampAbrConfig);
	
	/**
	 * @brief function to update downloadbps based on abrthreshold size
	 * @param bufferlen-Growable Buffer length
	 * @param downloadTimeMS -download time in ms
	 * @param currentProfilebps - CurrentProfile Bandwidth
	 * @param fragmentDurationMs - Total downloaded fragments duration in ms
	 * @param HTTP Header Type
	 * @return downloadbps
	 */
	BitsPerSecond CheckAbrThresholdSize(int bufferlen, int downloadTimeMs, BitsPerSecond currentProfilebps, int fragmentDurationMs, CurlAbortReason abortReason);
	
	/**
	 * @brief to update Bitrate Data
	 * @param mAbrBitrateData collection of recent (timestamp, estimated network bandwidth) samples
	 * @param downloadbps most recent estimate of network bandwidth
	 * @param lowLatencyMode true if playing low-latency stream
	 */
	void UpdateABRBitrateDataBasedOnCacheLength(std::vector<std::pair<long long,BitsPerSecond>> &mAbrBitrateData, BitsPerSecond downloadbps, bool LowLatencyMode );
	
	/**
	 * @brief Update Bitrate Data based on ABR CacheLife
	 * @param BitrateData vector
	 * @param tmpData vector
	 * @return none
	 */
	void UpdateABRBitrateDataBasedOnCacheLife(std::vector<std::pair<long long,BitsPerSecond>> &mAbrBitrateData, std::vector<BitsPerSecond> &tmpData);
	
	/**
	 * @brief Update Bitrate Data based on ABRCacheOutlier
	 * @param tmpData vector
	 * @return none
	 */
	BitsPerSecond UpdateABRBitrateDataBasedOnCacheOutlier(std::vector<BitsPerSecond> &tmpData);
	
	/**
	 * @brief Checks if a profile change is needed based on the most recently recorded network bandwidth samples and total fetched fragment duration.
	 * @param totalFetchedDuration - Total fragment fetched duration
	 * @param currProfileIndex - Current profile index
	 * @param availBW - Current network bandwidth using most recently recorded 3 samples
	 * @return bool - true if profile change is needed, else false
	 */
	bool CheckProfileChange(double totalFetchedDuration, int currProfileIndex, BitsPerSecond availBW);
	
	/*
	 * @brief Get Desired Profile based on Buffer availability
	 * @param currentProfileIndex, newProfileIndex -current and new profile
	 * @param currentBandwidth current profile index bitrate
	 * @param newBandwidth - bitrate of new profile index
	 * @param  bufferValue -Buffer availability
	 * @param minBufferNeeded - Minimum Buffer Needed
	 * @return none
	 */
	void GetDesiredProfileOnBuffer(int currProfileIndex,int &newProfileIndex,double bufferValue,double minBufferNeeded,const std::string& periodId= std::string());
	
	/*
	 * @brief function to update newprofileindex, if rampup happen from steady state
	 * @param currentProfileIndex - current profile index
	 * @param newProfileIndex - new profile index
	 * @param nwBandwidth - current network bandwidth using most recently recorded 3 samples
	 * @param bufferValue - buffer availability
	 * @param newBandwidth - bitrate of new profile index
	 * @param BitrateChangeReason is getting updated only if rampup occur
	 * @return none
	 */
	void CheckRampupFromSteadyState(int currProfileIndex, int &newProfileIndex, BitsPerSecond nwBandwidth, double bufferValue, BitsPerSecond newBandwidth, BitrateChangeReason &mhBitrateReason, int &mMaxBufferCountCheck, const std::string& periodId= std::string());
	
	/*
	 * @brief function to update newprofileindex if rampdown happen from steady state
	 *
	 * @param currentProfileIndex - current profile index
	 * @param newProfileIndex - new profile index
	 * @param BitrateChangeReason is getting updated only if rampdown occurred
	 * @param mABRLowBuffer counter
	 * @return none
	 */
	void CheckRampdownFromSteadyState(int currProfileIndex, int &newProfileIndex, BitrateChangeReason &mBitrateReason, int mABRLowBufferCounter, const std::string& periodId=std::string());
	
	/**
	 * @brief aampabr_GetCurrentTimeMS
	 *
	 */
	long long ABRGetCurrentTimeMS(void);
	
	
	/**
	 *    @brief function to get LowLatencyStartABR status
	 *    @return bool
	 */
	bool GetLowLatencyStartABR();
	
	/**
	 *     @brief function to Set LowLatencyStartABR status
	 *     @param[in] bStart - bool flag
	 *     @return void
	 */
	void SetLowLatencyStartABR(bool bStart);
	
	/**
	 *     @brief function to get LowLatencyServiceConfigured
	 *     @return bool
	 */
	bool GetLowLatencyServiceConfigured();
	
	/**
	 *     @brief function to Set LowLatencyServiceConfigured
	 *     @param[in] bConfig - bool flag
	 *     @return void
	 */
	void SetLowLatencyServiceConfigured(bool bConfig);
	
	/**
	 * @brief to Update the ChunkSpeedData based on low latency ABR speedstoreSize
	 * @param speedcache struct
	 * @param estimated-bps
	 * @param current time,time difference
	 * @return None
	 */
	void CheckLLDashABRSpeedStoreSize(struct SpeedCache *speedcache, BitsPerSecond &bitsPerSecond, long time_now, long total_dl_diff, long time_diff, long currentTotalDownloaded);
	
	/**
	 * @brief to Check if it is Good to capture speed sample
	 * @param time_diff Time Diff
	 * @return bool Good to Estimate
	 */
	bool IsABRDataGoodToEstimate(long time_diff);
	
	/**
	 * @brief - Fn to rampdown during fragment download failure based on buffer
	 * @param - current available buffer
	 * @return - desired profile based on buffer
	 */
	BitsPerSecond FragmentfailureRampdown(int currentBuffer,int currentProfileIndex);
	
private:
	bool bLowLatencyStartABR;             /**<Low Latency ABR Start Status */
	bool bLowLatencyServiceConfigured;    /**<Low Latency Service Configuration Status */
	
	/**
	 * @brief Add new profile info to sorted BW list
	 * @param[in] profileInfo profile info
	 * @param[in] idx profile index in list
	 */
	void addSortedBWProfileList(const ABRManager::ProfileInfo &profileInfo, int idx);
	
	/**
	 * @brief A list of available profiles.
	 */
	std::vector<ProfileInfo> mProfiles;
	
	/**
	 * @brief A sorted list of profiles with periodId.
	 * Populate the container with sorted order of BW (Bandwidth) vs its index under each periodId
	 */
	std::map<std::string, std::map<BitsPerSecond,int>> mSortedBWProfileList;
	
	/**
	 * @brief Define type: iterator of SortedBWProfileListIter
	 */
	typedef std::map<BitsPerSecond, int>::iterator SortedBWProfileListIter;
	
	/**
	 * @brief Define type: reverse iterator of SortedBWProfileListIter
	 */
	typedef std::map<BitsPerSecond, int>::reverse_iterator SortedBWProfileListRevIter;
	
	/**
	 * @brief state for CheckRampupFromSteadyState
	 */
	int mRampupFromSteadyStateLoop = 1;
	
	/**
	 * @brief Lowest iframe Profile index
	 */
	int mLowestIframeProfile = INVALID_PROFILE;
	
	/**
	 * @brief Desired iframe Profile index
	 */
	int mDesiredIframeProfile = 0;
	
	/**
	 * @brief Default initialization bitrate
	 */
	BitsPerSecond mDefaultInitBitrate = DEFAULT_BITRATE;
	
	/**
	 * @brief The number of ABR profiles that ramping up
	 */
	int mAbrProfileChangeUpCount = 0;
	
	/**
	 * @brief The number of ABR profiles that ramping down
	 */
	int mAbrProfileChangeDownCount = 0;
	
	/**
	 * @brief Default iframe bitrate
	 */
	BitsPerSecond mDefaultIframeBitrate = 0;
	/**
	 * @brief Default init bitrate value.
	 */
	static const int DEFAULT_BITRATE = 1000000;
	
	/**
	 * @brief The width of 4K video
	 */
	static const int WIDTH_FULL_HD = 1920;
	
	/**
	 * @brief The height of 4K video
	 */
	static const int HEIGHT_FULL_HD = 1080;
	
	/**
	 * @brief The default value of the network consistency count.
	 * If the profile change exceeds this value, the ABR will be performed.
	 * Used when bitrate ramping up/down
	 */
	static const int DEFAULT_ABR_NW_CONSISTENCY_COUNT = 2;
	
	/**
	 * @brief Mutex for make internal structures thread safe
	 */
	std::mutex mProfileLock;
};
#endif // !ABR_H
