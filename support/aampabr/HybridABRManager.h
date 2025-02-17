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
 * @file HybridABRManager.h
 * @brief Handles operations on Hybrid ABR functionalities
 ***************************************************/
#ifndef HYBRID_ABR_MANAGER_H
#define HYBRID_ABR_MANAGER_H

#include<iostream>
#include <vector>
#include <map>
#include <string>
#include <cstdio>
#include "ABRManager.h"

class HybridABRManager:public ABRManager
{
	public:
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
			eCURL_ABORT_REASON_LOW_BANDWIDTH_TIMEDOUT
		};


		int mABRHighBufferCounter;	    /**< ABR High buffer counter */
		int mABRLowBufferCounter;	    /**< ABR Low Buffer counter */

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

		bool bLowLatencyStartABR;             /**<Low Latency ABR Start Status */
		bool bLowLatencyServiceConfigured;    /**<Low Latency Service Configuration Status */
		double mLLDashCurrentPlayRate;        /**<Low Latency Current play Rate */
	public:

		/** @brief Read Config values
		 *   @params AampAbrConfig struct
		 *  @return none
		 */
		void ReadPlayerConfig(AampAbrConfig *mAampAbrConfig);


		/**
		 * @brief function to update downloadbps based on abrthreshold size
		 * @params bufferlen-Growable Buffer length
		 * @params downloadTimeMS -download time in ms
		 * @params currentProfilebps - CurrentProfile Bandwidth
		 * @params fragmentDurationMs - Total downloaded fragments duration in ms
		 * @params HTTP Header Type
		 * @return downloadbps
		 */
		long CheckAbrThresholdSize(int bufferlen, int downloadTimeMs ,long currentProfilebps, int fragmentDurationMs, CurlAbortReason abortReason);

		/**
		 * @brief to update Bitrate Data
		 * @params BitrateData vector
		 * @params download Bitrate
		 * @return none
		 */
		void UpdateABRBitrateDataBasedOnCacheLength(std::vector < std::pair<long long,long> > &mAbrBitrateData ,long downloadbps,bool LowLatencyMode );

		/**
		 * @brief Update Bitrate Data based on ABR CacheLife
		 * @params BitrateData vector
		 * @params tmpData vector
		 * @return none
		 */
		void UpdateABRBitrateDataBasedOnCacheLife(std::vector < std::pair<long long,long> > &mAbrBitrateData , std::vector< long> &tmpData);

		/**
		 * @@brief Update Bitrate Data based on ABRCacheOutlier
		 * @params tmpData vector
		 * @return none
		 */

		long UpdateABRBitrateDataBasedOnCacheOutlier(std::vector< long> &tmpData);

		/**
		 * @brief fcurrent network bandwidth using most recently recorded 3 sample function to check profilechange is needed or not
		 * @params totalFetchedDuration - Total fragment fetched duration
		 * @params currProfileIndex -current profileIndex
		 * @params availBW -current network bandwidth using most recently recorded 3 samples
		 * @return bool -true if profilechange needed else false
		 */

		bool CheckProfileChange(double totalFetchedDuration ,int currProfileIndex , long availBW);

		/*
		 * @brief function to check whether the profileidx is the lowest profile or not
		 * @params currentProfileIndex -current profile index to be checked.
		 * @params bool IsTrickmode - true if it is a trickplay,else false
		 * @retune - true if it is lowest profile ,else false
		 */
		bool IsLowestProfile(int currentProfileIndex , bool IsTrickmode);

		/*
		 * @brief Get Desired Profile based on Buffer availability
		 * @params currentProfileIndex , newProfileIndex -current and new profile
		 * @params currentBandwidth current profileIdx bitrate
		 * @params newBandwidth - bitrate of new profileIdx
		 * @params  bufferValue -Buffer availability
		 * @params minBufferNeeded - Minimum Buffer Needed
		 * @return none
		 */

		void GetDesiredProfileOnBuffer(int currProfileIndex,int &newProfileIndex,double bufferValue,double minBufferNeeded,const std::string& periodId= std::string());

		/*
		 * @brief function to update newprofileindex ,if rampup happen from steady state
		 * @params currentProfileIndex , newProfileIndex -current and new profile Idx
		 * @params nwBandwidth - current network bandwidth using most recently recorded 3 samples
		 * @params bufferValue -Buffer availability
		 * @params newBandwidth - bitrate of new profileIdx
		 * @params BitrateChangeReason is getting updated only if rampup occur
		 * @return none
		 */

		void CheckRampupFromSteadyState(int currProfileIndex,int &newProfileIndex,long nwBandwidth,double bufferValue,long newBandwidth,BitrateChangeReason &mhBitrateReason,int &mMaxBufferCountCheck,const std::string& periodId= std::string());

		/*
		 * @brief function to update newprofileindex ,if rampdown happen from steady state
		 * @params currentProfileIndex , newProfileIndex -current and new profile Idx
		 * @params BitrateChangeReason is getting updated only if rampdown occurred
		 * @params ABR Low Buffer counter
		 * @return none
		 */
		void CheckRampdownFromSteadyState(int currProfileIndex, int &newProfileIndex,BitrateChangeReason &mBitrateReason,int mABRLowBufferCounter,const std::string& periodId= std::string());

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
		 * @params speedcache struct
		 * @params  estimated-bps
		 * @params current time,time difference ,
		 * @return None
		 */
		void CheckLLDashABRSpeedStoreSize(struct SpeedCache *speedcache,long &bitsPerSecond,long time_now,long total_dl_diff,long time_diff,long currentTotalDownloaded);

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
		long FragmentfailureRampdown(int currentBuffer,int currentProfileIndex);
};
#endif
