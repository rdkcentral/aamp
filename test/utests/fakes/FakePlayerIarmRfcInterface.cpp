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
 * @file FakePlayerIarmRfcInterface.cpp
 * @brief Fake Player Iarm Rfc Interface manager
 */

#include "PlayerIarmRfcInterface.h"

/**< Static local variables */
std::shared_ptr<PlayerIarmRfcInterface> PlayerIarmRfcInterface::s_pPlayerOP = NULL;

/**
 * @brief PlayerIarmRfcInterface Constructor
 */
PlayerIarmRfcInterface::PlayerIarmRfcInterface()
{
}

/**
 * @brief PlayerIarmRfcInterface Destructor
 */
PlayerIarmRfcInterface::~PlayerIarmRfcInterface()
{
    s_pPlayerOP = NULL;
}


/**
 * @brief Check if source is UHD using video decoder dimensions
 */
bool PlayerIarmRfcInterface::IsSourceUHD()
{
    return false;
}

/**
 * @brief Is there support for live latency correction
 */
bool PlayerIarmRfcInterface::IsLiveLatencyCorrectionSupported()
{
	return false;
}

/**
 * @brief gets display resolution
 */
void PlayerIarmRfcInterface::GetDisplayResolution(int &width, int &height)
{
}

/**
 * @brief Check if  PlayerIarmRfcInterfaceInstance active
 */
bool PlayerIarmRfcInterface::IsPlayerIarmRfcInterfaceInstanceActive()
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
std::shared_ptr<PlayerIarmRfcInterface> PlayerIarmRfcInterface::GetPlayerIarmRfcInterfaceInstance()
{
    if(s_pPlayerOP == NULL) {
        s_pPlayerOP = std::shared_ptr<PlayerIarmRfcInterface>(new PlayerIarmRfcInterface());
    }

    return s_pPlayerOP;
}

/**
 * @brief Get paramName TR181 config
 */
char * PlayerIarmRfcInterface::GetTR181PlayerConfig(const char * paramName, size_t & iConfigLen)
{
	return nullptr;
}

/**
 * @brief get active interface , true if wifi false if not
 */
bool PlayerIarmRfcInterface::GetActiveInterface()
{
    return false;
}

/**
 * @brief setup active interface handler , true if wifi false if not
 */
bool PlayerIarmRfcInterface::IsActiveStreamingInterfaceWifi(void)
{
    return false;
}

/**
 * @brief initilaize IARM
 */
void PlayerIarmRfcInterface::IARMInit(const char* processName)
{
}

/**
 * @brief should wifi curl header be configured
 */
bool PlayerIarmRfcInterface::IsConfigWifiCurlHeader()
{
    return false;
}