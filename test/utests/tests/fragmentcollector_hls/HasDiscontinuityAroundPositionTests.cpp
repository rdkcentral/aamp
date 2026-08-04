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
 * @file HasDiscontinuityAroundPositionTests.cpp
 * @brief L1 tests for TrackState::HasDiscontinuityAroundPosition (RDKEMW-22611).
 *
 * These tests verify the PDT-based discontinuity pairing logic used to
 * synchronise audio and video tracks across an #EXT-X-DISCONTINUITY boundary.
 * The fix under test (RDKEMW-22611) ensures the AAMP_ERR_audioDiscontinue
 * telemetry marker is not raised for transient playlist-refresh-latency events
 * that always self-recover within one refresh cycle.
 *
 * Scenarios covered:
 *  1. Exact PDT match — returns true immediately (no refresh needed).
 *  2. PDT within targetDuration tolerance — returns true immediately.
 *  3. PDT diff > targetDuration and playlist already ahead — returns false;
 *     represents the genuine failure path that should fire the WARN log.
 *  4. No discontinuity entries in the index — returns false (empty index).
 *  5. Non-PDT path: position-based matching when discontinuityPDT == 0 and
 *     useDiscontinuityDateTime == false — returns true when playPosition
 *     falls within the culled-adjusted window.
 *  6. Non-PDT path: playPosition outside the window — returns false.
 *  7. Multiple stale + one current PDT entry — matches the current one.
 *
 * --- Loop-entry contract ---
 * HasDiscontinuityAroundPosition is gated by while(aamp->DownloadsAreEnabled()).
 * The fixture mocks this to return true so the loop body executes.
 * Early-exit on "not found" is guaranteed by setting mTrack->mProgramDateTime
 * ahead of (inputProgramDateTime + targetDurationSeconds), which triggers the
 * useProgramDateTimeIfAvailable break condition (line 6035 of
 * fragmentcollector_hls.cpp) without requiring a playlist-update wait.
 * useProgramDateTimeIfAvailable is activated by mocking
 * IsConfigSet(eAAMPConfig_HLSAVTrackSyncUsingStartTime) to return true.
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "priv_aamp.h"
#include "AampConfig.h"
#include "AampLogManager.h"
#include "fragmentcollector_hls.h"
#include "MockAampConfig.h"
#include "MockPrivateInstanceAAMP.h"

using ::testing::NiceMock;
using ::testing::Return;
using ::testing::_;

// ---------------------------------------------------------------------------
// Test subclass — overrides RunFetchLoop to prevent real thread spawning
// ---------------------------------------------------------------------------

/**
 * @brief Testable subclass of TrackState.
 *
 * Overrides RunFetchLoop() so that Start() does not spawn a blocking download
 * thread. No production logic is changed.
 */
class TestableTrackState : public TrackState
{
public:
    TestableTrackState(TrackType type,
                       StreamAbstractionAAMP_HLS* parent,
                       PrivateInstanceAAMP* aamp,
                       const char* name)
        : TrackState(type, parent, aamp, name) {}

    ~TestableTrackState()
    {
        threadDone = true;
    }

    bool threadDone{false};

	void RunFetchLoop() override
	{
		// No-op for unit tests: avoid background sleeps/loops.
		threadDone = true;
	}
};

// ---------------------------------------------------------------------------
// Fixture
// ---------------------------------------------------------------------------

/**
 * @brief Fixture for HasDiscontinuityAroundPosition tests.
 *
 * Creates the minimal object graph:
 *   PrivateInstanceAAMP → StreamAbstractionAAMP_HLS → TestableTrackState
 *
 * Default configuration:
 *  - DownloadsAreEnabled() = true  (loop body executes)
 *  - IsConfigSet(eAAMPConfig_HLSAVTrackSyncUsingStartTime) = true
 *    (useProgramDateTimeIfAvailable=true → PDT-based early-exit on no-match)
 *  - mTrack->targetDurationSeconds = 8.0 s (typical HLS CMAF live stream)
 *  - mTrack->mProgramDateTime = 0.0  (host playlist refresh time)
 *
 * For "not found" tests, each test sets mTrack->mProgramDateTime high enough
 * that the break condition fires immediately (see loop-entry contract above).
 */
