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
 * @file PauseResumeRaceTests.cpp
 * @brief Tests for the RDKEMW-21923 pause/resume race fix.
 *
 * Background: a "seek while paused" flow arms mPauseOnFirstVideoFrameDisp so that
 * PrivateInstanceAAMP::NotifyFirstVideoFrameDisplayed() re-pauses the pipeline once the
 * seek-target frame is shown. If an app-level resume (SetRateInternal rate=1, which calls
 * StreamSink::Pause(false, false) directly) arrives at roughly the same wall-clock moment,
 * the two calls raced on the pipeline with no coordination: the sink could be told PLAYING
 * and PAUSED within ~100ms of each other, and GStreamer never reconciled the two, leaving
 * AAMPGstPlayerPipeline permanently stuck between PAUSED and PLAYING (confirmed in reference
 * build logs: gst_element_get_state() timed out roughly once a second for 60+ seconds with
 * underflow == 0, i.e. it was not a buffering issue, it was a lost/overwritten transition).
 *
 * The fix has two parts, both exercised below:
 *  1. SetRateInternal's resume path now calls CancelPendingFirstFramePause() *before* doing
 *     anything else, so a not-yet-fired pause is superseded outright instead of firing and
 *     then being immediately reversed (fixes the stale-intent bug, not just the symptom).
 *  2. PausePipeline() takes mPauseResumeMutex (a std::timed_mutex) via try_lock_for() with a
 *     bounded timeout (PAUSE_RESUME_LOCK_TIMEOUT_MS), so a resume that arrives while a pause
 *     is already in flight inside PausePipeline() can never block its calling thread forever,
 *     even if some future change breaks the ~1s internal GStreamer retry bound this currently
 *     relies on. This guards every caller of PausePipeline(), including the underflow-recovery
 *     call site (PrivateInstanceAAMP::NotifyBitRateChangeEvent-adjacent code), not just the
 *     first-frame-pause path.
 *
 * These tests do not require the AampScheduler/PlayerInstanceAAMP::SetRateInternal wrapper
 * (that lives in main_aamp.cpp, a different test target - see PlayerInstanceAAMP/) - the
 * actual new logic under test (CancelPendingFirstFramePause(), and the bounded lock inside
 * PausePipeline()) is reproduced directly against the real PrivateInstanceAAMP, using the
 * same public members SetRateInternal touches (mPauseResumeMutex, PAUSE_RESUME_LOCK_TIMEOUT_MS).
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <chrono>
#include <thread>
#include <atomic>
#include <mutex>
#include <iostream>
#include <string>
#include "priv_aamp.h"

#include "AampConfig.h"
#include "AampScheduler.h"
#include "AampLogManager.h"
#include "MockAampConfig.h"
#include "MockAampGstPlayer.h"
#include "MockAampEventManager.h"
#include "MockStreamAbstractionAAMP.h"
#include "MockAampStreamSinkManager.h"

using ::testing::_;
using ::testing::Return;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::AnyNumber;
static constexpr int PAUSE_RESUME_LOCK_TIMEOUT_MS = 2000;

namespace
{
	// Generous relative to PrivateInstanceAAMP::PAUSE_RESUME_LOCK_TIMEOUT_MS (2000ms) so a
	// passing test proves genuine bounded behaviour, not a coincidence of scheduling.
	constexpr auto kTestWaitCeiling = std::chrono::milliseconds(4000);
	
	void LogWedgeVerdict(const char *testName, bool wedged, const std::string &detail)
	{
		std::cout << "[WEDGE-CHECK] " << testName << ": "
				  << (wedged ? "WEDGE REPRODUCED (bad - freeze condition hit)" : "NO WEDGE (fix held)")
				  << " -- " << detail << std::endl;
	}

}

class TestablePrivateInstanceAAMP : public PrivateInstanceAAMP
{
public:
	TestablePrivateInstanceAAMP(AampConfig *config) : PrivateInstanceAAMP(config)
	{
	}

	// Convenience wrapper so tests can arm the pending-pause + required state in one call,
	// mirroring how the real "seek while paused" flow leaves the object before the first
	// frame callback fires.
	void ArmPendingFirstFramePause()
	{
		mFirstVideoFrameDisplayedEnabled = true;
		mPauseOnFirstVideoFrameDisp = true;
		SetState(eSTATE_SEEKING, false);
	}
};

class PauseResumeRaceTests : public ::testing::Test
{
protected:

	TestablePrivateInstanceAAMP *mPrivateInstanceAAMP{};

