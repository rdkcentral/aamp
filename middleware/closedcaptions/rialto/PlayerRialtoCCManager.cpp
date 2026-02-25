/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2025 RDK Management
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
 *  @file PlayerRialtoCCManager.cpp
 *
 *  @brief Impl of Rialto ClosedCaption integration layer
 *
 */
#include "PlayerRialtoCCManager.h"
#include "PlayerLogManager.h" // Included for MW_LOG
#include <glib-object.h>  // Included for g_object_set
#include <cctype> // std::isdigit()

/**
 * @brief stores Handle
 */
int PlayerRialtoCCManager::Initialize(void * handle)
{
	MW_LOG_INFO("[INBAND_CC_FLOW] PlayerRialtoCCManager::Initialize(%p) called", handle);

	bool changedHandle = (handle != mSubtitleControlHandle);

	mSubtitleControlHandle = handle;
	MW_LOG_INFO("[INBAND_CC_FLOW] PlayerRialtoCCManager::Initialize: mSubtitleControlHandle set to %p, changedHandle=%d", mSubtitleControlHandle, changedHandle);

	if (GetTrack().empty())
	{
		// Apps expect to render default CC as CC1, so set that here in case
		// they do not explicitly call SetTrack().
		MW_LOG_INFO("[INBAND_CC_FLOW] PlayerRialtoCCManager::Setting default to \"CC1\"");
		(void) SetTrack("CC1");
	}
	else if (changedHandle)
	{
		// Configure the new handle.
		MW_LOG_INFO("[INBAND_CC_FLOW] PlayerRialtoCCManager::Handle changed, reconfiguring track: %s", GetTrack().c_str());
		(void) SetTrack(GetTrack(), mTrackFormat);
	}

	MW_LOG_INFO("[INBAND_CC_FLOW] PlayerRialtoCCManager::Initialize: EXIT - returning 0");
	return 0;
}

/**
 *  @brief Gets Handle or ID, Every client using subtec must call GetId in the beginning, save id, which is required for Release function.
 */
int PlayerRialtoCCManager::GetId()
{
    std::lock_guard<std::mutex> lock(mIdLock);
    mId++;
    mIdSet.insert(mId);
	MW_LOG_INFO("[INBAND_CC_FLOW] PlayerRialtoCCManager::GetId: Returning id=%d, total users=%d", mId, mIdSet.size());
    return mId;
}

/**
 *  @brief Reset internal state.
 */
void PlayerRialtoCCManager::ResetState()
{
	MW_LOG_INFO("PlayerRialtoCCManager::Resetting");
	PlayerCCManagerBase::ResetState();
	mSubtitleControlHandle = nullptr;
}

/**
 *  @brief Release CC resources
 */
void PlayerRialtoCCManager::Release(int id)
{
    std::lock_guard<std::mutex> lock(mIdLock);
    if (mIdSet.erase(id) > 0)
    {
		int id_size = mIdSet.size();
		MW_LOG_INFO("[INBAND_CC_FLOW] PlayerRialtoCCManager::Release: Removed id=%d, remaining users=%d", id, id_size);

		if (0 == id_size)
		{
			// Last user has released.
			// Note that this instance can be re-used later.
			// Therefore, ensure the state is reset so that it is the same as a
			// newly constructed instance.
			MW_LOG_INFO("[INBAND_CC_FLOW] PlayerRialtoCCManager::Release: Last user released, resetting state");
			ResetState();
		}
	}
	else
	{
		MW_LOG_WARN("[INBAND_CC_FLOW] PlayerRialtoCCManager::Release: ID=%d not found in tracked users", id);
	}

	return;
}

/**
 *  @brief Set CC track
 */
