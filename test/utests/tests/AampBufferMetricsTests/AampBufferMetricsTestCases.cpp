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
 * @file AampBufferMetricsTestCases.cpp
 * @brief Unit tests for AampBufferMetrics.
 *
 * ## Coverage areas
 *  1. Default construction and initial state.
 *  2. EMA smoothing: cold-start priming, convergence, jitter attenuation.
 *  3. State thresholds: GOOD→LOW, LOW→CRITICAL and reverse with hysteresis.
 *  4. Underflow: immediate CRITICAL regardless of current state.
 *  5. Reset hooks: seek, trickplay, stop.
 *  6. Listener: registration, deregistration, notification count & args.
 *  7. Config presets: StandardDASH vs LL-DASH thresholds.
 *  8. Unsupported media types: silently ignored.
 *  9. UpdateConfig: config replaced, alpha recomputed.
 * 10. ResetOnStop: clears listeners.
 * 11. Thread-safety smoke test: concurrent AddSample calls.
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "AampBufferMetrics.h"

#include <atomic>
#include <thread>
#include <vector>

using namespace testing;

// ---------------------------------------------------------------------------
// Mock listener
// ---------------------------------------------------------------------------

class MockBufferListener : public IBufferMetricsListener
{
public:
	MOCK_METHOD(void, OnBufferStateChanged,
	            (AampMediaType mediaType,
	             BufferState   oldState,
	             BufferState   newState,
	             double        smoothedSec),
	            (override));
};

// ---------------------------------------------------------------------------
// Test fixture
// ---------------------------------------------------------------------------

/**
 * @brief Fixture creating an AampBufferMetrics with well-known thresholds
 *        to make threshold arithmetic simple in tests.
 *
 * Configuration:
 *   criticalThreshold = 0.5 s
 *   lowThreshold      = 2.0 s
 *   hysteresis        = 0.3 s
 *   sampleInterval    = 0.25 s  (250 ms)
 *   tau               = 0.0001 s → alpha ≈ 1.0 (near-instantaneous EMA = raw)
 *
 * Setting tau extremely small makes the EMA effectively equal to the raw
 * value within one sample, so tests can reason about raw input/output directly
 * without needing to iterate the filter to convergence.
 */
class AampBufferMetricsTest : public Test
{
protected:
	/// Shared config: near-instantaneous EMA for deterministic testing.
	BufferMetricsConfig mCfg;

	void SetUp() override
	{
		mCfg.criticalThresholdSec = 0.5;
		mCfg.lowThresholdSec      = 2.0;
		mCfg.hysteresisSec        = 0.3;
		mCfg.sampleIntervalSec    = 0.25;
		// Near-zero tau → alpha ≈ 1.0 → EMA tracks raw value immediately.
		mCfg.tauSec               = 0.0001;
	}

	/// Create a fresh AampBufferMetrics with the fixture config.
	AampBufferMetrics Make() const { return AampBufferMetrics{mCfg}; }

	/**
	 * @brief Drive the EMA to convergence by feeding the same value
	 *        many times. With alpha ≈ 1 this requires only one call, but
	 *        the helper abstracts this for robustness across alpha values.
	 */
	static void DriveToLevel(AampBufferMetrics& bm,
	                         AampMediaType      mediaType,
	                         double             levelSec,
	                         int                samples = 50)
	{
		for (int i = 0; i < samples; ++i)
		{
			bm.AddSample(mediaType, levelSec);
		}
	}
};

// ===========================================================================
// 1. Construction & initial state
// ===========================================================================

/**
 * @test Initial state for video and audio tracks is GOOD.
 */
TEST_F(AampBufferMetricsTest, InitialState_IsGood)
{
	auto bm = Make();
	EXPECT_EQ(bm.GetState(eMEDIATYPE_VIDEO), BufferState::GOOD);
	EXPECT_EQ(bm.GetState(eMEDIATYPE_AUDIO), BufferState::GOOD);
}

