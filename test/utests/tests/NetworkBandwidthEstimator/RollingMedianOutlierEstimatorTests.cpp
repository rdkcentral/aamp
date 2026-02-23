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

#include <array>
#include <gtest/gtest.h>

#include "RollingMedianOutlierEstimator.h"

/**
 * @brief Unit tests for RollingMedianOutlierEstimator.
 */
class RollingMedianOutlierEstimatorTests : public ::testing::Test
{
};

/**
 * @brief Helper to create BandwidthEstimatorConfig for tests.
 */
static BandwidthEstimatorConfig MakeTestConfig(
	const int abrCacheLifeMs,
	const int abrCacheLength,
	const int abrCacheOutlierThresholdBps,
	const std::size_t lowLatencyCacheLength)
{
	return BandwidthEstimatorConfig(
		abrCacheLifeMs,
		abrCacheLength,
		abrCacheOutlierThresholdBps,
		lowLatencyCacheLength);
}

/**
 * @brief Test that estimator returns "unavailable" values when no samples added.
 */
TEST_F(RollingMedianOutlierEstimatorTests, ReturnsUnavailableWhenEmpty)
{
	RollingMedianOutlierEstimator estimator;
	estimator.SetConfig(MakeTestConfig(
		1000000000,
		10,
		1000000000,
		10));

	EXPECT_EQ(estimator.GetBandwidthBitsPerSecond(), -1);
	EXPECT_EQ(estimator.GetThroughputBytesPerSecond(), 0.0);
	EXPECT_EQ(estimator.GetTimeToFirstByteSeconds(), 0.0);
	EXPECT_EQ(estimator.GetPredictedDownloadTimeSeconds(1024), 0.0);
}

/**
 * @brief Test that estimator computes simple average without outlier removal.
 */
TEST_F(RollingMedianOutlierEstimatorTests, AverageWithoutOutlierRemoval)
{
	RollingMedianOutlierEstimator estimator;
	estimator.SetConfig(MakeTestConfig(
		1000000000,
		10,
		1000000000,
		10));

	struct Step
	{
		BitsPerSecond sampleBps;
		BitsPerSecond expectedEstimateBps;
	};

	const std::array<Step, 4> steps{{
		{8000, 8000},
		{12000, 10000},
		{16000, 12000},
		{20000, 14000},
	}};

	for (const auto &step : steps)
	{
		estimator.AddBandwidthSample(step.sampleBps, false);
		EXPECT_EQ(estimator.GetBandwidthBitsPerSecond(),
			step.expectedEstimateBps);
		EXPECT_EQ(estimator.GetThroughputBytesPerSecond(),
			static_cast<double>(step.expectedEstimateBps) / 8.0);
	}
}

/**
 * @brief Test that estimator rejects outliers outside threshold.
 */
TEST_F(RollingMedianOutlierEstimatorTests, RejectsOutliersOutsideThreshold)
{
	RollingMedianOutlierEstimator estimator;
	estimator.SetConfig(MakeTestConfig(
		1000000000,
		10,
		200,
		10));

	estimator.AddBandwidthSample(100, false);
	estimator.AddBandwidthSample(100, false);
	estimator.AddBandwidthSample(100, false);
	estimator.AddBandwidthSample(1000, false);

	EXPECT_EQ(estimator.GetBandwidthBitsPerSecond(), 100);
}

/**
 * @brief Test that estimator respects cache length in non-low-latency mode.
 */
TEST_F(RollingMedianOutlierEstimatorTests, RespectsCacheLengthInNonLowLatency)
{
	RollingMedianOutlierEstimator estimator;
	estimator.SetConfig(MakeTestConfig(
		1000000000,
		3,
		1000000000,
		3));

	estimator.AddBandwidthSample(10, false);
	estimator.AddBandwidthSample(20, false);
	estimator.AddBandwidthSample(30, false);
	EXPECT_EQ(estimator.GetBandwidthBitsPerSecond(), 20);

	// Cache length is 3: adding a 4th sample drops the oldest (10).
	estimator.AddBandwidthSample(40, false);
	EXPECT_EQ(estimator.GetBandwidthBitsPerSecond(), 30);
}

/**
 * @brief Test that estimator respects cache length in low-latency mode.
 */
TEST_F(RollingMedianOutlierEstimatorTests, PredictsDownloadTimeFromEstimatedBandwidth)
{
	const double kEpsilon = 1e-9;
	const std::size_t kSegmentSizeBytes = 8000;

	RollingMedianOutlierEstimator estimator;
	estimator.SetConfig(MakeTestConfig(
		1000000000,
		10,
		1000000000,
		10));

	estimator.AddBandwidthSample(8000, false);
	EXPECT_NEAR(
		estimator.GetPredictedDownloadTimeSeconds(kSegmentSizeBytes),
		static_cast<double>(kSegmentSizeBytes) * 8.0 / 8000.0,
		kEpsilon);

	estimator.AddBandwidthSample(16000, false);
	// Estimate becomes average: (8000 + 16000) / 2 = 12000
	EXPECT_NEAR(
		estimator.GetPredictedDownloadTimeSeconds(kSegmentSizeBytes),
		static_cast<double>(kSegmentSizeBytes) * 8.0 / 12000.0,
		kEpsilon);
}
