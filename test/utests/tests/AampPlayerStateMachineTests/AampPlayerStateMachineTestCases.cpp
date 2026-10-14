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
 * @file AampPlayerStateMachineTestCases.cpp
 * @brief L1 unit tests for PlayerStateMachine (GoF State pattern).
 *
 * Each test constructs a fresh PlayerStateMachine and drives it through
 * defined event sequences, asserting current state after each event.
 * No external dependencies — the state machine is pure business logic.
 */

#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <future>
#include <thread>
#include "AampPlayerStateMachine.h"

// ---------------------------------------------------------------------------
// Fixture
// ---------------------------------------------------------------------------

/**
 * @class AampPlayerStateMachineTest
 * @brief Fresh PlayerStateMachine per test case.
 */
class AampPlayerStateMachineTest : public ::testing::Test
{
protected:
	PlayerStateMachine m_sm;
};

// ===========================================================================
// Initial state
// ===========================================================================

/**
 * @test A newly constructed machine starts in the IDLE state.
 */
TEST_F(AampPlayerStateMachineTest, InitialState_IsIdle)
{
	EXPECT_EQ(m_sm.currentState(), PlayerStateId::IDLE);
}

/**
 * @test currentStateName() returns a non-empty string.
 */
TEST_F(AampPlayerStateMachineTest, InitialStateName_IsNonEmpty)
{
	EXPECT_STRNE(m_sm.currentStateName(), "");
}

// ===========================================================================
// Normal playback lifecycle transitions
// ===========================================================================

/**
 * @test IDLE + onPipelineLoaded → PIPELINE_CREATED.
 */
TEST_F(AampPlayerStateMachineTest, OnPipelineLoaded_FromIdle_TransitionsToPipelineCreated)
{
	m_sm.onPipelineLoaded();
	EXPECT_EQ(m_sm.currentState(), PlayerStateId::PIPELINE_CREATED);
}

/**
 * @test PIPELINE_CREATED + onSourceAttaching → SOURCES_ATTACHING.
 */
TEST_F(AampPlayerStateMachineTest, OnSourceAttaching_FromPipelineCreated_TransitionsToSourcesAttaching)
{
	m_sm.onPipelineLoaded();
	m_sm.onSourceAttaching();
	EXPECT_EQ(m_sm.currentState(), PlayerStateId::SOURCES_ATTACHING);
}

/**
 * @test SOURCES_ATTACHING + onAllSourcesAttached → SOURCES_ATTACHED.
 */
TEST_F(AampPlayerStateMachineTest, OnAllSourcesAttached_FromSourcesAttaching_TransitionsToSourcesAttached)
{
	m_sm.onPipelineLoaded();
	m_sm.onSourceAttaching();
	m_sm.onAllSourcesAttached();
	EXPECT_EQ(m_sm.currentState(), PlayerStateId::SOURCES_ATTACHED);
}

/**
 * @test SOURCES_ATTACHED + onPlaybackStarted → PLAYING.
 */
TEST_F(AampPlayerStateMachineTest, OnPlaybackStarted_FromSourcesAttached_TransitionsToPlaying)
{
	m_sm.onPipelineLoaded();
	m_sm.onSourceAttaching();
	m_sm.onAllSourcesAttached();
	m_sm.onPlaybackStarted();
	EXPECT_EQ(m_sm.currentState(), PlayerStateId::PLAYING);
}

/**
 * @test SOURCES_ATTACHED + onPlaybackPaused → PAUSED (pipeline paused for
 *       pre-roll before play() is issued).
 */
TEST_F(AampPlayerStateMachineTest, OnPlaybackPaused_FromSourcesAttached_TransitionsToPaused)
{
	m_sm.onPipelineLoaded();
	m_sm.onSourceAttaching();
	m_sm.onAllSourcesAttached();
	m_sm.onPlaybackPaused();
	EXPECT_EQ(m_sm.currentState(), PlayerStateId::PAUSED);
}

/**
 * @test PLAYING + onPlaybackPaused → PAUSED.
 */
TEST_F(AampPlayerStateMachineTest, OnPlaybackPaused_FromPlaying_TransitionsToPaused)
{
	m_sm.onPipelineLoaded();
	m_sm.onSourceAttaching();
	m_sm.onAllSourcesAttached();
	m_sm.onPlaybackStarted();
	m_sm.onPlaybackPaused();
	EXPECT_EQ(m_sm.currentState(), PlayerStateId::PAUSED);
}