/**
 * @test Initial smoothed level is 0.
 */
TEST_F(AampBufferMetricsTest, InitialSmoothedLevel_IsZero)
{
	auto bm = Make();
	EXPECT_DOUBLE_EQ(bm.GetSmoothedLevel(eMEDIATYPE_VIDEO), 0.0);
	EXPECT_DOUBLE_EQ(bm.GetSmoothedLevel(eMEDIATYPE_AUDIO), 0.0);
}

// ===========================================================================
// 2. EMA priming and smoothing
// ===========================================================================

/**
 * @test First sample primes EMA to the raw value (no cold-start ramp-in).
 */
TEST_F(AampBufferMetricsTest, EMA_FirstSample_PrimesImmediately)
{
	auto bm = Make();
	bm.AddSample(eMEDIATYPE_VIDEO, 5.0);
	// With alpha ≈ 1, EMA should be extremely close to 5.0.
	EXPECT_NEAR(bm.GetSmoothedLevel(eMEDIATYPE_VIDEO), 5.0, 1e-3);
}

/**
 * @test EMA with alpha=1 tracks the raw value exactly.
 */
TEST_F(AampBufferMetricsTest, EMA_AlphaOne_TracksRawInstantly)
{
	// Use tiny tau so that alpha → 1.
	auto bm = Make();
	bm.AddSample(eMEDIATYPE_VIDEO, 3.0);
	EXPECT_NEAR(bm.GetSmoothedLevel(eMEDIATYPE_VIDEO), 3.0, 0.01);
	bm.AddSample(eMEDIATYPE_VIDEO, 0.1);
	EXPECT_NEAR(bm.GetSmoothedLevel(eMEDIATYPE_VIDEO), 0.1, 0.01);
}

/**
 * @test With a real smoothing tau the EMA converges toward the raw value
 *       over many samples and does not jump immediately.
 */
TEST_F(AampBufferMetricsTest, EMA_RealTau_ConvergesGradually)
{
	BufferMetricsConfig cfg;
	cfg.criticalThresholdSec = 0.5;
	cfg.lowThresholdSec      = 2.0;
	cfg.hysteresisSec        = 0.3;
	cfg.sampleIntervalSec    = 0.25;
	cfg.tauSec               = 2.0; // Standard smoothing.
	AampBufferMetrics bm{cfg};

	// After one sample from cold start, EMA == raw (priming).
	bm.AddSample(eMEDIATYPE_VIDEO, 4.0);
	const double afterPrime = bm.GetSmoothedLevel(eMEDIATYPE_VIDEO);
	EXPECT_NEAR(afterPrime, 4.0, 0.01);

	// Suddenly drop to 0; EMA should not jump immediately to 0.
	bm.AddSample(eMEDIATYPE_VIDEO, 0.0);
	const double afterDrop = bm.GetSmoothedLevel(eMEDIATYPE_VIDEO);
	EXPECT_GT(afterDrop, 0.0); // EMA still > 0 (smoothed).
	EXPECT_LT(afterDrop, 4.0); // EMA has moved toward 0.
}

/**
 * @test EMA attenuates high-frequency jitter. A jittery input centred on
 *       3.0 s should yield a smoothed output within ±1.0 s of 3.0 s even
 *       for alpha ≈ 0.12 (default standard-DASH settings).
 */
TEST_F(AampBufferMetricsTest, EMA_AttenuatesJitter)
{
	BufferMetricsConfig cfg;
	cfg.criticalThresholdSec = 0.5;
	cfg.lowThresholdSec      = 2.0;
	cfg.hysteresisSec        = 0.3;
	cfg.sampleIntervalSec    = 0.25;
	cfg.tauSec               = 2.0;
	AampBufferMetrics bm{cfg};

	// Prime at mid-level.
	DriveToLevel(bm, eMEDIATYPE_VIDEO, 3.0, 40);

	// Now inject alternating high/low jitter around 3.0 s.
	for (int i = 0; i < 20; ++i)
	{
		bm.AddSample(eMEDIATYPE_VIDEO, (i % 2 == 0) ? 0.0 : 6.0);
	}

	// Smoothed output should remain tethered closer to 3.0 than raw extremes.
	const double smoothed = bm.GetSmoothedLevel(eMEDIATYPE_VIDEO);
	EXPECT_GT(smoothed, 1.0);
	EXPECT_LT(smoothed, 5.0);
}

