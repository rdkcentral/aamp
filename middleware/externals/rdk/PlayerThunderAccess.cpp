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
 * @file ThunderAccess.cpp
 * @brief wrapper class for accessing thunder plugins
 */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Weffc++"
#include <securityagent/SecurityTokenUtil.h>
#pragma GCC diagnostic pop
#include "PlayerThunderAccess.h"

#include "PlayerLogManager.h"


using namespace std;
using namespace WPEFramework;

#define SERVER_DETAILS  "127.0.0.1:9998"
#define MAX_LENGTH 1024

#define APP_ID "MainPlayer"

//PLUGIN CALL_SIGNS

#define AVINPUT_CALLSIGN "org.rdk.AVInput.1"
#define MEDIAPLAYER_CALLSIGN "org.rdk.MediaPlayer.1"
#define MEDIASETTINGS_CALLSIGN "org.rdk.MediaSettings.1"
#define RDKSHELL_CALLSIGN "org.rdk.RDKShell.1"
#define RMF_PLUGIN_CALLSIGN "org.rdk.MediaEngineRMF.1"
#define DS_CALLSIGN "org.rdk.DisplaySettings.1"
#define SECMANAGER_CALL_SIGN "org.rdk.SecManager.1"
#define WATERMARK_PLUGIN_CALLSIGN "org.rdk.Watermark.1"
#define HDMIINPUT_CALLSIGN "org.rdk.HdmiInput.1"
#define COMPOSITEINPUT_CALLSIGN "org.rdk.CompositeInput.1"

/**
 * @brief Structure to save the Thunder security token details
 **/
typedef struct PlayerThunderSecurity
{
    std::string securityToken;
    int tokenStatus;
    bool tokenQueried;
    PlayerThunderSecurity(): securityToken(), tokenStatus(0), tokenQueried(false) { };
}PlayerThunderSecurityData;

PlayerThunderSecurityData gPlayerSecurityData;


/**
 * @brief  PlayerThunderAccess constructor
 */
PlayerThunderAccess::PlayerThunderAccess(PlayerThunderAccessPlugin callsign)
                 : remoteObject(NULL),
                   controllerObject(NULL),
                   videoInputPort(-1),
                   PlayerThunderAccessBase(callsign)
{
    // LOG_INFO( "[ThunderAccess]Inside");

    switch(callsign)
    {
        case PlayerThunderAccessPlugin::AVINPUT :
            pluginCallsign = AVINPUT_CALLSIGN;
            break;
        case PlayerThunderAccessPlugin::MEDIAPLAYER :
            pluginCallsign = MEDIAPLAYER_CALLSIGN;
            break;
        case PlayerThunderAccessPlugin::MEDIASETTINGS :
            pluginCallsign = MEDIASETTINGS_CALLSIGN;
            break;
        case PlayerThunderAccessPlugin::RDKSHELL :
            pluginCallsign = RDKSHELL_CALLSIGN;
            break;
        case PlayerThunderAccessPlugin::RMF :
            pluginCallsign = RMF_PLUGIN_CALLSIGN;
            break;
        case PlayerThunderAccessPlugin::DS :
            pluginCallsign = DS_CALLSIGN;
            break;
        case PlayerThunderAccessPlugin::SECMANAGER :
            pluginCallsign = SECMANAGER_CALL_SIGN;
            break;
        case PlayerThunderAccessPlugin::WATERMARK :
            pluginCallsign = WATERMARK_PLUGIN_CALLSIGN;
            break;
        case PlayerThunderAccessPlugin::HDMIINPUT :
            pluginCallsign = HDMIINPUT_CALLSIGN;
            break;
        case PlayerThunderAccessPlugin::COMPOSITEINPUT :
            pluginCallsign = COMPOSITEINPUT_CALLSIGN;
            break;
        default:
            MW_LOG_ERR("Undefined plugin tried to initialize: %d", (int)callsign);
            pluginCallsign = "";
            break;
    }

    uint32_t status = Core::ERROR_NONE;

    Core::SystemInfo::SetEnvironment(_T("THUNDER_ACCESS"), (_T(SERVER_DETAILS)));

    if(!gPlayerSecurityData.tokenQueried)
    {
        unsigned char buffer[MAX_LENGTH] = {0};
        gPlayerSecurityData.tokenStatus = GetSecurityToken(MAX_LENGTH,buffer);
        if(gPlayerSecurityData.tokenStatus > 0){
            // LOG_INFO( "[ThunderAccess] : GetSecurityToken success");
            string sToken = (char*)buffer;
            gPlayerSecurityData.securityToken = "token=" + sToken;
        }
        gPlayerSecurityData.tokenQueried = true;
    }

    if (NULL == controllerObject) {
        /*Passing empty string instead of Controller callsign.This is assumed as controller plugin.*/
        if(gPlayerSecurityData.tokenStatus > 0){
            controllerObject = new JSONRPC::LinkType<Core::JSON::IElement>(_T(""), _T(""), false, gPlayerSecurityData.securityToken);
        }
        else{
            controllerObject = new JSONRPC::LinkType<Core::JSON::IElement>(_T(""));
        }

        if (NULL == controllerObject) {
            MW_LOG_WARN( "[ThunderAccess] Controller object creation failed");
        } else {
            MW_LOG_INFO( "[ThunderAccess] Controller object creation success");
        }
    }

    if(gPlayerSecurityData.tokenStatus > 0){
        remoteObject = new JSONRPC::LinkType<Core::JSON::IElement>(_T(pluginCallsign), _T(""), false, gPlayerSecurityData.securityToken);
    }
    else{
        remoteObject = new JSONRPC::LinkType<Core::JSON::IElement>(_T(pluginCallsign), _T(""));
    }
    if (NULL == remoteObject) {
        MW_LOG_WARN( "[ThunderAccess] %s Client initialization failed", pluginCallsign.c_str());
    } else {
        MW_LOG_INFO( "[ThunderAccess] %s Client initialization success", pluginCallsign.c_str());
    }
}

