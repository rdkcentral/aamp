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
 * @brief Transition table and event handling for PlayerStateMachine.
 */

#include "AampPlayerStateMachine.h"
#include "AampLogManager.h"

// ============================================================================
// Transition table
// ============================================================================

bool PlayerStateMachine::NextState(PlayerStateId from, Event event,
	PlayerStateId &to)
{
	using S = PlayerStateId;
	struct Transition
	{
		S from;
		Event event;
		S to;
	};
	// One row per allowed transition; any other (state, event) pair is
	// ignored.  Keep in step with docs/diagrams/direct-rialto-state-machine.puml.
	static constexpr Transition kTransitions[] = {
		{S::IDLE,              Event::PipelineLoaded,     S::PIPELINE_CREATED},
		{S::PIPELINE_CREATED,  Event::SourceAttaching,    S::SOURCES_ATTACHING},
		{S::SOURCES_ATTACHING, Event::AllSourcesAttached, S::SOURCES_ATTACHED},

		{S::SOURCES_ATTACHED,  Event::PlaybackStarted,    S::PLAYING},
		{S::PAUSED,            Event::PlaybackStarted,    S::PLAYING},
		{S::FLUSHED,           Event::PlaybackStarted,    S::PLAYING},

		// SOURCES_ATTACHED -> PAUSED is the pre-roll pause().
		{S::SOURCES_ATTACHED,  Event::PlaybackPaused,     S::PAUSED},
		{S::PLAYING,           Event::PlaybackPaused,     S::PAUSED},
		{S::FLUSHED,           Event::PlaybackPaused,     S::PAUSED},

		{S::SOURCES_ATTACHED,  Event::Flush,              S::FLUSHING},
		{S::PLAYING,           Event::Flush,              S::FLUSHING},
		{S::PAUSED,            Event::Flush,              S::FLUSHING},
		{S::FLUSHED,           Event::Flush,              S::FLUSHING},

		// The only exit from FLUSHING apart from Error.  FLUSHED keeps no
		// memory of the state before the flush: Rialto always sends SEEK_DONE
		// before the PLAYING/PAUSED of the same cycle, and that notification
		// moves the machine on.  PLAYING/PAUSED while FLUSHING would break
		// that guarantee, so they are left unhandled (WARN) rather than
		// guessed.
		{S::FLUSHING,          Event::FlushComplete,      S::FLUSHED},

		// Stop and Reconfigure are not handled in FLUSHING: Stop() and
		// Configure() wait for the flush to end before sending them.
		{S::PIPELINE_CREATED,  Event::Stop,               S::IDLE},
		{S::SOURCES_ATTACHING, Event::Stop,               S::IDLE},
		{S::SOURCES_ATTACHED,  Event::Stop,               S::IDLE},
		{S::PLAYING,           Event::Stop,               S::IDLE},
		{S::PAUSED,            Event::Stop,               S::IDLE},
		{S::FLUSHED,           Event::Stop,               S::IDLE},
		{S::ERROR,             Event::Stop,               S::IDLE},

		// Not from IDLE (no session) or ERROR (already reported).
		{S::PIPELINE_CREATED,  Event::Error,              S::ERROR},
		{S::SOURCES_ATTACHING, Event::Error,              S::ERROR},
		{S::SOURCES_ATTACHED,  Event::Error,              S::ERROR},
		{S::PLAYING,           Event::Error,              S::ERROR},
		{S::PAUSED,            Event::Error,              S::ERROR},
		{S::FLUSHING,          Event::Error,              S::ERROR},
		{S::FLUSHED,           Event::Error,              S::ERROR},

		{S::IDLE,              Event::Reconfigure,        S::IDLE},
		{S::PIPELINE_CREATED,  Event::Reconfigure,        S::IDLE},
		{S::SOURCES_ATTACHING, Event::Reconfigure,        S::IDLE},
		{S::SOURCES_ATTACHED,  Event::Reconfigure,        S::IDLE},
		{S::PLAYING,           Event::Reconfigure,        S::IDLE},
		{S::PAUSED,            Event::Reconfigure,        S::IDLE},
		{S::FLUSHED,           Event::Reconfigure,        S::IDLE},
		{S::ERROR,             Event::Reconfigure,        S::IDLE},
	};

	for (const Transition &t : kTransitions)
	{
		if (t.from == from && t.event == event)
		{
			to = t.to;
			return true;
		}
	}
	return false;
}

const char *PlayerStateMachine::StateName(PlayerStateId state)
{
	switch (state)
	{
		case PlayerStateId::IDLE:              return "IDLE";
		case PlayerStateId::PIPELINE_CREATED:  return "PIPELINE_CREATED";
		case PlayerStateId::SOURCES_ATTACHING: return "SOURCES_ATTACHING";
		case PlayerStateId::SOURCES_ATTACHED:  return "SOURCES_ATTACHED";
		case PlayerStateId::PLAYING:           return "PLAYING";
		case PlayerStateId::PAUSED:            return "PAUSED";
		case PlayerStateId::FLUSHING:          return "FLUSHING";
		case PlayerStateId::FLUSHED:           return "FLUSHED";
		case PlayerStateId::ERROR:             return "ERROR";
	}
	return "UNKNOWN";
}