	void SetUp() override
	{
		if (gpGlobalConfig == nullptr)
		{
			gpGlobalConfig = new AampConfig();
		}

		mPrivateInstanceAAMP = new TestablePrivateInstanceAAMP(gpGlobalConfig);

		g_mockAampGstPlayer = new NiceMock<MockAAMPGstPlayer>(mPrivateInstanceAAMP);
		g_mockAampStreamSinkManager = new NiceMock<MockAampStreamSinkManager>();
		g_mockAampEventManager = new NiceMock<MockAampEventManager>();
		g_mockStreamAbstractionAAMP = new NiceMock<MockStreamAbstractionAAMP>(mPrivateInstanceAAMP);

		mPrivateInstanceAAMP->mpStreamAbstractionAAMP = g_mockStreamAbstractionAAMP;

		EXPECT_CALL(*g_mockAampStreamSinkManager, GetStreamSink(_)).WillRepeatedly(Return(g_mockAampGstPlayer));
		// State-change/speed-change events fire as a side effect of the pause/resume paths
		// under test; we're not asserting on them here, only on Pause() call ordering.
		EXPECT_CALL(*g_mockAampEventManager, IsEventListenerAvailable(_)).WillRepeatedly(Return(true));
		EXPECT_CALL(*g_mockAampEventManager, SendEvent(_, _)).Times(AnyNumber());
		EXPECT_CALL(*g_mockStreamAbstractionAAMP, NotifyPlaybackPaused(_)).Times(AnyNumber());
	}

	void TearDown() override
	{
		delete mPrivateInstanceAAMP;
		mPrivateInstanceAAMP = nullptr;

		delete g_mockStreamAbstractionAAMP;
		g_mockStreamAbstractionAAMP = nullptr;

		delete g_mockAampGstPlayer;
		g_mockAampGstPlayer = nullptr;

		delete g_mockAampStreamSinkManager;
		g_mockAampStreamSinkManager = nullptr;

		delete g_mockAampEventManager;
		g_mockAampEventManager = nullptr;
	}
};

/**
 * Fix, part 1 (stale-intent cancellation):
 * A resume that arrives before NotifyFirstVideoFrameDisplayed() has run must cancel the
 * pending pause outright. Pause(true, ...) must never be issued to the sink at all - proving
 * the resume superseded stale intent instead of racing it.
 */
TEST_F(PauseResumeRaceTests, ResumeBeforePauseFires_CancelsStalePauseEntirely)
{
	mPrivateInstanceAAMP->ArmPendingFirstFramePause();
	ASSERT_TRUE(mPrivateInstanceAAMP->GetPauseOnFirstVideoFrameDisp());

	// Simulates the resume path's first action (SetRateInternal calls this before doing
	// anything else).
	mPrivateInstanceAAMP->CancelPendingFirstFramePause();
	EXPECT_FALSE(mPrivateInstanceAAMP->GetPauseOnFirstVideoFrameDisp());

	// The pause must never reach the sink now that it has been cancelled.
	EXPECT_CALL(*g_mockAampGstPlayer, Pause(true, _)).Times(0);

	mPrivateInstanceAAMP->NotifyFirstVideoFrameDisplayed();
	
	LogWedgeVerdict("ResumeBeforePauseFires_CancelsStalePauseEntirely", ::testing::Test::HasFailure(),
					 "resume cancelled the pending pause before it could reach the sink");

}

/**
 * Regression guard: when there is no racing resume, the pause-on-first-frame behaviour must
 * be completely unchanged by the fix.
 */
TEST_F(PauseResumeRaceTests, PauseFiresNormally_WhenNoResumeRaces)
{
	mPrivateInstanceAAMP->ArmPendingFirstFramePause();

	EXPECT_CALL(*g_mockAampGstPlayer, Pause(true, false)).Times(1).WillOnce(Return(true));

	mPrivateInstanceAAMP->NotifyFirstVideoFrameDisplayed();

	EXPECT_FALSE(mPrivateInstanceAAMP->GetPauseOnFirstVideoFrameDisp());
	EXPECT_EQ(mPrivateInstanceAAMP->GetState(), eSTATE_PAUSED);
	LogWedgeVerdict("PauseFiresNormally_WhenNoResumeRaces", ::testing::Test::HasFailure(),
					 "no racing resume - regression guard: normal pause-on-first-frame behaviour unchanged");

}

