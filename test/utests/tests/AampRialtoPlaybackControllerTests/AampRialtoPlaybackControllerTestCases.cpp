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

/**
 * @file AampRialtoPlaybackControllerTests.cpp
 * @brief Unit tests for AampRialtoPlaybackController.
 *
 * The controller is the single decision point for "may play() be issued
 * now?".  These tests pin the behaviour that AampRialtoPlayer previously
 * spread across Stream(), CheckAllSourcesAttached() and the SEEK_DONE
 * handler, including the guarantee that a play request is consumed exactly
 * once when two threads race to satisfy it.
 */

#include <gtest/gtest.h>

#include <atomic>
#include <string>
#include <thread>
#include <vector>

#include "AampRialtoPlaybackController.h"

class AampRialtoPlaybackControllerTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		m_controller = std::make_unique<AampRialtoPlaybackController>(
			[this](const char *reason)
			{
				++m_playCount;
				m_lastReason = (reason != nullptr) ? reason : "";
			});
	}

	void TearDown() override
	{
		m_controller.reset();
	}

	/// Put the controller into the "nothing is holding playback" state that
	/// a fully attached, non-flushing, position-resolved player reaches.
	void ReleaseAllHolds()
	{
		m_controller->ReleaseHold(PlayHold::SourcesNotAttached, "test");
		m_controller->ReleaseHold(PlayHold::Flushing, "test");
		m_controller->ReleaseHold(PlayHold::PositionPending, "test");
		m_controller->ReleaseHold(PlayHold::FragmentCaching, "test");
		m_playCount = 0;
		m_lastReason.clear();
	}

	std::unique_ptr<AampRialtoPlaybackController> m_controller;
	std::atomic<int> m_playCount{0};
	std::string m_lastReason;
};

TEST_F(AampRialtoPlaybackControllerTest, InitialState_SourcesNotAttachedIsHeld)
{
	/**
	 * @brief A freshly constructed controller mirrors a player that has not
	 *        yet attached any source, so play must not be issuable.
	 */
	EXPECT_TRUE(m_controller->IsHeld(PlayHold::SourcesNotAttached));
	EXPECT_FALSE(m_controller->IsHeld(PlayHold::Flushing));
	EXPECT_FALSE(m_controller->IsHeld(PlayHold::PositionPending));
	EXPECT_FALSE(m_controller->IsHeld(PlayHold::FragmentCaching));
	EXPECT_FALSE(m_controller->IsPlayPending());
	EXPECT_EQ(m_playCount, 0);
}

TEST_F(AampRialtoPlaybackControllerTest, RequestPlay_NoHolds_FiresImmediately)
{
	/**
	 * @brief Mirrors Stream() arriving after every precondition is already
	 *        satisfied: play() is issued synchronously.
	 */
	ReleaseAllHolds();

	m_controller->RequestPlay("Stream");

	EXPECT_EQ(m_playCount, 1);
	EXPECT_EQ(m_lastReason, "Stream");
	EXPECT_FALSE(m_controller->IsPlayPending());
}

TEST_F(AampRialtoPlaybackControllerTest, RequestPlay_WhileHeld_DoesNotFire)
{
	/**
	 * @brief Mirrors Stream() arriving before allSourcesAttached(): the
	 *        request is remembered but play() is deferred.
	 */
	m_controller->RequestPlay("Stream");

	EXPECT_EQ(m_playCount, 0);
	EXPECT_TRUE(m_controller->IsPlayPending());
}

TEST_F(AampRialtoPlaybackControllerTest, ReleaseLastHold_FiresDeferredPlay)
{
	/**
	 * @brief Mirrors CheckAllSourcesAttached() issuing the play() that
	 *        Stream() deferred.
	 */
	m_controller->RequestPlay("Stream");
	ASSERT_EQ(m_playCount, 0);

	m_controller->ReleaseHold(PlayHold::SourcesNotAttached,
		"CheckAllSourcesAttached");

	EXPECT_EQ(m_playCount, 1);
	EXPECT_EQ(m_lastReason, "CheckAllSourcesAttached");
	EXPECT_FALSE(m_controller->IsPlayPending());
}

