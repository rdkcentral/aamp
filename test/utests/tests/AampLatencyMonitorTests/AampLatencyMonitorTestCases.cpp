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
 * @file AampLatencyMonitorTestCases.cpp
 * @brief Unit tests for AampLatencyMonitor covering:
 *        - Threshold boundary validation and SetLatencyThresholds
 *        - Reset behaviour (ResetToNormalRate via Stop / EnableRateCorrection)
 *        - Rate-correction logic (speed-up, slow-down, return-to-normal)
 *        - State lifecycle (Start / Stop / IsRunning)
 *        - Adaptive latency threshold shifts driven by underflow events
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <atomic>
#include <chrono>
#include <thread>

#include "AampLatencyMonitor.h"
#include "priv_aamp.h"
#include "AampConfig.h"
#include "MockPrivateInstanceAAMP.h"
#include "MockAampStreamSinkManager.h"
#include "MockStreamSink.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::AtLeast;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::StrictMock;

// Required by the fake AAMP infrastructure.
AampConfig *gpGlobalConfig{nullptr};

// Convenience ms-unit aliases for default latency macros from AampDefine.h
// (originals are in seconds; tests work in milliseconds).
#define DEFAULT_MIN_LATENCY_MS    (DEFAULT_MIN_LOW_LATENCY    * 1000.0)
#define DEFAULT_TARGET_LATENCY_MS (DEFAULT_TARGET_LOW_LATENCY * 1000.0)
#define DEFAULT_MAX_LATENCY_MS    (DEFAULT_MAX_LOW_LATENCY    * 1000.0)

// ---------------------------------------------------------------------------
// Helper: build a LatencyConfig with very short poll intervals so worker
// tests complete quickly in CI.
// ---------------------------------------------------------------------------
static LatencyConfig MakeFastConfig(
	double normalRate  = DEFAULT_NORMAL_RATE_CORRECTION_SPEED, //1.00
	double minRate     = DEFAULT_MIN_RATE_CORRECTION_SPEED, //0.97
	double maxRate     = DEFAULT_MAX_RATE_CORRECTION_SPEED, //1.03
	double minLatMs    = DEFAULT_MIN_LATENCY_MS, //3000 ms
	double targetLatMs = DEFAULT_TARGET_LATENCY_MS, //6000 ms
	double maxLatMs    = DEFAULT_MAX_LATENCY_MS, //9000 ms
    double bufToEnable = DEFAULT_BUFFER_LEVEL_TO_ENABLE_LATENCY_SEC * 1000, // 0.0s
	double rebufStepMs    = 0.0,   // latency increment per rebuffering event
	double rebufMaxIncrMs = 0.0)   // max total accumulated increment (0 = uncapped)
{
	// monitorDelayMs = 0, monitorIntervalMs = 5 ms — fast for tests.
	return LatencyConfig{normalRate, minRate, maxRate,
		minLatMs, targetLatMs, maxLatMs,
		0, 5, bufToEnable, rebufStepMs, rebufMaxIncrMs};
}

// ---------------------------------------------------------------------------
// Fixture shared across all test groups.
// ---------------------------------------------------------------------------
class AampLatencyMonitorTest : public ::testing::Test
{
protected:
	PrivateInstanceAAMP*         mAamp     {nullptr};
	NiceMock<MockPrivateInstanceAAMP>*  mMockAamp {nullptr};
	NiceMock<MockAampStreamSinkManager>* mMockSinkMgr {nullptr};
	NiceMock<MockStreamSink>*    mMockSink {nullptr};
	AampConfig*                  mConfig   {nullptr};
	AampLatencyMonitor*          mMonitor  {nullptr};

	void SetUp() override
	{
		mConfig      = new AampConfig();
		mAamp        = new PrivateInstanceAAMP(mConfig);
		mMockAamp    = new NiceMock<MockPrivateInstanceAAMP>();
		mMockSinkMgr = new NiceMock<MockAampStreamSinkManager>();
		mMockSink    = new NiceMock<MockStreamSink>();

		g_mockPrivateInstanceAAMP  = mMockAamp;
		g_mockAampStreamSinkManager = mMockSinkMgr;

		// Default safe stubs — overridden per test as required.
		ON_CALL(*mMockAamp, GetState()).WillByDefault(Return(eSTATE_PLAYING));
		ON_CALL(*mMockAamp, IsAdPlaying()).WillByDefault(Return(false));
		ON_CALL(*mMockAamp, GetCurrentLatencyMs()).WillByDefault(Return(static_cast<long>(DEFAULT_TARGET_LATENCY_MS)));
		ON_CALL(*mMockAamp, GetBufferedDurationSecs()).WillByDefault(Return(5.0));
		ON_CALL(*mMockAamp, UpdateVideoEndMetrics(_)).WillByDefault(Return());
		ON_CALL(*mMockSinkMgr, GetStreamSink(_)).WillByDefault(Return(mMockSink));
		ON_CALL(*mMockSink, SetPlayBackRate(_)).WillByDefault(Return(true));

		mMonitor = new AampLatencyMonitor(mAamp);
	}

