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
#include <vector>

#include "HarmonicEwmaEstimator.h"

/**
 * @brief Unit tests for HarmonicEwmaEstimator.
 */
class HarmonicEwmaEstimatorTests : public ::testing::Test
{
};

/**
 * @brief Test median calculation with various input sizes.
 */
TEST_F(HarmonicEwmaEstimatorTests, MedianTest)
{
	std::vector<double> values;
	EXPECT_EQ(GetMedian(values), 0.0);
	values.push_back(10.0);
	EXPECT_EQ(GetMedian(values), 10.0);
	values.push_back(30.0);
	EXPECT_EQ(GetMedian(values), 20.0);
	values.push_back(30.0);
	EXPECT_EQ(GetMedian(values), 30.0);
	values.push_back(20.0);
	EXPECT_EQ(GetMedian(values), 25.0);
}

/**
 * @brief Test bandwidth estimation with simple increasing samples.
 */
TEST_F(HarmonicEwmaEstimatorTests, ThroughputPredictionTest)
{
	const double kEpsilon = 1e-6;
	const size_t kSegmentSizeBytes = 112463;

	struct TestData
	{
		double timeToFirstByteSeconds;
		double throughputBytesPerSecond;
		double predictedDownloadTimeSeconds;

		size_t downloadBytes;
		double totalTimeSeconds;
		double timeToFirstByteSecondsCurl;
	};

	const std::array<TestData, 8> testData{{
		{0.000000, 0.000000, 0.000000, 112463, 0.398257, 0.249254},
		{0.249254, 754770.038187, 0.398257, 112463, 0.130057, 0.048034},
		{0.148644, 935373.273008, 0.268877, 112463, 0.120855, 0.056830},
		{0.056830, 1107592.676445, 0.158368, 112463, 0.119009, 0.057246},
		{0.057038, 1239315.381966, 0.147784, 112463, 0.116028, 0.048744},
		{0.056830, 1315556.262523, 0.142317, 112463, 0.117878, 0.052516},
		{0.054673, 1380828.841638, 0.136119, 112463, 0.116275, 0.051314},
		{0.052516, 1433386.044995, 0.130976, 112463, 0.116544, 0.051999},
	}};

	HarmonicEwmaEstimator estimator;
	for (const auto &data : testData)
	{
		EXPECT_NEAR(
			data.timeToFirstByteSeconds,
			estimator.GetTimeToFirstByteSeconds(),
			kEpsilon);
		EXPECT_NEAR(
			data.throughputBytesPerSecond,
			estimator.GetThroughputBytesPerSecond(),
			kEpsilon);
		EXPECT_NEAR(
			data.predictedDownloadTimeSeconds,
			estimator.GetPredictedDownloadTimeSeconds(kSegmentSizeBytes),
			kEpsilon);

		DownloadMetrics downloadMetrics;
		downloadMetrics.m_size_download_bytes = data.downloadBytes;
		downloadMetrics.m_total_time_seconds = data.totalTimeSeconds;
		downloadMetrics.m_time_to_first_byte_seconds =
			data.timeToFirstByteSecondsCurl;
		estimator.UpdateDownloadMetrics(downloadMetrics);
	}
}

/**
 * @brief Test mid-download monitoring via DownloadContext.
 */
TEST_F(HarmonicEwmaEstimatorTests, MidDownloadMonitoringTest)
{
	const double kEpsilon = 1e-6;

	struct TestData
	{
		double now;
		double pct;
		size_t dlnow;
		size_t dltotal;
		double bytesPerSecond;
		double estimatedRemaining;
	};

	const std::array<TestData, 8> testData{{
		{271850.024298, 0.000000},
		{271850.227916, 14, 15908, 112463, 78126.688199, 1.235877},
		{271850.259620, 28, 32274, 112463, 253360.998893, 0.316501},
		{271850.259914, 43, 48640, 112463, 22418685.647001, 0.002847},
		{271850.294995, 56, 63960, 112463, 13625892.839653, 0.003560},
		{271850.295399, 71, 80326, 112463, 24379496.450812, 0.001318},
		{271850.317906, 85, 96692, 112463, 14918558.491420, 0.001057},
		{271850.326387, 100, 112463, 112463, 9694962.476504, 0.000000},
	}};

	DownloadContext downloadContext;
	downloadContext.Reset(testData[0].now);
	for (const auto &data : testData)
	{
		downloadContext.xferinfo(data.now, data.dltotal, data.dlnow);
		EXPECT_NEAR(
			data.estimatedRemaining,
			downloadContext.GetEstimatedRemainingTime(),
			kEpsilon);
		EXPECT_NEAR(
			data.bytesPerSecond,
			downloadContext.GetEstimatedThroughputBytesPerSecond(),
			kEpsilon);
	}
}

/**
 * @brief Test bandwidth estimation via in-flight progress updates.
 */
TEST_F(HarmonicEwmaEstimatorTests, EstimatorProgressUpdateTest)
{
	const double kEpsilon = 1e-6;

	struct TestData
	{
		double now;
		size_t dlnow;
		size_t dltotal;
		double bytesPerSecond;
	};

	const std::array<TestData, 8> testData{{
		{271850.024298, 0, 0, 0.0},
		{271850.227916, 15908, 112463, 78126.688199},
		{271850.259620, 32274, 112463, 253360.998893},
		{271850.259914, 48640, 112463, 22418685.647001},
		{271850.294995, 63960, 112463, 13625892.839653},
		{271850.295399, 80326, 112463, 24379496.450812},
		{271850.317906, 96692, 112463, 14918558.491420},
		{271850.326387, 112463, 112463, 9694962.476504},
	}};

	HarmonicEwmaEstimator estimator;
	for (const auto &data : testData)
	{
		DownloadProgressInfo progressInfo;
		progressInfo.m_now_seconds = data.now;
		progressInfo.m_total_bytes = data.dltotal;
		progressInfo.m_now_bytes = data.dlnow;
		estimator.UpdateDownloadProgress(progressInfo);

		EXPECT_NEAR(
			data.bytesPerSecond,
			estimator.GetThroughputBytesPerSecond(),
			kEpsilon);
	}
}
