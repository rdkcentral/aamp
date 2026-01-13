/*
* If not stated otherwise in this file or this component's license file the
* following copyright and licenses apply:
*
* Copyright 2022 RDK Management
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#include "abr.h"
#include "MockABRManager.h"

long ABRManager::mPersistBandwidth = 0;
long long ABRManager::mPersistBandwidthUpdatedTime = 0;

MockABRManager *g_mockABRManager = nullptr;

int ABRManager::getProfileCount()
{
	if (g_mockABRManager)
	{
		return g_mockABRManager->getProfileCount();
	}
	return 0;
}

int ABRManager::getBestMatchedProfileIndexByBandWidth(int bandwidth)
{
	return 0;
}

int ABRManager::getMaxBandwidthProfile(const std::string& periodId)
{
	return 0;
}

BitsPerSecond ABRManager::getBandwidthOfProfile(int profileIndex)
{
	return 0;
}

void ABRManager::clearProfiles()
{
	return;
}

void ABRManager::addProfile(const ABRManager::ProfileInfo &profile)
{
}

int ABRManager::getRampedDownProfileIndex(int currentProfileIndex, const std::string& periodId)
{
	return 0;
}

int ABRManager::getUserDataOfProfile(int currentProfileIndex)
{
	if (g_mockABRManager)
	{
		return g_mockABRManager->getUserDataOfProfile(currentProfileIndex);
	}
	return 0;
}

void ABRManager::setDefaultInitBitrate(long defaultInitBitrate)
{
}

void ABRManager::updateProfile()
{
}

int ABRManager::getDesiredIframeProfile() const
{
	return 0;
}

int ABRManager::getInitialProfileIndex(bool chooseMediumProfile, const std::string& periodId)
{
	return 0;
}

int ABRManager::getLowestIframeProfile() const
{
	return 0;
}

int ABRManager::getProfileIndexByBitrateRampUpOrDown(int currentProfileIndex, BitsPerSecond currentBandwidth, BitsPerSecond networkBandwidth, int nwConsistencyCnt, const std::string& periodId)
{
	return 0;
}

int ABRManager::getRampedUpProfileIndex(int currentProfileIndex, const std::string& periodId)
{
	return 0;
}

bool ABRManager::isProfileIndexBitrateLowest(int currentProfileIndex, const std::string& periodId)
{
	return true;
}

void ABRManager::setDefaultIframeBitrate(long defaultIframeBitrate)
{
}

int ABRManager::removeProfiles(std::vector<BitsPerSecond> profileBPS, int currentProfileIndex, const std::string& periodId)
{
	return 0;
}

int  ABRManager::getProfileIndexForLowestBandwidth()
{
	return 0;
}

int ABRManager::getClosestProfileIndexByBandwidth( long inputBandwidth )
{
	return 0;
}

void ABRManager::ReadPlayerConfig(AampAbrConfig *mAampAbrConfig)
{
}

long ABRManager::CheckAbrThresholdSize(int bufferlen, int downloadTimeMs ,long currentProfilebps ,int fragmentDurationMs , CurlAbortReason abortReason)
{
	return 0;
}

void ABRManager::UpdateABRBitrateDataBasedOnCacheLength(std::vector < std::pair<long long,long> > &mAbrBitrateData,long downloadbps,bool LowLatencyMode)
{
}

void ABRManager::UpdateABRBitrateDataBasedOnCacheLife(std::vector < std::pair<long long,long> > &mAbrBitrateData , std::vector<BitsPerSecond> &tmpData)
{
}

long ABRManager::UpdateABRBitrateDataBasedOnCacheOutlier(std::vector<BitsPerSecond> &tmpData)
{
	return 0;
}

bool ABRManager::CheckProfileChange(double totalFetchedDuration ,int currProfileIndex , long availBW)
{
	return false;
}

void ABRManager::GetDesiredProfileOnBuffer(int currProfileIndex,int &newProfileIndex,double bufferValue,double minBufferNeeded,const std::string& periodId)
{
}


void ABRManager::CheckRampupFromSteadyState(int currProfileIndex,int &newProfileIndex,long nwBandwidth,double bufferValue,long newBandwidth,BitrateChangeReason &mhBitrateReason,int &mMaxBufferCountCheck,const std::string& periodId)
{
}

void ABRManager::CheckRampdownFromSteadyState(int currProfileIndex, int &newProfileIndex,BitrateChangeReason &mBitrateReason,int mABRLowBufferCounter,const std::string& periodId)
{
}

long long ABRManager::ABRGetCurrentTimeMS(void)
{
	return 0;
}

bool ABRManager::GetLowLatencyStartABR()
{
	return false;
}

void ABRManager::SetLowLatencyStartABR(bool bStart)
{
}

bool ABRManager::GetLowLatencyServiceConfigured()
{
	return false;
}

void ABRManager::SetLowLatencyServiceConfigured(bool bConfig)
{
}

bool ABRManager::IsABRDataGoodToEstimate(long time_diff) 
{
	return false;
}

void ABRManager::CheckLLDashABRSpeedStoreSize(struct SpeedCache *speedcache,long &bitsPerSecond,long time_now,long total_dl_diff,long time_diff,long currentTotalDownloaded)
{
}

long ABRManager::FragmentfailureRampdown(int buffer,int currentprofileindex)
{
	return 0;
}
