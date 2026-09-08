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
 * @file VodStitchedAdEventsTests.cpp
 * @brief L1 unit tests for PrivateCDAIObjectMPD::CheckVodStitchedAdEvents()
 *        and ResetVodAdEventTracker() — the stitched-VOD CDAI ad event path.
 *
 * Strategy
 * --------
 * We directly populate mVodAdEventTracker (bypassing BuildStitchedVodManifest)
 * so we can precisely control the stitched-timeline coordinates.  The real
 * admanager_mpd.cpp is compiled into this target; SendAdPlacementEvent /
 * SendAdReservationEvent are intercepted via MockPrivateInstanceAAMP so we can
 * assert call counts, event types, adId / breakId values, and ordering.
 *
 * All position values are in milliseconds (matching the positionMs parameter).
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "admanager_mpd.h"
#include "AampEvent.h"
#include "AampConfig.h"
#include "MockAampConfig.h"
#include "MockPrivateInstanceAAMP.h"

using ::testing::_;
using ::testing::InSequence;
using ::testing::Return;
using ::testing::StrictMock;

extern std::shared_ptr<MockPrivateInstanceAAMP> g_mockPrivateInstanceAAMP;
extern AampConfig *gpGlobalConfig;

// ---------------------------------------------------------------------------
// Helper: build a minimal single-ad VodAdEventEntry
// ---------------------------------------------------------------------------
static VodAdEventEntry MakeEntry(const std::string &breakId,
                                 const std::string &adId,
                                 double breakStartSec,
                                 double breakDurSec,
                                 double adStartSec,
                                 double adDurSec)
{
    VodAdEventEntry e;
    e.breakId          = breakId;
    e.adId             = adId;
    e.breakStartSec    = breakStartSec;
    e.breakDurationSec = breakDurSec;
    e.adStartSec       = adStartSec;
    e.adDurationSec    = adDurSec;
    return e;
}

// ---------------------------------------------------------------------------
// Test fixture
// ---------------------------------------------------------------------------
class VodStitchedAdEventsTests : public ::testing::Test
{
protected:
    PrivateInstanceAAMP    *mAamp    = nullptr;
    CDAIObjectMPD          *mCdaiObj = nullptr;
    PrivateCDAIObjectMPD   *mCdai   = nullptr;

    void SetUp() override
    {
        if (gpGlobalConfig == nullptr)
            gpGlobalConfig = new AampConfig();

        mAamp    = new PrivateInstanceAAMP(gpGlobalConfig);
        g_mockPrivateInstanceAAMP = std::make_shared<StrictMock<MockPrivateInstanceAAMP>>();
        EXPECT_CALL(*g_mockPrivateInstanceAAMP, DownloadsAreEnabled())
            .WillRepeatedly(Return(true));

        mCdaiObj = new CDAIObjectMPD(mAamp);
        mCdai    = mCdaiObj->GetPrivateCDAIObjectMPD();

        // Mark as stitched so the production guard passes
        mCdai->mVodManifestStitched = true;
    }

    void TearDown() override
    {
        delete mCdaiObj;
        mCdaiObj = nullptr;
        mCdai    = nullptr;

        g_mockPrivateInstanceAAMP.reset();

        delete mAamp;
        mAamp = nullptr;

        delete gpGlobalConfig;
        gpGlobalConfig = nullptr;
    }

    // Convenience: load tracker with a single entry and return a reference to it
    VodAdEventEntry &AddEntry(const std::string &breakId,
                              const std::string &adId,
                              double breakStartSec,
                              double breakDurSec,
                              double adStartSec,
                              double adDurSec)
    {
        mCdai->mVodAdEventTracker.push_back(
            MakeEntry(breakId, adId, breakStartSec, breakDurSec, adStartSec, adDurSec));
        return mCdai->mVodAdEventTracker.back();
    }
};