	void TearDown() override
	{
		// Stop first to join any worker thread before destroying objects.
		if (mMonitor)
		{
			mMonitor->Stop();
			delete mMonitor;
			mMonitor = nullptr;
		}

		g_mockPrivateInstanceAAMP   = nullptr;
		g_mockAampStreamSinkManager = nullptr;

		delete mMockSink;    mMockSink    = nullptr;
		delete mMockSinkMgr; mMockSinkMgr = nullptr;
		delete mMockAamp;    mMockAamp    = nullptr;
		delete mAamp;        mAamp        = nullptr;
		delete mConfig;      mConfig      = nullptr;
	}

	/// @brief Block until the monitor worker is in kRunning state (up to
	///        @p maxWaitMs milliseconds).
	bool WaitForRunning(int maxWaitMs = 500)
	{
		auto deadline = std::chrono::steady_clock::now()
			+ std::chrono::milliseconds(maxWaitMs);
		while (!mMonitor->IsRunning())
		{
			if (std::chrono::steady_clock::now() >= deadline)
				return false;
			std::this_thread::sleep_for(std::chrono::milliseconds(2));
		}
		return true;
	}

	/// @brief Wait until GetCurrentRate() equals @p expected (up to @p maxWaitMs ms).
	bool WaitForRate(double expected, int maxWaitMs = 500)
	{
		auto deadline = std::chrono::steady_clock::now()
			+ std::chrono::milliseconds(maxWaitMs);
		while (mMonitor->GetCurrentRate() != expected)
		{
			if (std::chrono::steady_clock::now() >= deadline)
				return false;
			std::this_thread::sleep_for(std::chrono::milliseconds(2));
		}
		return true;
	}
};


// ===========================================================================
// 2. State lifecycle — Start / Stop / IsRunning
// ===========================================================================

/**
 * @test State_InitiallyIdle
 * @brief A freshly constructed monitor must not be running.
 */
TEST_F(AampLatencyMonitorTest, State_InitiallyIdle)
{
	EXPECT_FALSE(mMonitor->IsRunning());
}

/**
 * @test State_StartTransitionsToRunning
 * @brief Start() must cause the worker thread to reach kRunning.
 */
TEST_F(AampLatencyMonitorTest, State_StartTransitionsToRunning)
{
	mMonitor->Start(MakeFastConfig());
	EXPECT_TRUE(WaitForRunning());
}

/**
 * @test State_StopTransitionsToIdle
 * @brief Stop() must return the monitor to kIdle.
 */
TEST_F(AampLatencyMonitorTest, State_StopTransitionsToIdle)
{
	mMonitor->Start(MakeFastConfig());
	ASSERT_TRUE(WaitForRunning());
	mMonitor->Stop();
	EXPECT_FALSE(mMonitor->IsRunning());
}

/**
 * @test State_DoubleStartIsNoOp
 * @brief Calling Start() a second time while already running must be a no-op.
 */
TEST_F(AampLatencyMonitorTest, State_DoubleStartIsNoOp)
{
	mMonitor->Start(MakeFastConfig());
	ASSERT_TRUE(WaitForRunning());
	// Second call should not throw or crash.
	mMonitor->Start(MakeFastConfig());
	EXPECT_TRUE(mMonitor->IsRunning());
}

/**
 * @test State_DoubleStopIsNoOp
 * @brief Calling Stop() when already idle must be a no-op.
 */
TEST_F(AampLatencyMonitorTest, State_DoubleStopIsNoOp)
{
	mMonitor->Stop();   // called while still idle
	EXPECT_FALSE(mMonitor->IsRunning());
}

/**
 * @test State_StartWithNullAamp_DoesNotStart
 * @brief Constructing with nullptr and calling Start() must not start the thread.
 */
TEST_F(AampLatencyMonitorTest, State_StartWithNullAamp_DoesNotStart)
{
	AampLatencyMonitor nullMonitor(nullptr);
	nullMonitor.Start(MakeFastConfig());
	EXPECT_FALSE(nullMonitor.IsRunning());
	// nullMonitor destructor is safe (Stop() on idle is a no-op).
}

/**
 * @test State_InitialRateIsNormal
 * @brief Before Start(), GetCurrentRate() should return 1.0 (constructed default).
 */
TEST_F(AampLatencyMonitorTest, State_InitialRateIsNormal)
{
	EXPECT_DOUBLE_EQ(mMonitor->GetCurrentRate(), DEFAULT_NORMAL_RATE_CORRECTION_SPEED);
}

/**
 * @test State_StartSetsRateFromConfig
 * @brief After Start(), the initial rate should equal config.normalPlaybackRate.
 */
TEST_F(AampLatencyMonitorTest, State_StartSetsRateFromConfig)
{
	mMonitor->Start(MakeFastConfig(2.0));
	ASSERT_TRUE(WaitForRunning());
	EXPECT_DOUBLE_EQ(mMonitor->GetCurrentRate(), 2.0);
}

