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
 * @brief base class for interface between player and thunder
 */

#ifndef PLAYER_THUNDER_INTERFACE_BASE
#define PLAYER_THUNDER_INTERFACE_BASE

#include <string>
#include <vector>
#include <functional>

/*
 *@enum PlayerThunderAccessPlugin : Supported Plugins
 */
enum PlayerThunderAccessPlugin
{
    AVINPUT,
    MEDIAPLAYER,
    MEDIASETTINGS,
    RDKSHELL,
    RMF,
    DS,
    SECMANAGER,
    WATERMARK,
    HDMIINPUT,
    COMPOSITEINPUT
};

/*
 *@enum PlayerThunderAccessShim : Supported Shim
 */
enum PlayerThunderAccessShim
{
    VIDEOIN_SHIM,
    OTA_SHIM,
    RMF_SHIM
};

/*
 *@struct PlayerVideoStreamInfoData video stream data
 */
struct PlayerVideoStreamInfoData
{
    bool progressive;
    double frameRateN;
    double frameRateD;
    int width;
    int height;
};

/*
 *@struct PlayerStatusData player status data
 */
struct PlayerStatusData{

    std::string currState;
    std::string eventUrl;
    std::string reasonString;
    std::string ratingString;
    int ssi;

    bool vid_progressive;
    float vid_frameRateN;
    float vid_frameRateD;
    int vid_aspectRatioWidth;
    int vid_aspectRatioHeight;
    int vid_width;
    int vid_height;
    std::string vid_codec;
    std::string vid_hdrType;
    int vid_bitrate;

    std::string aud_codec;
    std::string aud_mixType;
    bool aud_isAtmos;
    int aud_bitrate;

};

/*
 *@struct PlayerAudioData player audio data
 */
struct PlayerAudioData{

    std::string language;
    std::string contentType;
    std::string name;
    std::string type;
    int pk;
    std::string mixType;

    PlayerAudioData(std::string lang, std::string content_Type, std::string nm, std::string t, int pk_int, std::string mix_Type)
    : language(lang),
    contentType(content_Type),
    name(nm),
    type(t),
    pk(pk_int),
    mixType(mix_Type)
    {
    }

};

/*
 *@struct PlayerPreferredAudioData player preferred audio data
 */

struct PlayerPreferredAudioData{

    std::string preferredLanguagesString;
    std::string pluginPreferredLanguagesString;
    std::string preferredRenditionString;
    std::string pluginPreferredRenditionString;
};

/*
 *@struct PlayerTextData player text data
 */
struct PlayerTextData{

    std::string type;
    std::string language;
    int ccServiceNumber;
    std::string ccType;
    std::string name;
    int pk;

    PlayerTextData(std::string t, std::string lang, int ccSer, std::string cc_Type, std::string nm, int pk_int)
    :type(t),
    language(lang),
    ccServiceNumber(ccSer),
    ccType(cc_Type),
    name(nm),
    pk(pk_int)
    {
    }

};

/**
 * @class PlayerThunderAccessBase
 * @brief Template Thunder Plugin Access from Player
 */
class PlayerThunderAccessBase
{
public:
    /**
     *   @fn PlayerThunderAccessBase
     *   @note   Security token acquisition, controller object creation
     *   @param   callsign plugin callsign
     */
    PlayerThunderAccessBase(PlayerThunderAccessPlugin callsign) {}

    /**
     *   @fn ~PlayerThunderAccessBase
     *   @note   clean up
     */
    virtual ~PlayerThunderAccessBase(){}

    /**
     *   @brief  PlayerThunderAccessBase copy constructor disabled
     */
    PlayerThunderAccessBase(const PlayerThunderAccessBase&) = delete;

    /**
     *   @brief  PlayerThunderAccessBase assignment disabled
     */
    PlayerThunderAccessBase& operator=(const PlayerThunderAccessBase&) = delete;

    /**
     *   @brief  ActivatePlugin
     *   @note   Plugin activation and Remote object creation
     *   @param  Plugin Callsign
     *   @retval true on success
     *   @retval false on failure
     */
    virtual bool ActivatePlugin() = 0;

    /**
     *   @fn UnSubscribeEvent
     *   @note   unSubscribe event data for the specific plugin
     *   @param  eventName Event name
     *   @retval true on success
     *   @retval false on failure
     */
    virtual bool UnSubscribeEvent (std::string eventName) = 0;

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
    virtual bool SetVideoRectangle(int x, int y, int w, int h, std::string videoInputType, PlayerThunderAccessShim shim) = 0;

    /**
     *   @fn SetPreferredAudioLanguages_OTA
     *   @param data player's input on preferred languages
     *   @param shim shim to set preferred language for
     */
    virtual void SetPreferredAudioLanguages(PlayerPreferredAudioData data, PlayerThunderAccessShim shim) = 0;

    /**
     *   @fn RegisterAllEventsVideoin
     *   @param OnSignalChangedCb callback for when signal change
     *   @param OnInputStatusChangedCb callback for when input status changes
     */
    virtual void RegisterAllEventsVideoin(std::function<void(std::string)> OnSignalChangedCb, std::function<void(std::string)> OnInputStatusChangedCb) = 0;

    /**
     *   @fn UnRegisterAllEventsVideoin
     */
    virtual void UnRegisterAllEventsVideoin() = 0;