/**
 * @brief  PlayerThunderAccess destructor
 */
PlayerThunderAccess::~PlayerThunderAccess()
{
    delete controllerObject;
    controllerObject = NULL;
    delete remoteObject;
    remoteObject = NULL;
}

/**
 * @brief  ActivatePlugin
 */
bool PlayerThunderAccess::ActivatePlugin()
{
    bool ret = true;
    JsonObject result;
    JsonObject controlParam;
    std::string response;
    uint32_t status = Core::ERROR_NONE;

    if (NULL != controllerObject) {
        controlParam["callsign"] = pluginCallsign;
        status = controllerObject->Invoke<JsonObject, JsonObject>(THUNDER_RPC_TIMEOUT, _T("activate"), controlParam, result);
        if (Core::ERROR_NONE == status){
            result.ToString(response);
            MW_LOG_INFO( "[ThunderAccess] %s plugin Activated. Response : %s ", pluginCallsign.c_str(), response.c_str());
        }
        else
        {
            MW_LOG_WARN( "[ThunderAccess] %s plugin Activation failed with error status : %u ", pluginCallsign.c_str(), status);
            ret = false;
        }
    } else {
        MW_LOG_WARN( "[ThunderAccess] Controller Object NULL ");
        ret = false;
    }

    return ret;
}


/*To Do: Only JSON Object can be used as parameter now*/
/**
 * @brief  subscribeEvent
 */
bool PlayerThunderAccess::SubscribeEvent (string eventName, std::function<void(const WPEFramework::Core::JSON::VariantContainer&)> functionHandler)
{
    bool ret = true;
    uint32_t status = Core::ERROR_NONE;
    if (NULL != remoteObject) {
        status = remoteObject->Subscribe<JsonObject>(THUNDER_RPC_TIMEOUT, _T(eventName), functionHandler);
        if (Core::ERROR_NONE == status) {
            MW_LOG_INFO( "[ThunderAccess] Subscribed to : %s", eventName.c_str());
        } else {
            MW_LOG_WARN( "[ThunderAccess] Subscription failed for : %s with error status %u", eventName.c_str(), status);
            ret = false;
        }
    } else {
        MW_LOG_WARN( "[ThunderAccess] remoteObject not created for the plugin!");
        ret = false;
    }
    return ret;
}


/*To Do: Only JSON Object can be used as parameter now*/

/**
 * @brief  unSubscribeEvent
 */
bool PlayerThunderAccess::UnSubscribeEvent (string eventName)
{
    bool ret = true;
    if (NULL != remoteObject) {
        remoteObject->Unsubscribe(THUNDER_RPC_TIMEOUT, _T(eventName));
        MW_LOG_INFO( "[ThunderAccess] UnSubscribed : %s event", eventName.c_str());
    } else {
        MW_LOG_WARN( "[ThunderAccess] remoteObject not created for the plugin!");
        ret = false;
    }
    return ret;
}


/**
 *  @brief  invokeJSONRPC
 *  @note   Invoke JSONRPC call for the plugin
 */
bool PlayerThunderAccess::InvokeJSONRPC(std::string method, const JsonObject &param, JsonObject &result, const uint32_t waitTime)
{
    bool ret = true;
    std::string response;
    uint32_t status = Core::ERROR_NONE;

    if(NULL == remoteObject)
    {
        MW_LOG_WARN( "[ThunderAccess] client not initialized! ");
        return false;
    }

    JsonObject result_internal;
    status = remoteObject->Invoke<JsonObject, JsonObject>(waitTime, _T(method), param, result_internal);
    if (Core::ERROR_NONE == status)
    {
        if (result_internal["success"].Boolean()) {
            result_internal.ToString(response);
            MW_LOG_TRACE( "[ThunderAccess] %s success! Response : %s", method.c_str() , response.c_str());
        } else {
            result_internal.ToString(response);
            MW_LOG_WARN( "[ThunderAccess] %s call failed! Response : %s", method.c_str() , response.c_str());
            ret = false;
        }
    } else {
        MW_LOG_WARN( "[ThunderAccess] %s : invoke failed with error status %u", method.c_str(), status);
        ret = false;
    }

    result = result_internal;
    return ret;
}

