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
#include "ContentSecurityManager.h"
#include "_base64.h"
#include <unistd.h>
#include <iomanip>
#include "ContentProtectionFirebolt.h"
#include <mutex>
#include <thread>
#include <chrono>
#include "PlayerJsonObject.h"
#include <condition_variable>
#include "PlayerLogManager.h"
#include <inttypes.h>
#include "contentprotection.h"
#include "fireboltaamp.h"
std::condition_variable mConnectionCV;
std::mutex mConnectionMutex;
using namespace Firebolt;
uint64_t ContentProtectionFirebolt::mSubscriptionId = 0; 

ContentProtectionFirebolt::ContentProtectionFirebolt() : mInitialized(false), mIsConnected(false), mSpeedStateMutex(), mContentProtectionMutex(), mFireboltInitMutex(), mListenerId(0)
{
	Initialize();	
}
ContentProtectionFirebolt::~ContentProtectionFirebolt()
{
	DeInitialize();
}

static int MapFireboltStatus(const std::string& statusStr) {
	if (statusStr == "GRANTED") return 1;
	if (statusStr == "NOT_REQUIRED") return 2;
	if (statusStr == "DENIED") return 3;
	if (statusStr == "FAILED") return 4;
	return -1;
}

// TODO- Yet to test Watermark Events as ContentProtection Thunder Plugin have issues.
void ContentProtectionFirebolt::SubscribeEvents()
{
	auto result =  Firebolt::IFireboltAampAccessor::Instance().ContentProtectionInterface().subscribeOnWatermarkStatusChanged(
			[this](const auto& status)
			{
				HandleWatermarkEvent(status.sessionId, status.status, status.appId);

			});
	if(result)
	{
		mSubscriptionId = result.value();
		MW_LOG_INFO("Subscribed to Firebolt Content Protection events. mSubscriptionId = %lld", mSubscriptionId);
	}
	else
	{
		MW_LOG_ERR("Failed to subscribe to watermark events: %d", static_cast<int>(result.error()));
	}
}

void ContentProtectionFirebolt::UnSubscribeEvents()
{
	MW_LOG_INFO("Unsubscribing from Firebolt Content Protection events %lld", mSubscriptionId);
	auto result =
		Firebolt::IFireboltAampAccessor::Instance().ContentProtectionInterface().unsubscribe(mSubscriptionId);
	if (result.error() != Firebolt::Error::None)
	{
		MW_LOG_ERR("Failed to Unsubscribe to watermark events: %d", static_cast<int>(result.error()));
	}
}

void ContentProtectionFirebolt::HandleWatermarkEvent(const std::string& sessionId, const std::string& statusStr, const std::string& appId)
{
	if(mInitialized)
	{
		//TODO Testing Watermark
		int mappedCode = MapFireboltStatus(statusStr);
		std::lock_guard<std::mutex> lock(mFireboltInitMutex);
		if (ContentSecurityManager::SendWatermarkSessionEvent_CB)
		{
			ContentSecurityManager::SendWatermarkSessionEvent_CB(std::stoi(sessionId), mappedCode, appId);
		}
	}
}

void ContentProtectionFirebolt::Initialize()
{
	std::lock_guard<std::mutex> lock(mFireboltInitMutex);
	if (mInitialized) return;
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
	std::unique_lock<std::mutex> mLock(mConnectionMutex);
	if (!mConnectionCV.wait_for(mLock, std::chrono::milliseconds(500), [this] { return mIsConnected; })) {
		MW_LOG_ERR("Firebolt Core To Be Initialized URL: [%s] Failed(Timeout) after 500ms", url.c_str());
		return;
	}
	mInitialized = true;
	/* hide watermarking at startup */
	int64_t sessionId = 0;
	ShowWatermark(false, sessionId);
	/* CP Thunder Plugin doesnt allow invalid sessionId like 0 as in Thunder, hence not calling CloseDrmSession */
	//CloseDrmSession(sessionId);
	SubscribeEvents();
	MW_LOG_WARN("Firebolt ContentProtection initialized with URL: [%s]", url.c_str());
}