    /**
     *   @fn StartHelper
     *   @param port port number
     *   @param  videoInputType string video input type
     */
    virtual void StartHelperVideoin(int port, std::string videoInputType) = 0;
    /**
     *   @fn StopHelper
     *   @param  videoInputType string video input type
     */
    virtual void StopHelperVideoin(std::string videoInputType) = 0;

    /**
     *   @fn RegisterEventOnVideoStreamInfoUpdateHdmiin
     */
    virtual void RegisterEventOnVideoStreamInfoUpdateHdmiin(std::function<void(PlayerVideoStreamInfoData)> videoInfoUpdatedMethodCb) = 0;

    /**
     *   @fn RegisterOnPlayerStatusOta
     *   @param onPlayerStatusCb callback for player when player status update
     */
    virtual void RegisterOnPlayerStatusOta(std::function<void(PlayerStatusData)> onPlayerStatusCb) = 0;

    /**
     *   @fn ReleaseOta
     */
    virtual void ReleaseOta() = 0;

    /**
     *   @fn StartOta
     *   @param url string containing url
     *   @param waylandDisplay string wayland display
     *   @param preferredLanguagesString player's preferred languages
     *   @param atsc_preferredLanguagesString ATSC preferred languages
     *   @param preferredRenditionString player's preferred rendition
     *   @param atsc_preferredRenditionString ATSC preferred rendition
     */
    virtual void StartOta(std::string url, std::string waylandDisplay, std::string preferredLanguagesString, std::string atsc_preferredLanguagesString, std::string preferredRenditionString, std::string atsc_preferredRenditionString) = 0;

    /**
     *   @fn StopOta
     */
    virtual void StopOta() = 0;

    /**
     * @fn GetAudioTracks
     * @param audData vector to be filled with audio data
     * @return string of current track pk
     */
    virtual std::string GetAudioTracksOta(std::vector<PlayerAudioData> audData) = 0;

    /**
     * @fn SetAudioTrackOta
     * @param index audio index to be set
     * @param primaryKey primary key
     * @return string of audioTrackIndex on success, empty string on failure
     */
    virtual std::string SetAudioTrackOta(int index, int primaryKey) = 0;

    /**
     * @fn GetTextTracksOta
     * @param txtData vector to be filled with text data
     * @return true if successful, false if failed
     */
    virtual bool GetTextTracksOta(std::vector<PlayerTextData> txtData) = 0;

    /**
     * @fn DisableContentRestrictionsOta
     *
     * @param[in] grace - seconds from current time, grace period, grace = -1 will allow an unlimited grace period
     * @param[in] time - seconds from current time,time till which the channel need to be kept unlocked
     * @param[in] eventChange - disable restriction handling till next program event boundary
     */
    virtual void DisableContentRestrictionsOta(long grace, long time, bool eventChange) = 0;

    /**
     * @fn EnableContentRestrictions
     *
     */
    virtual void EnableContentRestrictionsOta() = 0;

    /**
     * @fn InitRmf
     * @return true if successful, false if failed
     */
    virtual bool InitRmf() = 0;

    /**
     * @fn StartRmf
     * @param url url string
     * @param onPlayerStatusHandlerCb player status handler callback
     * @param onPlayerErrorHandlerCb player error handler callback
     * @return true if successful, false if failed
     */
    virtual bool StartRmf(std::string url, std::function<void(std::string)> onPlayerStatusHandlerCb, std::function<void(std::string)> onPlayerErrorHandlerCb) = 0;

    /**
     * @fn StopRmf
     *
     */
    virtual void StopRmf() = 0;

    /**
     * @fn DeleteWatermark
     * @param layerID layer ID
     * @return true if successful, false if failed
     */
    virtual bool DeleteWatermark(int layerID) = 0;

    /**
     * @fn CreateWatermark
     * @param layerID layer ID
     * @return true if successful, false if failed
     */
    virtual bool CreateWatermark(int layerID) = 0;

    /**
     * @fn ShowWatermark
     * @param opacity opacity
     * @return true if successful, false if failed
     */
    virtual bool ShowWatermark(int opacity) = 0;

    /**
     * @fn HideWatermark
     * @return true if successful, false if failed
     */
    virtual bool HideWatermark() = 0;

    /**
     * @fn UpdateWatermark
     * @param layerID layed id
     * @param sharedMemoryKey key of shared mem
     * @param size size
     * @return true if successful, false if failed
     */
    virtual bool UpdateWatermark(int layerID, int sharedMemoryKey, int size) = 0;

    /**
     * @fn GetMetaDataWatermark
     * @return string of meta data if successful, empty string if failed
     */
    virtual std::string GetMetaDataWatermark() = 0;

    /**
     * @fn PersistentStoreSaveWatermark
     * @param base64Image image data
     * @param metaData metaData
     * @return true if successful, false if failed
     */
    virtual bool PersistentStoreSaveWatermark(const char* base64Image, std::string metaData) = 0;

    /**
     * @fn PersistentStoreLoadWatermark
     * @param layerID layer id
     * @return true if successful, false if failed
     */
    virtual bool PersistentStoreLoadWatermark(int layerID) = 0;

    /**
     *   @fn IsThunderAccess
     *   @return true if thunder access available
     *   @return false f thunder access not available
     */
    virtual bool IsThunderAccess() = 0;


protected:
    std::string pluginCallsign;
};

#endif