/**
 * @test PAUSED + onPlaybackStarted → PLAYING (resume after pause).
 */
TEST_F(AampPlayerStateMachineTest, OnPlaybackStarted_FromPaused_TransitionsToPlaying)
{
	m_sm.onPipelineLoaded();
	m_sm.onSourceAttaching();
	m_sm.onAllSourcesAttached();
	m_sm.onPlaybackStarted();
	m_sm.onPlaybackPaused();
	m_sm.onPlaybackStarted();
	EXPECT_EQ(m_sm.currentState(), PlayerStateId::PLAYING);
}

// ===========================================================================
// Flush transitions
// ===========================================================================

/**
 * @test SOURCES_ATTACHED + onFlush → FLUSHING.
 */
TEST_F(AampPlayerStateMachineTest, OnFlush_FromSourcesAttached_TransitionsToFlushing)
{
	m_sm.onPipelineLoaded();
	m_sm.onSourceAttaching();
	m_sm.onAllSourcesAttached();
	m_sm.onFlush();
	EXPECT_EQ(m_sm.currentState(), PlayerStateId::FLUSHING);
}

/**
 * @test PLAYING + onFlush → FLUSHING.
 */
TEST_F(AampPlayerStateMachineTest, OnFlush_FromPlaying_TransitionsToFlushing)
{
	m_sm.onPipelineLoaded();
	m_sm.onSourceAttaching();
	m_sm.onAllSourcesAttached();
	m_sm.onPlaybackStarted();
	m_sm.onFlush();
	EXPECT_EQ(m_sm.currentState(), PlayerStateId::FLUSHING);
}

/**
 * @test PAUSED + onFlush → FLUSHING.
 */
TEST_F(AampPlayerStateMachineTest, OnFlush_FromPaused_TransitionsToFlushing)
{
	m_sm.onPipelineLoaded();
	m_sm.onSourceAttaching();
	m_sm.onAllSourcesAttached();
	m_sm.onPlaybackStarted();
	m_sm.onPlaybackPaused();
	m_sm.onFlush();
	EXPECT_EQ(m_sm.currentState(), PlayerStateId::FLUSHING);
}

/**
 * @test onSourceAttaching from FLUSHING leaves the machine in FLUSHING.
 *
 * There is no direct FLUSHING→SOURCES_ATTACHING path; a pipeline rebuild
 * always goes through onReconfigure (→IDLE) first.  Another source
 * attaching is not an error, so it is ignored without a WARN.
 */
TEST_F(AampPlayerStateMachineTest, OnSourceAttaching_FromFlushing_IsIgnored)
{
	m_sm.onPipelineLoaded();
	m_sm.onSourceAttaching();
	m_sm.onAllSourcesAttached();
	m_sm.onFlush();
	m_sm.onSourceAttaching();   // ignored
	EXPECT_EQ(m_sm.currentState(), PlayerStateId::FLUSHING);
}

/**
 * @test The second and third source attaching after the first moved the
 *       machine to SOURCES_ATTACHING is normal and must not log a WARN.
 */
TEST_F(AampPlayerStateMachineTest, OnSourceAttaching_WhileAlreadyAttaching_DoesNotWarn)
{
	m_sm.onPipelineLoaded();
	m_sm.onSourceAttaching();

	testing::internal::CaptureStdout();
	m_sm.onSourceAttaching();
	m_sm.onSourceAttaching();
	const std::string log = testing::internal::GetCapturedStdout();

	EXPECT_EQ(m_sm.currentState(), PlayerStateId::SOURCES_ATTACHING);
	EXPECT_EQ(log.find("[WARN]"), std::string::npos) << log;
}

/**
 * @test A source attaching after all sources attached (a deferred source,
 *       or one added while playing) leaves the state alone without a WARN.
 */
TEST_F(AampPlayerStateMachineTest, OnSourceAttaching_AfterAllAttached_DoesNotWarn)
{
	m_sm.onPipelineLoaded();
	m_sm.onSourceAttaching();
	m_sm.onAllSourcesAttached();
	m_sm.onPlaybackStarted();

	testing::internal::CaptureStdout();
	m_sm.onSourceAttaching();
	const std::string log = testing::internal::GetCapturedStdout();

	EXPECT_EQ(m_sm.currentState(), PlayerStateId::PLAYING);
	EXPECT_EQ(log.find("[WARN]"), std::string::npos) << log;
}

/**
 * @test onSourceAttaching with no pipeline (IDLE) is unexpected and still
 *       logs a WARN, leaving the machine in IDLE.
 */
