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
#include <cmath>
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

// Integer (ms) equivalents — use in Return() stubs and atomic initialisers to
// avoid implicit double→long narrowing conversions.
static constexpr long kMinLatencyMs    = static_cast<long>(DEFAULT_MIN_LATENCY_MS);
static constexpr long kTargetLatencyMs = static_cast<long>(DEFAULT_TARGET_LATENCY_MS);
static constexpr long kMaxLatencyMs    = static_cast<long>(DEFAULT_MAX_LATENCY_MS);

// ---------------------------------------------------------------------------
// Helper: build a LatencyConfig with very short poll intervals so worker
// tests complete quickly in CI.
// ---------------------------------------------------------------------------
static LatencyConfig MakeFastConfig(
	double normalRate     = DEFAULT_NORMAL_RATE_CORRECTION_SPEED, //1.00
	double minRate        = DEFAULT_MIN_RATE_CORRECTION_SPEED,    //0.97
	double maxRate        = DEFAULT_MAX_RATE_CORRECTION_SPEED,    //1.03
	double minLatMs       = DEFAULT_MIN_LATENCY_MS,               //5000 ms
	double targetLatMs    = DEFAULT_TARGET_LATENCY_MS,            //6000 ms
	double maxLatMs       = DEFAULT_MAX_LATENCY_MS,               //7000 ms
	double bufToEnable    = DEFAULT_BUFFER_LEVEL_TO_ENABLE_LATENCY_SEC * 1000, // 0.0s
	double rebufStepMs    = 0.0,   // latency increment per rebuffering event
	double rebufMaxIncrMs = 0.0,   // max total accumulated increment (0 = uncapped)
	double dangerBufferMs = 0.0,   // min buffer (ms) that must hold for latencyStableSec
	double latencyStableSec = 0.0) // stable buffer duration (s) before one restoration step
{
	// monitorDelayMs = 0, monitorIntervalMs = 5 ms — fast for tests.
	return LatencyConfig{normalRate, minRate, maxRate,
		minLatMs, targetLatMs, maxLatMs,
		0, 5, bufToEnable, rebufStepMs, rebufMaxIncrMs,
		dangerBufferMs, latencyStableSec};
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

		g_mockPrivateInstanceAAMP = std::shared_ptr<MockPrivateInstanceAAMP>(mMockAamp, [](MockPrivateInstanceAAMP*){});
		g_mockAampStreamSinkManager = std::shared_ptr<MockAampStreamSinkManager>(mMockSinkMgr, [](MockAampStreamSinkManager*){});

		// Default safe stubs — overridden per test as required.
		ON_CALL(*mMockAamp, GetState()).WillByDefault(Return(eSTATE_PLAYING));
		ON_CALL(*mMockAamp, IsAdPlaying()).WillByDefault(Return(false));
		ON_CALL(*mMockAamp, GetCurrentLatencyMs()).WillByDefault(Return(kTargetLatencyMs));
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
		g_mockAampStreamSinkManager.reset();

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

	/// @brief Wait until the effective minimum latency threshold equals @p expected
	///        (up to @p maxWaitMs milliseconds).  Used to detect that Run() has
	///        performed a threshold shift or restoration after being woken or polled.
	bool WaitForMinLatency(double expected, int maxWaitMs = 500)
	{
		auto deadline = std::chrono::steady_clock::now()
			+ std::chrono::milliseconds(maxWaitMs);
		while (true)
		{
			auto [minMs, targetMs, maxMs] = mMonitor->GetCurrentThresholds();
			if (std::abs(minMs - expected) < 0.001)
				return true;
			if (std::chrono::steady_clock::now() >= deadline)
				return false;
			std::this_thread::sleep_for(std::chrono::milliseconds(2));
		}
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
	ON_CALL(*mMockAamp, GetCurrentLatencyMs()).WillByDefault(Return(kMaxLatencyMs + 6000L)); // > 7000 ms max
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
	ON_CALL(*mMockAamp, GetCurrentLatencyMs()).WillByDefault(Return(kMaxLatencyMs + 6000L));
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
	ON_CALL(*mMockAamp, GetCurrentLatencyMs()).WillByDefault(Return(kTargetLatencyMs));
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
	// Latency = 10 000 ms > 7 000 ms max; buffer = 5 s >= 4 s target.
	ON_CALL(*mMockAamp, GetCurrentLatencyMs()).WillByDefault(Return(kMaxLatencyMs + 3000L));
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
	ON_CALL(*mMockAamp, GetCurrentLatencyMs()).WillByDefault(Return(kMaxLatencyMs + 3000L));
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
	// Latency = 3 000 ms < 5 000 ms min; buffer healthy.
	ON_CALL(*mMockAamp, GetCurrentLatencyMs()).WillByDefault(Return(kMinLatencyMs - 2000L));
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
	ON_CALL(*mMockAamp, GetCurrentLatencyMs()).WillByDefault(Return(kTargetLatencyMs));
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
	std::atomic<long> latency{kMaxLatencyMs + 3000L};
	ON_CALL(*mMockAamp, GetCurrentLatencyMs()).WillByDefault([&latency]() { return latency.load(); });
	ON_CALL(*mMockAamp, GetBufferedDurationSecs()).WillByDefault(Return(5.0));
	ON_CALL(*mMockSink, SetPlayBackRate(_)).WillByDefault(Return(true));

	mMonitor->Start(MakeFastConfig());
	ASSERT_TRUE(WaitForRunning());
	ASSERT_TRUE(WaitForRate(DEFAULT_MAX_RATE_CORRECTION_SPEED, 500));

	// Phase 2: latency caught up to target — expect return to normal.
	latency.store(kTargetLatencyMs - 100L); // <= kTargetLatencyMs, while at maxRate
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
	std::atomic<long> latency{kMinLatencyMs - 2000L};
	ON_CALL(*mMockAamp, GetCurrentLatencyMs()).WillByDefault([&latency]() { return latency.load(); });
	ON_CALL(*mMockAamp, GetBufferedDurationSecs()).WillByDefault(Return(5.0));
	ON_CALL(*mMockSink, SetPlayBackRate(_)).WillByDefault(Return(true));

	mMonitor->Start(MakeFastConfig());
	ASSERT_TRUE(WaitForRunning());
	ASSERT_TRUE(WaitForRate(DEFAULT_MIN_RATE_CORRECTION_SPEED, 500));

	// Phase 2: latency rose back to target — should return to normal.
	latency.store(kTargetLatencyMs + 100L); // >= kTargetLatencyMs, while at minRate
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
	ON_CALL(*mMockAamp, GetCurrentLatencyMs()).WillByDefault(Return(kMaxLatencyMs + 3000L));
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
	ON_CALL(*mMockAamp, GetCurrentLatencyMs()).WillByDefault(Return(kMaxLatencyMs + 3000L));
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
	ON_CALL(*mMockAamp, GetCurrentLatencyMs()).WillByDefault(Return(kMaxLatencyMs + 3000L));
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
	ON_CALL(*mMockAamp, GetCurrentLatencyMs()).WillByDefault(Return(kMaxLatencyMs + 3000L));
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
	ON_CALL(*mMockAamp, GetCurrentLatencyMs()).WillByDefault(Return(kMaxLatencyMs + 3000L));
	ON_CALL(*mMockAamp, GetBufferedDurationSecs()).WillByDefault(Return(-1.0));

	mMonitor->Start(MakeFastConfig()); // default correctionActivationThresholdSec = 0.0
	ASSERT_TRUE(WaitForRunning());
	std::this_thread::sleep_for(std::chrono::milliseconds(50));
	// -1.0 < 0.0 → poll is skipped, rate stays at normal.
	EXPECT_DOUBLE_EQ(mMonitor->GetCurrentRate(), DEFAULT_NORMAL_RATE_CORRECTION_SPEED);
}

// ===========================================================================
// 5. Adaptive latency threshold shifts driven by low-buffer events
// ===========================================================================

/**
 * @test AdaptiveThreshold_ZeroStep_ThresholdsUnchanged
 * @brief When rebufferingLatencyStepMs is 0 (default), OnBufferLevelUpdate()
 *        with a buffer below dangerBufferMs must leave all three thresholds
 *        unchanged.
 */
TEST_F(AampLatencyMonitorTest, AdaptiveThreshold_ZeroStep_ThresholdsUnchanged)
{
	// rebufStepMs = 0 → OnBufferLevelUpdate early-returns, no shift.
	mMonitor->Start(MakeFastConfig(
		DEFAULT_NORMAL_RATE_CORRECTION_SPEED,
		DEFAULT_MIN_RATE_CORRECTION_SPEED,
		DEFAULT_MAX_RATE_CORRECTION_SPEED,
		DEFAULT_MIN_LATENCY_MS, DEFAULT_TARGET_LATENCY_MS, DEFAULT_MAX_LATENCY_MS,
		0.0, /*rebufStepMs=*/0.0, /*rebufMaxIncrMs=*/0.0,
		/*dangerBufferMs=*/1000.0, /*latencyStableSec=*/5.0));
	ASSERT_TRUE(WaitForRunning());

	mMonitor->OnBufferLevelUpdate(500.0); // below danger, but rebufStepMs=0 → no-op

	auto [minMs, targetMs, maxMs] = mMonitor->GetCurrentThresholds();
	EXPECT_DOUBLE_EQ(minMs,    DEFAULT_MIN_LATENCY_MS);
	EXPECT_DOUBLE_EQ(targetMs, DEFAULT_TARGET_LATENCY_MS);
	EXPECT_DOUBLE_EQ(maxMs,    DEFAULT_MAX_LATENCY_MS);
}

/**
 * @test AdaptiveThreshold_MultipleRebuffers_ThresholdsAccumulate
 * @brief Each distinct low-buffer episode must accumulate one rebufferingLatencyStepMs
 *        shift.  Two separate episodes (separated by a healthy-buffer recovery that
 *        clears the episode guard) must produce an accumulated shift of 2 * kStep.
 *
 * Design note: with the notify-only OnBufferLevelUpdate pattern the episode guard
 * (mBelowDangerShifted) is set by Run() after the first shift.  Calling
 * OnBufferLevelUpdate() with a healthy buffer clears the guard so that the next
 * dip is treated as a new episode and triggers a fresh wakeup + shift.
 */
TEST_F(AampLatencyMonitorTest, AdaptiveThreshold_MultipleRebuffers_ThresholdsAccumulate)
{
	constexpr double kStep = 1000.0;
	// GetBufferedDurationSecs drives Run()'s buffer level; start below danger.
	std::atomic<double> bufSecs{0.5}; // 500ms < dangerBufferMs 1000ms
	ON_CALL(*mMockAamp, GetBufferedDurationSecs()).WillByDefault([&bufSecs](){ return bufSecs.load(); });
	ON_CALL(*mMockAamp, GetCurrentLatencyMs()).WillByDefault(Return(kTargetLatencyMs));

	mMonitor->Start(MakeFastConfig(
		DEFAULT_NORMAL_RATE_CORRECTION_SPEED, DEFAULT_MIN_RATE_CORRECTION_SPEED, DEFAULT_MAX_RATE_CORRECTION_SPEED,
		DEFAULT_MIN_LATENCY_MS, DEFAULT_TARGET_LATENCY_MS, DEFAULT_MAX_LATENCY_MS,
		0.0, kStep, /*rebufMaxIncrMs=*/0.0,
		/*dangerBufferMs=*/1000.0, /*latencyStableSec=*/5.0));
	ASSERT_TRUE(WaitForRunning());

	// ── Episode 1 ──────────────────────────────────────────────────────────────
	// Buffer is low (500ms); wake Run() early via notify.
	mMonitor->OnBufferLevelUpdate(500.0); // below danger → wakes Run()
	// Run() sees bufSecs=0.5 (<1s danger), shifts +kStep, sets mBelowDangerShifted.
	ASSERT_TRUE(WaitForMinLatency(DEFAULT_MIN_LATENCY_MS + kStep, 500));

	// ── Recovery ───────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────
	// Healthy buffer wakes Run() which clears the episode guard via UpdateDangerBufferState.
	bufSecs.store(5.0); // 5000ms >= danger 1000ms → healthy
	mMonitor->OnBufferLevelUpdate(5000.0); // wakes Run() to clear mBelowDangerShifted
	// Wait for Run() to wake, observe healthy buffer, and clear mBelowDangerShifted
	// before we change bufSecs back to a danger level for episode 2.
	std::this_thread::sleep_for(std::chrono::milliseconds(30));

	// ── Episode 2 ──────────────────────────────────────────────────────────────
	bufSecs.store(0.5); // back below danger
	mMonitor->OnBufferLevelUpdate(500.0); // flag clear → wakes Run() again
	// Run() sees bufSecs=0.5, shifts +kStep a second time.
	ASSERT_TRUE(WaitForMinLatency(DEFAULT_MIN_LATENCY_MS + 2.0 * kStep, 500));

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
	constexpr double kStep    = 1000.0;
	constexpr double kMaxIncr = 3000.0;

	std::atomic<double> bufSecs{0.5}; // 500ms < dangerBufferMs 1000ms
	ON_CALL(*mMockAamp, GetBufferedDurationSecs()).WillByDefault([&bufSecs](){ return bufSecs.load(); });
	ON_CALL(*mMockAamp, GetCurrentLatencyMs()).WillByDefault(Return(kTargetLatencyMs));

	mMonitor->Start(MakeFastConfig(
		DEFAULT_NORMAL_RATE_CORRECTION_SPEED, DEFAULT_MIN_RATE_CORRECTION_SPEED, DEFAULT_MAX_RATE_CORRECTION_SPEED,
		DEFAULT_MIN_LATENCY_MS, DEFAULT_TARGET_LATENCY_MS, DEFAULT_MAX_LATENCY_MS,
		0.0, kStep, kMaxIncr,
		/*dangerBufferMs=*/1000.0, /*latencyStableSec=*/5.0));
	ASSERT_TRUE(WaitForRunning());

	// Three distinct episodes drive accumulation to the cap (3000ms).
	for (int episode = 1; episode <= 3; ++episode)
	{
		// Dip below danger.
		bufSecs.store(0.5);
		mMonitor->OnBufferLevelUpdate(500.0);
		const double expectedAfter = std::min(static_cast<double>(episode) * kStep, kMaxIncr);
		ASSERT_TRUE(WaitForMinLatency(DEFAULT_MIN_LATENCY_MS + expectedAfter, 500));

		// Recover to clear the episode guard before the next dip.
		bufSecs.store(5.0);
		mMonitor->OnBufferLevelUpdate(5000.0); // wakes Run() to clear mBelowDangerShifted
		// Wait for Run() to wake, observe healthy buffer, and clear mBelowDangerShifted.
		std::this_thread::sleep_for(std::chrono::milliseconds(30));
	}

	// A fourth episode must not increase the shift beyond the cap.
	bufSecs.store(0.5);
	mMonitor->OnBufferLevelUpdate(500.0);
	std::this_thread::sleep_for(std::chrono::milliseconds(50));

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
	// Buffer below danger so Run() shifts on each poll.
	ON_CALL(*mMockAamp, GetBufferedDurationSecs()).WillByDefault(Return(0.5));
	ON_CALL(*mMockAamp, GetCurrentLatencyMs()).WillByDefault(Return(kTargetLatencyMs));

	mMonitor->Start(MakeFastConfig(
		DEFAULT_NORMAL_RATE_CORRECTION_SPEED, DEFAULT_MIN_RATE_CORRECTION_SPEED, DEFAULT_MAX_RATE_CORRECTION_SPEED,
		DEFAULT_MIN_LATENCY_MS, DEFAULT_TARGET_LATENCY_MS, DEFAULT_MAX_LATENCY_MS,
		0.0, kStep, /*rebufMaxIncrMs=*/0.0,
		/*dangerBufferMs=*/1000.0, /*latencyStableSec=*/10.0));
	ASSERT_TRUE(WaitForRunning());

	// Wake Run() and let it shift +kStep.
	mMonitor->OnBufferLevelUpdate(500.0);
	ASSERT_TRUE(WaitForMinLatency(DEFAULT_MIN_LATENCY_MS + kStep, 500));

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
	ON_CALL(*mMockAamp, GetBufferedDurationSecs()).WillByDefault(Return(0.5));
	ON_CALL(*mMockAamp, GetCurrentLatencyMs()).WillByDefault(Return(kTargetLatencyMs));

	mMonitor->Start(MakeFastConfig(
		DEFAULT_NORMAL_RATE_CORRECTION_SPEED, DEFAULT_MIN_RATE_CORRECTION_SPEED, DEFAULT_MAX_RATE_CORRECTION_SPEED,
		DEFAULT_MIN_LATENCY_MS, DEFAULT_TARGET_LATENCY_MS, DEFAULT_MAX_LATENCY_MS,
		0.0, kStep, /*rebufMaxIncrMs=*/0.0,
		/*dangerBufferMs=*/1000.0, /*latencyStableSec=*/5.0));
	ASSERT_TRUE(WaitForRunning());

	// Wake Run() and let it shift +kStep.
	mMonitor->OnBufferLevelUpdate(500.0);
	ASSERT_TRUE(WaitForMinLatency(DEFAULT_MIN_LATENCY_MS + kStep, 500));

	mMonitor->EnableRateCorrection(false);

	// After disabling rate correction the thresholds must be back to config defaults.
	auto [minMs, targetMs, maxMs] = mMonitor->GetCurrentThresholds();
	EXPECT_DOUBLE_EQ(minMs,    DEFAULT_MIN_LATENCY_MS);
	EXPECT_DOUBLE_EQ(targetMs, DEFAULT_TARGET_LATENCY_MS);
	EXPECT_DOUBLE_EQ(maxMs,    DEFAULT_MAX_LATENCY_MS);
}


/**
 * @test AdaptiveThreshold_ShiftedThreshold_ChangesRateCorrectionBehavior
 * @brief Behavioural test: latency that is in-band before a low-buffer episode
 *        must trigger a slow-down after Run() shifts minLatencyMs above the
 *        observed value.
 *
 * Setup:
 *   minLatencyMs = 5 000 ms (DEFAULT_MIN_LATENCY_MS), step = 1 000 ms.
 *   Observed latency = 5 200 ms (above min → in-band → normal rate).
 *   Buffer below danger → Run() shifts: minLatencyMs = 6 000 ms.
 *   5 200 ms < 6 000 ms → worker must apply minPlaybackRate.
 */
TEST_F(AampLatencyMonitorTest,
	AdaptiveThreshold_ShiftedThreshold_ChangesRateCorrectionBehavior)
{
	// Buffer starts BELOW danger so Run() shifts on its first poll after the wakeup.
	std::atomic<double> bufSecs{0.5}; // 500ms < dangerBufferMs 1000ms
	ON_CALL(*mMockAamp, GetBufferedDurationSecs()).WillByDefault([&bufSecs](){ return bufSecs.load(); });
	ON_CALL(*mMockAamp, GetCurrentLatencyMs()).WillByDefault(Return(kMinLatencyMs + 200L));
	ON_CALL(*mMockSink, SetPlayBackRate(_)).WillByDefault(Return(true));

	mMonitor->Start(MakeFastConfig(
		DEFAULT_NORMAL_RATE_CORRECTION_SPEED,
		DEFAULT_MIN_RATE_CORRECTION_SPEED,
		DEFAULT_MAX_RATE_CORRECTION_SPEED,
		DEFAULT_MIN_LATENCY_MS,   // 5 000 ms
		DEFAULT_TARGET_LATENCY_MS,
		DEFAULT_MAX_LATENCY_MS,
		0.0,
		/*rebufStepMs=*/1000.0,
		/*rebufMaxIncrMs=*/0.0,
		/*dangerBufferMs=*/1000.0,
		/*latencyStableSec=*/5.0));
	ASSERT_TRUE(WaitForRunning());

	// Buffer is 500ms < dangerBufferMs. Run() polls naturally and shifts immediately
	// (no manual OnBufferLevelUpdate needed to trigger the shift).
	// minLatencyMs shifts from 5000ms to 6000ms.
	// latency (3200ms) < new minLatencyMs (4000ms) → worker must slow down.
	EXPECT_CALL(*mMockSink, SetPlayBackRate(DEFAULT_MIN_RATE_CORRECTION_SPEED))
		.Times(AtLeast(1)).WillRepeatedly(Return(true));
	EXPECT_CALL(*mMockSink, SetPlayBackRate(DEFAULT_NORMAL_RATE_CORRECTION_SPEED))
		.Times(AnyNumber()).WillRepeatedly(Return(true));

	EXPECT_TRUE(WaitForRate(DEFAULT_MIN_RATE_CORRECTION_SPEED, 500));
}

// ===========================================================================
// 6. Dynamic threshold restoration (TryRestoreThresholdsLocked)
//
// TryRestoreThresholdsLocked is called from Run() when:
//   - dangerBufferMs > 0
//   - latencyStableSec > 0
//   - rebufferingLatencyStepMs > 0
//   - bufferMs >= dangerBufferMs for latencyStableSec consecutive seconds
//     (measured across polling intervals by the worker thread)
// ===========================================================================

/**
 * @test Restoration_ZeroWindowSec_RestorationDisabled
 * @brief When latencyStableSec == 0 the restoration timer in Run() is gated out.
 *        Low-buffer episodes still shift thresholds upward (the shift mechanism is
 *        independent), but the healthy-buffer window that would tighten them back
 *        never fires.
 */
TEST_F(AampLatencyMonitorTest, Restoration_ZeroWindowSec_RestorationDisabled)
{
	constexpr double kStep = 1000.0;
	// Buffer below danger so Run() can shift.
	ON_CALL(*mMockAamp, GetBufferedDurationSecs()).WillByDefault(Return(0.5));
	ON_CALL(*mMockAamp, GetCurrentLatencyMs()).WillByDefault(Return(kTargetLatencyMs));

	// latencyStableSec = 0 → Run()'s restoration timer block is guarded out.
	mMonitor->Start(MakeFastConfig(
		DEFAULT_NORMAL_RATE_CORRECTION_SPEED, DEFAULT_MIN_RATE_CORRECTION_SPEED, DEFAULT_MAX_RATE_CORRECTION_SPEED,
		DEFAULT_MIN_LATENCY_MS, DEFAULT_TARGET_LATENCY_MS, DEFAULT_MAX_LATENCY_MS,
		/*bufToEnable=*/0.0, kStep, /*rebufMaxIncrMs=*/0.0,
		/*dangerBufferMs=*/1000.0, /*latencyStableSec=*/0.0));
	ASSERT_TRUE(WaitForRunning());

	// Shift happens (Run() sees buffer < danger).
	mMonitor->OnBufferLevelUpdate(500.0);
	ASSERT_TRUE(WaitForMinLatency(DEFAULT_MIN_LATENCY_MS + kStep, 500));

	// Now buffer becomes healthy; with latencyStableSec=0 no restoration must fire.
	ON_CALL(*mMockAamp, GetBufferedDurationSecs()).WillByDefault(Return(5.0));
	std::this_thread::sleep_for(std::chrono::milliseconds(100)); // let Run() poll several times

	auto [minMs, targetMs, maxMs] = mMonitor->GetCurrentThresholds();
	EXPECT_DOUBLE_EQ(minMs,    DEFAULT_MIN_LATENCY_MS    + kStep); // shift kept, never restored
	EXPECT_DOUBLE_EQ(targetMs, DEFAULT_TARGET_LATENCY_MS + kStep);
	EXPECT_DOUBLE_EQ(maxMs,    DEFAULT_MAX_LATENCY_MS    + kStep);
}

/**
 * @test Restoration_AlreadyAtBase_TryRestoreIsNoOp
 * @brief When there is no accumulated increment (no prior rebuffering),
 *        Run() must not apply a restoration step even after the stable-buffer
 *        window elapses.
 */
TEST_F(AampLatencyMonitorTest, Restoration_AlreadyAtBase_TryRestoreIsNoOp)
{
	// Healthy buffer throughout — no shift, no restoration needed.
	ON_CALL(*mMockAamp, GetBufferedDurationSecs()).WillByDefault(Return(5.0)); // 5000ms >= danger 1000ms
	ON_CALL(*mMockAamp, GetCurrentLatencyMs()).WillByDefault(Return(kTargetLatencyMs));

	mMonitor->Start(MakeFastConfig(
		DEFAULT_NORMAL_RATE_CORRECTION_SPEED, DEFAULT_MIN_RATE_CORRECTION_SPEED, DEFAULT_MAX_RATE_CORRECTION_SPEED,
		DEFAULT_MIN_LATENCY_MS, DEFAULT_TARGET_LATENCY_MS, DEFAULT_MAX_LATENCY_MS,
		/*bufToEnable=*/0.0, /*rebufStepMs=*/1000.0, /*rebufMaxIncrMs=*/8000.0,
		/*dangerBufferMs=*/1000.0, /*latencyStableSec=*/5.0));
	ASSERT_TRUE(WaitForRunning());

	// Let Run() poll for several windows with healthy buffer.
	// accumulated == 0 → early-return in TryRestoreThresholdsLocked → thresholds unchanged.
	std::this_thread::sleep_for(std::chrono::milliseconds(300));

	auto [minMs, targetMs, maxMs] = mMonitor->GetCurrentThresholds();
	EXPECT_DOUBLE_EQ(minMs,    DEFAULT_MIN_LATENCY_MS);
	EXPECT_DOUBLE_EQ(targetMs, DEFAULT_TARGET_LATENCY_MS);
	EXPECT_DOUBLE_EQ(maxMs,    DEFAULT_MAX_LATENCY_MS);
}

/**
 * @test Restoration_OneStep_ReducesAccumulatedByStep
 * @brief After one rebuffering episode the thresholds are shifted by kStep.
 *        Once Run() observes a healthy buffer for latencyStableSec (0.1 s),
 *        one TryRestoreThresholdsLocked call must reduce the shift by kStep,
 *        returning thresholds to base.
 *
 * Restoration is driven entirely by Run() polling GetBufferedDurationSecs();
 * no OnBufferLevelUpdate is needed to trigger the timer or the restore step.
 */
TEST_F(AampLatencyMonitorTest, Restoration_OneStep_ReducesAccumulatedByStep)
{
	constexpr double kStep = 1000.0;
	ON_CALL(*mMockAamp, GetCurrentLatencyMs()).WillByDefault(Return(kTargetLatencyMs));

	// Phase 1: buffer below danger → Run() shifts on first poll.
	std::atomic<double> bufSecs{0.5}; // 500ms < dangerBufferMs 1000ms
	ON_CALL(*mMockAamp, GetBufferedDurationSecs()).WillByDefault([&bufSecs](){ return bufSecs.load(); });

	mMonitor->Start(MakeFastConfig(
		DEFAULT_NORMAL_RATE_CORRECTION_SPEED, DEFAULT_MIN_RATE_CORRECTION_SPEED, DEFAULT_MAX_RATE_CORRECTION_SPEED,
		DEFAULT_MIN_LATENCY_MS, DEFAULT_TARGET_LATENCY_MS, DEFAULT_MAX_LATENCY_MS,
		/*bufToEnable=*/0.0, kStep, /*rebufMaxIncrMs=*/8000.0,
		/*dangerBufferMs=*/1000.0, /*latencyStableSec=*/0.5));
	ASSERT_TRUE(WaitForRunning());

	// Wake Run() and wait for the shift.
	mMonitor->OnBufferLevelUpdate(500.0);
	ASSERT_TRUE(WaitForMinLatency(DEFAULT_MIN_LATENCY_MS + kStep, 500));

	// Phase 2: buffer recovers → Run() observes healthy buffer for >= 0.5 s → restores.
	// monitorIntervalMs = 5ms; after ~500ms of polls the window elapses and restore fires.
	bufSecs.store(5.0); // 5000ms >= dangerBufferMs 1000ms
	ASSERT_TRUE(WaitForMinLatency(DEFAULT_MIN_LATENCY_MS, 2000)); // allow ~1.5s for restoration

	auto [minMs, targetMs, maxMs] = mMonitor->GetCurrentThresholds();
	EXPECT_DOUBLE_EQ(minMs,    DEFAULT_MIN_LATENCY_MS);
	EXPECT_DOUBLE_EQ(targetMs, DEFAULT_TARGET_LATENCY_MS);
	EXPECT_DOUBLE_EQ(maxMs,    DEFAULT_MAX_LATENCY_MS);
}

/**
 * @test Restoration_StepLargerThanAccumulated_ClampsToBase
 * @brief When rebufferingLatencyStepMs exceeds the accumulated increment,
 *        TryRestoreThresholdsLocked must clamp the result at 0 and return
 *        thresholds exactly to their config-default values.
 */
TEST_F(AampLatencyMonitorTest, Restoration_StepLargerThanAccumulated_ClampsToBase)
{
	ON_CALL(*mMockAamp, GetCurrentLatencyMs()).WillByDefault(Return(kTargetLatencyMs));

	std::atomic<double> bufSecs{0.5}; // below danger initially
	ON_CALL(*mMockAamp, GetBufferedDurationSecs()).WillByDefault([&bufSecs](){ return bufSecs.load(); });

	mMonitor->Start(MakeFastConfig(
		DEFAULT_NORMAL_RATE_CORRECTION_SPEED, DEFAULT_MIN_RATE_CORRECTION_SPEED, DEFAULT_MAX_RATE_CORRECTION_SPEED,
		DEFAULT_MIN_LATENCY_MS, DEFAULT_TARGET_LATENCY_MS, DEFAULT_MAX_LATENCY_MS,
		/*bufToEnable=*/0.0, /*rebufStepMs=*/1000.0, /*rebufMaxIncrMs=*/8000.0,
		/*dangerBufferMs=*/1000.0, /*latencyStableSec=*/0.5));
	ASSERT_TRUE(WaitForRunning());

	// Shift: accumulated = 1000ms.
	mMonitor->OnBufferLevelUpdate(500.0);
	ASSERT_TRUE(WaitForMinLatency(DEFAULT_MIN_LATENCY_MS + 1000.0, 500));

	// Recovery: Run() observes healthy buffer for >= 0.5s → restores.
	// rebufStep (1000ms) == accumulated (1000ms) → clamps to 0 → back to base.
	bufSecs.store(5.0);
	ASSERT_TRUE(WaitForMinLatency(DEFAULT_MIN_LATENCY_MS, 2000));

	auto [minMs, targetMs, maxMs] = mMonitor->GetCurrentThresholds();
	EXPECT_DOUBLE_EQ(minMs,    DEFAULT_MIN_LATENCY_MS);
	EXPECT_DOUBLE_EQ(targetMs, DEFAULT_TARGET_LATENCY_MS);
	EXPECT_DOUBLE_EQ(maxMs,    DEFAULT_MAX_LATENCY_MS);
}

/**
 * @test Restoration_MultipleSteps_ThresholdsReturnToBase
 * @brief Multiple restoration cycles (one per latencyStableSec window of healthy buffer)
 *        must reduce the accumulated increment step-by-step until it reaches zero.
 */
TEST_F(AampLatencyMonitorTest, Restoration_MultipleSteps_ThresholdsReturnToBase)
{
	constexpr double kStep    = 1000.0;
	constexpr double kMaxIncr = 3000.0;
	ON_CALL(*mMockAamp, GetCurrentLatencyMs()).WillByDefault(Return(kTargetLatencyMs));

	std::atomic<double> bufSecs{0.5};
	ON_CALL(*mMockAamp, GetBufferedDurationSecs()).WillByDefault([&bufSecs](){ return bufSecs.load(); });

	mMonitor->Start(MakeFastConfig(
		DEFAULT_NORMAL_RATE_CORRECTION_SPEED, DEFAULT_MIN_RATE_CORRECTION_SPEED, DEFAULT_MAX_RATE_CORRECTION_SPEED,
		DEFAULT_MIN_LATENCY_MS, DEFAULT_TARGET_LATENCY_MS, DEFAULT_MAX_LATENCY_MS,
		/*bufToEnable=*/0.0, kStep, kMaxIncr, /*dangerBufferMs=*/1000.0, /*latencyStableSec=*/5.0));
	ASSERT_TRUE(WaitForRunning());

	// Accumulate maximum shift via 3 distinct episodes.
	for (int episode = 1; episode <= 3; ++episode)
	{
		bufSecs.store(0.5);
		mMonitor->OnBufferLevelUpdate(500.0);
		const double expected = std::min(static_cast<double>(episode) * kStep, kMaxIncr);
		ASSERT_TRUE(WaitForMinLatency(DEFAULT_MIN_LATENCY_MS + expected, 500));
		// Recover briefly to clear episode guard before the next dip.
		bufSecs.store(5.0);
		mMonitor->OnBufferLevelUpdate(5000.0); // clears mBelowDangerShifted
		std::this_thread::sleep_for(std::chrono::milliseconds(30));
	}
	{
		auto [minMs, targetMs, maxMs] = mMonitor->GetCurrentThresholds();
		EXPECT_DOUBLE_EQ(minMs, DEFAULT_MIN_LATENCY_MS + kMaxIncr);
	}

	// Now keep buffer healthy so Run() drives 3 restoration cycles.
	// Each cycle: Run() observes healthy buffer for >= latencyStableSec (5.0s) → one restore step.
	// Wait long enough for all 3 windows to complete (3 * 5.0s + margin).
	bufSecs.store(5.0);
	ASSERT_TRUE(WaitForMinLatency(DEFAULT_MIN_LATENCY_MS, 20000));

	auto [minMs, targetMs, maxMs] = mMonitor->GetCurrentThresholds();
	EXPECT_DOUBLE_EQ(minMs,    DEFAULT_MIN_LATENCY_MS);
	EXPECT_DOUBLE_EQ(targetMs, DEFAULT_TARGET_LATENCY_MS);
	EXPECT_DOUBLE_EQ(maxMs,    DEFAULT_MAX_LATENCY_MS);
}

/**
 * @test Restoration_UnhealthyBuffer_DoesNotTrigger
 * @brief When the buffer returned by GetBufferedDurationSecs() stays below
 *        dangerBufferMs across all Run() polls, the restoration timer is
 *        never started, so TryRestoreThresholdsLocked must never fire and
 *        the shift must remain at the cap.
 */
TEST_F(AampLatencyMonitorTest, Restoration_UnhealthyBuffer_DoesNotTrigger)
{
	constexpr double kStep = 1000.0;
	ON_CALL(*mMockAamp, GetCurrentLatencyMs()).WillByDefault(Return(kTargetLatencyMs));
	// Buffer stays at 500ms < dangerBufferMs (1000ms) throughout.
	ON_CALL(*mMockAamp, GetBufferedDurationSecs()).WillByDefault(Return(0.5));

	mMonitor->Start(MakeFastConfig(
		DEFAULT_NORMAL_RATE_CORRECTION_SPEED, DEFAULT_MIN_RATE_CORRECTION_SPEED, DEFAULT_MAX_RATE_CORRECTION_SPEED,
		DEFAULT_MIN_LATENCY_MS, DEFAULT_TARGET_LATENCY_MS, DEFAULT_MAX_LATENCY_MS,
		/*bufToEnable=*/0.0, kStep, /*rebufMaxIncrMs=*/kStep,
		/*dangerBufferMs=*/1000.0, /*latencyStableSec=*/5.0));
	ASSERT_TRUE(WaitForRunning());

	// First episode: Run() shifts +kStep and sets the episode guard.
	mMonitor->OnBufferLevelUpdate(500.0);
	ASSERT_TRUE(WaitForMinLatency(DEFAULT_MIN_LATENCY_MS + kStep, 500));

	// Buffer remains unhealthy for several poll cycles.
	// Episode guard (mBelowDangerShifted=true) prevents further shifts.
	// Run() never enters the healthy branch → restoration timer never starts.
	std::this_thread::sleep_for(std::chrono::milliseconds(300));

	auto [minMs, targetMs, maxMs] = mMonitor->GetCurrentThresholds();
	EXPECT_DOUBLE_EQ(minMs,    DEFAULT_MIN_LATENCY_MS    + kStep);
	EXPECT_DOUBLE_EQ(targetMs, DEFAULT_TARGET_LATENCY_MS + kStep);
	EXPECT_DOUBLE_EQ(maxMs,    DEFAULT_MAX_LATENCY_MS    + kStep);
}

// ===========================================================================
// 7. Episode-guard and notify-only pattern
//
// These tests verify the new design where OnBufferLevelUpdate() is a pure
// notifier (it never shifts thresholds) and Run() is the sole actor.
// The episode guard (mBelowDangerShifted) ensures exactly one shift per
// continuous low-buffer episode regardless of how many times either path
// observes the low buffer.
// ===========================================================================

/**
 * @test EpisodeGuard_SameEpisode_ShiftsOnlyOnce
 * @brief While buffer stays below dangerBufferMs the episode guard must prevent
 *        Run() from applying more than one shift per continuous low-buffer
 *        episode, even across multiple polling intervals.
 */
TEST_F(AampLatencyMonitorTest, EpisodeGuard_SameEpisode_ShiftsOnlyOnce)
{
	constexpr double kStep = 1000.0;
	// Buffer stays below danger throughout — one continuous episode.
	ON_CALL(*mMockAamp, GetBufferedDurationSecs()).WillByDefault(Return(0.5)); // 500ms < 1000ms danger
	ON_CALL(*mMockAamp, GetCurrentLatencyMs()).WillByDefault(Return(kTargetLatencyMs));

	mMonitor->Start(MakeFastConfig(
		DEFAULT_NORMAL_RATE_CORRECTION_SPEED, DEFAULT_MIN_RATE_CORRECTION_SPEED, DEFAULT_MAX_RATE_CORRECTION_SPEED,
		DEFAULT_MIN_LATENCY_MS, DEFAULT_TARGET_LATENCY_MS, DEFAULT_MAX_LATENCY_MS,
		0.0, kStep, /*rebufMaxIncrMs=*/0.0,
		/*dangerBufferMs=*/1000.0, /*latencyStableSec=*/5.0));
	ASSERT_TRUE(WaitForRunning());

	// Wake Run() for the first (and only) shift of this episode.
	mMonitor->OnBufferLevelUpdate(500.0);
	ASSERT_TRUE(WaitForMinLatency(DEFAULT_MIN_LATENCY_MS + kStep, 500));

	// Let Run() poll several more times — episode guard blocks any further shift.
	std::this_thread::sleep_for(std::chrono::milliseconds(100));

	auto [minMs, targetMs, maxMs] = mMonitor->GetCurrentThresholds();
	EXPECT_DOUBLE_EQ(minMs,    DEFAULT_MIN_LATENCY_MS    + kStep); // exactly one shift
	EXPECT_DOUBLE_EQ(targetMs, DEFAULT_TARGET_LATENCY_MS + kStep);
	EXPECT_DOUBLE_EQ(maxMs,    DEFAULT_MAX_LATENCY_MS    + kStep);
}

/**
 * @test EpisodeGuard_MultipleOnBufferCalls_SameEpisode_WakesOnce
 * @brief Multiple OnBufferLevelUpdate() calls with buffer below danger during
 *        the same episode must only send a single wakeup to Run() (once the
 *        episode guard is set by Run(), subsequent calls are suppressed without
 *        taking mSleepMutex).  The net result is still exactly one shift.
 */
TEST_F(AampLatencyMonitorTest, EpisodeGuard_MultipleOnBufferCalls_SameEpisode_WakesOnce)
{
	constexpr double kStep = 1000.0;
	ON_CALL(*mMockAamp, GetBufferedDurationSecs()).WillByDefault(Return(0.5));
	ON_CALL(*mMockAamp, GetCurrentLatencyMs()).WillByDefault(Return(kTargetLatencyMs));

	mMonitor->Start(MakeFastConfig(
		DEFAULT_NORMAL_RATE_CORRECTION_SPEED, DEFAULT_MIN_RATE_CORRECTION_SPEED, DEFAULT_MAX_RATE_CORRECTION_SPEED,
		DEFAULT_MIN_LATENCY_MS, DEFAULT_TARGET_LATENCY_MS, DEFAULT_MAX_LATENCY_MS,
		0.0, kStep, /*rebufMaxIncrMs=*/0.0,
		/*dangerBufferMs=*/1000.0, /*latencyStableSec=*/5.0));
	ASSERT_TRUE(WaitForRunning());

	// First call wakes Run(); Run() shifts and sets guard.
	mMonitor->OnBufferLevelUpdate(500.0);
	ASSERT_TRUE(WaitForMinLatency(DEFAULT_MIN_LATENCY_MS + kStep, 500));

	// Subsequent calls during the same episode must be no-ops.
	for (int i = 0; i < 10; ++i)
	{
		mMonitor->OnBufferLevelUpdate(500.0);
	}
	std::this_thread::sleep_for(std::chrono::milliseconds(50));

	auto [minMs, targetMs, maxMs] = mMonitor->GetCurrentThresholds();
	EXPECT_DOUBLE_EQ(minMs, DEFAULT_MIN_LATENCY_MS + kStep); // still exactly one shift
}

/**
 * @test EpisodeGuard_RecoveryThenNewDip_ShiftsTwice
 * @brief A recovery (buffer >= dangerBufferMs) clears the episode guard.
 *        The next dip must be treated as a fresh episode and trigger a second
 *        shift, accumulating 2 * kStep in total.
 */
TEST_F(AampLatencyMonitorTest, EpisodeGuard_RecoveryThenNewDip_ShiftsTwice)
{
	constexpr double kStep = 1000.0;
	std::atomic<double> bufSecs{0.5};
	ON_CALL(*mMockAamp, GetBufferedDurationSecs()).WillByDefault([&bufSecs](){ return bufSecs.load(); });
	ON_CALL(*mMockAamp, GetCurrentLatencyMs()).WillByDefault(Return(kTargetLatencyMs));

	mMonitor->Start(MakeFastConfig(
		DEFAULT_NORMAL_RATE_CORRECTION_SPEED, DEFAULT_MIN_RATE_CORRECTION_SPEED, DEFAULT_MAX_RATE_CORRECTION_SPEED,
		DEFAULT_MIN_LATENCY_MS, DEFAULT_TARGET_LATENCY_MS, DEFAULT_MAX_LATENCY_MS,
		0.0, kStep, /*rebufMaxIncrMs=*/0.0,
		/*dangerBufferMs=*/1000.0, /*latencyStableSec=*/5.0));
	ASSERT_TRUE(WaitForRunning());

	// Episode 1: dip → Run() shifts → guard set.
	mMonitor->OnBufferLevelUpdate(500.0);
	ASSERT_TRUE(WaitForMinLatency(DEFAULT_MIN_LATENCY_MS + kStep, 500));

	// Recovery: healthy buffer wakes Run() which clears the episode guard via UpdateDangerBufferState.
	bufSecs.store(5.0);
	mMonitor->OnBufferLevelUpdate(5000.0); // wakes Run() to clear mBelowDangerShifted
	// Wait for Run() to wake, observe healthy buffer, and clear mBelowDangerShifted
	// before we change bufSecs back to a danger level for episode 2.
	std::this_thread::sleep_for(std::chrono::milliseconds(30));

	// Episode 2: new dip → guard is clear → Run() shifts again.
	bufSecs.store(0.5);
	mMonitor->OnBufferLevelUpdate(500.0);
	ASSERT_TRUE(WaitForMinLatency(DEFAULT_MIN_LATENCY_MS + 2.0 * kStep, 500));

	auto [minMs, targetMs, maxMs] = mMonitor->GetCurrentThresholds();
	EXPECT_DOUBLE_EQ(minMs,    DEFAULT_MIN_LATENCY_MS    + 2.0 * kStep);
	EXPECT_DOUBLE_EQ(targetMs, DEFAULT_TARGET_LATENCY_MS + 2.0 * kStep);
	EXPECT_DOUBLE_EQ(maxMs,    DEFAULT_MAX_LATENCY_MS    + 2.0 * kStep);
}

// ===========================================================================
// 8. Download-failure path: Run() detects low buffer without OnBufferLevelUpdate
//
// When fragment downloads are failing, OnBufferLevelUpdate() is never called.
// Run()'s regular poll must still detect buffer < dangerBufferMs and shift
// the thresholds on the first dip detection.
// ===========================================================================

/**
 * @test DownloadFailure_RunDetectsLowBuffer_ShiftsThreshold
 * @brief When OnBufferLevelUpdate() is never called (simulating a sustained
 *        download failure) but GetBufferedDurationSecs() returns a value below
 *        dangerBufferMs, Run()'s regular polling loop must detect the low buffer
 *        and shift thresholds upward.
 */
TEST_F(AampLatencyMonitorTest, DownloadFailure_RunDetectsLowBuffer_ShiftsThreshold)
{
	constexpr double kStep = 1000.0;
	// Buffer starts healthy, then drops to simulate a download failure draining the buffer.
	std::atomic<double> bufSecs{5.0};
	ON_CALL(*mMockAamp, GetBufferedDurationSecs()).WillByDefault([&bufSecs](){ return bufSecs.load(); });
	ON_CALL(*mMockAamp, GetCurrentLatencyMs()).WillByDefault(Return(kTargetLatencyMs));

	mMonitor->Start(MakeFastConfig(
		DEFAULT_NORMAL_RATE_CORRECTION_SPEED, DEFAULT_MIN_RATE_CORRECTION_SPEED, DEFAULT_MAX_RATE_CORRECTION_SPEED,
		DEFAULT_MIN_LATENCY_MS, DEFAULT_TARGET_LATENCY_MS, DEFAULT_MAX_LATENCY_MS,
		0.0, kStep, /*rebufMaxIncrMs=*/0.0,
		/*dangerBufferMs=*/1000.0, /*latencyStableSec=*/5.0));
	ASSERT_TRUE(WaitForRunning());

	// Confirm no shift while buffer is healthy.
	std::this_thread::sleep_for(std::chrono::milliseconds(30));
	{
		auto [minMs, targetMs, maxMs] = mMonitor->GetCurrentThresholds();
		EXPECT_DOUBLE_EQ(minMs, DEFAULT_MIN_LATENCY_MS);
	}

	// Simulate download failure: buffer drains below danger.
	// OnBufferLevelUpdate is NOT called (download not completing).
	bufSecs.store(0.5); // 500ms < 1000ms dangerBufferMs

	// Run()'s next scheduled poll must detect low buffer and shift.
	ASSERT_TRUE(WaitForMinLatency(DEFAULT_MIN_LATENCY_MS + kStep, 500));

	auto [minMs, targetMs, maxMs] = mMonitor->GetCurrentThresholds();
	EXPECT_DOUBLE_EQ(minMs,    DEFAULT_MIN_LATENCY_MS    + kStep);
	EXPECT_DOUBLE_EQ(targetMs, DEFAULT_TARGET_LATENCY_MS + kStep);
	EXPECT_DOUBLE_EQ(maxMs,    DEFAULT_MAX_LATENCY_MS    + kStep);
}

/**
 * @test DownloadFailure_SustainedLowBuffer_ShiftsOnlyOnce
 * @brief Even if Run() polls many times while buffer stays below danger (e.g.
 *        during a prolonged download failure), the episode guard must ensure
 *        the thresholds are shifted exactly once for the continuous episode.
 */
TEST_F(AampLatencyMonitorTest, DownloadFailure_SustainedLowBuffer_ShiftsOnlyOnce)
{
	constexpr double kStep = 1000.0;
	// Buffer stays below danger for the entire test — no OnBufferLevelUpdate called.
	ON_CALL(*mMockAamp, GetBufferedDurationSecs()).WillByDefault(Return(0.5));
	ON_CALL(*mMockAamp, GetCurrentLatencyMs()).WillByDefault(Return(kTargetLatencyMs));

	mMonitor->Start(MakeFastConfig(
		DEFAULT_NORMAL_RATE_CORRECTION_SPEED, DEFAULT_MIN_RATE_CORRECTION_SPEED, DEFAULT_MAX_RATE_CORRECTION_SPEED,
		DEFAULT_MIN_LATENCY_MS, DEFAULT_TARGET_LATENCY_MS, DEFAULT_MAX_LATENCY_MS,
		0.0, kStep, /*rebufMaxIncrMs=*/0.0,
		/*dangerBufferMs=*/1000.0, /*latencyStableSec=*/5.0));
	ASSERT_TRUE(WaitForRunning());

	// Wait for the first shift to happen.
	ASSERT_TRUE(WaitForMinLatency(DEFAULT_MIN_LATENCY_MS + kStep, 500));

	// Let Run() poll many more times (monitorIntervalMs=5ms → ~20 more polls).
	std::this_thread::sleep_for(std::chrono::milliseconds(100));

	// Episode guard must have blocked all subsequent shifts.
	auto [minMs, targetMs, maxMs] = mMonitor->GetCurrentThresholds();
	EXPECT_DOUBLE_EQ(minMs,    DEFAULT_MIN_LATENCY_MS    + kStep);
	EXPECT_DOUBLE_EQ(targetMs, DEFAULT_TARGET_LATENCY_MS + kStep);
	EXPECT_DOUBLE_EQ(maxMs,    DEFAULT_MAX_LATENCY_MS    + kStep);
}

/**
 * @test DownloadFailure_OnBufferUpdateBeforeRunPolls_ShiftsOnce
 * @brief If OnBufferLevelUpdate fires (partial fragment) before Run()'s
 *        next scheduled poll, Run() must see the episode guard already set
 *        and skip its own shift — net result is exactly one shift.
 */
TEST_F(AampLatencyMonitorTest, DownloadFailure_OnBufferUpdateBeforeRunPolls_ShiftsOnce)
{
	constexpr double kStep = 1000.0;
	// Buffer below danger — both OnBufferLevelUpdate and Run() will observe it.
	ON_CALL(*mMockAamp, GetBufferedDurationSecs()).WillByDefault(Return(0.5));
	ON_CALL(*mMockAamp, GetCurrentLatencyMs()).WillByDefault(Return(kTargetLatencyMs));

	// Use a longer poll interval so OnBufferLevelUpdate fires first.
	LatencyConfig cfg{
		DEFAULT_NORMAL_RATE_CORRECTION_SPEED, DEFAULT_MIN_RATE_CORRECTION_SPEED,
		DEFAULT_MAX_RATE_CORRECTION_SPEED,
		DEFAULT_MIN_LATENCY_MS, DEFAULT_TARGET_LATENCY_MS, DEFAULT_MAX_LATENCY_MS,
		/*monitorDelay=*/0, /*monitorInterval=*/200, // 200ms interval
		/*bufToEnable=*/0.0, kStep, /*rebufMaxIncrMs=*/0.0,
		/*dangerBufferMs=*/1000.0, /*latencyStableSec=*/5.0};
	mMonitor->Start(cfg);
	ASSERT_TRUE(WaitForRunning());

	// OnBufferLevelUpdate fires before the 200ms poll interval expires.
	// It sees !mBelowDangerShifted → wakes Run() immediately.
	// Run() shifts and sets the guard.
	mMonitor->OnBufferLevelUpdate(500.0);
	ASSERT_TRUE(WaitForMinLatency(DEFAULT_MIN_LATENCY_MS + kStep, 500));

	// The scheduled poll at 200ms fires later; guard is already set → no second shift.
	std::this_thread::sleep_for(std::chrono::milliseconds(300));

	auto [minMs, targetMs, maxMs] = mMonitor->GetCurrentThresholds();
	EXPECT_DOUBLE_EQ(minMs, DEFAULT_MIN_LATENCY_MS + kStep); // exactly one shift
}
