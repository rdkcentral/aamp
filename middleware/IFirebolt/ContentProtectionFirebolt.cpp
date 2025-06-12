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
 * limitations under the License.m
 */

#include "ContentProtectionPriv.h"
#include "_base64.h"
#include <unistd.h>
#include <iomanip>
#include "ContentProtectionFirebolt.h"
#include <mutex>
#include <thread>
#include <chrono>
#include <condition_variable>
#include "AampLogManager.h"
#include "firebolt.h"
#include "contentprotection.h"

std::condition_variable mConnectionCV;
std::mutex mConnectionMutex;
using namespace Firebolt;

ContentProtectionFirebolt::ContentProtectionFirebolt() : mInitialized(false), mIsConnected(false), mSpeedStateMutex()
{
	Initialize();
}

ContentProtectionFirebolt::~ContentProtectionFirebolt()
{
    Deinitialize();
}

class WatermarkEventHandler : public Firebolt::ContentProtection::IContentProtection::IOnWatermarkStatusChangedNotification {
public:
    WatermarkEventHandler(ContentProtectionFirebolt* parent) : mParent(parent) {}

    void onWatermarkStatusChanged(const Firebolt::ContentProtection::WatermarkStatus& status) override {
        mParent->HandleWatermarkEvent(status.sessionId, status.status, status.appId);
    }

private:
    ContentProtectionFirebolt* mParent;
};

static int MapFireboltStatus(const std::string& statusStr) {
    if (statusStr == "GRANTED") return 1;
    if (statusStr == "NOT_REQUIRED") return 2;
    if (statusStr == "DENIED") return 3;
    if (statusStr == "FAILED") return 4;
    return -1;
}

void ContentProtectionFirebolt::SubscribeEvents()
{
    AAMPLOG_INFO("Subscribing to Firebolt Content Protection events");
    Firebolt::Error err;

    mEventHandler = std::make_unique<WatermarkEventHandler>(this);
    Firebolt::IFireboltAccessor::Instance().ContentProtectionInterface().subscribe(*mEventHandler, &err);

    if (err != Firebolt::Error::None)
    {
        AAMPLOG_ERR("Failed to subscribe to watermark events: %d", static_cast<int>(err));
    }
}

void ContentProtectionFirebolt::UnSubscribeEvents()
{
    AAMPLOG_INFO("Unsubscribing from Firebolt Content Protection events");
    Firebolt::Error err;

    if (mEventHandler)
    {
        Firebolt::IFireboltAccessor::Instance().ContentProtectionInterface().unsubscribe(*mEventHandler, &err);

        if (err != Firebolt::Error::None)
        {
            AAMPLOG_ERR("Failed to unsubscribe from watermark events: %d", static_cast<int>(err));
        }
        mEventHandler.reset();
    }
}

void ContentProtectionFirebolt::HandleWatermarkEvent(const std::string& sessionId, const std::string& statusStr, const std::string& appId)
{
    int mappedCode = MapFireboltStatus(statusStr);

    std::lock_guard<std::mutex> lock(mCallbackMutex);
    if (ContentProtectionBase::SendWatermarkSessionEvent_CB)
    {
        ContentProtectionBase::SendWatermarkSessionEvent_CB(std::stoi(sessionId), mappedCode, appId);
    }
}