// ===========================================================================
// 3. Reset behaviour — Stop resets rate, EnableRateCorrection(false) resets
// ===========================================================================

/**
 * @test Reset_StopResetsRateToNormal
 * @brief After Stop() the rate stored in the monitor equals normalPlaybackRate.
 */
TEST_F(AampLatencyMonitorTest, Reset_StopResetsRateToNormal)
{
	// Use latency > maxLatencyMs so the worker drives rate to maxRate.
	ON_CALL(*mMockAamp, GetCurrentLatencyMs()).WillByDefault(Return(15000L)); // > 9000 ms max
	ON_CALL(*mMockAamp, GetBufferedDurationSecs()).WillByDefault(Return(5.0));
	ON_CALL(*mMockSink, SetPlayBackRate(_)).WillByDefault(Return(true));

	mMonitor->Start(MakeFastConfig());
	ASSERT_TRUE(WaitForRunning());

	// Wait for the worker to apply the fast rate.
	ASSERT_TRUE(WaitForRate(DEFAULT_MAX_RATE_CORRECTION_SPEED, 500));

	// Stop() must reset the rate via the sink.
	EXPECT_CALL(*mMockSinkMgr, GetStreamSink(_))
		.WillRepeatedly(Return(mMockSink));
	EXPECT_CALL(*mMockSink, SetPlayBackRate(DEFAULT_NORMAL_RATE_CORRECTION_SPEED))
		.Times(AtLeast(1))
		.WillRepeatedly(Return(true));

	mMonitor->Stop();
	EXPECT_DOUBLE_EQ(mMonitor->GetCurrentRate(), DEFAULT_NORMAL_RATE_CORRECTION_SPEED);
}

/**
 * @test Reset_EnableRateCorrectionFalse_ResetsToNormal
 * @brief Disabling rate correction while at a non-normal rate should trigger
 *        ResetToNormalRate on the next worker iteration.
 */
TEST_F(AampLatencyMonitorTest, Reset_EnableRateCorrectionFalse_ResetsToNormal)
{
	// Drive the monitor to maxRate first.
	ON_CALL(*mMockAamp, GetCurrentLatencyMs()).WillByDefault(Return(15000L));
	ON_CALL(*mMockAamp, GetBufferedDurationSecs()).WillByDefault(Return(5.0));
    EXPECT_CALL(*mMockSinkMgr, GetStreamSink(_))
		.WillRepeatedly(Return(mMockSink));
    // Expect the worker to request maxRate from the sink due to high latency.
    EXPECT_CALL(*mMockSink, SetPlayBackRate(DEFAULT_MAX_RATE_CORRECTION_SPEED))
		.Times(AtLeast(1))
		.WillRepeatedly(Return(true));

	mMonitor->Start(MakeFastConfig());
	ASSERT_TRUE(WaitForRunning());
	ASSERT_TRUE(WaitForRate(DEFAULT_MAX_RATE_CORRECTION_SPEED, 500));

    // EnableRateCorrection(false) must reset the rate via the sink.
	EXPECT_CALL(*mMockSink, SetPlayBackRate(DEFAULT_NORMAL_RATE_CORRECTION_SPEED))
		.Times(AtLeast(1))
		.WillRepeatedly(Return(true));
    
	// Disable correction — worker should reset to normal.
	mMonitor->EnableRateCorrection(false);
	ASSERT_TRUE(WaitForRate(DEFAULT_NORMAL_RATE_CORRECTION_SPEED, 500));
}

/**
 * @test Reset_EnableRateCorrectionFalse_ThenTrue_Resumes
 * @brief Re-enabling correction after it was disabled resumes normal monitoring.
 */
TEST_F(AampLatencyMonitorTest, Reset_EnableRateCorrectionFalse_ThenTrue_Resumes)
{
	// Latency in-band so rate stays normal once correction re-enabled.
	ON_CALL(*mMockAamp, GetCurrentLatencyMs()).WillByDefault(Return(static_cast<long>(DEFAULT_TARGET_LATENCY_MS)));
	ON_CALL(*mMockAamp, GetBufferedDurationSecs()).WillByDefault(Return(5.0));

	mMonitor->Start(MakeFastConfig());
	ASSERT_TRUE(WaitForRunning());

	mMonitor->EnableRateCorrection(false);
	// Small pause to let the worker react.
	std::this_thread::sleep_for(std::chrono::milliseconds(20));
	EXPECT_DOUBLE_EQ(mMonitor->GetCurrentRate(), DEFAULT_NORMAL_RATE_CORRECTION_SPEED);

	mMonitor->EnableRateCorrection(true);
	// Monitor should still be running and at normal rate.
	std::this_thread::sleep_for(std::chrono::milliseconds(20));
	EXPECT_DOUBLE_EQ(mMonitor->GetCurrentRate(), DEFAULT_NORMAL_RATE_CORRECTION_SPEED);
	EXPECT_TRUE(mMonitor->IsRunning());
}

/**
 * @test Reset_EnableRateCorrectionSameState_IsNoOp
 * @brief Calling EnableRateCorrection with the current value must be a no-op.
 */
