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
 * @file AampPlayerStateMachine.h
 * @brief Lifecycle state machine for AampRialtoPlayer.
 *
 * States
 * ------
 *   IDLE               — constructed; no Rialto pipeline
 *   PIPELINE_CREATED   — load() succeeded
 *   SOURCES_ATTACHING  — waiting for all attachSource() calls to complete
 *   SOURCES_ATTACHED   — allSourcesAttached() sent to Rialto server
 *   PLAYING            — server confirmed PLAYING
 *   PAUSED             — server confirmed PAUSED
 *   FLUSHING           — Flush() in progress; new segment arrival expected
 *   FLUSHED            — SEEK_DONE received; awaiting Rialto's next
 *                         PLAYING/PAUSED notification to drive the machine
 *                         onward (Rialto always sends SEEK_DONE before any
 *                         subsequent PLAYING/PAUSED for the same flush cycle)
 *   ERROR              — server reported a fatal error
 *
 * Stop() returns the machine to IDLE; there is no separate stopped state.
 *
 * Every valid transition is logged at MILESTONE level via AAMPLOG_MIL.  The
 * transitions are one table in AampPlayerStateMachine.cpp, drawn in
 * docs/diagrams/direct-rialto-state-machine.puml.
 */

#ifndef AAMP_PLAYER_STATE_MACHINE_H
#define AAMP_PLAYER_STATE_MACHINE_H

#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>

// ---------------------------------------------------------------------------
// State identifier enum
// ---------------------------------------------------------------------------

/**
 * @brief Identifies each discrete player lifecycle state.
 *
 * Returned by PlayerStateMachine::currentState().
 */
enum class PlayerStateId
{
	IDLE,               ///< Constructed; no pipeline
	PIPELINE_CREATED,   ///< load() succeeded
	SOURCES_ATTACHING,  ///< Waiting for attachSource() calls
	SOURCES_ATTACHED,   ///< allSourcesAttached() sent to Rialto
	PLAYING,            ///< Server confirmed PLAYING
	PAUSED,             ///< Server confirmed PAUSED
	FLUSHING,           ///< Flush in progress; new segment arrival expected
	FLUSHED,            ///< SEEK_DONE received; awaiting Rialto PLAYING/PAUSED
	ERROR               ///< Server reported a fatal error
};


// ---------------------------------------------------------------------------
// PlayerStateMachine
// ---------------------------------------------------------------------------

/**
 * @class PlayerStateMachine
 * @brief Holds the current PlayerStateId and applies lifecycle events to it.
 *
 * Each event moves the machine only from the states listed for it in the
 * transition table; from any other state it is ignored with a WARN (or a
 * TRACE for the expected repeats noted on onSourceAttaching(),
 * onPlaybackStarted() and onStop()).
 *
 * Thread-safe: all public methods are protected by an internal mutex so the
 * state machine can be driven from concurrent threads (e.g. IPC callback
 * thread firing onPlaybackStarted, AAMP thread firing onFlush).  Each call is
 * atomic on its own; a sequence such as "read currentState(), then act" is
 * not.  The one sequence that must be atomic (entering FLUSHING after waiting
 * for a previous flush) is serialised by AampRialtoPlayer::m_flushCallMutex.
 */
class PlayerStateMachine
{
public:
	PlayerStateMachine();
	~PlayerStateMachine();

	PlayerStateMachine(const PlayerStateMachine &) = delete;
	PlayerStateMachine &operator=(const PlayerStateMachine &) = delete;

	// -----------------------------------------------------------------------
	// Observers
	// -----------------------------------------------------------------------

	/// Returns the current state identifier (thread-safe snapshot).
	PlayerStateId currentState() const;

	/// Returns the current state name as a C string (thread-safe snapshot).
	const char *currentStateName() const;

	// -----------------------------------------------------------------------
	// Events (each acquires the internal mutex)
	// -----------------------------------------------------------------------

	/// IMediaPipeline::load() succeeded.
	void onPipelineLoaded();

	/// A source is being attached.  Only the first source moves the machine
	/// on; ignored without a WARN once past PIPELINE_CREATED.
	void onSourceAttaching();

	/// allSourcesAttached() was sent to Rialto.
	void onAllSourcesAttached();

	/// Rialto reported PLAYING.  A repeat while PLAYING is ignored quietly.
	void onPlaybackStarted();

	/// Rialto reported PAUSED.
	void onPlaybackPaused();

	/// Flush() is starting a flush cycle.
	void onFlush();

	/// Fired when PlaybackState::SEEK_DONE is received from Rialto, or when a
	/// flush ends early.
	///
	/// Transitions FLUSHING -> FLUSHED unconditionally.  Rialto always sends
	/// SEEK_DONE before any subsequent PLAYING/PAUSED notification for the
	/// same flush cycle, so FLUSHED simply waits for that notification to
	/// arrive and drive the machine onward via onPlaybackStarted()/
	/// onPlaybackPaused().  A no-op (WARN logged) if not currently FLUSHING.
	void onFlushComplete();

	/**
	 * @brief Block until the machine is not FLUSHING, or @p timeout elapses.
	 *
	 * On timeout the flush is abandoned: the machine leaves FLUSHING as if
	 * onFlushComplete() had fired, @p onAbandoned runs (without the internal
	 * lock held), then waiters are woken.  Without that a lost SEEK_DONE would
	 * block Stop(), the recovery path, forever.
	 *
	 * Takes no claim: a caller that wants FLUSHING calls onFlush() afterwards
	 * and must serialise the two itself.
	 *
	 * Every transition wakes the threads waiting here.
	 *
	 * @return true if the flush was abandoned.
	 */
	bool waitForFlushComplete(std::chrono::milliseconds timeout,
		const std::function<void()> &onAbandoned = nullptr);

	/// Stop(): back to IDLE.  Ignored without a WARN when already IDLE.
	void onStop();

	/// Rialto reported a fatal error.
	/// @return the state the machine was in before the transition.
	PlayerStateId onError();

	/// Configure() is starting a new session: back to IDLE.
	void onReconfigure();

private:
	/// The events of the transition table.
	enum class Event
	{
		PipelineLoaded,
		SourceAttaching,
		AllSourcesAttached,
		PlaybackStarted,
		PlaybackPaused,
		Flush,
		FlushComplete,
		Stop,
		Error,
		Reconfigure
	};

	/// Look @p event up in the transition table.
	/// @return true and @p to if @p event moves the machine on from @p from.
	static bool NextState(PlayerStateId from, Event event, PlayerStateId &to);

	static const char *StateName(PlayerStateId state);
	static const char *EventName(Event event);

	/// Apply @p event, log the transition (MIL) or its absence (WARN), and
	/// wake waitForFlushComplete() callers.
	/// @return the state before the event.
	PlayerStateId dispatch(Event event);

	/// As dispatch() for a caller that already holds m_mutex and wakes the
	/// waiters itself.
	PlayerStateId dispatchLocked(Event event);

	mutable std::mutex      m_mutex;
	std::condition_variable m_stateChanged;
	PlayerStateId           m_state{PlayerStateId::IDLE};
};

#endif // AAMP_PLAYER_STATE_MACHINE_H
