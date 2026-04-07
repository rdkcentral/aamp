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
 * @brief No-logic linker stubs for PlayerStateMachine used in L1 tests.
 *
 * These stubs satisfy the linker when AampRialtoPlayer.h is included (it
 * embeds a PlayerStateMachine by value) without pulling in the real state
 * machine logic.
 */

#include "AampPlayerStateMachine.h"

// ---------------------------------------------------------------------------
// IPlayerState — non-inline virtual methods declared in the header
// ---------------------------------------------------------------------------

std::unique_ptr<IPlayerState> IPlayerState::onStop()        { return nullptr; }
std::unique_ptr<IPlayerState> IPlayerState::onError()       { return nullptr; }
std::unique_ptr<IPlayerState> IPlayerState::onReconfigure() { return nullptr; }

// ---------------------------------------------------------------------------
// PlayerStateMachine
// ---------------------------------------------------------------------------

PlayerStateMachine::PlayerStateMachine()  = default;
PlayerStateMachine::~PlayerStateMachine() = default;

PlayerStateId PlayerStateMachine::currentState() const
{
	return PlayerStateId::IDLE;
}

const char *PlayerStateMachine::currentStateName() const
{
	return "IDLE";
}

void PlayerStateMachine::onPipelineLoaded()    {}
void PlayerStateMachine::onSourceAttaching()   {}
void PlayerStateMachine::onAllSourcesAttached() {}
void PlayerStateMachine::onPlaybackStarted()   {}
void PlayerStateMachine::onPlaybackPaused()    {}
void PlayerStateMachine::onFlush()             {}
void PlayerStateMachine::onStop()              {}
void PlayerStateMachine::onError()             {}
void PlayerStateMachine::onReconfigure()       {}

void PlayerStateMachine::dispatch(
	std::unique_ptr<IPlayerState> (IPlayerState::* /*handler*/)()) {}
