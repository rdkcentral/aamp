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
 * @file PlayerThunderInterface.h
 * @brief interface between player and thunder
 */

#ifndef PLAYER_THUNDER_INTERFACE_H
#define PLAYER_THUNDER_INTERFACE_H

#include "PlayerThunderAccessBase.h"

/**
 * @class PlayerThunderInterface
 * @brief Interface Thunder for Player
 */
class PlayerThunderInterface
{
public:
    /**
     *   @fn PlayerThunderInterface
     *   @note   Security token acquisition, controller object creation
     *   @param   callsign plugin callsign
     */
    PlayerThunderInterface(PlayerThunderAccessPlugin callsign);

    /**
     *   @fn ~PlayerThunderInterface
     *   @note   clean up
     */
    ~PlayerThunderInterface();

    /**
     *   @brief  PlayerThunderInterface copy constructor disabled
     */
    PlayerThunderInterface(const PlayerThunderInterface&) = delete;

    /**
     *   @brief  PlayerThunderInterface assignment disabled
     */
    PlayerThunderInterface& operator=(const PlayerThunderInterface&) = delete;

    /**
     *   @brief  ActivatePlugin
     *   @note   Plugin activation and Remote object creation
     *   @param  Plugin Callsign
     *   @retval true on success
     *   @retval false on failure
     */
    bool ActivatePlugin();

    /**
     *   @fn UnSubscribeEvent
     *   @note   unSubscribe event data for the specific plugin
     *   @param  eventName Event name
     *   @retval true on success
     *   @retval false on failure
     */
    bool UnSubscribeEvent (std::string eventName);

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
    bool SetVideoRectangle(int x, int y, int w, int h, std::string videoInputType, PlayerThunderAccessShim shim);

    /**
     *   @fn SetPreferredAudioLanguages_OTA
     *   @param data player's input on preferred languages
     *   @param shim shim to set preferred language for
     */
    void SetPreferredAudioLanguages(PlayerPreferredAudioData data, PlayerThunderAccessShim shim);

    //-----------------VIDEOIN APIs begin-------------------

    /**
     *   @fn RegisterAllEventsVideoin
     *   @param OnSignalChangedCb callback for when signal change
     *   @param OnInputStatusChangedCb callback for when input status changes
     */
    void RegisterAllEventsVideoin(std::function<void(std::string)> OnSignalChangedCb, std::function<void(std::string)> OnInputStatusChangedCb);

    /**
     *   @fn UnRegisterAllEventsVideoin
     */
    void UnRegisterAllEventsVideoin();

    /**
     *   @fn StartHelper
     *   @param port port number
     *   @param  videoInputType string video input type
     */
    void StartHelperVideoin(int port, std::string videoInputType);

    /**
     *   @fn StopHelper
     *   @param  videoInputType string video input type
     */
    void StopHelperVideoin(std::string videoInputType);

    //-----------------VIDEOIN APIs end-----------------

    //-----------------HDMIIN APIs begin----------------

    /**
     *   @fn RegisterEventOnVideoStreamInfoUpdateHdmiin
     */
    void RegisterEventOnVideoStreamInfoUpdateHdmiin(std::function<void(PlayerVideoStreamInfoData)> videoInfoUpdatedMethodCb);

    //-----------------HDMIIN APIs end------------------

    //-----------------OTA APIs begin-------------------

    /**
     *   @fn RegisterOnPlayerStatusOta
     *   @param onPlayerStatusCb callback for player when player status update
     */
    void RegisterOnPlayerStatusOta(std::function<void(PlayerStatusData)> onPlayerStatusCb);

    /**
     *   @fn ReleaseOta
     */
    void ReleaseOta();

    /**
     *   @fn StartOta
     *   @param url string containing url
     *   @param waylandDisplay string wayland display
     *   @param preferredLanguagesString player's preferred languages
     *   @param atsc_preferredLanguagesString ATSC preferred languages
     *   @param preferredRenditionString player's preferred rendition
     *   @param atsc_preferredRenditionString ATSC preferred rendition
     */
    void StartOta(std::string url, std::string waylandDisplay, std::string preferredLanguagesString, std::string atsc_preferredLanguagesString, std::string preferredRenditionString, std::string atsc_preferredRenditionString);

