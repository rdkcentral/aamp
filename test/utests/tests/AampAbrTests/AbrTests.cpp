/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2024 RDK Management
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


#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <cstddef>
#include <thread>
#include <atomic>
#include <vector>
#include "priv_aamp.h"
#include "AampConfig.h"
#include "MockAampConfig.h"
#include "abr.h"

using ::testing::NiceMock;
using ::testing::Return;
using ::testing::_;

AampConfig *gpGlobalConfig{nullptr};

extern ABRManager::AampAbrConfig eAAMPAbrConfig;

class AbrTests : public ::testing::Test
{
protected:
	void SetUp() override
	{
		ABRManager::setPersistBandwidth(0, 0);

		eAAMPAbrConfig = ABRManager::AampAbrConfig();
		// Cache life is interpreted as milliseconds by the estimator.
		// Use a large value to avoid timing-related flakes in unit tests.
		eAAMPAbrConfig.abrCacheLife = 600000;
		eAAMPAbrConfig.abrCacheLength = 3;
		eAAMPAbrConfig.abrCacheOutlier = 1000000;
		eAAMPAbrConfig.bandwidthEstimatorType =	BANDWIDTH_ESTIMATION_ALGORITHM_ROLLING_MEDIAN_OUTLIER;
	}
};

class AampAbrConfigTests : public ::testing::Test
{
public:
	PrivateInstanceAAMP *aamp{nullptr};
	AampConfig *config{nullptr};

protected:
	void SetUp() override
	{
		config = new AampConfig();
		aamp = new PrivateInstanceAAMP(config);
		g_mockAampConfig = new NiceMock<MockAampConfig>();
	}

	void TearDown() override
	{
		delete g_mockAampConfig;
		g_mockAampConfig = nullptr;

		delete aamp;
		aamp = nullptr;

		delete config;
		config = nullptr;
	}
};

/**
 * @brief Test loading ABR configuration from AampConfig.
 */
TEST_F(AampAbrConfigTests, LoadAampAbrConfig)
{
	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_ABRCacheLife))
		.WillRepeatedly(Return(3));
	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_ABRCacheLength))
		.WillRepeatedly(Return(2));
	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_ABRSkipDuration))
		.WillRepeatedly(Return(6));
	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_ABRNWConsistency))
		.WillRepeatedly(Return(2));
	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_ABRThresholdSize))
		.WillRepeatedly(Return(3));
	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_MaxABRNWBufferRampUp))
		.WillRepeatedly(Return(15));
	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_MinABRNWBufferRampDown))
		.WillRepeatedly(Return(10));
	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_ABRCacheOutlier))
		.WillRepeatedly(Return(10000));
	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_ABRBufferCounter))
		.WillRepeatedly(Return(4));
	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_ABRBandwidthEstimator))
		.WillRepeatedly(Return(BANDWIDTH_ESTIMATION_ALGORITHM_HARMONIC_EWMA));

	aamp->LoadAampAbrConfig();

	EXPECT_EQ(eAAMPAbrConfig.abrCacheLife, 3);
	EXPECT_EQ(eAAMPAbrConfig.abrCacheLength, 2);
	EXPECT_EQ(eAAMPAbrConfig.abrSkipDuration, 6);
	EXPECT_EQ(eAAMPAbrConfig.abrNwConsistency, 2);
	EXPECT_EQ(eAAMPAbrConfig.abrThresholdSize, 3);
	EXPECT_EQ(eAAMPAbrConfig.abrMaxBuffer, 15);
	EXPECT_EQ(eAAMPAbrConfig.abrMinBuffer, 10);
	EXPECT_EQ(eAAMPAbrConfig.abrCacheOutlier, 10000);
	EXPECT_EQ(eAAMPAbrConfig.abrBufferCounter, 4);
	EXPECT_EQ(eAAMPAbrConfig.bandwidthEstimatorType, BANDWIDTH_ESTIMATION_ALGORITHM_HARMONIC_EWMA);
}

/**
 * @brief Test default ABRManager state and behavior.
 */
