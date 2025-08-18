#include "DeviceFireboltInterface.h"
#include "fireboltaamp.h"
#include "PlayerLogManager.h"
#include "PlayerExternalsRdkInterface.h"

#include <cstring>
#include <cstdio>
#include <mutex>
#include <chrono>
#include <condition_variable>

std::shared_ptr<DeviceFireboltInterface> s_pDeviceFireboltInterface = nullptr;

std::mutex mFireboltConnectionMutex;
std::condition_variable mFireboltConnectionCV;

static void HDCPEventHandlerFirebolt(const Firebolt::Device::HDCPVersionMap& t_HDCPVersionMap);
static void ResolutionHandlerFirebolt(const std::string& t_res);
static void getActiveInterfaceEventHandlerFirebolt (const Firebolt::Device::NetworkInfoResult& t_NetworkInfoResult);

std::shared_ptr<DeviceFireboltInterface> DeviceFireboltInterface::GetInstance()
{
    if(nullptr == s_pDeviceFireboltInterface)
    {
        s_pDeviceFireboltInterface = std::shared_ptr<DeviceFireboltInterface>(new DeviceFireboltInterface());
    }

    return s_pDeviceFireboltInterface;
}

DeviceFireboltInterface::DeviceFireboltInterface()
{
    m_pFireboltInterface = FireboltInterface::GetInstance();
}

DeviceFireboltInterface::~DeviceFireboltInterface()
{
	RegisterDsMgrEventHandler();
	m_pFireboltInterface = nullptr;
}


void DeviceFireboltInterface::RegisterDsMgrEventHandler()
{
       
	MW_LOG_WARN("Subscribing to Firebolt hdcp change event ");

	auto result =  Firebolt::IFireboltAampAccessor::Instance().DeviceInterface().subscribeOnHdcpChanged(
					[](const auto& hdcpProtocol) {
						MW_LOG_ERR("[Event] HDCP changed");
						HDCPEventHandlerFirebolt(hdcpProtocol);
					});

	if(result)
	{
		mDsMgrSubscriptionId.push_back(result.value());
	}

	else
	{
		MW_LOG_ERR("Failed to subscribe to hdcp change events: %d", static_cast<int>(result.error()));
	}

	MW_LOG_WARN("Subscribing to Firebolt resolution change event ");

	result = Firebolt::IFireboltAampAccessor::Instance().DeviceInterface().subscribeOnVideoResolutionChanged(
					[](const std::string& videoResolution) 
					{
						MW_LOG_WARN("[Event] Video resolution changed: %s" , videoResolution.c_str());
						ResolutionHandlerFirebolt(videoResolution);
					});
	if(result)
	{
		MW_LOG_ERR("resolution changed");
        mDsMgrSubscriptionId.push_back(result.value());
	}
	else
	{
		MW_LOG_ERR("Failed to get video resolution %d ",  static_cast<int>(result.error()));
	}

}

void DeviceFireboltInterface::RemoveDsMgrEventHandler()
{
	//removes everything ...
    Firebolt::IFireboltAampAccessor::Instance().DeviceInterface().unsubscribeAll();        
}

bool DeviceFireboltInterface::IsActiveStreamingInterfaceWifi()
{
	bool bRet = false;
    MW_LOG_WARN("Subscribing to Firebolt Network change event ");

	auto result =  Firebolt::IFireboltAampAccessor::Instance().DeviceInterface().subscribeOnNetworkChanged(
					[](const auto& network) {
						MW_LOG_ERR("[Event] network changed");
					    getActiveInterfaceEventHandlerFirebolt(network);
					});
	
	if(result)
	{
		mNtwMgrSubscriptionId.push_back(result.value());
	}
	else
	{
		MW_LOG_ERR("Failed to subscribe to network change events: %d", static_cast<int>(result.error()));
	}

	auto curr_network = Firebolt::IFireboltAampAccessor::Instance().DeviceInterface().network();

	if(curr_network)
	{
		if(curr_network.value().type == Firebolt::Device::NetworkType::WIFI)
		{
			bRet = true;
		}
	}
	else
	{
		MW_LOG_ERR("Failed to get current interface");
	}
	

	return bRet;

}

char * DeviceFireboltInterface::GetTR181Config(const char * paramName, size_t & iConfigLen)
{
    return nullptr;
}

static void getActiveInterfaceEventHandlerFirebolt (const Firebolt::Device::NetworkInfoResult& t_NetworkInfoResult)
{
    PlayerExternalsRdkInterface *pInstance = PlayerExternalsRdkInterface::GetPlayerExternalsRdkInterfaceInstance();

	if(t_NetworkInfoResult.state == Firebolt::Device::NetworkState::CONNECTED)
	{
		std::string interface = "unknown";
		if(t_NetworkInfoResult.type == Firebolt::Device::NetworkType::WIFI)
		{
			interface = "wlan";
			pInstance->SetActiveInterface(true);
		}
		else if(t_NetworkInfoResult.type == Firebolt::Device::NetworkType::ETHERNET)
		{
			interface = "eth";
			pInstance->SetActiveInterface(false);
		}
		else
		{
			MW_LOG_ERR("Unsupported Interface %d", (int)t_NetworkInfoResult.type);
		}
		MW_LOG_WARN("getActiveInterfaceEventHandler activeinterface changed to %s\n", interface.c_str());
	}
	else
	{
		MW_LOG_WARN("getActiveInterfaceEventHandler interface type:%d state:%d\n", (int)t_NetworkInfoResult.type, (int)t_NetworkInfoResult.state);
	}
    
	
}

/**
 * @brief IARM event handler for HDCP and HDMI hot plug events
 */
static void HDCPEventHandlerFirebolt(const Firebolt::Device::HDCPVersionMap& t_HDCPVersionMap)
{
    PlayerExternalsRdkInterface *pInstance = PlayerExternalsRdkInterface::GetPlayerExternalsRdkInterfaceInstance();

    if(t_HDCPVersionMap.hdcp2_2)
	{
		pInstance->setHdcpProtocol(dsHDCP_VERSION_2X);
	}
	else if(t_HDCPVersionMap.hdcp1_4)
	{
		pInstance->setHdcpProtocol(dsHDCP_VERSION_1X);
	}
	else
	{
		MW_LOG_ERR("Unknown HDCP protocol");
	}
            
}

/**
 * @brief IARM event handler for resolution changes
 */
static void ResolutionHandlerFirebolt(const std::string& t_res)
{
    PlayerExternalsRdkInterface *pInstance = PlayerExternalsRdkInterface::GetPlayerExternalsRdkInterfaceInstance();
	int width = 1280;
	int height = 720;

	MW_LOG_WARN("Resolution: %s", t_res.c_str());

	pInstance->SetResolution(width, height);

}
