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
 * @file PlayerThunderInterface.cpp
 * @brief interface between player and thunder
 */

#include "PlayerThunderInterface.h"

#ifdef USE_CPP_THUNDER_PLUGIN_ACCESS
#include "PlayerThunderAccess.h"
#endif

/**
 * @class FakeThunderAccess
 * @brief Fake Thunder Plugin Access from Player
 */
class FakeThunderAccess : public PlayerThunderAccessBase
{
public:
    /**
     *   @fn FakeThunderAccess
     *   @note   Security token acquisition, controller object creation
     *   @param   callsign plugin callsign
     */
    FakeThunderAccess(PlayerThunderAccessPlugin callsign): PlayerThunderAccessBase(callsign){}

    /**
     *   @fn ~FakeThunderAccess
     *   @note   clean up
     */
    ~FakeThunderAccess(){}

    /**
     *   @brief  FakeThunderAccess copy constructor disabled
     */
    FakeThunderAccess(const FakeThunderAccess&) = delete;

    /**
     *   @brief  FakeThunderAccess assignment disabled
     */
    FakeThunderAccess& operator=(const FakeThunderAccess&) = delete;

    /**
     *   @brief  ActivatePlugin
     *   @note   Plugin activation and Remote object creation
     *   @param  Plugin Callsign
     *   @retval true on success
     *   @retval false on failure
     */
    bool ActivatePlugin() override {return false;}

    /**
     *   @fn UnSubscribeEvent
     *   @note   unSubscribe event data for the specific plugin
     *   @param  eventName Event name
     *   @retval true on success
     *   @retval false on failure
     */
    bool UnSubscribeEvent (std::string eventName) override {return false;}

    /**
     *   @fn SetVideoRectangle
     *   @note   Set video rectangle dimensions
     *   @param  x x offset
     *   @param  y y offset
     *   @param  w width
     *   @param  h height
     *   @param  videoInputType string video input type
     *   @param  shim shim to set video rectangle for
     *   @retval true if success
     *   @retval false if failure
     */
    bool SetVideoRectangle(int x, int y, int w, int h, std::string videoInputType, PlayerThunderAccessShim shim) override {return false;}

    /**
     *   @fn SetPreferredAudioLanguages_OTA
     *   @param data player's input on preferred languages
     *   @param shim shim to set preferred language for
     */
    void SetPreferredAudioLanguages(PlayerPreferredAudioData data, PlayerThunderAccessShim shim) override {}

    /**
     *   @fn RegisterAllEventsVideoin
     *   @param OnSignalChangedCb callback for when signal change
     *   @param OnInputStatusChangedCb callback for when input status changes
     */
    void RegisterAllEventsVideoin(std::function<void(std::string)> OnSignalChangedCb, std::function<void(std::string)> OnInputStatusChangedCb) override {}

    /**
     *   @fn UnRegisterAllEventsVideoin
     */
    void UnRegisterAllEventsVideoin() override{};

    /**
     *   @fn StartHelper
     *   @param  videoInputType string video input type
     *   @param port port number
     */
    void StartHelperVideoin(int port, std::string videoInputType) override{}
    /**
     *   @fn StopHelper
     *   @param  videoInputType string video input type
     */
    void StopHelperVideoin(std::string videoInputType) override {}

    /**
     *   @fn RegisterEventOnVideoStreamInfoUpdateHdmiin
     */
    void RegisterEventOnVideoStreamInfoUpdateHdmiin(std::function<void(PlayerVideoStreamInfoData)> videoInfoUpdatedMethodCb) override {}

    /**
     *   @fn RegisterOnPlayerStatusOta
     *   @param onPlayerStatusCb callback for player when player status update
     */
    void RegisterOnPlayerStatusOta(std::function<void(PlayerStatusData)> onPlayerStatusCb) override {}

    /**
     *   @fn StartOta
     *   @param url string containing url
     *   @param waylandDisplay string wayland display
     *   @param preferredLanguagesString player's preferred languages
     *   @param atsc_preferredLanguagesString ATSC preferred languages
     *   @param preferredRenditionString player's preferred rendition
     *   @param atsc_preferredRenditionString ATSC preferred rendition
     */
    void StartOta(std::string url, std::string waylandDisplay, std::string preferredLanguagesString, std::string atsc_preferredLanguagesString, std::string preferredRenditionString, std::string atsc_preferredRenditionString) override {}

    /**
     *   @fn StopOta
     */
    void StopOta() override {}

    /**
     *   @fn ReleaseOta
     */
    void ReleaseOta() override {}

    /**
     * @fn GetAudioTracks
     * @param audData vector to be filled with audio data
     * @return string of current track pk
     */
    std::string GetAudioTracksOta(std::vector<PlayerAudioData> audData) override
    {
        std::string ret = "";
        return ret;
    }

