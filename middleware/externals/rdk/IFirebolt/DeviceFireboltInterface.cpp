#include "DeviceFireboltInterface.h"

#include <cstring>
#include <cstdio>
#include <mutex>
#include <chrono>
#include <condition_variable>

#include "fireboltaamp.h"

#include "PlayerLogManager.h"

#include "PlayerExternalsRdkInterface.h"


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
    const char* firebolt_endpoint = std::getenv("FIREBOLT_ENDPOINT");

	if (!firebolt_endpoint) {
		MW_LOG_ERR("FIREBOLT_ENDPOINT not set; cannot initialize Firebolt");
		return;
	}
	std::string url = firebolt_endpoint;
	if (!CreateFireboltInstance(url))
	{
		MW_LOG_ERR("Failed to create FireboltInstance URL: [%s]", url.c_str());
		return;
	}
	/*Wait Time is 500 millisecond*/
	std::unique_lock<std::mutex> mLock(mFireboltConnectionMutex);
	if (!mFireboltConnectionCV.wait_for(mLock, std::chrono::milliseconds(500), [this] { return mIsConnected; })) {
		MW_LOG_ERR("Firebolt Core To Be Initialized URL: [%s] Failed(Timeout) after 500ms", url.c_str());
		return;
	}
}

bool DeviceFireboltInterface::CreateFireboltInstance(const std::string &url)
{
    const std::string config = "{\
                                \"waitTime\": 3000,\
                                \"logLevel\": \"Info\",\
                                \"workerPool\":{\
                                \"queueSize\": 8,\
                                \"threadCount\": 3\
                                    },\
                                \"wsUrl\": " + url +
                                "}";

	auto callback = [this](bool connected, Firebolt::Error error) {
		this->ConnectionChanged(connected, static_cast<int>(error));
	};
	mIsConnected = false;
	MW_LOG_ERR("CreateFireboltInstance url: %s -- config : %s", url.c_str(), config.c_str());
	Firebolt::Error errorInitialize = Firebolt::IFireboltAampAccessor::Instance().Initialize(config);
	if (errorInitialize != Firebolt::Error::None)
	{
		MW_LOG_ERR("Failed to create FireboltInstance InitializeError:\"%d\"", static_cast<int>(errorInitialize));
		return false;
	}
	auto errorConnect = Firebolt::IFireboltAampAccessor::Instance().Connect(callback);
	if (!errorConnect)
	{
		MW_LOG_ERR("Failed to create FireboltInstance ConnectError:\"%d\"",  static_cast<int>(errorConnect.error()));
		return false;
	}
	mListenerId = *errorConnect;
	MW_LOG_INFO("Firebolt Instance created successfully, Connected to Firebolt!");
	return true;
}

void DeviceFireboltInterface::ConnectionChanged(const bool connected, int error)
{
	MW_LOG_WARN("Firebolt connection changed. Connected: %d Error : %d", connected, error);
	{
		std::lock_guard<std::mutex> lock(mFireboltConnectionMutex);
		mIsConnected = connected;
	}
	mFireboltConnectionCV.notify_one();    
}

void DeviceFireboltInterface::DestroyFireboltInstance()
{
	MW_LOG_WARN("Destroying Firebolt instance");
	Firebolt::IFireboltAampAccessor::Instance().Disconnect(mListenerId);
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
    for(int i = 0; i < mDsMgrSubscriptionId.size(); i++)
    {
        MW_LOG_WARN("Unsubscribing from Firebolt Content Protection events %lld idx:%d", mDsMgrSubscriptionId[i], i);
        auto result =
            Firebolt::IFireboltAampAccessor::Instance().ContentProtectionInterface().unsubscribe(mDsMgrSubscriptionId[i]);
        if (result.error() != Firebolt::Error::None)
        {
            MW_LOG_ERR("Failed to Unsubscribe to watermark events: %d", static_cast<int>(result.error()));
        }
    }
    mDsMgrSubscriptionId.clear();
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
