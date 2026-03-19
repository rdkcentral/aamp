/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2026 RDK Management
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
#include "RollingMedianOutlierEstimator.h"

#include <algorithm>
#include <sys/time.h>
#include <cstdlib>
/**
 * @brief Get current time in milliseconds.
 * @return Current time in milliseconds.
 */
long long RollingMedianOutlierEstimator::GetCurrentTimeMs()
{
	struct timeval t;
	gettimeofday(&t, NULL);
	return static_cast<long long>(t.tv_sec * 1e3 + t.tv_usec * 1e-3);
}

/**
 * @brief Constructor.
 */
RollingMedianOutlierEstimator::RollingMedianOutlierEstimator()
	: mConfig(0, 0, 0, DEFAULT_ABR_CHUNK_CACHE_LENGTH),
	  mAbrBitrateData()
{
}

/**
 * @brief Constructor with configuration.
 * @param[in] config Configuration parameters.
 */
RollingMedianOutlierEstimator::RollingMedianOutlierEstimator(const BandwidthEstimatorConfig &config)
	: mConfig(config),
	  mAbrBitrateData()
{
}

/**
 * @brief Set configuration parameters.
 * @param[in] config Configuration parameters.
 */
void RollingMedianOutlierEstimator::SetConfig(const BandwidthEstimatorConfig &config)
{
	mConfig = config;
}

/**
 * @brief Get current configuration parameters.
 * @return Configuration parameters.
 */
BandwidthEstimatorConfig RollingMedianOutlierEstimator::GetConfig() const
{
	return mConfig;
}

/**
 * @brief Update ABR bitrate data based on cache length.
 * @param[in] downloadbps Download bandwidth in bits per second.
 * @param[in] lowLatencyMode Whether low latency mode is enabled.
 */
void RollingMedianOutlierEstimator::UpdateABRBitrateDataBasedOnCacheLength(BitsPerSecond downloadbps, bool lowLatencyMode)
{
	mAbrBitrateData.push_back(std::make_pair(GetCurrentTimeMs(), downloadbps));

	if (lowLatencyMode)
	{
		if (mAbrBitrateData.size() > mConfig.mLowLatencyCacheLength)
		{
			mAbrBitrateData.erase(mAbrBitrateData.begin());
		}
	}
	else
	{
		if (mAbrBitrateData.size() > static_cast<size_t>(mConfig.mAbrCacheLength))
		{
			mAbrBitrateData.erase(mAbrBitrateData.begin());
		}
	}
}

/**
 * @brief Update ABR bitrate data based on cache life.
 * @param[out] tmpData Temporary vector to store valid bitrate samples.
 */