bool PlayerThunderAccess::SetVideoRectangle(int x, int y, int w, int h, std::string videoInputType, PlayerThunderAccessShim shim)
{
    bool bRet = false;
    switch(shim)
    {
        case PlayerThunderAccessShim::VIDEOIN_SHIM:
            bRet = SetVideoRectangle_VIDEOIN(x, y, w, h, videoInputType);
            break;
        case PlayerThunderAccessShim::OTA_SHIM:
            bRet = SetVideoRectangle_OTA(x, y, w, h);
            break;
        case PlayerThunderAccessShim::RMF_SHIM:
            bRet = SetVideoRectangle_RMF(x, y, w, h);
            break;
        default:
            MW_LOG_ERR("Undefined shim used: %d", (int)shim);
            break;
    }
    return bRet;
}

bool PlayerThunderAccess::SetVideoRectangle_VIDEOIN(int x, int y, int w, int h, std::string videoInputType)
{
    int screenWidth = 0;
    int screenHeight = 0;
    int widthFromDS = 0;
    int heightFromDS = 0;
    float width_ratio = 1.0, height_ratio = 1.0;
    if(GetScreenResolution(screenWidth,screenHeight) && GetResolutionFromDS_VIDEOIN(widthFromDS,heightFromDS))
    {
        if((0 != screenWidth) && (0 != screenHeight))
        {
            width_ratio = (float)widthFromDS /(float) screenWidth;
            height_ratio =(float) heightFromDS / (float) screenHeight;
            MW_LOG_INFO("screenWidth:%d screenHeight:%d widthFromDS:%d heightFromDS:%d width_ratio:%f height_ratio:%f",screenWidth,screenHeight,widthFromDS,heightFromDS,width_ratio,height_ratio);
        }
    }

    JsonObject param;
    JsonObject result;
    param["x"] = (int) (x * width_ratio);
    param["y"] = (int) (y * height_ratio);
    param["w"] = (int) (w * width_ratio);
    param["h"] = (int) (h * height_ratio);
    param["typeOfInput"] = videoInputType;
    MW_LOG_WARN("type:%s x:%d y:%d w:%d h:%d w-ratio:%f h-ratio:%f",videoInputType.c_str(),x,y,w,h,width_ratio,height_ratio);
    return InvokeJSONRPC("setVideoRectangle", param, result);

}

bool PlayerThunderAccess::GetResolutionFromDS_VIDEOIN(int & widthFromDS, int & heightFromDS)
{
	bool bRetVal = false;

	JsonObject param;
    JsonObject result;

    PlayerThunderAccess* thunderDsObj = new PlayerThunderAccess(PlayerThunderAccessPlugin::DS);

    if( thunderDsObj->InvokeJSONRPC("getCurrentResolution", param, result) )
    {
        widthFromDS = result["w"].Number();
        heightFromDS = result["h"].Number();
        MW_LOG_INFO("widthFromDS:%d heightFromDS:%d ",widthFromDS, heightFromDS);
        bRetVal = true;
    }
	return bRetVal;
}

bool PlayerThunderAccess::GetScreenResolution(int & screenWidth, int & screenHeight)
{
	bool bRetVal = false;

    JsonObject param;
    JsonObject result;

    PlayerThunderAccess* thunderRDKShellObj = new PlayerThunderAccess(PlayerThunderAccessPlugin::RDKSHELL);

    if( thunderRDKShellObj->InvokeJSONRPC("getScreenResolution", param, result) )
    {
        screenWidth = result["w"].Number();
        screenHeight = result["h"].Number();
        MW_LOG_INFO("screenWidth:%d screenHeight:%d ",screenWidth, screenHeight);
        bRetVal = true;
    }
	return bRetVal;
}

/**
 *  @brief  Registers  Event to input plugin and to mRegisteredEvents list for later use.
 */
void PlayerThunderAccess::RegisterEvent_VIDEOIN(string eventName, std::function<void(const WPEFramework::Core::JSON::VariantContainer&)> functionHandler)
{
	bool bSubscribed;
	bSubscribed = SubscribeEvent(_T(eventName), functionHandler);
	if(bSubscribed)
	{
		mRegisteredEvents.push_back(eventName);
	}
}

/**
 *  @brief  Registers all Events to input plugin
 */
void PlayerThunderAccess::RegisterAllEventsVideoin(std::function<void(std::string)> OnSignalChangedCb, std::function<void(std::string)> OnInputStatusChangedCb)
{

    mOnSignalChangedCb = OnSignalChangedCb;
    mOnInputStatusChangedCb = OnInputStatusChangedCb;

	std::function<void(const WPEFramework::Core::JSON::VariantContainer&)> inputStatusChangedMethod = std::bind(&PlayerThunderAccess::OnInputStatusChanged, this, std::placeholders::_1);

	RegisterEvent_VIDEOIN("onInputStatusChanged",inputStatusChangedMethod);

	std::function<void(const WPEFramework::Core::JSON::VariantContainer&)> signalChangedMethod = std::bind(&PlayerThunderAccess::OnSignalChanged, this, std::placeholders::_1);

	RegisterEvent_VIDEOIN("onSignalChanged",signalChangedMethod);
}

/**
 *  @brief  UnRegisters all Events to input plugin
 */