TEST_F(AbrTests, DefaultEstimatorIsRMOAndUnavailableInitially)
{
	ABRManager abrManager;

	EXPECT_TRUE(abrManager.HasBandwidthEstimator());
	EXPECT_EQ(abrManager.GetBandwidthEstimationAlgorithm(), BANDWIDTH_ESTIMATION_ALGORITHM_ROLLING_MEDIAN_OUTLIER);
	EXPECT_EQ(abrManager.GetCurrentlyAvailableBandwidth(), -1);
	EXPECT_EQ(abrManager.GetNetworkBandwidth(), 0);
}

/**
 * @brief Test RMO estimator with various bandwidth samples.
 */
TEST_F(AbrTests, RMOAveragesSamplesWithConfiguredCacheLength)
{
	eAAMPAbrConfig.abrCacheLength = 3;
	eAAMPAbrConfig.abrCacheOutlier = 1000000;

	ABRManager abrManager;
	abrManager.ReadPlayerConfig(&eAAMPAbrConfig);
	abrManager.AddBandwidthSample(1000, false);
	abrManager.AddBandwidthSample(2000, false);
	abrManager.AddBandwidthSample(3000, false);

	EXPECT_EQ(abrManager.GetCurrentlyAvailableBandwidth(), 2000);
}

/**
 * @brief Test RMO estimator trims samples to configured cache length.
 */
TEST_F(AbrTests, RMOTrimsSamplesToConfiguredCacheLength)
{
	eAAMPAbrConfig.abrCacheLength = 2;
	eAAMPAbrConfig.abrCacheOutlier = 1000000;

	ABRManager abrManager;
	abrManager.ReadPlayerConfig(&eAAMPAbrConfig);
	abrManager.AddBandwidthSample(1000, false);
	abrManager.AddBandwidthSample(2000, false);
	abrManager.AddBandwidthSample(3000, false);

	EXPECT_EQ(abrManager.GetCurrentlyAvailableBandwidth(), 2500);
}

/**
 * @brief Test RMO estimator rejects outlier samples beyond configured threshold.
 */
TEST_F(AbrTests, RMORejectsOutlierSamplesBeyondConfiguredThreshold)
{
	eAAMPAbrConfig.abrCacheLength = 10;
	eAAMPAbrConfig.abrCacheOutlier = 5000;

	ABRManager abrManager;
	abrManager.ReadPlayerConfig(&eAAMPAbrConfig);
	abrManager.AddBandwidthSample(1000, false);
	abrManager.AddBandwidthSample(1100, false);
	abrManager.AddBandwidthSample(100000, false); // Gets rejected

	EXPECT_EQ(abrManager.GetCurrentlyAvailableBandwidth(), 1050);
}

/**
 * @brief Test RMO estimator rejects high outlier but not low from 2 samples
 */
TEST_F(AbrTests, RMORejectsHigherOutlierWhen2SamplesPresent)
{
	eAAMPAbrConfig.abrCacheLength = 10;
	eAAMPAbrConfig.abrCacheOutlier = 5000;

	// Previously both samples would get rejected and bandwidth would be unavailable (-1).
	// Now with the fix to reject only samples greater than the outlier threshold, the bandwidth
	// should be available and calculated from the remaining sample.
	ABRManager abrManager;
	abrManager.ReadPlayerConfig(&eAAMPAbrConfig);
	abrManager.AddBandwidthSample(1000, false);
	abrManager.AddBandwidthSample(100000, false); // Gets rejected as outlier

	EXPECT_EQ(abrManager.GetCurrentlyAvailableBandwidth(), 1000);
}

/**
 * @brief Test Harmonic EWMA estimator computes bandwidth from download metrics.
 */
TEST_F(AbrTests, HarmonicEWMAComputesBandwidthFromDownloadMetrics)
{
	ABRManager abrManager;
	abrManager.SelectBandwidthEstimationAlgorithm(
		BANDWIDTH_ESTIMATION_ALGORITHM_HARMONIC_EWMA);

	DownloadMetrics metrics;
	metrics.m_size_download_bytes = 8000;
	metrics.m_total_time_seconds = 2.0;
	metrics.m_time_to_first_byte_seconds = 0.5;

	const double payloadSeconds = metrics.m_total_time_seconds - metrics.m_time_to_first_byte_seconds;
	const double bytesPerSecond = static_cast<double>(metrics.m_size_download_bytes) / payloadSeconds;
	const BitsPerSecond expectedBitsPerSecond =	static_cast<BitsPerSecond>(bytesPerSecond * 8.0);

	abrManager.ReportDownloadComplete(0, false, metrics);
	EXPECT_EQ(abrManager.GetCurrentlyAvailableBandwidth(), expectedBitsPerSecond);
}