class HasDiscontinuityAroundPositionTest : public ::testing::Test
{
protected:
    PrivateInstanceAAMP*        mAamp{nullptr};
    StreamAbstractionAAMP_HLS*  mHls{nullptr};
    TestableTrackState*         mTrack{nullptr};

    // Shorthand: the PDT that satisfies the immediate-exit break condition.
    // mProgramDateTime >= inputProgramDateTime + targetDurationSeconds
    // With inputProgramDateTime=0 and targetDurationSeconds=8, any value >= 8 works.
    static constexpr double kHighPdt = 100.0;

    void SetUp() override
    {
        if (gpGlobalConfig == nullptr)
            gpGlobalConfig = new AampConfig();

        g_mockAampConfig = std::make_shared<NiceMock<MockAampConfig>>();
        g_mockPrivateInstanceAAMP =
            std::make_shared<NiceMock<MockPrivateInstanceAAMP>>();

        mAamp  = new PrivateInstanceAAMP(gpGlobalConfig);
        mHls   = new StreamAbstractionAAMP_HLS(mAamp, 0, 0.0);
        mTrack = new TestableTrackState(eTRACK_VIDEO, mHls, mAamp, "video");

        // Allow the while-loop body to execute.
        ON_CALL(*g_mockPrivateInstanceAAMP, DownloadsAreEnabled())
            .WillByDefault(Return(true));

        // Activate the PDT-based early-exit so "not found" tests don't wait.
        ON_CALL(*g_mockAampConfig,
                IsConfigSet(eAAMPConfig_HLSAVTrackSyncUsingStartTime))
            .WillByDefault(Return(true));

        mTrack->targetDurationSeconds = 8.0;
        // mProgramDateTime is public; leave at default 0.0 for "found" tests.
        // "Not found" tests set it to kHighPdt.
    }

    void TearDown() override
    {
        delete mTrack;  mTrack = nullptr;
        delete mHls;    mHls   = nullptr;
        delete mAamp;   mAamp  = nullptr;

        g_mockPrivateInstanceAAMP.reset();
        g_mockAampConfig.reset();

        delete gpGlobalConfig; gpGlobalConfig = nullptr;
    }

    /**
     * @brief Append one DiscontinuityIndexNode to mTrack->mDiscontinuityIndex.
     *
     * @param relativePosition  Playlist-relative position (seconds from start).
     * @param pdt               discontinuityPDT (Unix epoch seconds, 0 = absent).
     * @param fragDuration      Fragment duration at the discontinuity boundary.
     */
    void AddDiscontinuity(double relativePosition, double pdt,
                          double fragDuration = 4.0)
    {
        DiscontinuityIndexNode node{};
        node.discontinuitySequenceIndex =
            static_cast<uint64_t>(mTrack->mDiscontinuityIndex.size());
        node.fragmentIdx      = static_cast<int>(mTrack->mDiscontinuityIndex.size());
        node.position         = relativePosition;
        node.discontinuityPDT = pdt;
        node.fragmentDuration = fragDuration;
        mTrack->mDiscontinuityIndex.push_back(node);
    }
};

// ---------------------------------------------------------------------------
// Tests — PDT path (useDiscontinuityDateTime = true)
// ---------------------------------------------------------------------------

/**
 * @test ExactPdtMatch_ReturnsTrue
 * @brief When the visitor PDT exactly matches an entry's discontinuityPDT,
 *        HasDiscontinuityAroundPosition must return true immediately.
 *
 * Normal case — no playlist-refresh latency. The marker must NOT fire.
 * Mirrors session 10 boxlog at 06:35:42 (diff = 0.000 s).
 */
TEST_F(HasDiscontinuityAroundPositionTest, ExactPdtMatch_ReturnsTrue)
{
    constexpr double kPdt = 1780641328.022;
    AddDiscontinuity(7190.984, kPdt);

    AampTime diff{};
    bool isDiffChkReq = true;
    // inputProgramDateTime=0 → break condition (mProgramDateTime=0 >= 0+8) false
    // → loop runs, finds exact match, returns true.
    bool result = mTrack->HasDiscontinuityAroundPosition(
        kPdt,    // visitor PDT == host PDT → exact match
        true,
        diff,
        7187.072,
        7.808,
        0.0,     // inputProgramDateTime
        isDiffChkReq);

    EXPECT_TRUE(result);
    EXPECT_NEAR(diff.inSeconds(), 0.0, 1e-3);
}

