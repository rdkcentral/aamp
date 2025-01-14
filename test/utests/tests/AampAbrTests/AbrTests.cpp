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

#include <cstdlib>
#include <iostream>
#include <string>
#include <string.h>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "priv_aamp.h"
#include "AampConfig.h"
#include "MockAampConfig.h"
#include "HybridABRManager.h"

using ::testing::NiceMock;
using ::testing::Return;
using ::testing::_;

AampConfig *gpGlobalConfig{nullptr};

extern HybridABRManager::AampAbrConfig eAAMPAbrConfig;

class AampAbrTests : public ::testing::Test
{
	public:
		PrivateInstanceAAMP *aamp{nullptr};
		AampConfig *config{nullptr};
	protected:
		void SetUp() override
		{
			config=new AampConfig();
			aamp = new PrivateInstanceAAMP(config);
			g_mockAampConfig = new NiceMock<MockAampConfig>();
		}

		void TearDown() override
		{
			delete g_mockAampConfig;
			g_mockAampConfig = nullptr;

			delete config;
			config = nullptr;

			delete aamp;
			aamp = nullptr;
		}

};
TEST_F(AampAbrTests,LoadAampAbrConfig)
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

	aamp->LoadAampAbrConfig();

	EXPECT_EQ(eAAMPAbrConfig.abrCacheLife,3);
	EXPECT_EQ(eAAMPAbrConfig.abrCacheLength,2);
	EXPECT_EQ(eAAMPAbrConfig.abrSkipDuration,6);
	EXPECT_EQ(eAAMPAbrConfig.abrNwConsistency,2);
	EXPECT_EQ(eAAMPAbrConfig.abrThresholdSize,3);
	EXPECT_EQ(eAAMPAbrConfig.abrMaxBuffer,15);
	EXPECT_EQ(eAAMPAbrConfig.abrMinBuffer,10);
	EXPECT_EQ(eAAMPAbrConfig.abrCacheOutlier,10000);
	EXPECT_EQ(eAAMPAbrConfig.abrBufferCounter,4);
}
