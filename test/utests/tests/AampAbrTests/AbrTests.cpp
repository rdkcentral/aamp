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
		.WillRepeatedly(Return(4));
	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_ABRBufferCounter))
		.WillRepeatedly(Return(5));
	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_ABRBandwidthEstimator))
		.WillRepeatedly(Return(BANDWIDTH_ESTIMATION_ALGORITHM_ROLLING_MEDIAN_OUTLIER));
}


