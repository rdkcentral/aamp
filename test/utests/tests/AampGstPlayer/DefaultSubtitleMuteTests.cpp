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
#include "middleware/InterfacePlayerPriv.h"
using ::testing::_;
/**
 * @brief Test fixture for default subtitle mute state
 * 
 * This test suite validates that the default subtitle/CC rendering behavior
 * is correctly initialized and applied when the subtitle sink is created.
 */
class DefaultSubtitleMuteTests : public ::testing::Test
{
protected:
	InterfacePlayerRDK *mInterfacePlayerRDK{};
	void SetUp() override
	{
		// Create a fresh InterfacePlayerRDK instance
		mInterfacePlayerRDK = new InterfacePlayerRDK();
	}
	void TearDown() override
	{
		delete mInterfacePlayerRDK;
		mInterfacePlayerRDK = nullptr;
	}
};
/**
 * @brief Test that GstPlayerPriv::subtitleMuted defaults to true
 * 
 * This test verifies that subtitles start in a muted state by default,
 * ensuring CC/WebVTT rendering is disabled unless explicitly enabled by the user.
 * 
 * Related to VPLAY-12061: Closed captioning issues after introducing webvTT 
 * along with inBand CC tracks. The fix disables webvtt rendering by default.
 */
TEST_F(DefaultSubtitleMuteTests, GstPlayerPriv_DefaultSubtitleMutedState_IsTrue)
{
	// Verify that the default state of subtitleMuted is true
	ASSERT_TRUE(mInterfacePlayerRDK->interfacePlayerPriv->gstPrivateContext->subtitleMuted);
}
/**
 * @brief Test that default subtitle mute state prevents rendering
 * 
 * This test ensures that when subtitles are muted by default, they remain
 * muted until explicitly unmuted by the application, preventing unexpected
 * subtitle rendering behavior.
 */
TEST_F(DefaultSubtitleMuteTests, GstPlayerPriv_DefaultState_PreventsSubtitleRendering)
{
	// Given: Fresh InterfacePlayerRDK instance with default state
	auto *gstPriv = mInterfacePlayerRDK->interfacePlayerPriv->gstPrivateContext;
	
	// Then: Subtitle mute should be enabled (true)
	EXPECT_TRUE(gstPriv->subtitleMuted);
	
	// And: Video and audio mute should be disabled (false) by default
	EXPECT_FALSE(gstPriv->videoMuted);
	EXPECT_FALSE(gstPriv->audioMuted);
}
/**
 * @brief Test that SetSubtitleMute correctly updates the state
 * 
 * This test validates that the SetSubtitleMute method properly updates
 * the internal subtitleMuted state, which is used when creating the
 * subtitle sink to set its initial mute property.
 */
TEST_F(DefaultSubtitleMuteTests, SetSubtitleMute_UpdatesInternalState_Correctly)
{
	auto *gstPriv = mInterfacePlayerRDK->interfacePlayerPriv->gstPrivateContext;
	
	// Initially should be muted (true)
	ASSERT_TRUE(gstPriv->subtitleMuted);
	
	// When: Unmuting subtitles
	mInterfacePlayerRDK->SetSubtitleMute(false);
	
	// Then: Internal state should be updated
	EXPECT_FALSE(gstPriv->subtitleMuted);
	
	// When: Re-muting subtitles
	mInterfacePlayerRDK->SetSubtitleMute(true);
	
	// Then: Internal state should reflect muted again
	EXPECT_TRUE(gstPriv->subtitleMuted);
}
/**
 * @brief Test initial state consistency across player lifecycle
 * 
 * This test ensures that creating multiple InterfacePlayerRDK instances
 * consistently initializes subtitleMuted to true, preventing state pollution
 * between different player instances.
 */
TEST_F(DefaultSubtitleMuteTests, MultipleInstances_ConsistentDefaultState)
{
	// Create multiple instances and verify consistent default state
	InterfacePlayerRDK *player1 = new InterfacePlayerRDK();
	InterfacePlayerRDK *player2 = new InterfacePlayerRDK();
	InterfacePlayerRDK *player3 = new InterfacePlayerRDK();
	
	// All instances should have subtitles muted by default
	EXPECT_TRUE(player1->interfacePlayerPriv->gstPrivateContext->subtitleMuted);
	EXPECT_TRUE(player2->interfacePlayerPriv->gstPrivateContext->subtitleMuted);
	EXPECT_TRUE(player3->interfacePlayerPriv->gstPrivateContext->subtitleMuted);
	
	// Clean up
	delete player1;
	delete player2;
	delete player3;
}
