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
 * @file PlayerThunderAccess.h
 * @brief shim for dispatching UVE HDMI input playback
 */

#ifndef PLAYER_THUNDERACCESS_H_
#define PLAYER_THUNDERACCESS_H_

#include "Module.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Weffc++"
#include <core/core.h>
#include <websocket/websocket.h>
#pragma GCC diagnostic pop

#include "PlayerThunderInterface.h"

using namespace std;
using namespace WPEFramework;

#define THUNDER_RPC_TIMEOUT 5000


/**
 * @class PlayerThunderAccess
 * @brief Support Thunder Plugin Access from Player
 */
class PlayerThunderAccess : public PlayerThunderAccessBase
{
public:
    /**
     *   @fn PlayerThunderAccess
     *   @note   Security token acquisition, controller object creation
     *   @param   callsign plugin callsign
     */
    PlayerThunderAccess(PlayerThunderAccessPlugin callsign);

    /**
     *   @fn ~PlayerThunderAccess
     *   @note   clean up
     */
    ~PlayerThunderAccess();

    /**
     *   @brief  PlayerThunderAccess copy constructor disabled
     */
    PlayerThunderAccess(const PlayerThunderAccess&) = delete;

    /**
     *   @brief  PlayerThunderAccess assignment disabled
     */
    PlayerThunderAccess& operator=(const PlayerThunderAccess&) = delete;

    /**
     *   @brief  ActivatePlugin
     *   @note   Plugin activation and Remote object creation
     *   @param  Plugin Callsign
     *   @retval true on success
     *   @retval false on failure
     */
    bool ActivatePlugin() override;

    /**
     *   @fn UnSubscribeEvent
     *   @note   unSubscribe event data for the specific plugin
     *   @param  eventName Event name
     *   @retval true on success
     *   @retval false on failure
     */
    bool UnSubscribeEvent (string eventName)override;

    /**
     *   @fn InvokeJSONRPC
     *   @note   Invoke JSONRPC call for the plugin
     *   @param  method,param,result,  method,waitTime reference to input param, result and waitTime (default = THUNDER_RPC_TIMEOUT)
     *   @retval true on success
     *   @retval false on failure
     */
    bool InvokeJSONRPC(std::string method, const JsonObject &param, JsonObject &result, const uint32_t waitTime = THUNDER_RPC_TIMEOUT);

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
    bool SetVideoRectangle(int x, int y, int w, int h, std::string videoInputType, PlayerThunderAccessShim shim) override;

    /**
     *   @fn SetPreferredAudioLanguages_OTA
     *   @param data player's input on preferred languages
     *   @param shim shim to set preferred language for
     */
    void SetPreferredAudioLanguages(PlayerPreferredAudioData data, PlayerThunderAccessShim shim) override;

    /**
     *   @fn RegisterAllEventsVideoin
     *   @param OnSignalChangedCb callback for when signal change
     *   @param OnInputStatusChangedCb callback for when input status changes
     */
    void RegisterAllEventsVideoin(std::function<void(std::string)> OnSignalChangedCb, std::function<void(std::string)> OnInputStatusChangedCb) override;

    /**
     *   @fn UnRegisterAllEventsVideoin
     */
    void UnRegisterAllEventsVideoin() override;

    /**
     *   @fn StartHelper
     *   @param port port number
     *   @param videoInputType string video input type
     */
    void StartHelperVideoin(int port, std::string videoInputType) override;
    /**
     *   @fn StopHelper
     *   @param  videoInputType string video input type
     */
    void StopHelperVideoin(std::string videoInputType) override;

    /**
     *   @fn RegisterEventOnVideoStreamInfoUpdateHdmiin
     *   @param videoInfoUpdatedMethodCb callback for when video info updated
     */
    void RegisterEventOnVideoStreamInfoUpdateHdmiin(std::function<void(PlayerVideoStreamInfoData)> videoInfoUpdatedMethodCb) override;

    /**
     *   @fn RegisterOnPlayerStatusOta
     *   @param onPlayerStatusCb callback for player when player status update
     */
    void RegisterOnPlayerStatusOta(std::function<void(PlayerStatusData)> onPlayerStatusCb) override;

    /**
     *   @fn ReleaseOta
     */
    void ReleaseOta() override;

    /**
     *   @fn StartOta
     *   @param url string containing url
     *   @param waylandDisplay string wayland display
     *   @param preferredLanguagesString player's preferred languages
     *   @param atsc_preferredLanguagesString ATSC preferred languages
     *   @param preferredRenditionString player's preferred rendition
     *   @param atsc_preferredRenditionString ATSC preferred rendition
     */
    void StartOta(std::string url, std::string waylandDisplay, std::string preferredLanguagesString, std::string atsc_preferredLanguagesString, std::string preferredRenditionString, std::string atsc_preferredRenditionString) override;

