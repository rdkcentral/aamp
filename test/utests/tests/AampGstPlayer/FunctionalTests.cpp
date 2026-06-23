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
#include "middleware/InterfacePlayerRDK.h"
#include "aampgstplayer.h"
#include "MockGStreamer.h"
#include "MockGLib.h"
#include "MockAampConfig.h"
#include "MockPrivateInstanceAAMP.h"
#include "MockAampUtils.h"

using ::testing::NiceMock;
using ::testing::Return;
using ::testing::StrEq;
using ::testing::Eq;
using ::testing::_;
using ::testing::NotNull;
using ::testing::Matcher;

AampConfig *gpGlobalConfig{nullptr};

class AAMPGstPlayerTests : public ::testing::Test
{

protected:
	AAMPGstPlayer *mAAMPGstPlayer;
	PrivateInstanceAAMP *mPrivateInstanceAAMP;

	void SetUp() override
	{
		g_mockAampUtils = std::make_shared<NiceMock<MockAampUtils>>();
		g_mockGStreamer = new NiceMock<MockGStreamer>();
		g_mockGLib = std::make_shared<MockGLib>();
		g_mockAampConfig = std::make_shared<NiceMock<MockAampConfig>>();
		g_mockPrivateInstanceAAMP = std::make_shared<MockPrivateInstanceAAMP>();
		mPrivateInstanceAAMP = new PrivateInstanceAAMP{};
	}

	void TearDown() override
	{
		g_mockPrivateInstanceAAMP.reset();

		g_mockAampConfig.reset();

		g_mockGLib.reset();

		delete g_mockGStreamer;
		g_mockGStreamer = nullptr;

		g_mockAampUtils.reset();

		delete mPrivateInstanceAAMP;
		mPrivateInstanceAAMP = nullptr;
	}

public:
	void ConstructAMPGstPlayer()
	{
		std::string debug_level{"test_level"};

		EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_GstDebugLevel))
					.WillOnce(Return(debug_level));

		mAAMPGstPlayer = new AAMPGstPlayer{mPrivateInstanceAAMP, nullptr};
	}

	void DestroyAMPGstPlayer()
	{
		delete mAAMPGstPlayer;
		mAAMPGstPlayer = nullptr;
	}
};

TEST_F(AAMPGstPlayerTests, Constructor)
{
	ConstructAMPGstPlayer();
	DestroyAMPGstPlayer();
}

TEST_F(AAMPGstPlayerTests, SetAudioVolume_NoSink)
{
	// Setup
	ConstructAMPGstPlayer();

	// Code under test

	// No sink, so no call to set volume or mute expected
	int volume = 0;
	EXPECT_CALL(*g_mockGLib, g_object_set(NotNull(), StrEq("mute"), Matcher<int>(_))).Times(0);
	EXPECT_CALL(*g_mockGLib, g_object_set(NotNull(), StrEq("volume"), Matcher<double>(_))).Times(0);
	mAAMPGstPlayer->SetAudioVolume(volume);

	volume = 100;
	mAAMPGstPlayer->SetAudioVolume(volume);

	volume = 50;
	mAAMPGstPlayer->SetAudioVolume(volume);

	//Tidy Up
	DestroyAMPGstPlayer();
}

TEST_F(AAMPGstPlayerTests, SetVideoMute_NoSink)
{
	// Setup
	ConstructAMPGstPlayer();

	bool mute = true;
	EXPECT_CALL(*g_mockGLib, g_object_set(NotNull(), StrEq("show-video-window"), Matcher<int>(_))).Times(0);
	mAAMPGstPlayer->SetVideoMute(mute);

	//Tidy Up
	DestroyAMPGstPlayer();
}