void ContentProtectionFirebolt::Initialize()
{
	std::lock_guard<std::mutex> lock(mFireboltMutex);
	if (mInitialized) return;

	const char* firebolt_endpoint = std::getenv("FIREBOLT_ENDPOINT");
	if (!firebolt_endpoint) {
		AAMPLOG_ERR("FIREBOLT_ENDPOINT not set; cannot initialize Firebolt");
		return;
	}

	std::string url = firebolt_endpoint;
	if (!CreateFireboltInstance(url))
	{
		AAMPLOG_ERR("Failed to create FireboltInstance URL: [%s]", url.c_str());
		return;
	}
	/*Wait Time is 500 millisecond*/
	std::unique_lock<std::mutex> mLock(mConnectionMutex);
	if (!mConnectionCV.wait_for(mLock, std::chrono::milliseconds(500), [this] { return mIsConnected; })) {
		AAMPLOG_ERR("Firebolt connection timeout after 500ms");
		DestroyFireboltInstance();
		return;
	}
	mInitialized = true;
        /* hide watermarking at startup */
        int sessionId = 0;
	ShowWatermark(false, std::to_string(sessionId));
        /*
        * Release any unexpectedly open sessions.
        * These could exist if a previous aamp session crashed
        * 'firstInstance' check is a defensive measure allowing for the usage of
        * GetInstance() & DestroyInstance() to change in the future
        * InstanceMutex use in GetInstance() makes this thread safe
        * (currently mCPMutex is also locked but ideally the scope of this would be reduced)*/

	static bool firstInstance = true;
        if(firstInstance)
        {
                firstInstance=false;
                //Instructs closePlaybackSession to close all open 'player' sessions
                AAMPLOG_WARN("ContentProtection call closeDrmSession to ensure no old sessions exist.");
                CloseDrmSession(std::to_string(sessionId));
        }
	SubscribeEvents();
	AAMPLOG_INFO("Firebolt ContentProtection initialized with URL: [%s]", url.c_str());
}

void ContentProtectionFirebolt::Deinitialize()
{
	std::lock_guard<std::mutex> lock(mFireboltMutex);
	if (!mInitialized) return;
	ShowWatermark(false);
	UnSubscribeEvents();
	DestroyFireboltInstance();
	mIsConnected = false;
	mInitialized = false;
	AAMPLOG_INFO("Firebolt Core deinitialized");
}

bool ContentProtectionFirebolt::CreateFireboltInstance(const std::string &url)
{
	/*
    const std::string config = "{\
            \"waitTime\": 3000,\
            \"logLevel\": \"Info\",\
            \"workerPool\":{\
            \"queueSize\": 8,\
            \"threadCount\": 3\
            },\
            \"wsUrl\": "  url 
                               "}";
	*/
	const std::string config = "{"
            "\"waitTime\": 3000,"
            "\"logLevel\": \"Info\","
            "\"workerPool\":{"
            "\"queueSize\": 8,"
            "\"threadCount\": 3"
            "},"
            "\"wsUrl\": \"" + url + "\"" // Concatenate the variable here
            "}";

    	auto callback = [this](bool connected, Firebolt::Error error) {
		this->ConnectionChanged(connected, static_cast<int>(error));
	};

    mIsConnected = false; 		//TODO-Needed?
    AAMPLOG_ERR("CreateFireboltInstance url: %s -- config : %s", url.c_str(), config.c_str());
    Firebolt::Error errorInitialize = Firebolt::IFireboltAccessor::Instance().Initialize(config);
    Firebolt::Error errorConnect = Firebolt::IFireboltAccessor::Instance().Connect(callback);

    if (errorInitialize == Firebolt::Error::None && errorConnect == Firebolt::Error::None)
    {
	    return true;
    } 
    AAMPLOG_ERR("Failed to create FireboltInstance InitializeError:\"%d\" ConnectError:\"%d\"", static_cast<int>(errorInitialize), static_cast<int>(errorConnect));
    return false;
}

void ContentProtectionFirebolt::ConnectionChanged(const bool connected, int error)
{
	{
		std::lock_guard<std::mutex> lock(mConnectionMutex);
		mIsConnected = connected;
	}
	mConnectionCV.notify_one();    
	AAMPLOG_INFO("Firebolt connection changed. Connected: %d Error : %d", connected, static_cast<int>(error));

}

void ContentProtectionFirebolt::DestroyFireboltInstance()
{
    AAMPLOG_INFO("Destroying Firebolt instance");
    Firebolt::IFireboltAccessor::Instance().Disconnect();
    Firebolt::IFireboltAccessor::Instance().Deinitialize();
    Firebolt::IFireboltAccessor::Instance().Dispose();
}

bool ContentProtectionFirebolt::Initialized()
{
    return mInitialized;
}

bool ContentProtectionFirebolt::IsActive(bool /*force*/)
{
    return Initialized();
}