void PlayerThunderAccess::UnRegisterAllEventsVideoin()
{
    for (auto const& evtName : mRegisteredEvents) {
		UnSubscribeEvent(_T(evtName));
	}
	mRegisteredEvents.clear();
}

/**
 *  @brief  Gets  onSignalChanged and translates into player events
 */
void PlayerThunderAccess::OnInputStatusChanged(const JsonObject& parameters)
{
    std::string message;
    parameters.ToString(message);
    MW_LOG_WARN("%s",message.c_str());

    std::string strStatus = parameters["status"].String();

    mOnInputStatusChangedCb(strStatus);
}

/**
 *  @brief  Gets  onSignalChanged and translates into player events
 */
void PlayerThunderAccess::OnSignalChanged (const JsonObject& parameters)
{
    std::string message;
    parameters.ToString(message);
    MW_LOG_WARN("%s",message.c_str());
    std::string strStatus = parameters["signalStatus"].String();

    mOnSignalChangedCb(strStatus);
}

/**
 *  @brief  calls start on video in specified by port and method name
 */
void PlayerThunderAccess::StartHelperVideoin(int port, std::string videoInputType)
{
	MW_LOG_WARN("port:%d",port);

	videoInputPort = port;
	JsonObject param;
    JsonObject result;
    const std::string & methodName = "startInput";
    param["portId"] = videoInputPort;
    param["typeOfInput"] = videoInputType;
    InvokeJSONRPC(methodName, param, result);
}

/**
 *  @brief  Stops streaming.
 */
void PlayerThunderAccess::StopHelperVideoin(std::string videoInputType)
{
	if( videoInputPort>=0 )
	{

		JsonObject param;
        JsonObject result;
        const std::string methodName = "stopInput";
        param["typeOfInput"] = videoInputType;
        InvokeJSONRPC(methodName, param, result);

		videoInputPort = -1;
	}
}

void PlayerThunderAccess::RegisterEventOnVideoStreamInfoUpdateHdmiin(std::function<void(PlayerVideoStreamInfoData)> videoInfoUpdatedMethodCb)
{
    mVideoInfoUpdatedMethodCb = videoInfoUpdatedMethodCb;
    std::function<void(const WPEFramework::Core::JSON::VariantContainer&)> videoInfoUpdatedMethod = std::bind(&PlayerThunderAccess::OnVideoStreamInfoUpdate, this, std::placeholders::_1);
    RegisterEvent_VIDEOIN("videoStreamInfoUpdate", videoInfoUpdatedMethod);
}

/**
 * @brief  Gets videoStreamInfoUpdate event and translates into player events
 */
void PlayerThunderAccess::OnVideoStreamInfoUpdate(const JsonObject& parameters)
{

    std::string message;
    parameters.ToString(message);
    MW_LOG_WARN("%s",message.c_str());

    JsonObject videoInfoObj = parameters;
    PlayerVideoStreamInfoData data;
    data.progressive = videoInfoObj["progressive"].Boolean();
    data.frameRateN = static_cast<double> (videoInfoObj["frameRateN"].Number());
    data.frameRateD = static_cast<double> (videoInfoObj["frameRateD"].Number());
    data.width = (int)videoInfoObj["width"].Number();
    data.height = (int)videoInfoObj["height"].Number();
    mVideoInfoUpdatedMethodCb(data);

}

void PlayerThunderAccess::RegisterOnPlayerStatusOta(std::function<void(PlayerStatusData)> onPlayerStatusCb)
{
    mOnPlayerStatusCb = onPlayerStatusCb;
    std::function<void(const WPEFramework::Core::JSON::VariantContainer&)> actualMethod = std::bind(&PlayerThunderAccess::onPlayerStatusHandler_OTA, this, std::placeholders::_1);
    mEventSubscribed = SubscribeEvent(_T("onPlayerStatus"), actualMethod);
}

void PlayerThunderAccess::onPlayerStatusHandler_OTA(const JsonObject& parameters)
{

    PlayerStatusData data;
    std::string message;
	parameters.ToString(message);

	JsonObject playerData = parameters[APP_ID].Object();
	MW_LOG_TRACE( "[OTA_SHIM]Received event : message : %s ",  message.c_str());
	/* For detailed event data, we can print or use details like
	   playerData["locator"].String(), playerData["length"].String(), playerData["position"].String() */
	data.currState = playerData["playerStatus"].String();
    data.eventUrl = playerData["locator"].String();
    data.reasonString = playerData["blockedReason"].String();

    JsonObject ratingObj = playerData["rating"].Object();
    ratingObj.ToString(data.ratingString);

    data.ssi = playerData["ssi"].Number();

    JsonObject videoInfoObj = playerData["videoInfo"].Object();
    data.vid_progressive = videoInfoObj["progressive"].Boolean();
    data.vid_frameRateN = static_cast<float> (videoInfoObj["frameRateN"].Number());
	data.vid_frameRateD = static_cast<float> (videoInfoObj["frameRateD"].Number());
    data.vid_aspectRatioWidth = videoInfoObj["aspectRatioWidth"].Number();
    data.vid_aspectRatioHeight = videoInfoObj["aspectRatioHeight"].Number();
    data.vid_width =  videoInfoObj["width"].Number();
    data.vid_height =  videoInfoObj["height"].Number();
    data.vid_codec = videoInfoObj["codec"].String();
    data.vid_hdrType = videoInfoObj["hdrType"].String();
    data.vid_bitrate = videoInfoObj["bitrate"].Number();


    JsonObject audioInfoObj = playerData["audioInfo"].Object();
    data.aud_codec = audioInfoObj["codec"].String();
    data.aud_mixType = audioInfoObj["mixType"].String();
    data.aud_isAtmos =  audioInfoObj["isAtmos"].Boolean();
    data.aud_bitrate = audioInfoObj["bitrate"].Number();

    mOnPlayerStatusCb(data);

}

