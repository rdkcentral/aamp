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
class FlushedState;
class StoppedState;  // forward decl retained; Stop() calls WaitForFlushToComplete
					  // so onStop() should never be dispatched from FLUSHING
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
	std::unique_ptr<IPlayerState> onReconfigure()    override; // first-tune reset
};

class PipelineCreatedState final : public IPlayerState
{
public:
	PlayerStateId id()   const override { return PlayerStateId::PIPELINE_CREATED; }
	const char   *name() const override { return "PIPELINE_CREATED"; }

	std::unique_ptr<IPlayerState> onSourceAttaching() override;
	std::unique_ptr<IPlayerState> onStop()            override;
	std::unique_ptr<IPlayerState> onError()           override;
	std::unique_ptr<IPlayerState> onReconfigure()     override;
};

class SourcesAttachingState final : public IPlayerState
{
public:
	PlayerStateId id()   const override { return PlayerStateId::SOURCES_ATTACHING; }
	const char   *name() const override { return "SOURCES_ATTACHING"; }

	std::unique_ptr<IPlayerState> onAllSourcesAttached() override;
	std::unique_ptr<IPlayerState> onStop()               override;
	std::unique_ptr<IPlayerState> onError()              override;
	std::unique_ptr<IPlayerState> onReconfigure()        override;
};

class SourcesAttachedState final : public IPlayerState
{
public:
	PlayerStateId id()   const override { return PlayerStateId::SOURCES_ATTACHED; }
	const char   *name() const override { return "SOURCES_ATTACHED"; }

	std::unique_ptr<IPlayerState> onPlaybackStarted() override;
	std::unique_ptr<IPlayerState> onFlush()           override;
	std::unique_ptr<IPlayerState> onStop()            override;
	std::unique_ptr<IPlayerState> onError()           override;
	std::unique_ptr<IPlayerState> onReconfigure()     override;
};

class PlayingState final : public IPlayerState
{
public:
	PlayerStateId id()   const override { return PlayerStateId::PLAYING; }
	const char   *name() const override { return "PLAYING"; }

	std::unique_ptr<IPlayerState> onPlaybackPaused()  override;
	std::unique_ptr<IPlayerState> onFlush()           override;
	std::unique_ptr<IPlayerState> onStop()            override;
	std::unique_ptr<IPlayerState> onError()           override;
	std::unique_ptr<IPlayerState> onReconfigure()     override;
	// Duplicate onPlaybackStarted while already PLAYING is handled at the
	// PlayerStateMachine level (see onPlaybackStarted() below) so dispatch()
	// never sees an unexpected null for this event.
};

class PausedState final : public IPlayerState
{
public:
	PlayerStateId id()   const override { return PlayerStateId::PAUSED; }
	const char   *name() const override { return "PAUSED"; }

	std::unique_ptr<IPlayerState> onPlaybackStarted() override;
	std::unique_ptr<IPlayerState> onFlush()           override;
	std::unique_ptr<IPlayerState> onStop()            override;
	std::unique_ptr<IPlayerState> onError()           override;
	std::unique_ptr<IPlayerState> onReconfigure()     override;
};

class FlushingState final : public IPlayerState
{
public:
	PlayerStateId id()   const override { return PlayerStateId::FLUSHING; }
	const char   *name() const override { return "FLUSHING"; }

	std::unique_ptr<IPlayerState> onFlushComplete() override;
	std::unique_ptr<IPlayerState> onError()         override;

	// onPlaybackStarted() and onPlaybackPaused() are intentionally NOT
	// overridden here.  Rialto guarantees that SEEK_DONE is always sent
	// before any subsequent PLAYING/PAUSED notification for the same flush
	// cycle, so a PLAYING/PAUSED notification cannot legitimately arrive
	// while still FLUSHING.  Should that assumption ever be violated,
	// dispatch() logs a WARN (no transition defined) rather than silently
	// mishandling the event - a safer failure mode than guessing.
	//
	// onFlushComplete() is the sole normal exit from FLUSHING: it fires when
	// SEEK_DONE arrives and unconditionally transitions to FLUSHED (see
	// FlushedState below), which then waits for Rialto's next PLAYING/PAUSED
	// notification to drive the machine onward.
	//
	// onStop() is not overridden: Stop() calls WaitForFlushToComplete()
	// before dispatching onStop(), so FLUSHING is never the current state
	// when onStop is dispatched.
	//
	// onReconfigure() is not overridden: Configure() calls
	// WaitForFlushToComplete() before onReconfigure() is fired, so FLUSHING
	// is never the current state at that point either.
};