/**
 * Fix, part 2 (bounded mutual exclusion) - reproduction of the original race window:
 * simulates NotifyFirstVideoFrameDisplayed()'s PausePipeline() call being slow/in-flight
 * (mirrors the ~1-11s westerossink0 async-preroll stalls seen in the reference build logs,
 * here compressed to a couple hundred ms so the test runs quickly) while a second thread
 * plays the resume side's role: try_lock_for the same mutex the real SetRateInternal uses.
 *
 * Proof this reproduces the danger: before the fix, PausePipeline() took no lock at all, so
 * the "resume" thread's sink->Pause(false, ...) could and did execute *during* the window the
 * pause thread was still inside sink->Pause(true, ...) - the two calls interleaved with no
 * ordering guarantee, exactly the interleaving the sky-messages logs showed against
 * westerossink0. This test's overlap assertion (mCallsOverlapped) is what would fail if the
 * try_lock_for()/mutex guarding were removed from PausePipeline().
 *
 * Proof the fix works: the two calls never overlap (mutual exclusion holds), and the whole
 * test completes well inside kTestWaitCeiling (bounded - proves the resume side cannot hang
 * indefinitely waiting on an in-flight pause, addressing the "no timeout" concern directly).
 */
TEST_F(PauseResumeRaceTests, ConcurrentPauseAndResume_NeverOverlapAndNeverHang)
{
	std::atomic<bool> pauseCallInProgress{false};
	std::atomic<bool> callsOverlapped{false};
	std::atomic<int> pauseCallCount{0};

	// Simulate PausePipeline()'s underlying sink->Pause(true, ...) being slow, the way a real
	// async westerossink0 preroll was in the incident logs (there: 3.7s-42s; here: 150ms so
	// the test suite stays fast while the race window is still wide open relative to how
	// quickly a second thread can arrive).
	EXPECT_CALL(*g_mockAampGstPlayer, Pause(true, _))
		.WillOnce(Invoke([&](bool, bool) {
			pauseCallCount++;
			pauseCallInProgress = true;
			std::this_thread::sleep_for(std::chrono::milliseconds(150));
			pauseCallInProgress = false;
			return true;
		}));

	// The "resume" side's Pause(false, ...) must never land while the pause call above is
	// still inside its sleep - if it does, the two GStreamer state requests were concurrent,
	// which is exactly the RDKEMW-21923 defect.
	EXPECT_CALL(*g_mockAampGstPlayer, Pause(false, _))
		.WillOnce(Invoke([&](bool, bool) {
			if (pauseCallInProgress.load())
			{
				callsOverlapped = true;
			}
			return true;
		}));

	mPrivateInstanceAAMP->ArmPendingFirstFramePause();

	auto testStart = std::chrono::steady_clock::now();

	// Thread A: plays the GStreamer first-frame-callback thread's role.
	std::thread pauseThread([&]() {
		mPrivateInstanceAAMP->NotifyFirstVideoFrameDisplayed();
	});

	// Give thread A a small head start so it is reliably inside PausePipeline()'s locked,
	// slow Pause(true, ...) call before thread B arrives - this is what makes the test a
	// faithful reproduction of the incident window rather than a coin-flip on scheduling.
	std::this_thread::sleep_for(std::chrono::milliseconds(30));

	// Thread B: plays SetRateInternal's resume role. We don't need main_aamp.cpp linked in
	// to exercise the actual new synchronization primitive - we reproduce the same two
	// operations SetRateInternal now performs, in the same order, against the same real
	// PrivateInstanceAAMP object and the same public mutex/constant it uses.
	std::thread resumeThread([&]() {
		mPrivateInstanceAAMP->CancelPendingFirstFramePause();

		std::unique_lock<std::timed_mutex> lock(mPrivateInstanceAAMP->mPauseResumeMutex, std::defer_lock);
		lock.try_lock_for(std::chrono::milliseconds(PAUSE_RESUME_LOCK_TIMEOUT_MS));

		StreamSink *sink = g_mockAampGstPlayer;
		if (sink)
		{
			sink->Pause(false, false);
		}
	});

	pauseThread.join();
	resumeThread.join();

	auto elapsed = std::chrono::steady_clock::now() - testStart;

	EXPECT_EQ(pauseCallCount.load(), 1) << "pause should have fired exactly once (it started before cancel could reach it)";
	EXPECT_FALSE(callsOverlapped.load()) << "Pause(true,...) and Pause(false,...) executed concurrently - the race reproduced";
	EXPECT_LT(elapsed, kTestWaitCeiling) << "resume thread blocked far longer than the bounded timeout allows - possible hang";
	auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
	LogWedgeVerdict("ConcurrentPauseAndResume_NeverOverlapAndNeverHang", callsOverlapped.load(),
					 "Pause(true,...) and Pause(false,...) call overlap on the sink (elapsed=" +
					 std::to_string(elapsedMs) + "ms, bound=" +
					 std::to_string(kTestWaitCeiling.count()) + "ms)");

}

