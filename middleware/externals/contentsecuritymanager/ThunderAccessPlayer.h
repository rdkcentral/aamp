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
 * @file ThunderAccessPlayer.h
 * @brief shim for dispatching UVE HDMI input playback
 */

#ifndef THUNDERACCESSPLAYER_H_
#define THUNDERACCESSPLAYER_H_

#include <string>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>

#include "Module.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Weffc++"
#include <core/core.h>
#include <websocket/websocket.h>
#pragma GCC diagnostic pop

using namespace std;
using namespace WPEFramework;

#define THUNDER_RPC_TIMEOUT 5000

class  PlayerLogManager;

/**
 * @class ThunderAccessPlayer
 * @brief Support Thunder Plugin Access from AAMP
 */
class ThunderAccessPlayer
{
public:
    /**
     *   @fn ThunderAccessPlayer
     *   @note   Security token acquisition, controller object creation
     */
    ThunderAccessPlayer(std::string callsign);
	
    /**
     *   @fn ~ThunderAccessPlayer
     *   @note   clean up
     */
    ~ThunderAccessPlayer();
   
    /**
     *   @brief  ThunderAccessPlayer copy constructor disabled
     */
    ThunderAccessPlayer(const ThunderAccessPlayer&) = delete;

    /**
     *   @brief  ThunderAccessPlayer assignment disabled
     */
    ThunderAccessPlayer& operator=(const ThunderAccessPlayer&) = delete;

    /**
     *   @brief  ActivatePlugin
     *   @note   Plugin activation and Remote object creation
     *   @param  Plugin Callsign
     *   @retval true on success
     *   @retval false on failure
     */
    bool ActivatePlugin();
    /**
     *   @fn InvokeJSONRPC
     *   @note   Invoke JSONRPC call for the plugin
     *   @param  method,param,result,  method,waitTime reference to input param, result and waitTime (default = THUNDER_RPC_TIMEOUT)
     *   @retval true on success
     *   @retval false on failure
     */
    bool InvokeJSONRPC(std::string method, const JsonObject &param, JsonObject &result, const uint32_t waitTime = THUNDER_RPC_TIMEOUT);
	
    /**
     *   @fn SubscribeEvent
     *   @note   Subscribe event data for the specific plugin
     *   @param  eventName,functionHandler Event name, Event handler
     *   @retval true on success
     *   @retval false on failure
     */
    bool SubscribeEvent (std::string eventName, std::function<void(const WPEFramework::Core::JSON::VariantContainer&)> functionHandler);
    /**
     *   @fn UnSubscribeEvent
     *   @note   unSubscribe event data for the specific plugin
     *   @param  eventName Event name
     *   @retval true on success
     *   @retval false on failure
     */
    bool UnSubscribeEvent (std::string eventName);

private:
    /**< The Remote object connected to specific Plugin*/
    JSONRPC::LinkType<Core::JSON::IElement> *remoteObject;
    /**< The Remote object connected to controller Plugin*/
    JSONRPC::LinkType<Core::JSON::IElement> *controllerObject;
    std::string pluginCallsign;
};
#endif // THUNDERACCESSPLAYER_H_
