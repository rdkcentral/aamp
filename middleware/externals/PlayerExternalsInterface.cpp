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
 * @file PlayerExternalsInterface.cpp
 * @brief Output protection management for Player
 */

#include "PlayerExternalsInterface.h"
#include "PlayerExternalUtils.h"

#ifdef IARM_MGR
#include "PlayerExternalsRdkInterface.h"
#endif

/**< Static variable for singleton */
std::shared_ptr<PlayerExternalsInterface> PlayerExternalsInterface::s_pPlayerOP = NULL;

/**
 * @brief PlayerExternalsInterface Constructor
 */
PlayerExternalsInterface::PlayerExternalsInterface()
{
#ifdef IARM_MGR
    MW_PRE_LOGGER_LOG("Device API IARM/Firebolt\n");
    m_pIarmInterface = PlayerExternalsRdkInterface::GetPlayerExternalsRdkInterfaceInstance();
#else
    MW_PRE_LOGGER_LOG("Device API FAKE\n");
    m_pIarmInterface = std::shared_ptr<PlayerExternalsInterfaceBase>(new FakePlayerExternalsInterface());
#endif

}

/**
 * @brief PlayerExternalsInterface Destructor
 */
PlayerExternalsInterface::~PlayerExternalsInterface()
{
    m_pIarmInterface = nullptr;
    s_pPlayerOP = NULL;    
}

void PlayerExternalsInterface::Initialize()
{
    if(s_pPlayerOP != NULL)
    {
        MW_PRE_LOGGER_LOG("PlayerExternalsInterface::Initialize\n");
        m_pIarmInterface->Initialize();
    }
    else
    {
        MW_PRE_LOGGER_LOG("PlayerExternalsInterface not found to initialize\n");
    }
}

/**
 * @brief Check if source is UHD using video decoder dimensions
 */
bool PlayerExternalsInterface::IsSourceUHD()
{
    return m_pIarmInterface->IsSourceUHD();
}

/**
 * @brief gets display resolution
 */
void PlayerExternalsInterface::GetDisplayResolution(int &width, int &height)
{
    m_pIarmInterface->GetDisplayResolution(width, height);
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
 * @brief gets paramName TR181 config
 */
char * PlayerExternalsInterface::GetTR181PlayerConfig(const char * paramName, size_t & iConfigLen)
{
    char * sRet = nullptr;
    sRet = m_pIarmInterface->GetTR181Config(paramName, iConfigLen);    
    return sRet;
}

/**
 * @brief gets current active interface
 */
bool PlayerExternalsInterface::GetActiveInterface()
{
    bool bRet = false;
    bRet = m_pIarmInterface->GetActiveInterface();
    return bRet;
}

/**
 * @brief checks if Wifi Curl Header ought to be configured
 */
bool PlayerExternalsInterface::IsConfigWifiCurlHeader()
{
    bool bRet = false;
#ifdef IARM_MGR
    bRet = true;
#else
    bRet = false;
#endif
    return bRet;
}

void PlayerExternalsInterface::SetUseFireBoltSDK(bool t_use_firebolt_sdk)
{
    m_pIarmInterface->SetUseFireBoltSDK(t_use_firebolt_sdk);
}
