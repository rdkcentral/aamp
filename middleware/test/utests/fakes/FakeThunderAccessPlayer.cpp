/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2018 RDK Management
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
#include "ThunderAccessPlayer.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Weffc++"

/**
 * @brief Structure to save the Thunder security token details
 **/
typedef struct ThunderSecurityPlayer
{
    std::string securityToken;
    int tokenStatus;
    bool tokenQueried;
    ThunderSecurityPlayer(){ };
}ThunderSecurityPlayerData;

ThunderSecurityPlayerData gSecurityPlayerData;


/**
 * @brief  ThunderAccessPlayer constructor
 */
ThunderAccessPlayer::ThunderAccessPlayer(std::string callsign)
{

}

/**
 * @brief  ThunderAccessPlayer destructor
 */
ThunderAccessPlayer::~ThunderAccessPlayer()
{
}

/**
 * @brief  ActivatePlugin
 */
bool ThunderAccessPlayer::ActivatePlugin()
{
    return false;
}

/*To Do: Only JSON Object can be used as parameter now*/

/**
 * @brief  unSubscribeEvent
 */
bool ThunderAccessPlayer::UnSubscribeEvent (std::string eventName)
{
    return false;
}