void PlayerThunderAccess::ReleaseOta()
{
    JsonObject param;
    JsonObject result;
    param["id"] = APP_ID;
    param["tag"] = "MyApp";
    InvokeJSONRPC("release", param, result);

    // unsubscribing only if subscribed
    if (mEventSubscribed)
    {
        UnSubscribeEvent(_T("onPlayerStatus"));
        mEventSubscribed = false;
    }
    else
    {
        MW_LOG_WARN("[OTA_SHIM]OTA Destructor finds Player Status Event not Subscribed !! ");
    }

    MW_LOG_INFO("[OTA_SHIM]StreamAbstractionAAMP_OTA Destructor called !! ");
}

void PlayerThunderAccess::StartOta(std::string url, std::string waylandDisplay, std::string preferredLanguagesString, std::string atsc_preferredLanguagesString, std::string preferredRenditionString, std::string atsc_preferredRenditionString)
{
    MW_LOG_INFO( "[OTA_SHIM] url : %s ", url.c_str());

    JsonObject result;

    SetPreferredAudioLanguages_OTA(preferredLanguagesString, atsc_preferredLanguagesString, preferredRenditionString, atsc_preferredRenditionString);

    JsonObject createParam;
    createParam["id"] = APP_ID;
    createParam["tag"] = "MyApp";
    InvokeJSONRPC("create", createParam, result);

    JsonObject displayParam;
    displayParam["id"] = APP_ID;
    displayParam["display"] = waylandDisplay;
    InvokeJSONRPC("setWaylandDisplay", displayParam, result);

    JsonObject loadParam;
    loadParam["id"] = APP_ID;
    loadParam["url"] = url;
    loadParam["autoplay"] = true;
    InvokeJSONRPC("load", loadParam, result);
}

void PlayerThunderAccess::StopOta()
{
    JsonObject param;
    JsonObject result;
    param["id"] = APP_ID;
    InvokeJSONRPC("stop", param, result);
}

bool PlayerThunderAccess::SetVideoRectangle_OTA(int x, int y, int w, int h)
{
    JsonObject param;
    JsonObject result;
    param["id"] = APP_ID;
    param["x"] = x;
    param["y"] = y;
    param["w"] = w;
    param["h"] = h;
    int screenWidth = 0;
    int screenHeight = 0;
    if(GetScreenResolution(screenWidth,screenHeight))
    {
        JsonObject meta;
        meta["resWidth"] = screenWidth;
        meta["resHeight"] = screenHeight;
        param["meta"] = meta;
    }

    return InvokeJSONRPC("setVideoRectangle", param, result);
}

void PlayerThunderAccess::SetPreferredAudioLanguages(PlayerPreferredAudioData data, PlayerThunderAccessShim shim)
{
    switch(shim)
    {
        case PlayerThunderAccessShim::OTA_SHIM:
            SetPreferredAudioLanguages_OTA(data.preferredLanguagesString, data.pluginPreferredLanguagesString, data.preferredRenditionString, data.pluginPreferredRenditionString);
            break;
        case PlayerThunderAccessShim::RMF_SHIM:
            SetPreferredAudioLanguages_RMF(data.preferredLanguagesString, data.pluginPreferredLanguagesString);
            break;
        default:
            MW_LOG_ERR("Undefined shim used: %d", (int)shim);
            break;
    }
}

void PlayerThunderAccess::SetPreferredAudioLanguages_OTA(std::string preferredLanguagesString, std::string atsc_preferredLanguagesString, std::string preferredRenditionString, std::string atsc_preferredRenditionString)
{
    JsonObject properties;
    bool modifiedLang = false;
    bool modifiedRend = false;

    if((0 != preferredLanguagesString.length()) && (preferredLanguagesString != atsc_preferredLanguagesString)){
        properties["preferredAudioLanguage"] = preferredLanguagesString.c_str();
        modifiedLang = true;
    }
    if((0 != preferredRenditionString.length()) && (preferredRenditionString != atsc_preferredRenditionString)){

        if(0 == preferredRenditionString.compare("VISUALLY_IMPAIRED")){
            properties["visuallyImpaired"] = true;
            modifiedRend = true;
        }else if(0 == preferredRenditionString.compare("NORMAL")){
            properties["visuallyImpaired"] = false;
            modifiedRend = true;
        }else{
            /*No rendition settings to MediaSettings*/
        }
    }
    if(modifiedLang || modifiedRend)
    {
        bool rpcResult = false;
        JsonObject result;
        JsonObject param;

        param["properties"] = properties;
        PlayerThunderAccess* mediaSettingsObj = new PlayerThunderAccess(PlayerThunderAccessPlugin::MEDIASETTINGS);
        mediaSettingsObj->ActivatePlugin();
        rpcResult = mediaSettingsObj->InvokeJSONRPC("setProperties", param, result);
        if (rpcResult){
            if (!result["success"].Boolean()){
                std::string responseStr;
                result.ToString(responseStr);
                MW_LOG_WARN( "[OTA_SHIM] setProperties API failed result:%s", responseStr.c_str());
            }else{
                std::string paramStr;
                param.ToString(paramStr);
                MW_LOG_WARN( "[OTA_SHIM] setProperties success with param:%s", paramStr.c_str());

            }
        }
    }
}

