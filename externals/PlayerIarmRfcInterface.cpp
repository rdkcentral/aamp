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
 * @file PlayerIarmRfcInterface.cpp
 * @brief Output protection management for Player
 */

#include "PlayerIarmRfcInterface.h"
#include "PlayerExternalUtils.h"
#ifdef IARM_MGR
#include "PlayerIarmRdkInterface.h"
#endif

/**< Static variable for singleton */
std::shared_ptr<PlayerIarmRfcInterface> PlayerIarmRfcInterface::s_pPlayerOP = NULL;

/**
 * @brief PlayerIarmRfcInterface Constructor
 */
PlayerIarmRfcInterface::PlayerIarmRfcInterface()
{
#ifdef IARM_MGR
    if(!IsContainerEnvironment())
    {
        m_pIarmInterface = new PlayerIarmRdkInterface();
    }
    else
    {
        m_pIarmInterface = new FakePlayerIarmInterface();
    }
    
#else
    m_pIarmInterface = new FakePlayerIarmInterface();
#endif
    // Get initial HDCP status
    m_pIarmInterface->SetHDMIStatus();
    m_pIarmInterface->IARMRegisterDsMgrEventHandler();

}

/**
 * @brief PlayerIarmRfcInterface Destructor
 */
PlayerIarmRfcInterface::~PlayerIarmRfcInterface()
{
    m_pIarmInterface->IARMRemoveDsMgrEventHandler();
    s_pPlayerOP = NULL;
}

/**
 * @brief Check if source is UHD using video decoder dimensions
 */
bool PlayerIarmRfcInterface::IsSourceUHD()
{
    return m_pIarmInterface->IsSourceUHD();
}

/**
 * @brief check if Live Latency COrrection is supported
 */
bool PlayerIarmRfcInterface::IsLiveLatencyCorrectionSupported()
{
    bool bRet = false;;
    if(!IsContainerEnvironment())
    {
	    bRet = m_pIarmInterface->IsLiveLatencyCorrectionSupported();
    }
    
    return bRet;
}

/**
 * @brief gets display resolution
 */
void PlayerIarmRfcInterface::GetDisplayResolution(int &width, int &height)
{
    if(!IsContainerEnvironment())
    {
        m_pIarmInterface->GetDisplayResolution(width, height);
    }
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
 * @brief gets paramName TR181 config
 */
char * PlayerIarmRfcInterface::GetTR181PlayerConfig(const char * paramName, size_t & iConfigLen)
{
    char * sRet = nullptr;
    if(!IsContainerEnvironment())
    {
	    sRet = m_pIarmInterface->GetTR181Config(paramName, iConfigLen);
    }
    
    return sRet;
}

/**
 * @brief gets current active interface
 */
bool PlayerIarmRfcInterface::GetActiveInterface()
{
    bool bRet = false;
    if(!IsContainerEnvironment())
    {
        bRet = m_pIarmInterface->GetActiveInterface();
    }

    return bRet;
}

/**
 * @brief sets up interfaces to retrieve current active interface
 */
bool PlayerIarmRfcInterface::IsActiveStreamingInterfaceWifi(void)
{
    bool bRet = false;
#ifdef IARM_MGR
    if(!IsContainerEnvironment())
    {
        bRet = PlayerIarmRdkInterface::IsActiveStreamingInterfaceWifi();
    }
#else
    bRet = FakePlayerIarmInterface::IsActiveStreamingInterfaceWifi();
#endif

    return bRet;
}

/**
 * @brief Initializes IARM
 */
void PlayerIarmRfcInterface::IARMInit(const char* processName){

#ifdef IARM_MGR
    if(!IsContainerEnvironment())
    {
        PlayerIarmRdkInterface::IARMInit(processName);
    }
#else
    FakePlayerIarmInterface::IARMInit(processName);
#endif

}

/**
 * @brief checks if Wifi Curl Header ought to be configured
 */
bool PlayerIarmRfcInterface::IsConfigWifiCurlHeader()
{
    bool bRet = false;
#ifdef IARM_MGR
    if(!IsContainerEnvironment())
    {
        bRet = true;
    }
#else
    bRet = false;
#endif
    return bRet;
}

