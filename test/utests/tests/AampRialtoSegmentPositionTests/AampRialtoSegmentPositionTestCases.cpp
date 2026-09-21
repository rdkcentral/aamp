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
 * @file AampRialtoSegmentPositionTestCases.cpp
 * @brief Unit tests for AampRialtoSegmentPosition.
 *
 * These pin the interlock rules that previously lived as loose atomics on
 * AampRialtoPlayer: what a Flush() stages, when the authority flag is
 * consumed, which injector thread drives a deferred flush, and what
 * survives end of session.
 */

#include <gtest/gtest.h>

#include <atomic>
#include <thread>
#include <vector>

#include "AampRialtoSegmentPosition.h"

class AampRialtoSegmentPositionTest : public ::testing::Test
{
protected:
	AampRialtoSegmentPosition m_position;
};

TEST_F(AampRialtoSegmentPositionTest, InitialState_NothingStaged)
{
	/**
	 * @brief A fresh session has no staged position, a normal rate and a
	 *        zero baseline, so GetPositionMilliseconds() reports from the
	 *        start of the timeline.
	 */
	EXPECT_EQ(m_position.StagedPositionNs(),
		AampRialtoSegmentPosition::kNoStagedPosition);
	EXPECT_EQ(m_position.StagedRate(), AAMP_NORMAL_PLAY_RATE);
	EXPECT_EQ(m_position.BaselineNs(), 0);
}