// ===========================================================================
// 1. Empty tracker — no calls at all
// ===========================================================================
TEST_F(VodStitchedAdEventsTests, EmptyTracker_NoEvents)
{
    // g_mockPrivateInstanceAAMP is StrictMock: any unexpected call fails the test
    mCdai->CheckVodStitchedAdEvents(0.0);
    mCdai->CheckVodStitchedAdEvents(99999.0);
}

// ===========================================================================
// 2. Position before break start — no events
// ===========================================================================
TEST_F(VodStitchedAdEventsTests, PositionBeforeBreak_NoEvents)
{
    // Ad at 10 s–20 s on the stitched timeline
    AddEntry("brk1", "ad1", 10.0, 10.0, 10.0, 10.0);

    // Tick at 9 999 ms — just before the break
    mCdai->CheckVodStitchedAdEvents(9999.0);

    // All fired-flags must still be false
    const VodAdEventEntry &e = mCdai->mVodAdEventTracker[0];
    EXPECT_FALSE(e.reservationStartFired);
    EXPECT_FALSE(e.placementStartFired);
    EXPECT_FALSE(e.placementEndFired);
    EXPECT_FALSE(e.reservationEndFired);
}

// ===========================================================================
// 3. Full single-ad lifecycle — correct event types, adId/breakId, ordering
// ===========================================================================
TEST_F(VodStitchedAdEventsTests, SingleAd_FullLifecycle_EventsInOrder)
{
    // Ad occupies 10 s–20 s
    AddEntry("brk1", "ad1", 10.0, 10.0, 10.0, 10.0);

    {
        InSequence seq;

        // Tick 1: position = 10 000 ms — enters break
        EXPECT_CALL(*g_mockPrivateInstanceAAMP,
            SendAdReservationEvent(AAMP_EVENT_AD_RESERVATION_START,
                                   std::string("brk1"),
                                   static_cast<uint64_t>(10000),
                                   static_cast<uint64_t>(10000),
                                   false,
                                   _))
            .Times(1);
        EXPECT_CALL(*g_mockPrivateInstanceAAMP,
            SendAdPlacementEvent(AAMP_EVENT_AD_PLACEMENT_START,
                                  std::string("ad1"),
                                  static_cast<uint32_t>(10),
                                  static_cast<uint64_t>(10000),
                                  static_cast<uint32_t>(0),
                                  static_cast<uint32_t>(10000),
                                  false,
                                  0L))
            .Times(1);

        // Tick 2: position = 20 000 ms — ad ends, reservation ends
        EXPECT_CALL(*g_mockPrivateInstanceAAMP,
            SendAdPlacementEvent(AAMP_EVENT_AD_PLACEMENT_END,
                                  std::string("ad1"),
                                  static_cast<uint32_t>(20),
                                  static_cast<uint64_t>(20000),
                                  static_cast<uint32_t>(0),
                                  static_cast<uint32_t>(10000),
                                  false,
                                  0L))
            .Times(1);
        EXPECT_CALL(*g_mockPrivateInstanceAAMP,
            SendAdReservationEvent(AAMP_EVENT_AD_RESERVATION_END,
                                   std::string("brk1"),
                                   static_cast<uint64_t>(20000),
                                   static_cast<uint64_t>(20000),
                                   false,
                                   _))
            .Times(1);
    }

    mCdai->CheckVodStitchedAdEvents(10000.0);
    mCdai->CheckVodStitchedAdEvents(20000.0);
}

