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

/**
 * @brief Test that ResetCurrentlyAvailableBandwidth() suppresses the estimate for
 *        exactly one download cycle without discarding EWMA history.
 *
 * Steps:
 *  1. Seed the estimator with a completed-segment sample so it holds a positive estimate.
 *  2. Call ResetCurrentlyAvailableBandwidth() (simulating a forced profile switch).
 *  3. Verify GetBandwidthBitsPerSecond() now returns -1 (suppressed).
 *  4. Feed a second completed-segment sample via UpdateDownloadMetrics().
 *  5. Verify GetBandwidthBitsPerSecond() returns a positive value again (suppression cleared).
 */
TEST_F(HarmonicEwmaEstimatorTests, ResetCurrentlyAvailableBandwidth_SuppressesForOneCycle)
{
	HarmonicEwmaEstimator estimator;

	// Step 1: seed with a completed segment so the estimator has a positive estimate.
	DownloadMetrics firstSample;
	firstSample.m_size_download_bytes     = 112463;
	firstSample.m_total_time_seconds      = 0.398257;
	firstSample.m_time_to_first_byte_seconds = 0.249254;
	estimator.UpdateDownloadMetrics(firstSample);
	EXPECT_GT(estimator.GetBandwidthBitsPerSecond(), static_cast<BitsPerSecond>(0));

	// Step 2: simulate a forced profile switch.
	estimator.ResetCurrentlyAvailableBandwidth();

	// Step 3: estimate must be suppressed (returns -1) without touching EWMA history.
	EXPECT_EQ(estimator.GetBandwidthBitsPerSecond(), static_cast<BitsPerSecond>(-1));

	// Step 4: complete another segment download — this clears the one-shot flag.
	DownloadMetrics secondSample;
	secondSample.m_size_download_bytes     = 112463;
	secondSample.m_total_time_seconds      = 0.130057;
	secondSample.m_time_to_first_byte_seconds = 0.048034;
	estimator.UpdateDownloadMetrics(secondSample);

	// Step 5: suppression must be lifted; estimate must be positive again.
	EXPECT_GT(estimator.GetBandwidthBitsPerSecond(), static_cast<BitsPerSecond>(0));
}

/**
 * @brief Regression test for stale in-flight progress estimate after an aborted download.
 *
 * Scenario (mirrors the intermittent test_6002 EWMA failure):
 *  1. An in-flight progress update arrives with a very high instantaneous
 *     throughput (simulating the initial burst before a server stall).
 *  2. The download is aborted; UpdateDownloadMetrics() is called with
 *     zero downloaded bytes (the session was torn down before any segment
 *     data was committed — e.g. curl error 18 / lowBWTimeout).
 *  3. After the aborted-download metrics are recorded, neither
 *     GetThroughputBytesPerSecond() nor GetBandwidthBitsPerSecond() must
 *     reflect the prior high progress burst.
 *
 * Critical: the aborted sample carries zero bytes so payload_bytes_per_second
 * is 0.  All three EWMA accumulators (fast, slow, harmonic) therefore remain
 * at zero, and GetThroughputBytesPerSecond() falls through to the final branch:
 *
 *   return (m_progressHasSample) ? m_progressBytesPerSecond : 0.0;
 *
 * Without the fix in UpdateDownloadMetrics() that clears m_progressBytesPerSecond,
 * m_progressHasSample, and m_progressContextValid, this branch returns the
 * inflated burst value (100s of Mbit/s) because m_progressHasSample is still
 * true.  A non-zero aborted byte-count would mask the bug: std::min(ewma, burst)
 * caps the result to the (low) EWMA regardless of whether the fix is present,
 * causing the test to pass even on the unfixed code.
 */
TEST_F(HarmonicEwmaEstimatorTests, ProgressEstimateInvalidatedAfterAbortedDownload)
{
	HarmonicEwmaEstimator estimator;

	// Step 1: simulate the initial burst that arrives before a server-induced stall.
	// ~16 KB lands almost instantly (1 ms), giving a very high instantaneous rate.
	const double burstNow = 1000.0; // arbitrary monotonic base time
	const std::size_t burstBytes = 16384;
	const std::size_t totalBytes = 2000000; // full segment never arrives

	DownloadProgressInfo burst;
	burst.m_now_seconds = burstNow;
	burst.m_total_bytes = totalBytes;
	burst.m_now_bytes   = 0; // initialise context

	estimator.UpdateDownloadProgress(burst); // establishes context

	burst.m_now_seconds = burstNow + 0.001; // 1 ms later
	burst.m_now_bytes   = burstBytes;
	estimator.UpdateDownloadProgress(burst);

	// The progress estimate must now be very high (the burst inflated it).
	const double progressThroughput = estimator.GetThroughputBytesPerSecond();
	EXPECT_GT(progressThroughput, 1e6) // well above 1 MB/s
		<< "Pre-condition failed: progress burst should produce a high estimate";
	EXPECT_GT(estimator.GetBandwidthBitsPerSecond(), static_cast<BitsPerSecond>(0));

	// Step 2: download is aborted — zero bytes were committed to the metrics
	// (the session was torn down before curl reported any completed data).
	// Zero bytes keeps all EWMA accumulators at 0, forcing GetThroughputBytesPerSecond()
	// into the branch that is guarded solely by m_progressHasSample.
	DownloadMetrics abortedSample;
	abortedSample.m_size_download_bytes      = 0;     // no bytes delivered
	abortedSample.m_total_time_seconds       = 9.013; // total wall time including stall
	abortedSample.m_time_to_first_byte_seconds = 0.001;
	estimator.UpdateDownloadMetrics(abortedSample);

	// Step 3: all EWMA accumulators are still 0 (zero-byte sample contributes nothing).
	// The fix clears m_progressHasSample so GetThroughputBytesPerSecond() returns 0.
	// Without the fix m_progressHasSample remains true and the burst value is returned.
	const double postAbortThroughput = estimator.GetThroughputBytesPerSecond();
	EXPECT_LT(postAbortThroughput, progressThroughput)
		<< "Throughput after aborted download must be lower than the prior progress burst";
	EXPECT_EQ(postAbortThroughput, 0.0)
		<< "Throughput must be 0 — stale progress estimate must not survive UpdateDownloadMetrics";

	// Bandwidth must also be -1 (no usable estimate) rather than the burst value.
	const BitsPerSecond postAbortBandwidth = estimator.GetBandwidthBitsPerSecond();
	EXPECT_EQ(postAbortBandwidth, static_cast<BitsPerSecond>(-1))
		<< "Bandwidth must be -1 after aborted download with no prior EWMA history";
}
