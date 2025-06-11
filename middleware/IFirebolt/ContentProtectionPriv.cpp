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

/**
 * @file ContentProtectionPriv.cpp
 * @brief Class impl for ContentProtectionBase
 */


#include "ContentProtectionPriv.h"
#include <string.h>
#include "_base64.h"
#include <inttypes.h> // For PRId64
#include "cJSON.h"
#include "PlayerJsonObject.h"
#include "ContentProtectionFirebolt.h"

/**
 * @brief Callback function pointer for sending watermark session events
 */
std::function<void(uint32_t, uint32_t, const std::string&)> ContentProtectionBase::SendWatermarkSessionEvent_CB;

/**
 * @brief Maximum attempts to retry license acquisition on recoverable errors
 */
#define MAX_LICENSE_REQUEST_ATTEMPT 2

/**
 * @brief Internal mutex for managing singleton lifecycle thread-safely when GetInstance() & DestroyInstance()
 */
static std::mutex InstanceMutex;

/*Event Handlers*/
#ifdef USE_SECMANAGER
//void watermarkSessionHandler(const PlayerJsonObject& parameters);
#endif

/**
 * @brief Sleep utility for retry delays
 * @param milliseconds Milliseconds to sleep
 */
void msec_sleep(int milliseconds)
{
	struct timespec req, rem;
	if (milliseconds > 0)
	{
		req.tv_sec = milliseconds / 1000;
		req.tv_nsec = (milliseconds % 1000) * 1000000;
		nanosleep(&req, &rem);
	}
}

std::shared_ptr<ContentProtectionSession::SessionManager> ContentProtectionSession::SessionManager::getInstance(int64_t sessionID, std::size_t inputSummaryHash)
{
	std::shared_ptr<SessionManager> returnValue;

	static std::mutex instancesMutex;
	std::lock_guard<std::mutex> lock{instancesMutex};
	static std::map<int64_t, std::weak_ptr<SessionManager>> instances;

	//Remove pointers to expired instances
	{
		std::vector<int64_t> keysToRemove;
		for (auto i : instances)
		{
			if(i.second.expired())
			{
				keysToRemove.push_back(i.first);
			}
		}
		if(keysToRemove.size())
		{
			std::stringstream ss;
			ss<<"ContentProtectionSession: "<<keysToRemove.size()<<" expired (";
			for(auto key:keysToRemove)
			{
				ss<<key<<",";
				instances.erase(key);
			}

			ss<<"), instances remaining."<< instances.size();
			AAMPLOG_MIL("%s",ss.str().c_str());
		}
	}

	/* Only create or retrieve instances for valid sessionIDs.
	 * <0 is used as an invalid value e.g. PLAYER_SECMGR_INVALID_SESSION_ID
	 * 0 removes all sessions which is not the intended behaviour here*/
	if(sessionID>0)
	{
		if(instances.count(sessionID)>0)
		{
			//get an existing pointer which may be no longer valid
			returnValue = instances[sessionID].lock();

			if(!returnValue)
			{
				//unexpected
				AAMPLOG_WARN("ContentProtectionSession: session ID %" PRId64 " reused or session closed too early.",
				sessionID);
			}
		}

		if(returnValue)
		{
			if(returnValue->getInputSummaryHash()!=inputSummaryHash)
			{
				//this should only occur after a successful updatePlaybackSession
				AAMPLOG_MIL("ContentProtectionSession: session ID %" PRId64 " input data changed.", sessionID);
				returnValue->setInputSummaryHash(inputSummaryHash);
			}
		}
		else
		{
			/* where an existing, valid instance is not available for sessionID
			* create a new instance & save a pointer to it for possible future reuse*/
			returnValue.reset(new SessionManager{sessionID, inputSummaryHash});
			instances[sessionID] = returnValue;
			AAMPLOG_WARN("ContentProtectionSession: new instance created for ID:%" PRId64 ", %zu instances total.",
			sessionID,
			instances.size());
		}
	}
	else
	{
		AAMPLOG_WARN("ContentProtectionSession: invalid ID:%" PRId64 ".", sessionID);
	}

	return returnValue;
}

