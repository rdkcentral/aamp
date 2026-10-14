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
 * @file AampRialtoFlushCoordinator.cpp
 * @brief Implementation of AampRialtoFlushCoordinator.
 */

#include "AampRialtoFlushCoordinator.h"
#include "AampLogManager.h"

AampRialtoFlushCoordinator::AampRialtoFlushCoordinator(
	PlayerStateMachine &stateMachine)
	: m_stateMachine(stateMachine)
{
}

bool AampRialtoFlushCoordinator::WaitForComplete(
	const std::function<void()> &onAbandoned)
{
	bool timedOut = false;
	{
		std::unique_lock<std::mutex> lock(m_mutex);
		// The predicate is evaluated under the mutex so that a following
		// ClaimFlushing() is atomic with the check: no concurrent caller can
		// pass the predicate and also claim FLUSHING before us.
		timedOut = !m_cv.wait_for(lock, m_completeTimeout, [this]()
		{
			return m_stateMachine.currentState() != PlayerStateId::FLUSHING;
		});
		if (timedOut)
		{
			// SEEK_DONE completes the flush under this same mutex, so the
			// state is still FLUSHING here.
			m_stateMachine.onFlushComplete();
		}
	}

	if (timedOut)
	{
		AAMPLOG_ERR("SEEK_DONE not received within %lld ms - abandoning flush",
			static_cast<long long>(m_completeTimeout.count()));
		if (onAbandoned)
		{
			onAbandoned();
		}
		m_cv.notify_all();
	}
	return timedOut;
}

void AampRialtoFlushCoordinator::ClaimFlushing()
{
	std::lock_guard<std::mutex> lock(m_mutex);
	m_stateMachine.onFlush();
}

void AampRialtoFlushCoordinator::Complete()
{
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		m_stateMachine.onFlushComplete();
	}
	m_cv.notify_all();
}

PlayerStateId AampRialtoFlushCoordinator::SignalError()
{
	PlayerStateId previous;
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		previous = m_stateMachine.currentState();
		m_stateMachine.onError();
	}
	m_cv.notify_all();
	return previous;
}
