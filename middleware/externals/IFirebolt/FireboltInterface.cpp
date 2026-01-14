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
 * @file FireboltInterface.cpp
 * @brief Firebolt common interface
 */

#include "FireboltInterface.h"
#include "PlayerLogManager.h"

#include <mutex>
#include <condition_variable>

std::shared_ptr<FireboltInterface> s_pFireboltInterface = nullptr;

std::mutex mFireboltInterfaceConnectionMutex;
std::condition_variable mFireboltInterfaceConnectionCV;

std::shared_ptr<FireboltInterface> FireboltInterface::GetInstance()
{
    if(nullptr == s_pFireboltInterface)
    {
        s_pFireboltInterface = std::shared_ptr<FireboltInterface>(new FireboltInterface());
    }
    return s_pFireboltInterface;
}

FireboltInterface::FireboltInterface()
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
	std::unique_lock<std::mutex> mLock(mFireboltInterfaceConnectionMutex);
	if (!mFireboltInterfaceConnectionCV.wait_for(mLock, std::chrono::milliseconds(500), [this] { return mIsConnected; })) {
		MW_LOG_ERR("Firebolt Core To Be Initialized URL: [%s] Failed(Timeout) after 500ms", url.c_str());
		return;
	}

    MW_LOG_WARN("Firebolt ContentProtection initialized with URL: [%s]", url.c_str());
}

bool FireboltInterface::CreateFireboltInstance(const std::string &url)
{
    const std::string config = "{\
                                \"waitTime\": 5000,\
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

void FireboltInterface::ConnectionChanged(const bool connected, int error)
{
	MW_LOG_WARN("Firebolt connection changed. Connected: %d Error : %d", connected, error);
	{
		std::lock_guard<std::mutex> lock(mFireboltInterfaceConnectionMutex);
		mIsConnected = connected;
	}
	mFireboltInterfaceConnectionCV.notify_one();    
}

void FireboltInterface::DestroyFireboltInstance()
{
	MW_LOG_WARN("Destroying Firebolt instance");
	Firebolt::IFireboltAampAccessor::Instance().Disconnect(mListenerId);
}

FireboltInterface::~FireboltInterface()
{
    Firebolt::IFireboltAampAccessor::Instance().ContentProtectionInterface().unsubscribeAll();
    Firebolt::IFireboltAampAccessor::Instance().DeviceInterface().unsubscribeAll();
    DestroyFireboltInstance();
}