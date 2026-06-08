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
 * @test FLUSHING + onSourceAttaching → SOURCES_ATTACHING (re-attach after seek).
 */
TEST_F(AampPlayerStateMachineTest, OnSourceAttaching_FromFlushing_TransitionsToSourcesAttaching)
{
	m_sm.onPipelineLoaded();
	m_sm.onSourceAttaching();
	m_sm.onAllSourcesAttached();
	m_sm.onFlush();
	m_sm.onSourceAttaching();
	EXPECT_EQ(m_sm.currentState(), PlayerStateId::SOURCES_ATTACHING);
}

/**
 * @test FLUSHING + onPlaybackStarted → PLAYING.
 */
TEST_F(AampPlayerStateMachineTest,
	OnPlaybackStarted_FromFlushing_TransitionsToPlaying)
{
	m_sm.onPipelineLoaded();
	m_sm.onSourceAttaching();
	m_sm.onAllSourcesAttached();
	m_sm.onFlush();
	m_sm.onPlaybackStarted();
	EXPECT_EQ(m_sm.currentState(), PlayerStateId::PLAYING);
}

/**
 * @test FLUSHING + onPlaybackPaused → PAUSED.
 */
TEST_F(AampPlayerStateMachineTest,
	OnPlaybackPaused_FromFlushing_TransitionsToPaused)
{
	m_sm.onPipelineLoaded();
	m_sm.onSourceAttaching();
	m_sm.onAllSourcesAttached();
	m_sm.onPlaybackStarted();
	m_sm.onPlaybackPaused();
	m_sm.onFlush();
	m_sm.onPlaybackPaused();
	EXPECT_EQ(m_sm.currentState(), PlayerStateId::PAUSED);
}

// ===========================================================================
// Cross-state events: onStop (valid from any non-terminal state)
// ===========================================================================

/**
 * @test onStop from PIPELINE_CREATED → STOPPED.
 */
TEST_F(AampPlayerStateMachineTest, OnStop_FromPipelineCreated_TransitionsToStopped)
{
	m_sm.onPipelineLoaded();
	m_sm.onStop();
	EXPECT_EQ(m_sm.currentState(), PlayerStateId::STOPPED);
}

/**
 * @test onStop from SOURCES_ATTACHING → STOPPED.
 */
TEST_F(AampPlayerStateMachineTest, OnStop_FromSourcesAttaching_TransitionsToStopped)
{
	m_sm.onPipelineLoaded();
	m_sm.onSourceAttaching();
	m_sm.onStop();
	EXPECT_EQ(m_sm.currentState(), PlayerStateId::STOPPED);
}

/**
 * @test onStop from PLAYING → STOPPED.
 */
TEST_F(AampPlayerStateMachineTest, OnStop_FromPlaying_TransitionsToStopped)
{
	m_sm.onPipelineLoaded();
	m_sm.onSourceAttaching();
	m_sm.onAllSourcesAttached();
	m_sm.onPlaybackStarted();
	m_sm.onStop();
	EXPECT_EQ(m_sm.currentState(), PlayerStateId::STOPPED);
}

/**
 * @test onStop from PAUSED → STOPPED.
 */
TEST_F(AampPlayerStateMachineTest, OnStop_FromPaused_TransitionsToStopped)
{
	m_sm.onPipelineLoaded();
	m_sm.onSourceAttaching();
	m_sm.onAllSourcesAttached();
	m_sm.onPlaybackStarted();
	m_sm.onPlaybackPaused();
	m_sm.onStop();
	EXPECT_EQ(m_sm.currentState(), PlayerStateId::STOPPED);
}

/**
 * @test onStop from FLUSHING → STOPPED.
 */
TEST_F(AampPlayerStateMachineTest, OnStop_FromFlushing_TransitionsToStopped)
{
	m_sm.onPipelineLoaded();
	m_sm.onSourceAttaching();
	m_sm.onAllSourcesAttached();
	m_sm.onFlush();
	m_sm.onStop();
	EXPECT_EQ(m_sm.currentState(), PlayerStateId::STOPPED);
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
 * @test onReconfigure from STOPPED → IDLE.
 */
TEST_F(AampPlayerStateMachineTest, OnReconfigure_FromStopped_TransitionsToIdle)
{
	m_sm.onPipelineLoaded();
	m_sm.onStop();
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
 * @test onReconfigure from FLUSHING → IDLE.
 */
TEST_F(AampPlayerStateMachineTest, OnReconfigure_FromFlushing_TransitionsToIdle)
{
	m_sm.onPipelineLoaded();
	m_sm.onSourceAttaching();
	m_sm.onAllSourcesAttached();
	m_sm.onFlush();
	m_sm.onReconfigure();
	EXPECT_EQ(m_sm.currentState(), PlayerStateId::IDLE);
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