ContentProtectionSession::SessionManager::~SessionManager()
{
	if(mID>0)
	{
		ContentProtection::GetInstance()->ReleaseSession(mID);
	}
}
void ContentProtectionSession::SessionManager::setInputSummaryHash(std::size_t inputSummaryHash)
{
	mInputSummaryHash=inputSummaryHash;
	std::stringstream ss;
	ss<<"Input summary hash updated to: "<<inputSummaryHash << "for ID "<<mID;
	AAMPLOG_MIL("%s", ss.str().c_str());
}


ContentProtectionSession::SessionManager::SessionManager(int64_t sessionID, std::size_t inputSummaryHash):
mID(sessionID),
mInputSummaryHash(inputSummaryHash)
{};

ContentProtectionSession::ContentProtectionSession(int64_t sessionID, std::size_t inputSummaryHash):
mpSessionManager(ContentProtectionSession::SessionManager::getInstance(sessionID, inputSummaryHash)),
sessionIdMutex()
{};

int64_t ContentProtectionSession::getSessionID(void) const
{
	std::lock_guard<std::mutex>lock(sessionIdMutex);
	int64_t ID = CONTENT_PROTECTION_INVALID_SESSION_ID;
	if(mpSessionManager)
	{
		ID = mpSessionManager->getID();
	}

	return ID;
}

std::size_t ContentProtectionSession::getInputSummaryHash()
{
	std::lock_guard<std::mutex>lock(sessionIdMutex);
	std::size_t hash=0;
	if(mpSessionManager)
	{
		hash = mpSessionManager->getInputSummaryHash();
	}

	return hash;
}

/**
 * @brief ContentProtectionBase Constructor
 */
ContentProtectionBase::ContentProtectionBase() :  mCPMutex(), mSchedulerStarted(false),
				   mRegisteredEvents()
{
	std::lock_guard<std::mutex> lock(mCPMutex);

	/*Start Scheduler for handling RDKShell API invocation*/    
	if(false == mSchedulerStarted)
	{
		StartScheduler();
		mSchedulerStarted = true;
	}
}

/**
 * @brief PlayerScheduler Destructor
 */
ContentProtectionBase::~ContentProtectionBase()
{
	std::lock_guard<std::mutex> lock(mCPMutex);

	/*Stop Scheduler used for handling RDKShell API invocation*/    
	if(true == mSchedulerStarted)
	{
		StopScheduler();
		mSchedulerStarted = false;
	}
}

/**
 * @brief Calculates a hash based on the input DRM/session metadata
 *
 * Used to determine whether session reuse is possible.
 *
 * @param moneyTraceMetdata Trace info (e.g. distributedTraceId)
 * @param contentMetdata Metadata describing the content
 * @param contMetaLen Length of content metadata
 * @param licenseRequest DRM challenge
 * @param keySystemId DRM system (e.g., widevine)
 * @param mediaUsage Usage context (e.g., streaming)
 * @param accessToken Auth token
 * @param isVideoMuted Whether content is muted (affects session state)
 * @return std::size_t Hash value of the session
 */
static std::size_t getInputSummaryHash(const char* moneyTraceMetdata[][2], const char* contentMetdata,
					size_t contMetaLen, const char* licenseRequest, const char* keySystemId,
					const char* mediaUsage, const char* accessToken, bool isVideoMuted)
{
	std::stringstream ss;
	ss<< moneyTraceMetdata[0][1]<<isVideoMuted<<//sessionConfiguration (only variables)
	//ignoring hard coded aspectDimensions 
	keySystemId<<mediaUsage<<accessToken<<contentMetdata<<licenseRequest;

	std::string InputSummary =  ss.str();

	auto returnHash = std::hash<std::string>{}(InputSummary);
	ss<<"ContentProtection input summary hash: "<<returnHash << "(" << InputSummary << ")";
	AAMPLOG_MIL("%s", ss.str().c_str());

	return returnHash;
}