TEST_F(AampLatencyMonitorTest, Reset_EnableRateCorrectionSameState_IsNoOp)
{
	mMonitor->Start(MakeFastConfig());
	ASSERT_TRUE(WaitForRunning());
	// Already enabled by default; calling again with true should not crash.
	mMonitor->EnableRateCorrection(true);
	EXPECT_TRUE(mMonitor->IsRunning());
}

// ===========================================================================
// 4. Rate-correction logic
// ===========================================================================

/**
 * @test RateCorrection_HighLatency_SpeedsUp
 * @brief When latency > maxLatencyMs the worker
 *        must request maxPlaybackRate from the sink.
 */
TEST_F(AampLatencyMonitorTest, RateCorrection_HighLatency_SpeedsUp)
{
	// Latency = 12 000 ms > 9 000 ms max; buffer = 5 s >= 4 s target.
	ON_CALL(*mMockAamp, GetCurrentLatencyMs()).WillByDefault(Return(12000L));
	ON_CALL(*mMockAamp, GetBufferedDurationSecs()).WillByDefault(Return(5.0));

	EXPECT_CALL(*mMockSink, SetPlayBackRate(DEFAULT_MAX_RATE_CORRECTION_SPEED)).Times(AtLeast(1)).WillRepeatedly(Return(true));
	// Allow the reset-to-normal call issued by Stop() during TearDown.
	EXPECT_CALL(*mMockSink, SetPlayBackRate(DEFAULT_NORMAL_RATE_CORRECTION_SPEED)).Times(AnyNumber()).WillRepeatedly(Return(true));

	mMonitor->Start(MakeFastConfig());
	ASSERT_TRUE(WaitForRunning());
	EXPECT_TRUE(WaitForRate(DEFAULT_MAX_RATE_CORRECTION_SPEED, 500));
}

/**
 * @test RateCorrection_BufferBelowThreshold_SkipsPoll
 * @brief When the buffer level is below correctionActivationThresholdSec the
 *        worker must skip the entire poll and reset the rate to normal,
 *        regardless of the current latency.
 */
TEST_F(AampLatencyMonitorTest, RateCorrection_BufferBelowThreshold_SkipsPoll)
{
	// Latency is out-of-band-high, but buffer (1.5 s) is below the configured
	// correction-enable threshold (2000ms) — the poll must be skipped.
	ON_CALL(*mMockAamp, GetCurrentLatencyMs()).WillByDefault(Return(12000L));
	ON_CALL(*mMockAamp, GetBufferedDurationSecs()).WillByDefault(Return(1.5));

	mMonitor->Start(MakeFastConfig(
		DEFAULT_NORMAL_RATE_CORRECTION_SPEED,
		DEFAULT_MIN_RATE_CORRECTION_SPEED,
		DEFAULT_MAX_RATE_CORRECTION_SPEED,
		DEFAULT_MIN_LATENCY_MS, DEFAULT_TARGET_LATENCY_MS, DEFAULT_MAX_LATENCY_MS,
		2000)); // correctionActivationThresholdMs = 2000ms
	ASSERT_TRUE(WaitForRunning());
	std::this_thread::sleep_for(std::chrono::milliseconds(50));
	// Rate must remain at normal — poll is skipped when buffer is below threshold.
	EXPECT_DOUBLE_EQ(mMonitor->GetCurrentRate(), DEFAULT_NORMAL_RATE_CORRECTION_SPEED);
}

/**
 * @test RateCorrection_LowLatency_SlowsDown
 * @brief When latency < minLatencyMs the worker must request minPlaybackRate.
 */
TEST_F(AampLatencyMonitorTest, RateCorrection_LowLatency_SlowsDown)
{
	// Latency = 1 000 ms < 3 000 ms min; buffer healthy.
	ON_CALL(*mMockAamp, GetCurrentLatencyMs()).WillByDefault(Return(1000L));
	ON_CALL(*mMockAamp, GetBufferedDurationSecs()).WillByDefault(Return(5.0));

	EXPECT_CALL(*mMockSink, SetPlayBackRate(DEFAULT_MIN_RATE_CORRECTION_SPEED)).Times(AtLeast(1)).WillRepeatedly(Return(true));
	// Allow the reset-to-normal call issued by Stop() during TearDown.
	EXPECT_CALL(*mMockSink, SetPlayBackRate(DEFAULT_NORMAL_RATE_CORRECTION_SPEED)).Times(AnyNumber()).WillRepeatedly(Return(true));

	mMonitor->Start(MakeFastConfig());
	ASSERT_TRUE(WaitForRunning());
	EXPECT_TRUE(WaitForRate(DEFAULT_MIN_RATE_CORRECTION_SPEED, 500));
}

/**
 * @test RateCorrection_InBandLatency_StaysNormal
 * @brief When latency is within [minLatencyMs, maxLatencyMs] the rate must
 *        remain at normalPlaybackRate.
 */