void ContentProtectionFirebolt::DeInitialize()
{
	/* SessionID is not used internally in CP Thunder Plugin for ShowWatermark.
	   However Native SDK requires it to be sent. Keeping it dummy*/
	ShowWatermark(false, 0);
	UnSubscribeEvents();
	DestroyFireboltInstance();
	mIsConnected = false;
	mInitialized = false;
	MW_LOG_INFO("Firebolt Core de-initialized");
}

bool ContentProtectionFirebolt::CreateFireboltInstance(const std::string &url)
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

void ContentProtectionFirebolt::ConnectionChanged(const bool connected, int error)
{
	MW_LOG_WARN("Firebolt connection changed. Connected: %d Error : %d", connected, error);
	{
		std::lock_guard<std::mutex> lock(mConnectionMutex);
		mIsConnected = connected;
	}
	mConnectionCV.notify_one();    
}

void ContentProtectionFirebolt::DestroyFireboltInstance()
{
	MW_LOG_WARN("Destroying Firebolt instance");
	Firebolt::IFireboltAampAccessor::Instance().Disconnect(mListenerId);
}

bool ContentProtectionFirebolt::IsActive(bool /*force*/)
{
	return mInitialized;
}

bool ContentProtectionFirebolt::AcquireLicenseOpenOrUpdate( std::string clientId, std::string appId, const char* licenseUrl, const char* moneyTraceMetadata[][2],
		const char* accessAttributes[][2], const char* contentMetadata, size_t contMetaLen,
		const char* licenseRequest, size_t licReqLen, const char* keySystemId,
		const char* mediaUsage, const char* accessToken, size_t accTokenLen,
		ContentSecurityManagerSession &session,
		char** licenseResponse, size_t* licenseResponseLength, int32_t* statusCode, int32_t* reasonCode, int32_t* businessStatus, bool isVideoMuted, int sleepTime)
{
	// licenseUrl un-used now
	(void) licenseUrl;

	bool ret = false;
	bool result = false;
	unsigned int retryCount = 0;
	bool update = false;

	//Initializing it with default error codes (which would be sent if there any jsonRPC
	//call failures to thunder)
	*statusCode = CONTENT_SECURITY_MANAGER_DRM_FAILURE;
	*reasonCode = CONTENT_SECURITY_MANAGER_DRM_GEN_FAILURE;

	PlayerJsonObject param;
	PlayerJsonObject response;
	PlayerJsonObject sessionConfig;
	PlayerJsonObject aspectDimensions;
	const char* apiName = "OpenDrmSession";

	std::string accessTokenStr = accessToken
		? std::string(accessToken, accTokenLen)
		: std::string();
	std::string contentMetaDataStr = contentMetadata
		? std::string(contentMetadata, contMetaLen)
		: std::string();

	std::string keySystem = keySystemId ? keySystemId : "";
	std::string licenseRequestStr = licenseRequest
		? std::string(licenseRequest, licReqLen)
		: std::string();		

	// use .add() instead of operator[]
	sessionConfig.add("distributedTraceType", "money");
	sessionConfig.add("distributedTraceId", moneyTraceMetadata[0][1]);
	sessionConfig.add("sessionState", isVideoMuted ? "inactive" : "active");

	// width/height are numbers, but PlayerJsonObject's add expects strings -> so convert to string
	aspectDimensions.add("width", std::to_string(1920));
	aspectDimensions.add("height", std::to_string(1080));

	std::string mediaUsageStr = mediaUsage ? mediaUsage : "";

	param.add("sessionConfiguration", sessionConfig);
	param.add("contentAspectDimensions", aspectDimensions);
	param.add("mediaUsage", mediaUsageStr);

	int64_t sessionId;
	if (session.isSessionValid())
	{
		// If sessionId is present, we are trying to acquire a new license within the same session
		apiName = "UpdateDrmSession";
		sessionId = session.getSessionID();
		update = true;
	}

	{
		std::lock_guard<std::mutex> lock(mContentProtectionMutex);
		if (!accessTokenStr.empty() &&
				!contentMetaDataStr.empty() &&
				!licenseRequestStr.empty())

		{
			MW_LOG_INFO("Access token, Content metadata and license request are copied successfully, passing details with ContentProtection");

			//Set json params to be used by sec manager
			param.add("accessToken", accessTokenStr);
			param.add("contentMetadata", contentMetaDataStr);

			std::string initData = param.print_UnFormatted();
			MW_LOG_WARN("ContentProtection %s param: %s",apiName, initData.c_str());
			bool result = false;
			//invoke "openDrmSession" or "updateDrmSession" with retries for specific error cases
			do
			{
				std::string drmSession;
				if (!IsActive())
				{
					MW_LOG_ERR("Firebolt is not active (or) channel couldn't be opened");
					return false;
				}

				// Call the openDrmSession method from the interface
				if(!update)
				{
					result = OpenDrmSession(clientId, appId, keySystem,
							licenseRequest, initData, sessionId, drmSession);
				}
				else
				{
					result = 
						UpdateDrmSession(sessionId,
								licenseRequest, initData, drmSession);					
				}
				if (drmSession.empty())
				{
					MW_LOG_WARN("DrmSession Response is empty.");
				}
				if (result) 
				{
					ContentSecurityManagerSession newSession;
					PlayerJsonObject sessionObj(drmSession);

					if(!drmSession.empty())
					{
						/*
						 * Ensure all sessions have a Session ID created to manage lifetime
						 * multiple object creation is OK as an existing instance should be returned
						 * where input data changes e.g. following a call to updatePlaybackSession
						 * the input data to the shared session is updated here
						 */

						newSession = ContentSecurityManagerSession(sessionId, 
								ContentSecurityManager::getInputSummaryHash(moneyTraceMetadata, contentMetadata,
									contMetaLen, licenseRequest, keySystemId,
									mediaUsage, accessToken, isVideoMuted));

						std::string license;
						if (sessionObj.get("license", license))
						{
							MW_LOG_TRACE("ContentProtection obtained license with length: %zu and data: %s", license.size(), license.c_str());

							if (!license.empty())
							{
								unsigned char* licenseDecoded = nullptr;
								size_t licenseDecodedLen = 0;

								licenseDecoded = base64_Decode(license.c_str(), &licenseDecodedLen);
								MW_LOG_TRACE("ContentProtection license decoded len: %zu and data: %p", licenseDecodedLen, licenseDecoded);

								if (licenseDecoded != nullptr && licenseDecodedLen != 0)
								{
									*licenseResponse = (char*)malloc(licenseDecodedLen);
									if (*licenseResponse)
									{
										memcpy(*licenseResponse, licenseDecoded, licenseDecodedLen);
										*licenseResponseLength = licenseDecodedLen;

										MW_LOG_INFO("ContentProtection license post base64 decode length: %zu", *licenseResponseLength);
									}
									else
									{
										MW_LOG_ERR("ContentProtection failed to allocate memory for license!");
									}
									free(licenseDecoded);
									ret = true;
								}
								else
								{
									MW_LOG_ERR("ContentProtection license base64 decode failed!");
								}
							}
						}
					}

					if (newSession.isSessionValid() && !session.isSessionValid())
					{
						session = newSession;
					}

				}

				if(!drmSession.empty())
				{

					PlayerJsonObject response(drmSession);
					PlayerJsonObject resultContext;
					if (response.get("secManagerResultContext", resultContext))
					{
						int value = -1;

						// Get statusCode
						if (resultContext.get("class", value))
						{
							*statusCode = value;
						}

						// Get reasonCode
						if (resultContext.get("reason", value))
						{
							*reasonCode = value;
						}

						// Get businessStatus
						if (resultContext.get("businessStatus", value))
						{
							*businessStatus = value;
						}

						MW_LOG_WARN("ContentProtection Parsed Status Code: %d, Reason: %d, Business Status: %d",
								statusCode ? *statusCode : -1,
								reasonCode ? *reasonCode : -1,
								businessStatus ? *businessStatus : -1);
					}
				}
				if(!ret)
				{
					//As per Secmanager retry is meaningful only for
					//Digital Rights Management Failure Class (200) or
					//Watermarking Failure Class (300)
					//having the reasons -
					//DRM license service network timeout / Request/network time out (3).
					//DRM license network connection failure/Watermark vendor-access service connection failure (4)
					//DRM license server busy/Watermark service busy (5)
					if((*statusCode == CONTENT_SECURITY_MANAGER_DRM_FAILURE || *statusCode == CONTENT_SECURITY_MANAGER_WM_FAILURE) &&
							(*reasonCode == CONTENT_SECURITY_MANAGER_SERVICE_TIMEOUT ||
							 *reasonCode == CONTENT_SECURITY_MANAGER_SERVICE_CON_FAILURE ||
							 *reasonCode == CONTENT_SECURITY_MANAGER_SERVICE_BUSY ) && retryCount < MAX_LICENSE_REQUEST_ATTEMPTS)
					{
						++retryCount;
						MW_LOG_WARN("ContentProtection license request failed, response for %s : statusCode: %d, reasonCode: %d, so retrying with delay %d, retry count : %u", apiName, *statusCode, *reasonCode, sleepTime, retryCount );
						ms_sleep(sleepTime);						
					}
					else
					{
						MW_LOG_ERR("ContentProtection license request failed, response for %s : statusCode: %d, reasonCode: %d", apiName, *statusCode, *reasonCode);
						break;
					}
				}
				else
				{
					MW_LOG_INFO("ContentProtection license request success, response for %s : statusCode: %d, reasonCode: %d, session status: %s", apiName, *statusCode, *reasonCode, isVideoMuted ? "inactive" : "active");
					break;
				}
			}
			while(retryCount < MAX_LICENSE_REQUEST_ATTEMPTS);
		}
		else
		{
			MW_LOG_ERR("ContentProtection Failed to copy access token to the shared memory, %s is aborted statusCode: %d, reasonCode: %d", apiName, *statusCode, *reasonCode);
		}
	}
	return ret;
}