/**
 * @class FlushedState
 * @brief Flush cycle complete (SEEK_DONE received); awaiting Rialto's next
 *        PLAYING/PAUSED notification to drive the machine onward.
 *
 * There is deliberately no memory of what state was active before the
 * flush started.  Rialto's own subsequent notification is the single
 * source of truth for whether playback resumed as PLAYING or PAUSED - this
 * relies on the guarantee that SEEK_DONE always precedes that notification
 * for the same flush cycle (see FlushingState above).
 *
 * If Flush() was called before playback ever started (e.g. an initial
 * position seek from SOURCES_ATTACHED), the machine simply remains in
 * FLUSHED until Stream()/play() is eventually issued and Rialto confirms
 * PLAYING - functionally equivalent to the old SOURCES_ATTACHED restore
 * path, without needing to name it separately.
 */
class FlushedState final : public IPlayerState
{
public:
	PlayerStateId id()   const override { return PlayerStateId::FLUSHED; }
	const char   *name() const override { return "FLUSHED"; }

	std::unique_ptr<IPlayerState> onPlaybackStarted() override;
	std::unique_ptr<IPlayerState> onPlaybackPaused()  override;
	std::unique_ptr<IPlayerState> onFlush()           override;
	std::unique_ptr<IPlayerState> onStop()            override;
	std::unique_ptr<IPlayerState> onError()           override;
	std::unique_ptr<IPlayerState> onReconfigure()     override;
};

// StoppedState is removed; onStop() now transitions directly to IDLE.
// The forward decl above is kept so the compiler does not complain if any
// stale reference exists during incremental builds.
class StoppedState final : public IPlayerState
{
public:
	PlayerStateId id()   const override { return PlayerStateId::STOPPED; }
	const char   *name() const override { return "STOPPED"; }
};

class ErrorState final : public IPlayerState
{
public:
	PlayerStateId id()   const override { return PlayerStateId::ERROR; }
	const char   *name() const override { return "ERROR"; }

	std::unique_ptr<IPlayerState> onStop()        override;
	std::unique_ptr<IPlayerState> onReconfigure() override;
	// onError() is not overridden: already in ERROR; a second fatal
	// notification would produce a no-op (dispatch warns) rather than
	// a spurious self-transition.
};

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

std::unique_ptr<IPlayerState> FlushingState::onFlushComplete()
{
	return std::make_unique<FlushedState>();
}

std::unique_ptr<IPlayerState> FlushedState::onPlaybackStarted()
{
	return std::make_unique<PlayingState>();
}

std::unique_ptr<IPlayerState> FlushedState::onPlaybackPaused()
{
	return std::make_unique<PausedState>();
}

std::unique_ptr<IPlayerState> FlushedState::onFlush()
{
	return std::make_unique<FlushingState>();
}

// onStop — valid from any active state (except FLUSHING; Stop waits for
// flush before dispatching, and IDLE has nothing to stop).
std::unique_ptr<IPlayerState> PipelineCreatedState::onStop()      { return std::make_unique<IdleState>(); }
std::unique_ptr<IPlayerState> SourcesAttachingState::onStop()     { return std::make_unique<IdleState>(); }
std::unique_ptr<IPlayerState> SourcesAttachedState::onStop()      { return std::make_unique<IdleState>(); }
std::unique_ptr<IPlayerState> PlayingState::onStop()              { return std::make_unique<IdleState>(); }
std::unique_ptr<IPlayerState> PausedState::onStop()               { return std::make_unique<IdleState>(); }
std::unique_ptr<IPlayerState> FlushedState::onStop()              { return std::make_unique<IdleState>(); }
std::unique_ptr<IPlayerState> ErrorState::onStop()                { return std::make_unique<IdleState>(); }