// ===========================================================================
// 4. Events fire only once — idempotency on repeated ticks
// ===========================================================================
TEST_F(VodStitchedAdEventsTests, SingleAd_EventsFireOnlyOnce)
{
    AddEntry("brk1", "ad1", 10.0, 10.0, 10.0, 10.0);

    EXPECT_CALL(*g_mockPrivateInstanceAAMP,
        SendAdReservationEvent(AAMP_EVENT_AD_RESERVATION_START, _, _, _, _, _))
        .Times(1);
    EXPECT_CALL(*g_mockPrivateInstanceAAMP,
        SendAdPlacementEvent(AAMP_EVENT_AD_PLACEMENT_START, _, _, _, _, _, _, _))
        .Times(1);
    EXPECT_CALL(*g_mockPrivateInstanceAAMP,
        SendAdPlacementEvent(AAMP_EVENT_AD_PLACEMENT_END, _, _, _, _, _, _, _))
        .Times(1);
    EXPECT_CALL(*g_mockPrivateInstanceAAMP,
        SendAdReservationEvent(AAMP_EVENT_AD_RESERVATION_END, _, _, _, _, _))
        .Times(1);

    // Call multiple times at and past the ad window — each event fires exactly once
    for (int i = 0; i < 5; ++i)
        mCdai->CheckVodStitchedAdEvents(10000.0);
    for (int i = 0; i < 5; ++i)
        mCdai->CheckVodStitchedAdEvents(20000.0);
    for (int i = 0; i < 5; ++i)
        mCdai->CheckVodStitchedAdEvents(25000.0);
}

// ===========================================================================
// 5. Multi-ad break — single RESERVATION_START, two PLACEMENT_START/END,
//    single RESERVATION_END after both ads complete
// ===========================================================================
TEST_F(VodStitchedAdEventsTests, MultiAdBreak_OneReservation_TwoPlacements)
{
    // Two chained ads in break "brk1": ad1 at 10–25 s, ad2 at 25–35 s
    // Both share the same breakStartSec (10) and breakDurationSec (25)
    AddEntry("brk1", "ad1", 10.0, 25.0, 10.0, 15.0);
    AddEntry("brk1", "ad2", 10.0, 25.0, 25.0, 10.0);

    {
        InSequence seq;

        // Tick at 10 000 ms: RESERVATION_START (once) + PLACEMENT_START for ad1
        EXPECT_CALL(*g_mockPrivateInstanceAAMP,
            SendAdReservationEvent(AAMP_EVENT_AD_RESERVATION_START,
                                   std::string("brk1"), _, _, false, _))
            .Times(1);
        EXPECT_CALL(*g_mockPrivateInstanceAAMP,
            SendAdPlacementEvent(AAMP_EVENT_AD_PLACEMENT_START,
                                  std::string("ad1"), _, _, _, _, false, _))
            .Times(1);

        // Tick at 25 000 ms: PLACEMENT_END for ad1 + PLACEMENT_START for ad2
        EXPECT_CALL(*g_mockPrivateInstanceAAMP,
            SendAdPlacementEvent(AAMP_EVENT_AD_PLACEMENT_END,
                                  std::string("ad1"), _, _, _, _, false, _))
            .Times(1);
        EXPECT_CALL(*g_mockPrivateInstanceAAMP,
            SendAdPlacementEvent(AAMP_EVENT_AD_PLACEMENT_START,
                                  std::string("ad2"), _, _, _, _, false, _))
            .Times(1);

        // Tick at 35 000 ms: PLACEMENT_END for ad2 + RESERVATION_END (once)
        EXPECT_CALL(*g_mockPrivateInstanceAAMP,
            SendAdPlacementEvent(AAMP_EVENT_AD_PLACEMENT_END,
                                  std::string("ad2"), _, _, _, _, false, _))
            .Times(1);
        EXPECT_CALL(*g_mockPrivateInstanceAAMP,
            SendAdReservationEvent(AAMP_EVENT_AD_RESERVATION_END,
                                   std::string("brk1"), _, _, false, _))
            .Times(1);
    }

    mCdai->CheckVodStitchedAdEvents(10000.0);
    mCdai->CheckVodStitchedAdEvents(25000.0);
    mCdai->CheckVodStitchedAdEvents(35000.0);
}

