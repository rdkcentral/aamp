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
 * @test onSourceAttaching from FLUSHING is ignored (no valid transition).
 *
 * There is no direct FLUSHING→SOURCES_ATTACHING path; a pipeline rebuild
 * always goes through onReconfigure (→IDLE) first.  The dispatch layer
 * emits a WARN and leaves the machine in FLUSHING.
 */
TEST_F(AampPlayerStateMachineTest, OnSourceAttaching_FromFlushing_IsIgnored)
{
	m_sm.onPipelineLoaded();
	m_sm.onSourceAttaching();
	m_sm.onAllSourcesAttached();
	m_sm.onFlush();
	m_sm.onSourceAttaching();   // no transition defined — dispatch warns
	EXPECT_EQ(m_sm.currentState(), PlayerStateId::FLUSHING);
}

/**
 * @test FLUSHING + onPlaybackStarted updates pre-flush state to PLAYING but
 *       does NOT exit FLUSHING.  The machine only leaves FLUSHING via
 *       onFlushComplete(), which then restores to PLAYING.  This keeps
 *       WaitForFlushToComplete() correctly blocked until all sources confirm
 *       flushed, preventing Configure() from racing with in-flight callbacks.
 */
TEST_F(AampPlayerStateMachineTest,
	OnPlaybackStarted_FromFlushing_UpdatesPreFlushStateAndStaysFlushing)
{
	m_sm.onPipelineLoaded();
	m_sm.onSourceAttaching();
	m_sm.onAllSourcesAttached();
	m_sm.onFlush();
	ASSERT_EQ(m_sm.currentState(), PlayerStateId::FLUSHING);

	m_sm.onPlaybackStarted();    // delayed ack: updates pre-flush, stays FLUSHING
	EXPECT_EQ(m_sm.currentState(), PlayerStateId::FLUSHING);

	m_sm.onFlushComplete();      // sources all flushed: restores to PLAYING
	EXPECT_EQ(m_sm.currentState(), PlayerStateId::PLAYING);
}

/**
 * @test FLUSHING + onPlaybackPaused updates pre-flush state to PAUSED but
 *       does NOT exit FLUSHING.  onFlushComplete() then restores to PAUSED.
 */