// onError — valid from any state in which a Rialto FAILURE notification
// can arrive (FLUSHING and FLUSHED included; IDLE and ERROR itself are
// excluded).
std::unique_ptr<IPlayerState> PipelineCreatedState::onError()     { return std::make_unique<ErrorState>(); }
std::unique_ptr<IPlayerState> SourcesAttachingState::onError()    { return std::make_unique<ErrorState>(); }
std::unique_ptr<IPlayerState> SourcesAttachedState::onError()     { return std::make_unique<ErrorState>(); }
std::unique_ptr<IPlayerState> PlayingState::onError()             { return std::make_unique<ErrorState>(); }
std::unique_ptr<IPlayerState> PausedState::onError()              { return std::make_unique<ErrorState>(); }
std::unique_ptr<IPlayerState> FlushingState::onError()            { return std::make_unique<ErrorState>(); }
std::unique_ptr<IPlayerState> FlushedState::onError()             { return std::make_unique<ErrorState>(); }

// onReconfigure — Configure() fires this on every tune, including first tune
// from IDLE, retune from any active state, and recovery after ERROR.
// Not valid from FLUSHING: Configure() calls WaitForFlushToComplete() before
// dispatching onReconfigure(), so FLUSHING is never the current state here.
std::unique_ptr<IPlayerState> IdleState::onReconfigure()          { return std::make_unique<IdleState>(); }
std::unique_ptr<IPlayerState> PipelineCreatedState::onReconfigure()   { return std::make_unique<IdleState>(); }
std::unique_ptr<IPlayerState> SourcesAttachingState::onReconfigure()  { return std::make_unique<IdleState>(); }
std::unique_ptr<IPlayerState> SourcesAttachedState::onReconfigure()   { return std::make_unique<IdleState>(); }
std::unique_ptr<IPlayerState> PlayingState::onReconfigure()       { return std::make_unique<IdleState>(); }
std::unique_ptr<IPlayerState> PausedState::onReconfigure()        { return std::make_unique<IdleState>(); }
std::unique_ptr<IPlayerState> ErrorState::onReconfigure()         { return std::make_unique<IdleState>(); }
std::unique_ptr<IPlayerState> FlushedState::onReconfigure()       { return std::make_unique<IdleState>(); }

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
	std::unique_ptr<IPlayerState> (IPlayerState::*handler)(),
	const char *eventName)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	auto next = (m_state.get()->*handler)();
	if (next)
	{
		AAMPLOG_MIL("PlayerState: %s -> %s",
			m_state->name(), next->name());
		m_state = std::move(next);
	}
	else
	{
		AAMPLOG_WARN("PlayerState: event '%s' has no transition from state '%s' - ignored",
			eventName, m_state->name());
	}
}

void PlayerStateMachine::onPipelineLoaded()
{
	dispatch(&IPlayerState::onPipelineLoaded, "onPipelineLoaded");
}

void PlayerStateMachine::onSourceAttaching()
{
	dispatch(&IPlayerState::onSourceAttaching, "onSourceAttaching");
}

void PlayerStateMachine::onAllSourcesAttached()
{
	dispatch(&IPlayerState::onAllSourcesAttached, "onAllSourcesAttached");
}

void PlayerStateMachine::onPlaybackStarted()
{
	{
		// Duplicate PLAYING notification while already PLAYING is tolerated
		// silently rather than logging a spurious "no transition" WARN.
		std::lock_guard<std::mutex> lock(m_mutex);
		if (m_state->id() == PlayerStateId::PLAYING)
		{
			AAMPLOG_INFO("PlayerState: duplicate onPlaybackStarted in PLAYING - ignored");
			return;
		}
	}
	dispatch(&IPlayerState::onPlaybackStarted, "onPlaybackStarted");
}

void PlayerStateMachine::onPlaybackPaused()
{
	dispatch(&IPlayerState::onPlaybackPaused, "onPlaybackPaused");
}

void PlayerStateMachine::onFlush()
{
	dispatch(&IPlayerState::onFlush, "onFlush");
}

void PlayerStateMachine::onFlushComplete()
{
	dispatch(&IPlayerState::onFlushComplete, "onFlushComplete");
}

void PlayerStateMachine::onStop()
{
	dispatch(&IPlayerState::onStop, "onStop");
}

void PlayerStateMachine::onError()
{
	dispatch(&IPlayerState::onError, "onError");
}

void PlayerStateMachine::onReconfigure()
{
	dispatch(&IPlayerState::onReconfigure, "onReconfigure");
}