    /**
     *   @fn StopOta
     */
    void StopOta() override;

    /**
     * @fn GetAudioTracksOta
     * @param audData vector to be filled with audio data
     * @return string of current track pk
     */
    std::string GetAudioTracksOta(std::vector<PlayerAudioData> audData) override;

    /**
     * @fn SetAudioTrackOta
     * @param index audio index to be set
     * @param primaryKey primary key
     * @return string of audioTrackIndex on success, empty string on failure
     */
    std::string SetAudioTrackOta(int index, int primaryKey) override;

    /**
     * @fn GetTextTracksOta
     * @param txtData vector to be filled with text data
     * @return true if successful, false if failed
     */
    bool GetTextTracksOta(std::vector<PlayerTextData> txtData) override;

    /**
     * @fn DisableContentRestrictionsOta
     *
     * @param[in] grace - seconds from current time, grace period, grace = -1 will allow an unlimited grace period
     * @param[in] time - seconds from current time,time till which the channel need to be kept unlocked
     * @param[in] eventChange - disable restriction handling till next program event boundary
     */
    void DisableContentRestrictionsOta(long grace, long time, bool eventChange) override;

    /**
     * @fn EnableContentRestrictions
     *
     */
    void EnableContentRestrictionsOta() override;

    /**
     * @fn InitRmf
     * @return true if successful, false if failed
     */
    bool InitRmf() override;

    /**
     * @fn StartRmf
     * @param url url string
     * @param onPlayerStatusHandlerCb player status handler callback
     * @param onPlayerErrorHandlerCb player error handler callback
     * @return true if successful, false if failed
     */
    bool StartRmf(std::string url, std::function<void(std::string)> onPlayerStatusHandlerCb, std::function<void(std::string)> onPlayerErrorHandlerCb) override;

    /**
     * @fn StopRmf
     *
     */
    void StopRmf() override;

    /**
     * @fn DeleteWatermark
     * @param layerID layer ID
     * @return true if successful, false if failed
     */
    bool DeleteWatermark(int layerID) override;

    /**
     * @fn CreateWatermark
     * @param layerID layer ID
     * @return true if successful, false if failed
     */
    bool CreateWatermark(int layerID) override;

    /**
     * @fn ShowWatermark
     * @param opacity opacity
     * @return true if successful, false if failed
     */
    bool ShowWatermark(int opacity) override;

    /**
     * @fn HideWatermark
     * @return true if successful, false if failed
     */
    bool HideWatermark() override;

    /**
     * @fn UpdateWatermark
     * @param layerID layed id
     * @param sharedMemoryKey key of shared mem
     * @param size size
     * @return true if successful, false if failed
     */
    bool UpdateWatermark(int layerID, int sharedMemoryKey, int size) override;

    /**
     * @fn GetMetaDataWatermark
     * @return string of meta data if successful, empty string if failed
     */
    std::string GetMetaDataWatermark() override;

    /**
     * @fn PersistentStoreSaveWatermark
     * @param base64Image image data
     * @param metaData metaData
     * @return true if successful, false if failed
     */
    bool PersistentStoreSaveWatermark(const char* base64Image, std::string metaData) override;

    /**
     * @fn PersistentStoreLoadWatermark
     * @param layerID layer id
     * @return true if successful, false if failed
     */
    bool PersistentStoreLoadWatermark(int layerID) override;

    /**
     *   @fn IsThunderAccess
     *   @return true if thunder access available
     *   @return false f thunder access not available
     */
    bool IsThunderAccess() override {return true;}


protected:

    /**< The Remote object connected to specific Plugin*/
    JSONRPC::LinkType<Core::JSON::IElement> *remoteObject;
    /**< The Remote object connected to controller Plugin*/
    JSONRPC::LinkType<Core::JSON::IElement> *controllerObject;

private:

    std::list<std::string> mRegisteredEvents;

    bool mEventSubscribed;

    int videoInputPort;

    std::function<void(std::string)> mOnSignalChangedCb;    //callback to player on signal change
    std::function<void(std::string)> mOnInputStatusChangedCb;   //callback to player on input status change
    std::function<void(PlayerVideoStreamInfoData)> mVideoInfoUpdatedMethodCb;  //callback to player on video info update
    std::function<void(PlayerStatusData)> mOnPlayerStatusCb;    //callback to player on player status update
    std::function<void(std::string)> mOnPlayerStatusHandlerCb;    //callback to player on status change, rmf
    std::function<void(std::string)> mOnPlayerErrorHandlerCb;   //callback to player on error, rmf