/**
 * @test Negative raw sample is clamped to 0 (guards against bad inputs).
 */
TEST_F(AampBufferMetricsTest, AddSample_NegativeInputClamped)
{
	auto bm = Make();
	bm.AddSample(eMEDIATYPE_VIDEO, -1.0);
	EXPECT_GE(bm.GetSmoothedLevel(eMEDIATYPE_VIDEO), 0.0);
}

// ===========================================================================
// 3. State threshold transitions
// ===========================================================================

/**
 * @test GOOD state transitions to LOW when EMA drops below lowThreshold.
 */
TEST_F(AampBufferMetricsTest, Threshold_GoodToLow_WhenBelowLow)
{
	auto bm = Make();
	// Prime at a healthy level.
	DriveToLevel(bm, eMEDIATYPE_VIDEO, 5.0);
	EXPECT_EQ(bm.GetState(eMEDIATYPE_VIDEO), BufferState::GOOD);

	// Drop just below the low threshold (2.0 s).
	DriveToLevel(bm, eMEDIATYPE_VIDEO, 1.8);
	EXPECT_EQ(bm.GetState(eMEDIATYPE_VIDEO), BufferState::LOW);
}

/**
 * @test LOW state transitions to CRITICAL when EMA drops below criticalThreshold.
 */
TEST_F(AampBufferMetricsTest, Threshold_LowToCritical_WhenBelowCritical)
{
	auto bm = Make();
	DriveToLevel(bm, eMEDIATYPE_VIDEO, 5.0);
	DriveToLevel(bm, eMEDIATYPE_VIDEO, 1.0); // Goes LOW first.
	EXPECT_EQ(bm.GetState(eMEDIATYPE_VIDEO), BufferState::LOW);

	DriveToLevel(bm, eMEDIATYPE_VIDEO, 0.3); // Below critical threshold.
	EXPECT_EQ(bm.GetState(eMEDIATYPE_VIDEO), BufferState::CRITICAL);
}

/**
 * @test GOOD→CRITICAL skips LOW when EMA drops in one step far below both
 *       thresholds.
 */
TEST_F(AampBufferMetricsTest, Threshold_GoodToDirectCritical_OnSuddenDrop)
{
	auto bm = Make();
	DriveToLevel(bm, eMEDIATYPE_VIDEO, 5.0);
	DriveToLevel(bm, eMEDIATYPE_VIDEO, 0.1); // Below critical.
	EXPECT_EQ(bm.GetState(eMEDIATYPE_VIDEO), BufferState::CRITICAL);
}

/**
 * @test Hysteresis prevents premature upward transition from CRITICAL.
 *       EMA at (criticalThreshold + hysteresis / 2) should stay CRITICAL.
 */
TEST_F(AampBufferMetricsTest, Hysteresis_PreventsCriticalToLowTooEarly)
{
	auto bm = Make();
	// Force CRITICAL.
	DriveToLevel(bm, eMEDIATYPE_VIDEO, 0.1);
	ASSERT_EQ(bm.GetState(eMEDIATYPE_VIDEO), BufferState::CRITICAL);

	// Rise to just under (criticalThreshold + hysteresis) = 0.5 + 0.3 = 0.8 s.
	// Use 0.75 s — within the hysteresis band.
	DriveToLevel(bm, eMEDIATYPE_VIDEO, 0.75);
	EXPECT_EQ(bm.GetState(eMEDIATYPE_VIDEO), BufferState::CRITICAL);
}

/**
 * @test Upward transition from CRITICAL to LOW fires once EMA clears
 *       (criticalThreshold + hysteresis).
 */
