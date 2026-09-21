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
	}
	return name;
}

} // namespace

AampRialtoPlaybackController::AampRialtoPlaybackController(PlayAction onPlay)
	: m_onPlay(std::move(onPlay))
	, m_holds(static_cast<uint32_t>(PlayHold::SourcesNotAttached))
	, m_playPending(false)
{
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
	bool issuePlay = false;
	uint32_t holds = 0u;
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		m_playPending = true;
		issuePlay = ClaimPlayLocked();
		holds = m_holds.load(std::memory_order_relaxed);
	}

	if (issuePlay)
	{
		AAMPLOG_INFO("play requested by %s - issuing now", reason);
		m_onPlay(reason);
	}
	else
	{
		AAMPLOG_INFO("play requested by %s - deferred, holds=0x%x",
			reason, holds);
	}
}

void AampRialtoPlaybackController::CancelPlayRequest(const char *reason)
{
	bool discarded = false;
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		discarded = m_playPending;
		m_playPending = false;
	}

	if (discarded)
	{
		AAMPLOG_INFO("pending play request discarded by %s", reason);
	}
}

void AampRialtoPlaybackController::AddHold(PlayHold hold, const char *reason)
{
	const uint32_t bit = static_cast<uint32_t>(hold);
	bool changed = false;
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		const uint32_t before = m_holds.load(std::memory_order_relaxed);
		changed = ((before & bit) == 0u);
		m_holds.store(before | bit, std::memory_order_relaxed);
	}

	if (changed)
	{
		AAMPLOG_INFO("hold %s applied by %s", HoldName(hold), reason);
	}
}

void AampRialtoPlaybackController::ReleaseHold(PlayHold hold, const char *reason)
{
	const uint32_t bit = static_cast<uint32_t>(hold);
	bool changed = false;
	bool issuePlay = false;
	uint32_t remaining = 0u;
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		const uint32_t before = m_holds.load(std::memory_order_relaxed);
		changed = ((before & bit) != 0u);
		remaining = before & ~bit;
		m_holds.store(remaining, std::memory_order_relaxed);
		issuePlay = ClaimPlayLocked();
	}

	if (changed)
	{
		AAMPLOG_INFO("hold %s released by %s, remaining=0x%x",
			HoldName(hold), reason, remaining);
	}

	if (issuePlay)
	{
		AAMPLOG_INFO("all holds clear - issuing play deferred until %s", reason);
		m_onPlay(reason);
	}
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
