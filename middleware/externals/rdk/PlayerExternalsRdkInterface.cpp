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

#include <hostIf_tr69ReqHandler.h>
#include "tr181api.h"
#include "_base64.h"

#define DISPLAY_WIDTH_UNKNOWN       -1  /**< Parsing failed for getResolution().getName(); */
#define DISPLAY_HEIGHT_UNKNOWN      -1  /**< Parsing failed for getResolution().getName(); */
#define DISPLAY_RESOLUTION_NA        0  /**< Resolution not available yet or not connected to HDMI */

/**
 * @brief Enumeration for net_srv_mgr active interface event callback
 */
typedef enum _NetworkManager_EventId_t {
	IARM_BUS_NETWORK_MANAGER_EVENT_SET_INTERFACE_ENABLED=50,
	IARM_BUS_NETWORK_MANAGER_EVENT_INTERFACE_IPADDRESS=55,
	IARM_BUS_NETWORK_MANAGER_MAX
} IARM_Bus_NetworkManager_EventId_t;

/**
 * @struct _IARM_BUS_NetSrvMgr_Iface_EventData_t
 * @brief IARM Bus struct contains active streaming interface, original definition present in homenetworkingservice.h
 */
typedef struct _IARM_BUS_NetSrvMgr_Iface_EventData_t {
	union{
		char activeIface[10];
		char allNetworkInterfaces[50];
		char enableInterface[10];
	};
	char interfaceCount;
	bool isInterfaceEnabled;
} IARM_BUS_NetSrvMgr_Iface_EventData_t;

PlayerExternalsRdkInterface* s_pPlayerIarmRdkOP = NULL;

static bool isInterfaceWifi = false;

static void HDMIEventHandler(const char *owner, IARM_EventId_t eventId, void *data, size_t len);
static void ResolutionHandler(const char *owner, IARM_EventId_t eventId, void *data, size_t len);
static void getActiveInterfaceEventHandler (const char *owner, IARM_EventId_t eventId, void *data, size_t len);

/**
 * @brief Singleton for object creation
 */
PlayerExternalsRdkInterface * PlayerExternalsRdkInterface::GetPlayerExternalsRdkInterfaceInstance()
{
    if(s_pPlayerIarmRdkOP == NULL) {
        s_pPlayerIarmRdkOP = new PlayerExternalsRdkInterface();
    }

    return s_pPlayerIarmRdkOP;
}

void PlayerExternalsRdkInterface::IARMInit(const char* processName)
{
    //char processName[20] = {0};
    IARM_Result_t result;
    //snprintf(processName, sizeof(processName), "PLAYER-%u", getpid());
    if (IARM_RESULT_SUCCESS == (result = IARM_Bus_Init(processName))) {
            printf("IARM Interface Inited in Player\n");
    }
    else {
            printf("IARM Interface Inited Externally : %d\n", result);
    }

    if (IARM_RESULT_SUCCESS == (result = IARM_Bus_Connect())) {
            printf("IARM Interface Connected in Player\n");
    }
    else {
            printf("IARM Interface Connected Externally :%d\n", result);
    }
}