TEST_F(AampBufferMetricsTest, Hysteresis_AllowsCriticalToLow_AboveBand)
{
	auto bm = Make();
	DriveToLevel(bm, eMEDIATYPE_VIDEO, 0.1);
	ASSERT_EQ(bm.GetState(eMEDIATYPE_VIDEO), BufferState::CRITICAL);

	// Rise to 0.85 s > (0.5 + 0.3 = 0.8 s).
	DriveToLevel(bm, eMEDIATYPE_VIDEO, 0.85);
	EXPECT_EQ(bm.GetState(eMEDIATYPE_VIDEO), BufferState::LOW);
}

/**
 * @test Upward transition from LOW to GOOD requires EMA > lowThreshold + hysteresis.
 */
TEST_F(AampBufferMetricsTest, Hysteresis_LowToGood_RequiresClearance)
{
	auto bm = Make();
	DriveToLevel(bm, eMEDIATYPE_VIDEO, 5.0);   // GOOD
	DriveToLevel(bm, eMEDIATYPE_VIDEO, 1.5);   // LOW
	ASSERT_EQ(bm.GetState(eMEDIATYPE_VIDEO), BufferState::LOW);

	// Rise to just below (lowThreshold + hysteresis) = 2.0 + 0.3 = 2.3 s.
	DriveToLevel(bm, eMEDIATYPE_VIDEO, 2.2);
	EXPECT_EQ(bm.GetState(eMEDIATYPE_VIDEO), BufferState::LOW);

	// Rise above 2.3 s → GOOD.
	DriveToLevel(bm, eMEDIATYPE_VIDEO, 2.4);
	EXPECT_EQ(bm.GetState(eMEDIATYPE_VIDEO), BufferState::GOOD);
}

/**
 * @test Downward transition from GOOD to LOW fires immediately at lowThreshold
 *       (no downward hysteresis).
 */
TEST_F(AampBufferMetricsTest, Threshold_GoodToLow_NoDownwardHysteresis)
{
	auto bm = Make();
	DriveToLevel(bm, eMEDIATYPE_VIDEO, 5.0);

	// Exactly at lowThreshold - epsilon → should be LOW.
	DriveToLevel(bm, eMEDIATYPE_VIDEO, 1.99);
	EXPECT_EQ(bm.GetState(eMEDIATYPE_VIDEO), BufferState::LOW);
}

/**
 * @test Audio and video tracks are tracked independently.
 */
TEST_F(AampBufferMetricsTest, Threshold_VideoAndAudioIndependent)
{
	auto bm = Make();
	DriveToLevel(bm, eMEDIATYPE_VIDEO, 5.0);
	DriveToLevel(bm, eMEDIATYPE_AUDIO, 5.0);

	// Drop only video.
	DriveToLevel(bm, eMEDIATYPE_VIDEO, 0.1);

	EXPECT_EQ(bm.GetState(eMEDIATYPE_VIDEO), BufferState::CRITICAL);
	EXPECT_EQ(bm.GetState(eMEDIATYPE_AUDIO), BufferState::GOOD);
}

// ===========================================================================
// 4. NotifyUnderflow
// ===========================================================================

/**
 * @test NotifyUnderflow immediately forces CRITICAL state.
 */
TEST_F(AampBufferMetricsTest, NotifyUnderflow_ForcesCritical)
{
	auto bm = Make();
	DriveToLevel(bm, eMEDIATYPE_VIDEO, 10.0);
	ASSERT_EQ(bm.GetState(eMEDIATYPE_VIDEO), BufferState::GOOD);

	bm.NotifyUnderflow(eMEDIATYPE_VIDEO);

	EXPECT_EQ(bm.GetState(eMEDIATYPE_VIDEO), BufferState::CRITICAL);
	EXPECT_DOUBLE_EQ(bm.GetSmoothedLevel(eMEDIATYPE_VIDEO), 0.0);
}