/**
 *  @brief GetAudioTrackInternal get the primary key for the selected audio
 */
int PlayerThunderAccess::GetAudioTrackInternal_OTA()
{
	int pk = 0;

	JsonObject param;
    JsonObject result;

    MW_LOG_TRACE("[OTA_SHIM]Entered ");
    param["id"] = APP_ID;
    InvokeJSONRPC("getAudioTrack", param, result);
    pk = result["pk"].Number();

    return pk;
}

/**
 *  @brief GetAudioTracks get the available audio tracks for the selected service / media
 */
std::string PlayerThunderAccess::GetAudioTracksOta(std::vector<PlayerAudioData> audData)
{
	JsonObject param;
    JsonObject result;
    JsonArray attributesArray;
    std::string aTrackIdx = "";
    std::string output;
    JsonArray outputArray;
    JsonObject audioData;
    int i = 0,arrayCount = 0;
    long bandwidth = -1;
    int currentTrackPk = 0;

    currentTrackPk = GetAudioTrackInternal_OTA();

    attributesArray.Add("pk"); // int - Unique primary key dynamically allocated. Used for track selection.
    attributesArray.Add("name"); // 	Name to display in the UI when doing track selection
    attributesArray.Add("type");  // e,g "MPEG4_AAC" "MPEG2" etc
    attributesArray.Add("description"); //Track description supplied by the content provider
    attributesArray.Add("language"); //ISO 639-2 three character text language (terminology variant per DVB standard, i.e. "deu" instead of "ger")
    attributesArray.Add("contentType"); // e.g "DIALOGUE" , "EMERGENCY" , "MUSIC_AND_EFFECTS" etc
    attributesArray.Add("mixType"); // Signaled audio mix type - orthogonal to AudioTrackType; For example, ac3 could be either surround or stereo.e.g "STEREO" , "SURROUND_SOUND"
    attributesArray.Add("isSelected"); // Is Currently selected track

    param["id"] = APP_ID;
    param["attributes"] = attributesArray;

    InvokeJSONRPC("getAudioTracks", param, result);

    result.ToString(output);
    MW_LOG_TRACE( "[OTA_SHIM]:audio track output : %s ",  output.c_str());
    outputArray = result["table"].Array();
    arrayCount = outputArray.Length();

    for(i = 0; i < arrayCount; i++)
    {
        audioData = outputArray[i].Object();

        if(currentTrackPk == audioData["pk"].Number()){
            aTrackIdx = to_string(i);
        }

        PlayerAudioData temp(audioData["language"].String(), audioData["contentType"].String(), audioData["name"].String(), audioData["type"].String(), (int)audioData["pk"].Number(), audioData["mixType"].String());

        audData.push_back(temp);

    }
    return aTrackIdx;
}

/**
 *  @brief SetAudioTrack sets a specific audio track
 */
std::string PlayerThunderAccess::SetAudioTrackOta(int trackId, int primaryKey)
{
	JsonObject param;
    JsonObject result;

    std::string audioTrackIndex = "";

    param["id"] = APP_ID;

    param["trackPk"] = primaryKey;

    InvokeJSONRPC("setAudioTrack", param, result);
    if (result["success"].Boolean()) {
        audioTrackIndex = to_string(trackId);
    }

    return audioTrackIndex;
}

/**
 * @brief GetTextTracks get the available text tracks for the selected service / media
 */
