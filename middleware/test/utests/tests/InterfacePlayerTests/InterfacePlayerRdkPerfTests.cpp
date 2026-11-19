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

#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <vector>
#include "InterfacePlayerPriv.h"

class InterfacePlayerRdkPerfTests : public ::testing::Test
{
protected:
	void SetUp() override
	{
		while (PIIsTuneProfileActive())
		{
			PIStopTuneProfile();
		}
		PISetProfileAlwaysOn(false);
		RDKPerfStubEvents().clear();
		mPlayerA = std::make_unique<GstPlayerPriv>();
		mPlayerB = std::make_unique<GstPlayerPriv>();
	}
	void TearDown() override
	{
		mPlayerA.reset();
		mPlayerB.reset();
		while (PIIsTuneProfileActive())
		{
			PIStopTuneProfile();
		}
		RDKPerfStubEvents().clear();
	}
	std::unique_ptr<GstPlayerPriv> mPlayerA;
	std::unique_ptr<GstPlayerPriv> mPlayerB;
};

TEST_F(InterfacePlayerRdkPerfTests, SuccessfulTuneRecordsOnceAndStaysOpenForClientCallback)
{
	mPlayerA->StartConfigureToFirstFrameProfile();
	EXPECT_TRUE(mPlayerA->IsConfigureToFirstFrameProfileActive());
	EXPECT_TRUE(PIIsTuneProfileActive());

	EXPECT_TRUE(mPlayerA->RecordConfigureToFirstFrame());
	EXPECT_FALSE(mPlayerA->RecordConfigureToFirstFrame());
	EXPECT_TRUE(mPlayerA->IsConfigureToFirstFrameProfileActive());
	{
		PI_PROFILE_SCOPE_IF("first_frame_client_callback", mPlayerA->IsConfigureToFirstFrameProfileActive());
	}
	EXPECT_EQ(RDKPerfStubEvents(), (std::vector<std::string>{"start:first_frame_client_callback", "stop:first_frame_client_callback"}));

	EXPECT_FALSE(mPlayerA->StopConfigureToFirstFrameProfile());
	EXPECT_FALSE(mPlayerA->IsConfigureToFirstFrameProfileActive());
	EXPECT_FALSE(PIIsTuneProfileActive());
}

TEST_F(InterfacePlayerRdkPerfTests, TimeoutReportedOnlyWithoutFirstFrame)
{
	mPlayerA->StartConfigureToFirstFrameProfile();
	EXPECT_TRUE(mPlayerA->StopConfigureToFirstFrameProfile(true));
	EXPECT_FALSE(mPlayerA->StopConfigureToFirstFrameProfile(true));
	EXPECT_FALSE(PIIsTuneProfileActive());

	mPlayerA->StartConfigureToFirstFrameProfile();
	EXPECT_TRUE(mPlayerA->RecordConfigureToFirstFrame());
	EXPECT_FALSE(mPlayerA->StopConfigureToFirstFrameProfile(true));
	EXPECT_FALSE(PIIsTuneProfileActive());
}

TEST_F(InterfacePlayerRdkPerfTests, StopEndsWindowWithoutReporting)
{
	mPlayerA->StartConfigureToFirstFrameProfile();
	EXPECT_FALSE(mPlayerA->StopConfigureToFirstFrameProfile());
	EXPECT_FALSE(mPlayerA->RecordConfigureToFirstFrame());
	EXPECT_FALSE(mPlayerA->IsConfigureToFirstFrameProfileActive());
	EXPECT_FALSE(PIIsTuneProfileActive());
}

TEST_F(InterfacePlayerRdkPerfTests, RetuneWhileActiveDecrementsOncePerWindow)
{
	mPlayerA->StartConfigureToFirstFrameProfile();
	mPlayerA->StartConfigureToFirstFrameProfile();
	mPlayerB->StartConfigureToFirstFrameProfile();
	mPlayerB->StopConfigureToFirstFrameProfile();
	EXPECT_TRUE(PIIsTuneProfileActive());
	mPlayerA->StopConfigureToFirstFrameProfile();
	EXPECT_FALSE(PIIsTuneProfileActive());
}

TEST_F(InterfacePlayerRdkPerfTests, TuningPlayerDoesNotEnableOtherPlayersScopes)
{
	mPlayerA->StartConfigureToFirstFrameProfile();
	EXPECT_TRUE(mPlayerA->IsConfigureToFirstFrameProfileActive());
	EXPECT_FALSE(mPlayerB->IsConfigureToFirstFrameProfileActive());
	{
		PI_PROFILE_SCOPE_IF("player_pause", mPlayerB->IsConfigureToFirstFrameProfileActive());
		PI_PROFILE_SCOPE_IF("pipeline_create", mPlayerA->IsConfigureToFirstFrameProfileActive());
	}
	EXPECT_EQ(RDKPerfStubEvents(), (std::vector<std::string>{"start:pipeline_create", "stop:pipeline_create"}));
	mPlayerA->StopConfigureToFirstFrameProfile();
}
