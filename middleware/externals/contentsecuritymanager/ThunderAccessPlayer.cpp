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
#include "PlayerLogManager.h"
#include "ThunderAccessPlayer.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Weffc++"
#ifdef USE_CPP_THUNDER_PLUGIN_ACCESS
#ifndef DISABLE_SECURITY_TOKEN
#include <securityagent/SecurityTokenUtil.h>
#endif
#pragma GCC diagnostic pop

using namespace std;
using namespace WPEFramework;
#endif
#define SERVER_DETAILS  "127.0.0.1:9998"

#define MAX_LENGTH 1024

//Delete non-array object
#define SAFE_DELETE(ptr) { delete(ptr); ptr = NULL; }

/**
 * @brief Structure to save the Thunder security token details
 **/
typedef struct ThunderSecurityPlayer
{
    std::string securityToken;
    int tokenStatus;
    bool tokenQueried;
    ThunderSecurityPlayer(): securityToken(), tokenStatus(0), tokenQueried(false) { };
}ThunderSecurityPlayerData;

ThunderSecurityPlayerData gSecurityPlayerData;


/**
 * @brief  ThunderAccessPlayer constructor
 */
ThunderAccessPlayer::ThunderAccessPlayer(std::string callsign)
                 : remoteObject(NULL)
                   ,controllerObject(NULL)
                   ,pluginCallsign(callsign)
{
    MW_LOG_INFO( "[ThunderAccessPlayer]Inside");
    uint32_t status = Core::ERROR_NONE;

    Core::SystemInfo::SetEnvironment(_T("THUNDER_ACCESS"), (_T(SERVER_DETAILS)));
    string sToken = "";
#ifdef DISABLE_SECURITY_TOKEN
     gSecurityPlayerData.securityToken = "token=" + sToken;
     gSecurityPlayerData.tokenQueried = true;
#else
    if(!gSecurityPlayerData.tokenQueried)
    {
        unsigned char buffer[MAX_LENGTH] = {0};
        gSecurityPlayerData.tokenStatus = GetSecurityToken(MAX_LENGTH,buffer);
        if(gSecurityPlayerData.tokenStatus > 0){
            MW_LOG_INFO( "[ThunderAccessPlayer] : GetSecurityToken success");
            sToken = (char*)buffer;
            gSecurityPlayerData.securityToken = "token=" + sToken;
        }
        gSecurityPlayerData.tokenQueried = true;

        //MW_LOG_WARN( "[ThunderAccessPlayer] securityToken : %s tokenStatus : %d tokenQueried : %s", gSecurityPlayerData.securityToken.c_str(), gSecurityPlayerData.tokenStatus, ((gSecurityPlayerData.tokenQueried)?"true":"false"));
    }
#endif
    if (NULL == controllerObject) {
        /*Passing empty string instead of Controller callsign.This is assumed as controller plugin.*/
        if(gSecurityPlayerData.tokenStatus > 0){
            controllerObject = new JSONRPC::LinkType<Core::JSON::IElement>(_T(""), _T(""), false, gSecurityPlayerData.securityToken);
        }
        else{
            controllerObject = new JSONRPC::LinkType<Core::JSON::IElement>(_T(""));
        }

        if (NULL == controllerObject) {
            MW_LOG_WARN( "[ThunderAccessPlayer] Controller object creation failed");
        } else {
            MW_LOG_INFO( "[ThunderAccessPlayer] Controller object creation success");
        }
    }

    if(gSecurityPlayerData.tokenStatus > 0){
        remoteObject = new JSONRPC::LinkType<Core::JSON::IElement>(_T(pluginCallsign), _T(""), false, gSecurityPlayerData.securityToken);
    }
    else{
        remoteObject = new JSONRPC::LinkType<Core::JSON::IElement>(_T(pluginCallsign), _T(""));
    }
    if (NULL == remoteObject) {
        MW_LOG_WARN( "[ThunderAccessPlayer] %s Client initialization failed", pluginCallsign.c_str());
    } else {
        MW_LOG_INFO( "[ThunderAccessPlayer] %s Client initialization success", pluginCallsign.c_str());
    }
}

