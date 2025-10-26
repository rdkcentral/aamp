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
#include "ContentProtectionFirebolt.h"
#include "PlayerSecInterface.h"
#include "_base64.h"
#include "PlayerJsonObject.h"
#include "PlayerLogManager.h"
#include "contentprotection.h"
#include "fireboltaamp.h"
#include "FireboltInterface.h"

#include <unistd.h>
#include <iomanip>
#include <mutex>
#include <thread>
#include <chrono>
#include <condition_variable>
#include <inttypes.h>

std::condition_variable mConnectionCV;
std::mutex mConnectionMutex;
using namespace Firebolt;
uint64_t ContentProtectionFirebolt::mSubscriptionId = 0;

//Lookup table to convert CPS error to secmanager error
static const std::map<const int32_t, std::pair<const int32_t, const int32_t>> ContentProtectionSecManagerErrorLookUp =
{
	{CONTENT_PROTECTION_SERVICE_INVALID_ASPECT_DIMENSIONS,              {CONTENT_SECURITY_MANAGER_CLASS_RESULT_API_FAIL,        CONTENT_SECURITY_MANAGER_REASON_API_INVALID_ASPECT_DIMENSION}},
	{CONTENT_PROTECTION_SERVICE_INVALID_KEY_SYSTEM,                     {CONTENT_SECURITY_MANAGER_CLASS_RESULT_API_FAIL,        CONTENT_SECURITY_MANAGER_REASON_API_INVALID_KEY_SYSTEM_PARAM}},
	{CONTENT_PROTECTION_SERVICE_INVALID_LICENSE_REQUEST,                {CONTENT_SECURITY_MANAGER_CLASS_RESULT_API_FAIL,        CONTENT_SECURITY_MANAGER_REASON_API_INVALID_DRM_LICENSE_PARAM}},
	{CONTENT_PROTECTION_SERVICE_INVALID_CONTENT_METADATA,               {CONTENT_SECURITY_MANAGER_CLASS_RESULT_API_FAIL,        CONTENT_SECURITY_MANAGER_REASON_API_INVALID_CONTENT_METADATA}},
	{CONTENT_PROTECTION_SERVICE_INVALID_MEDIA_USAGE,                    {CONTENT_SECURITY_MANAGER_CLASS_RESULT_API_FAIL,        CONTENT_SECURITY_MANAGER_REASON_API_INVALID_MEDIA_USAGE}},
	{CONTENT_PROTECTION_SERVICE_INVALID_ACCESS_TOKEN,                   {CONTENT_SECURITY_MANAGER_CLASS_RESULT_API_FAIL,        CONTENT_SECURITY_MANAGER_REASON_API_INVALID_ACCESS_TOKEN}},
	{CONTENT_PROTECTION_SERVICE_INVALID_ACCESS_ATTRIBUTES,              {CONTENT_SECURITY_MANAGER_CLASS_RESULT_API_FAIL,        CONTENT_SECURITY_MANAGER_REASON_API_INVALID_ACCESS_ATTRIBUTE}},
	{CONTENT_PROTECTION_SERVICE_INVALID_SESSION_ID,                     {CONTENT_SECURITY_MANAGER_CLASS_RESULT_API_FAIL,        CONTENT_SECURITY_MANAGER_REASON_API_INVALID_SESSION_ID}},
	{CONTENT_PROTECTION_SERVICE_INVALID_CLIENT_ID,                      {CONTENT_SECURITY_MANAGER_CLASS_RESULT_API_FAIL,        CONTENT_SECURITY_MANAGER_REASON_API_INVALID_CLIENT_ID}},
	{CONTENT_PROTECTION_SERVICE_INVALID_WATERMARKING_PARAM,             {CONTENT_SECURITY_MANAGER_CLASS_RESULT_API_FAIL,        CONTENT_SECURITY_MANAGER_REASON_API_INVALID_WATERMARK_PARAMETER}},
	{CONTENT_PROTECTION_SERVICE_INVALID_CONTENT_ATTRIBUTES,             {CONTENT_SECURITY_MANAGER_CLASS_RESULT_API_FAIL,        CONTENT_SECURITY_MANAGER_REASON_API_INVALID_CONTENT_PARAMETER}},
	{CONTENT_PROTECTION_SERVICE_DRM_GENERAL_FAILURE,                    {CONTENT_SECURITY_MANAGER_CLASS_RESULT_DRM_FAIL,        CONTENT_SECURITY_MANAGER_REASON_DRM_GENERAL_FAILURE}},
	{CONTENT_PROTECTION_SERVICE_DRM_LICENSE_NW_TIMEOUT,                 {CONTENT_SECURITY_MANAGER_CLASS_RESULT_DRM_FAIL,        CONTENT_SECURITY_MANAGER_REASON_DRM_LICENSE_TIMEOUT}},
	{CONTENT_PROTECTION_SERVICE_DRM_LICENSE_NW_CONNECTION_FAILURE,      {CONTENT_SECURITY_MANAGER_CLASS_RESULT_DRM_FAIL,        CONTENT_SECURITY_MANAGER_REASON_DRM_LICENSE_NETWORK_FAIL}},
	{CONTENT_PROTECTION_SERVICE_DRM_ACCESS_TOKEN_EXPIRED,               {CONTENT_SECURITY_MANAGER_CLASS_RESULT_DRM_FAIL,        CONTENT_SECURITY_MANAGER_REASON_DRM_ACCESS_TOKEN_EXPIRED}},
	{CONTENT_PROTECTION_SERVICE_DRM_MAC_KEY_NOT_PROVISIONED,            {CONTENT_SECURITY_MANAGER_CLASS_RESULT_DRM_FAIL,        CONTENT_SECURITY_MANAGER_REASON_DRM_MAC_TOKEN_NO_PROV}},
	{CONTENT_PROTECTION_SERVICE_DRM_MEMORY_ALLOCATION_ERROR,            {CONTENT_SECURITY_MANAGER_CLASS_RESULT_DRM_FAIL,        CONTENT_SECURITY_MANAGER_REASON_DRM_MEMORY_ALLOCATION_ERROR}},
	{CONTENT_PROTECTION_SERVICE_DRM_SECURITY_API_FAILURE,               {CONTENT_SECURITY_MANAGER_CLASS_RESULT_DRM_FAIL,        CONTENT_SECURITY_MANAGER_REASON_DRM_SECAPI_USAGE_FAILURE}},
	{CONTENT_PROTECTION_SERVICE_DRM_ENTITLEMENT_FAILURE,                {CONTENT_SECURITY_MANAGER_CLASS_RESULT_DRM_FAIL,        CONTENT_SECURITY_MANAGER_REASON_DRM_ENTITLEMENT_ERROR}},
	{CONTENT_PROTECTION_SERVICE_WATERMARK_GENERAL_FAILURE,              {CONTENT_SECURITY_MANAGER_CLASS_RESULT_WATERMARK_FAIL,        CONTENT_SECURITY_MANAGER_REASON_WM_GENERAL_FAILURE}},
	{CONTENT_PROTECTION_SERVICE_WATERMARK_TIMEOUT,                      {CONTENT_SECURITY_MANAGER_CLASS_RESULT_WATERMARK_FAIL,        CONTENT_SECURITY_MANAGER_REASON_WM_NETWORK_TIMEOUT}},
	{CONTENT_PROTECTION_SERVICE_WATERMARK_MEMORY_ALLOCATION_ERROR,      {CONTENT_SECURITY_MANAGER_CLASS_RESULT_WATERMARK_FAIL,        CONTENT_SECURITY_MANAGER_REASON_WM_MEMORY_ALLOCATION_ERROR}},
	{CONTENT_PROTECTION_SERVICE_WATERMARK_PERCEPTIBILITY_ERROR,         {CONTENT_SECURITY_MANAGER_CLASS_RESULT_WATERMARK_FAIL,        CONTENT_SECURITY_MANAGER_REASON_WM_PERCEPTIBILITY_INVALID_INPUT}}
};

