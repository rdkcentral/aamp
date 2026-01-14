/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2024 RDK Management
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
 * @file PlayerMetadata.hpp 
 * @brief Context-free common utility functions.
 */
#ifndef PLAYER_METADATA_HPP
#define PLAYER_METADATA_HPP

#include <string>
#include "PlayerLogManager.h"

// Global variable for player name
static std::string gPlayerName;

/*
 * @brief To set the player's name
 *
 */
void SetPlayerName(const std::string& name) {
    if(name != gPlayerName)
    {
        MW_LOG_INFO("Set Player Name[%s]",name.c_str());
        gPlayerName = name;
    }
}

/*
 * @brief To get the player's name
 *
 */
std::string GetPlayerName() {
    MW_LOG_INFO("Get Player Name[%s]",gPlayerName.c_str());
    return gPlayerName;
}

#endif // PLAYER_METADATA_HPP