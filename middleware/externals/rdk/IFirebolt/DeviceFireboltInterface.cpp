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
	RegisterDsMgrEventHandler();
	RegisterNtwMgrEventHandler();
}

DeviceFireboltInterface::~DeviceFireboltInterface()
{
	RemoveEventHandlers();
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
		MW_LOG_INFO("HDCP changed event registerd");
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
		MW_LOG_INFO("Resolution changed event registerd");
        mDsMgrSubscriptionId.push_back(result.value());
	}
	else
	{
		MW_LOG_ERR("Failed to get video resolution %d ",  static_cast<int>(result.error()));
	}

}

void DeviceFireboltInterface::RemoveEventHandlers()
{
	//removes everything ...
    Firebolt::IFireboltAampAccessor::Instance().DeviceInterface().unsubscribeAll();        
}

void DeviceFireboltInterface::RegisterNtwMgrEventHandler()
{
	MW_LOG_WARN("Subscribing to Firebolt Network change event ");

	auto result =  Firebolt::IFireboltAampAccessor::Instance().DeviceInterface().subscribeOnNetworkChanged(
					[](const auto& network) {
						MW_LOG_ERR("[Event] network changed");
					    getActiveInterfaceEventHandlerFirebolt(network);
					});
	
	if(result)
	{
		MW_LOG_INFO("Network changed event registerd");
		mNtwMgrSubscriptionId.push_back(result.value());
	}
	else
	{
		MW_LOG_ERR("Failed to subscribe to network change events: %d", static_cast<int>(result.error()));
	}

	std::shared_ptr<PlayerExternalsRdkInterface> pInstance = PlayerExternalsRdkInterface::GetPlayerExternalsRdkInterfaceInstance();

	auto network = Firebolt::IFireboltAampAccessor::Instance().DeviceInterface().network();

	if(network)
	{
		if(network.value().type == Firebolt::Device::NetworkType::WIFI)
		{
			pInstance->SetActiveInterface(true);
		}
		else
		{
			pInstance->SetActiveInterface(false);
		}
	}

}

char * DeviceFireboltInterface::GetTR181Config(const char * paramName, size_t & iConfigLen)
{
	MW_LOG_ERR("TR181 not supported for firebolt");
    return nullptr;
}

static void getActiveInterfaceEventHandlerFirebolt (const Firebolt::Device::NetworkInfoResult& t_NetworkInfoResult)
{
    std::shared_ptr<PlayerExternalsRdkInterface> pInstance = PlayerExternalsRdkInterface::GetPlayerExternalsRdkInterfaceInstance();

	if(t_NetworkInfoResult.state == Firebolt::Device::NetworkState::CONNECTED)
	{
		std::string interface = "unknown";
		if(t_NetworkInfoResult.type == Firebolt::Device::NetworkType::WIFI)
		{
			interface = "wlan";
			pInstance->SetActiveInterface(true);
			MW_LOG_INFO("Network interface changed to wifi");
		}
		else if(t_NetworkInfoResult.type == Firebolt::Device::NetworkType::ETHERNET)
		{
			interface = "eth";
			pInstance->SetActiveInterface(false);
			MW_LOG_INFO("Network interface changed to ethernet");
		}
		else
		{
			MW_LOG_ERR("Unsupported Interface %d", (int)t_NetworkInfoResult.type);
		}
		MW_LOG_INFO("getActiveInterfaceEventHandler activeinterface changed to %s\n", interface.c_str());
	}
	else
	{
		MW_LOG_ERR("Disconnected interface type:%d state:%d\n", (int)t_NetworkInfoResult.type, (int)t_NetworkInfoResult.state);
	}
    
	
}

/**
 * @brief IARM event handler for HDCP and HDMI hot plug events
 */
static void HDCPEventHandlerFirebolt(const Firebolt::Device::HDCPVersionMap& t_HDCPVersionMap)
{
    std::shared_ptr<PlayerExternalsRdkInterface> pInstance = PlayerExternalsRdkInterface::GetPlayerExternalsRdkInterfaceInstance();

    if(t_HDCPVersionMap.hdcp2_2)
	{
		pInstance->setHdcpProtocol(dsHDCP_VERSION_2X);
		MW_LOG_INFO("HDCP protocol updated 2_2");
	}
	else if(t_HDCPVersionMap.hdcp1_4)
	{
		pInstance->setHdcpProtocol(dsHDCP_VERSION_1X);
		MW_LOG_INFO("HDCP protocol updated 1_4");
	}
	else
	{
		MW_LOG_ERR("Unknown HDCP protocol");
	}

	pInstance->SetHDMIStatus();
            
}

/**
 * @brief IARM event handler for resolution changes
 */
static void ResolutionHandlerFirebolt(const std::string& t_res)
{
    int width = 1280;
	int height = 720;

	MW_LOG_INFO("Resolution: %s", t_res.c_str());

	auto curr_network = Firebolt::IFireboltAampAccessor::Instance().DeviceInterface().videoResolution();

	if(curr_network)
	{
		std::shared_ptr<PlayerExternalsRdkInterface> pInstance = PlayerExternalsRdkInterface::GetPlayerExternalsRdkInterfaceInstance();
		width = curr_network.value()[0];
		height = curr_network.value()[1];
		pInstance->SetResolution(width, height);
		MW_LOG_INFO("Updating resolution [%d][%d]", curr_network.value()[0], curr_network.value()[1]);
	}
	else
	{
		MW_LOG_ERR("Failed to get current resolution");
	}

}