/**
 * @test NotifyUnderflow when already CRITICAL does not fire listener again.
 */
TEST_F(AampBufferMetricsTest, NotifyUnderflow_AlreadyCritical_NoSpuriousCallback)
{
	auto bm = Make();
	DriveToLevel(bm, eMEDIATYPE_VIDEO, 0.1); // already CRITICAL

	MockBufferListener mockListener;
	bm.AddListener(&mockListener);

	// Expect zero calls because state is already CRITICAL.
	EXPECT_CALL(mockListener, OnBufferStateChanged(_, _, _, _)).Times(0);

	bm.NotifyUnderflow(eMEDIATYPE_VIDEO);
}

/**
 * @test NotifyUnderflow fires listener with correct arguments.
 */
TEST_F(AampBufferMetricsTest, NotifyUnderflow_FiresListenerWithCorrectArgs)
{
	auto bm = Make();
	DriveToLevel(bm, eMEDIATYPE_VIDEO, 5.0);

	MockBufferListener mockListener;
	bm.AddListener(&mockListener);

	EXPECT_CALL(mockListener,
	            OnBufferStateChanged(eMEDIATYPE_VIDEO,
	                                 BufferState::GOOD,
	                                 BufferState::CRITICAL,
	                                 0.0))
	    .Times(1);

	bm.NotifyUnderflow(eMEDIATYPE_VIDEO);
}

// ===========================================================================
// 5. Reset hooks
// ===========================================================================

/**
 * @test ResetOnSeek clears EMA and resets state to GOOD.
 */
TEST_F(AampBufferMetricsTest, ResetOnSeek_ClearsEmaAndState)
{
	auto bm = Make();
	DriveToLevel(bm, eMEDIATYPE_VIDEO, 0.1); // Drive to CRITICAL.

	bm.ResetOnSeek();

	EXPECT_EQ(bm.GetState(eMEDIATYPE_VIDEO), BufferState::GOOD);
	EXPECT_DOUBLE_EQ(bm.GetSmoothedLevel(eMEDIATYPE_VIDEO), 0.0);
}

/**
 * @test After ResetOnSeek the EMA primes correctly on the first new sample.
 */
TEST_F(AampBufferMetricsTest, ResetOnSeek_NextSamplePrimesEMA)
{
	auto bm = Make();
	DriveToLevel(bm, eMEDIATYPE_VIDEO, 0.1);
	bm.ResetOnSeek();

	bm.AddSample(eMEDIATYPE_VIDEO, 4.0);
	EXPECT_NEAR(bm.GetSmoothedLevel(eMEDIATYPE_VIDEO), 4.0, 0.1);
}

/**
 * @test ResetOnTrickplay resets both tracks.
 */
TEST_F(AampBufferMetricsTest, ResetOnTrickplay_ResetsBothTracks)
{
	auto bm = Make();
	DriveToLevel(bm, eMEDIATYPE_VIDEO, 0.1);
	DriveToLevel(bm, eMEDIATYPE_AUDIO, 0.1);

	bm.ResetOnTrickplay();

	EXPECT_EQ(bm.GetState(eMEDIATYPE_VIDEO), BufferState::GOOD);
	EXPECT_EQ(bm.GetState(eMEDIATYPE_AUDIO), BufferState::GOOD);
}

/**
 * @test ResetOnStop clears state, EMA, and listeners.
 */
TEST_F(AampBufferMetricsTest, ResetOnStop_ClearsListeners)
{
	auto bm = Make();
	MockBufferListener mockListener;
	bm.AddListener(&mockListener);

	bm.ResetOnStop();

	// After stop, adding a sample that would normally fire a transition should
	// not invoke the listener because it was cleared.
	EXPECT_CALL(mockListener, OnBufferStateChanged(_, _, _, _)).Times(0);

	// Drive from GOOD (reset state) to LOW.
	DriveToLevel(bm, eMEDIATYPE_VIDEO, 1.0);
}