TEST_F(AampPlayerStateMachineTest, OnSourceAttaching_FromIdle_StillWarns)
{
	testing::internal::CaptureStdout();
	m_sm.onSourceAttaching();
	const std::string log = testing::internal::GetCapturedStdout();

	EXPECT_EQ(m_sm.currentState(), PlayerStateId::IDLE);
	EXPECT_NE(log.find("[WARN]"), std::string::npos) << log;
}

/**
 * @test onSourceAttaching from ERROR is unexpected and still logs a WARN.
 */
TEST_F(AampPlayerStateMachineTest, OnSourceAttaching_FromError_StillWarns)
{
	m_sm.onPipelineLoaded();
	m_sm.onError();

	testing::internal::CaptureStdout();
	m_sm.onSourceAttaching();
	const std::string log = testing::internal::GetCapturedStdout();

	EXPECT_EQ(m_sm.currentState(), PlayerStateId::ERROR);
	EXPECT_NE(log.find("[WARN]"), std::string::npos) << log;
}

/**
 * @test Stop() on an idle player (including one already stopped) is not an
 *       error: the machine stays IDLE and nothing is logged as a WARN.
 */
TEST_F(AampPlayerStateMachineTest, OnStop_FromIdle_DoesNotWarn)
{
	testing::internal::CaptureStdout();
	m_sm.onStop();
	const std::string log = testing::internal::GetCapturedStdout();

	EXPECT_EQ(m_sm.currentState(), PlayerStateId::IDLE);
	EXPECT_EQ(log.find("[WARN]"), std::string::npos) << log;
}

/**
 * @test FLUSHING + onPlaybackStarted is ignored (no transition defined).
 *
 * Rialto guarantees SEEK_DONE is always sent before any subsequent
 * PLAYING/PAUSED notification for the same flush cycle, so a PLAYING
 * notification cannot legitimately arrive while still FLUSHING.  Should
 * this assumption ever be violated, dispatch() logs a WARN instead of
 * silently mishandling the event.
 */
TEST_F(AampPlayerStateMachineTest, OnPlaybackStarted_FromFlushing_IsIgnored)
{
	m_sm.onPipelineLoaded();
	m_sm.onSourceAttaching();
	m_sm.onAllSourcesAttached();
	m_sm.onFlush();
	ASSERT_EQ(m_sm.currentState(), PlayerStateId::FLUSHING);

	m_sm.onPlaybackStarted();   // no transition defined — dispatch warns
	EXPECT_EQ(m_sm.currentState(), PlayerStateId::FLUSHING);
}

/**
 * @test FLUSHING + onPlaybackPaused is ignored (no transition defined).
 *
 * Same reasoning as OnPlaybackStarted_FromFlushing_IsIgnored above.
 */
TEST_F(AampPlayerStateMachineTest, OnPlaybackPaused_FromFlushing_IsIgnored)
{
	m_sm.onPipelineLoaded();
	m_sm.onSourceAttaching();
	m_sm.onAllSourcesAttached();
	m_sm.onPlaybackStarted();
	m_sm.onPlaybackPaused();
	m_sm.onFlush();
	ASSERT_EQ(m_sm.currentState(), PlayerStateId::FLUSHING);

	m_sm.onPlaybackPaused();    // no transition defined — dispatch warns
	EXPECT_EQ(m_sm.currentState(), PlayerStateId::FLUSHING);
}

// ===========================================================================
// Cross-state events: onStop — Stop() waits for FLUSHING before dispatching
// so FLUSHING is never the source state in production; onStop() goes to IDLE.
// ===========================================================================

/**
 * @test onStop from PIPELINE_CREATED → IDLE.
 */
TEST_F(AampPlayerStateMachineTest, OnStop_FromPipelineCreated_TransitionsToIdle)
{
	m_sm.onPipelineLoaded();
	m_sm.onStop();
	EXPECT_EQ(m_sm.currentState(), PlayerStateId::IDLE);
}

/**
 * @test onStop from SOURCES_ATTACHING → IDLE.
 */
TEST_F(AampPlayerStateMachineTest, OnStop_FromSourcesAttaching_TransitionsToIdle)
{
	m_sm.onPipelineLoaded();
	m_sm.onSourceAttaching();
	m_sm.onStop();
	EXPECT_EQ(m_sm.currentState(), PlayerStateId::IDLE);
}

/**
 * @test onStop from SOURCES_ATTACHED → IDLE.
 */