    /**
     * @fn SetAudioTrackOta
     * @param index audio index to be set
     * @param primaryKey primary key
     * @return string of audioTrackIndex on success, empty string on failure
     */
    std::string SetAudioTrackOta(int index, int primaryKey) override
    {
        std::string ret = "";
        return ret;
    }

    /**
     * @fn GetTextTracksOta
     * @param txtData vector to be filled with text data
     * @return true if successful, false if failed
     */
    bool GetTextTracksOta(std::vector<PlayerTextData> txtData) override {return false;}

    /**
     * @fn DisableContentRestrictionsOta
     *
     * @param[in] grace - seconds from current time, grace period, grace = -1 will allow an unlimited grace period
     * @param[in] time - seconds from current time,time till which the channel need to be kept unlocked
     * @param[in] eventChange - disable restriction handling till next program event boundary
     */
    void DisableContentRestrictionsOta(long grace, long time, bool eventChange) override {};

    /**
     * @fn EnableContentRestrictions
     *
     */
    void EnableContentRestrictionsOta() override {}

    /**
     * @fn InitRmf
     * @return true if successful, false if failed
     */
    bool InitRmf() override {return false;}

    /**
     * @fn StartRmf
     * @param url url string
     * @param onPlayerStatusHandlerCb player status handler callback
     * @param onPlayerErrorHandlerCb player error handler callback
     * @return true if successful, false if failed
     */
    bool StartRmf(std::string url, std::function<void(std::string)> onPlayerStatusHandlerCb, std::function<void(std::string)> onPlayerErrorHandlerCb) override {return false;}

    /**
     * @fn StopRmf
     *
     */
    void StopRmf() override {}

    /**
     * @fn DeleteWatermark
     * @param layerID layer ID
     * @return true if successful, false if failed
     */
    bool DeleteWatermark(int layerID) override {return false;}

    /**
     * @fn CreateWatermark
     * @param layerID layer ID
     * @return true if successful, false if failed
     */
    bool CreateWatermark(int layerID) override {return false;}

    /**
     * @fn ShowWatermark
     * @param opacity opacity
     * @return true if successful, false if failed
     */
    bool ShowWatermark(int opacity) override {return false;}

    /**
     * @fn HideWatermark
     * @return true if successful, false if failed
     */
    bool HideWatermark() override {return false;}

    /**
     * @fn UpdateWatermark
     * @param layerID layed id
     * @param sharedMemoryKey key of shared mem
     * @param size size
     * @return true if successful, false if failed
     */
    bool UpdateWatermark(int layerID, int sharedMemoryKey, int size) override {return false;}

    /**
     * @fn GetMetaDataWatermark
     * @return string of meta data if successful, empty string if failed
     */
    std::string GetMetaDataWatermark() override
    {
        std::string metaData = "";
        return metaData;
    }

    /**
     * @fn PersistentStoreSaveWatermark
     * @param base64Image image data
     * @param metaData metaData
     * @return true if successful, false if failed
     */
    bool PersistentStoreSaveWatermark(const char* base64Image, std::string metaData) override {return false;}

    /**
     * @fn PersistentStoreLoadWatermark
     * @param layerID layer id
     * @return true if successful, false if failed
     */
    bool PersistentStoreLoadWatermark(int layerID) override {return false;}

    /**
     *   @fn IsThunderAccess
     *   @return true if thunder access available
     *   @return false f thunder access not available
     */
    bool IsThunderAccess() override {return false;}


};


PlayerThunderInterface::PlayerThunderInterface(PlayerThunderAccessPlugin callsign)
{
#ifdef USE_CPP_THUNDER_PLUGIN_ACCESS
    m_pThunderAccess = new PlayerThunderAccess(callsign);
#else
    m_pThunderAccess = new FakeThunderAccess(callsign);
#endif
}

PlayerThunderInterface::~PlayerThunderInterface()
{
    delete m_pThunderAccess;
}

bool PlayerThunderInterface::ActivatePlugin()
{
    return m_pThunderAccess->ActivatePlugin();
}

bool PlayerThunderInterface::UnSubscribeEvent(std::string eventName)
{
    return m_pThunderAccess->UnSubscribeEvent(eventName);
}

bool PlayerThunderInterface::SetVideoRectangle(int x, int y, int w, int h, std::string videoInputType, PlayerThunderAccessShim shim)
{
    return m_pThunderAccess->SetVideoRectangle(x, y, w, h, videoInputType, shim);
}


void PlayerThunderInterface::RegisterAllEventsVideoin(std::function<void(std::string)> OnSignalChangedCb, std::function<void(std::string)> OnInputStatusChangedCb)
{
    m_pThunderAccess->RegisterAllEventsVideoin(OnSignalChangedCb, OnInputStatusChangedCb);
}

