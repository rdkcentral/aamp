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

/**
 * @brief stores Handle
 */
int PlayerRialtoCCManager::Initialize(void * handle)
{
	MW_LOG_WARN("PlayerRialtoCCManager::Initialize(%p) called", handle);
	mSubtitleControlHandle = handle;
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
	MW_LOG_WARN("PlayerRialtoCCManager::id:%d,users:%d", mId, mIdSet.size());
    return mId;
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
		MW_LOG_WARN("PlayerRialtoCCManager::users:%d", id_size);

		if (0 == id_size)
		{
			// Last user has released - deinit.

			MW_LOG_WARN("PlayerRialtoCCManager::Would de-init");
			MW_LOG_TRACE("PlayerRialtoCCManager::NotImplemented:{{DEINIT}}");
		}
	}
	else
	{
		MW_LOG_TRACE("PlayerRialtoCCManager::ID:%d not found", id);
	}

	return;
}

/**
 *  @brief To start CC rendering
 */
void PlayerRialtoCCManager::StartRendering()
{
	MW_LOG_TRACE("PlayerRialtoCCManager::NotImplemented -");
	MW_LOG_TRACE("PlayerRialtoCCManager::{{UNMUTE}}:%p", mSubtitleControlHandle);
	return;
}

/**
 *  @brief To stop CC rendering
 */
void PlayerRialtoCCManager::StopRendering()
{
	MW_LOG_TRACE("PlayerRialtoCCManager::NotImplemented -");
	MW_LOG_TRACE("PlayerRialtoCCManager::{{MUTE}}:%p", mSubtitleControlHandle);
	return;
}

/**
 *  @brief set digital channel with specified id
 */
int PlayerRialtoCCManager::SetDigitalChannel(unsigned int id)
{
	MW_LOG_TRACE("PlayerRialtoCCManager::NotImplemented -");
	MW_LOG_TRACE("PlayerRialtoCCManager::{{TEXT-TRACK-IDENTIFIER}}:%p:%u",
		mSubtitleControlHandle, id);
	return 0;
}

/**
 *  @brief set analog channel with specified id
 */
int PlayerRialtoCCManager::SetAnalogChannel(unsigned int id)
{
	MW_LOG_TRACE("PlayerRialtoCCManager::NotImplemented -");
	MW_LOG_TRACE("PlayerRialtoCCManager::{{TEXT-TRACK-IDENTIFIER}}:%p:%u",
		mSubtitleControlHandle, id);
	return 0;
}

/**
 *  @brief Constructor
 */
PlayerRialtoCCManager::PlayerRialtoCCManager()
{
	// Apps expect to render default CC as CC1, so set that here in case
	// they do not explicitly call SetTrack().
	MW_LOG_WARN("PlayerRialtoCCManager::Setting default to \"CC1\"");
	SetTrack("CC1");
	return;
}