TEST_F(AampRialtoSegmentPositionTest, StageRequested_RecordsPositionAndRate)
{
	/**
	 * @brief Flush() stages the caller's position and rate for
	 *        AttachSource() and the eventual SEEK_DONE commit.
	 */
	m_position.StageRequested(5'000'000'000LL, /*rate=*/4,
		/*authoritative=*/false);

	EXPECT_EQ(m_position.StagedPositionNs(), 5'000'000'000LL);
	EXPECT_EQ(m_position.StagedRate(), 4);
}

TEST_F(AampRialtoSegmentPositionTest, StageRequested_AcceptsNegativePosition)
{
	/**
	 * @brief The staged value is passed through unclamped; clamping is the
	 *        caller's concern at the point it is applied to a source.
	 */
	m_position.StageRequested(-1'000LL, AAMP_NORMAL_PLAY_RATE,
		/*authoritative=*/false);

	EXPECT_EQ(m_position.StagedPositionNs(), -1'000LL);
}

TEST_F(AampRialtoSegmentPositionTest, ConsumeAuthoritative_AnswersOnce)
{
	/**
	 * @brief Configure() consults the flag exactly once when deciding
	 *        whether re-arming the position hold would be redundant.
	 */
	m_position.StageRequested(1'000LL, AAMP_NORMAL_PLAY_RATE,
		/*authoritative=*/true);

	EXPECT_TRUE(m_position.ConsumeAuthoritative());
	EXPECT_FALSE(m_position.ConsumeAuthoritative());
}

TEST_F(AampRialtoSegmentPositionTest,
	StageRequested_NonAuthoritativeClearsEarlierAuthority)
{
	/**
	 * @brief The flag is mirrored unconditionally, so a later placeholder
	 *        flush cannot leave a stale true from an earlier authoritative
	 *        one still pending.
	 */
	m_position.StageRequested(1'000LL, AAMP_NORMAL_PLAY_RATE,
		/*authoritative=*/true);
	m_position.StageRequested(2'000LL, AAMP_NORMAL_PLAY_RATE,
		/*authoritative=*/false);

	EXPECT_FALSE(m_position.ConsumeAuthoritative());
}

TEST_F(AampRialtoSegmentPositionTest, ClaimDeferredFlush_SucceedsOnlyOnce)
{
	/**
	 * @brief Video and audio inject on separate threads; only one may
	 *        drive the deferred Flush() for a given armed window.
	 */
	m_position.ResetFlushClaim();

	EXPECT_TRUE(m_position.ClaimDeferredFlush());
	EXPECT_FALSE(m_position.ClaimDeferredFlush());
	EXPECT_FALSE(m_position.ClaimDeferredFlush());
}

TEST_F(AampRialtoSegmentPositionTest, ResetFlushClaim_ReopensElection)
{
	/**
	 * @brief Each newly armed window re-opens the election so the next
	 *        period's first sample can drive its own flush.
	 */
	m_position.ResetFlushClaim();
	ASSERT_TRUE(m_position.ClaimDeferredFlush());

	m_position.ResetFlushClaim();

	EXPECT_TRUE(m_position.ClaimDeferredFlush());
}

TEST_F(AampRialtoSegmentPositionTest, ClaimDeferredFlush_IsRaceFree)
{
	/**
	 * @brief Concurrent injector threads must produce exactly one winner.
	 */
	constexpr int kIterations = 2000;

	for (int i = 0; i < kIterations; ++i)
	{
		AampRialtoSegmentPosition position;
		position.ResetFlushClaim();

		std::atomic<int> winners{0};
		std::atomic<bool> go{false};
		std::vector<std::thread> threads;
		for (int t = 0; t < 2; ++t)
		{
			threads.emplace_back([&]
			{
				while (!go.load(std::memory_order_acquire)) { }
				if (position.ClaimDeferredFlush())
				{
					++winners;
				}
			});
		}

		go.store(true, std::memory_order_release);
		for (auto &thread : threads)
		{
			thread.join();
		}

		ASSERT_EQ(winners.load(), 1) << "iteration " << i;
	}
}

TEST_F(AampRialtoSegmentPositionTest, CommitBaseline_UpdatesBaseline)
{
	/**
	 * @brief SEEK_DONE and a first video attach both establish where the
	 *        current segment starts.
	 */
	m_position.CommitBaseline(7'000'000'000LL);

	EXPECT_EQ(m_position.BaselineNs(), 7'000'000'000LL);
}

TEST_F(AampRialtoSegmentPositionTest, CommitBaseline_IsIndependentOfStaging)
{
	/**
	 * @brief Staging a new flush position must not move the baseline until
	 *        that flush actually commits.
	 */
	m_position.CommitBaseline(3'000'000'000LL);

	m_position.StageRequested(9'000'000'000LL, AAMP_NORMAL_PLAY_RATE,
		/*authoritative=*/false);

	EXPECT_EQ(m_position.BaselineNs(), 3'000'000'000LL);
}

TEST_F(AampRialtoSegmentPositionTest, ClearForNewSession_DiscardsStagedAndBaseline)
{
	/**
	 * @brief Stop() ends the session, so neither the staged seek position
	 *        nor the baseline may leak into the next tune.
	 */
	m_position.StageRequested(5'000'000'000LL, /*rate=*/2,
		/*authoritative=*/false);
	m_position.CommitBaseline(4'000'000'000LL);

	m_position.ClearForNewSession();

	EXPECT_EQ(m_position.StagedPositionNs(),
		AampRialtoSegmentPosition::kNoStagedPosition);
	EXPECT_EQ(m_position.BaselineNs(), 0);
}

TEST_F(AampRialtoSegmentPositionTest, ClearForNewSession_KeepsStagedRate)
{
	/**
	 * @brief The staged rate is consumed by the next flush cycle, not by
	 *        session teardown, so Stop() must leave it untouched.
	 */
	m_position.StageRequested(5'000'000'000LL, /*rate=*/2,
		/*authoritative=*/false);

	m_position.ClearForNewSession();

	EXPECT_EQ(m_position.StagedRate(), 2);
}

TEST_F(AampRialtoSegmentPositionTest, ClearForNewSession_KeepsAuthority)
{
	/**
	 * @brief The authority flag is consumed by Configure(), which may run
	 *        after a teardown Stop(), so it must survive.
	 */
	m_position.StageRequested(5'000'000'000LL, AAMP_NORMAL_PLAY_RATE,
		/*authoritative=*/true);

	m_position.ClearForNewSession();

	EXPECT_TRUE(m_position.ConsumeAuthoritative());
}
