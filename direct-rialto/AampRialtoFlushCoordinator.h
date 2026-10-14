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
 * @file AampRialtoFlushCoordinator.h
 * @brief Synchronisation of flush cycles: serialises the calls that start or
 *        end one, and lets other threads wait for the FLUSHING state to end.
 */

#ifndef AAMP_RIALTO_FLUSH_COORDINATOR_H
#define AAMP_RIALTO_FLUSH_COORDINATOR_H

#include "AampPlayerStateMachine.h"

#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>

/**
 * @class AampRialtoFlushCoordinator
 * @brief Owns the locks, the condition variable and the timeout behind a
 *        flush cycle, and performs every state-machine transition that enters
 *        or leaves FLUSHING.
 *
 * What it does not own: what a flush does to sources, the pipeline and the
 * playback controller.  The player passes the one piece of that work that
 * must happen between leaving FLUSHING and waking waiters (the rate commit of
 * an abandoned flush) to WaitForComplete().
 *
 * Lock order (outermost first): the serialisation lock from
 * SerialiseCalls(), then the internal mutex, then the state machine's own
 * mutex.  The internal mutex is never held across a call into the player.
 */
class AampRialtoFlushCoordinator
{
public:
	/// @param stateMachine Must outlive the coordinator.
	explicit AampRialtoFlushCoordinator(PlayerStateMachine &stateMachine);

	AampRialtoFlushCoordinator(const AampRialtoFlushCoordinator &) = delete;
	AampRialtoFlushCoordinator &operator=(
		const AampRialtoFlushCoordinator &) = delete;

	/// Upper bound on the wait for SEEK_DONE.  Generous because abandoning a
	/// flush that is merely slow would drop its position commit.
	void SetCompleteTimeout(std::chrono::milliseconds timeout)
	{
		m_completeTimeout = timeout;
	}

	/// Serialise whole Flush()/Stop()-style calls: the state check and the
	/// FLUSHING claim of one are only atomic with respect to another flush
	/// under this lock.  Recursive because Flush() recovers through Stop().
	/// Hold the returned lock for the length of the call.
	std::unique_lock<std::recursive_mutex> SerialiseCalls()
	{
		return std::unique_lock<std::recursive_mutex>(m_callMutex);
	}

	/**
	 * @brief Block until the state machine is not FLUSHING, or the timeout
	 *        elapses.
	 *
	 * On timeout the flush is abandoned: the state machine leaves FLUSHING,
	 * @p onAbandoned runs, then waiters are woken.  Without that a lost
	 * SEEK_DONE would block Stop(), the recovery path, forever.
	 *
	 * Takes no claim: the caller that wants FLUSHING calls ClaimFlushing()
	 * afterwards, so Stop() never deadlocks on a claim made by the Flush()
	 * that called it.
	 *
	 * @param onAbandoned Optional; runs without any coordinator lock held.
	 * @return true if the flush was abandoned.
	 */
	bool WaitForComplete(const std::function<void()> &onAbandoned = nullptr);

	/// Enter FLUSHING.  Done under the internal mutex, so a concurrent
	/// WaitForComplete() either sees FLUSHING and blocks, or finishes first.
	void ClaimFlushing();

	/// Leave FLUSHING (SEEK_DONE, or an early exit of Flush()) and wake every
	/// waiter.
	void Complete();

	/// Move the state machine to ERROR and wake every waiter, since a failure
	/// can arrive while FLUSHING.  @return the state before the transition.
	PlayerStateId SignalError();

private:
	PlayerStateMachine &m_stateMachine;
	std::recursive_mutex m_callMutex;
	std::mutex m_mutex;
	std::condition_variable m_cv;
	std::chrono::milliseconds m_completeTimeout{5000};
};

#endif // AAMP_RIALTO_FLUSH_COORDINATOR_H