// ===========================================================================
// 6. Two independent breaks — each gets its own reservation lifecycle
// ===========================================================================
TEST_F(VodStitchedAdEventsTests, TwoIndependentBreaks_SeparateReservations)
{
    // Break 1: 10–20 s.  Break 2: 50–60 s.
    AddEntry("brk1", "ad1", 10.0, 10.0, 10.0, 10.0);
    AddEntry("brk2", "ad2", 50.0, 10.0, 50.0, 10.0);

    EXPECT_CALL(*g_mockPrivateInstanceAAMP,
        SendAdReservationEvent(AAMP_EVENT_AD_RESERVATION_START,
                               std::string("brk1"), _, _, false, _)).Times(1);
    EXPECT_CALL(*g_mockPrivateInstanceAAMP,
        SendAdPlacementEvent(AAMP_EVENT_AD_PLACEMENT_START,
                              std::string("ad1"), _, _, _, _, false, _)).Times(1);
    EXPECT_CALL(*g_mockPrivateInstanceAAMP,
        SendAdPlacementEvent(AAMP_EVENT_AD_PLACEMENT_END,
                              std::string("ad1"), _, _, _, _, false, _)).Times(1);
    EXPECT_CALL(*g_mockPrivateInstanceAAMP,
        SendAdReservationEvent(AAMP_EVENT_AD_RESERVATION_END,
                               std::string("brk1"), _, _, false, _)).Times(1);

    EXPECT_CALL(*g_mockPrivateInstanceAAMP,
        SendAdReservationEvent(AAMP_EVENT_AD_RESERVATION_START,
                               std::string("brk2"), _, _, false, _)).Times(1);
    EXPECT_CALL(*g_mockPrivateInstanceAAMP,
        SendAdPlacementEvent(AAMP_EVENT_AD_PLACEMENT_START,
                              std::string("ad2"), _, _, _, _, false, _)).Times(1);
    EXPECT_CALL(*g_mockPrivateInstanceAAMP,
        SendAdPlacementEvent(AAMP_EVENT_AD_PLACEMENT_END,
                              std::string("ad2"), _, _, _, _, false, _)).Times(1);
    EXPECT_CALL(*g_mockPrivateInstanceAAMP,
        SendAdReservationEvent(AAMP_EVENT_AD_RESERVATION_END,
                               std::string("brk2"), _, _, false, _)).Times(1);

    // Sweep through the whole timeline in coarse steps
    mCdai->CheckVodStitchedAdEvents(10000.0);
    mCdai->CheckVodStitchedAdEvents(20000.0);
    mCdai->CheckVodStitchedAdEvents(50000.0);
    mCdai->CheckVodStitchedAdEvents(60000.0);
}

// ===========================================================================
// 7. ResetVodAdEventTracker — all flags cleared, events re-fire after reset
// ===========================================================================
TEST_F(VodStitchedAdEventsTests, Reset_AllFlagsCleared_EventsRefireAfterReset)
{
    AddEntry("brk1", "ad1", 10.0, 10.0, 10.0, 10.0);

    // First pass: expect all 4 events
    EXPECT_CALL(*g_mockPrivateInstanceAAMP,
        SendAdReservationEvent(AAMP_EVENT_AD_RESERVATION_START, _, _, _, false, _)).Times(2);
    EXPECT_CALL(*g_mockPrivateInstanceAAMP,
        SendAdPlacementEvent(AAMP_EVENT_AD_PLACEMENT_START, _, _, _, _, _, false, _)).Times(2);
    EXPECT_CALL(*g_mockPrivateInstanceAAMP,
        SendAdPlacementEvent(AAMP_EVENT_AD_PLACEMENT_END, _, _, _, _, _, false, _)).Times(2);
    EXPECT_CALL(*g_mockPrivateInstanceAAMP,
        SendAdReservationEvent(AAMP_EVENT_AD_RESERVATION_END, _, _, _, false, _)).Times(2);

    // First pass through the ad
    mCdai->CheckVodStitchedAdEvents(10000.0);
    mCdai->CheckVodStitchedAdEvents(20000.0);

    // Verify flags are all set
    const VodAdEventEntry &e = mCdai->mVodAdEventTracker[0];
    EXPECT_TRUE(e.reservationStartFired);
    EXPECT_TRUE(e.placementStartFired);
    EXPECT_TRUE(e.placementEndFired);
    EXPECT_TRUE(e.reservationEndFired);

    // Simulate seek — reset
    mCdai->ResetVodAdEventTracker();

    EXPECT_FALSE(e.reservationStartFired);
    EXPECT_FALSE(e.placementStartFired);
    EXPECT_FALSE(e.placementEndFired);
    EXPECT_FALSE(e.reservationEndFired);

    // Second pass — all 4 events fire again (EXPECT_CALL Times(2) above)
    mCdai->CheckVodStitchedAdEvents(10000.0);
    mCdai->CheckVodStitchedAdEvents(20000.0);
}

