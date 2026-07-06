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
 * @file AampPlayerStateMachine.cpp
 * @brief GoF State-pattern implementation for AampRialtoPlayer.
 *
 * All concrete IPlayerState subclasses are defined here and are private to
 * this translation unit.  Callers only interact with PlayerStateMachine and
 * the PlayerStateId enum.
 */

#include "AampPlayerStateMachine.h"
#include "AampLogManager.h"
#include <cassert>

// ============================================================================
// Concrete state declarations (internal to this TU)
// ============================================================================

class IdleState;
class PipelineCreatedState;
class SourcesAttachingState;
class SourcesAttachedState;
class PlayingState;
class PausedState;
class FlushingState;
class StoppedState;
class ErrorState;

// ============================================================================
// Concrete state implementations
// ============================================================================

class IdleState final : public IPlayerState
{
public:
	PlayerStateId id()   const override { return PlayerStateId::IDLE; }
	const char   *name() const override { return "IDLE"; }

	std::unique_ptr<IPlayerState> onPipelineLoaded() override;
};

class PipelineCreatedState final : public IPlayerState
{
public:
	PlayerStateId id()   const override { return PlayerStateId::PIPELINE_CREATED; }
	const char   *name() const override { return "PIPELINE_CREATED"; }

	std::unique_ptr<IPlayerState> onSourceAttaching() override;
};

class SourcesAttachingState final : public IPlayerState
{
public:
	PlayerStateId id()   const override { return PlayerStateId::SOURCES_ATTACHING; }
	const char   *name() const override { return "SOURCES_ATTACHING"; }

	std::unique_ptr<IPlayerState> onAllSourcesAttached() override;
};

class SourcesAttachedState final : public IPlayerState
{
public:
	PlayerStateId id()   const override { return PlayerStateId::SOURCES_ATTACHED; }
	const char   *name() const override { return "SOURCES_ATTACHED"; }

	std::unique_ptr<IPlayerState> onPlaybackStarted() override;
	std::unique_ptr<IPlayerState> onFlush()           override;
};

class PlayingState final : public IPlayerState
{
public:
	PlayerStateId id()   const override { return PlayerStateId::PLAYING; }
	const char   *name() const override { return "PLAYING"; }

	std::unique_ptr<IPlayerState> onPlaybackPaused()  override;
	std::unique_ptr<IPlayerState> onFlush()           override;
	// Tolerate a duplicate PLAYING notification without transitioning.
	std::unique_ptr<IPlayerState> onPlaybackStarted() override { return nullptr; }
};

class PausedState final : public IPlayerState
{
public:
	PlayerStateId id()   const override { return PlayerStateId::PAUSED; }
	const char   *name() const override { return "PAUSED"; }

	std::unique_ptr<IPlayerState> onPlaybackStarted() override;
	std::unique_ptr<IPlayerState> onFlush()           override;
};

class FlushingState final : public IPlayerState
{
public:
	PlayerStateId id()   const override { return PlayerStateId::FLUSHING; }
	const char   *name() const override { return "FLUSHING"; }

	/// After a flush new init fragments arrive, restarting source attachment.
	std::unique_ptr<IPlayerState> onSourceAttaching() override;

	// onPlaybackStarted() and onPlaybackPaused() are intentionally NOT
	// overridden here.  When Rialto sends PLAYING or PAUSED during a flush
	// (delayed ack for a play() that was in-flight when Flush() started),
	// PlayerStateMachine::onPlaybackStarted/Paused() intercepts the call,
	// updates m_preFlushStateId, and returns without transitioning.  The
	// machine stays in FLUSHING until onFlushComplete() fires — keeping
	// WaitForFlushToComplete() correctly blocked until all sources confirm
	// flushed.  See PlayerStateMachine::onPlaybackStarted/Paused() below.
};

