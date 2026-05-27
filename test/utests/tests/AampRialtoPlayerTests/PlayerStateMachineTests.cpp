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
 * @file PlayerStateMachineTests.cpp
 * @brief L1 unit tests for PlayerStateMachine (GoF State pattern).
 *
 * Tests verify state transitions in response to lifecycle events.
 */

#include <gtest/gtest.h>
#include "AampPlayerStateMachine.h"

// ===========================================================================
// Test Fixture
// ===========================================================================

class PlayerStateMachineTest : public ::testing::Test
{
protected:
	PlayerStateMachine m_sm;
};

// ===========================================================================
// FlushingState transitions
// ===========================================================================

// RED TEST: FlushingState should transition to PlayingState when Rialto
// sends PLAYING notification (onPlaybackStarted event).
// This fixes the issue where the state machine stays stuck in FLUSHING
// when Rialto sends playback state changes during a flush/seek.
TEST_F(PlayerStateMachineTest,
	FlushingState_OnPlaybackStarted_TransitionsToPlayingState)
{
	// Arrange: transition to FLUSHING state
	m_sm.onReconfigure();                 // → IDLE
	m_sm.onPipelineLoaded();              // → PIPELINE_CREATED
	m_sm.onSourceAttaching();             // → SOURCES_ATTACHING
	m_sm.onAllSourcesAttached();          // → SOURCES_ATTACHED
	m_sm.onFlush();                       // → FLUSHING
	
	ASSERT_EQ(m_sm.currentState(), PlayerStateId::FLUSHING);
	
	// Act: Rialto sends PLAYING notification
	m_sm.onPlaybackStarted();
	
	// Assert: state machine should transition to PLAYING
	EXPECT_EQ(m_sm.currentState(), PlayerStateId::PLAYING);
}

// RED TEST: FlushingState should transition to PausedState when Rialto
// sends PAUSED notification (onPlaybackPaused event).
// This handles the case where a seek with keepPaused=1 causes Rialto
// to send PAUSED while the state machine is in FLUSHING.
TEST_F(PlayerStateMachineTest,
	FlushingState_OnPlaybackPaused_TransitionsToPausedState)
{
	// Arrange: transition to FLUSHING state from PAUSED
	m_sm.onReconfigure();                 // → IDLE
	m_sm.onPipelineLoaded();              // → PIPELINE_CREATED
	m_sm.onSourceAttaching();             // → SOURCES_ATTACHING
	m_sm.onAllSourcesAttached();          // → SOURCES_ATTACHED
	m_sm.onPlaybackStarted();             // → PLAYING
	m_sm.onPlaybackPaused();              // → PAUSED
	m_sm.onFlush();                       // → FLUSHING
	
	ASSERT_EQ(m_sm.currentState(), PlayerStateId::FLUSHING);
	
	// Act: Rialto sends PAUSED notification
	m_sm.onPlaybackPaused();
	
	// Assert: state machine should transition to PAUSED
	EXPECT_EQ(m_sm.currentState(), PlayerStateId::PAUSED);
}

// Verify existing transition: FlushingState → SourcesAttachingState
// (this should continue to work)
TEST_F(PlayerStateMachineTest,
	FlushingState_OnSourceAttaching_TransitionsToSourcesAttachingState)
{
	// Arrange: transition to FLUSHING state
	m_sm.onReconfigure();                 // → IDLE
	m_sm.onPipelineLoaded();              // → PIPELINE_CREATED
	m_sm.onSourceAttaching();             // → SOURCES_ATTACHING
	m_sm.onAllSourcesAttached();          // → SOURCES_ATTACHED
	m_sm.onFlush();                       // → FLUSHING
	
	ASSERT_EQ(m_sm.currentState(), PlayerStateId::FLUSHING);
	
	// Act: new init fragments arrive
	m_sm.onSourceAttaching();
	
	// Assert: state machine should transition to SOURCES_ATTACHING
	EXPECT_EQ(m_sm.currentState(), PlayerStateId::SOURCES_ATTACHING);
}

// ===========================================================================
// Cross-state event handling (verify FLUSHING handles global events)
// ===========================================================================

TEST_F(PlayerStateMachineTest,
	FlushingState_OnStop_TransitionsToStoppedState)
{
	// Arrange
	m_sm.onReconfigure();
	m_sm.onPipelineLoaded();
	m_sm.onSourceAttaching();
	m_sm.onAllSourcesAttached();
	m_sm.onFlush();
	ASSERT_EQ(m_sm.currentState(), PlayerStateId::FLUSHING);
	
	// Act
	m_sm.onStop();
	
	// Assert
	EXPECT_EQ(m_sm.currentState(), PlayerStateId::STOPPED);
}

TEST_F(PlayerStateMachineTest,
	FlushingState_OnError_TransitionsToErrorState)
{
	// Arrange
	m_sm.onReconfigure();
	m_sm.onPipelineLoaded();
	m_sm.onSourceAttaching();
	m_sm.onAllSourcesAttached();
	m_sm.onFlush();
	ASSERT_EQ(m_sm.currentState(), PlayerStateId::FLUSHING);
	
	// Act
	m_sm.onError();
	
	// Assert
	EXPECT_EQ(m_sm.currentState(), PlayerStateId::ERROR);
}

TEST_F(PlayerStateMachineTest,
	FlushingState_OnReconfigure_TransitionsToIdleState)
{
	// Arrange
	m_sm.onReconfigure();
	m_sm.onPipelineLoaded();
	m_sm.onSourceAttaching();
	m_sm.onAllSourcesAttached();
	m_sm.onFlush();
	ASSERT_EQ(m_sm.currentState(), PlayerStateId::FLUSHING);
	
	// Act
	m_sm.onReconfigure();
	
	// Assert
	EXPECT_EQ(m_sm.currentState(), PlayerStateId::IDLE);
}