/**
 * @brief  ThunderAccessPlayer destructor
 */
ThunderAccessPlayer::~ThunderAccessPlayer()
{
    SAFE_DELETE(controllerObject);
    SAFE_DELETE(remoteObject);
}

/**
 * @brief  ActivatePlugin
 */
bool ThunderAccessPlayer::ActivatePlugin()
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
            MW_LOG_INFO( "[ThunderAccessPlayer] %s plugin Activated. Response : %s ", pluginCallsign.c_str(), response.c_str());
        }
        else
        {
            MW_LOG_WARN( "[ThunderAccessPlayer] %s plugin Activation failed with error status : %u ", pluginCallsign.c_str(), status);
            ret = false;
        }
    } else {
        MW_LOG_WARN( "[ThunderAccessPlayer] Controller Object NULL ");
        ret = false;
    }
    return ret;
}

/*To Do: Only JSON Object can be used as parameter now*/
/**
 * @brief  subscribeEvent
 */
bool ThunderAccessPlayer::SubscribeEvent (string eventName, std::function<void(const WPEFramework::Core::JSON::VariantContainer&)> functionHandler)
{
    bool ret = true;
    uint32_t status = Core::ERROR_NONE;
    if (NULL != remoteObject) {
        status = remoteObject->Subscribe<JsonObject>(THUNDER_RPC_TIMEOUT, _T(eventName), functionHandler);
        if (Core::ERROR_NONE == status) {
            MW_LOG_INFO( "[ThunderAccessPlayer] Subscribed to : %s", eventName.c_str());
        } else {
            MW_LOG_WARN( "[ThunderAccessPlayer] Subscription failed for : %s with error status %u", eventName.c_str(), status);
            ret = false;
        }
    } else {
        MW_LOG_WARN( "[ThunderAccessPlayer] remoteObject not created for the plugin!");
        ret = false;
    }
    return ret;
}

/*To Do: Only JSON Object can be used as parameter now*/

/**
 * @brief  unSubscribeEvent
 */
bool ThunderAccessPlayer::UnSubscribeEvent (std::string eventName)
{
    bool ret = true;
    if (NULL != remoteObject) {
        remoteObject->Unsubscribe(THUNDER_RPC_TIMEOUT, _T(eventName));
        MW_LOG_INFO( "[ThunderAccessPlayer] UnSubscribed : %s event", eventName.c_str());
    } else {
        MW_LOG_WARN( "[ThunderAccessPlayer] remoteObject not created for the plugin!");
        ret = false;
    }
    return ret;
}

/**
 *  @brief  invokeJSONRPC
 *  @note   Invoke JSONRPC call for the plugin
 */
bool ThunderAccessPlayer::InvokeJSONRPC(std::string method, const JsonObject &param, JsonObject &result, const uint32_t waitTime)
{
    bool ret = true;
    std::string response;
    uint32_t status = Core::ERROR_NONE;

    if(NULL == remoteObject)
    {
        MW_LOG_WARN( "[ThunderAccessPlayer] client not initialized! ");
        return false;
    }

    JsonObject result_internal;
    status = remoteObject->Invoke<JsonObject, JsonObject>(waitTime, _T(method), param, result_internal);
    if (Core::ERROR_NONE == status)
    {
        if (result_internal["success"].Boolean()) {
            result_internal.ToString(response);
            MW_LOG_TRACE( "[ThunderAccessPlayer] %s success! Response : %s", method.c_str() , response.c_str());
        } else {
            result_internal.ToString(response);
            MW_LOG_WARN( "[ThunderAccessPlayer] %s call failed! Response : %s", method.c_str() , response.c_str());
            ret = false;
        }
    } else {
        MW_LOG_WARN( "[ThunderAccessPlayer] %s : invoke failed with error status %u", method.c_str(), status);
        ret = false;
    }

    result = result_internal;
    return ret;
}