bool ContentProtectionBase::AcquireLicense(std::string clientId, std::string appId, const char* licenseUrl, const char* moneyTraceMetdata[][2],
					const char* accessAttributes[][2], const char* contentMetdata, size_t contMetaLen,
					const char* licenseRequest, size_t licReqLen, const char* keySystemId,
					const char* mediaUsage, const char* accessToken, size_t accTokenLen,
					ContentProtectionSession &session,
					char** licenseResponse, size_t* licenseResponseLength, int32_t* statusCode, int32_t* reasonCode, int32_t* businessStatus, bool isVideoMuted, int sleepTime)
{
	bool success = false;

	auto inputSummaryHash = getInputSummaryHash(moneyTraceMetdata, contentMetdata,
					contMetaLen, licenseRequest, keySystemId,
					mediaUsage, accessToken, isVideoMuted);

	if(!session.isSessionValid())
	{
		AAMPLOG_MIL("%s, open new session.", session.ToString().c_str());
	}
	else if(inputSummaryHash==session.getInputSummaryHash())
	{
		AAMPLOG_MIL("%s, & input data matches, set session to active.", session.ToString().c_str());
		success = UpdateSessionState(session.getSessionID(), true);
	}
	else
	{
		AAMPLOG_MIL("%s but the input data has changed, update session.", session.ToString().c_str());
	}

	if(!success)
	{
		/*old AcquireLicense() code used for open & update (expected operation) and
		 * if setPlaybackSessionState doesn't have the expected result*/
		success = AcquireLicenseOpenOrUpdate( clientId, appId, licenseUrl, moneyTraceMetdata,
					accessAttributes, contentMetdata, contMetaLen,
					licenseRequest, licReqLen, keySystemId,
					mediaUsage, accessToken, accTokenLen,
					session,
					licenseResponse, licenseResponseLength, statusCode, reasonCode,
					businessStatus, isVideoMuted, sleepTime);
	}

	return success;
}


/**
 * @brief To acquire license from SecManager
 */