bool ContentProtectionFirebolt::CloseDrmSession(const std::string& sessionId)
{
    Firebolt::Error error = Firebolt::Error::None;
    bool ret = false;
    // Check if Firebolt is active before proceeding
    if (!IsActive())
    {
        AAMPLOG_ERR("Firebolt is not active (or) channel couldn't be opened");
        return false;
    }
    std::lock_guard<std::mutex> lock(mCPFMutex);
    // Call the closeDrmSession method from the interface
    Firebolt::IFireboltAccessor::Instance().ContentProtectionInterface().closeDrmSession(sessionId, &error);

    // Check for errors
    if (error == Firebolt::Error::None)
    {
        // No error, session was closed successfully
        AAMPLOG_INFO("Drm session closed successfully for sessionId: %s", sessionId.c_str());
        ret = true;
    }
    else
    {
        // An error occurred, log the error
        AAMPLOG_ERR("CloseDrmSession: failed for sessionID: %s Firebolt Error: \"%d\"", sessionId.c_str(), static_cast<int>(error));
    }
    return ret;
}

bool ContentProtectionFirebolt::SetDrmSessionState(const std::string& sessionId, bool active)
{
    bool result = false;
    Firebolt::Error error = Firebolt::Error::None;

    // Check if Firebolt is active before proceeding
    if (!IsActive())
    {
        AAMPLOG_ERR("Firebolt is not active (or) channel couldn't be opened");
        return result;
    }

    Firebolt::ContentProtection::SessionState sessionState;
    if (active)
    {
        sessionState = Firebolt::ContentProtection::SessionState::ACTIVE;
    }
    else
    {
        sessionState = Firebolt::ContentProtection::SessionState::INACTIVE;
    }
	std::lock_guard<std::mutex> lock(mCPFMutex);
    // Call the setDrmSessionState method from the interface
    Firebolt::IFireboltAccessor::Instance().ContentProtectionInterface().setDrmSessionState(sessionId, sessionState, &error);

    // Check for errors
    if (error == Firebolt::Error::None)
    {
        // No error, state was set successfully
        AAMPLOG_INFO("DRM session state set to %d for sessionId: %s", static_cast<int>(sessionState), sessionId.c_str());
        result = true;
    }
    else
    {
        // An error occurred, log the error
        AAMPLOG_ERR("DRM session state failed to set to %d for sessionId: %s, Firebolt Error: \"%d\"", static_cast<int>(sessionState), sessionId.c_str(), static_cast<int>(error));
    }
    return result;
}

/**
 * @brief To set Playback Speed State to SecManager
 */
bool ContentProtectionFirebolt::SetPlaybackPosition(const std::string& sessionId, float speed, int32_t position)
{
    bool ret = false;
    Firebolt::Error error = Firebolt::Error::None;
    // Check if Firebolt is active before proceeding
    if (!IsActive())
    {
        AAMPLOG_ERR("Firebolt is not active (or) channel couldn't be opened");
        return ret;
    }
    std::lock_guard<std::mutex> lock(mCPFMutex);
    // Call the setPlaybackPosition method from the interface
    Firebolt::IFireboltAccessor::Instance().ContentProtectionInterface().setPlaybackPosition(sessionId, speed, position, &error);

    // Check for errors
    if (error == Firebolt::Error::None)
    {
        // No error, playback position was set successfully
        AAMPLOG_INFO("SetPlaybackPosition set successfully for sessionId: %s at position %d with speed %.2f", sessionId.c_str(), position, speed);
        ret = true;
    }
    else
    {
        // An error occurred, log the error
    	AAMPLOG_ERR("SetPlaybackPosition failed to set for ID: %s Firebolt Error: \"%d\"", sessionId.c_str(), static_cast<int>(error));
    }
    return ret;
}

/**
 * @brief Show watermark image
 */
