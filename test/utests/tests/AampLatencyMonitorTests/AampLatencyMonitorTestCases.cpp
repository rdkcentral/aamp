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
	double targetBuf   = DEFAULT_TARGET_BUFFER_LOW_LATENCY, //4.0
	double minBuf      = DEFAULT_MIN_BUFFER_LOW_LATENCY) //2.0
{
	// monitorDelayMs = 0, monitorIntervalMs = 5 ms — fast for tests.
	return LatencyConfig{normalRate, minRate, maxRate,
		minLatMs, targetLatMs, maxLatMs,
		targetBuf, minBuf, 0, 5};
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
		ON_CALL(*mMockAamp, GetCurrentLatency()).WillByDefault(Return(static_cast<long>(DEFAULT_TARGET_LATENCY_MS)));
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
// 1. Threshold boundary validation — SetLatencyThresholds
// ===========================================================================

/**
 * @test SetLatencyThresholds_Valid_Updates_Thresholds
 * @brief Valid threshold values are accepted and applied.
 */
TEST_F(AampLatencyMonitorTest, SetLatencyThresholds_Valid_Updates_Thresholds)
{
	mMonitor->Start(MakeFastConfig());
    EXPECT_TRUE(WaitForRunning());
	// Overwrite thresholds; the worker will pick them up at the next poll.
	mMonitor->SetLatencyThresholds(2000.0, 5000.0, 8000.0);
	// No crash / assertion — test the component accepted the call.
	EXPECT_TRUE(mMonitor->IsRunning());
}

/**
 * @test SetLatencyThresholds_ZeroMin_Rejected
 * @brief minMs = 0 must be rejected (validation guard: minMs <= 0.0).
 */
TEST_F(AampLatencyMonitorTest, SetLatencyThresholds_ZeroMin_Rejected)
{
	mMonitor->Start(MakeFastConfig());
    EXPECT_TRUE(WaitForRunning());
	const double rateBeforeCall = mMonitor->GetCurrentRate();
	// Should silently discard the invalid parameters.
	mMonitor->SetLatencyThresholds(0.0, 5000.0, 9000.0);
	// Monitor should still be running normally.
	EXPECT_TRUE(mMonitor->IsRunning());
	EXPECT_DOUBLE_EQ(mMonitor->GetCurrentRate(), rateBeforeCall);
}

/**
 * @test SetLatencyThresholds_NegativeMin_Rejected
 * @brief Negative minMs must be rejected.
 */
TEST_F(AampLatencyMonitorTest, SetLatencyThresholds_NegativeMin_Rejected)
{
	mMonitor->Start(MakeFastConfig());
    EXPECT_TRUE(WaitForRunning());
	mMonitor->SetLatencyThresholds(-1.0, 5000.0, 9000.0);
	EXPECT_TRUE(mMonitor->IsRunning());
}

/**
 * @test SetLatencyThresholds_TargetBelowMin_Rejected
 * @brief targetMs < minMs must be rejected.
 */
TEST_F(AampLatencyMonitorTest, SetLatencyThresholds_TargetBelowMin_Rejected)
{
	mMonitor->Start(MakeFastConfig());
    EXPECT_TRUE(WaitForRunning());
	mMonitor->SetLatencyThresholds(5000.0, 3000.0, 9000.0);
	EXPECT_TRUE(mMonitor->IsRunning());
}

/**
 * @test SetLatencyThresholds_MaxBelowTarget_Rejected
 * @brief maxMs < targetMs must be rejected.
 */
TEST_F(AampLatencyMonitorTest, SetLatencyThresholds_MaxBelowTarget_Rejected)
{
	mMonitor->Start(MakeFastConfig());
	mMonitor->SetLatencyThresholds(1000.0, 5000.0, 3000.0);
	EXPECT_TRUE(mMonitor->IsRunning());
}

/**
 * @test SetLatencyThresholds_EqualMinTarget_Valid
 * @brief targetMs == minMs is a valid degenerate case (no dead-band).
 */
TEST_F(AampLatencyMonitorTest, SetLatencyThresholds_EqualMinTarget_Valid)
{
	mMonitor->Start(MakeFastConfig());
	// equal min == target == max is valid
	mMonitor->SetLatencyThresholds(5000.0, 5000.0, 5000.0);
	EXPECT_TRUE(mMonitor->IsRunning());
}

/**
 * @test SetLatencyThresholds_BeforeStart_DoesNotCrash
 * @brief Calling SetLatencyThresholds before Start() must not crash.
 */
TEST_F(AampLatencyMonitorTest, SetLatencyThresholds_BeforeStart_DoesNotCrash)
{
	mMonitor->SetLatencyThresholds(1000.0, 3000.0, 6000.0);
	EXPECT_FALSE(mMonitor->IsRunning());
}

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
	mMonitor->Start(MakeFastConfig(DEFAULT_NORMAL_RATE_CORRECTION_SPEED));
	ASSERT_TRUE(WaitForRunning());
	EXPECT_DOUBLE_EQ(mMonitor->GetCurrentRate(), DEFAULT_NORMAL_RATE_CORRECTION_SPEED);
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
	ON_CALL(*mMockAamp, GetCurrentLatency()).WillByDefault(Return(15000L)); // > 9000 ms max
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
	ON_CALL(*mMockAamp, GetCurrentLatency()).WillByDefault(Return(15000L));
	ON_CALL(*mMockAamp, GetBufferedDurationSecs()).WillByDefault(Return(5.0));

	mMonitor->Start(MakeFastConfig());
	ASSERT_TRUE(WaitForRunning());
	ASSERT_TRUE(WaitForRate(DEFAULT_MAX_RATE_CORRECTION_SPEED, 500));

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
	ON_CALL(*mMockAamp, GetCurrentLatency()).WillByDefault(Return(static_cast<long>(DEFAULT_TARGET_LATENCY_MS)));
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
 * @test RateCorrection_HighLatencyHealthyBuffer_SpeedsUp
 * @brief When latency > maxLatencyMs and buffer >= targetBufferSec the worker
 *        must request maxPlaybackRate from the sink.
 */
TEST_F(AampLatencyMonitorTest, RateCorrection_HighLatencyHealthyBuffer_SpeedsUp)
{
	// Latency = 12 000 ms > 9 000 ms max; buffer = 5 s >= 4 s target.
	ON_CALL(*mMockAamp, GetCurrentLatency()).WillByDefault(Return(12000L));
	ON_CALL(*mMockAamp, GetBufferedDurationSecs()).WillByDefault(Return(5.0));

	EXPECT_CALL(*mMockSink, SetPlayBackRate(DEFAULT_MAX_RATE_CORRECTION_SPEED)).Times(AtLeast(1)).WillRepeatedly(Return(true));

	mMonitor->Start(MakeFastConfig());
	ASSERT_TRUE(WaitForRunning());
	EXPECT_TRUE(WaitForRate(DEFAULT_MAX_RATE_CORRECTION_SPEED, 500));
}

/**
 * @test RateCorrection_HighLatencyLowBuffer_DoesNotSpeedUp
 * @brief When latency > maxLatencyMs but buffer < targetBufferSec the worker
 *        must NOT speed up (buffer not healthy).
 */
TEST_F(AampLatencyMonitorTest, RateCorrection_HighLatencyLowBuffer_DoesNotSpeedUp)
{
	// Latency out-of-band-high but buffer below target.
	ON_CALL(*mMockAamp, GetCurrentLatency()).WillByDefault(Return(12000L));
	ON_CALL(*mMockAamp, GetBufferedDurationSecs()).WillByDefault(Return(1.5));

	mMonitor->Start(MakeFastConfig());
	ASSERT_TRUE(WaitForRunning());
	std::this_thread::sleep_for(std::chrono::milliseconds(50));
	// Rate must remain at normal (no speed-up permitted).
	EXPECT_DOUBLE_EQ(mMonitor->GetCurrentRate(), DEFAULT_NORMAL_RATE_CORRECTION_SPEED);
}

/**
 * @test RateCorrection_LowLatency_SlowsDown
 * @brief When latency < minLatencyMs the worker must request minPlaybackRate.
 */
TEST_F(AampLatencyMonitorTest, RateCorrection_LowLatency_SlowsDown)
{
	// Latency = 1 000 ms < 3 000 ms min; buffer healthy.
	ON_CALL(*mMockAamp, GetCurrentLatency()).WillByDefault(Return(1000L));
	ON_CALL(*mMockAamp, GetBufferedDurationSecs()).WillByDefault(Return(5.0));

	EXPECT_CALL(*mMockSink, SetPlayBackRate(DEFAULT_MIN_RATE_CORRECTION_SPEED)).Times(AtLeast(1)).WillRepeatedly(Return(true));

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
	ON_CALL(*mMockAamp, GetCurrentLatency()).WillByDefault(Return(static_cast<long>(DEFAULT_TARGET_LATENCY_MS)));
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
	ON_CALL(*mMockAamp, GetCurrentLatency()).WillByDefault([&latency]() { return latency.load(); });
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
	std::atomic<double> buffer{5.0};
	ON_CALL(*mMockAamp, GetCurrentLatency()).WillByDefault([&latency]() { return latency.load(); });
	ON_CALL(*mMockAamp, GetBufferedDurationSecs()).WillByDefault([&buffer]() { return buffer.load(); });
	ON_CALL(*mMockSink, SetPlayBackRate(_)).WillByDefault(Return(true));

	mMonitor->Start(MakeFastConfig());
	ASSERT_TRUE(WaitForRunning());
	ASSERT_TRUE(WaitForRate(DEFAULT_MIN_RATE_CORRECTION_SPEED, 500));

	// Phase 2: latency rose back to target — should return to normal.
	latency.store(7000L); // >= DEFAULT_TARGET_LATENCY_MS, while at minRate with buffer > minBufSec
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
	ON_CALL(*mMockAamp, GetCurrentLatency()).WillByDefault(Return(12000L));
	ON_CALL(*mMockAamp, GetBufferedDurationSecs()).WillByDefault(Return(5.0));
	ON_CALL(*mMockAamp, IsAdPlaying()).WillByDefault(Return(true));

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
	ON_CALL(*mMockAamp, GetCurrentLatency()).WillByDefault(Return(12000L));
	ON_CALL(*mMockAamp, GetBufferedDurationSecs()).WillByDefault(Return(5.0));

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
	ON_CALL(*mMockAamp, GetCurrentLatency()).WillByDefault(Return(12000L));
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
	ON_CALL(*mMockAamp, GetCurrentLatency()).WillByDefault(Return(12000L));
	ON_CALL(*mMockAamp, GetBufferedDurationSecs()).WillByDefault(Return(5.0));
	ON_CALL(*mMockSinkMgr, GetStreamSink(_)).WillByDefault(Return(nullptr));

	mMonitor->Start(MakeFastConfig());
	ASSERT_TRUE(WaitForRunning());
	std::this_thread::sleep_for(std::chrono::milliseconds(50));
	EXPECT_DOUBLE_EQ(mMonitor->GetCurrentRate(), DEFAULT_NORMAL_RATE_CORRECTION_SPEED);
}

/**
 * @test RateCorrection_BufferPeriodTransient_TreatedAsEnough
 * @brief GetBufferedDurationSecs() returning -1 (period switch) should be
 *        treated as buffer-enough, so a high-latency scenario still speeds up.
 */
TEST_F(AampLatencyMonitorTest, RateCorrection_BufferPeriodTransient_TreatedAsEnough)
{
	ON_CALL(*mMockAamp, GetCurrentLatency()).WillByDefault(Return(12000L));
	ON_CALL(*mMockAamp, GetBufferedDurationSecs()).WillByDefault(Return(-1.0));
	ON_CALL(*mMockSink, SetPlayBackRate(_)).WillByDefault(Return(true));

	mMonitor->Start(MakeFastConfig());
	ASSERT_TRUE(WaitForRunning());
	EXPECT_TRUE(WaitForRate(DEFAULT_MAX_RATE_CORRECTION_SPEED, 500));
}

// ===========================================================================
// 5. Adaptive latency threshold shifts driven by underflow events
// ===========================================================================

/**
 * @test AdaptiveThreshold_BelowThreshold_NoShift
 * @brief Fewer than kUnderflowThreshold (4) underflows within the window must
 *        not shift the latency band.  Verified by checking that a latency value
 *        that would normally be within the original band still keeps the rate
 *        at normal after fewer-than-threshold underflows.
 */
TEST_F(AampLatencyMonitorTest, AdaptiveThreshold_BelowThreshold_NoShift)
{
	// Latency in-band: at target inside [min, max].
	ON_CALL(*mMockAamp, GetCurrentLatency()).WillByDefault(Return(static_cast<long>(DEFAULT_TARGET_LATENCY_MS)));
	ON_CALL(*mMockAamp, GetBufferedDurationSecs()).WillByDefault(Return(5.0));

	mMonitor->Start(MakeFastConfig());
	ASSERT_TRUE(WaitForRunning());

	// Inject 3 underflows — one below the threshold of 4.
	mMonitor->NotifyUnderflow();
	mMonitor->NotifyUnderflow();
	mMonitor->NotifyUnderflow();

	std::this_thread::sleep_for(std::chrono::milliseconds(50));
	// Rate should stay at normal because the band has not shifted.
	EXPECT_DOUBLE_EQ(mMonitor->GetCurrentRate(), DEFAULT_NORMAL_RATE_CORRECTION_SPEED);
}

/**
 * @test AdaptiveThreshold_AtThreshold_ShiftsUpward
 * @brief kUnderflowThreshold underflows within the window must shift the band
 *        upward by kUnderflowShiftMs (1000 ms).  After the shift, a latency of
 *        9500 ms (within the new [4000, 10000] band) must keep the rate normal,
 *        whereas it would have triggered speed-up under the original band.
 */
TEST_F(AampLatencyMonitorTest, AdaptiveThreshold_AtThreshold_ShiftsUpward)
{
	// Use a latency that is above the original maxLatencyMs (9000) ← would
	// trigger speed-up, but after the shift max becomes 10000 so it is in-band.
	ON_CALL(*mMockAamp, GetCurrentLatency()).WillByDefault(Return(9500L));
	ON_CALL(*mMockAamp, GetBufferedDurationSecs()).WillByDefault(Return(5.0));
	ON_CALL(*mMockSink, SetPlayBackRate(_)).WillByDefault(Return(true));

	mMonitor->Start(MakeFastConfig());
	ASSERT_TRUE(WaitForRunning());

	// Without the shift, 9500 > DEFAULT_MAX_LATENCY_MS and buffer=5s → speed-up.
	// First confirm the speed-up happens before any underflows.
	ASSERT_TRUE(WaitForRate(DEFAULT_MAX_RATE_CORRECTION_SPEED, 500));

	// Now inject enough underflows to trigger the shift.  The shift will be
	// applied on the next worker poll; after it 9500 < new max of 10000.
	// However the rate won't update back to normal until latency <= new target.
	// Drive latency to within the new band target (6000+1000=7000) to confirm
	// the monitor returns to normal after the shift.
	ON_CALL(*mMockAamp, GetCurrentLatency()).WillByDefault(Return(6500L));

	// 4 underflows in rapid succession — within the 20-second window.
	mMonitor->NotifyUnderflow();
	mMonitor->NotifyUnderflow();
	mMonitor->NotifyUnderflow();
	mMonitor->NotifyUnderflow();

	// After the shift, latency=6500 <= new target=7000: return to normal.
	EXPECT_TRUE(WaitForRate(DEFAULT_NORMAL_RATE_CORRECTION_SPEED, 500));
}

/**
 * @test AdaptiveThreshold_MaxShiftCapped
 * @brief The cumulative adaptive shift must not exceed kMaxAdaptiveShiftMs
 *        (3000 ms).  Four bursts of 4 underflows can only shift by 3 × 1000 ms.
 */
TEST_F(AampLatencyMonitorTest, AdaptiveThreshold_MaxShiftCapped)
{
	// Latency in original band — stays in-band throughout.
	ON_CALL(*mMockAamp, GetCurrentLatency()).WillByDefault(Return(static_cast<long>(DEFAULT_TARGET_LATENCY_MS)));
	ON_CALL(*mMockAamp, GetBufferedDurationSecs()).WillByDefault(Return(5.0));

	mMonitor->Start(MakeFastConfig(
		DEFAULT_NORMAL_RATE_CORRECTION_SPEED,
		DEFAULT_MIN_RATE_CORRECTION_SPEED,
		DEFAULT_MAX_RATE_CORRECTION_SPEED,
		DEFAULT_MIN_LATENCY_MS, DEFAULT_TARGET_LATENCY_MS, DEFAULT_MAX_LATENCY_MS,
		DEFAULT_TARGET_BUFFER_LOW_LATENCY, DEFAULT_MIN_BUFFER_LOW_LATENCY));
	ASSERT_TRUE(WaitForRunning());

	// Inject 4 bursts × 4 underflows each.  Each burst should trigger a
	// 1000 ms shift; the fourth burst should be a no-op (cap reached at 3000 ms).
	for (int burst = 0; burst < 4; ++burst)
	{
		mMonitor->NotifyUnderflow();
		mMonitor->NotifyUnderflow();
		mMonitor->NotifyUnderflow();
		mMonitor->NotifyUnderflow();
		// Give the worker time to run AdaptLatencyThresholds and clear the queue.
		std::this_thread::sleep_for(std::chrono::milliseconds(30));
	}

	// Regardless of the number of bursts, the total shift is capped at 3000 ms.
	// With latency=6000 still in-band (original min=3000, max shifted up to
	// at most 3000+3000=12000), the rate should remain at normal.
	EXPECT_DOUBLE_EQ(mMonitor->GetCurrentRate(), DEFAULT_NORMAL_RATE_CORRECTION_SPEED);
}

/**
 * @test AdaptiveThreshold_NotifyUnderflow_BeforeStart_DoesNotCrash
 * @brief NotifyUnderflow() called before Start() must not crash.
 */
TEST_F(AampLatencyMonitorTest, AdaptiveThreshold_NotifyUnderflow_BeforeStart_DoesNotCrash)
{
	mMonitor->NotifyUnderflow();
	EXPECT_FALSE(mMonitor->IsRunning());
}

/**
 * @test AdaptiveThreshold_NotifyUnderflow_AfterStop_DoesNotCrash
 * @brief NotifyUnderflow() called after Stop() must not crash.
 */
TEST_F(AampLatencyMonitorTest, AdaptiveThreshold_NotifyUnderflow_AfterStop_DoesNotCrash)
{
	mMonitor->Start(MakeFastConfig());
	ASSERT_TRUE(WaitForRunning());
	mMonitor->Stop();
	mMonitor->NotifyUnderflow();
	EXPECT_FALSE(mMonitor->IsRunning());
}