    /**
     *   @fn StopOta
     */
    void StopOta();

    /**
     * @fn GetAudioTracks
     * @param audData vector to be filled with audio data
     * @return string of current track pk
     */
    std::string GetAudioTracksOta(std::vector<PlayerAudioData> audData);

    /**
     * @fn SetAudioTrackOta
     * @param index audio index to be set
     * @param primaryKey primary key
     * @return string of audioTrackIndex on success, empty string on failure
     */
    std::string SetAudioTrackOta(int index, int primaryKey);

    /**
     * @fn GetTextTracksOta
     * @param txtData vector to be filled with text data
     * @return true if successful, false if failed
     */
    bool GetTextTracksOta(std::vector<PlayerTextData> txtData);

    /**
     * @fn DisableContentRestrictionsOta
     *
     * @param[in] grace - seconds from current time, grace period, grace = -1 will allow an unlimited grace period
     * @param[in] time - seconds from current time,time till which the channel need to be kept unlocked
     * @param[in] eventChange - disable restriction handling till next program event boundary
     */
    void DisableContentRestrictionsOta(long grace, long time, bool eventChange);

    /**
     * @fn EnableContentRestrictions
     *
     */
    void EnableContentRestrictionsOta();

    //-----------------OTA APIs end-------------------

    //-----------------RMF APIs begin-----------------

    /**
     * @fn InitRmf
     * @return true if successful, false if failed
     */
    bool InitRmf();

    /**
     * @fn StartRmf
     * @param url url string
     * @param onPlayerStatusHandlerCb player status handler callback
     * @param onPlayerErrorHandlerCb player error handler callback
     * @return true if successful, false if failed
     */
    bool StartRmf(std::string url, std::function<void(std::string)> onPlayerStatusHandlerCb, std::function<void(std::string)> onPlayerErrorHandlerCb);

    /**
     * @fn StopRmf
     *
     */
    void StopRmf();

    //-----------------RMF APIs end-------------------

    //--------------Watermark APIs begins-------------

    /**
     * @fn DeleteWatermark
     * @param layerID layer ID
     * @return true if successful, false if failed
     */
    bool DeleteWatermark(int layerID);

    /**
     * @fn CreateWatermark
     * @param layerID layer ID
     * @return true if successful, false if failed
     */
    bool CreateWatermark(int layerID);

    /**
     * @fn ShowWatermark
     * @param opacity opacity
     * @return true if successful, false if failed
     */
    bool ShowWatermark(int opacity);

    /**
     * @fn HideWatermark
     * @return true if successful, false if failed
     */
    bool HideWatermark();

    /**
     * @fn UpdateWatermark
     * @param layerID layed id
     * @param sharedMemoryKey key of shared mem
     * @param size size
     * @return true if successful, false if failed
     */
    bool UpdateWatermark(int layerID, int sharedMemoryKey, int size);

    /**
     * @fn GetMetaDataWatermark
     * @return string of meta data if successful, empty string if failed
     */
    std::string GetMetaDataWatermark();

    /**
     * @fn PersistentStoreSaveWatermark
     * @param base64Image image data
     * @param metaData metaData
     * @return true if successful, false if failed
     */
    bool PersistentStoreSaveWatermark(const char* base64Image, std::string metaData);

    /**
     * @fn PersistentStoreLoadWatermark
     * @param layerID layer id
     * @return true if successful, false if failed
     */
    bool PersistentStoreLoadWatermark(int layerID);

    //--------------Watermark APIs end----------------

    /**
     *   @fn IsThunderAccess
     *   @return true if thunder access available
     *   @return false f thunder access not available
     */
    bool IsThunderAccess();

private:

    PlayerThunderAccessBase* m_pThunderAccess = nullptr;

};



#endif