/**
 * @brief Test switching bandwidth estimators retains appropriate state.
 */
TEST_F(AbrTests, SwitchingEstimatorsUsesNewEstimatorState)
{
	eAAMPAbrConfig.abrCacheLength = 3;
	eAAMPAbrConfig.abrCacheOutlier = 1000000;

	ABRManager abrManager;
	abrManager.ReadPlayerConfig(&eAAMPAbrConfig);
	abrManager.AddBandwidthSample(1000, false);
	EXPECT_EQ(abrManager.GetCurrentlyAvailableBandwidth(), 1000);

	abrManager.SelectBandwidthEstimationAlgorithm(BANDWIDTH_ESTIMATION_ALGORITHM_HARMONIC_EWMA);
	EXPECT_EQ(abrManager.GetBandwidthEstimationAlgorithm(), BANDWIDTH_ESTIMATION_ALGORITHM_HARMONIC_EWMA);
	// New estimator starts with no samples, so bandwidth is unavailable.
	EXPECT_EQ(abrManager.GetCurrentlyAvailableBandwidth(), -1);

	DownloadMetrics metrics;
	metrics.m_size_download_bytes = 4000;
	metrics.m_total_time_seconds = 1.0;
	metrics.m_time_to_first_byte_seconds = 0.25;

	const double payloadSeconds = metrics.m_total_time_seconds - metrics.m_time_to_first_byte_seconds;
	const double bytesPerSecond = static_cast<double>(metrics.m_size_download_bytes) / payloadSeconds;
	const BitsPerSecond expectedBitsPerSecond =	static_cast<BitsPerSecond>(bytesPerSecond * 8.0);

	abrManager.ReportDownloadComplete(0, false, metrics);
	EXPECT_EQ(abrManager.GetCurrentlyAvailableBandwidth(), expectedBitsPerSecond);
}

/**
 * @brief Helper to add video profiles to an ABRManager for rampup tests.
 */
static void AddTestProfiles(ABRManager &mgr)
{
	ABRManager::ProfileInfo p{};
	p.isIframeTrack = false;

	p.bandwidthBitsPerSecond = 1000000; // index 0
	mgr.addProfile(p);

	p.bandwidthBitsPerSecond = 2000000; // index 1
	mgr.addProfile(p);

	p.bandwidthBitsPerSecond = 4000000; // index 2
	mgr.addProfile(p);
}

/**
 * @brief CheckRampupFromSteadyState must not modify newProfileIndex
 *        when nwBandwidth is zero (divide-by-zero guard).
 */
TEST_F(AbrTests, CheckRampupFromSteadyState_ZeroBandwidth_NoChange)
{
	ABRManager abrManager;
	abrManager.ReadPlayerConfig(&eAAMPAbrConfig);
	AddTestProfiles(abrManager);

	int currProfileIndex = 0;
	int newProfileIndex = currProfileIndex;
	BitsPerSecond nwBandwidth = 0;
	double bufferValue = 20.0;
	BitsPerSecond newBandwidth = 2000000;
	ABRManager::BitrateChangeReason reason = ABRManager::eAAMP_BITRATE_CHANGE_BY_ABR;
	int maxBufferCountCheck = 1;

	abrManager.CheckRampupFromSteadyState(
		currProfileIndex, newProfileIndex, nwBandwidth, bufferValue,
		newBandwidth, reason, maxBufferCountCheck);

	EXPECT_EQ(newProfileIndex, currProfileIndex);
	EXPECT_EQ(reason, ABRManager::eAAMP_BITRATE_CHANGE_BY_ABR);
	EXPECT_EQ(maxBufferCountCheck, 1);
}

/**
 * @brief CheckRampupFromSteadyState must not modify newProfileIndex
 *        when nwBandwidth is negative.
 */