bool PlayerThunderAccess::GetTextTracksOta(std::vector<PlayerTextData> txtData)
{
	MW_LOG_TRACE("[OTA_SHIM]");
	JsonObject param;
    JsonObject result;
    JsonArray attributesArray;
    std::string output;
    JsonArray outputArray;
    JsonObject textData;
    int arrayCount = 0;

    bool bRet = false;

    attributesArray.Add("pk"); // int - Unique primary key dynamically allocated. Used for track selection.
    attributesArray.Add("name"); //  Name to display in the UI when doing track selection
    attributesArray.Add("type");  // Specific track type for the track - "CC" for ATSC Closed caption
    attributesArray.Add("description"); //Track description supplied by the content provider
    attributesArray.Add("language"); //ISO 639-2 three character text language
    attributesArray.Add("contentType"); // Track content type e.g "HEARING_IMPAIRED", "EASY_READER"
    attributesArray.Add("ccServiceNumber"); // Set to 1-63 for 708 CC Subtitles and 0 for 608
    attributesArray.Add("isSelected"); // Is Currently selected track
    attributesArray.Add("ccTypeIs708"); // Is 708 cc track
    attributesArray.Add("ccType"); // Actual cc track type

    param["id"] = APP_ID;
    param["attributes"] = attributesArray;

    InvokeJSONRPC("getSubtitleTracks", param, result);

    result.ToString(output);
    MW_LOG_TRACE( "[OTA_SHIM]:text track output : %s ", output.c_str());
    outputArray = result["table"].Array();
    arrayCount = outputArray.Length();

    std::string txtTrackIdx = "";
    std::string instreamId;
    int ccIndex = 0;

    for(int i = 0; i < arrayCount; i++)
    {

        std::string trackType;
        textData = outputArray[i].Object();
        trackType = textData["type"].String();

        PlayerTextData temp(textData["type"].String(), textData["language"].String(), (int)textData["ccServiceNumber"].Number(), textData["ccType"].String(), textData["name"].String(), (int)textData["pk"].Number());

        txtData.push_back(temp);

        // txtTracks.push_back(TextTrackInfo(index, languageCode, true, empty, textData["name"].String(), serviceNo, empty, (int)textData["pk"].Number()));

    }

    if(!txtData.empty())
    {
        bRet = true;
    }

    return bRet;
}

/**
 *  @brief Disable Restrictions (unlock) till seconds mentioned
 */
void PlayerThunderAccess::DisableContentRestrictionsOta(long grace, long time, bool eventChange)
{
	JsonObject param;
    JsonObject result;
    param["id"] = APP_ID;
    if(-1 == grace){

        param["grace"] = -1;
        param["time"] = -1;
        param["eventChange"] = false;
        MW_LOG_WARN( "[OTA_SHIM] unlocked till next reboot or explicit enable" );
    }else{
        param["grace"] = 0;
        param["time"] = time;
        param["eventChange"] = eventChange;

        if(-1 != time)
            MW_LOG_WARN( "[OTA_SHIM] unlocked for %ld sec ", time);

        if(eventChange)
            MW_LOG_WARN( "[OTA_SHIM] unlocked till next program ");
    }
    InvokeJSONRPC("disableContentRestrictionsUntil", param, result);


}

/**
 *  @brief Enable Content Restriction (lock)
 */
void PlayerThunderAccess::EnableContentRestrictionsOta()
{
	MW_LOG_WARN( "[OTA_SHIM] locked ");
    JsonObject param;
    JsonObject result;
    param["id"] = APP_ID;
    InvokeJSONRPC("enableContentRestrictions", param, result);
}

bool PlayerThunderAccess::InitRmf()
{
    bool retval = true;
    ActivatePlugin();
	JsonObject param;
	JsonObject result;
	param["source_type"] = "qam";
	if(false == InvokeJSONRPC("initialize", param, result)) //Note: do not terminate unless we're desperate for resources. deinit is sluggish.
	{
		MW_LOG_ERR("Failed to initialize RMF plugin");
		retval = false;
	}

    return retval;
}

void PlayerThunderAccess::onPlayerStatusHandler_RMF(const JsonObject& parameters) {
	std::string message;
	parameters.ToString(message);

	JsonObject playerData = parameters[APP_ID].Object();
	MW_LOG_WARN( "[RMF_SHIM]Received event : message : %s ",  message.c_str());


	std::string title = parameters["title"].String();

	mOnPlayerStatusHandlerCb(title);
}

void PlayerThunderAccess::onPlayerErrorHandler_RMF(const JsonObject& parameters) {
	std::string message;
	parameters.ToString(message);

	MW_LOG_WARN( "[RMF_SHIM]Received error : message : %s ",  message.c_str());

	std::string error_message = parameters["source"].String() + ": " + parameters["title"].String() + "- " + parameters["message"].String();
	mOnPlayerErrorHandlerCb(error_message);
}

bool PlayerThunderAccess::StartRmf(std::string url, std::function<void(std::string)> onPlayerStatusHandlerCb, std::function<void(std::string)> onPlayerErrorHandlerCb)
{
    bool bRet = true;
    mOnPlayerStatusHandlerCb = onPlayerStatusHandlerCb;
    mOnPlayerErrorHandlerCb = onPlayerErrorHandlerCb;

    JsonObject result;

	std::function<void(const WPEFramework::Core::JSON::VariantContainer&)> eventHandler = std::bind(&PlayerThunderAccess::onPlayerStatusHandler_RMF, this, std::placeholders::_1);
	std::function<void(const WPEFramework::Core::JSON::VariantContainer&)> errorHandler = std::bind(&PlayerThunderAccess::onPlayerErrorHandler_RMF, this, std::placeholders::_1);

	if(true != SubscribeEvent(_T("onStatusChanged"), eventHandler))
	{
		MW_LOG_ERR("Failed to register for onStatusChanged notification from RMF plugin");
	}
	if(true != SubscribeEvent(_T("onError"), errorHandler))
	{
		MW_LOG_ERR("Failed to register for onError notification from RMF plugin");
	}

	JsonObject playParam;
	playParam["source_type"] = "qam";
	playParam["identifier"] = url;
	if(true != InvokeJSONRPC("play", playParam, result))
	{
        bRet = false;
	}

    return bRet;
}

