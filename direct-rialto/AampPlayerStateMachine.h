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
 * @brief GoF State-pattern state machine for AampRialtoPlayer.
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
 *   STOPPED            — Stop() was called
 *   ERROR              — server reported a fatal error
 *
 * Every valid transition is logged at MILESTONE level via AAMPLOG_MIL.
 */

#ifndef AAMP_PLAYER_STATE_MACHINE_H
#define AAMP_PLAYER_STATE_MACHINE_H

#include <memory>
#include <mutex>

// ---------------------------------------------------------------------------
// State identifier enum
// ---------------------------------------------------------------------------

/**
 * @brief Identifies each discrete player lifecycle state.
 *
 * Used with PlayerStateMachine::currentState() so callers can inspect state
 * without depending on the internal IPlayerState hierarchy.
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
	STOPPED,            ///< Stop() was called
	ERROR               ///< Server reported a fatal error
};

// ---------------------------------------------------------------------------
// IPlayerState — GoF State interface (incomplete type visible to callers)
// ---------------------------------------------------------------------------

/**
 * @class IPlayerState
 * @brief Abstract base for all concrete player states (GoF State pattern).
 *
 * Each concrete state overrides only the event handlers that trigger a
 * valid transition from that state.  The default implementation of every
 * handler returns @c nullptr, meaning "stay in the current state".
 *
 * The concrete implementations live in PlayerStateMachine.cpp and are
 * internal to this translation unit — no external code should depend on
 * them directly.
 */
class IPlayerState
{
public:
	virtual ~IPlayerState() = default;

	/// Returns the enum identifier of this state.
	virtual PlayerStateId id() const = 0;

	/// Returns a human-readable name for logging.
	virtual const char *name() const = 0;

	// -----------------------------------------------------------------------
	// Event handlers (return a new state on transition, nullptr to stay put)
	// -----------------------------------------------------------------------

	/// Fired when IMediaPipeline::load() succeeds inside Configure().
	virtual std::unique_ptr<IPlayerState> onPipelineLoaded()     { return nullptr; }

	/// Fired when AttachVideoSource/AttachAudioSource calls attachSource()
	/// for the first time (source not yet attached).
	virtual std::unique_ptr<IPlayerState> onSourceAttaching()    { return nullptr; }

	/// Fired when CheckAllSourcesAttached() calls allSourcesAttached().
	virtual std::unique_ptr<IPlayerState> onAllSourcesAttached() { return nullptr; }

	/// Fired when notifyPlaybackState(PLAYING) is received from Rialto.
	virtual std::unique_ptr<IPlayerState> onPlaybackStarted()    { return nullptr; }

	/// Fired when notifyPlaybackState(PAUSED) is received from Rialto.
	virtual std::unique_ptr<IPlayerState> onPlaybackPaused()     { return nullptr; }

	/// Fired when Flush() is called.
	virtual std::unique_ptr<IPlayerState> onFlush()              { return nullptr; }

	/// Fired when all sources confirm SourceFlushedEvent, completing the
	/// flush cycle.  Only meaningful from FLUSHING; ignored from all other
	/// states so the edge-case race (Rialto PLAYING arriving before
	/// onFlushComplete) cannot cause a double-transition.
	virtual std::unique_ptr<IPlayerState> onFlushComplete()      { return nullptr; }

	/// Override in concrete states where Stop is a valid transition.
	virtual std::unique_ptr<IPlayerState> onStop()        { return nullptr; }

	/// Override in concrete states where a fatal Error is a valid transition.
	virtual std::unique_ptr<IPlayerState> onError()       { return nullptr; }

	/// Override in concrete states where a re-tune (Reconfigure) is valid.
	virtual std::unique_ptr<IPlayerState> onReconfigure() { return nullptr; }
};

// ---------------------------------------------------------------------------
// PlayerStateMachine — GoF Context
// ---------------------------------------------------------------------------

/**
 * @class PlayerStateMachine
 * @brief Owns the current IPlayerState and dispatches lifecycle events.
 *
 * Whenever a transition occurs, the old and new state names are logged at
 * MILESTONE level (AAMPLOG_MIL) so transitions can be tracked in production
 * logs.
 *
 * Thread-safe: all public methods are protected by an internal mutex so the
 * state machine can be driven from concurrent threads (e.g. IPC callback
 * thread firing onPlaybackStarted, AAMP thread firing onFlush).
 */
class PlayerStateMachine
{
public:
	PlayerStateMachine();
	~PlayerStateMachine(); ///< Defined in .cpp (IPlayerState must be complete)

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
	// Event dispatchers (each acquires the internal mutex)
	// -----------------------------------------------------------------------

	/// @see IPlayerState::onPipelineLoaded
	void onPipelineLoaded();

	/// @see IPlayerState::onSourceAttaching
	void onSourceAttaching();

	/// @see IPlayerState::onAllSourcesAttached
	void onAllSourcesAttached();

	/// @see IPlayerState::onPlaybackStarted
	void onPlaybackStarted();

	/// @see IPlayerState::onPlaybackPaused
	void onPlaybackPaused();

	/// @see IPlayerState::onFlush
	void onFlush();

	/// Fired when all sources report SourceFlushedEvent.
	///
	/// Restores the state that was current when onFlush() was called
	/// (PLAYING → PLAYING, PAUSED → PAUSED, SOURCES_ATTACHED →
	/// SOURCES_ATTACHED).  If the machine has already left FLUSHING
	/// via the edge-case onPlaybackStarted/Paused path, this is a no-op.
	void onFlushComplete();

	/// @see IPlayerState::onStop
	void onStop();

	/// @see IPlayerState::onError
	void onError();

	/// @see IPlayerState::onReconfigure
	void onReconfigure();

private:
	/// Execute a handler, apply the returned transition, and log MIL.
	/// Logs a WARN when the handler returns nullptr (no transition defined
	/// for this event from the current state) so unexpected events are visible
	/// in production logs.
	void dispatch(std::unique_ptr<IPlayerState> (IPlayerState::*handler)(),
	              const char *eventName);

	mutable std::mutex            m_mutex;
	std::unique_ptr<IPlayerState> m_state;

	/// State saved by onFlush() so that onFlushComplete() can restore it.
	/// Only meaningful while the machine is in FLUSHING; undefined otherwise.
	PlayerStateId m_preFlushStateId{PlayerStateId::IDLE};
};

#endif // AAMP_PLAYER_STATE_MACHINE_H
