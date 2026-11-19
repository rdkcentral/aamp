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
#include <string>
#include <vector>
#include "PlayerLogManager.h"

class PlayerProfileScopeTests : public ::testing::Test
{
protected:
	void SetUp() override
	{
		ResetState();
	}
	void TearDown() override
	{
		ResetState();
	}
	static void ResetState()
	{
		while (PIIsTuneProfileActive())
		{
			PIStopTuneProfile();
		}
		PISetProfileAlwaysOn(false);
		RDKPerfStubEvents().clear();
	}
	static std::vector<std::string>& Events()
	{
		return RDKPerfStubEvents();
	}
};

TEST_F(PlayerProfileScopeTests, NotRecordedWithoutActiveTune)
{
	{
		PI_PROFILE_SCOPE("idle_scope");
	}
	EXPECT_TRUE(Events().empty());
}

TEST_F(PlayerProfileScopeTests, TimerCoversWholeScopeDuringTune)
{
	PIStartTuneProfile();
	{
		PI_PROFILE_SCOPE("tune_scope");
		Events().push_back("body");
	}
	PIStopTuneProfile();
	EXPECT_EQ(Events(), (std::vector<std::string>{"start:tune_scope", "body", "stop:tune_scope"}));
}

TEST_F(PlayerProfileScopeTests, NestedScopesAreRecordedInOrder)
{
	PIStartTuneProfile();
	{
		PI_PROFILE_SCOPE("outer");
		{
			PI_PROFILE_SCOPE("inner");
		}
	}
	PIStopTuneProfile();
	EXPECT_EQ(Events(), (std::vector<std::string>{"start:outer", "start:inner", "stop:inner", "stop:outer"}));
}

TEST_F(PlayerProfileScopeTests, ScopeIfFollowsExplicitCondition)
{
	PIStartTuneProfile();
	{
		PI_PROFILE_SCOPE_IF("other_player", false);
		PI_PROFILE_SCOPE_IF("this_player", true);
	}
	PIStopTuneProfile();
	EXPECT_EQ(Events(), (std::vector<std::string>{"start:this_player", "stop:this_player"}));
}

TEST_F(PlayerProfileScopeTests, AlwaysOnRecordsOutsideTune)
{
	PISetProfileAlwaysOn(true);
	EXPECT_TRUE(PIIsProfileAlwaysOn());
	{
		PI_PROFILE_SCOPE("seek_after_first_frame");
		PI_PROFILE_SCOPE_IF("player_scope", false);
	}
	EXPECT_EQ(Events(), (std::vector<std::string>{"start:seek_after_first_frame", "start:player_scope",
		"stop:player_scope", "stop:seek_after_first_frame"}));
}

TEST_F(PlayerProfileScopeTests, TuneCountTracksConcurrentPlayersAndNeverUnderflows)
{
	PIStopTuneProfile();
	EXPECT_FALSE(PIIsTuneProfileActive());
	PIStartTuneProfile();
	PIStartTuneProfile();
	PIStopTuneProfile();
	EXPECT_TRUE(PIIsTuneProfileActive());
	PIStopTuneProfile();
	EXPECT_FALSE(PIIsTuneProfileActive());
}
