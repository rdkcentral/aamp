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
		ABRManager::mPersistBandwidth = 0;
		ABRManager::mPersistBandwidthUpdatedTime = 0;

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