class StoppedState final : public IPlayerState
{
public:
	PlayerStateId id()   const override { return PlayerStateId::STOPPED; }
	const char   *name() const override { return "STOPPED"; }
	// Re-configure (re-tune) resets to IDLE; handled by base onReconfigure().
};

class ErrorState final : public IPlayerState
{
public:
	PlayerStateId id()   const override { return PlayerStateId::ERROR; }
	const char   *name() const override { return "ERROR"; }
	// Re-configure after error resets to IDLE; handled by base onReconfigure().
};

// ============================================================================
// IPlayerState base implementations for cross-state events
// ============================================================================

std::unique_ptr<IPlayerState> IPlayerState::onStop()
{
	return std::make_unique<StoppedState>();
}

std::unique_ptr<IPlayerState> IPlayerState::onError()
{
	return std::make_unique<ErrorState>();
}

std::unique_ptr<IPlayerState> IPlayerState::onReconfigure()
{
	return std::make_unique<IdleState>();
}

// ============================================================================
// Concrete state transition bodies
// ============================================================================

std::unique_ptr<IPlayerState> IdleState::onPipelineLoaded()
{
	return std::make_unique<PipelineCreatedState>();
}

std::unique_ptr<IPlayerState> PipelineCreatedState::onSourceAttaching()
{
	return std::make_unique<SourcesAttachingState>();
}

std::unique_ptr<IPlayerState> SourcesAttachingState::onAllSourcesAttached()
{
	return std::make_unique<SourcesAttachedState>();
}

std::unique_ptr<IPlayerState> SourcesAttachedState::onPlaybackStarted()
{
	return std::make_unique<PlayingState>();
}

std::unique_ptr<IPlayerState> SourcesAttachedState::onFlush()
{
	return std::make_unique<FlushingState>();
}

std::unique_ptr<IPlayerState> PlayingState::onPlaybackPaused()
{
	return std::make_unique<PausedState>();
}

std::unique_ptr<IPlayerState> PlayingState::onFlush()
{
	return std::make_unique<FlushingState>();
}

std::unique_ptr<IPlayerState> PausedState::onPlaybackStarted()
{
	return std::make_unique<PlayingState>();
}

std::unique_ptr<IPlayerState> PausedState::onFlush()
{
	return std::make_unique<FlushingState>();
}

std::unique_ptr<IPlayerState> FlushingState::onSourceAttaching()
{
	return std::make_unique<SourcesAttachingState>();
}

// ============================================================================
// PlayerStateMachine
// ============================================================================

PlayerStateMachine::PlayerStateMachine()
	: m_state(std::make_unique<IdleState>())
{
}

PlayerStateMachine::~PlayerStateMachine() = default;

PlayerStateId PlayerStateMachine::currentState() const
{
	std::lock_guard<std::mutex> lock(m_mutex);
	return m_state->id();
}

const char *PlayerStateMachine::currentStateName() const
{
	std::lock_guard<std::mutex> lock(m_mutex);
	return m_state->name();
}

void PlayerStateMachine::dispatch(
	std::unique_ptr<IPlayerState> (IPlayerState::*handler)())
{
	std::lock_guard<std::mutex> lock(m_mutex);
	auto next = (m_state.get()->*handler)();
	if (next)
	{
		AAMPLOG_MIL("PlayerState: %s -> %s",
			m_state->name(), next->name());
		m_state = std::move(next);
	}
}

void PlayerStateMachine::onPipelineLoaded()
{
	dispatch(&IPlayerState::onPipelineLoaded);
}

void PlayerStateMachine::onSourceAttaching()
{
	dispatch(&IPlayerState::onSourceAttaching);
}

void PlayerStateMachine::onAllSourcesAttached()
{
	dispatch(&IPlayerState::onAllSourcesAttached);
}