const char *PlayerStateMachine::EventName(Event event)
{
	switch (event)
	{
		case Event::PipelineLoaded:     return "onPipelineLoaded";
		case Event::SourceAttaching:    return "onSourceAttaching";
		case Event::AllSourcesAttached: return "onAllSourcesAttached";
		case Event::PlaybackStarted:    return "onPlaybackStarted";
		case Event::PlaybackPaused:     return "onPlaybackPaused";
		case Event::Flush:              return "onFlush";
		case Event::FlushComplete:      return "onFlushComplete";
		case Event::Stop:               return "onStop";
		case Event::Error:              return "onError";
		case Event::Reconfigure:        return "onReconfigure";
	}
	return "unknown";
}

// ============================================================================
// PlayerStateMachine
// ============================================================================

PlayerStateMachine::PlayerStateMachine() = default;

PlayerStateMachine::~PlayerStateMachine() = default;

PlayerStateId PlayerStateMachine::currentState() const
{
	std::lock_guard<std::mutex> lock(m_mutex);
	return m_state;
}

const char *PlayerStateMachine::currentStateName() const
{
	std::lock_guard<std::mutex> lock(m_mutex);
	return StateName(m_state);
}

PlayerStateId PlayerStateMachine::dispatch(Event event)
{
	PlayerStateId previous;
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		previous = dispatchLocked(event);
	}
	m_stateChanged.notify_all();
	return previous;
}

PlayerStateId PlayerStateMachine::dispatchLocked(Event event)
{
	const PlayerStateId previous = m_state;
	PlayerStateId next = previous;
	if (NextState(previous, event, next))
	{
		AAMPLOG_MIL("PlayerState: %s -> %s",
			StateName(previous), StateName(next));
		m_state = next;
	}
	else
	{
		AAMPLOG_WARN("PlayerState: event '%s' has no transition from state '%s' - ignored",
			EventName(event), StateName(previous));
	}
	return previous;
}

void PlayerStateMachine::onPipelineLoaded()
{
	dispatch(Event::PipelineLoaded);
}

void PlayerStateMachine::onSourceAttaching()
{
	{
		// Only the first source moves the machine on; later sources, and
		// sources added once all are attached, are not errors.
		std::lock_guard<std::mutex> lock(m_mutex);
		if (m_state != PlayerStateId::IDLE
			&& m_state != PlayerStateId::PIPELINE_CREATED
			&& m_state != PlayerStateId::ERROR)
		{
			AAMPLOG_TRACE("PlayerState: onSourceAttaching in %s - already past "
				"PIPELINE_CREATED, ignored", StateName(m_state));
			return;
		}
	}
	dispatch(Event::SourceAttaching);
}

void PlayerStateMachine::onAllSourcesAttached()
{
	dispatch(Event::AllSourcesAttached);
}

void PlayerStateMachine::onPlaybackStarted()
{
	{
		// Duplicate PLAYING notification while already PLAYING is tolerated
		// silently rather than logging a spurious "no transition" WARN.
		std::lock_guard<std::mutex> lock(m_mutex);
		if (m_state == PlayerStateId::PLAYING)
		{
			AAMPLOG_INFO("PlayerState: duplicate onPlaybackStarted in PLAYING - ignored");
			return;
		}
	}
	dispatch(Event::PlaybackStarted);
}

void PlayerStateMachine::onPlaybackPaused()
{
	dispatch(Event::PlaybackPaused);
}

void PlayerStateMachine::onFlush()
{
	dispatch(Event::Flush);
}

void PlayerStateMachine::onFlushComplete()
{
	dispatch(Event::FlushComplete);
}

bool PlayerStateMachine::waitForFlushComplete(
	std::chrono::milliseconds timeout,
	const std::function<void()> &onAbandoned)
{
	bool timedOut = false;
	{
		std::unique_lock<std::mutex> lock(m_mutex);
		timedOut = !m_stateChanged.wait_for(lock, timeout, [this]()
		{
			return m_state != PlayerStateId::FLUSHING;
		});
		if (timedOut)
		{
			// SEEK_DONE completes the flush under this same mutex, so the
			// state is still FLUSHING here.
			dispatchLocked(Event::FlushComplete);
		}
	}

	if (timedOut)
	{
		AAMPLOG_ERR("SEEK_DONE not received within %lld ms - abandoning flush",
			static_cast<long long>(timeout.count()));
		if (onAbandoned)
		{
			onAbandoned();
		}
		m_stateChanged.notify_all();
	}
	return timedOut;
}

void PlayerStateMachine::onStop()
{
	{
		// Stop() on an idle player, or one that is already stopped, is normal.
		std::lock_guard<std::mutex> lock(m_mutex);
		if (m_state == PlayerStateId::IDLE)
		{
			AAMPLOG_TRACE("PlayerState: onStop in IDLE - nothing to stop");
			return;
		}
	}
	dispatch(Event::Stop);
}

PlayerStateId PlayerStateMachine::onError()
{
	return dispatch(Event::Error);
}

void PlayerStateMachine::onReconfigure()
{
	dispatch(Event::Reconfigure);
}
