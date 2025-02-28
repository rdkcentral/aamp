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
 * @file HybridABRManager.cpp
 * @brief Handles operations on Hybrid ABR functionalities
 ***************************************************/

#include <vector>
#include <map>
#include <string>
#include <cstdio>
#include "HybridABRManager.h"
#include <cmath>
#include <chrono>
#include <cstdio>
#include <stdarg.h>
#if !defined(__APPLE__)
#if defined(USE_SYSTEMD_JOURNAL_PRINT)
#define ENABLE_RDK_LOGGER true
#include <systemd/sd-journal.h>
#elif defined(USE_SYSLOG_HELPER_PRINT)
#define ENABLE_RDK_LOGGER true
#include "syslog_helper_ifc.h"
#endif
#endif
#include <sys/time.h>
#include <algorithm>

#define MAX_DEBUG_LOG_BUFF_SIZE 1024
#define DEFAULT_ABR_CHUNK_CACHE_LENGTH	10					/**< Default ABR chunk cache length */
#define DEFAULT_ABR_ELAPSED_MILLIS_FOR_ESTIMATE	100			        /**< Duration(ms) to check Chunk Speed */
#define MAX_LOW_LATENCY_DASH_ABR_SPEEDSTORE_SIZE 10
//Low Latency DASH SERVICE PROFILE URL
#define LL_DASH_SERVICE_PROFILE "http://www.dashif.org/guidelines/low-latency-live-v5"

#define AAMPABRLOG(LEVEL,LEVELSTR,FORMAT, ...) \
	do { \
		if(LEVEL) { \
			ABRLogger(LEVELSTR,__FUNCTION__, __LINE__,FORMAT,##__VA_ARGS__); }\
	} while (0)


#define AAMPABRLOG_TRACE(FORMAT, ...) AAMPABRLOG(eAAMPAbrConfig.tracelogging,"TRACE",FORMAT, ##__VA_ARGS__)
#define AAMPABRLOG_INFO(FORMAT, ...)  AAMPABRLOG(eAAMPAbrConfig.infologging,"INFO",FORMAT, ##__VA_ARGS__)
#define AAMPABRLOG_WARN(FORMAT, ...)  AAMPABRLOG(eAAMPAbrConfig.warnlogging,"WARN",FORMAT, ##__VA_ARGS__)
#define AAMPABRLOG_ERR(FORMAT, ...)   AAMPABRLOG(eAAMPAbrConfig.debuglogging,"ERROR",FORMAT, ##__VA_ARGS__)

HybridABRManager::AampAbrConfig eAAMPAbrConfig;

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

	SpeedCache() : last_sample_time_val(0), prev_dlnow(0), prevSampleTotalDownloaded(0), totalDownloaded(0), speed_now(0), start_val(0), bStart(false) , totalWeight(0), weightedBitsPerSecond(0), mChunkSpeedData()
	{
	}
};

/** @brief Read Config values
 *  @return none
 */
void HybridABRManager::ReadPlayerConfig(AampAbrConfig *mAampAbrConfig)
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
	logprintf("[%s][%d]PlayerConfig : ABRCacheLife %d ,ABRCacheLength %d ,ABRSkipDuration %d , ABRNwConsistency %d ,ABRThresholdSize %d ,ABRMaxBuffer %d ,ABRMinBuffer %d ABRCacheOutlier %d ABRBufferCounter %d ",__FUNCTION__,__LINE__,eAAMPAbrConfig.abrCacheLife,eAAMPAbrConfig.abrCacheLength,eAAMPAbrConfig.abrSkipDuration,eAAMPAbrConfig.abrNwConsistency,eAAMPAbrConfig.abrThresholdSize,eAAMPAbrConfig.abrMaxBuffer,eAAMPAbrConfig.abrMinBuffer,eAAMPAbrConfig.abrCacheOutlier,eAAMPAbrConfig.abrBufferCounter);
}


/**
 * @brief function to update downloadbps value when video segment size is greater than abrthreshold size
 * @return downloadbps
 */