bool ContentProtectionBase::AcquireLicenseOpenOrUpdate( std::string clientId, std::string appId, const char* licenseUrl, const char* moneyTraceMetdata[][2],
		const char* accessAttributes[][2], const char* contentMetdata, size_t contMetaLen,
		const char* licenseRequest, size_t licReqLen, const char* keySystemId,
		const char* mediaUsage, const char* accessToken, size_t accTokenLen,
		ContentProtectionSession &session,
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
	*statusCode = CONTENT_PROTECTION_DRM_FAILURE;
	*reasonCode = CONTENT_PROTECTION_DRM_GEN_FAILURE;

#ifdef USE_SECMANAGER
	PlayerJsonObject param;
	PlayerJsonObject response;
	PlayerJsonObject sessionConfig;
	PlayerJsonObject aspectDimensions;
	const char* apiName = "OpenDrmSession";

	std::string keySystem = keySystemId ? keySystemId : "";
	std::string licenseRequestStr = licenseRequest
		? std::string(licenseRequest, licReqLen)
		: std::string();		

	// use .add() instead of operator[]
	sessionConfig.add("distributedTraceType", "money");
	sessionConfig.add("distributedTraceId", moneyTraceMetdata[0][1]);
	sessionConfig.add("sessionState", isVideoMuted ? "inactive" : "active");

	// width/height are numbers, but PlayerJsonObject's add expects strings -> so convert to string
	aspectDimensions.add("width", std::to_string(1920));
	aspectDimensions.add("height", std::to_string(1080));

	std::string mediaUsageStr = mediaUsage ? mediaUsage : "";

	std::string accessTokenStr = accessToken
		? std::string(accessToken, accTokenLen)
		: std::string();
	std::string contentMetadataStr = contentMetdata
		? std::string(contentMetdata, contMetaLen)
		: std::string();

	param.add("sessionConfiguration", sessionConfig);
	param.add("contentAspectDimensions", aspectDimensions);
	param.add("mediaUsage", mediaUsageStr);

	std::string sessionId;
	if (session.isSessionValid())
	{
		// If sessionId is present, we are trying to acquire a new license within the same session
		apiName = "UpdateDrmSession";
		sessionId = std::to_string(session.getSessionID());
		update = true;
	}

	{
		std::lock_guard<std::mutex> lock(mCPMutex);
		if (!accessTokenStr.empty() &&
				!contentMetadataStr.empty() &&
				!licenseRequestStr.empty())

		{
			AAMPLOG_INFO("Access token, Content metadata and license request are copied successfully, passing details with ContentProtection");

			//Set json params to be used by sec manager
			param.add("accessToken", accessTokenStr);
			param.add("contentMetadata", contentMetadataStr);

			std::string initData = param.print_UnFormatted();
			AAMPLOG_WARN("ContentProtection %s param: %s",apiName, initData.c_str());
			bool result = false;
			//invoke "openPlaybackSession" or "updatePlaybackSession" with retries for specific error cases
			do
			{
				std::string drmSession;
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
					AAMPLOG_WARN("openDrmSession Response is empty.");
					return false;
				}
				//cJSON* root = nullptr;
				if (result) 
				{
					ContentProtectionSession newSession;
					int id = 0;
					PlayerJsonObject sessionObj(drmSession);
					if(sessionObj.get("sessionId", id))
						AAMPLOG_WARN("11111SAMIIIII sessionId str %s",sessionId.c_str());
					AAMPLOG_WARN("11111SAMIIIII sessionId int %d",id);

					if(!drmSession.empty()) //TO-DO print
					{
						/*
						 * Ensure all sessions have a Session ID created to manage lifetime
						 * multiple object creation is OK as an existing instance should be returned
						 * where input data changes e.g. following a call to updatePlaybackSession
						 * the input data to the shared session is updated here
						 */

						newSession = ContentProtectionSession(id, 
								getInputSummaryHash(moneyTraceMetdata, contentMetdata,
									contMetaLen, licenseRequest, keySystemId,
									mediaUsage, accessToken, isVideoMuted));					

						std::string license;
						if (sessionObj.get("license", license))
						{
							AAMPLOG_TRACE("ContentProtection obtained license with length: %zu and data: %s", license.size(), license.c_str());

							if (!license.empty())
							{
								unsigned char* licenseDecoded = nullptr;
								size_t licenseDecodedLen = 0;

								licenseDecoded = base64_Decode(license.c_str(), &licenseDecodedLen);
								AAMPLOG_TRACE("ContentProtection license decoded len: %zu and data: %p", licenseDecodedLen, licenseDecoded);

								if (licenseDecoded != nullptr && licenseDecodedLen != 0)
								{
									*licenseResponse = (char*)malloc(licenseDecodedLen);
									if (*licenseResponse)
									{
										memcpy(*licenseResponse, licenseDecoded, licenseDecodedLen);
										*licenseResponseLength = licenseDecodedLen;

										AAMPLOG_INFO("ContentProtection license post base64 decode length: %zu", *licenseResponseLength);
									}
									else
									{
										AAMPLOG_ERR("ContentProtection failed to allocate memory for license!");
									}
									free(licenseDecoded);
									ret = true;
								}
								else
								{
									AAMPLOG_ERR("ContentProtection license base64 decode failed!");
								}
							}
						}

						if (newSession.isSessionValid() && !session.isSessionValid())
						{
							session = newSession;
						}
					}
				}
//TO-DO:Does DrmSession empty check required??
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

					AAMPLOG_WARN("ContentProtection Parsed Status Code: %d, Reason: %d, Business Status: %d",
							statusCode ? *statusCode : -1,
							reasonCode ? *reasonCode : -1,
							businessStatus ? *businessStatus : -1);
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
					if((*statusCode == CONTENT_PROTECTION_DRM_FAILURE || *statusCode == CONTENT_PROTECTION_WM_FAILURE) &&
							(*reasonCode == CONTENT_PROTECTION_SERVICE_TIMEOUT ||
							 *reasonCode == CONTENT_PROTECTION_SERVICE_CON_FAILURE ||
							 *reasonCode == CONTENT_PROTECTION_SERVICE_BUSY ) && retryCount < MAX_LICENSE_REQUEST_ATTEMPT)
					{
						retryCount;
						AAMPLOG_WARN("ContentProtection license request failed, response for %s : statusCode: %d, reasonCode: %d, so retrying with delay %d, retry count : %u", apiName, *statusCode, *reasonCode, sleepTime, retryCount );
						msec_sleep(sleepTime);						
					}
					else
					{
						AAMPLOG_ERR("ContentProtection license request failed, response for %s : statusCode: %d, reasonCode: %d", apiName, *statusCode, *reasonCode);
						break;
					}
				}
				else
				{
					AAMPLOG_INFO("ContentProtection license request success, response for %s : statusCode: %d, reasonCode: %d, session status: %s", apiName, *statusCode, *reasonCode, isVideoMuted ? "inactive" : "active");
					break;
				}
			}
			while(retryCount < MAX_LICENSE_REQUEST_ATTEMPT);
		}
		else
		{
			AAMPLOG_ERR("ContentProtection Failed to copy access token to the shared memory, %s is aborted statusCode: %d, reasonCode: %d", apiName, *statusCode, *reasonCode);
		}
	}
