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
	RemoveEventHandler();
	m_pFireboltInterface = nullptr;
}


void DeviceFireboltInterface::RegisterDsMgrEventHandler()
{
       
	MW_LOG_WARN("Subscribing to Firebolt hdcp change event ");

	auto result =  Firebolt::IFireboltAampAccessor::Instance().DeviceInterface().subscribeOnHdcpChanged(
					[](const auto& network) {
						MW_LOG_ERR("hdcp changed");
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
						if (auto videoResolution = Firebolt::IFireboltAampAccessor::Instance().DeviceInterface().videoResolution())
						{
							// MW_LOG_WARN("Device video resolution is: " << videoResolution.value()[0],videoResolution.value()[1]);
						}

						//Firebolt::Device::Resolution printVideoResolution = *videoResolution;

						// MW_LOG_ERR("resolution changed");

						//MW_LOG_INFO("Device video resolution is: %d, %d" , printVideoResolution.value()[0] , printVideoResolution.value()[1]);

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
    MW_LOG_WARN("Subscribing to Firebolt Network change event ");

	auto result =  Firebolt::IFireboltAampAccessor::Instance().DeviceInterface().subscribeOnNetworkChanged(
					[](const auto& network) {
						MW_LOG_ERR("network changed");
					    MW_LOG_ERR("CAHNGED %d %d", static_cast<int>(network.state), static_cast<int>(network.type));
						std::cout << "[Subscription] Network changed" << std::endl; 
					});
	
	if(result)
	{
		mNtwMgrSubscriptionId.push_back(result.value());
	}
	else
	{
		MW_LOG_ERR("Failed to subscribe to network change events: %d", static_cast<int>(result.error()));
	}

}

char * DeviceFireboltInterface::GetTR181Config(const char * paramName, size_t & iConfigLen)
{
    return nullptr;
}