/**
 * @brief SetPreferredAudioLanguages set the preferred audio language list
 */
void PlayerThunderAccess::SetPreferredAudioLanguages_RMF(std::string preferredLanguagesString, std::string rmf_preferredLanguages)
{
	JsonObject properties;
	bool modifiedLang = false;
	bool modifiedRend = false;

	if((0 != preferredLanguagesString.length()) && (preferredLanguagesString != rmf_preferredLanguages)){
		properties["preferredAudioLanguage"] = preferredLanguagesString.c_str();
		modifiedLang = true;
	}
	if(modifiedLang || modifiedRend)
	{
		bool rpcResult = false;
		JsonObject result;
		JsonObject param;

		param["properties"] = properties;
		//TODO: Pass preferred audio language to MediaEngineRMF. Not currently supported.

	}
}

void PlayerThunderAccess::StopRmf()
{
    JsonObject param;
	JsonObject result;;
	if(true != InvokeJSONRPC("stop", param, result))
	{
		MW_LOG_ERR("Failed to stop RMF playback");
	}
	UnSubscribeEvent(_T("onStatusChanged"));
	UnSubscribeEvent(_T("onError"));
}

/**
 * @brief SetVideoRectangle sets the position coordinates (x,y) & size (w,h)
 */
bool PlayerThunderAccess::SetVideoRectangle_RMF(int x, int y, int w, int h)
{
    bool bRet = true;
	JsonObject param;
	JsonObject videoRect;
	JsonObject result;
	videoRect["x"] = x;
	videoRect["y"] = y;
	videoRect["width"] = w;
	videoRect["height"] = h;

	param["video_rectangle"] = videoRect;

	if(true != InvokeJSONRPC("setVideoRectangle", param, result))
	{
		bRet = false;
	}

    return bRet;
}

/**
 * @brief DeleteWatermark delete watermark of layer
 */
bool PlayerThunderAccess::DeleteWatermark(int layerID)
{
    bool ret = false;
    JsonObject param, result;
    param["id"] = layerID;
    bool success = InvokeJSONRPC("deleteWatermark", param, result);
    if(success && result["success"].Boolean())
    {
        ret = true;
    }

    return ret;
}

/**
 * @brief CreateWatermark create watermark
 */
bool PlayerThunderAccess::CreateWatermark(int layerID)
{
    bool ret = false;
    JsonObject param, result;
    param["id"] = layerID;
    param["zorder"] = 1;
    bool success = InvokeJSONRPC("createWatermark", param, result);
    if(success && result["success"].Boolean())
    {
        ret = true;
    }

    return ret;
}

/**
 * @brief ShowWatermark create watermark
 */
bool PlayerThunderAccess::ShowWatermark(int opacity)
{
    bool ret = false;
    JsonObject param, result;
    param["show"] = true;
    param["alpha"] = opacity;
    bool success = InvokeJSONRPC("showWatermark", param, result);
    if(success && result["success"].Boolean())
    {
        ret = true;
    }

    return ret;
}

/**
 * @brief HideWatermark hides watermark
 */
bool PlayerThunderAccess::HideWatermark()
{
    bool ret = false;
    JsonObject param, result;
	param["show"] = false;
	bool success = InvokeJSONRPC("showWatermark", param, result);

	if(success && result["success"].Boolean())
    {
        ret = true;
    }
    return ret;
}

/**
 * @brief UpdateWatermark updates watermark
 */
bool PlayerThunderAccess::UpdateWatermark(int layerID, int sharedMemoryKey, int size)
{
    bool ret = false;
    JsonObject param, result;
	param["id"] = layerID;
	param["key"] = sharedMemoryKey;
	param["size"] = size;
	bool success = InvokeJSONRPC("updateWatermark", param, result);
	if(success && result["success"].Boolean())
    {
        ret = true;
    }
    return ret;
}

/**
 * @brief GetMetaDataWatermark gets metadata for watermark
 */
std::string PlayerThunderAccess::GetMetaDataWatermark()
{
    JsonObject param, result;
	bool success = InvokeJSONRPC("PersistentStoreMetadata", param, result);
	std::string metaData="";
	if(success)
	{
		metaData = result["metadata"].String();
    }
    return metaData;
}

/**
 * @brief PersistentStoreSaveWatermark saves in persistent store
 */
bool PlayerThunderAccess::PersistentStoreSaveWatermark(const char* base64Image, std::string metaData)
{
    bool ret = false;
    JsonObject param, result;
    param["image"] = base64Image;
    param["metadata"] = metaData;
    bool success = InvokeJSONRPC("PersistentStoreSave", param, result);
    if(success && result["success"].Boolean())
    {
        ret = true;
    }
    return ret;
}

/**
 * @brief PersistentStoreLoadWatermark loads from persistent store
 */
bool PlayerThunderAccess::PersistentStoreLoadWatermark(int layerID)
{
    bool ret = false;
    JsonObject param, result;
	param["id"] = layerID;
	bool success = InvokeJSONRPC("PersistentStoreLoad", param, result);
	if(success && result["success"].Boolean())
    {
        ret = true;
    }
	return ret;
}