void PlayerThunderInterface::UnRegisterAllEventsVideoin()
{
    m_pThunderAccess->UnRegisterAllEventsVideoin();
}

void PlayerThunderInterface::StartHelperVideoin(int port, std::string videoInputType)
{
    m_pThunderAccess->StartHelperVideoin(port, videoInputType);
}

void PlayerThunderInterface::StopHelperVideoin(std::string videoInputType)
{
    m_pThunderAccess->StopHelperVideoin(videoInputType);
}

void PlayerThunderInterface::RegisterEventOnVideoStreamInfoUpdateHdmiin(std::function<void(PlayerVideoStreamInfoData)> videoInfoUpdatedMethodCb)
{
    m_pThunderAccess->RegisterEventOnVideoStreamInfoUpdateHdmiin(videoInfoUpdatedMethodCb);
}

void PlayerThunderInterface::RegisterOnPlayerStatusOta(std::function<void(PlayerStatusData)> onPlayerStatusCb)
{
    m_pThunderAccess->RegisterOnPlayerStatusOta(onPlayerStatusCb);
}

void PlayerThunderInterface::ReleaseOta()
{
    m_pThunderAccess->ReleaseOta();
}

void PlayerThunderInterface::StartOta(std::string url, std::string waylandDisplay, std::string preferredLanguagesString, std::string atsc_preferredLanguagesString, std::string preferredRenditionString, std::string atsc_preferredRenditionString)
{
    m_pThunderAccess->StartOta(url, waylandDisplay, preferredLanguagesString, atsc_preferredLanguagesString, preferredRenditionString, atsc_preferredRenditionString);
}

void PlayerThunderInterface::StopOta()
{
    m_pThunderAccess->StopOta();
}

void PlayerThunderInterface::SetPreferredAudioLanguages(PlayerPreferredAudioData data, PlayerThunderAccessShim shim)
{
    m_pThunderAccess->SetPreferredAudioLanguages(data, shim);
}

std::string PlayerThunderInterface::GetAudioTracksOta(std::vector<PlayerAudioData> audData)
{
    return m_pThunderAccess->GetAudioTracksOta(audData);
}

std::string PlayerThunderInterface::SetAudioTrackOta(int index, int primaryKey)
{
    return m_pThunderAccess->SetAudioTrackOta(index, primaryKey);
}

bool PlayerThunderInterface::GetTextTracksOta(std::vector<PlayerTextData> txtData)
{
    return m_pThunderAccess->GetTextTracksOta(txtData);
}

void PlayerThunderInterface::DisableContentRestrictionsOta(long grace, long time, bool eventChange)
{
    return m_pThunderAccess->DisableContentRestrictionsOta(grace, time, eventChange);
}

void PlayerThunderInterface::EnableContentRestrictionsOta()
{
    return m_pThunderAccess->EnableContentRestrictionsOta();
}

bool PlayerThunderInterface::InitRmf()
{
    return m_pThunderAccess->InitRmf();
}

bool PlayerThunderInterface::StartRmf(std::string url, std::function<void(std::string)> onPlayerStatusHandlerCb, std::function<void(std::string)> onPlayerErrorHandlerCb)
{
    return m_pThunderAccess->StartRmf(url, onPlayerStatusHandlerCb, onPlayerErrorHandlerCb);
}

void PlayerThunderInterface::StopRmf()
{
    m_pThunderAccess->StopRmf();
}

bool PlayerThunderInterface::DeleteWatermark(int layerID)
{
    return m_pThunderAccess->DeleteWatermark(layerID);
}

bool PlayerThunderInterface::CreateWatermark(int layerID)
{
    return m_pThunderAccess->CreateWatermark(layerID);
}

bool PlayerThunderInterface::ShowWatermark(int opacity)
{
    return m_pThunderAccess->ShowWatermark(opacity);
}

bool PlayerThunderInterface::HideWatermark()
{
    return m_pThunderAccess->HideWatermark();
}

bool PlayerThunderInterface::UpdateWatermark(int layerID, int sharedMemoryKey, int size)
{
    return m_pThunderAccess->UpdateWatermark(layerID, sharedMemoryKey, size);
}

std::string PlayerThunderInterface::GetMetaDataWatermark()
{
    return m_pThunderAccess->GetMetaDataWatermark();
}

bool PlayerThunderInterface::PersistentStoreSaveWatermark(const char* base64Image, std::string metaData)
{
    return m_pThunderAccess->PersistentStoreSaveWatermark(base64Image, metaData);
}

bool PlayerThunderInterface::PersistentStoreLoadWatermark(int layerID)
{
    return m_pThunderAccess->PersistentStoreLoadWatermark(layerID);
}

bool PlayerThunderInterface::IsThunderAccess()
{
    return m_pThunderAccess->IsThunderAccess();
}