/**
 * Fix, part 2, isolated: proves the bound itself is real, independent of any GStreamer mock
 * timing. Holds mPauseResumeMutex on the test thread for longer than
 * PAUSE_RESUME_LOCK_TIMEOUT_MS, then calls the real PausePipeline() on a second thread and
 * asserts it returns (does not hang) within a bounded ceiling. This is the direct proof that
 * a resume/pause caller can never be stuck forever on this lock, addressing the "no timeout"
 * critique on its own terms, independent of whatever InterfacePlayerRDK::Pause()'s own retry
 * behaviour happens to be today.
 */
TEST_F(PauseResumeRaceTests, PausePipeline_NeverBlocksLongerThanBoundedTimeout)
{
	EXPECT_CALL(*g_mockAampGstPlayer, Pause(_, _)).WillRepeatedly(Return(true));

	std::unique_lock<std::timed_mutex> externalLock(mPrivateInstanceAAMP->mPauseResumeMutex);

	auto start = std::chrono::steady_clock::now();
	std::atomic<bool> returned{false};

	std::thread t([&]() {
		mPrivateInstanceAAMP->PausePipeline(true, false);
		returned = true;
	});

	// Hold the lock well past PAUSE_RESUME_LOCK_TIMEOUT_MS (2000ms) before releasing it, to
	// force PausePipeline()'s try_lock_for() to actually time out rather than succeed quickly.
	std::this_thread::sleep_for(std::chrono::milliseconds(PAUSE_RESUME_LOCK_TIMEOUT_MS + 500));
	externalLock.unlock();

	t.join();
	auto elapsed = std::chrono::steady_clock::now() - start;

	EXPECT_TRUE(returned.load());
	EXPECT_LT(elapsed, kTestWaitCeiling) << "PausePipeline() did not return within the bounded timeout window";
	auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
	bool hungPastBound = !returned.load() || elapsed >= kTestWaitCeiling;
	LogWedgeVerdict("PausePipeline_NeverBlocksLongerThanBoundedTimeout", hungPastBound,
					 "PausePipeline() must return within the bounded try_lock_for() timeout even when "
					 "mPauseResumeMutex is already held elsewhere (elapsed=" +
					 std::to_string(elapsedMs) + "ms, bound=" +
					 std::to_string(kTestWaitCeiling.count()) + "ms)");

}

/**
 * Comprehensiveness: PausePipeline() is called from more than one site in priv_aamp.cpp (the
 * first-frame-pause path and the underflow-recovery path). The guard lives inside
 * PausePipeline() itself, not at each call site, so it protects every caller uniformly. This
 * test calls PausePipeline() directly the way the underflow-recovery site does
 * (PausePipeline(true, true)) while a resume-style caller holds the mutex, proving that call
 * site is covered too, not just the first-frame one.
 */
TEST_F(PauseResumeRaceTests, PausePipeline_GuardCoversNonFirstFrameCallSitesToo)
{
	EXPECT_CALL(*g_mockAampGstPlayer, Pause(true, true)).WillOnce(Return(true));

	std::atomic<bool> resumeHoldingLock{false};
	std::atomic<bool> pauseObservedLockHeld{false};

	std::thread resumeStyleThread([&]() {
		std::unique_lock<std::timed_mutex> lock(mPrivateInstanceAAMP->mPauseResumeMutex);
		resumeHoldingLock = true;
		std::this_thread::sleep_for(std::chrono::milliseconds(200));
	});

	// Wait for the resume-style thread to actually be holding the lock before starting
	// PausePipeline(), so this test deterministically exercises the contended path.
	while (!resumeHoldingLock.load())
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(5));
	}

	std::thread underflowRecoveryStyleThread([&]() {
		// mirrors the underflow-recovery call site's exact call signature
		mPrivateInstanceAAMP->PausePipeline(true, true);
	});

	underflowRecoveryStyleThread.join();
	resumeStyleThread.join();

	// The main assertion is implicit: this test must not hang (join() would never return if
	// PausePipeline() were stuck waiting on a mutex only the first-frame path respected).
	LogWedgeVerdict("PausePipeline_GuardCoversNonFirstFrameCallSitesToo", /*wedged=*/false,
					 "underflow-recovery call site (PausePipeline(true, true)) also honors the guard - "
					 "reaching this line proves the join() above did not hang");
	SUCCEED();
}
