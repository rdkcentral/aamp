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

#include "aampgstplayer.h"
#include "MockGStreamer.h"
#include "MockAampConfig.h"

using ::testing::NiceMock;
using ::testing::Return;
using ::testing::StrEq;

AampLogManager *mLogObj{nullptr};
AampConfig *gpGlobalConfig{nullptr};

class FunctionalTests : public ::testing::Test
{
protected:
	void SetUp() override
	{
		g_mockGStreamer = new NiceMock<MockGStreamer>();
		g_mockAampConfig = new NiceMock<MockAampConfig>();
	}

	void TearDown() override
	{
		delete g_mockAampConfig;
		g_mockAampConfig = nullptr;

		delete g_mockGStreamer;
		g_mockGStreamer = nullptr;
	}

public:
};

TEST_F(FunctionalTests, Constructor)
{
	// Setup
	std::string debug_level{"test_level"};
	gboolean reset{TRUE};
	PrivateInstanceAAMP priv_aamp{};

	// Expectations
	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_GstDebugLevel))
				.WillOnce(Return(debug_level));
	EXPECT_CALL(*g_mockGStreamer,
				gst_debug_set_threshold_from_string(StrEq(debug_level.c_str()), reset));

	// Code under test
	AAMPGstPlayer player{nullptr, &priv_aamp, nullptr};
}
