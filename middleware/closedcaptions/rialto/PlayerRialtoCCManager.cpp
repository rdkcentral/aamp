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

/**
 * @brief stores Handle
 */
int PlayerRialtoCCManager::Initialize(void * handle)
{
	MW_LOG_INFO("PlayerRialtoCCManager::Initialize(%p) called", handle);

	bool changedHandle = (handle != mSubtitleControlHandle);

	mSubtitleControlHandle = handle;

	if (GetTrack().empty())
	{
		// Apps expect to render default CC as CC1, so set that here in case
		// they do not explicitly call SetTrack().
		MW_LOG_INFO("PlayerRialtoCCManager::Setting default to \"CC1\"");
		(void) SetTrack("CC1");
	}
	else if (changedHandle)
	{
		// Configure the new handle.
		(void) SetTrack(GetTrack());
	}

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
	MW_LOG_INFO("PlayerRialtoCCManager::id:%d,users:%d", mId, mIdSet.size());
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
		MW_LOG_INFO("PlayerRialtoCCManager::users:%d", id_size);

		if (0 == id_size)
		{
			// Last user has released.
			// Note that this instance can be re-used later.
			// Therefore, ensure the state is reset so that it is the same as a
			// newly constructed instance.
			ResetState();
		}
	}
	else
	{
		MW_LOG_WARN("PlayerRialtoCCManager::ID:%d not found", id);
	}

	return;
}

/**
 *  @brief Set CC track
 */
int PlayerRialtoCCManager::SetTrack(const std::string &track, const CCFormat format)
{
	mTrack = track;	// For PlayerCCManager::GetTrack()

	MW_LOG_INFO("PlayerRialtoCCManager::set track \"%s\"", track.c_str());

	if (nullptr != mSubtitleControlHandle)
	{
		g_object_set(mSubtitleControlHandle, "text-track-identifier", track.c_str(), NULL);
	}
	else
	{
		MW_LOG_INFO("PlayerRialtoCCManager::No current handle - track \"%s\" cached", track.c_str());
	}

	return 0;
}

/**
 *  @brief To start CC rendering
 */
void PlayerRialtoCCManager::StartRendering()
{
	MW_LOG_INFO("PlayerRialtoCCManager::unmuting");

	if (nullptr != mSubtitleControlHandle)
	{
		g_object_set(mSubtitleControlHandle, "mute", FALSE, NULL);
	}
	else
	{
		MW_LOG_INFO("PlayerRialtoCCManager::Failed to unmute");
	}
	return;
}

/**
 *  @brief To stop CC rendering
 */
void PlayerRialtoCCManager::StopRendering()
{
	MW_LOG_INFO("PlayerRialtoCCManager::muting");

	if (nullptr != mSubtitleControlHandle)
	{
		g_object_set(mSubtitleControlHandle, "mute", TRUE, NULL);
	}
	else
	{
		MW_LOG_INFO("PlayerRialtoCCManager::Failed to mute");
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