TEST_F(AbrTests, CheckRampupFromSteadyState_NegativeBandwidth_NoChange)
{
	ABRManager abrManager;
	abrManager.ReadPlayerConfig(&eAAMPAbrConfig);
	AddTestProfiles(abrManager);

	int currProfileIndex = 1;
	int newProfileIndex = currProfileIndex;
	BitsPerSecond nwBandwidth = -1;
	double bufferValue = 15.0;
	BitsPerSecond newBandwidth = 4000000;
	ABRManager::BitrateChangeReason reason = ABRManager::eAAMP_BITRATE_CHANGE_BY_ABR;
	int maxBufferCountCheck = 1;

	abrManager.CheckRampupFromSteadyState(
		currProfileIndex, newProfileIndex, nwBandwidth, bufferValue,
		newBandwidth, reason, maxBufferCountCheck);

	EXPECT_EQ(newProfileIndex, currProfileIndex);
	EXPECT_EQ(reason, ABRManager::eAAMP_BITRATE_CHANGE_BY_ABR);
}

/**
 * @brief CheckRampupFromSteadyState allows rampup when nwBandwidth is valid
 *        and threshold is within range.
 */
TEST_F(AbrTests, CheckRampupFromSteadyState_ValidBandwidth_RampsUp)
{
	eAAMPAbrConfig.abrBufferCounter = 2;

	ABRManager abrManager;
	abrManager.ReadPlayerConfig(&eAAMPAbrConfig);
	AddTestProfiles(abrManager);

	int currProfileIndex = 0;
	int newProfileIndex = currProfileIndex;
	BitsPerSecond nwBandwidth = 1800000;
	double bufferValue = 20.0;
	// newBandwidth 2000000 is ~11% above nwBandwidth, within the 0-30% threshold
	BitsPerSecond newBandwidth = 2000000;
	ABRManager::BitrateChangeReason reason = ABRManager::eAAMP_BITRATE_CHANGE_BY_ABR;
	int maxBufferCountCheck = 1;

	abrManager.CheckRampupFromSteadyState(
		currProfileIndex, newProfileIndex, nwBandwidth, bufferValue,
		newBandwidth, reason, maxBufferCountCheck);

	// Should ramp up to the next profile (index 1)
	EXPECT_EQ(newProfileIndex, 1);
	EXPECT_EQ(reason, ABRManager::eAAMP_BITRATE_CHANGE_BY_BUFFER_FULL);
}

/**
 * @brief updateProfile correctly selects the desired iframe profile
 *        from a set of mixed video + iframe profiles.
 */
TEST_F(AbrTests, UpdateProfile_SelectsCorrectIframeProfile)
{
	ABRManager abrManager;
	abrManager.ReadPlayerConfig(&eAAMPAbrConfig);

	// Add video profiles (not iframe)
	ABRManager::ProfileInfo video{};
	video.isIframeTrack = false;
	video.bandwidthBitsPerSecond = 3000000;
	video.width = 1920;
	video.height = 1080;
	abrManager.addProfile(video); // index 0

	video.bandwidthBitsPerSecond = 6000000;
	abrManager.addProfile(video); // index 1

	// Add iframe profiles
	ABRManager::ProfileInfo iframe{};
	iframe.isIframeTrack = true;
	iframe.width = 640;
	iframe.height = 360;

	iframe.bandwidthBitsPerSecond = 500000;
	abrManager.addProfile(iframe); // index 2

	iframe.bandwidthBitsPerSecond = 1500000;
	abrManager.addProfile(iframe); // index 3

	abrManager.updateProfile();

	// Non-4K, no default iframe bitrate: should pick lowest as lowest,
	// second as desired (legacy "first two" logic)
	EXPECT_EQ(abrManager.getLowestIframeProfile(), 2);
	EXPECT_EQ(abrManager.getDesiredIframeProfile(), 3);
}

/**
 * @brief updateProfile with default iframe bitrate selects the profile
 *        below the configured default.
 */
