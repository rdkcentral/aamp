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
 * @file FakePlayerThunderInterface.cpp
 * @brief Fake Player Thunder Interface
 */

#include "PlayerThunderInterface.h"

PlayerThunderInterface::PlayerThunderInterface(PlayerThunderAccessPlugin callsign)
{
}

PlayerThunderInterface::~PlayerThunderInterface()
{
}

bool PlayerThunderInterface::ActivatePlugin()
{
    return false;
}

bool PlayerThunderInterface::UnSubscribeEvent(std::string eventName)
{
    return false;
}

bool PlayerThunderInterface::SetVideoRectangle(int x, int y, int w, int h, std::string videoInputType, PlayerThunderAccessShim shim)
{
    return false;
}


void PlayerThunderInterface::RegisterAllEventsVideoin(std::function<void(std::string)> OnSignalChangedCb, std::function<void(std::string)> OnInputStatusChangedCb)
{
}

void PlayerThunderInterface::UnRegisterAllEventsVideoin()
{
}

void PlayerThunderInterface::StartHelperVideoin(int port, std::string videoInputType)
{
}

void PlayerThunderInterface::StopHelperVideoin(std::string videoInputType)
{
}

void PlayerThunderInterface::RegisterEventOnVideoStreamInfoUpdateHdmiin(std::function<void(PlayerVideoStreamInfoData)> videoInfoUpdatedMethodCb)
{
}

void PlayerThunderInterface::RegisterOnPlayerStatusOta(std::function<void(PlayerStatusData)> onPlayerStatusCb)
{
}

void PlayerThunderInterface::ReleaseOta()
{
}

void PlayerThunderInterface::StartOta(std::string url, std::string waylandDisplay, std::string preferredLanguagesString, std::string atsc_preferredLanguagesString, std::string preferredRenditionString, std::string atsc_preferredRenditionString)
{
}

void PlayerThunderInterface::StopOta()
{
}

std::string PlayerThunderInterface::GetAudioTracksOta(std::vector<PlayerAudioData> audData)
{
    std::string ret = "";
    return ret;
}

std::string PlayerThunderInterface::SetAudioTrackOta(int index, int primaryKey)
{
    std::string ret = "";
    return ret;
}

bool PlayerThunderInterface::GetTextTracksOta(std::vector<PlayerTextData> txtData)
{
    return false;
}

void PlayerThunderInterface::DisableContentRestrictionsOta(long grace, long time, bool eventChange)
{
}

void PlayerThunderInterface::EnableContentRestrictionsOta()
{
}

bool PlayerThunderInterface::InitRmf()
{
    return false;
}

bool PlayerThunderInterface::StartRmf(std::string url, std::function<void(std::string)> onPlayerStatusHandlerCb, std::function<void(std::string)> onPlayerErrorHandlerCb)
{
    return false;
}

void PlayerThunderInterface::SetPreferredAudioLanguages(PlayerPreferredAudioData data, PlayerThunderAccessShim shim)
{
}

void PlayerThunderInterface::StopRmf()
{
}

bool PlayerThunderInterface::DeleteWatermark(int layerID)
{
    return false;
}

bool PlayerThunderInterface::CreateWatermark(int layerID)
{
    return false;
}

bool PlayerThunderInterface::ShowWatermark(int opacity)
{
    return false;
}

bool PlayerThunderInterface::HideWatermark()
{
    return false;
}

bool PlayerThunderInterface::UpdateWatermark(int layerID, int sharedMemoryKey, int size)
{
    return false;
}

std::string PlayerThunderInterface::GetMetaDataWatermark()
{
    std::string ret = "";
    return ret;
}

bool PlayerThunderInterface::PersistentStoreSaveWatermark(const char* base64Image, std::string metaData)
{
    return false;
}

bool PlayerThunderInterface::PersistentStoreLoadWatermark(int layerID)
{
    return false;
}

bool PlayerThunderInterface::IsThunderAccess()
{
    return false;
}