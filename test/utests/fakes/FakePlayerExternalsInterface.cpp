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
 * @file FakePlayerExternalsInterface.cpp
 * @brief Fake Player Iarm Rfc Interface manager
 */

#include "PlayerExternalsInterface.h"

/**< Static local variables */
std::shared_ptr<PlayerExternalsInterface> PlayerExternalsInterface::s_pPlayerOP = NULL;

/**
 * @brief PlayerExternalsInterface Constructor
 */
PlayerExternalsInterface::PlayerExternalsInterface()
{
}

/**
 * @brief PlayerExternalsInterface Destructor
 */
PlayerExternalsInterface::~PlayerExternalsInterface()
{
    s_pPlayerOP = NULL;
}


/**
 * @brief Check if source is UHD using video decoder dimensions
 */
bool PlayerExternalsInterface::IsSourceUHD()
{
    return false;
}

/**
 * @brief gets display resolution
 */
void PlayerExternalsInterface::GetDisplayResolution(int &width, int &height)
{
}

/**
 * @brief Check if  PlayerExternalsInterfaceInstance active
 */
bool PlayerExternalsInterface::IsPlayerExternalsInterfaceInstanceActive()
{
    bool retval = false;

    if(s_pPlayerOP != NULL) {
        retval = true;
    }
    return retval;
}

/**
 * @brief Singleton for object creation
 */
std::shared_ptr<PlayerExternalsInterface> PlayerExternalsInterface::GetPlayerExternalsInterfaceInstance()
{
    if(s_pPlayerOP == NULL) {
        s_pPlayerOP = std::shared_ptr<PlayerExternalsInterface>(new PlayerExternalsInterface());
    }

    return s_pPlayerOP;
}

/**
 * @brief Get paramName TR181 config
 */
char * PlayerExternalsInterface::GetTR181PlayerConfig(const char * paramName, size_t & iConfigLen)
{
	return nullptr;
}

/**
 * @brief get active interface , true if wifi false if not
 */
bool PlayerExternalsInterface::GetActiveInterface()
{
    return false;
}

/**
 * @brief setup active interface handler , true if wifi false if not
 */
bool PlayerExternalsInterface::IsActiveStreamingInterfaceWifi(void)
{
    return false;
}

/**
 * @brief initilaize IARM
 */
void PlayerExternalsInterface::IARMInit(const char* processName)
{
}

/**
 * @brief should wifi curl header be configured
 */
bool PlayerExternalsInterface::IsConfigWifiCurlHeader()
{
    return false;
}