void RollingMedianOutlierEstimator::UpdateABRBitrateDataBasedOnCacheLife(std::vector<BitsPerSecond> &tmpData)
{
	long long presentTime = GetCurrentTimeMs();
	for (auto bitrateIter = mAbrBitrateData.begin();
		 bitrateIter != mAbrBitrateData.end();)
	{
		if ((bitrateIter->first <= 0) ||
			(presentTime - bitrateIter->first >
			 mConfig.mAbrCacheLife))
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
 * @brief Update ABR bitrate data based on cache outlier.
 * @param[in,out] tmpData Temporary vector containing valid bitrate samples.
 * @return Calculated average bitrate after removing outliers, or -1 if unavailable.
 */
BitsPerSecond RollingMedianOutlierEstimator::UpdateABRBitrateDataBasedOnCacheOutlier(std::vector<BitsPerSecond> &tmpData)
{
	BitsPerSecond ret = -1;
	BitsPerSecond medianbps = 0;

	size_t initialSize = tmpData.size();
	std::sort(tmpData.begin(), tmpData.end());
	if (initialSize % 2)
	{
		medianbps = tmpData.at(initialSize / 2);
	}
	else
	{
		BitsPerSecond m1 = tmpData.at(initialSize / 2 - 1);
		BitsPerSecond m2 = tmpData.at(initialSize / 2);
		medianbps = (m1 + m2) / 2;
	}

	long diffOutlier = 0;
	BitsPerSecond avg = 0;
	int abrOutlierDiffBytes = mConfig.mAbrCacheOutlier;
	for (auto tmpDataIter = tmpData.begin();
		 tmpDataIter != tmpData.end();)
	{
		if ( initialSize == 2)
		{
			// With 2 samples then only reject the higher outlier but not the lower one
			// for the lower sample then diffOutlier will be negative hence not rejected
			diffOutlier = (*tmpDataIter) - medianbps;
		}
		else
		{
			diffOutlier = std::abs((*tmpDataIter) - medianbps);
		}

		if (diffOutlier > abrOutlierDiffBytes)
		{
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
		ret = (avg / static_cast<BitsPerSecond>(tmpData.size()));
	}
	else
	{
		ret = -1;
	}

	return ret;
}

/**
 * @brief Get human-readable algorithm name.
 * @return Algorithm name.
 */
const char *RollingMedianOutlierEstimator::GetNetworkEstimatorName() const
{
	return "Rolling_Median_Outlier_Average";
}

/**
 * @brief Reset internal state.
 */
void RollingMedianOutlierEstimator::Reset()
{
	mAbrBitrateData.clear();
}

/**
 * @brief Add a bandwidth sample.
 * @param[in] downloadbps Download bandwidth in bits per second.
 * @param[in] lowLatencyMode Whether low latency mode is enabled.
 */
void RollingMedianOutlierEstimator::AddBandwidthSample(
	BitsPerSecond downloadbps, bool lowLatencyMode)
{
	UpdateABRBitrateDataBasedOnCacheLength(downloadbps, lowLatencyMode);
}

/**
 * @brief Add download metrics derived from curl (optional).
 * @param[in] metrics Download metrics.
 */
void RollingMedianOutlierEstimator::UpdateDownloadMetrics(
	const DownloadMetrics &metrics)
{
	(void)metrics;
}

/**
 * @brief Get current bandwidth estimate.
 * @return Estimated bandwidth in bits per second, or -1 if unavailable.
 */
BitsPerSecond RollingMedianOutlierEstimator::GetBandwidthBitsPerSecond()
{
	std::vector<BitsPerSecond> tmpData;
	UpdateABRBitrateDataBasedOnCacheLife(tmpData);
	if (tmpData.empty())
	{
		return -1;
	}
	return UpdateABRBitrateDataBasedOnCacheOutlier(tmpData);
}

/**
 * @brief Get current robust throughput estimate.
 * @return Estimated throughput in bytes per second.
 */
double RollingMedianOutlierEstimator::GetThroughputBytesPerSecond()
{
	const BitsPerSecond bandwidthBitsPerSecond = GetBandwidthBitsPerSecond();
	if (bandwidthBitsPerSecond <= 0)
	{
		return 0.0;
	}
	return static_cast<double>(bandwidthBitsPerSecond) / 8.0;
}

/**
 * @brief Get current overhead (TTFB) estimate.
 * @return Estimated time to first byte in seconds.
 */
double RollingMedianOutlierEstimator::GetTimeToFirstByteSeconds()
{
	return 0.0;
}

/**
 * @brief Predict completion time for a new segment.
 * @param[in] segment_size_bytes Size of the segment in bytes.
 * @return Predicted download time in seconds.
 */
double RollingMedianOutlierEstimator::GetPredictedDownloadTimeSeconds(std::size_t segment_size_bytes)
{
	const BitsPerSecond bandwidthBitsPerSecond = GetBandwidthBitsPerSecond();
	if (bandwidthBitsPerSecond <= 0)
	{
		return 0.0;
	}
	return (static_cast<double>(segment_size_bytes) * 8.0) /
		   static_cast<double>(bandwidthBitsPerSecond);
}

/**
 * @brief Reset only the currently available bandwidth estimate.
 */
void RollingMedianOutlierEstimator::ResetCurrentlyAvailableBandwidth()
{
	mAbrBitrateData.clear();
}