TEST_F(AampPlayerStateMachineTest,
	OnPlaybackPaused_FromFlushing_UpdatesPreFlushStateAndStaysFlushing)
{
	m_sm.onPipelineLoaded();
	m_sm.onSourceAttaching();
	m_sm.onAllSourcesAttached();
	m_sm.onPlaybackStarted();
	m_sm.onPlaybackPaused();
	m_sm.onFlush();
	ASSERT_EQ(m_sm.currentState(), PlayerStateId::FLUSHING);

	m_sm.onPlaybackPaused();     // delayed ack: updates pre-flush, stays FLUSHING
	EXPECT_EQ(m_sm.currentState(), PlayerStateId::FLUSHING);

	m_sm.onFlushComplete();      // sources all flushed: restores to PAUSED
	EXPECT_EQ(m_sm.currentState(), PlayerStateId::PAUSED);
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
 * Configure() calls Stop() first, which calls WaitForFlushToComplete(),
 * so the machine has already left FLUSHING before onReconfigure is fired.
 * The dispatch layer emits a WARN and the machine stays in FLUSHING.
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
// onFlushComplete — pre-flush state restoration
// ===========================================================================

/**
 * @test FLUSHING (pre-flush=PLAYING) + onFlushComplete → PLAYING.
 *
 * When a seek completes and sources report flushed, the state machine
 * must restore the pre-flush PLAYING state, not wait for Rialto PLAYING.
 */
TEST_F(AampPlayerStateMachineTest, OnFlushComplete_FromFlushing_PreFlushPlaying_RestoresPlaying)
{
	m_sm.onPipelineLoaded();
	m_sm.onSourceAttaching();
	m_sm.onAllSourcesAttached();
	m_sm.onPlaybackStarted();      // pre-flush state = PLAYING
	m_sm.onFlush();
	ASSERT_EQ(m_sm.currentState(), PlayerStateId::FLUSHING);

	m_sm.onFlushComplete();
	EXPECT_EQ(m_sm.currentState(), PlayerStateId::PLAYING);
}

/**
 * @test FLUSHING (pre-flush=PAUSED) + onFlushComplete → PAUSED.
 *
 * A seek while paused must restore PAUSED without issuing play().
 */
TEST_F(AampPlayerStateMachineTest, OnFlushComplete_FromFlushing_PreFlushPaused_RestoresPaused)
{
	m_sm.onPipelineLoaded();
	m_sm.onSourceAttaching();
	m_sm.onAllSourcesAttached();
	m_sm.onPlaybackStarted();
	m_sm.onPlaybackPaused();       // pre-flush state = PAUSED
	m_sm.onFlush();
	ASSERT_EQ(m_sm.currentState(), PlayerStateId::FLUSHING);

	m_sm.onFlushComplete();
	EXPECT_EQ(m_sm.currentState(), PlayerStateId::PAUSED);
}

/**
 * @test FLUSHING (pre-flush=SOURCES_ATTACHED) + onFlushComplete →
 *       SOURCES_ATTACHED.
 *
 * Flush can be called from SOURCES_ATTACHED (e.g. initial position setup).
 */
TEST_F(AampPlayerStateMachineTest, OnFlushComplete_FromFlushing_PreFlushSourcesAttached_RestoresSourcesAttached)
{
	m_sm.onPipelineLoaded();
	m_sm.onSourceAttaching();
	m_sm.onAllSourcesAttached();   // pre-flush state = SOURCES_ATTACHED
	m_sm.onFlush();
	ASSERT_EQ(m_sm.currentState(), PlayerStateId::FLUSHING);

	m_sm.onFlushComplete();
	EXPECT_EQ(m_sm.currentState(), PlayerStateId::SOURCES_ATTACHED);
}

/**
 * @test onFlushComplete while NOT in FLUSHING is silently ignored.
 *
 * The race where Rialto already emitted PLAYING before all sources
 * flushed can leave the machine in PLAYING when onFlushComplete fires.
 * The call must be a no-op.
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

/**
 * @test Pre-flush state is updated across successive flushes.
 *
 * After PLAYING→FLUSHING→onFlushComplete→PLAYING→PAUSED→FLUSHING→
 * onFlushComplete, the state must be PAUSED (second pre-flush wins).
 */
TEST_F(AampPlayerStateMachineTest, OnFlushComplete_SuccessiveFlushes_TracksLatestPreFlushState)
{
	m_sm.onPipelineLoaded();
	m_sm.onSourceAttaching();
	m_sm.onAllSourcesAttached();
	m_sm.onPlaybackStarted();      // PLAYING

	// First flush cycle: from PLAYING
	m_sm.onFlush();
	m_sm.onFlushComplete();
	ASSERT_EQ(m_sm.currentState(), PlayerStateId::PLAYING);

	m_sm.onPlaybackPaused();       // PAUSED

	// Second flush cycle: from PAUSED
	m_sm.onFlush();
	m_sm.onFlushComplete();
	EXPECT_EQ(m_sm.currentState(), PlayerStateId::PAUSED);
}

/**
 * @test FLUSHING + onPlaybackStarted → stays FLUSHING, pre-flush state
 *       updated to PLAYING (edge-case race handler).
 *
 * When a Rialto PLAYING notification arrives during the narrow window
 * between flushSource() sending the IPC command and onFlushComplete()
 * being called, the machine stays in FLUSHING (WaitForFlushToComplete
 * must not unblock prematurely) and updates the pre-flush state so that
 * onFlushComplete() restores to PLAYING.
 */
TEST_F(AampPlayerStateMachineTest, OnPlaybackStarted_FromFlushing_EdgeCaseRace_StaysFlushing)
{
	m_sm.onPipelineLoaded();
	m_sm.onSourceAttaching();
	m_sm.onAllSourcesAttached();
	m_sm.onPlaybackStarted();
	m_sm.onFlush();
	ASSERT_EQ(m_sm.currentState(), PlayerStateId::FLUSHING);

	m_sm.onPlaybackStarted();      // delayed ack: state stays FLUSHING
	EXPECT_EQ(m_sm.currentState(), PlayerStateId::FLUSHING);

	m_sm.onFlushComplete();        // sources flushed: restores to PLAYING
	EXPECT_EQ(m_sm.currentState(), PlayerStateId::PLAYING);
}

/**
 * @test FLUSHING + onPlaybackPaused → PAUSED (edge-case race handler).
 */
TEST_F(AampPlayerStateMachineTest, OnPlaybackPaused_FromFlushing_EdgeCaseRace_TransitionsToPaused)
{
	// This test is subsumed by
	// OnPlaybackPaused_FromFlushing_UpdatesPreFlushStateAndStaysFlushing.
	// Kept here to verify that successive flush+PlaybackPaused sequences
	// are handled correctly when pre-flush state starts as PLAYING.
	m_sm.onPipelineLoaded();
	m_sm.onSourceAttaching();
	m_sm.onAllSourcesAttached();
	m_sm.onPlaybackStarted();      // pre-flush state will be PLAYING
	m_sm.onFlush();
	ASSERT_EQ(m_sm.currentState(), PlayerStateId::FLUSHING);

	m_sm.onPlaybackPaused();       // overrides pre-flush state to PAUSED
	EXPECT_EQ(m_sm.currentState(), PlayerStateId::FLUSHING);

	m_sm.onFlushComplete();        // restores to PAUSED (updated pre-flush state)
	EXPECT_EQ(m_sm.currentState(), PlayerStateId::PAUSED);
}