TEST_F(AampPlayerStateMachineTest, OnStop_FromSourcesAttached_TransitionsToIdle)
{
	m_sm.onPipelineLoaded();
	m_sm.onSourceAttaching();
	m_sm.onAllSourcesAttached();
	m_sm.onStop();
	EXPECT_EQ(m_sm.currentState(), PlayerStateId::IDLE);
}

/**
 * @test onStop from PLAYING → IDLE.
 */
TEST_F(AampPlayerStateMachineTest, OnStop_FromPlaying_TransitionsToIdle)
{
	m_sm.onPipelineLoaded();
	m_sm.onSourceAttaching();
	m_sm.onAllSourcesAttached();
	m_sm.onPlaybackStarted();
	m_sm.onStop();
	EXPECT_EQ(m_sm.currentState(), PlayerStateId::IDLE);
}

/**
 * @test onStop from PAUSED → IDLE.
 */
TEST_F(AampPlayerStateMachineTest, OnStop_FromPaused_TransitionsToIdle)
{
	m_sm.onPipelineLoaded();
	m_sm.onSourceAttaching();
	m_sm.onAllSourcesAttached();
	m_sm.onPlaybackStarted();
	m_sm.onPlaybackPaused();
	m_sm.onStop();
	EXPECT_EQ(m_sm.currentState(), PlayerStateId::IDLE);
}

/**
 * @test onStop from FLUSHING is ignored — FlushingState has no onStop override.
 *
 * Stop() always calls WaitForFlushToComplete() before dispatching onStop(),
 * so FLUSHING is never the current state when onStop is dispatched in
 * production.  The dispatch layer emits a WARN and the machine stays in
 * FLUSHING; it is not a valid no-arg transition here.
 */
TEST_F(AampPlayerStateMachineTest, OnStop_FromFlushing_IsIgnored)
{
	m_sm.onPipelineLoaded();
	m_sm.onSourceAttaching();
	m_sm.onAllSourcesAttached();
	m_sm.onFlush();
	m_sm.onStop();              // no transition defined — dispatch warns
	EXPECT_EQ(m_sm.currentState(), PlayerStateId::FLUSHING);
}

// ===========================================================================
// Cross-state events: onError
// ===========================================================================

/**
 * @test onError from PLAYING → ERROR.
 */
TEST_F(AampPlayerStateMachineTest, OnError_FromPlaying_TransitionsToError)
{
	m_sm.onPipelineLoaded();
	m_sm.onSourceAttaching();
	m_sm.onAllSourcesAttached();
	m_sm.onPlaybackStarted();
	m_sm.onError();
	EXPECT_EQ(m_sm.currentState(), PlayerStateId::ERROR);
}

/**
 * @test onError from SOURCES_ATTACHED → ERROR.
 */
TEST_F(AampPlayerStateMachineTest, OnError_FromSourcesAttached_TransitionsToError)
{
	m_sm.onPipelineLoaded();
	m_sm.onSourceAttaching();
	m_sm.onAllSourcesAttached();
	m_sm.onError();
	EXPECT_EQ(m_sm.currentState(), PlayerStateId::ERROR);
}

/**
 * @test onError from PAUSED → ERROR.
 */
TEST_F(AampPlayerStateMachineTest, OnError_FromPaused_TransitionsToError)
{
	m_sm.onPipelineLoaded();
	m_sm.onSourceAttaching();
	m_sm.onAllSourcesAttached();
	m_sm.onPlaybackStarted();
	m_sm.onPlaybackPaused();
	m_sm.onError();
	EXPECT_EQ(m_sm.currentState(), PlayerStateId::ERROR);
}

/**
 * @test onError from PIPELINE_CREATED → ERROR.
 */
TEST_F(AampPlayerStateMachineTest, OnError_FromPipelineCreated_TransitionsToError)
{
	m_sm.onPipelineLoaded();
	m_sm.onError();
	EXPECT_EQ(m_sm.currentState(), PlayerStateId::ERROR);
}

/**
 * @test onError from SOURCES_ATTACHING → ERROR.
 */
TEST_F(AampPlayerStateMachineTest, OnError_FromSourcesAttaching_TransitionsToError)
{
	m_sm.onPipelineLoaded();
	m_sm.onSourceAttaching();
	m_sm.onError();
	EXPECT_EQ(m_sm.currentState(), PlayerStateId::ERROR);
}

/**
 * @test onStop from ERROR → IDLE.
 */