void PlayerExternalsRdkInterface::IARMRegisterDsMgrEventHandler()
{
    IARM_Bus_RegisterEventHandler(IARM_BUS_DSMGR_NAME,IARM_BUS_DSMGR_EVENT_HDMI_HOTPLUG, HDMIEventHandler);
    IARM_Bus_RegisterEventHandler(IARM_BUS_DSMGR_NAME,IARM_BUS_DSMGR_EVENT_HDCP_STATUS, HDMIEventHandler);
    IARM_Bus_RegisterEventHandler(IARM_BUS_DSMGR_NAME,IARM_BUS_DSMGR_EVENT_RES_POSTCHANGE, ResolutionHandler);
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

/**
 * @brief IARM event handler for HDCP and HDMI hot plug events
 */
static void HDMIEventHandler(const char *owner, IARM_EventId_t eventId, void *data, size_t len)
{
    PlayerExternalsRdkInterface *pInstance = PlayerExternalsRdkInterface::GetPlayerExternalsRdkInterfaceInstance();

    switch (eventId) {
        case IARM_BUS_DSMGR_EVENT_HDMI_HOTPLUG :
        {
            IARM_Bus_DSMgr_EventData_t *eventData = (IARM_Bus_DSMgr_EventData_t *)data;
            int hdmi_hotplug_event = eventData->data.hdmi_hpd.event;

            const char *hdmihotplug = (hdmi_hotplug_event == dsDISPLAY_EVENT_CONNECTED) ? "connected" : "disconnected";
            MW_LOG_WARN(" Received IARM_BUS_DSMGR_EVENT_HDMI_HOTPLUG  event data:%d status: %s\n",
                       hdmi_hotplug_event, hdmihotplug);

            pInstance->SetHDMIStatus();

            break;
        }
        case IARM_BUS_DSMGR_EVENT_HDCP_STATUS :
        {
            IARM_Bus_DSMgr_EventData_t *eventData = (IARM_Bus_DSMgr_EventData_t *)data;
            int hdcpStatus = eventData->data.hdmi_hdcp.hdcpStatus;
            const char *hdcpStatusStr = (hdcpStatus == dsHDCP_STATUS_AUTHENTICATED) ? "authenticated" : "authentication failure";
            MW_LOG_WARN(" Received IARM_BUS_DSMGR_EVENT_HDCP_STATUS  event data:%d status:%s\n",
                      hdcpStatus, hdcpStatusStr);

            pInstance->SetHDMIStatus();
            break;
        }
        default:
            MW_LOG_WARN(" Received unknown IARM bus event:%d\n", eventId);
            break;
    }
}

/**
 * @brief IARM event handler for resolution changes
 */
static void ResolutionHandler(const char *owner, IARM_EventId_t eventId, void *data, size_t len)
{
    PlayerExternalsRdkInterface *pInstance = PlayerExternalsRdkInterface::GetPlayerExternalsRdkInterfaceInstance();

    switch (eventId) {
        case IARM_BUS_DSMGR_EVENT_RES_PRECHANGE:
            MW_LOG_WARN(" Received IARM_BUS_DSMGR_EVENT_RES_PRECHANGE \n");
            break;
        case IARM_BUS_DSMGR_EVENT_RES_POSTCHANGE:
        {
            int width = 1280;
            int height = 720;

            IARM_Bus_DSMgr_EventData_t *eventData = (IARM_Bus_DSMgr_EventData_t *)data;
            width   = eventData->data.resn.width ;
            height  = eventData->data.resn.height ;

            MW_LOG_WARN(" Received IARM_BUS_DSMGR_EVENT_RES_POSTCHANGE event width : %d height : %d\n", width, height);
            pInstance->SetResolution(width, height);
            break;
        }
        default:
            MW_LOG_WARN(" Received unknown resolution event %d\n", eventId);
            break;
    }
}

void PlayerExternalsRdkInterface::IARMRemoveDsMgrEventHandler()
{
    IARM_Bus_RemoveEventHandler(IARM_BUS_DSMGR_NAME,IARM_BUS_DSMGR_EVENT_HDMI_HOTPLUG, HDMIEventHandler);
    IARM_Bus_RemoveEventHandler(IARM_BUS_DSMGR_NAME,IARM_BUS_DSMGR_EVENT_HDCP_STATUS, HDMIEventHandler);
    IARM_Bus_RemoveEventHandler(IARM_BUS_DSMGR_NAME,IARM_BUS_DSMGR_EVENT_RES_POSTCHANGE, ResolutionHandler);
}

/**
 * @brief Active streaming interface is wifi
 *
 * @return bool - true if wifi interface connected
 */
bool PlayerExternalsRdkInterface::IsActiveStreamingInterfaceWifi(void)
{
        bool wifiStatus = false;
        IARM_Result_t ret = IARM_RESULT_SUCCESS;
        IARM_BUS_NetSrvMgr_Iface_EventData_t param;

        ret = IARM_Bus_Call("NET_SRV_MGR", "getActiveInterface", (void*)&param, sizeof(param));
        if (ret != IARM_RESULT_SUCCESS) {
                MW_LOG_ERR("NET_SRV_MGR getActiveInterface read failed : %d\n", ret);
        }
        else
        {
                MW_LOG_WARN("NET_SRV_MGR getActiveInterface = %s\n", param.activeIface);
                if (!strcmp(param.activeIface, "WIFI")){
                        wifiStatus = true;
                }
        }
        IARM_Bus_RegisterEventHandler("NET_SRV_MGR", IARM_BUS_NETWORK_MANAGER_EVENT_INTERFACE_IPADDRESS, getActiveInterfaceEventHandler);
		isInterfaceWifi = wifiStatus;
        return wifiStatus;
}

bool PlayerExternalsRdkInterface::GetActiveInterface()
{
    return isInterfaceWifi;
}

bool PlayerExternalsRdkInterface::IsLiveLatencyCorrectionSupported()
{
    bool IsMS12V2 = true;
    try {
		//Get the HDMI port
		device::Manager::Initialize();
		std::string strVideoPort = device::Host::getInstance().getDefaultVideoPortName();
		::device::VideoOutputPort &vPort = ::device::Host::getInstance().getVideoOutputPort(strVideoPort);
		int caps;
		vPort.getAudioOutputPort().getAudioCapabilities(&caps);
		if(((caps & dsAUDIOSUPPORT_MS12V2) != dsAUDIOSUPPORT_MS12V2))
		{
			IsMS12V2 = false;
			MW_LOG_INFO("MS12V2 Audio not supported in this device\n");
		}
		device::Manager::DeInitialize();
	}
	catch (...) {
		MW_LOG_WARN("DeviceSettings exception caught\n");
	}
	return IsMS12V2 ;
}

static void getActiveInterfaceEventHandler (const char *owner, IARM_EventId_t eventId, void *data, size_t len)
{
	static char previousInterface[20] = {'\0'};


	if (strcmp (owner, "NET_SRV_MGR") != 0)
		return;

	IARM_BUS_NetSrvMgr_Iface_EventData_t *param = (IARM_BUS_NetSrvMgr_Iface_EventData_t *) data;

	if (NULL == strstr (param->activeIface, previousInterface) || (strlen(previousInterface) == 0))
	{
		memset(previousInterface, 0, sizeof(previousInterface));
		strncpy(previousInterface, param->activeIface, sizeof(previousInterface) - 1);
		MW_LOG_WARN("getActiveInterfaceEventHandler EventId %d activeinterface %s\n", eventId,  param->activeIface);
	}

	if (NULL != strstr (param->activeIface, "wlan"))
	{
		isInterfaceWifi = true;
	}
	else if (NULL != strstr (param->activeIface, "eth"))
	{
		isInterfaceWifi = false;
	}


}

char * PlayerExternalsRdkInterface::GetTR181Config(const char * paramName, size_t & iConfigLen)
{

	char *  strConfig = NULL;
	IARM_Result_t result;
	HOSTIF_MsgData_t param;
	memset(&param,0,sizeof(param));
	snprintf(param.paramName,TR69HOSTIFMGR_MAX_PARAM_LEN,"%s",paramName);
	param.reqType = HOSTIF_GET;

	result = IARM_Bus_Call(IARM_BUS_TR69HOSTIFMGR_NAME,IARM_BUS_TR69HOSTIFMGR_API_GetParams,
                    (void *)&param,	sizeof(param));
	if(result  == IARM_RESULT_SUCCESS)
	{
		if(fcNoFault == param.faultCode)
		{
			if(param.paramtype == hostIf_StringType && param.paramLen > 0 )
			{
				std::string strforLog(param.paramValue,param.paramLen);

				iConfigLen = param.paramLen;
				const char *src = (const char*)(param.paramValue);
				strConfig = (char * ) base64_Decode(src,&iConfigLen);

				MW_LOG_INFO("GetTR181PlayerConfig: Got:%s En-Len:%d Dec-len:%d\n",strforLog.c_str(),param.paramLen,iConfigLen);
			}
			else
			{
				MW_LOG_ERR("GetTR181PlayerConfig: Not a string param type=%d or Invalid len:%d \n",param.paramtype, param.paramLen);
			}
		}
	}
	else
	{
		MW_LOG_ERR("GetTR181PlayerConfig: Failed to retrieve value result=%d\n",result);
	}
	return strConfig;

}