void ContentProtectionFirebolt::CloseDrmSession(int64_t sessionId)
{
	// Check if Firebolt is active before proceeding
	if (!IsActive())
	{
		MW_LOG_ERR("Firebolt is not active (or) channel couldn't be opened");
		return;
	}
	std::lock_guard<std::mutex> lock(mContentProtectionMutex);
	// Call the closeDrmSession method from the interface
	auto result = Firebolt::IFireboltAampAccessor::Instance().ContentProtectionInterface().closeDrmSession(std::to_string(sessionId));
	// Check for errors
	if (result.error() == Firebolt::Error::None)
	{
		// No error, session was closed successfully
		MW_LOG_INFO("Drm session closed successfully for sessionId: %" PRId64 "", sessionId);
	}
	else
	{
		// An error occurred, log the error
		MW_LOG_ERR("CloseDrmSession: failed for sessionID: %" PRId64 " Firebolt Error: \"%d\"", sessionId, static_cast<int>(result.error()));
	}
}
bool ContentProtectionFirebolt::SetDrmSessionState(int64_t sessionId, bool active)
{
	bool ret = false;
	Firebolt::Error error = Firebolt::Error::None;
	// Check if Firebolt is active before proceeding
	if (!IsActive())
	{
		MW_LOG_ERR("Firebolt is not active (or) channel couldn't be opened");
		return ret;
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
	std::lock_guard<std::mutex> lock(mContentProtectionMutex);
	// Call the setDrmSessionState method from the interface
	auto result = Firebolt::IFireboltAampAccessor::Instance().ContentProtectionInterface().setDrmSessionState(std::to_string(sessionId), sessionState);
	// Check for errors
	if (result.error() == Firebolt::Error::None)
	{
		// No error, state was set successfully
		MW_LOG_INFO("DRM session state set to %d for sessionId: %" PRId64 "", active, sessionId);
		ret = true;
	}
	else
	{
		// An error occurred, log the error
		MW_LOG_ERR("DRM session state failed to set to %d for sessionId: %" PRId64 ", Firebolt Error: \"%d\"", static_cast<int>(sessionState), sessionId, static_cast<int>(result.error()));
	}
	return ret;
}
/**
 * @brief To set Playback Speed State to SecManager
 */
bool ContentProtectionFirebolt::SetPlaybackPosition(int64_t sessionId, float speed, int32_t position)
{
	bool ret = false;
	// Check if Firebolt is active before proceeding
	if (!IsActive())
	{
		MW_LOG_ERR("Firebolt is not active (or) channel couldn't be opened");
		return ret;
	}
	std::lock_guard<std::mutex> lock(mContentProtectionMutex);
	// Call the setPlaybackPosition method from the interface
	auto result = Firebolt::IFireboltAampAccessor::Instance().ContentProtectionInterface().setPlaybackPosition(std::to_string(sessionId), speed, position);
	// Check for errors
	if (result.error() == Firebolt::Error::None)
	{
		// No error, playback position was set successfully
		MW_LOG_INFO("SetPlaybackPosition set successfully for sessionId: %" PRId64 " at position %d with speed %.2f", sessionId, position, speed);
		ret = true;
	}
	else
	{
		// An error occurred, log the error
		MW_LOG_ERR("SetPlaybackPosition failed to set for ID: %" PRId64 " Firebolt Error: \"%d\"", sessionId, static_cast<int>(result.error()));
	}
	return ret;
}
/**
 * @brief Show watermark image
 */
void ContentProtectionFirebolt::ShowWatermark(bool show, int64_t sessionId)
{
	// Check if Firebolt is active before proceeding
	if (!IsActive()) {
		MW_LOG_ERR("Firebolt is not active (or) channel couldn't be opened");
		return;
	}

	std::lock_guard<std::mutex> lock(mContentProtectionMutex);
	// Call the showWatermark method from the interface
	auto result = Firebolt::IFireboltAampAccessor::Instance().ContentProtectionInterface().showWatermark(std::to_string(sessionId), show, 0);
	// Check for errors
	if (result.error() == Firebolt::Error::None) {
		// No error, watermark visibility was successfully set
		MW_LOG_INFO("ShowWatermark visibility set successfully. Show: %d", show);
	} else {
		// An error occurred, log the error
		MW_LOG_ERR("showWatermark failed. Firebolt Error: \"%d\"", static_cast<int>(result.error()));
	}
}
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
		MW_LOG_ERR("Unknown KeySystem string: %s returning to default", keySystemStr.c_str());
		return Firebolt::ContentProtection::KeySystem::WIDEVINE; // safest fallback default
	}
}
bool ContentProtectionFirebolt::OpenDrmSession(std::string& clientId, std::string appId, std::string keySystem, std::string licenseRequest, std::string initData, int64_t &sessionId, std::string &response)
{
	bool ret = false;
	// Check if the system is active before proceeding
	if (!IsActive()) {
		MW_LOG_ERR("Firebolt is not active (or) channel couldn't be opened");
		return false; // Return false if system is not active
	}
//	Firebolt::ContentProtection::DRMSession drmSession;
	auto drmSession = Firebolt::IFireboltAampAccessor::Instance()
		.ContentProtectionInterface()
		.openDrmSession(clientId, appId, convertStringToKeySystem(keySystem),
				licenseRequest, initData);
	if (drmSession)
	{
		MW_LOG_WARN("DRM session opened successfully with sessionId: '%s' with Response %s", drmSession->sessionId.c_str(), drmSession->openSessionResponse.c_str());
		response = drmSession->openSessionResponse;
		sessionId = std::stoll(drmSession->sessionId);
		ret = true;
	}
	else
	{
		  MW_LOG_ERR("openDrmSession: Firebolt Error: \"%d\"", static_cast<int>(drmSession.error()));
	}
	return ret;
}
bool ContentProtectionFirebolt::UpdateDrmSession(int64_t sessionId, std::string licenseRequest, std::string initData, std::string &response)
{
	bool ret = false;
	// Check if the system is active before proceeding
	if (!IsActive()) {
		MW_LOG_ERR("Firebolt is not active (or) channel couldn't be opened");
		return false; // Return false if system is not active
	}

	auto drmSession =  Firebolt::IFireboltAampAccessor::Instance()
		.ContentProtectionInterface()
		.updateDrmSession(std::to_string(sessionId),
				licenseRequest, initData);
	if(drmSession)
	{
		MW_LOG_INFO("DRM session updated successfully for sessionId: %" PRId64 " with Response %s", sessionId, drmSession.value().c_str());
		response = drmSession.value();
		ret = true;
	}
	else
	{
		MW_LOG_ERR("updateDrmSession: Firebolt Error: \"%d\"", static_cast<int>(drmSession.error()));
	}
	return ret;
}
