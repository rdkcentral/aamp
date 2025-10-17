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
 * @file PlayerExternalsRdkInterface.cpp
 * @brief player interface with IARM specific to RDK
 */
#include "PlayerExternalsRdkInterface.h"
#include "PlayerExternalUtils.h"
#include "DeviceInterfaceBase.h"
#include "DeviceIARMInterface.h"
#include "DeviceFireboltInterface.h"
#include "PlayerExternalsInterface.h"

#include <cstdio>

#define DISPLAY_WIDTH_UNKNOWN       -1  /**< Parsing failed for getResolution().getName(); */
#define DISPLAY_HEIGHT_UNKNOWN      -1  /**< Parsing failed for getResolution().getName(); */
#define DISPLAY_RESOLUTION_NA        0  /**< Resolution not available yet or not connected to HDMI */

std::shared_ptr<PlayerExternalsRdkInterface> s_pPlayerIarmRdkOP = nullptr;

static bool isInterfaceWifi = false;

/**
 * @brief Singleton for object creation
 */
std::shared_ptr<PlayerExternalsRdkInterface> PlayerExternalsRdkInterface::GetPlayerExternalsRdkInterfaceInstance()
{
    if(s_pPlayerIarmRdkOP == nullptr) {
        s_pPlayerIarmRdkOP = std::shared_ptr<PlayerExternalsRdkInterface>(new PlayerExternalsRdkInterface());
    }

    return s_pPlayerIarmRdkOP;
}

PlayerExternalsRdkInterface::PlayerExternalsRdkInterface()
{
    
}

void PlayerExternalsRdkInterface::Initialize()
{

    MW_PRE_LOGGER_LOG("Initializing started \n");

    /*
    IARM Deprecation Note:
    IARM is to be deprecated in favor of DeviceSettings and Firebolt Device API.
    */
    /*
    Remove the section between the comment section remove-start and remove-end when deprecating IARM
    */
    
    //remove-start
    //initialize only if needed
    if(m_initialized != InitState::NOT_INITIALIZED)
    {
        if(m_initialized == InitState::FIREBOLT && (m_use_firebolt_sdk || IsContainerEnvironment()))
        {
            MW_PRE_LOGGER_LOG("Firebolt already Inited \n");
            //firebolt already inited
            return;
        }
        else if(m_initialized == InitState::IARM && (!m_use_firebolt_sdk) && (!IsContainerEnvironment()))
        {
            MW_PRE_LOGGER_LOG("IARM already Inited \n");
            //IARM already inited
            return;
        }
        else
        {
            MW_PRE_LOGGER_LOG("m_use_firebolt_sdk or IsContainerEnvironment() has changed, init again \n");
            //m_use_firebolt_sdk has changed init again
        }
    }
    else
    {
        MW_PRE_LOGGER_LOG("Initializing \n");
    }
    //remove-end
    
    if(m_pDeviceInterfaceBase)
    {
        m_pDeviceInterfaceBase = nullptr;
    }

    MW_PRE_LOGGER_LOG("m_use_firebolt_sdk : %d, IsContainerEnvironment() : %d \n", m_use_firebolt_sdk, IsContainerEnvironment());

    //remove-start
    if(m_use_firebolt_sdk || IsContainerEnvironment()) //if explicitly config'd to or if in container go for firebolt
    {
    //remove-end
        MW_PRE_LOGGER_LOG("Using Firebolt \n");
        m_pDeviceInterfaceBase = DeviceFireboltInterface::GetInstance();
        DeviceFireboltInterface::Initialize();
    //remove-start
        m_initialized = PlayerExternalsRdkInterface::InitState::FIREBOLT;
    }
    else
    {
        MW_PRE_LOGGER_LOG("Using IARM \n");
        m_pDeviceInterfaceBase = DeviceIARMInterface::GetInstance();
        DeviceIARMInterface::Initialize();
        m_initialized = PlayerExternalsRdkInterface::InitState::IARM;
    }
    //remove-end

    MW_PRE_LOGGER_LOG("Done getting interface \n");

    SetHDMIStatus();

    MW_PRE_LOGGER_LOG("Initializing completed \n");
}

PlayerExternalsRdkInterface::~PlayerExternalsRdkInterface()
{
    m_pDeviceInterfaceBase = nullptr;
    s_pPlayerIarmRdkOP = nullptr;
}

void PlayerExternalsRdkInterface::GetDisplayResolution(int &width, int &height)
{
    width   = m_displayWidth;
    height  = m_displayHeight;
}

void PlayerExternalsRdkInterface::SetResolution(int width, int height)
{
    MW_LOG_WARN(" Resolution : width %d height:%d\n",width,height);
    m_displayWidth   = width;
    m_displayHeight  = height;
}

/**
 * @brief Set the HDCP status using data from DeviceSettings
 */