/**
 * @brief Convert the CPS DRM error code into secmanager error code to have a unified verbose error reported
 */
bool getContentProtectionAsVerboseErrorCode(int32_t httpCode, int32_t &secManagerClass, int32_t &secManagerReasonCode )
{
	secManagerClass = CONTENT_SECURITY_MANAGER_DRM_FAILURE;
	secManagerReasonCode = CONTENT_SECURITY_MANAGER_DRM_GEN_FAILURE;
	//look for the correct code from the lookup
	auto it = ContentProtectionSecManagerErrorLookUp.find(httpCode);
	if (it != ContentProtectionSecManagerErrorLookUp.end()) {
		secManagerClass = it->second.first;
		secManagerReasonCode = it->second.second;
		return true;
	}
	return false;
}

ContentProtectionFirebolt::ContentProtectionFirebolt() : mInitialized(false), mSpeedStateMutex(), mContentProtectionMutex(), mFireboltInitMutex()
{
	Initialize();	
}
ContentProtectionFirebolt::~ContentProtectionFirebolt()
{
	DeInitialize();
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
	MW_LOG_INFO("ContentSecurityManager HanldeWatermarkEvent invoked  | sessionId=%s status=%s appId=%s",
            sessionId.c_str(), statusStr.c_str(), appId.c_str());
	if(mInitialized)
	{
   		MW_LOG_INFO("HandleWaterMarkEvent Triggered");
    	PlayerJsonObject statusJson(statusStr);
		int reasonCode = -1;
		if (statusJson.get("failureReason", reasonCode ))
        {
			MW_LOG_INFO("HandleWaterMarkEvent Failure ReasonCode %d", reasonCode);
        }
		else
		{
			MW_LOG_INFO("Json Parsing Failed to extract Watermarking status");
		}
		std::lock_guard<std::mutex> lock(mFireboltInitMutex);
		if (ContentSecurityManager::SendWatermarkSessionEvent_CB)
		{
			MW_LOG_INFO("ContentSecurityManager SendWatermarkSessionEvent_CB invoked | sessionId=%s reasonCode =%d appId=%s",
            sessionId.c_str(), reasonCode, appId.c_str());
			ContentSecurityManager::SendWatermarkSessionEvent_CB(std::stoi(sessionId), reasonCode, appId);
		}
	}
}