TEST_F(AbrTests, UpdateProfile_DefaultIframeBitrate_SelectsBelowDefault)
{
	ABRManager abrManager;
	abrManager.ReadPlayerConfig(&eAAMPAbrConfig);
	abrManager.setDefaultIframeBitrate(1200000);

	ABRManager::ProfileInfo video{};
	video.isIframeTrack = false;
	video.bandwidthBitsPerSecond = 3000000;
	video.width = 1920;
	video.height = 1080;
	abrManager.addProfile(video); // index 0

	ABRManager::ProfileInfo iframe{};
	iframe.isIframeTrack = true;
	iframe.width = 640;
	iframe.height = 360;

	iframe.bandwidthBitsPerSecond = 500000;
	abrManager.addProfile(iframe); // index 1

	iframe.bandwidthBitsPerSecond = 1000000;
	abrManager.addProfile(iframe); // index 2

	iframe.bandwidthBitsPerSecond = 2000000;
	abrManager.addProfile(iframe); // index 3

	abrManager.updateProfile();

	// Default iframe bitrate = 1200000: should pick highest below that = index 2 (1000000)
	EXPECT_EQ(abrManager.getLowestIframeProfile(), 1);
	EXPECT_EQ(abrManager.getDesiredIframeProfile(), 2);
}

/**
 * @brief Bug #11: FragmentfailureRampdown must skip iframe tracks when
 *        selecting a rampdown target, matching every other ABR function.
 */
TEST_F(AbrTests, FragmentfailureRampdown_SkipsIframeTrack)
{
	eAAMPAbrConfig.abrMaxBuffer = 30;

	ABRManager abrManager;
	abrManager.ReadPlayerConfig(&eAAMPAbrConfig);

	ABRManager::ProfileInfo p{};
	// Profile 0: 500 kbps video
	p.isIframeTrack = false;
	p.bandwidthBitsPerSecond = 500000;
	p.width = 320; p.height = 240;
	abrManager.addProfile(p);

	// Profile 1: 800 kbps iframe — should be skipped
	p.isIframeTrack = true;
	p.bandwidthBitsPerSecond = 800000;
	p.width = 640; p.height = 360;
	abrManager.addProfile(p);

	// Profile 2: 1 Mbps video
	p.isIframeTrack = false;
	p.bandwidthBitsPerSecond = 1000000;
	p.width = 640; p.height = 360;
	abrManager.addProfile(p);

	// Profile 3: 2 Mbps video (current)
	p.isIframeTrack = false;
	p.bandwidthBitsPerSecond = 2000000;
	p.width = 1280; p.height = 720;
	abrManager.addProfile(p);

	// Buffer at 15/30 = 50%.  Sorted video BWs: 500k, 1M, 2M.
	// Profile percentages (relative to max 2M): 25%, 50%, 100%.
	// Iterating descending: 100% !< 50% skip, 50% !< 50% skip, 25% < 50% → 500k.
	BitsPerSecond result = abrManager.FragmentfailureRampdown(15, 3);
	EXPECT_EQ(result, 500000);
}

TEST_F(AbrTests, FragmentfailureRampdown_NoIframes_NormalBehavior)
{
	eAAMPAbrConfig.abrMaxBuffer = 30;

	ABRManager abrManager;
	abrManager.ReadPlayerConfig(&eAAMPAbrConfig);

	ABRManager::ProfileInfo p{};
	p.isIframeTrack = false;

	p.bandwidthBitsPerSecond = 500000;
	p.width = 320; p.height = 240;
	abrManager.addProfile(p);

	p.bandwidthBitsPerSecond = 1000000;
	p.width = 640; p.height = 360;
	abrManager.addProfile(p);

	p.bandwidthBitsPerSecond = 2000000;
	p.width = 1280; p.height = 720;
	abrManager.addProfile(p);

	// Buffer 21/30 = 70%.  Profile percentages: 25%, 50%, 100%.
	// Descending: 100% !< 70%, 50% < 70% AND 1M < 2M → 1M.
	BitsPerSecond result = abrManager.FragmentfailureRampdown(21, 2);
	EXPECT_EQ(result, 1000000);
}