TEST_F(AampRialtoPlaybackControllerTest,
	ReleaseHold_OtherHoldsRemaining_DoesNotFire)
{
	/**
	 * @brief Mirrors SEEK_DONE arriving while a newer Discontinuity() has
	 *        already re-armed the position: the request must survive until
	 *        the remaining hold clears.
	 */
	m_controller->AddHold(PlayHold::Flushing, "Flush");
	m_controller->AddHold(PlayHold::PositionPending, "Discontinuity");
	m_controller->ReleaseHold(PlayHold::SourcesNotAttached, "attach");
	m_controller->RequestPlay("Stream");
	ASSERT_EQ(m_playCount, 0);

	m_controller->ReleaseHold(PlayHold::Flushing, "SEEK_DONE");

	EXPECT_EQ(m_playCount, 0);
	EXPECT_TRUE(m_controller->IsPlayPending());

	m_controller->ReleaseHold(PlayHold::PositionPending, "Flush");

	EXPECT_EQ(m_playCount, 1);
	EXPECT_EQ(m_lastReason, "Flush");
}

TEST_F(AampRialtoPlaybackControllerTest,
	ReleaseHold_NoPlayRequested_DoesNotFire)
{
	/**
	 * @brief Mirrors a seek performed while paused: no Stream() was called,
	 *        so SEEK_DONE must not start playback.
	 */
	m_controller->AddHold(PlayHold::Flushing, "Flush");

	m_controller->ReleaseHold(PlayHold::Flushing, "SEEK_DONE");
	m_controller->ReleaseHold(PlayHold::SourcesNotAttached, "attach");

	EXPECT_EQ(m_playCount, 0);
}

TEST_F(AampRialtoPlaybackControllerTest, DeferredPlay_FiresOnlyOnce)
{
	/**
	 * @brief A single deferred request must be consumed by whichever site
	 *        releases the final hold, and must not fire again when an
	 *        unrelated hold is later released.
	 */
	m_controller->AddHold(PlayHold::Flushing, "Flush");
	m_controller->RequestPlay("Stream");

	m_controller->ReleaseHold(PlayHold::SourcesNotAttached, "attach");
	ASSERT_EQ(m_playCount, 0);
	m_controller->ReleaseHold(PlayHold::Flushing, "SEEK_DONE");
	ASSERT_EQ(m_playCount, 1);

	m_controller->AddHold(PlayHold::Flushing, "Flush");
	m_controller->ReleaseHold(PlayHold::Flushing, "SEEK_DONE");

	EXPECT_EQ(m_playCount, 1);
}

TEST_F(AampRialtoPlaybackControllerTest, RequestPlay_WhenUnheld_FiresEachTime)
{
	/**
	 * @brief AAMP treats Stream() as unconditional, so repeated requests
	 *        must each re-issue play() rather than being deduplicated.
	 */
	ReleaseAllHolds();

	m_controller->RequestPlay("Stream");
	m_controller->RequestPlay("Stream");
	m_controller->RequestPlay("Stream");

	EXPECT_EQ(m_playCount, 3);
}

TEST_F(AampRialtoPlaybackControllerTest, CancelPlayRequest_PreventsDeferredPlay)
{
	/**
	 * @brief Mirrors Stop() and an explicit Pause(true) discarding a play
	 *        request that was still deferred.
	 */
	m_controller->RequestPlay("Stream");
	ASSERT_TRUE(m_controller->IsPlayPending());

	m_controller->CancelPlayRequest("Stop");
	EXPECT_FALSE(m_controller->IsPlayPending());

	m_controller->ReleaseHold(PlayHold::SourcesNotAttached, "attach");

	EXPECT_EQ(m_playCount, 0);
}