int PlayerRialtoCCManager::SetTrack(const std::string &track, const CCFormat format)
{
	// Cache the original track string and the format so the prefix (if any)
	// can be re-applied correctly from the cached values.
	mTrack = track;	// For PlayerCCManager::GetTrack()
	mTrackFormat = format;

	MW_LOG_INFO("[INBAND_CC_FLOW] PlayerRialtoCCManager::SetTrack: ENTRY - track=\"%s\", format=%d", track.c_str(), format);

	if (nullptr != mSubtitleControlHandle)
	{
		MW_LOG_INFO("[INBAND_CC_FLOW] PlayerRialtoCCManager::SetTrack: Calling g_object_set with text-track-identifier=\"%s\"", track.c_str());
		// We expect 'track' to have an alphabetic prefix. If it does not,
		// add one based on 'format'.
		std::string textTrackIdentifier;
		if (!track.empty() && std::isdigit(static_cast<unsigned char>(track[0])))
		{
			if (eCLOSEDCAPTION_FORMAT_608 == format)
			{
				textTrackIdentifier = "CC";
			}
			else if (eCLOSEDCAPTION_FORMAT_708 == format)
			{
				textTrackIdentifier = "SERVICE";
			}
		}
		textTrackIdentifier += track;

		MW_LOG_INFO("PlayerRialtoCCManager::set track (modified) \"%s\"", textTrackIdentifier.c_str());

		g_object_set(mSubtitleControlHandle, "text-track-identifier", textTrackIdentifier.c_str(), NULL);
		MW_LOG_INFO("[INBAND_CC_FLOW] PlayerRialtoCCManager::SetTrack: g_object_set completed successfully");
	}
	else
	{
		MW_LOG_WARN("[INBAND_CC_FLOW] PlayerRialtoCCManager::SetTrack: mSubtitleControlHandle is NULL! Track \"%s\" cached (will be applied later)", track.c_str());
	}

	return 0;
}

/**
 *  @brief To start CC rendering
 */
void PlayerRialtoCCManager::StartRendering()
{
	MW_LOG_INFO("[INBAND_CC_FLOW] PlayerRialtoCCManager::StartRendering: ENTRY");

	if (nullptr != mSubtitleControlHandle)
	{
		MW_LOG_INFO("[INBAND_CC_FLOW] PlayerRialtoCCManager::StartRendering: Calling g_object_set to unmute subtitles");
		g_object_set(mSubtitleControlHandle, "mute", FALSE, NULL);
		MW_LOG_INFO("[INBAND_CC_FLOW] PlayerRialtoCCManager::StartRendering: Unmute completed");
	}
	else
	{
		MW_LOG_WARN("[INBAND_CC_FLOW] PlayerRialtoCCManager::StartRendering: mSubtitleControlHandle is NULL! Cannot unmute");
	}
	return;
}

/**
 *  @brief To stop CC rendering
 */
void PlayerRialtoCCManager::StopRendering()
{
	MW_LOG_INFO("[INBAND_CC_FLOW] PlayerRialtoCCManager::StopRendering: ENTRY");

	if (nullptr != mSubtitleControlHandle)
	{
		MW_LOG_INFO("[INBAND_CC_FLOW] PlayerRialtoCCManager::StopRendering: Calling g_object_set to mute subtitles");
		g_object_set(mSubtitleControlHandle, "mute", TRUE, NULL);
		MW_LOG_INFO("[INBAND_CC_FLOW] PlayerRialtoCCManager::StopRendering: Mute completed");
	}
	else
	{
		MW_LOG_WARN("[INBAND_CC_FLOW] PlayerRialtoCCManager::StopRendering: mSubtitleControlHandle is NULL! Cannot mute");
	}
	return;
}

/* NOTE WELL: SetDigitalChannel() and SetAnalogChannel() should never be
** called as they are only called from the base class implementation of
** SetTrack(), which we override.
**
** However, they are declared pure virtual in the base class, so we need
** these stubs to satisfy that.
** Further, their return code is strictly an enum which is subtec-specific
** (CC_VL_OS_API_RESULT), so this should be moved from the base class to
** the subtec class.
*/

/**
 * @fn SetDigitalChannel
 *
 * @return CC_VL_OS_API_RESULT
 */
int PlayerRialtoCCManager::SetDigitalChannel(unsigned int id)
{
	MW_LOG_ERR("PlayerRialtoCCManager::Should not be called! (%u)", id);
	return 0;
}

/**
 * @fn SetAnalogChannel
 *
 * @return CC_VL_OS_API_RESULT
 */
int PlayerRialtoCCManager::SetAnalogChannel(unsigned int id)
{
	MW_LOG_ERR("PlayerRialtoCCManager::Should not be called! (%u)", id);
	return 0;
}

/**
 *  @brief Constructor
 */
PlayerRialtoCCManager::PlayerRialtoCCManager()
{
	return;
}