/**
 * @test ResetOnStop also resets video state to GOOD.
 */
TEST_F(AampBufferMetricsTest, ResetOnStop_ResetsState)
{
	auto bm = Make();
	DriveToLevel(bm, eMEDIATYPE_VIDEO, 0.1);

	bm.ResetOnStop();

	EXPECT_EQ(bm.GetState(eMEDIATYPE_VIDEO), BufferState::GOOD);
}

// ===========================================================================
// 6. Listener registration and callbacks
// ===========================================================================

/**
 * @test Listener is called exactly once on a state transition.
 */
TEST_F(AampBufferMetricsTest, Listener_CalledOnTransition)
{
	auto bm = Make();
	MockBufferListener mockListener;
	bm.AddListener(&mockListener);

	DriveToLevel(bm, eMEDIATYPE_VIDEO, 5.0); // Transition from init→GOOD (no change).

	// Exactly one call for GOOD→LOW.
	EXPECT_CALL(mockListener,
	            OnBufferStateChanged(eMEDIATYPE_VIDEO,
	                                 BufferState::GOOD,
	                                 BufferState::LOW, _))
	    .Times(1);

	DriveToLevel(bm, eMEDIATYPE_VIDEO, 1.5);
}

/**
 * @test Listener is NOT called when state does not change.
 */
TEST_F(AampBufferMetricsTest, Listener_NotCalledWhenStateUnchanged)
{
	auto bm = Make();
	MockBufferListener mockListener;
	bm.AddListener(&mockListener);

	EXPECT_CALL(mockListener, OnBufferStateChanged(_, _, _, _)).Times(0);

	// Keep pumping a healthy level — no state transition.
	DriveToLevel(bm, eMEDIATYPE_VIDEO, 5.0);
}

/**
 * @test Duplicate AddListener for same pointer is a no-op.
 */
TEST_F(AampBufferMetricsTest, Listener_DuplicateAddIsNoOp)
{
	auto bm = Make();
	MockBufferListener mockListener;
	bm.AddListener(&mockListener);
	bm.AddListener(&mockListener); // Should be ignored.

	// Transitioning to LOW should fire only once, not twice.
	EXPECT_CALL(mockListener,
	            OnBufferStateChanged(_, _, BufferState::LOW, _))
	    .Times(1);

	DriveToLevel(bm, eMEDIATYPE_VIDEO, 5.0);
	DriveToLevel(bm, eMEDIATYPE_VIDEO, 1.5);
}

/**
 * @test RemoveListener stops further callbacks.
 */
TEST_F(AampBufferMetricsTest, Listener_RemoveStopsCallbacks)
{
	auto bm = Make();
	MockBufferListener mockListener;
	bm.AddListener(&mockListener);
	bm.RemoveListener(&mockListener);

	EXPECT_CALL(mockListener, OnBufferStateChanged(_, _, _, _)).Times(0);

	DriveToLevel(bm, eMEDIATYPE_VIDEO, 5.0);
	DriveToLevel(bm, eMEDIATYPE_VIDEO, 1.5);
}

/**
 * @test AddListener with null pointer is a no-op (no crash).
 */
TEST_F(AampBufferMetricsTest, Listener_NullAddIsNoOp)
{
	auto bm = Make();
	EXPECT_NO_THROW(bm.AddListener(nullptr));
}

/**
 * @test Callback receives correct smoothedSec value at transition point.
 */
TEST_F(AampBufferMetricsTest, Listener_SmoothedSecPassedCorrectly)
{
	auto bm = Make();
	MockBufferListener mockListener;
	bm.AddListener(&mockListener);

	double capturedSmoothed{0.0};
	ON_CALL(mockListener, OnBufferStateChanged(_, _, _, _))
	    .WillByDefault([&capturedSmoothed](AampMediaType, BufferState, BufferState,
	                                       double smoothed) {
		    capturedSmoothed = smoothed;
	    });
	EXPECT_CALL(mockListener, OnBufferStateChanged(_, _, _, _)).Times(1);

	DriveToLevel(bm, eMEDIATYPE_VIDEO, 5.0);
	DriveToLevel(bm, eMEDIATYPE_VIDEO, 1.5);

	// Smoothed level should match what GetSmoothedLevel() reports.
	EXPECT_NEAR(capturedSmoothed,
	            bm.GetSmoothedLevel(eMEDIATYPE_VIDEO), 0.01);
}