void ContentProtectionFirebolt::ShowWatermark(bool show, const std::string& sessionId)
{
    Firebolt::Error error = Firebolt::Error::None;
	
    // Check if Firebolt is active before proceeding
    if (!IsActive()) {
        AAMPLOG_ERR("Firebolt is not active (or) channel couldn't be opened");
        return;
    }
	
	std::lock_guard<std::mutex> lock(mCPFMutex);
    // Call the showWatermark method from the interface
    Firebolt::IFireboltAccessor::Instance().ContentProtectionInterface().showWatermark(sessionId, show, 0, &error); //TO-DO : Opacity Level

    // Check for errors
    if (error == Firebolt::Error::None) {
        // No error, watermark visibility was successfully set
        AAMPLOG_INFO("ShowWatermark visibility set successfully. Show: %d", show);
    } else {
        // An error occurred, log the error
        AAMPLOG_ERR("showWatermark failed. Firebolt Error: \"%d\"", static_cast<int>(error));
    }
}

//To-do: using Firebolt::ContentProtection??
static Firebolt::ContentProtection::KeySystem convertStringToKeySystem(const std::string& keySystemStr)
{
    if (keySystemStr.find("widevine") != std::string::npos)
    {
        return Firebolt::ContentProtection::KeySystem::WIDEVINE;
    }
    else if (keySystemStr.find("playready") != std::string::npos)
    {
        return Firebolt::ContentProtection::KeySystem::PLAYREADY;
    }
    else if (keySystemStr.find("clearkey") != std::string::npos)
    {
        return Firebolt::ContentProtection::KeySystem::CLEARKEY;
    }
    else
    {
        AAMPLOG_ERR("Unknown KeySystem string: %s returning to default", keySystemStr.c_str());
        return Firebolt::ContentProtection::KeySystem::WIDEVINE; // safest fallback default
    }
}

bool ContentProtectionFirebolt::OpenDrmSession(std::string& clientId, std::string appId, std::string keySystem, std::string licenseRequest, std::string initData, std::string& sessionId, std::string &response)
{
    Firebolt::Error error = Firebolt::Error::None;
    bool ret = false;
    // Check if the system is active before proceeding
    if (!IsActive()) {
        AAMPLOG_ERR("Firebolt is not active (or) channel couldn't be opened");
        return false; // Return false if system is not active
    }
    Firebolt::ContentProtection::DRMSession drmSession;
    drmSession = Firebolt::IFireboltAccessor::Instance()
    .ContentProtectionInterface()
    .openDrmSession(clientId, appId, convertStringToKeySystem(keySystem),
            licenseRequest, initData, &error);
    if (error != Firebolt::Error::None)
    {
        AAMPLOG_ERR("openDrmSession: Firebolt Error: \"%d\"", static_cast<int>(error));
    }
    else
    {
	AAMPLOG_INFO("DRM session opened successfully with sessionId: '%s' with Response %s", drmSession.sessionId.c_str(), drmSession.openSessionResponse.c_str());
        response = drmSession.openSessionResponse;
        sessionId = drmSession.sessionId;
        ret = true;
    }  
    return ret;
}

bool ContentProtectionFirebolt::UpdateDrmSession(const std::string& sessionId, std::string licenseRequest, std::string initData, std::string &response)
{
    // Create an error object to capture any errors
    Firebolt::Error error = Firebolt::Error::None;
    bool ret = false;
    // Check if the system is active before proceeding
    if (!IsActive()) {
        AAMPLOG_ERR("Firebolt is not active (or) channel couldn't be opened");
        return false; // Return false if system is not active
    }
    Firebolt::ContentProtection::DRMSession drmSession;
    drmSession =  Firebolt::IFireboltAccessor::Instance()
                                .ContentProtectionInterface()
                                .updateDrmSession(sessionId,
                                                licenseRequest, initData, &error);

    if (error != Firebolt::Error::None)
    {
        AAMPLOG_ERR("updateDrmSession: Firebolt Error: \"%d\"", static_cast<int>(error));
    }
    else
    {
        AAMPLOG_INFO("DRM session updated successfully for sessionId: %s with Response %s", sessionId.c_str(), drmSession.openSessionResponse.c_str());
        response = drmSession.openSessionResponse;
        ret = true;
    }
    return ret;
}