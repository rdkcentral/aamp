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
 * @file AampRialtoPlaybackController.h
 * @brief Arbitrates when Rialto's play() may be issued.
 */

#ifndef AAMP_RIALTO_PLAYBACK_CONTROLLER_H
#define AAMP_RIALTO_PLAYBACK_CONTROLLER_H

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>

/**
 * @brief Reasons that currently prevent play() from being issued.
 *
 * Values are bit flags so the complete set can be held in one atomic word
 * and read without locking.
 */
enum class PlayHold : uint32_t
{
	/// allSourcesAttached() has not yet been accepted by Rialto.
	SourcesNotAttached = 1u << 0,
	/// A flush cycle is in progress; Rialto has not yet sent SEEK_DONE.
	Flushing           = 1u << 1,
	/// No position has been established for the current period yet.
	PositionPending    = 1u << 2,
	/// AAMP is refilling its fragment cache and has paused the pipeline.
	FragmentCaching    = 1u << 3,
};

/**
 * @class AampRialtoPlaybackController
 * @brief Single decision point for "may playback start now?".
 *
 * The player records a play request (AAMP called Stream()) and, separately,
 * adds or releases the holds that describe why playback cannot start yet.
 * Whenever a request is outstanding and no hold remains, the controller
 * fires the injected play action exactly once.
 *
 * This replaces the previous arrangement in which several call sites each
 * re-derived their own preconditions and raced to call play(), coordinated
 * by a hand-rolled sequentially-consistent rendezvous between two atomics.
 *
 * Thread-safe.  The play action is always invoked with the internal mutex
 * released, so it may call back into this object and may safely perform
 * blocking Rialto IPC.
 */
class AampRialtoPlaybackController
{
public:
	/// Invoked when a play request becomes satisfiable.  @p reason is the
	/// caller-supplied description of whatever released the final hold.
	using PlayAction = std::function<void(const char *reason)>;

	/**
	 * @brief Construct the controller.
	 *
	 * Starts with PlayHold::SourcesNotAttached held, matching a freshly
	 * constructed player that has not yet attached any source.
	 *
	 * @param[in] onPlay  Non-null action invoked to issue play().
	 */
	explicit AampRialtoPlaybackController(PlayAction onPlay);

	AampRialtoPlaybackController(const AampRialtoPlaybackController &) = delete;
	AampRialtoPlaybackController &operator=(
		const AampRialtoPlaybackController &) = delete;

	/**
	 * @brief Record that playback is wanted, and start it if nothing holds.
	 *
	 * Repeated calls while unheld re-issue play() each time; AAMP treats
	 * Stream() as unconditional.
	 */
	void RequestPlay(const char *reason);

	/// Discard any outstanding play request without issuing play().
	void CancelPlayRequest(const char *reason);

	/// Add @p hold.  Never issues play().
	void AddHold(PlayHold hold, const char *reason);

	/// Release @p hold, issuing play() if a request is outstanding and no
	/// other hold remains.
	void ReleaseHold(PlayHold hold, const char *reason);

	/// True while @p hold is applied.  Lock-free.
	bool IsHeld(PlayHold hold) const;

	/// True while a play request is outstanding.
	bool IsPlayPending() const;

private:
	/// Consume the outstanding request if it can be satisfied now.
	/// Caller must hold m_mutex.
	bool ClaimPlayLocked();

	PlayAction m_onPlay;

	/// Serialises hold/request updates so that exactly one caller can
	/// consume a given play request.
	mutable std::mutex m_mutex;

	/// Bitwise OR of the applied PlayHold values.  Written under m_mutex,
	/// read without it.
	std::atomic<uint32_t> m_holds;

	/// True while a play request is outstanding.  Guarded by m_mutex.
	bool m_playPending;
};

#endif // AAMP_RIALTO_PLAYBACK_CONTROLLER_H