/**
 * @test PdtWithinTolerance_ReturnsTrue
 * @brief A PDT difference ≤ targetDurationSeconds (8 s) must still match.
 *
 * Covers the small audio/video PDT skew observed on live HLS CMAF streams:
 * e.g. 1780641386.596 vs 1780641386.580 → diff = 0.016 s << 8 s.
 */
TEST_F(HasDiscontinuityAroundPositionTest, PdtWithinTolerance_ReturnsTrue)
{
    constexpr double kHostPdt    = 1780641386.580;
    constexpr double kVisitorPdt = 1780641386.596; // diff = 0.016 s
    AddDiscontinuity(7192.520, kHostPdt);

    AampTime diff{};
    bool isDiffChkReq = true;
    bool result = mTrack->HasDiscontinuityAroundPosition(
        kVisitorPdt,
        true,
        diff,
        7192.576,
        62.464,
        0.0,
        isDiffChkReq);

    EXPECT_TRUE(result);
    // diff = discontinuityDateTime(host) - position(visitor) per line 5965
    EXPECT_NEAR(diff.inSeconds(), kHostPdt - kVisitorPdt, 1e-3);
}

/**
 * @test PdtDiffExceedsTolerance_ReturnsFalse
 * @brief When PDT diff > targetDuration and the host playlist is already ahead,
 *        HasDiscontinuityAroundPosition must return false (genuine failure).
 *
 * This is the path that should fire the "Ignoring discontinuity" WARN log
 * (RDKEMW-22611). mProgramDateTime is set to kHighPdt so the break condition
 * `mProgramDateTime >= inputProgramDateTime + targetDurationSeconds` fires
 * immediately after the scan finds no match.
 *
 * Mirrors session 10 at 06:36:07: visitor PDT 1780641358.102 vs host entry
 * 1780641328.022 → diff = 30.08 s >> targetDuration(8 s).
 */
TEST_F(HasDiscontinuityAroundPositionTest, PdtDiffExceedsTolerance_ReturnsFalse)
{
    // Host track has only the stale discontinuity (not yet refreshed).
    AddDiscontinuity(7167.561, 1780641328.022);
    // Force immediate exit after first miss via PDT break condition.
    mTrack->mProgramDateTime = kHighPdt;

    AampTime diff{};
    bool isDiffChkReq = true;
    bool result = mTrack->HasDiscontinuityAroundPosition(
        1780641358.102, // visitor PDT — 30 s ahead of only host entry
        true,
        diff,
        7193.728,
        31.232,
        0.0,            // inputProgramDateTime: kHighPdt(100) >= 0+8 → breaks
        isDiffChkReq);

    EXPECT_FALSE(result);
}

/**
 * @test EmptyIndex_ReturnsFalse
 * @brief An empty mDiscontinuityIndex must return false immediately.
 *
 * Guards against the case where the other track's playlist has not been
 * indexed yet (e.g. first refresh still in flight).
 */
TEST_F(HasDiscontinuityAroundPositionTest, EmptyIndex_ReturnsFalse)
{
    // No entries — mDiscontinuityIndex is empty.
    mTrack->mProgramDateTime = kHighPdt; // force exit on first miss

    AampTime diff{};
    bool isDiffChkReq = true;
    bool result = mTrack->HasDiscontinuityAroundPosition(
        1780641328.022,
        true,
        diff,
        7187.072,
        7.808,
        0.0,
        isDiffChkReq);

    EXPECT_FALSE(result);
}

// ---------------------------------------------------------------------------
// Tests — non-PDT path (useDiscontinuityDateTime = false, discontinuityPDT=0)
// ---------------------------------------------------------------------------