void PlayerExternalsRdkInterface::SetHDMIStatus()
{
    bool                    isConnected              = false;
    bool                    isHDCPCompliant          = false;
    bool                    isHDCPEnabled            = true;
    dsHdcpProtocolVersion_t hdcpProtocol             = dsHDCP_VERSION_MAX;
    dsHdcpProtocolVersion_t hdcpReceiverProtocol     = dsHDCP_VERSION_MAX;
    dsHdcpProtocolVersion_t hdcpCurrentProtocol      = dsHDCP_VERSION_MAX;




    try {
        //Get the HDMI port
	device::Manager::Initialize();
        std::string strVideoPort = device::Host::getInstance().getDefaultVideoPortName();
        ::device::VideoOutputPort &vPort = ::device::Host::getInstance().getVideoOutputPort(strVideoPort);
        isConnected        = vPort.isDisplayConnected();
        hdcpProtocol       = (dsHdcpProtocolVersion_t)vPort.getHDCPProtocol();
        if(isConnected) {
            isHDCPCompliant          = (vPort.getHDCPStatus() == dsHDCP_STATUS_AUTHENTICATED);
            isHDCPEnabled            = vPort.isContentProtected();
            hdcpReceiverProtocol     = (dsHdcpProtocolVersion_t)vPort.getHDCPReceiverProtocol();
            hdcpCurrentProtocol      = (dsHdcpProtocolVersion_t)vPort.getHDCPCurrentProtocol();
            //get the resolution of the TV
            int width,height;
            int iResID = vPort.getResolution().getPixelResolution().getId();
            if( device::PixelResolution::k720x480 == iResID )
            {
                width =  720;
                height = 480;
            }
            else if(  device::PixelResolution::k720x576 == iResID )
            {
                width = 720;
                height = 576;
            }
            else if(  device::PixelResolution::k1280x720 == iResID )
            {
                width =  1280;
                height = 720;
            }
            else if(  device::PixelResolution::k1920x1080 == iResID )
            {
                width =  1920;
                height = 1080;
            }
            else if(  device::PixelResolution::k3840x2160 == iResID )
            {
                width =  3840;
                height = 2160;
            }
            else if(  device::PixelResolution::k4096x2160 == iResID )
            {
                width =  4096;
                height = 2160;
            }
            else
            {
                width =  DISPLAY_WIDTH_UNKNOWN;
                height = DISPLAY_HEIGHT_UNKNOWN;
                std::string _res = vPort.getResolution().getName();
                MW_LOG_ERR(" ERR parse failed for getResolution().getName():%s id:%d\n",(_res.empty() ? "NULL" : _res.c_str()),iResID);
            }

            SetResolution(width, height);
        }
        else {
            isHDCPCompliant = false;
            isHDCPEnabled = false;
            SetResolution(DISPLAY_RESOLUTION_NA,DISPLAY_RESOLUTION_NA);
        }

	device::Manager::DeInitialize();
    }
    catch (...) {
        MW_LOG_WARN("DeviceSettings exception caught\n");
    }

    m_isHDCPEnabled = isHDCPEnabled;

    if(m_isHDCPEnabled) {
        if(hdcpCurrentProtocol == dsHDCP_VERSION_2X) {
            m_hdcpCurrentProtocol = hdcpCurrentProtocol;
        }
        else {
            m_hdcpCurrentProtocol = dsHDCP_VERSION_1X;
        }
        MW_LOG_WARN(" detected HDCP version %s\n", m_hdcpCurrentProtocol == dsHDCP_VERSION_2X ? "2.x" : "1.4");
    }
    else {
        MW_LOG_WARN("DeviceSettings HDCP is not enabled\n");
    }

    if(!isConnected) {
        m_hdcpCurrentProtocol = dsHDCP_VERSION_1X;
        MW_LOG_WARN(" GetHDCPVersion: Did not detect HDCP version defaulting to 1.4 (%d)\n", m_hdcpCurrentProtocol);
    }


    return;
}

void PlayerExternalsRdkInterface::setHdcpProtocol(dsHdcpProtocolVersion_t t_protocol)
{
    m_hdcpCurrentProtocol = t_protocol;
    MW_LOG_WARN(" detected HDCP version %s\n", m_hdcpCurrentProtocol == dsHDCP_VERSION_2X ? "2.x" : "1.4");
}

std::shared_ptr<DeviceInterfaceBase> PlayerExternalsRdkInterface::GetDeviceInterface()
{
    return m_pDeviceInterfaceBase;
}

bool PlayerExternalsRdkInterface::GetActiveInterface()
{
    return isInterfaceWifi;
}

void PlayerExternalsRdkInterface::SetActiveInterface(bool isWifi)
{
    isInterfaceWifi = isWifi;
}

char * PlayerExternalsRdkInterface::GetTR181Config(const char * paramName, size_t & iConfigLen)
{
    return m_pDeviceInterfaceBase->GetTR181Config(paramName, iConfigLen);
}

void PlayerExternalsRdkInterface::SetUseFireBoltSDK(bool t_use_firebolt_sdk)
{
    MW_PRE_LOGGER_LOG("old : %d, new : %d \n", m_use_firebolt_sdk, t_use_firebolt_sdk);
    if(m_use_firebolt_sdk != t_use_firebolt_sdk)
    {
        m_use_firebolt_sdk = t_use_firebolt_sdk;
        //reinitialize
        m_initialized = InitState::NOT_INITIALIZED;
        Initialize();

    }
    
}