TEST_F(AampLatencyMonitorTest, RateCorrection_InBandLatency_StaysNormal)
{
	// Latency at target — squarely in the dead-band [min, max].
	ON_CALL(*mMockAamp, GetCurrentLatencyMs()).WillByDefault(Return(static_cast<long>(DEFAULT_TARGET_LATENCY_MS)));
	ON_CALL(*mMockAamp, GetBufferedDurationSecs()).WillByDefault(Return(5.0));

	mMonitor->Start(MakeFastConfig());
	ASSERT_TRUE(WaitForRunning());
	std::this_thread::sleep_for(std::chrono::milliseconds(50));
	EXPECT_DOUBLE_EQ(mMonitor->GetCurrentRate(), DEFAULT_NORMAL_RATE_CORRECTION_SPEED);
}

/**
 * @test RateCorrection_ReturnToNormal_AfterSpeedUp
 * @brief Once latency falls back to targetLatencyMs while running fast,
 *        the worker must return to normalPlaybackRate.
 */
TEST_F(AampLatencyMonitorTest, RateCorrection_ReturnToNormal_AfterSpeedUp)
{
	// Phase 1: high latency — drive to maxRate.
	std::atomic<long> latency{12000L};
	ON_CALL(*mMockAamp, GetCurrentLatencyMs()).WillByDefault([&latency]() { return latency.load(); });
	ON_CALL(*mMockAamp, GetBufferedDurationSecs()).WillByDefault(Return(5.0));
	ON_CALL(*mMockSink, SetPlayBackRate(_)).WillByDefault(Return(true));

	mMonitor->Start(MakeFastConfig());
	ASSERT_TRUE(WaitForRunning());
	ASSERT_TRUE(WaitForRate(DEFAULT_MAX_RATE_CORRECTION_SPEED, 500));

	// Phase 2: latency caught up to target — expect return to normal.
	latency.store(5000L); // <= DEFAULT_TARGET_LATENCY_MS, while at maxRate
	EXPECT_TRUE(WaitForRate(DEFAULT_NORMAL_RATE_CORRECTION_SPEED, 500));
}

/**
 * @test RateCorrection_ReturnToNormal_AfterSlowDown
 * @brief Once latency rises back to targetLatencyMs while running slow,
 *        the worker must return to normalPlaybackRate.
 */
TEST_F(AampLatencyMonitorTest, RateCorrection_ReturnToNormal_AfterSlowDown)
{
	// Phase 1: low latency — drive to minRate.
	std::atomic<long> latency{1000L};
	ON_CALL(*mMockAamp, GetCurrentLatencyMs()).WillByDefault([&latency]() { return latency.load(); });
	ON_CALL(*mMockAamp, GetBufferedDurationSecs()).WillByDefault(Return(5.0));
	ON_CALL(*mMockSink, SetPlayBackRate(_)).WillByDefault(Return(true));

	mMonitor->Start(MakeFastConfig());
	ASSERT_TRUE(WaitForRunning());
	ASSERT_TRUE(WaitForRate(DEFAULT_MIN_RATE_CORRECTION_SPEED, 500));

	// Phase 2: latency rose back to target — should return to normal.
	latency.store(7000L); // >= DEFAULT_TARGET_LATENCY_MS, while at minRate
	EXPECT_TRUE(WaitForRate(DEFAULT_NORMAL_RATE_CORRECTION_SPEED, 500));
}

/**
 * @test RateCorrection_AdPlaying_SkipsCorrection
 * @brief When an ad is playing the worker must reset to normalPlaybackRate and
 *        skip the latency check.
 */
TEST_F(AampLatencyMonitorTest, RateCorrection_AdPlaying_SkipsCorrection)
{
	// Latency would normally trigger speed-up, but ad is playing.
	ON_CALL(*mMockAamp, GetCurrentLatencyMs()).WillByDefault(Return(12000L));
	ON_CALL(*mMockAamp, GetBufferedDurationSecs()).WillByDefault(Return(5.0));
	ON_CALL(*mMockAamp, IsAdPlaying()).WillByDefault(Return(true));

    EXPECT_CALL(*mMockSink, SetPlayBackRate(_)).Times(0); // no rate changes when ad is playing
	mMonitor->Start(MakeFastConfig());
	ASSERT_TRUE(WaitForRunning());
	std::this_thread::sleep_for(std::chrono::milliseconds(50));
	// Should stay at normal rate because ad is playing.
	EXPECT_DOUBLE_EQ(mMonitor->GetCurrentRate(), DEFAULT_NORMAL_RATE_CORRECTION_SPEED);
}

/**
 * @test RateCorrection_NotPlaying_SkipsCorrection
 * @brief When player state != eSTATE_PLAYING the worker must skip the poll.
 */