TEST_F(AbrTests, FragmentfailureRampdown_FallbackLowestIsNotIframe)
{
	eAAMPAbrConfig.abrMaxBuffer = 30;

	ABRManager abrManager;
	abrManager.ReadPlayerConfig(&eAAMPAbrConfig);

	ABRManager::ProfileInfo p{};

	// Profile 0: 200 kbps iframe — lowest BW overall but should be excluded
	p.isIframeTrack = true;
	p.bandwidthBitsPerSecond = 200000;
	p.width = 160; p.height = 120;
	abrManager.addProfile(p);

	// Profile 1: 500 kbps video — lowest video profile
	p.isIframeTrack = false;
	p.bandwidthBitsPerSecond = 500000;
	p.width = 320; p.height = 240;
	abrManager.addProfile(p);

	// Profile 2: 2 Mbps video (current)
	p.isIframeTrack = false;
	p.bandwidthBitsPerSecond = 2000000;
	p.width = 1280; p.height = 720;
	abrManager.addProfile(p);

	// Buffer 1/30 ≈ 3.3%.  Sorted video BWs: 500k, 2M.
	// Profile percentages: 25%, 100%.  Both ≥ 3.3%, but only 500k < currentbw.
	// 25% < 3.3% is false, so no profile matches → fallback to lowest.
	// Without the fix, fallback would return 200k (the iframe track).
	BitsPerSecond result = abrManager.FragmentfailureRampdown(1, 2);
	EXPECT_EQ(result, 500000);
}

/**
 * @brief setPersistBandwidth round-trips bandwidth and timestamp correctly.
 */
TEST_F(AbrTests, PersistBandwidth_SetGetRoundTrip)
{
	ABRManager::setPersistBandwidth(5000000, 12345);
	auto data = ABRManager::getPersistBandwidth();
	EXPECT_EQ(data.bandwidth, 5000000);
	EXPECT_EQ(data.updatedTimeMs, 12345);

	ABRManager::setPersistBandwidth(0, 0);
	data = ABRManager::getPersistBandwidth();
	EXPECT_EQ(data.bandwidth, 0);
	EXPECT_EQ(data.updatedTimeMs, 0);
}

/**
 * @brief Large 64-bit timestamp values round-trip correctly, covering
 *        values that would produce torn reads on ARM without atomics.
 */
TEST_F(AbrTests, PersistBandwidthUpdatedTime_LargeValueRoundTrip)
{
	constexpr int64_t largeTime = INT64_C(0x1234567890ABCDEF);
	ABRManager::setPersistBandwidth(42, largeTime);
	auto data = ABRManager::getPersistBandwidth();
	EXPECT_EQ(data.bandwidth, 42);
	EXPECT_EQ(data.updatedTimeMs, largeTime);
}

/**
 * @brief Concurrent writers and readers must always observe a consistent
 *        (bandwidth, timestamp) pair — never a mix of two different writes.
 *
 * Writers alternate between two distinct pairs. Readers verify every
 * observation matches one of those pairs. A torn or unpaired read would
 * produce a combination from two different writes, failing the test.
 * This is the primary regression test for abr-bugs.md #4.
 */
TEST_F(AbrTests, PersistBandwidthData_ConcurrentAccessConsistentPair)
{
	constexpr BitsPerSecond kBwA = 1000000;
	constexpr int64_t kTimeA = INT64_C(0x00000000FFFFFFFF);
	constexpr BitsPerSecond kBwB = 9000000;
	constexpr int64_t kTimeB = INT64_C(0xFFFFFFFF00000000);
	constexpr int kIterations = 100000;

	std::atomic<bool> stop{false};
	std::atomic<bool> failure{false};

	// Writer thread: alternates between two known pairs
	std::thread writer([&]() {
		for (int i = 0; i < kIterations; ++i)
		{
			if (i % 2 == 0)
				ABRManager::setPersistBandwidth(kBwA, kTimeA);
			else
				ABRManager::setPersistBandwidth(kBwB, kTimeB);
		}
		stop.store(true, std::memory_order_release);
	});

	// Reader thread: verifies consistent pairing
	std::thread reader([&]() {
		while (!stop.load(std::memory_order_acquire))
		{
			auto data = ABRManager::getPersistBandwidth();
			bool isZero = (data.bandwidth == 0 && data.updatedTimeMs == 0);
			bool isPairA = (data.bandwidth == kBwA && data.updatedTimeMs == kTimeA);
			bool isPairB = (data.bandwidth == kBwB && data.updatedTimeMs == kTimeB);
			if (!isZero && !isPairA && !isPairB)
			{
				failure.store(true, std::memory_order_release);
				break;
			}
		}
	});

	writer.join();
	reader.join();

	EXPECT_FALSE(failure.load());
}

