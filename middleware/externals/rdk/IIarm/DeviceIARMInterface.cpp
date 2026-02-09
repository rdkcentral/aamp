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
 * @file DeviceIARMInterface.cpp
 * @brief IARM interface
 */

/*
IARM Deprecation Note:
IARM is to be deprecated in favor of DeviceSettings and Firebolt Device API.
*/
/*
Remove the entire folder externals/rdk/IARM
*/


#include "DeviceIARMInterface.h"

#include <cstring>
#include <cstdio>
#include <libIARM.h>
#include <libIBus.h>
#include <iarmUtil.h>
#include "libIBusDaemon.h"
#include <hostIf_tr69ReqHandler.h>
#include "tr181api.h"
#include "_base64.h"

#include "PlayerLogManager.h"

#include "PlayerExternalsRdkInterface.h"

#include "PlayerExternalUtils.h"

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

std::shared_ptr<DeviceIARMInterface> s_pDeviceIARMInterface = nullptr;

static void HDMIEventHandler(const char *owner, IARM_EventId_t eventId, void *data, size_t len);
static void ResolutionHandler(const char *owner, IARM_EventId_t eventId, void *data, size_t len);
static void getActiveInterfaceEventHandler (const char *owner, IARM_EventId_t eventId, void *data, size_t len);


std::shared_ptr<DeviceIARMInterface> DeviceIARMInterface::GetInstance()
{
    if(nullptr == s_pDeviceIARMInterface)
    {
        s_pDeviceIARMInterface = std::shared_ptr<DeviceIARMInterface>(new DeviceIARMInterface());
    }

    return s_pDeviceIARMInterface;
}

DeviceIARMInterface::DeviceIARMInterface()
{

    DeviceIARMInterface::IARMInit();
    
    
}

DeviceIARMInterface::~DeviceIARMInterface()
{
    MW_PRE_LOGGER_LOG("DeviceIARMInterface destructor called \n");

    RemoveEventHandlers();

    s_pDeviceIARMInterface = nullptr;
}

void DeviceIARMInterface::Initialize()
{
    if(s_pDeviceIARMInterface)
    {
        s_pDeviceIARMInterface->RegisterDsMgrEventHandler();
        s_pDeviceIARMInterface->RegisterNtwMgrEventHandler();
    }
    
}

void DeviceIARMInterface::IARMInit()
{
    //char processName[20] = {0};
    IARM_Result_t result;
    MW_PRE_LOGGER_LOG("IARM Interface Init started in Player\n");

    //snprintf(processName, sizeof(processName), "PLAYER-%u", getpid());
    if (IARM_RESULT_SUCCESS == (result = IARM_Bus_Init("PLAYER"))) {
            MW_PRE_LOGGER_LOG("IARM Interface Inited in Player\n");
    }
    else {
            MW_PRE_LOGGER_LOG("IARM Interface Inited Externally : %d\n", result);
    }

    if (IARM_RESULT_SUCCESS == (result = IARM_Bus_Connect())) {
            MW_PRE_LOGGER_LOG("IARM Interface Connected in Player\n");
    }
    else {
            MW_PRE_LOGGER_LOG("IARM Interface Connected Externally :%d\n", result);
    }

    MW_PRE_LOGGER_LOG("IARM Interface Init completed in Player\n");

}

void DeviceIARMInterface::RegisterDsMgrEventHandler()
{
    IARM_Bus_RegisterEventHandler(IARM_BUS_DSMGR_NAME,IARM_BUS_DSMGR_EVENT_HDMI_HOTPLUG, HDMIEventHandler);
    IARM_Bus_RegisterEventHandler(IARM_BUS_DSMGR_NAME,IARM_BUS_DSMGR_EVENT_HDCP_STATUS, HDMIEventHandler);
    IARM_Bus_RegisterEventHandler(IARM_BUS_DSMGR_NAME,IARM_BUS_DSMGR_EVENT_RES_POSTCHANGE, ResolutionHandler);
}

void DeviceIARMInterface::RemoveEventHandlers()
{
    IARM_Bus_RemoveEventHandler(IARM_BUS_DSMGR_NAME,IARM_BUS_DSMGR_EVENT_HDMI_HOTPLUG, HDMIEventHandler);
    IARM_Bus_RemoveEventHandler(IARM_BUS_DSMGR_NAME,IARM_BUS_DSMGR_EVENT_HDCP_STATUS, HDMIEventHandler);
    IARM_Bus_RemoveEventHandler(IARM_BUS_DSMGR_NAME,IARM_BUS_DSMGR_EVENT_RES_POSTCHANGE, ResolutionHandler);
    IARM_Bus_RemoveEventHandler("NET_SRV_MGR", IARM_BUS_NETWORK_MANAGER_EVENT_INTERFACE_IPADDRESS, getActiveInterfaceEventHandler);
}

void DeviceIARMInterface::RegisterNtwMgrEventHandler()
{
    std::shared_ptr<PlayerExternalsRdkInterface> pInstance = PlayerExternalsRdkInterface::GetPlayerExternalsRdkInterfaceInstance();

    bool wifiStatus = false;
    IARM_Result_t ret = IARM_RESULT_SUCCESS;
    IARM_BUS_NetSrvMgr_Iface_EventData_t param;

    ret = IARM_Bus_Call("NET_SRV_MGR", "getActiveInterface", (void*)&param, sizeof(param));
    if (ret != IARM_RESULT_SUCCESS)
    {
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
    pInstance->SetActiveInterface(wifiStatus);
}

char * DeviceIARMInterface::GetTR181Config(const char * paramName, size_t & iConfigLen)
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
				strConfig = (char * ) base64_Decode(src,&iConfigLen,strlen(src));

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

static void getActiveInterfaceEventHandler (const char *owner, IARM_EventId_t eventId, void *data, size_t len)
{
    std::shared_ptr<PlayerExternalsRdkInterface> pInstance = PlayerExternalsRdkInterface::GetPlayerExternalsRdkInterfaceInstance();

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
		pInstance->SetActiveInterface(true);
	}
	else if (NULL != strstr (param->activeIface, "eth"))
	{
		pInstance->SetActiveInterface(false);
	}
    
	
}

/**
 * @brief IARM event handler for HDCP and HDMI hot plug events
 */
static void HDMIEventHandler(const char *owner, IARM_EventId_t eventId, void *data, size_t len)
{
    std::shared_ptr<PlayerExternalsRdkInterface> pInstance = PlayerExternalsRdkInterface::GetPlayerExternalsRdkInterfaceInstance();

    switch (eventId)
    {
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
    std::shared_ptr<PlayerExternalsRdkInterface> pInstance = PlayerExternalsRdkInterface::GetPlayerExternalsRdkInterfaceInstance();

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