TEST_F(AampPlayerStateMachineTest, OnStop_FromError_TransitionsToIdle)
{
	m_sm.onPipelineLoaded();
	m_sm.onSourceAttaching();
	m_sm.onAllSourcesAttached();
	m_sm.onPlaybackStarted();
	m_sm.onError();
	ASSERT_EQ(m_sm.currentState(), PlayerStateId::ERROR);
	m_sm.onStop();
	EXPECT_EQ(m_sm.currentState(), PlayerStateId::IDLE);
}

/**
 * @test onError from FLUSHING → ERROR.
 */
TEST_F(AampPlayerStateMachineTest, OnError_FromFlushing_TransitionsToError)
{
	m_sm.onPipelineLoaded();
	m_sm.onSourceAttaching();
	m_sm.onAllSourcesAttached();
	m_sm.onFlush();
	m_sm.onError();
	EXPECT_EQ(m_sm.currentState(), PlayerStateId::ERROR);
}

// ===========================================================================
// Cross-state events: onReconfigure (re-tune resets to IDLE)
// ===========================================================================

/**
 * @test onReconfigure from IDLE → IDLE (no-op re-tune from initial state).
 */
TEST_F(AampPlayerStateMachineTest, OnReconfigure_FromIdle_StaysIdle)
{
	m_sm.onReconfigure();
	EXPECT_EQ(m_sm.currentState(), PlayerStateId::IDLE);
}

/**
 * @test onReconfigure from PIPELINE_CREATED → IDLE.
 */
TEST_F(AampPlayerStateMachineTest, OnReconfigure_FromPipelineCreated_TransitionsToIdle)
{
	m_sm.onPipelineLoaded();
	m_sm.onReconfigure();
	EXPECT_EQ(m_sm.currentState(), PlayerStateId::IDLE);
}

/**
 * @test onReconfigure from SOURCES_ATTACHING → IDLE.
 */
TEST_F(AampPlayerStateMachineTest, OnReconfigure_FromSourcesAttaching_TransitionsToIdle)
{
	m_sm.onPipelineLoaded();
	m_sm.onSourceAttaching();
	m_sm.onReconfigure();
	EXPECT_EQ(m_sm.currentState(), PlayerStateId::IDLE);
}

/**
 * @test onReconfigure from SOURCES_ATTACHED → IDLE.
 */
TEST_F(AampPlayerStateMachineTest, OnReconfigure_FromSourcesAttached_TransitionsToIdle)
{
	m_sm.onPipelineLoaded();
	m_sm.onSourceAttaching();
	m_sm.onAllSourcesAttached();
	m_sm.onReconfigure();
	EXPECT_EQ(m_sm.currentState(), PlayerStateId::IDLE);
}

/**
 * @test onReconfigure from PLAYING → IDLE.
 */
TEST_F(AampPlayerStateMachineTest, OnReconfigure_FromPlaying_TransitionsToIdle)
{
	m_sm.onPipelineLoaded();
	m_sm.onSourceAttaching();
	m_sm.onAllSourcesAttached();
	m_sm.onPlaybackStarted();
	m_sm.onReconfigure();
	EXPECT_EQ(m_sm.currentState(), PlayerStateId::IDLE);
}

/**
 * @test onReconfigure from PAUSED → IDLE.
 */
TEST_F(AampPlayerStateMachineTest, OnReconfigure_FromPaused_TransitionsToIdle)
{
	m_sm.onPipelineLoaded();
	m_sm.onSourceAttaching();
	m_sm.onAllSourcesAttached();
	m_sm.onPlaybackStarted();
	m_sm.onPlaybackPaused();
	m_sm.onReconfigure();
	EXPECT_EQ(m_sm.currentState(), PlayerStateId::IDLE);
}

/**
 * @test onReconfigure from ERROR → IDLE.
 */
TEST_F(AampPlayerStateMachineTest, OnReconfigure_FromError_TransitionsToIdle)
{
	m_sm.onPipelineLoaded();
	m_sm.onError();
	m_sm.onReconfigure();
	EXPECT_EQ(m_sm.currentState(), PlayerStateId::IDLE);
}

/**
 * @test onReconfigure from FLUSHING is ignored — FlushingState has no
 *       onReconfigure override.
 *
 * Configure() calls WaitForFlushToComplete() directly before dispatching
 * onReconfigure(), so the machine has already left FLUSHING by the time
 * onReconfigure is fired in production.  The dispatch layer emits a WARN
 * and the machine stays in FLUSHING.
 */
