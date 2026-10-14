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
 * @file AampRialtoPlaybackController.cpp
 * @brief Implementation of the play()-arbitration controller.
 */

#include "AampRialtoPlaybackController.h"
#include "AampLogManager.h"

namespace {

const char *HoldName(PlayHold hold)
{
	const char *name = "Unknown";
	switch (hold)
	{
		case PlayHold::SourcesNotAttached:
			name = "SourcesNotAttached";
			break;
		case PlayHold::Flushing:
			name = "Flushing";
			break;
		case PlayHold::PositionPending:
			name = "PositionPending";
			break;
		case PlayHold::FragmentCaching:
			name = "FragmentCaching";
			break;
		case PlayHold::PreRollIncomplete:
			name = "PreRollIncomplete";
			break;
	}
	return name;
}

} // namespace

AampRialtoPlaybackController::AampRialtoPlaybackController(PlayAction onPlay,
	PreRollAction onPreRollReady)
	: m_onPlay(std::move(onPlay))
	, m_onPreRollReady(std::move(onPreRollReady))
	, m_holds(kInitialHolds)
	, m_playPending(false)
	, m_preRollReady(false)
{
}

void AampRialtoPlaybackController::EvaluateLocked(
	bool &issuePlay, bool &startPreRoll)
{
	issuePlay = ClaimPlayLocked();

	const bool preRollReady = m_playPending &&
		(m_holds.load(std::memory_order_relaxed) ==
			static_cast<uint32_t>(PlayHold::PreRollIncomplete));
	startPreRoll = preRollReady && !m_preRollReady;
	m_preRollReady = preRollReady;
}

void AampRialtoPlaybackController::Dispatch(
	bool issuePlay, bool startPreRoll, const char *reason,
	std::unique_lock<std::recursive_mutex> &playLock)
{
	if (startPreRoll && m_onPreRollReady)
	{
		AAMPLOG_INFO("only pre-roll remains - starting pre-roll (%s)", reason);
		playLock.unlock();
		m_onPreRollReady(reason);
		playLock.lock();
	}
	if (issuePlay)
	{
		m_onPlay(reason);
	}
}

bool AampRialtoPlaybackController::ClaimPlayLocked()
{
	bool claimed = false;
	if (m_playPending && (m_holds.load(std::memory_order_relaxed) == 0u))
	{
		m_playPending = false;
		claimed = true;
	}
	return claimed;
}

void AampRialtoPlaybackController::RequestPlay(const char *reason)
{
	std::unique_lock<std::recursive_mutex> playLock(m_playMutex);
	bool issuePlay = false;
	bool startPreRoll = false;
	uint32_t holds = 0u;
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		m_playPending = true;
		EvaluateLocked(issuePlay, startPreRoll);
		holds = m_holds.load(std::memory_order_relaxed);
	}

	if (issuePlay)
	{
		AAMPLOG_INFO("play requested by %s - issuing now", reason);
	}
	else
	{
		AAMPLOG_INFO("play requested by %s - deferred, holds=0x%x",
			reason, holds);
	}
	Dispatch(issuePlay, startPreRoll, reason, playLock);
}

void AampRialtoPlaybackController::CancelPlayRequest(const char *reason)
{
	std::unique_lock<std::recursive_mutex> playLock(m_playMutex);
	bool discarded = false;
	bool issuePlay = false;
	bool startPreRoll = false;
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		discarded = m_playPending;
		m_playPending = false;
		EvaluateLocked(issuePlay, startPreRoll);
	}

	if (discarded)
	{
		AAMPLOG_INFO("pending play request discarded by %s", reason);
	}
}

void AampRialtoPlaybackController::Reset(const char *reason)
{
	std::unique_lock<std::recursive_mutex> playLock(m_playMutex);
	bool discardedRequest = false;
	uint32_t droppedHolds = 0u;
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		discardedRequest = m_playPending;
		m_playPending = false;
		m_preRollReady = false;
		droppedHolds = m_holds.load(std::memory_order_relaxed) &
			~kInitialHolds;
		m_holds.store(kInitialHolds, std::memory_order_relaxed);
	}

	AAMPLOG_INFO("controller reset by %s (playRequestDiscarded=%d "
		"holdsDropped=0x%x)", reason, discardedRequest, droppedHolds);
}

void AampRialtoPlaybackController::AddHold(PlayHold hold, const char *reason)
{
	std::unique_lock<std::recursive_mutex> playLock(m_playMutex);
	const uint32_t bit = static_cast<uint32_t>(hold);
	bool changed = false;
	bool issuePlay = false;
	bool startPreRoll = false;
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		const uint32_t before = m_holds.load(std::memory_order_relaxed);
		changed = ((before & bit) == 0u);
		m_holds.store(before | bit, std::memory_order_relaxed);
		EvaluateLocked(issuePlay, startPreRoll);
	}

	if (changed)
	{
		AAMPLOG_INFO("hold %s applied by %s", HoldName(hold), reason);
	}
	Dispatch(issuePlay, startPreRoll, reason, playLock);
}

void AampRialtoPlaybackController::ReleaseHold(PlayHold hold, const char *reason)
{
	std::unique_lock<std::recursive_mutex> playLock(m_playMutex);
	const uint32_t bit = static_cast<uint32_t>(hold);
	bool changed = false;
	bool issuePlay = false;
	bool startPreRoll = false;
	uint32_t remaining = 0u;
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		const uint32_t before = m_holds.load(std::memory_order_relaxed);
		changed = ((before & bit) != 0u);
		remaining = before & ~bit;
		m_holds.store(remaining, std::memory_order_relaxed);
		EvaluateLocked(issuePlay, startPreRoll);
	}

	if (changed)
	{
		AAMPLOG_INFO("hold %s released by %s, remaining=0x%x",
			HoldName(hold), reason, remaining);
	}
	if (issuePlay)
	{
		AAMPLOG_INFO("all holds clear - issuing play deferred until %s", reason);
	}
	Dispatch(issuePlay, startPreRoll, reason, playLock);
}

bool AampRialtoPlaybackController::IsHeld(PlayHold hold) const
{
	return (m_holds.load(std::memory_order_relaxed) &
		static_cast<uint32_t>(hold)) != 0u;
}

bool AampRialtoPlaybackController::IsPlayPending() const
{
	std::lock_guard<std::mutex> lock(m_mutex);
	return m_playPending;
}