void ContentProtectionFirebolt::Initialize()
{
	MW_LOG_INFO("ContentProtectionFirebolt Initialize ");
	m_pFireboltInterface = FireboltInterface::GetInstance();
	mInitialized = true;
	/* hide watermarking at startup */
	int64_t sessionId = 0;
	ShowWatermark(false, sessionId);
	/* CP Thunder Plugin doesnt allow invalid sessionId like 0 as in Thunder, hence not calling CloseDrmSession */
	//CloseDrmSession(sessionId);
	SubscribeEvents();
}

void ContentProtectionFirebolt::DeInitialize()
{
	/* SessionID is not used internally in CP Thunder Plugin for ShowWatermark.
	   However Native SDK requires it to be sent. Keeping it dummy*/
	ShowWatermark(false, 0);
	UnSubscribeEvents();
	mInitialized = false;
	m_pFireboltInterface = nullptr;
	MW_LOG_INFO("Firebolt Core de-initialized");
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
	int32_t errorCode = CONTENT_SECURITY_MANAGER_DRM_GEN_ERR_NONE;

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
	aspectDimensions.add("width", 1920);
	aspectDimensions.add("height", 1080);

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
							licenseRequest, initData, sessionId, errorCode, drmSession);
				}
				else
				{
					result = 
						UpdateDrmSession(sessionId, errorCode,
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
				else if( errorCode != CONTENT_SECURITY_MANAGER_DRM_GEN_ERR_NONE)
				{
					getContentProtectionAsVerboseErrorCode(errorCode,*statusCode,*reasonCode);
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
bool ContentProtectionFirebolt::OpenDrmSession(std::string& clientId, std::string appId, std::string keySystem, std::string licenseRequest, std::string initData, int64_t &sessionId, int32_t &errorCode, std::string &response)
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
		  errorCode =  static_cast<int>(drmSession.error());
		  MW_LOG_ERR("openDrmSession: Firebolt Error: \"%d\"", errorCode);
	}
	return ret;
}
bool ContentProtectionFirebolt::UpdateDrmSession(int64_t sessionId, int32_t &errorCode, std::string licenseRequest, std::string initData, std::string &response)
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
		errorCode =  static_cast<int>(drmSession.error());
		MW_LOG_ERR("updateDrmSession: Firebolt Error: \"%d\"", errorCode);
	}
	return ret;
}