// ===========================================================================
// 7. Config presets
// ===========================================================================

/**
 * @test StandardDASH preset has lower threshold (0.5 s critical vs 0.2 s LL-DASH).
 */
TEST_F(AampBufferMetricsTest, ConfigPreset_StandardDASH_CriticalThresholdHigherThanLLDASH)
{
	const auto stdCfg = BufferMetricsConfig::StandardDASH();
	const auto llCfg  = BufferMetricsConfig::LLDASH();
	EXPECT_GT(stdCfg.criticalThresholdSec, llCfg.criticalThresholdSec);
	EXPECT_GT(stdCfg.lowThresholdSec,      llCfg.lowThresholdSec);
}

/**
 * @test LL-DASH preset enters CRITICAL at a lower absolute level (0.2 s).
 */
TEST_F(AampBufferMetricsTest, ConfigPreset_LLDASH_CriticalAt0_2s)
{
	AampBufferMetrics bm{BufferMetricsConfig::LLDASH()};
	DriveToLevel(bm, eMEDIATYPE_VIDEO, 5.0);
	// 0.15 s < LL-DASH critical (0.2 s) → CRITICAL.
	DriveToLevel(bm, eMEDIATYPE_VIDEO, 0.15);
	EXPECT_EQ(bm.GetState(eMEDIATYPE_VIDEO), BufferState::CRITICAL);
}

/**
 * @test Standard DASH at 0.15 s is not yet CRITICAL (threshold is 0.5 s).
 *
 * Note: 0.15 s is below the standard-DASH critical threshold (0.5 s) so
 * it SHOULD be CRITICAL; this test instead verifies the preset is used.
 */
TEST_F(AampBufferMetricsTest, ConfigPreset_StandardDASH_CriticalAt0_5s)
{
	AampBufferMetrics bm{BufferMetricsConfig::StandardDASH()};
	DriveToLevel(bm, eMEDIATYPE_VIDEO, 5.0);
	// 0.4 s is below standard critical (0.5 s) → CRITICAL.
	DriveToLevel(bm, eMEDIATYPE_VIDEO, 0.4);
	EXPECT_EQ(bm.GetState(eMEDIATYPE_VIDEO), BufferState::CRITICAL);

	// 0.6 s is above critical but below lowThreshold (2.0 s) → LOW.
	DriveToLevel(bm, eMEDIATYPE_VIDEO, 0.6);
	// Need to clear hysteresis (0.5 + 0.3 = 0.8 s).
	DriveToLevel(bm, eMEDIATYPE_VIDEO, 0.85);
	EXPECT_EQ(bm.GetState(eMEDIATYPE_VIDEO), BufferState::LOW);
}

// ===========================================================================
// 8. Unsupported media types
// ===========================================================================

/**
 * @test AddSample on unsupported type does not crash and is silently ignored.
 */
TEST_F(AampBufferMetricsTest, UnsupportedMediaType_Ignored)
{
	auto bm = Make();
	EXPECT_NO_THROW(bm.AddSample(eMEDIATYPE_SUBTITLE, 3.0));
	EXPECT_EQ(bm.GetState(eMEDIATYPE_SUBTITLE), BufferState::CRITICAL);
	EXPECT_DOUBLE_EQ(bm.GetSmoothedLevel(eMEDIATYPE_SUBTITLE), 0.0);
}

/**
 * @test NotifyUnderflow on unsupported type does not crash.
 */