TEST_F(AampPlayerStateMachineTest, OnReconfigure_FromFlushing_IsIgnored)
{
	m_sm.onPipelineLoaded();
	m_sm.onSourceAttaching();
	m_sm.onAllSourcesAttached();
	m_sm.onFlush();
	m_sm.onReconfigure();       // no transition defined — dispatch warns
	EXPECT_EQ(m_sm.currentState(), PlayerStateId::FLUSHING);
}

// ===========================================================================
// Ignored / no-op events
// ===========================================================================

/**
 * @test Duplicate PLAYING notification while already PLAYING is ignored
 *       (state remains PLAYING, no invalid transition).
 */
TEST_F(AampPlayerStateMachineTest, OnPlaybackStarted_WhilePlaying_IsIgnored)
{
	m_sm.onPipelineLoaded();
	m_sm.onSourceAttaching();
	m_sm.onAllSourcesAttached();
	m_sm.onPlaybackStarted();
	m_sm.onPlaybackStarted(); // duplicate — must be a no-op
	EXPECT_EQ(m_sm.currentState(), PlayerStateId::PLAYING);
}

/**
 * @test onPipelineLoaded while in PIPELINE_CREATED is ignored
 *       (no state change expected from an unexpected event).
 */
TEST_F(AampPlayerStateMachineTest, OnPipelineLoaded_WhilePipelineCreated_IsIgnored)
{
	m_sm.onPipelineLoaded();
	m_sm.onPipelineLoaded(); // unexpected — no transition defined
	EXPECT_EQ(m_sm.currentState(), PlayerStateId::PIPELINE_CREATED);
}

// ===========================================================================
// Thread-safety smoke test
// ===========================================================================

/**
 * @test Concurrent calls to onStop and onReconfigure must not crash or
 *       corrupt the state machine (data-race check).
 */
TEST_F(AampPlayerStateMachineTest, ConcurrentEvents_DoNotCrash)
{
	m_sm.onPipelineLoaded();
	m_sm.onSourceAttaching();
	m_sm.onAllSourcesAttached();
	m_sm.onPlaybackStarted();

	// Drive events from two threads simultaneously.
	std::thread t1([this]() {
		for (int i = 0; i < 50; ++i)
		{
			m_sm.onStop();
			m_sm.onReconfigure();
			m_sm.onPipelineLoaded();
		}
	});
	std::thread t2([this]() {
		for (int i = 0; i < 50; ++i)
		{
			m_sm.onError();
			m_sm.onReconfigure();
			m_sm.onPipelineLoaded();
		}
	});
	t1.join();
	t2.join();
	// Any valid state is acceptable; the test passes if there is no crash.
	SUCCEED();
}

// ===========================================================================
// onFlushComplete — FLUSHING -> FLUSHED
// ===========================================================================

/**
 * @test FLUSHING + onFlushComplete → FLUSHED.
 *
 * SEEK_DONE always completes the flush cycle by moving to FLUSHED,
 * regardless of what state was active before the flush started.  Rialto's
 * subsequent PLAYING/PAUSED notification (guaranteed to arrive after
 * SEEK_DONE) is what actually drives the machine onward - see the
 * FromFlushed tests below.
 */
TEST_F(AampPlayerStateMachineTest, OnFlushComplete_FromFlushing_TransitionsToFlushed)
{
	m_sm.onPipelineLoaded();
	m_sm.onSourceAttaching();
	m_sm.onAllSourcesAttached();
	m_sm.onPlaybackStarted();
	m_sm.onFlush();
	ASSERT_EQ(m_sm.currentState(), PlayerStateId::FLUSHING);

	m_sm.onFlushComplete();
	EXPECT_EQ(m_sm.currentState(), PlayerStateId::FLUSHED);
}

/**
 * @test onFlushComplete while NOT in FLUSHING is silently ignored (WARN
 *       logged, no transition).
 */
TEST_F(AampPlayerStateMachineTest, OnFlushComplete_WhileNotFlushing_IsIgnored)
{
	m_sm.onPipelineLoaded();
	m_sm.onSourceAttaching();
	m_sm.onAllSourcesAttached();
	m_sm.onPlaybackStarted();      // state = PLAYING (not FLUSHING)

	m_sm.onFlushComplete();        // must be a no-op
	EXPECT_EQ(m_sm.currentState(), PlayerStateId::PLAYING);
}

// ===========================================================================
// FLUSHED — Rialto's post-seek notification drives the machine onward
// ===========================================================================