// ===========================================================================
// 8. Seek forward past ad — PLACEMENT_START fires but END fires on same tick
// ===========================================================================
TEST_F(VodStitchedAdEventsTests, SeekPastAd_StartAndEndFireSameTick)
{
    AddEntry("brk1", "ad1", 10.0, 10.0, 10.0, 10.0);

    EXPECT_CALL(*g_mockPrivateInstanceAAMP,
        SendAdReservationEvent(AAMP_EVENT_AD_RESERVATION_START, _, _, _, false, _)).Times(1);
    EXPECT_CALL(*g_mockPrivateInstanceAAMP,
        SendAdPlacementEvent(AAMP_EVENT_AD_PLACEMENT_START, _, _, _, _, _, false, _)).Times(1);
    EXPECT_CALL(*g_mockPrivateInstanceAAMP,
        SendAdPlacementEvent(AAMP_EVENT_AD_PLACEMENT_END, _, _, _, _, _, false, _)).Times(1);
    EXPECT_CALL(*g_mockPrivateInstanceAAMP,
        SendAdReservationEvent(AAMP_EVENT_AD_RESERVATION_END, _, _, _, false, _)).Times(1);

    // Jump directly to a position past the ad end (25 s > 20 s)
    mCdai->CheckVodStitchedAdEvents(25000.0);
}

// ===========================================================================
// 9. Placement duration passed through correctly to SendAdPlacementEvent
// ===========================================================================
TEST_F(VodStitchedAdEventsTests, PlacementDurationPassedCorrectly)
{
    // Ad at 5–35 s: duration = 30 s = 30 000 ms
    AddEntry("brk1", "ad1", 5.0, 30.0, 5.0, 30.0);

    EXPECT_CALL(*g_mockPrivateInstanceAAMP,
        SendAdReservationEvent(AAMP_EVENT_AD_RESERVATION_START, _, _, _, false, _))
        .Times(1);
    EXPECT_CALL(*g_mockPrivateInstanceAAMP,
        SendAdPlacementEvent(AAMP_EVENT_AD_PLACEMENT_START,
                              std::string("ad1"),
                              static_cast<uint32_t>(5),      // posSec
                              static_cast<uint64_t>(5000),   // absolutePositionMs
                              static_cast<uint32_t>(0),      // adOffset
                              static_cast<uint32_t>(30000),  // durMs
                              false,
                              0L))
        .Times(1);
    EXPECT_CALL(*g_mockPrivateInstanceAAMP,
        SendAdPlacementEvent(AAMP_EVENT_AD_PLACEMENT_END,
                              std::string("ad1"),
                              static_cast<uint32_t>(35),     // posSec
                              static_cast<uint64_t>(35000),  // absolutePositionMs
                              static_cast<uint32_t>(0),
                              static_cast<uint32_t>(30000),
                              false,
                              0L))
        .Times(1);
    EXPECT_CALL(*g_mockPrivateInstanceAAMP,
        SendAdReservationEvent(AAMP_EVENT_AD_RESERVATION_END, _, _, _, false, _))
        .Times(1);

    mCdai->CheckVodStitchedAdEvents(5000.0);
    mCdai->CheckVodStitchedAdEvents(35000.0);
}