TEST_F(AampBufferMetricsTest, UnsupportedMediaType_NotifyUnderflowNoOp)
{
	auto bm = Make();
	EXPECT_NO_THROW(bm.NotifyUnderflow(eMEDIATYPE_SUBTITLE));
}

// ===========================================================================
// 9. UpdateConfig
// ===========================================================================

/**
 * @test UpdateConfig replaces thresholds; state classification uses new params.
 */
TEST_F(AampBufferMetricsTest, UpdateConfig_NewThresholdsApplied)
{
	auto bm = Make();
	DriveToLevel(bm, eMEDIATYPE_VIDEO, 5.0);

	// Switch to very tight thresholds (LL-DASH style).
	bm.UpdateConfig(BufferMetricsConfig::LLDASH());

	// 1.0 s is above LL-DASH low threshold (0.8 s) → state should be GOOD
	// after EMA stabilises.
	DriveToLevel(bm, eMEDIATYPE_VIDEO, 1.0);
	EXPECT_EQ(bm.GetState(eMEDIATYPE_VIDEO), BufferState::GOOD);
}

// ===========================================================================
// 10. Full transition cycle
// ===========================================================================

/**
 * @test Complete state cycle: GOOD → LOW → CRITICAL → LOW → GOOD.
 */
TEST_F(AampBufferMetricsTest, FullCycle_GoodLowCriticalLowGood)
{
	auto bm = Make();

	DriveToLevel(bm, eMEDIATYPE_VIDEO, 5.0);
	EXPECT_EQ(bm.GetState(eMEDIATYPE_VIDEO), BufferState::GOOD);

	DriveToLevel(bm, eMEDIATYPE_VIDEO, 1.5);
	EXPECT_EQ(bm.GetState(eMEDIATYPE_VIDEO), BufferState::LOW);

	DriveToLevel(bm, eMEDIATYPE_VIDEO, 0.2);
	EXPECT_EQ(bm.GetState(eMEDIATYPE_VIDEO), BufferState::CRITICAL);

	// Recover through critical hysteresis (> 0.8 s) but below low (2.0 s).
	DriveToLevel(bm, eMEDIATYPE_VIDEO, 0.9);
	EXPECT_EQ(bm.GetState(eMEDIATYPE_VIDEO), BufferState::LOW);

	// Recover through low hysteresis (> 2.3 s).
	DriveToLevel(bm, eMEDIATYPE_VIDEO, 2.5);
	EXPECT_EQ(bm.GetState(eMEDIATYPE_VIDEO), BufferState::GOOD);
}

// ===========================================================================
// 11. Thread-safety smoke test
// ===========================================================================

/**
 * @test Concurrent AddSample calls from multiple threads must not crash or
 *       corrupt state.
 *
 * This is a data-race smoke test, not a precise functional check.
 * It verifies that locking is in place and no segfault / TSAN error occurs.
 */
TEST_F(AampBufferMetricsTest, ThreadSafety_ConcurrentAddSample_NoRace)
{
	auto bm = Make();

	constexpr int kThreads  = 4;
	constexpr int kSamples  = 1000;

	std::atomic<bool> start{false};
	std::vector<std::thread> threads;
	threads.reserve(kThreads);

	for (int t = 0; t < kThreads; ++t)
	{
		threads.emplace_back([&bm, &start, t]() {
			while (!start.load()) {} // Spin until all threads are ready.
			for (int i = 0; i < kSamples; ++i)
			{
				const double level = (t % 2 == 0) ? 5.0 : 0.1;
				bm.AddSample(eMEDIATYPE_VIDEO, level);
				bm.AddSample(eMEDIATYPE_AUDIO, level);
			}
		});
	}

	start.store(true);
	for (auto& th : threads)
	{
		th.join();
	}

	// After concurrent writes the state should be valid (not undefined).
	const BufferState vs = bm.GetState(eMEDIATYPE_VIDEO);
	EXPECT_TRUE(vs == BufferState::GOOD ||
	            vs == BufferState::LOW  ||
	            vs == BufferState::CRITICAL);
}