long HybridABRManager::CheckAbrThresholdSize(int bufferlen, int downloadTimeMs ,long currentProfilebps ,int fragmentDurationMs , CurlAbortReason abortReason)
{
	long downloadbps = ((long)(bufferlen / downloadTimeMs)*8000);
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
void HybridABRManager::UpdateABRBitrateDataBasedOnCacheLength(std::vector < std::pair<long long,long> > &mAbrBitrateData,long downloadbps,bool LowLatencyMode)
{
	mAbrBitrateData.push_back(std::make_pair(ABRGetCurrentTimeMS() ,downloadbps));
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
void HybridABRManager::UpdateABRBitrateDataBasedOnCacheLife(std::vector < std::pair<long long,long> > &mAbrBitrateData , std::vector< long> &tmpData)
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
long HybridABRManager::UpdateABRBitrateDataBasedOnCacheOutlier(std::vector< long> &tmpData)
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
		//AAMPLOG_WARN("No prior data available for abr , return -1 ");
		ret = -1;
	}
	return ret;

}
/*
 * @brief Function for ABR check for each segment download
 * @return bool true if profilechange needed else false
 */

bool HybridABRManager::CheckProfileChange(double totalFetchedDuration ,int currProfileIndex , long availBW)
{
	bool checkProfileChange = true;
	long currBW = getBandwidthOfProfile(currProfileIndex);
	//Avoid doing ABR during initial buffering which will affect tune times adversely
	if ( totalFetchedDuration > 0 && totalFetchedDuration < eAAMPAbrConfig.abrSkipDuration)
	{
		AAMPABRLOG_TRACE("[%s][%d] TotalFetchedDuration %lf ",__FUNCTION__,__LINE__,totalFetchedDuration);
		//For initial fragment downloads, check available bw is less than default bw
		//If available BW is less than current selected one, we need ABR
		if (availBW > 0 && availBW < currBW)
		{
			AAMPABRLOG_WARN("Changing profile due to low available bandwidth(%ld) than default(%ld)!! ", availBW, currBW);

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

void HybridABRManager::GetDesiredProfileOnBuffer(int currProfileIndex,int &newProfileIndex,double bufferValue,double minBufferNeeded,const std::string& periodId)
{
	long currentBandwidth = getBandwidthOfProfile(currProfileIndex);
	long newBandwidth     = getBandwidthOfProfile(newProfileIndex);
	AAMPABRLOG_INFO("[%s][%d] CurrProfileIndex %d ,newProfileIndex %d,CurrentBandwidth %ld,newBandwidth %ld,BufferValue %lf ,minBufferNeeded %lf",__FUNCTION__, __LINE__, currProfileIndex,newProfileIndex,currentBandwidth,newBandwidth,bufferValue,minBufferNeeded);
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
			// Also if delta of current profile to new profile is 1 , then ignore the change
			// if bigger rampdown , then adjust to new profile
			// else retain current profile
			if(bufferValue > minBufferNeeded && getRampedDownProfileIndex(currProfileIndex,periodId) == newProfileIndex)
				newProfileIndex = currProfileIndex;
		}
	}
}

/**
 *  @brief Get Desired Profile on steady state while rampup
 */

void HybridABRManager::CheckRampupFromSteadyState(int currProfileIndex,int &newProfileIndex,long nwBandwidth,double bufferValue,long newBandwidth,BitrateChangeReason &mhBitrateReason,int &mMaxBufferCountCheck,const std::string& periodId)
{
	int abrThreshold = (int)((newBandwidth - nwBandwidth) * 100) / (int)nwBandwidth;
	AAMPABRLOG_INFO("[%s][%d]  currProfileIndex %d, newProfileIndex %d ,nwBandwidth %ld ,bufferValue %lf ,newBandwidth %ld threshold %d(30)",__FUNCTION__,__LINE__,currProfileIndex,newProfileIndex,nwBandwidth,bufferValue,newBandwidth, abrThreshold);
	int nProfileIdx = getRampedUpProfileIndex(currProfileIndex,periodId);
	// switch to new profile only on bitrate difference is less than 30 percentage
	if(abrThreshold >= 0 && abrThreshold <= 30)
		newProfileIndex = nProfileIdx;
	if(newProfileIndex  != currProfileIndex)
	{
		static int loop = 1;
		AAMPABRLOG_WARN("Attempted rampup from steady state ->currProf:%d newProf:%d bufferValue:%lf threshold:%d(30)",
				currProfileIndex,newProfileIndex,bufferValue,abrThreshold);
		loop = (++loop >4)?1:loop;
		mMaxBufferCountCheck =  pow(eAAMPAbrConfig.abrBufferCounter,loop);
		mhBitrateReason = eAAMP_BITRATE_CHANGE_BY_BUFFER_FULL;
	}
}

/**
 * @brief Get Desired Profile on steady state while rampdown
 */

void HybridABRManager::CheckRampdownFromSteadyState(int currProfileIndex, int &newProfileIndex,BitrateChangeReason &mBitrateReason,int mABRLowBufferCounter,const std::string& periodId)
{
	AAMPABRLOG_INFO("[%s][%d] currProfileIndex %d ,newProfileIndex %d, mABRLowBufferCounter %d",__FUNCTION__,__LINE__,currProfileIndex,newProfileIndex,mABRLowBufferCounter);
	if(mABRLowBufferCounter >= eAAMPAbrConfig.abrBufferCounter)
	{
		newProfileIndex = getRampedDownProfileIndex(currProfileIndex,periodId);
		if(newProfileIndex  != currProfileIndex)
		{
			mBitrateReason = eAAMP_BITRATE_CHANGE_BY_BUFFER_EMPTY;
			AAMPABRLOG_WARN("Attempted rampdown from steady state ->currProf:%d newProf:%d",currProfileIndex,newProfileIndex);
		}
	}
}

/**
 * @brief function to get currenttime in ms
 *
 **/

long long HybridABRManager::ABRGetCurrentTimeMS(void)
{
	struct timeval t;
	gettimeofday(&t, NULL);
	return (long long)(t.tv_sec*1e3 + t.tv_usec*1e-3);
}


/**
 *  @brief Get Low Latency ABR Start Status
 */
bool HybridABRManager::GetLowLatencyStartABR()
{
	return (this->bLowLatencyStartABR);
}

/**
 *  @brief Set Low Latency ABR Start Status
 */
void HybridABRManager::SetLowLatencyStartABR(bool bStart)
{
	this->bLowLatencyStartABR = bStart;
}


/**
 *  @brief Get Low Latency Service Configuration Status
 */
bool HybridABRManager::GetLowLatencyServiceConfigured()
{
	return (this->bLowLatencyServiceConfigured);
}

/**
 *  @brief Set Low Latency Service Configuration Status
 */
void HybridABRManager::SetLowLatencyServiceConfigured(bool bConfig)
{
	this->bLowLatencyServiceConfigured = bConfig;
}

/**
 * @brief Check if it is Good to capture speed sample
 * @param time_diff Time Diff
 * @retval bool Good to Estimate
 */
bool HybridABRManager::IsABRDataGoodToEstimate(long time_diff) {

	return time_diff >= DEFAULT_ABR_ELAPSED_MILLIS_FOR_ESTIMATE;
}


/**
 * @brief to Update the ChunkSpeedData based on low latency ABR speedstoreSize 
 * @params speedcache struct
 * @params estimated-bps
 * @return None
 */
void HybridABRManager::CheckLLDashABRSpeedStoreSize(struct SpeedCache *speedcache,long &bitsPerSecond,long time_now,long total_dl_diff,long time_diff,long currentTotalDownloaded)
{
	speedcache->last_sample_time_val = time_now;
	//speed @ bits per second
	speedcache->speed_now = ((long)(total_dl_diff / time_diff)* 8000);

	double weight = std::sqrt((double)total_dl_diff);
	speedcache->weightedBitsPerSecond += weight * speedcache->speed_now;
	speedcache->totalWeight += weight;
	speedcache->mChunkSpeedData.push_back(std::make_pair(weight ,speedcache->speed_now));
	
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
long HybridABRManager::FragmentfailureRampdown(int currentBuffer, int currentProfileIndex)
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
		AAMPABRLOG_WARN("Index: %d, bandwidth %d , profile percentage %lf, buffer percentage %lf",i,(int)availableProfiles[i].bandwidthBitsPerSecond,profilePercentage,bufferPercentage);
		// Check if profile bandwidth percentage is less than buffer percentage ,and it should be a rampdown
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
