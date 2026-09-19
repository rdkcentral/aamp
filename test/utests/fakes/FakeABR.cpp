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

std::atomic<ABRManager::PersistBandwidthData> ABRManager::mPersistBandwidthData{};

std::shared_ptr<MockABRManager> g_mockABRManager{};

ABRManager::ABRManager() : bLowLatencyStartABR(false) , bLowLatencyServiceConfigured(false) , mBandwidthEstimationAlgorithm(BANDWIDTH_ESTIMATION_ALGORITHM_ROLLING_MEDIAN_OUTLIER)
{
}

ABRManager::~ABRManager()
{
}

void ABRManager::SelectBandwidthEstimationAlgorithm(BandwidthEstimationAlgorithm type)
{
	std::lock_guard<std::mutex> lock(mBandwidthEstimatorLock);
	mBandwidthEstimationAlgorithm = type;
}

BandwidthEstimationAlgorithm ABRManager::GetBandwidthEstimationAlgorithm() const
{
	std::lock_guard<std::mutex> lock(mBandwidthEstimatorLock);
	return mBandwidthEstimationAlgorithm;
}

void ABRManager::AddBandwidthSample(BitsPerSecond downloadbps, bool lowLatencyMode)
{
	std::lock_guard<std::mutex> lock(mBandwidthEstimatorLock);
	if (downloadbps > 0)
	{
		mBandwidthState.availableBandwidth = downloadbps;
		mBandwidthState.networkBandwidth = downloadbps;
	}
}

void ABRManager::ReportDownloadComplete(BitsPerSecond downloadbps, bool lowLatencyMode,	const DownloadMetrics &metrics)
{
	std::lock_guard<std::mutex> lock(mBandwidthEstimatorLock);
	if (downloadbps > 0)
	{
		mBandwidthState.availableBandwidth = downloadbps;
		mBandwidthState.networkBandwidth = downloadbps;
	}
}

void ABRManager::ReportDownloadProgress(BitsPerSecond downloadbps, bool lowLatencyMode,	const DownloadProgressInfo &progressInfo)
{
	std::lock_guard<std::mutex> lock(mBandwidthEstimatorLock);
	if (downloadbps > 0)
	{
		mBandwidthState.availableBandwidth = downloadbps;
		mBandwidthState.networkBandwidth = downloadbps;
	}
}

void ABRManager::SetInitialBandwidthForProfile(BitsPerSecond bitsPerSecond, bool trickPlay,	int profile)
{
	std::lock_guard<std::mutex> lock(mBandwidthEstimatorLock);
	mBandwidthState.availableBandwidth = bitsPerSecond;
	mBandwidthState.networkBandwidth = bitsPerSecond;
}

void ABRManager::ResetCurrentlyAvailableBandwidth()
{
}

BitsPerSecond ABRManager::GetCurrentlyAvailableBandwidth()
{
	std::lock_guard<std::mutex> lock(mBandwidthEstimatorLock);
	return mBandwidthState.availableBandwidth;
}

BitsPerSecond ABRManager::GetNetworkBandwidth()
{
	std::lock_guard<std::mutex> lock(mBandwidthEstimatorLock);
	return mBandwidthState.networkBandwidth;
}

bool ABRManager::HasBandwidthEstimator() const
{
	return true;
}

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

int ABRManager::getProfileOfBandwidth(BitsPerSecond bandwidth)
{
	(void)bandwidth;
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

void ABRManager::setDefaultInitBitrate(BitsPerSecond defaultInitBitrate)
{
	(void)defaultInitBitrate;
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
	if (g_mockABRManager)
	{
		return g_mockABRManager->getRampedUpProfileIndex(currentProfileIndex, periodId);
	}
	return 0;
}

bool ABRManager::isProfileIndexBitrateLowest(int currentProfileIndex, const std::string& periodId)
{
	return true;
}

void ABRManager::setDefaultIframeBitrate(BitsPerSecond defaultIframeBitrate)
{
	(void)defaultIframeBitrate;
}

int ABRManager::removeProfiles(std::vector<BitsPerSecond> profileBPS, int currentProfileIndex, const std::string& periodId)
{
	return 0;
}

int  ABRManager::getProfileIndexForLowestBandwidth()
{
	return 0;
}

int ABRManager::getClosestProfileIndexByBandwidth(BitsPerSecond inputBandwidth)
{
	(void)inputBandwidth;
	return 0;
}

void ABRManager::ReadPlayerConfig(AampAbrConfig *mAampAbrConfig)
{
}

BitsPerSecond ABRManager::CheckAbrThresholdSize(int bufferlen, int downloadTimeMs, BitsPerSecond currentProfilebps, int fragmentDurationMs, CurlAbortReason abortReason)
{
	(void)bufferlen;
	(void)downloadTimeMs;
	(void)currentProfilebps;
	(void)fragmentDurationMs;
	(void)abortReason;
	return 0;
}

bool ABRManager::CheckProfileChange(double totalFetchedDuration, int currProfileIndex, BitsPerSecond availBW)
{
	(void)totalFetchedDuration;
	(void)currProfileIndex;
	(void)availBW;
	return false;
}

void ABRManager::GetDesiredProfileOnBuffer(int currProfileIndex,int &newProfileIndex,double bufferValue,double minBufferNeeded,const std::string& periodId)
{
}


void ABRManager::CheckRampupFromSteadyState(int currProfileIndex, int &newProfileIndex, BitsPerSecond nwBandwidth, double bufferValue, BitsPerSecond newBandwidth, BitrateChangeReason &mhBitrateReason, int &mMaxBufferCountCheck, const std::string& periodId)
{
	if (g_mockABRManager)
	{
		g_mockABRManager->CheckRampupFromSteadyState(currProfileIndex, newProfileIndex, nwBandwidth, bufferValue, newBandwidth, mhBitrateReason, mMaxBufferCountCheck, periodId);
		return;
	}
	(void)currProfileIndex;
	(void)newProfileIndex;
	(void)nwBandwidth;
	(void)bufferValue;
	(void)newBandwidth;
	(void)mhBitrateReason;
	(void)mMaxBufferCountCheck;
	(void)periodId;
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

void ABRManager::CheckLLDashABRSpeedStoreSize(struct SpeedCache *speedcache, BitsPerSecond &bitsPerSecond, long time_now, long total_dl_diff, long time_diff, long currentTotalDownloaded)
{
	(void)speedcache;
	(void)bitsPerSecond;
	(void)time_now;
	(void)total_dl_diff;
	(void)time_diff;
	(void)currentTotalDownloaded;
}

BitsPerSecond ABRManager::FragmentfailureRampdown(int currentBuffer, int currentProfileIndex)
{
	(void)currentBuffer;
	(void)currentProfileIndex;
	return 0;
}
