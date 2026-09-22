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
 * @file SetRateInternalIframeGuardTests.cpp
 * @brief Regression tests for the no-iframe-track guard in SetRateInternal().
 *
 * VPAAMP-1139: SetRateInternal() was blocking trickplay on VOD DASH streams
 * that have no dedicated iframe AdaptationSet even when
 * synthesizeIframeForVOD=true in aamp.cfg.  The guard must allow trickplay
 * through whenever IsVODIframeSynthesisEnabled() returns true (config flag
 * set AND stream is VOD i.e. !live AND !localTSB).
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "main_aamp.h"
#include "priv_aamp.h"
#include "AampConfig.h"
#include "AampDefine.h"
#include "MockAampConfig.h"
#include "MockPrivateInstanceAAMP.h"
#include "MockStreamAbstractionAAMP.h"

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;

// ---------------------------------------------------------------------------
// Fixture
// ---------------------------------------------------------------------------

/**
 * @class SetRateInternalIframeGuardTests
 * @brief Tests for the no-iframe-track early-return guard in SetRateInternal().
 *
 * All tests exercise a VOD (non-live, non-TSB) stream with no iframe
 * AdaptationSet (mIsIframeTrackPresent = false) and a trickplay rate of 4x.
 * The only variable is whether synthesizeIframeForVOD is enabled.
 *
 * Oracles:
 *  - Primary   : aamp->rate after the call.
 *    Guard fires → returns early → rate stays at AAMP_NORMAL_PLAY_RATE (1.0).
 *    Guard bypassed → trickplay path runs → rate updated to 4.0.
 *  - Secondary : NotifySpeedChanged(AAMP_NORMAL_PLAY_RATE, _) call count.
 *    The guard calls NotifySpeedChanged(1.0) before returning; the trickplay
 *    path calls it with the new rate (4.0), never with 1.0.
 */
class SetRateInternalIframeGuardTests : public ::testing::Test
{
protected:
    /**
     * @brief Exposes SetRateInternal() and the internal PrivateInstanceAAMP
     *        pointer without breaking encapsulation in production code.
     */
    class TestablePIA : public PlayerInstanceAAMP
    {
    public:
        TestablePIA() : PlayerInstanceAAMP() {}

        void SetRate_Internal(float rate, int overshootcorrection)
        {
            SetRateInternal(rate, overshootcorrection);
        }

        PrivateInstanceAAMP *GetPrivAamp() { return aamp; }
    };

    TestablePIA         *mPlayer{};
    PrivateInstanceAAMP *mPriv{};   ///< Non-owning; lifetime owned by mPlayer.

    void SetUp() override
    {
        if (gpGlobalConfig == nullptr)
        {
            gpGlobalConfig = new AampConfig();
        }

        g_mockAampConfig          = std::make_shared<NiceMock<MockAampConfig>>();
        g_mockPrivateInstanceAAMP = std::make_shared<NiceMock<MockPrivateInstanceAAMP>>();

        mPlayer = new TestablePIA();
        mPriv   = mPlayer->GetPrivAamp();

        g_mockStreamAbstractionAAMP =
            std::make_shared<NiceMock<MockStreamAbstractionAAMP>>(mPriv);
        mPriv->mpStreamAbstractionAAMP = g_mockStreamAbstractionAAMP.get();

        // ---- Safe defaults (ON_CALL) ----------------------------------------
        // All IsConfigSet queries return false unless overridden in the test.
        // This prevents "unmatched expectation" failures when the component
        // queries config flags other than the one under test (e.g.
        // eAAMPConfig_RepairIframes, eAAMPConfig_EnableGstPositionQuery).
        ON_CALL(*g_mockAampConfig, IsConfigSet(_)).WillByDefault(Return(false));

        // All NotifySpeedChanged calls are silently consumed unless a test
        // adds a stricter EXPECT_CALL for a specific argument value.
        ON_CALL(*g_mockPrivateInstanceAAMP, NotifySpeedChanged(_, _))
            .WillByDefault(Return());

        // ---- Common stream preconditions ------------------------------------
        // Normal play (rate=1.0), pipeline running, no iframe track present.
        // Stream is VOD: mIsLive and mLocalAAMPTsb are protected members
        // initialised to false in FakePrivateInstanceAAMP — that is the VOD
        // condition IsVODIframeSynthesisEnabled() requires; no override needed.
        mPriv->mbUsingExternalPlayer = false;
        mPriv->rate                  = AAMP_NORMAL_PLAY_RATE;
        mPriv->mSinkPaused           = false;
        mPriv->mIsIframeTrackPresent = false;
    }