TEST_F(AampLatencyMonitorTest, RateCorrection_NotPlaying_SkipsCorrection)
{
	ON_CALL(*mMockAamp, GetState()).WillByDefault(Return(eSTATE_BUFFERING));
	ON_CALL(*mMockAamp, GetCurrentLatencyMs()).WillByDefault(Return(12000L));
	ON_CALL(*mMockAamp, GetBufferedDurationSecs()).WillByDefault(Return(5.0));

    EXPECT_CALL(*mMockSink, SetPlayBackRate(_)).Times(0); // no rate changes when state != eSTATE_PLAYING

	mMonitor->Start(MakeFastConfig());
	ASSERT_TRUE(WaitForRunning());
	std::this_thread::sleep_for(std::chrono::milliseconds(50));
	EXPECT_DOUBLE_EQ(mMonitor->GetCurrentRate(), DEFAULT_NORMAL_RATE_CORRECTION_SPEED);
}

/**
 * @test RateCorrection_SinkReturnsFailure_RateUnchanged
 * @brief If SetPlayBackRate() fails the monitor must not update mCurrentRate.
 */
TEST_F(AampLatencyMonitorTest, RateCorrection_SinkReturnsFailure_RateUnchanged)
{
	ON_CALL(*mMockAamp, GetCurrentLatencyMs()).WillByDefault(Return(12000L));
	ON_CALL(*mMockAamp, GetBufferedDurationSecs()).WillByDefault(Return(5.0));
	// Sink rejects rate change.
	ON_CALL(*mMockSink, SetPlayBackRate(_)).WillByDefault(Return(false));

	mMonitor->Start(MakeFastConfig());
	ASSERT_TRUE(WaitForRunning());
	std::this_thread::sleep_for(std::chrono::milliseconds(50));
	// Rate must remain at normal since the sink rejected the change.
	EXPECT_DOUBLE_EQ(mMonitor->GetCurrentRate(), DEFAULT_NORMAL_RATE_CORRECTION_SPEED);
}

/**
 * @test RateCorrection_NoSink_RateUnchanged
 * @brief If GetStreamSink() returns nullptr the monitor must not crash and the
 *        rate must remain at its current value.
 */
TEST_F(AampLatencyMonitorTest, RateCorrection_NoSink_RateUnchanged)
{
	ON_CALL(*mMockAamp, GetCurrentLatencyMs()).WillByDefault(Return(12000L));
	ON_CALL(*mMockAamp, GetBufferedDurationSecs()).WillByDefault(Return(5.0));
	ON_CALL(*mMockSinkMgr, GetStreamSink(_)).WillByDefault(Return(nullptr));

	mMonitor->Start(MakeFastConfig());
	ASSERT_TRUE(WaitForRunning());
	std::this_thread::sleep_for(std::chrono::milliseconds(50));
	EXPECT_DOUBLE_EQ(mMonitor->GetCurrentRate(), DEFAULT_NORMAL_RATE_CORRECTION_SPEED);
}

/**
 * @test RateCorrection_NegativeBuffer_SkipsPoll
 * @brief GetBufferedDurationSecs() returning a negative value (e.g. during a
 *        period switch) is below the default threshold (0.0), so the poll must
 *        be skipped and the rate must remain at normal.
 */
TEST_F(AampLatencyMonitorTest, RateCorrection_NegativeBuffer_SkipsPoll)
{
	ON_CALL(*mMockAamp, GetCurrentLatencyMs()).WillByDefault(Return(12000L));
	ON_CALL(*mMockAamp, GetBufferedDurationSecs()).WillByDefault(Return(-1.0));

	mMonitor->Start(MakeFastConfig()); // default correctionActivationThresholdSec = 0.0
	ASSERT_TRUE(WaitForRunning());
	std::this_thread::sleep_for(std::chrono::milliseconds(50));
	// -1.0 < 0.0 → poll is skipped, rate stays at normal.
	EXPECT_DOUBLE_EQ(mMonitor->GetCurrentRate(), DEFAULT_NORMAL_RATE_CORRECTION_SPEED);
}

// ===========================================================================
// 5. Adaptive latency threshold shifts driven by rebuffering events
// ===========================================================================

/**
 * @test AdaptiveThreshold_ZeroStep_ThresholdsUnchanged
 * @brief When rebufferingLatencyStepMs is 0 (default), OnRebufferingStart()
 *        must leave all three thresholds unchanged.
 */
TEST_F(AampLatencyMonitorTest, AdaptiveThreshold_ZeroStep_ThresholdsUnchanged)
{
	mMonitor->Start(MakeFastConfig()); // rebufStepMs = 0 by default
	ASSERT_TRUE(WaitForRunning()); // wait for worker to start before calling OnRebufferingStart()

	mMonitor->OnRebufferingStart();

	auto [minMs, targetMs, maxMs] = mMonitor->GetCurrentThresholds();
	EXPECT_DOUBLE_EQ(minMs,    DEFAULT_MIN_LATENCY_MS);
	EXPECT_DOUBLE_EQ(targetMs, DEFAULT_TARGET_LATENCY_MS);
	EXPECT_DOUBLE_EQ(maxMs,    DEFAULT_MAX_LATENCY_MS);
}

/**
 * @test AdaptiveThreshold_MultipleRebuffers_ThresholdsAccumulate
 * @brief Multiple OnRebufferingStart() calls must accumulate the step so that
 *        after N calls the shift equals N * rebufferingLatencyStepMs.
 */