/**
 * @test FLUSHED + onPlaybackStarted → PLAYING.
 *
 * Covers seek-while-playing: Rialto sends PLAYING after SEEK_DONE.
 */
TEST_F(AampPlayerStateMachineTest, OnPlaybackStarted_FromFlushed_TransitionsToPlaying)
{
	m_sm.onPipelineLoaded();
	m_sm.onSourceAttaching();
	m_sm.onAllSourcesAttached();
	m_sm.onPlaybackStarted();
	m_sm.onFlush();
	m_sm.onFlushComplete();
	ASSERT_EQ(m_sm.currentState(), PlayerStateId::FLUSHED);

	m_sm.onPlaybackStarted();
	EXPECT_EQ(m_sm.currentState(), PlayerStateId::PLAYING);
}

/**
 * @test FLUSHED + onPlaybackPaused → PAUSED.
 *
 * Covers seek-while-paused: Rialto sends PAUSED after SEEK_DONE.
 */
TEST_F(AampPlayerStateMachineTest, OnPlaybackPaused_FromFlushed_TransitionsToPaused)
{
	m_sm.onPipelineLoaded();
	m_sm.onSourceAttaching();
	m_sm.onAllSourcesAttached();
	m_sm.onPlaybackStarted();
	m_sm.onPlaybackPaused();
	m_sm.onFlush();
	m_sm.onFlushComplete();
	ASSERT_EQ(m_sm.currentState(), PlayerStateId::FLUSHED);

	m_sm.onPlaybackPaused();
	EXPECT_EQ(m_sm.currentState(), PlayerStateId::PAUSED);
}

/**
 * @test FLUSHED + onFlush → FLUSHING.
 *
 * Covers a second seek arriving before Rialto's post-seek PLAYING/PAUSED
 * notification for the first seek has arrived.
 */
TEST_F(AampPlayerStateMachineTest, OnFlush_FromFlushed_TransitionsToFlushing)
{
	m_sm.onPipelineLoaded();
	m_sm.onSourceAttaching();
	m_sm.onAllSourcesAttached();
	m_sm.onPlaybackStarted();
	m_sm.onFlush();
	m_sm.onFlushComplete();
	ASSERT_EQ(m_sm.currentState(), PlayerStateId::FLUSHED);

	m_sm.onFlush();
	EXPECT_EQ(m_sm.currentState(), PlayerStateId::FLUSHING);
}

/**
 * @test FLUSHED + onStop → IDLE.
 */
TEST_F(AampPlayerStateMachineTest, OnStop_FromFlushed_TransitionsToIdle)
{
	m_sm.onPipelineLoaded();
	m_sm.onSourceAttaching();
	m_sm.onAllSourcesAttached();
	m_sm.onFlush();
	m_sm.onFlushComplete();
	ASSERT_EQ(m_sm.currentState(), PlayerStateId::FLUSHED);

	m_sm.onStop();
	EXPECT_EQ(m_sm.currentState(), PlayerStateId::IDLE);
}

/**
 * @test FLUSHED + onError → ERROR.
 */
TEST_F(AampPlayerStateMachineTest, OnError_FromFlushed_TransitionsToError)
{
	m_sm.onPipelineLoaded();
	m_sm.onSourceAttaching();
	m_sm.onAllSourcesAttached();
	m_sm.onFlush();
	m_sm.onFlushComplete();
	ASSERT_EQ(m_sm.currentState(), PlayerStateId::FLUSHED);

	m_sm.onError();
	EXPECT_EQ(m_sm.currentState(), PlayerStateId::ERROR);
}

/**
 * @test FLUSHED + onReconfigure → IDLE.
 */
TEST_F(AampPlayerStateMachineTest, OnReconfigure_FromFlushed_TransitionsToIdle)
{
	m_sm.onPipelineLoaded();
	m_sm.onSourceAttaching();
	m_sm.onAllSourcesAttached();
	m_sm.onFlush();
	m_sm.onFlushComplete();
	ASSERT_EQ(m_sm.currentState(), PlayerStateId::FLUSHED);

	m_sm.onReconfigure();
	EXPECT_EQ(m_sm.currentState(), PlayerStateId::IDLE);
}

// ===========================================================================
// waitForFlushComplete / waiter wake-up
// ===========================================================================

using namespace std::chrono_literals;

class AampPlayerStateMachineFlushWaitTest : public AampPlayerStateMachineTest
{
protected:
	/// PLAYING is a state a flush may start from.
	void MoveToPlaying()
	{
		m_sm.onPipelineLoaded();
		m_sm.onSourceAttaching();
		m_sm.onAllSourcesAttached();
		m_sm.onPlaybackStarted();
	}
};