    /**
     *   @fn RegisterEvent
     *   @param[in] eventName : Event name
     *   @param[in] functionHandler : Event function pointer
     *   @return NA
     */
    void RegisterEvent_VIDEOIN(string eventName, std::function<void(const WPEFramework::Core::JSON::VariantContainer&)> functionHandler);

    /**
     *   @fn GetScreenResolution
     *   @param[out] screenWidth : width
     *   @param[in] screenHeight : height
     *   @return true if success, false if failure
     */
    bool GetScreenResolution(int & screenWidth, int & screenHeight);

    /**
     *   @fn SetVideoRectangle
     *   @note   Set video rectangle dimensions
     *   @param  x x offset
     *   @param  y y offset
     *   @param  w width
     *   @param  h height
     *   @param  videoInputType string video input type
     *   @retval true if success
     *   @retval false if failure
     */
    bool SetVideoRectangle_VIDEOIN(int x, int y, int w, int h, std::string videoInputType);

    /**
     *   @fn SetVideoRectangle_OTA
     *   @note   Set video rectangle dimensions
     *   @param  x x offset
     *   @param  y y offset
     *   @param  w width
     *   @param  h height
     *   @retval true if success
     *   @retval false if failure
     */
    bool SetVideoRectangle_OTA(int x, int y, int w, int h);

    /**
     *   @fn SetVideoRectangle_RMF
     *   @note   Set video rectangle dimensions
     *   @param  x x offset
     *   @param  y y offset
     *   @param  w width
     *   @param  h height
     *   @retval true if success
     *   @retval false if failure
     */
    bool SetVideoRectangle_RMF(int x, int y, int w, int h);

    /**
     *   @fn SetPreferredAudioLanguages_OTA
     *   @param preferredLanguagesString string containing preferred language
     *   @param atsc_preferredLanguagesString string containing ATSC preferred language
     *   @param preferredRenditionString string containing preferred rendition
     *   @param atsc_preferredRenditionString string containing ATSC preferred rendition
     */
    void SetPreferredAudioLanguages_OTA(std::string preferredLanguagesString, std::string atsc_preferredLanguagesString, std::string preferredRenditionString, std::string atsc_preferredRenditionString);

    /**
     *   @fn SetPreferredAudioLanguages_RMF
     *   @param preferredLanguagesString string containing preferred language to be set
     *   @param rmf_preferredLanguages string RMF preferred language
     */
    void SetPreferredAudioLanguages_RMF(std::string preferredLanguagesString, std::string rmf_preferredLanguages);

    /**
     *   @fn GetResolutionFromDS_VIDEOIN
     *   @param[out] widthFromDS : width
     *   @param[in] heightFromDS : height
     *   @return true if success, false if failure
     */
    bool GetResolutionFromDS_VIDEOIN(int & widthFromDS, int & heightFromDS);

    /**
     *   @fn SubscribeEvent
     *   @note   Subscribe event data for the specific plugin
     *   @param  eventName,functionHandler Event name, Event handler
     *   @retval true on success
     *   @retval false on failure
     */
    bool SubscribeEvent (string eventName, std::function<void(const WPEFramework::Core::JSON::VariantContainer&)> functionHandler);

    /**
     * @fn GetAudioTrackInternal
     *
     */
    int GetAudioTrackInternal_OTA();

    /**
     * @fn OnInputStatusChanged callback
     *
     */
    void OnInputStatusChanged(const JsonObject& parameters);

    /**
     * @fn OnSignalChanged callback
     *
     */
    void OnSignalChanged (const JsonObject& parameters);

    /**
     * @fn OnVideoStreamInfoUpdate callback
     *
     */
    void OnVideoStreamInfoUpdate(const JsonObject& parameters);

    /**
     * @fn onPlayerStatusHandler_OTA callback
     *
     */
    void onPlayerStatusHandler_OTA(const JsonObject& parameters);

    /**
     * @fn onPlayerStatusHandler_RMF callback
     *
     */
    void onPlayerStatusHandler_RMF(const JsonObject& parameters);

    /**
     * @fn onPlayerErrorHandler_RMF callback
     *
     */
    void onPlayerErrorHandler_RMF(const JsonObject& parameters);

};
#endif // PLAYER_THUNDERACCESS_H_
