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
 * @file PlayerDirectRialtoCCManager.cpp
 * @brief Implementation of PlayerDirectRialtoCCManager.
 */

#include "PlayerDirectRialtoCCManager.h"
#include "PlayerLogManager.h"

#include <cctype>
#include <string>

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

/*static*/
std::string PlayerDirectRialtoCCManager::mapTrackIdentifier(
	const std::string &track, CCFormat format)
{
	// If the track string already has an alphabetic prefix (e.g. "CC1",
	// "SERVICE3"), use it as-is.  If it starts with a digit, prepend the
	// appropriate prefix so the Rialto server can identify the CC service.
	// This mirrors the logic in PlayerRialtoCCManager::SetTrack().
	if (!track.empty() && std::isdigit(static_cast<unsigned char>(track[0])))
	{
		std::string prefix;
		if (format == eCLOSEDCAPTION_FORMAT_608)
		{
			prefix = "CC";
		}
		else if (format == eCLOSEDCAPTION_FORMAT_708)
		{
			prefix = "SERVICE";
		}
		return prefix + track;
	}
	return track;
}

// ---------------------------------------------------------------------------
// PlayerCCManagerBase overrides
// ---------------------------------------------------------------------------

int PlayerDirectRialtoCCManager::Initialize(void *handle)
{
	MW_LOG_INFO("ENTRY handle=%p", handle);

	auto *newControl = static_cast<IDirectRialtoCC *>(handle);
	const bool changedHandle = (newControl != m_control);
	m_control = newControl;

	if (m_control == nullptr)
	{
		MW_LOG_WARN("Initialize called with null handle");
		MW_LOG_INFO("EXIT");
		return 0;
	}

	if (GetTrack().empty())
	{
		// Apps expect CC1 as the default; apply it so the first frame
		// renders without an explicit SetTextTrack() call.
		MW_LOG_INFO("Setting default track CC1");
		(void) SetTrack("CC1");
	}
	else if (changedHandle)
	{
		// Re-apply the cached track on the new handle (e.g. re-tune).
		(void) SetTrack(GetTrack(), mTrackFormat);
	}

	MW_LOG_INFO("EXIT");
	return 0;
}

int PlayerDirectRialtoCCManager::GetId()
{
	std::lock_guard<std::mutex> lock(m_idLock);
	++m_id;
	m_idSet.insert(m_id);
	MW_LOG_INFO("id=%d users=%zu", m_id, m_idSet.size());
	return m_id;
}

void PlayerDirectRialtoCCManager::Release(int id)
{
	std::lock_guard<std::mutex> lock(m_idLock);
	if (m_idSet.erase(id) > 0)
	{
		MW_LOG_INFO("id=%d users=%zu", id, m_idSet.size());
		if (m_idSet.empty())
		{
			ResetState();
		}
	}
	else
	{
		MW_LOG_WARN("id=%d not found", id);
	}
}

int PlayerDirectRialtoCCManager::SetTrack(
	const std::string &track, CCFormat format)
{
	// Cache for re-application after Initialize().
	mTrack       = track;
	mTrackFormat = format;

	MW_LOG_INFO("track=\"%s\" format=%d", track.c_str(), static_cast<int>(format));

	if (m_control == nullptr)
	{
		MW_LOG_INFO("No control handle — track cached");
		return 0;
	}

	const std::string identifier = mapTrackIdentifier(track, format);
	MW_LOG_INFO("setTextTrackIdentifier=\"%s\"", identifier.c_str());
	m_control->setTextTrackIdentifier(identifier);
	return 0;
}

void PlayerDirectRialtoCCManager::StartRendering()
{
	MW_LOG_INFO("ENTRY — unmuting CC");
	if (m_control == nullptr)
	{
		MW_LOG_WARN("No control handle — cannot unmute");
		return;
	}
	m_control->setCCMute(false);
	MW_LOG_INFO("EXIT");
}

void PlayerDirectRialtoCCManager::StopRendering()
{
	MW_LOG_INFO("ENTRY — muting CC");
	if (m_control == nullptr)
	{
		MW_LOG_WARN("No control handle — cannot mute");
		return;
	}
	m_control->setCCMute(true);
	MW_LOG_INFO("EXIT");
}

void PlayerDirectRialtoCCManager::ResetState()
{
	MW_LOG_INFO("ENTRY");
	PlayerCCManagerBase::ResetState();
	m_control = nullptr;
	MW_LOG_INFO("EXIT");
}
