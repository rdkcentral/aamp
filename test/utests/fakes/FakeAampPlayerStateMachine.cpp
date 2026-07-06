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
 * @file FakeAampPlayerStateMachine.cpp
 * @brief Lightweight fake PlayerStateMachine for L1 tests.
 *
 * Implements the same state-transition table as the real machine but
 * without the concrete IPlayerState class hierarchy.  This allows
 * tests that call GetCurrentPlayerState() to observe real transitions
 * without pulling in the full production state machine.
 */

#include "AampPlayerStateMachine.h"

// ---------------------------------------------------------------------------
// PlayerStateMachine — enum-driven transition table
// ---------------------------------------------------------------------------

// File-scope storage for the fake state.
// thread_local ensures each test thread has its own independent state,
// preventing interference between parallel test executions.
static thread_local PlayerStateId s_fakeState{PlayerStateId::IDLE};

PlayerStateMachine::PlayerStateMachine()
	// m_mutex default-initialises; m_state is unused in this fake.
	// Reset the thread-local state to IDLE for each new instance so that
	// each test's fresh AampRialtoPlayer starts from a clean state.
{
	s_fakeState = PlayerStateId::IDLE;
}

PlayerStateMachine::~PlayerStateMachine() = default;

PlayerStateId PlayerStateMachine::currentState() const
{
	return s_fakeState;
}

const char *PlayerStateMachine::currentStateName() const
{
	switch (s_fakeState)
	{
		case PlayerStateId::IDLE:              return "IDLE";
		case PlayerStateId::PIPELINE_CREATED:  return "PIPELINE_CREATED";
		case PlayerStateId::SOURCES_ATTACHING: return "SOURCES_ATTACHING";
		case PlayerStateId::SOURCES_ATTACHED:  return "SOURCES_ATTACHED";
		case PlayerStateId::PLAYING:           return "PLAYING";
		case PlayerStateId::PAUSED:            return "PAUSED";
		case PlayerStateId::FLUSHING:          return "FLUSHING";
		case PlayerStateId::STOPPED:           return "STOPPED";
		case PlayerStateId::ERROR:             return "ERROR";
		default:                               return "UNKNOWN";
	}
}

void PlayerStateMachine::onPipelineLoaded()
{
	if (s_fakeState == PlayerStateId::IDLE)
	{
		s_fakeState = PlayerStateId::PIPELINE_CREATED;
	}
}

void PlayerStateMachine::onSourceAttaching()
{
	if (s_fakeState == PlayerStateId::PIPELINE_CREATED ||
	    s_fakeState == PlayerStateId::FLUSHING)
	{
		s_fakeState = PlayerStateId::SOURCES_ATTACHING;
	}
}

void PlayerStateMachine::onAllSourcesAttached()
{
	if (s_fakeState == PlayerStateId::SOURCES_ATTACHING)
	{
		s_fakeState = PlayerStateId::SOURCES_ATTACHED;
	}
}

void PlayerStateMachine::onPlaybackStarted()
{
	if (s_fakeState == PlayerStateId::SOURCES_ATTACHED)
	{
		s_fakeState = PlayerStateId::PLAYING;
	}
}

void PlayerStateMachine::onPlaybackPaused()
{
	if (s_fakeState == PlayerStateId::PLAYING)
	{
		s_fakeState = PlayerStateId::PAUSED;
	}
}

void PlayerStateMachine::onFlush()
{
	if (s_fakeState == PlayerStateId::SOURCES_ATTACHED ||
	    s_fakeState == PlayerStateId::PLAYING          ||
	    s_fakeState == PlayerStateId::PAUSED)
	{
		s_fakeState = PlayerStateId::FLUSHING;
	}
}

void PlayerStateMachine::onStop()
{
	s_fakeState = PlayerStateId::STOPPED;
}

void PlayerStateMachine::onError()
{
	s_fakeState = PlayerStateId::ERROR;
}

void PlayerStateMachine::onReconfigure()
{
	s_fakeState = PlayerStateId::IDLE;
}

void PlayerStateMachine::dispatch(
	std::unique_ptr<IPlayerState> (IPlayerState::*handler)(),
	const char *eventName)
{
}