TEST_F(AampLatencyMonitorTest, AdaptiveThreshold_MultipleRebuffers_ThresholdsAccumulate)
{
	constexpr double kStep = 1000.0;
	mMonitor->Start(MakeFastConfig(
		DEFAULT_NORMAL_RATE_CORRECTION_SPEED,
		DEFAULT_MIN_RATE_CORRECTION_SPEED,
		DEFAULT_MAX_RATE_CORRECTION_SPEED,
		DEFAULT_MIN_LATENCY_MS, DEFAULT_TARGET_LATENCY_MS, DEFAULT_MAX_LATENCY_MS,
		0.0, kStep, /*rebufMaxIncrMs=*/0.0));
	ASSERT_TRUE(WaitForRunning()); // wait for worker to start before calling OnRebufferingStart()

	mMonitor->OnRebufferingStart();
	mMonitor->OnRebufferingStart();

	auto [minMs, targetMs, maxMs] = mMonitor->GetCurrentThresholds();
	EXPECT_DOUBLE_EQ(minMs,    DEFAULT_MIN_LATENCY_MS    + 2.0 * kStep);
	EXPECT_DOUBLE_EQ(targetMs, DEFAULT_TARGET_LATENCY_MS + 2.0 * kStep);
	EXPECT_DOUBLE_EQ(maxMs,    DEFAULT_MAX_LATENCY_MS    + 2.0 * kStep);
}

/**
 * @test AdaptiveThreshold_MaxIncrementCap_ThresholdsClamped
 * @brief When the accumulated increment would exceed
 *        rebufferingLatencyMaxIncrementMs the thresholds must be clamped
 *        at exactly (base + maxIncrement) and not grow further.
 */
TEST_F(AampLatencyMonitorTest, AdaptiveThreshold_MaxIncrementCap_ThresholdsClamped)
{
	constexpr double kStep   = 1000.0;
	constexpr double kMaxIncr = 3000.0;
	mMonitor->Start(MakeFastConfig(
		DEFAULT_NORMAL_RATE_CORRECTION_SPEED,
		DEFAULT_MIN_RATE_CORRECTION_SPEED,
		DEFAULT_MAX_RATE_CORRECTION_SPEED,
		DEFAULT_MIN_LATENCY_MS, DEFAULT_TARGET_LATENCY_MS, DEFAULT_MAX_LATENCY_MS,
		0.0, kStep, kMaxIncr));
	ASSERT_TRUE(WaitForRunning()); // wait for worker to start before calling OnRebufferingStart()

	// Four calls: cap should be reached after the third.
	mMonitor->OnRebufferingStart(); // accumulated = 1000
	mMonitor->OnRebufferingStart(); // accumulated = 2000
	mMonitor->OnRebufferingStart(); // accumulated = 3000 (at cap)
	mMonitor->OnRebufferingStart(); // still capped at 3000

	auto [minMs, targetMs, maxMs] = mMonitor->GetCurrentThresholds();
	EXPECT_DOUBLE_EQ(minMs,    DEFAULT_MIN_LATENCY_MS    + kMaxIncr);
	EXPECT_DOUBLE_EQ(targetMs, DEFAULT_TARGET_LATENCY_MS + kMaxIncr);
	EXPECT_DOUBLE_EQ(maxMs,    DEFAULT_MAX_LATENCY_MS    + kMaxIncr);
}

/**
 * @test AdaptiveThreshold_Stop_ResetsThresholdsToConfigDefaults
 * @brief After Stop() the dynamic thresholds must be restored to the values
 *        that were passed to Start(), and GetCurrentThresholds() must reflect
 *        those defaults immediately.
 */
TEST_F(AampLatencyMonitorTest, AdaptiveThreshold_Stop_ResetsThresholdsToConfigDefaults)
{
	constexpr double kStep = 1000.0;
	mMonitor->Start(MakeFastConfig(
		DEFAULT_NORMAL_RATE_CORRECTION_SPEED,
		DEFAULT_MIN_RATE_CORRECTION_SPEED,
		DEFAULT_MAX_RATE_CORRECTION_SPEED,
		DEFAULT_MIN_LATENCY_MS, DEFAULT_TARGET_LATENCY_MS, DEFAULT_MAX_LATENCY_MS,
		0.0, kStep, /*rebufMaxIncrMs=*/0.0));
	ASSERT_TRUE(WaitForRunning()); // wait for worker to start before calling OnRebufferingStart()

	mMonitor->OnRebufferingStart();
	{
		auto [minMs, targetMs, maxMs] = mMonitor->GetCurrentThresholds();
		// Verify the shift was applied before Stop().
		EXPECT_DOUBLE_EQ(minMs, DEFAULT_MIN_LATENCY_MS + kStep);
	}

	mMonitor->Stop();

	// After Stop() the thresholds must be back to config defaults.
	auto [minMs, targetMs, maxMs] = mMonitor->GetCurrentThresholds();
	EXPECT_DOUBLE_EQ(minMs,    DEFAULT_MIN_LATENCY_MS);
	EXPECT_DOUBLE_EQ(targetMs, DEFAULT_TARGET_LATENCY_MS);
	EXPECT_DOUBLE_EQ(maxMs,    DEFAULT_MAX_LATENCY_MS);
}