void PlayerStateMachine::onPlaybackStarted()
{
	{
		// Edge-case race: a delayed PLAYING ack for a play() that was in-flight
		// when Flush() started.  Update m_preFlushStateId so onFlushComplete()
		// will restore to PLAYING, but keep the machine in FLUSHING — this is
		// the key advantage over immediately transitioning: WaitForFlushToComplete()
		// (predicate: state != FLUSHING) stays blocked until all sources confirm
		// flushed, preventing Configure() or a new Flush() from racing with
		// in-flight OnSourceFlushed callbacks.
		std::lock_guard<std::mutex> lock(m_mutex);
		if (m_state->id() == PlayerStateId::FLUSHING)
		{
			AAMPLOG_INFO("PlayerState: onPlaybackStarted during FLUSHING - "
				"updating pre-flush state to PLAYING, "
				"state remains FLUSHING until all sources flush");
			m_preFlushStateId = PlayerStateId::PLAYING;
			return;
		}
	}
	dispatch(&IPlayerState::onPlaybackStarted);
}

void PlayerStateMachine::onPlaybackPaused()
{
	{
		// Edge-case race: same reasoning as onPlaybackStarted above.
		std::lock_guard<std::mutex> lock(m_mutex);
		if (m_state->id() == PlayerStateId::FLUSHING)
		{
			AAMPLOG_INFO("PlayerState: onPlaybackPaused during FLUSHING - "
				"updating pre-flush state to PAUSED, "
				"state remains FLUSHING until all sources flush");
			m_preFlushStateId = PlayerStateId::PAUSED;
			return;
		}
	}
	dispatch(&IPlayerState::onPlaybackPaused);
}

void PlayerStateMachine::onFlush()
{
	// Save the current state before entering FLUSHING so that
	// onFlushComplete() can restore it when all sources report flushed.
	std::lock_guard<std::mutex> lock(m_mutex);
	m_preFlushStateId = m_state->id();
	AAMPLOG_INFO("PlayerState: saving pre-flush state '%s' (id=%d)",
		m_state->name(), static_cast<int>(m_preFlushStateId));
	auto next = m_state->onFlush();
	if (next)
	{
		AAMPLOG_MIL("PlayerState: %s -> %s",
			m_state->name(), next->name());
		m_state = std::move(next);
	}
}

void PlayerStateMachine::onFlushComplete()
{
	std::lock_guard<std::mutex> lock(m_mutex);

	if (m_state->id() != PlayerStateId::FLUSHING)
	{
		// The edge-case race: onPlaybackStarted/Paused already exited
		// FLUSHING before all sources confirmed flushed.  This call is a
		// no-op — the correct post-flush state was already applied.
		AAMPLOG_INFO("PlayerState: onFlushComplete ignored - current state is '"
			"%s', not FLUSHING (edge-case race already resolved)",
			m_state->name());
		return;
	}

	std::unique_ptr<IPlayerState> restored;
	switch (m_preFlushStateId)
	{
	case PlayerStateId::PLAYING:
		restored = std::make_unique<PlayingState>();
		break;
	case PlayerStateId::PAUSED:
		restored = std::make_unique<PausedState>();
		break;
	case PlayerStateId::SOURCES_ATTACHED:
		restored = std::make_unique<SourcesAttachedState>();
		break;
	default:
		AAMPLOG_WARN(
			"PlayerState: onFlushComplete - unexpected pre-flush state %d,"
			" restoring SOURCES_ATTACHED as safe fallback",
			static_cast<int>(m_preFlushStateId));
		restored = std::make_unique<SourcesAttachedState>();
		break;
	}

	AAMPLOG_MIL("PlayerState: %s -> %s (flush complete; pre-flush state restored)",
		m_state->name(), restored->name());
	m_state = std::move(restored);
}

void PlayerStateMachine::onStop()
{
	dispatch(&IPlayerState::onStop);
}

void PlayerStateMachine::onError()
{
	dispatch(&IPlayerState::onError);
}

void PlayerStateMachine::onReconfigure()
{
	dispatch(&IPlayerState::onReconfigure);
}