TEST_F(AampRialtoPlaybackControllerTest, AddHold_IsIdempotent)
{
	/**
	 * @brief Adding an already-applied hold must not require a matching
	 *        extra release; Configure() may re-arm a hold that is already
	 *        set.
	 */
	m_controller->AddHold(PlayHold::PositionPending, "Configure");
	m_controller->AddHold(PlayHold::PositionPending, "Discontinuity");
	m_controller->ReleaseHold(PlayHold::SourcesNotAttached, "attach");
	m_controller->RequestPlay("Stream");

	m_controller->ReleaseHold(PlayHold::PositionPending, "Flush");

	EXPECT_FALSE(m_controller->IsHeld(PlayHold::PositionPending));
	EXPECT_EQ(m_playCount, 1);
}

TEST_F(AampRialtoPlaybackControllerTest, ReleaseHold_NotHeld_IsHarmless)
{
	/**
	 * @brief Releasing a hold that was never applied must neither fire
	 *        play() nor disturb the remaining holds.
	 */
	m_controller->ReleaseHold(PlayHold::FragmentCaching,
		"NotifyFragmentCachingComplete");

	EXPECT_EQ(m_playCount, 0);
	EXPECT_TRUE(m_controller->IsHeld(PlayHold::SourcesNotAttached));
}

TEST_F(AampRialtoPlaybackControllerTest, HoldsAreIndependent)
{
	/**
	 * @brief Each hold must be tracked separately so releasing one cannot
	 *        clear another.
	 */
	m_controller->AddHold(PlayHold::Flushing, "Flush");
	m_controller->AddHold(PlayHold::PositionPending, "Configure");
	m_controller->AddHold(PlayHold::FragmentCaching, "caching");

	m_controller->ReleaseHold(PlayHold::PositionPending, "Flush");

	EXPECT_TRUE(m_controller->IsHeld(PlayHold::SourcesNotAttached));
	EXPECT_TRUE(m_controller->IsHeld(PlayHold::Flushing));
	EXPECT_FALSE(m_controller->IsHeld(PlayHold::PositionPending));
	EXPECT_TRUE(m_controller->IsHeld(PlayHold::FragmentCaching));
}

TEST_F(AampRialtoPlaybackControllerTest, PlayActionMayReenterController)
{
	/**
	 * @brief The play action performs blocking Rialto IPC and may observe
	 *        or change controller state, so it must be invoked with the
	 *        internal mutex released.  A deadlock fails this test by
	 *        hanging rather than by assertion.
	 */
	bool observedHold = true;
	AampRialtoPlaybackController controller(
		[&](const char *)
		{
			observedHold = controller.IsHeld(PlayHold::SourcesNotAttached);
			controller.AddHold(PlayHold::FragmentCaching, "reentrant");
		});

	controller.ReleaseHold(PlayHold::SourcesNotAttached, "attach");
	controller.RequestPlay("Stream");

	EXPECT_FALSE(observedHold);
	EXPECT_TRUE(controller.IsHeld(PlayHold::FragmentCaching));
}

TEST_F(AampRialtoPlaybackControllerTest, ConcurrentRequestAndRelease_FiresOnce)
{
	/**
	 * @brief Replaces the hand-rolled seq_cst rendezvous between
	 *        m_playRequested and m_allSourcesAttachedFlag: when Stream()
	 *        and CheckAllSourcesAttached() race, exactly one of them must
	 *        issue play() — never zero, never two.
	 */
	constexpr int kIterations = 2000;

	for (int i = 0; i < kIterations; ++i)
	{
		std::atomic<int> plays{0};
		AampRialtoPlaybackController controller(
			[&plays](const char *) { ++plays; });

		std::atomic<bool> go{false};
		std::thread requester([&]
		{
			while (!go.load(std::memory_order_acquire)) { }
			controller.RequestPlay("Stream");
		});
		std::thread releaser([&]
		{
			while (!go.load(std::memory_order_acquire)) { }
			controller.ReleaseHold(PlayHold::SourcesNotAttached,
				"CheckAllSourcesAttached");
		});

		go.store(true, std::memory_order_release);
		requester.join();
		releaser.join();

		ASSERT_EQ(plays.load(), 1) << "iteration " << i;
		ASSERT_FALSE(controller.IsPlayPending()) << "iteration " << i;
	}
}