/**
 * @test Without a flush in progress the wait returns at once, not abandoned.
 */
TEST_F(AampPlayerStateMachineFlushWaitTest, WaitForFlushComplete_NotFlushing_ReturnsFalseAtOnce)
{
	MoveToPlaying();
	bool handlerRan = false;

	const auto start = std::chrono::steady_clock::now();
	const bool abandoned =
		m_sm.waitForFlushComplete(10s, [&]() { handlerRan = true; });

	EXPECT_FALSE(abandoned);
	EXPECT_FALSE(handlerRan);
	EXPECT_LT(std::chrono::steady_clock::now() - start, 1s);
	EXPECT_EQ(m_sm.currentState(), PlayerStateId::PLAYING);
}

/**
 * @test onFlushComplete() releases a thread blocked in waitForFlushComplete()
 *       without abandoning the flush.
 */
TEST_F(AampPlayerStateMachineFlushWaitTest, OnFlushComplete_WakesWaiter_NotAbandoned)
{
	MoveToPlaying();
	m_sm.onFlush();

	std::atomic<bool> handlerRan{false};
	auto waiter = std::async(std::launch::async, [&]()
	{
		return m_sm.waitForFlushComplete(10s, [&]() { handlerRan = true; });
	});

	// The waiter must really be blocked: FLUSHING has not ended.
	EXPECT_EQ(waiter.wait_for(100ms), std::future_status::timeout);

	m_sm.onFlushComplete();

	ASSERT_EQ(waiter.wait_for(5s), std::future_status::ready);
	EXPECT_FALSE(waiter.get());
	EXPECT_FALSE(handlerRan.load());
	EXPECT_EQ(m_sm.currentState(), PlayerStateId::FLUSHED);
}

/**
 * @test When SEEK_DONE never arrives the wait gives up: the machine leaves
 *       FLUSHING, the handler runs once after that, and true is returned.
 */
TEST_F(AampPlayerStateMachineFlushWaitTest, WaitForFlushComplete_Timeout_AbandonsFlush)
{
	MoveToPlaying();
	m_sm.onFlush();

	int handlerCalls = 0;
	PlayerStateId stateInHandler = PlayerStateId::FLUSHING;
	const bool abandoned = m_sm.waitForFlushComplete(50ms, [&]()
	{
		++handlerCalls;
		stateInHandler = m_sm.currentState();
	});

	EXPECT_TRUE(abandoned);
	EXPECT_EQ(handlerCalls, 1);
	EXPECT_EQ(stateInHandler, PlayerStateId::FLUSHED);
	EXPECT_EQ(m_sm.currentState(), PlayerStateId::FLUSHED);
}

/**
 * @test A timed-out wait with no handler is allowed.
 */
TEST_F(AampPlayerStateMachineFlushWaitTest, WaitForFlushComplete_TimeoutWithoutHandler_StillAbandons)
{
	MoveToPlaying();
	m_sm.onFlush();

	EXPECT_TRUE(m_sm.waitForFlushComplete(20ms));
	EXPECT_EQ(m_sm.currentState(), PlayerStateId::FLUSHED);
}

/**
 * @test A failure that arrives while FLUSHING wakes the waiter (no timeout)
 *       and onError() reports the state it interrupted.
 */
TEST_F(AampPlayerStateMachineFlushWaitTest, OnError_WhileFlushing_ReturnsPreviousAndWakesWaiter)
{
	MoveToPlaying();
	m_sm.onFlush();

	auto waiter = std::async(std::launch::async,
		[&]() { return m_sm.waitForFlushComplete(10s); });
	EXPECT_EQ(waiter.wait_for(100ms), std::future_status::timeout);

	EXPECT_EQ(m_sm.onError(), PlayerStateId::FLUSHING);

	ASSERT_EQ(waiter.wait_for(5s), std::future_status::ready);
	EXPECT_FALSE(waiter.get());
	EXPECT_EQ(m_sm.currentState(), PlayerStateId::ERROR);
}

/**
 * @test onError() outside a flush reports the current state.
 */
TEST_F(AampPlayerStateMachineFlushWaitTest, OnError_WhenPlaying_ReturnsPlaying)
{
	MoveToPlaying();

	EXPECT_EQ(m_sm.onError(), PlayerStateId::PLAYING);
	EXPECT_EQ(m_sm.currentState(), PlayerStateId::ERROR);
}