    void TearDown() override
    {
        mPriv = nullptr;   ///< Non-owning; deleted via mPlayer below.

        delete mPlayer;
        mPlayer = nullptr;

        g_mockStreamAbstractionAAMP.reset();
        g_mockPrivateInstanceAAMP.reset();
        g_mockAampConfig.reset();

        delete gpGlobalConfig;
        gpGlobalConfig = nullptr;
    }
};

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

/**
 * @test SetRateInternal_NoIframeTrack_SynthesisDisabled_GuardBlocksTrickplay
 * @brief When no iframe AdaptationSet is present AND synthesizeIframeForVOD is
 *        disabled, the guard must reject trickplay by calling
 *        NotifySpeedChanged(AAMP_NORMAL_PLAY_RATE) and returning early without
 *        updating aamp->rate.
 *
 * This is the pre-existing behaviour that must be preserved for streams that
 * genuinely have no trickplay support.
 */
TEST_F(SetRateInternalIframeGuardTests,
    SetRateInternal_NoIframeTrack_SynthesisDisabled_GuardBlocksTrickplay)
{
    // Synthesis disabled (default ON_CALL already returns false for all flags).
    // Verify the guard fires: NotifySpeedChanged(1.0) called exactly once.
    EXPECT_CALL(*g_mockPrivateInstanceAAMP,
                NotifySpeedChanged(AAMP_NORMAL_PLAY_RATE, _))
        .Times(1);

    mPlayer->SetRate_Internal(4.0f, 0);

    // Guard returned early → aamp->rate was never updated to the requested rate.
    EXPECT_FLOAT_EQ(mPriv->rate, AAMP_NORMAL_PLAY_RATE);
}

/**
 * @test SetRateInternal_NoIframeTrack_SynthesisEnabled_GuardAllowsTrickplay
 * @brief When no iframe AdaptationSet is present BUT synthesizeIframeForVOD is
 *        enabled on a VOD stream, the guard must NOT reject trickplay.
 *
 * Regression test for VPAAMP-1139: before the fix the early-return guard
 * checked only mIsIframeTrackPresent and always blocked trickplay on streams
 * without a dedicated iframe track, ignoring IsVODIframeSynthesisEnabled().
 */
TEST_F(SetRateInternalIframeGuardTests,
    SetRateInternal_NoIframeTrack_SynthesisEnabled_GuardAllowsTrickplay)
{
    // Synthesis enabled: config flag set, stream is VOD (mIsLive=false,
    // mLocalAAMPTsb=false) → IsVODIframeSynthesisEnabled() returns true.
    ON_CALL(*g_mockAampConfig,
            IsConfigSet(eAAMPConfig_SynthesizeIframeForVOD))
        .WillByDefault(Return(true));

    // No EXPECT_CALL on NotifySpeedChanged here.  Adding one would cause
    // NiceMock to report a failure when the trickplay path calls
    // NotifySpeedChanged(4.0, ...) and the matcher for 1.0 does not match.
    // The aamp->rate oracle below is sufficient to prove the guard was
    // bypassed without any EXPECT_CALL noise.

    mPlayer->SetRate_Internal(4.0f, 0);

    // Primary oracle: guard bypassed → trickplay path ran → aamp->rate=4.0.
    // If the guard had fired instead, aamp->rate would still be 1.0.
    EXPECT_FLOAT_EQ(mPriv->rate, 4.0f);
}