/**
 * @test AdaptiveThreshold_DisableRateCorrection_ResetsThresholdsToConfigDefaults
 * @brief After disabling rate correction the dynamic thresholds must be restored to the values
 *        that were passed to Start(), and GetCurrentThresholds() must reflect
 *        those defaults immediately.
 */
TEST_F(AampLatencyMonitorTest, AdaptiveThreshold_DisableRateCorrection_ResetsThresholdsToConfigDefaults)
{
	constexpr double kStep = 1000.0;
	mMonitor->Start(MakeFastConfig(
		DEFAULT_NORMAL_RATE_CORRECTION_SPEED,
		DEFAULT_MIN_RATE_CORRECTION_SPEED,
		DEFAULT_MAX_RATE_CORRECTION_SPEED,
		DEFAULT_MIN_LATENCY_MS, DEFAULT_TARGET_LATENCY_MS, DEFAULT_MAX_LATENCY_MS,
		0.0, kStep, /*rebufMaxIncrMs=*/0.0));
	ASSERT_TRUE(WaitForRunning()); // wait for worker to start before calling OnRebufferingStart()

	mMonitor->OnRebufferingStart();
	{
		auto [minMs, targetMs, maxMs] = mMonitor->GetCurrentThresholds();
		// Verify the shift was applied before disabling rate correction.
		EXPECT_DOUBLE_EQ(minMs, DEFAULT_MIN_LATENCY_MS + kStep);
	}

	mMonitor->EnableRateCorrection(false);

	// After disabling rate correction the thresholds must be back to config defaults.
	auto [minMs, targetMs, maxMs] = mMonitor->GetCurrentThresholds();
	EXPECT_DOUBLE_EQ(minMs,    DEFAULT_MIN_LATENCY_MS);
	EXPECT_DOUBLE_EQ(targetMs, DEFAULT_TARGET_LATENCY_MS);
	EXPECT_DOUBLE_EQ(maxMs,    DEFAULT_MAX_LATENCY_MS);
}


/**
 * @test AdaptiveThreshold_ShiftedThreshold_ChangesRateCorrectionBehavior
 * @brief Behavioural test: latency that is in-band before rebuffering must
 *        trigger a slow-down after OnRebufferingStart() shifts minLatencyMs
 *        above the observed value.
 *
 * Setup:
 *   minLatencyMs = 3 000 ms, step = 1 000 ms.
 *   Observed latency = 3 200 ms (above min → in-band → normal rate).
 *   After OnRebufferingStart(): minLatencyMs = 4 000 ms.
 *   3 200 ms < 4 000 ms → worker must apply minPlaybackRate.
 */
TEST_F(AampLatencyMonitorTest,
	AdaptiveThreshold_ShiftedThreshold_ChangesRateCorrectionBehavior)
{
	// Latency just above the default min — initially in-band.
	ON_CALL(*mMockAamp, GetCurrentLatencyMs()).WillByDefault(Return(3200L));
	ON_CALL(*mMockAamp, GetBufferedDurationSecs()).WillByDefault(Return(5.0));
	ON_CALL(*mMockSink, SetPlayBackRate(_)).WillByDefault(Return(true));

	mMonitor->Start(MakeFastConfig(
		DEFAULT_NORMAL_RATE_CORRECTION_SPEED,
		DEFAULT_MIN_RATE_CORRECTION_SPEED,
		DEFAULT_MAX_RATE_CORRECTION_SPEED,
		DEFAULT_MIN_LATENCY_MS,   // 3 000 ms
		DEFAULT_TARGET_LATENCY_MS,
		DEFAULT_MAX_LATENCY_MS,
		0.0,
		/*rebufStepMs=*/1000.0,
		/*rebufMaxIncrMs=*/0.0));
	ASSERT_TRUE(WaitForRunning());

	// Confirm the rate stays normal while 3 200 ms > min (3 000 ms).
	std::this_thread::sleep_for(std::chrono::milliseconds(30));
	EXPECT_DOUBLE_EQ(mMonitor->GetCurrentRate(), DEFAULT_NORMAL_RATE_CORRECTION_SPEED);

	// Rebuffering shifts minLatencyMs to 4 000 ms; now 3 200 < 4 000.
	EXPECT_CALL(*mMockSink, SetPlayBackRate(DEFAULT_MIN_RATE_CORRECTION_SPEED))
		.Times(AtLeast(1)).WillRepeatedly(Return(true));
	EXPECT_CALL(*mMockSink, SetPlayBackRate(DEFAULT_NORMAL_RATE_CORRECTION_SPEED))
		.Times(AnyNumber()).WillRepeatedly(Return(true));

	mMonitor->OnRebufferingStart();
	EXPECT_TRUE(WaitForRate(DEFAULT_MIN_RATE_CORRECTION_SPEED, 500));
}