/**
 * @test NoPdt_PositionInWindow_ReturnsTrue
 * @brief Non-PDT path: playPosition inside the culled-delta window returns true.
 *
 * When discontinuityPDT==0 and useDiscontinuityDateTime==false the function
 * compares playPosition.nearestSecond() against
 * [pos - |deltaCulledSec| - targetDuration - 1,
 *  pos + |deltaCulledSec| + targetDuration + 1].
 *
 * With deltaCulledSec=0 (inputCulledSec == mCulledSeconds==0) and
 * targetDuration=8, the window around pos=7192 is [7183, 7201].
 * playPosition=7191 is inside → must return true.
 * Also verifies isDiffChkReq is cleared to false (no diff check needed when
 * the host entry has no PDT).
 */
TEST_F(HasDiscontinuityAroundPositionTest, NoPdt_PositionInWindow_ReturnsTrue)
{
    AddDiscontinuity(7192.0, /*pdt=*/0.0);
    // mCulledSeconds defaults to 0.0; inputCulledSec=0.0 → deltaCulledSec=0.
    // Window: [7183, 7201].  playPosition=7191 is inside.
    // mProgramDateTime stays 0.0; inputProgramDateTime=0.0 →
    // break condition (0 >= 0+8) is false → loop runs, finds match.

    AampTime diff{};
    bool isDiffChkReq = true;
    bool result = mTrack->HasDiscontinuityAroundPosition(
        7191.0,  // position (not epoch on non-PDT path, but not used for comparison)
        false,   // useDiscontinuityDateTime = false
        diff,
        7191.0,  // playPosition — inside [7183, 7201]
        0.0,     // inputCulledSec == mCulledSeconds(0) → deltaCulledSec=0
        0.0,     // inputProgramDateTime
        isDiffChkReq);

    EXPECT_TRUE(result);
    // isDiffChkReq must be cleared when discontinuityPDT==0 on host side.
    EXPECT_FALSE(isDiffChkReq);
}

/**
 * @test NoPdt_PositionOutsideWindow_ReturnsFalse
 * @brief Non-PDT path: playPosition outside the window returns false.
 *
 * Same setup as NoPdt_PositionInWindow_ReturnsTrue but playPosition=7050
 * is well outside [7183, 7201]. mProgramDateTime set high to ensure the
 * "not found" path exits cleanly without waiting for a playlist refresh.
 */
TEST_F(HasDiscontinuityAroundPositionTest, NoPdt_PositionOutsideWindow_ReturnsFalse)
{
    AddDiscontinuity(7192.0, /*pdt=*/0.0);
    mTrack->mProgramDateTime = kHighPdt; // force exit after miss

    AampTime diff{};
    bool isDiffChkReq = true;
    bool result = mTrack->HasDiscontinuityAroundPosition(
        7050.0,  // position
        false,
        diff,
        7050.0,  // playPosition — outside [7183, 7201]
        0.0,
        0.0,
        isDiffChkReq);

    EXPECT_FALSE(result);
}

// ---------------------------------------------------------------------------
// Tests — multi-entry index
// ---------------------------------------------------------------------------

/**
 * @test MultipleEntries_MatchesCurrentPdt
 * @brief With multiple stale + one current PDT entry the function must find
 *        the matching entry and return true.
 *
 * Replicates session 10 state at 06:36:24 after the second playlist refresh:
 *   index[0] pdt=1780641328.022 (stale, diff=45.079 s > 8 s → skip)
 *   index[1] pdt=1780641358.102 (stale, diff=14.999 s > 8 s → skip)
 *   index[2] pdt=1780641373.101 (current, diff=0.000 s → match)
 * Visitor PDT = 1780641373.101.
 */
TEST_F(HasDiscontinuityAroundPositionTest, MultipleEntries_MatchesCurrentPdt)
{
    AddDiscontinuity(7144.128, 1780641328.022);
    AddDiscontinuity(7174.208, 1780641358.102);
    AddDiscontinuity(7189.248, 1780641373.101); // current entry

    AampTime diff{};
    bool isDiffChkReq = true;
    bool result = mTrack->HasDiscontinuityAroundPosition(
        1780641373.101,
        true,
        diff,
        7189.248,
        46.847,
        0.0,
        isDiffChkReq);

    EXPECT_TRUE(result);
    EXPECT_NEAR(diff.inSeconds(), 0.0, 1e-3);
}
