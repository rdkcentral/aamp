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
#ifndef ROLLING_MEDIAN_OUTLIER_BANDWIDTH_ESTIMATOR_H
#define ROLLING_MEDIAN_OUTLIER_BANDWIDTH_ESTIMATOR_H

#include <utility>
#include <vector>

#include "BandwidthEstimatorBase.h"

/**
 * @brief Bandwidth estimator using the legacy ABR algorithm:
 * rolling-window samples + median + outlier rejection + average.
 */
class RollingMedianOutlierEstimator : public BandwidthEstimatorBase
{
private:
	BandwidthEstimatorConfig mConfig;
	std::vector<std::pair<long long, BitsPerSecond>> mAbrBitrateData;

	static long long GetCurrentTimeMs();
	void UpdateABRBitrateDataBasedOnCacheLength(BitsPerSecond downloadbps, bool lowLatencyMode);
	void UpdateABRBitrateDataBasedOnCacheLife(std::vector<BitsPerSecond> &tmpData);
	BitsPerSecond UpdateABRBitrateDataBasedOnCacheOutlier(std::vector<BitsPerSecond> &tmpData);

public:
	RollingMedianOutlierEstimator();
	explicit RollingMedianOutlierEstimator(const BandwidthEstimatorConfig &config);
	void SetConfig(const BandwidthEstimatorConfig &config) override;
	BandwidthEstimatorConfig GetConfig() const;
	const char *GetNetworkEstimatorName() const override;
	void Reset() override;
	void AddBandwidthSample(BitsPerSecond downloadbps, bool lowLatencyMode) override;
	void UpdateDownloadMetrics(const DownloadMetrics &metrics) override;
	BitsPerSecond GetBandwidthBitsPerSecond() override;
	double GetThroughputBytesPerSecond() override;
	double GetTimeToFirstByteSeconds() override;
	double GetPredictedDownloadTimeSeconds(std::size_t segment_size_bytes) override;
	void ResetCurrentlyAvailableBandwidth() override;
};

#endif