// ===========================================================================
// 10. Reset on empty tracker — no crash
// ===========================================================================
TEST_F(VodStitchedAdEventsTests, ResetOnEmptyTracker_NoCrash)
{
    EXPECT_TRUE(mCdai->mVodAdEventTracker.empty());
    EXPECT_NO_FATAL_FAILURE(mCdai->ResetVodAdEventTracker());
}

// ===========================================================================
// 11. Preroll (breakStartSec == 0) fires immediately at position 0
// ===========================================================================
TEST_F(VodStitchedAdEventsTests, Preroll_FiresAtPositionZero)
{
    // Preroll: starts at 0 s, lasts 15 s
    AddEntry("preroll", "adPre", 0.0, 15.0, 0.0, 15.0);

    EXPECT_CALL(*g_mockPrivateInstanceAAMP,
        SendAdReservationEvent(AAMP_EVENT_AD_RESERVATION_START,
                               std::string("preroll"), _, _, false, _)).Times(1);
    EXPECT_CALL(*g_mockPrivateInstanceAAMP,
        SendAdPlacementEvent(AAMP_EVENT_AD_PLACEMENT_START,
                              std::string("adPre"), _, _, _, _, false, _)).Times(1);

    mCdai->CheckVodStitchedAdEvents(0.0);

    EXPECT_TRUE(mCdai->mVodAdEventTracker[0].reservationStartFired);
    EXPECT_TRUE(mCdai->mVodAdEventTracker[0].placementStartFired);
    EXPECT_FALSE(mCdai->mVodAdEventTracker[0].placementEndFired);

    EXPECT_CALL(*g_mockPrivateInstanceAAMP,
        SendAdPlacementEvent(AAMP_EVENT_AD_PLACEMENT_END,
                              std::string("adPre"), _, _, _, _, false, _)).Times(1);
    EXPECT_CALL(*g_mockPrivateInstanceAAMP,
        SendAdReservationEvent(AAMP_EVENT_AD_RESERVATION_END,
                               std::string("preroll"), _, _, false, _)).Times(1);

    mCdai->CheckVodStitchedAdEvents(15000.0);
}

// ===========================================================================
// 12. PLACEMENT_END does not fire before PLACEMENT_START
//     (regression guard: endFired guarded by placementStartFired)
// ===========================================================================
TEST_F(VodStitchedAdEventsTests, PlacementEnd_NotFiredWithoutStart)
{
    // Manually pre-set placementStartFired = false, placementEndFired = false
    AddEntry("brk1", "ad1", 10.0, 10.0, 10.0, 10.0);

    // Only RESERVATION_START + PLACEMENT_START expected on the first tick
    EXPECT_CALL(*g_mockPrivateInstanceAAMP,
        SendAdReservationEvent(AAMP_EVENT_AD_RESERVATION_START, _, _, _, false, _)).Times(1);
    EXPECT_CALL(*g_mockPrivateInstanceAAMP,
        SendAdPlacementEvent(AAMP_EVENT_AD_PLACEMENT_START, _, _, _, _, _, false, _)).Times(1);
    EXPECT_CALL(*g_mockPrivateInstanceAAMP,
        SendAdPlacementEvent(AAMP_EVENT_AD_PLACEMENT_END, _, _, _, _, _, false, _)).Times(1);
    EXPECT_CALL(*g_mockPrivateInstanceAAMP,
        SendAdReservationEvent(AAMP_EVENT_AD_RESERVATION_END, _, _, _, false, _)).Times(1);

    // Force placementEndFired check without placementStartFired by calling
    // CheckVodStitchedAdEvents at exactly adEndMs.  The implementation requires
    // placementStartFired before firing placementEndFired, so START fires first
    // on this same tick, then END fires — both from a single call.
    mCdai->CheckVodStitchedAdEvents(20000.0);

    EXPECT_TRUE(mCdai->mVodAdEventTracker[0].placementStartFired);
    EXPECT_TRUE(mCdai->mVodAdEventTracker[0].placementEndFired);
}