#endif
	return ret;
}

/**
 * @brief To update session state to SecManager
 */
bool ContentProtectionBase::UpdateSessionState(int64_t sessionId, bool active)
{
	bool result = false;
	result = SetDrmSessionState(std::to_string(sessionId), active);
	return result;
}

/**
 * @brief To notify SecManager to release a session
 */
void ContentProtectionBase::ReleaseSession(int64_t sessionId)
{
	CloseDrmSession(std::to_string(sessionId));
}

/**
 * @brief To set Playback Speed State to SecManager
 */
bool ContentProtectionBase::setPlaybackSpeedState(int64_t sessionId, float speed, int32_t position) 
{
	bool ret = false;
	ret = SetPlaybackPosition(std::to_string(sessionId), speed, position);
	return ret;
}

/**
 * @brief To get scheduler status
 */
bool ContentProtectionBase::getSchedulerStatus()
{
	return mSchedulerStarted;
}

/**
 * @brief To set Watermark Session callback
 */
void ContentProtectionBase::setWatermarkSessionEvent_CB(const std::function<void(uint32_t, uint32_t, const std::string&)>& callback)
{
	SendWatermarkSessionEvent_CB = callback;
	return;
}

/**
 * @brief To set Watermark Session callback
 */
std::function<void(uint32_t, uint32_t, const std::string&)>& ContentProtectionBase::getWatermarkSessionEvent_CB( )
{
	return SendWatermarkSessionEvent_CB;
}

/**
 * @brief Checks whether the Content Protection feature is enabled at compile-time
 * @return true if enabled via USE_SECMANAGER macro
 */
bool ContentProtection::ContentProtectionEnabled()
{
#if defined(USE_SECMANAGER)
    return true;
#else
    return false;
#endif
}
/**
 * @brief Singleton instance
 */
ContentProtectionBase *ContentProtection::mInstance = NULL;

/**
 *  @brief Get the singleton instance
 */
ContentProtectionBase *ContentProtection::GetInstance()
{
	std::lock_guard<std::mutex> lock{InstanceMutex};
	if (mInstance == nullptr)
	{
#if defined(USE_SECMANAGER)
		mInstance = new ContentProtectionFirebolt();
#else
		AAMPLOG_WARN("No subtec support on simulators. Creating a dummy instance!");
		mInstance = new FakeContentProtection();
#endif
	}
	return mInstance;
}

/**
 *  @brief Destroy instance
 */
void ContentProtection::DestroyInstance()
{
	std::lock_guard<std::mutex> lock{InstanceMutex};
	if (mInstance)
	{
		/* hide watermarking before secman shutdown */
		delete mInstance;
		mInstance = nullptr;
	}
}