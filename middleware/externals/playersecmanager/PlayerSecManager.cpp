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
 * @file PlayerSecManager.cpp
 * @brief Class impl for PlayerSecManager
 */

#include "PlayerSecManager.h"
#include "PlayerLogManager.h"
#include <string.h>
#include "_base64.h"
#include <inttypes.h> // For PRId64

static PlayerSecManager *Instance = nullptr; /**< singleton instance*/

std::function<void(uint32_t, uint32_t, const std::string&)> PlayerSecManager::SendWatermarkSessionEvent_CB;

/* mutex GetInstance() & DestroyInstance() to improve thread safety
 * There is still a race between using the pointer returned from GetInstance() and calling DestroyInstance()*/
static std::mutex InstanceMutex;

#define MAX_LICENSE_REQUEST_ATTEMPT 2

/*Event Handlers*/
#ifdef USE_CPP_THUNDER_PLUGIN_ACCESS
void watermarkSessionHandler(const JsonObject& parameters);
void addWatermarkHandler(const JsonObject& parameters);
void updateWatermarkHandler(const JsonObject& parameters);
void removeWatermarkHandler(const JsonObject& parameters);
void showWatermarkHandler(const JsonObject& parameters);
#endif

/**
 * @brief Sleep for given milliseconds
 */
void ms_sleep(int milliseconds)
{
	struct timespec req, rem;
	if (milliseconds > 0)
	{
		req.tv_sec = milliseconds / 1000;
		req.tv_nsec = (milliseconds % 1000) * 1000000;
		nanosleep(&req, &rem);
	}
}

std::shared_ptr<PlayerSecManagerSession::SessionManager> PlayerSecManagerSession::SessionManager::getInstance(int64_t sessionID, std::size_t inputSummaryHash)
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
			ss<<"PlayerSecManagerSession: "<<keysToRemove.size()<<" expired (";
			for(auto key:keysToRemove)
			{
				ss<<key<<",";
				instances.erase(key);
			}

			ss<<"), instances remaining."<< instances.size();
			MW_LOG_MIL("%s",ss.str().c_str());
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
				MW_LOG_WARN("PlayerSecManagerSession: session ID %" PRId64 " reused or session closed too early.",
				sessionID);
			}
		}

		if(returnValue)
		{
			if(returnValue->getInputSummaryHash()!=inputSummaryHash)
			{
				//this should only occur after a successful updatePlaybackSession
				MW_LOG_MIL("PlayerSecManagerSession: session ID %" PRId64 " input data changed.", sessionID);
				returnValue->setInputSummaryHash(inputSummaryHash);
			}
		}
		else
		{
			/* where an existing, valid instance is not available for sessionID
			* create a new instance & save a pointer to it for possible future reuse*/
			returnValue.reset(new SessionManager{sessionID, inputSummaryHash});
			instances[sessionID] = returnValue;
			MW_LOG_WARN("PlayerSecManagerSession: new instance created for ID:%" PRId64 ", %zu instances total.",
			sessionID,
			instances.size());
		}
	}
	else
	{
		MW_LOG_WARN("PlayerSecManagerSession: invalid ID:%" PRId64 ".", sessionID);
	}

	return returnValue;
}

PlayerSecManagerSession::SessionManager::~SessionManager()
{
	if(mID>0)
	{
		PlayerSecManager::GetInstance()->ReleaseSession(mID);
	}
}
void PlayerSecManagerSession::SessionManager::setInputSummaryHash(std::size_t inputSummaryHash)
{
	mInputSummaryHash=inputSummaryHash;
	std::stringstream ss;
	ss<<"Input summary hash updated to: "<<inputSummaryHash << "for ID "<<mID;
	MW_LOG_MIL("%s", ss.str().c_str());
}


PlayerSecManagerSession::SessionManager::SessionManager(int64_t sessionID, std::size_t inputSummaryHash):
mID(sessionID),
mInputSummaryHash(inputSummaryHash)
{};

PlayerSecManagerSession::PlayerSecManagerSession(int64_t sessionID, std::size_t inputSummaryHash):
mpSessionManager(PlayerSecManagerSession::SessionManager::getInstance(sessionID, inputSummaryHash)),
sessionIdMutex()
{};

int64_t PlayerSecManagerSession::getSessionID(void) const
{
	std::lock_guard<std::mutex>lock(sessionIdMutex);
	int64_t ID = PLAYER_SECMGR_INVALID_SESSION_ID;
	if(mpSessionManager)
	{
		ID = mpSessionManager->getID();
	}

	return ID;
}

std::size_t PlayerSecManagerSession::getInputSummaryHash()
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
 * @brief To get PlayerSecManager instance
 */
PlayerSecManager* PlayerSecManager::GetInstance()
{
	std::lock_guard<std::mutex> lock{InstanceMutex};
#ifdef USE_CPP_THUNDER_PLUGIN_ACCESS
	if (Instance == nullptr)
	{
		Instance = new PlayerSecManager();
	}
#endif
	return Instance;
}

/**
 * @brief To release PlayerSecManager singelton instance
 */
void PlayerSecManager::DestroyInstance()
{
	std::lock_guard<std::mutex> lock{InstanceMutex};
#ifdef USE_CPP_THUNDER_PLUGIN_ACCESS
	if (Instance)
	{
		/* hide watermarking before SecManager shutdown */
		Instance->ShowWatermark(false);
		delete Instance;
		Instance = nullptr;
	}
#endif
}

/**
 * @brief PlayerScheduler Constructor
 */
PlayerSecManager::PlayerSecManager() : mSecManagerObj(SECMANAGER_CALL_SIGN), mSecMutex(), mSchedulerStarted(false),
				   mRegisteredEvents(), mWatermarkPluginObj(WATERMARK_PLUGIN_CALLSIGN), mWatMutex(), mSpeedStateMutex()
{
	std::lock_guard<std::mutex> lock(mSecMutex);
	mSecManagerObj.ActivatePlugin();
	{
		std::lock_guard<std::mutex> lock(mWatMutex);
		mWatermarkPluginObj.ActivatePlugin();
	}

	/* hide watermarking at startup */
	ShowWatermark(false);

	/*Start Scheduler for handling RDKShell API invocation*/
	if(false == mSchedulerStarted)
	{
		StartScheduler(); // pass dummy required playerId parameter; note that we don't yet have a valid player instance to derive it from
		mSchedulerStarted = true;
	}
#ifdef USE_CPP_THUNDER_PLUGIN_ACCESS
	/*
	* Release any unexpectedly open sessions.
	* These could exist if a previous player session crashed
	* 'firstInstance' check is a defensive measure allowing for the usage of
	* GetInstance() & DestroyInstance() to change in the future
	* InstanceMutex use in GetInstance() makes this thread safe
	* (currently mSecMutex is also locked but ideally the scope of this would be reduced)*/
	static bool firstInstance = true;
	if(firstInstance)
	{
		firstInstance=false;
		JsonObject result;
		JsonObject param;
		param["clientId"] = "player";
		param["sessionId"] = 0; //Instructs closePlaybackSession to close all open 'player' sessions
		MW_LOG_WARN("SecManager call closePlaybackSession to ensure no old sessions exist.");
		bool rpcResult = mSecManagerObj.InvokeJSONRPC("closePlaybackSession", param, result);

		if (rpcResult && result["success"].Boolean())
		{
			MW_LOG_WARN("old SecManager sessions removed");
		}
		else
		{
			MW_LOG_WARN("no old SecManager sessions to remove");
		}
	}

	RegisterAllEvents();
#endif
}

/**
 * @brief PlayerScheduler Destructor
 */
PlayerSecManager::~PlayerSecManager()
{
	std::lock_guard<std::mutex> lock(mSecMutex);

	/*Stop Scheduler used for handling RDKShell API invocation*/
	if(true == mSchedulerStarted)
	{
		StopScheduler();
		mSchedulerStarted = false;
	}

	UnRegisterAllEvents();
}

#ifdef USE_CPP_THUNDER_PLUGIN_ACCESS
static std::size_t getInputSummaryHash(const char* moneyTraceMetadata[][2], const char* contentMetadata,
					size_t contMetaLen, const char* licenseRequest, const char* keySystemId,
					const char* mediaUsage, const char* accessToken, bool isVideoMuted)
{
	std::stringstream ss;
	ss<< moneyTraceMetadata[0][1]<<isVideoMuted<<//sessionConfiguration (only variables)
	//ignoring hard coded aspectDimensions 
	keySystemId<<mediaUsage<<accessToken<<contentMetadata<<licenseRequest;

	std::string InputSummary =  ss.str();

	auto returnHash = std::hash<std::string>{}(InputSummary);
	ss<<"SecManager input summary hash: "<<returnHash << "(" << InputSummary << ")";
	MW_LOG_MIL("%s", ss.str().c_str());

	return returnHash;
}
#endif

bool PlayerSecManager::AcquireLicense( const char* licenseUrl, const char* moneyTraceMetadata[][2],
					const char* accessAttributes[][2], const char* contentMetadata, size_t contMetaLen,
					const char* licenseRequest, size_t licReqLen, const char* keySystemId,
					const char* mediaUsage, const char* accessToken, size_t accTokenLen,
					PlayerSecManagerSession &session,
					char** licenseResponse, size_t* licenseResponseLength, int32_t* statusCode, int32_t* reasonCode, int32_t* businessStatus, bool isVideoMuted, int sleepTime)
{
	bool success = false;
#ifdef USE_CPP_THUNDER_PLUGIN_ACCESS
	auto inputSummaryHash = getInputSummaryHash(moneyTraceMetadata, contentMetadata,
					contMetaLen, licenseRequest, keySystemId,
					mediaUsage, accessToken, isVideoMuted);

	if(!session.isSessionValid())
	{
		MW_LOG_MIL("%s, open new session.", session.ToString().c_str());
	}
	else if(inputSummaryHash==session.getInputSummaryHash())
	{
		MW_LOG_MIL("%s, & input data matches, set session to active.", session.ToString().c_str());
		success = UpdateSessionState(session.getSessionID(), true);
	}
	else
	{
		MW_LOG_MIL("%s but the input data has changed, update session.", session.ToString().c_str());
	}

	if(!success)
	{
		/*old AcquireLicense() code used for open & update (expected operation) and
		 * if setPlaybackSessionState doesn't have the expected result*/
		success = AcquireLicenseOpenOrUpdate( licenseUrl, moneyTraceMetadata,
					accessAttributes, contentMetadata, contMetaLen,
					licenseRequest, licReqLen, keySystemId,
					mediaUsage, accessToken, accTokenLen,
					session,
					licenseResponse, licenseResponseLength, statusCode, reasonCode,
					businessStatus, isVideoMuted, sleepTime);
	}
#endif
	return success;
}


/**
 * @brief To acquire license from SecManager
 */
bool PlayerSecManager::AcquireLicenseOpenOrUpdate( const char* licenseUrl, const char* moneyTraceMetadata[][2],
					const char* accessAttributes[][2], const char* contentMetadata, size_t contMetaLen,
					const char* licenseRequest, size_t licReqLen, const char* keySystemId,
					const char* mediaUsage, const char* accessToken, size_t accTokenLen,
					PlayerSecManagerSession &session,
					char** licenseResponse, size_t* licenseResponseLength, int32_t* statusCode, int32_t* reasonCode, int32_t* businessStatus, bool isVideoMuted, int sleepTime)
{
	// licenseUrl un-used now
	(void) licenseUrl;

	bool ret = false;
	//Initializing it with default error codes (which would be sent if there any jsonRPC
	//call failures to thunder)
	*statusCode = SECMANAGER_DRM_FAILURE;
	*reasonCode = SECMANAGER_DRM_GEN_FAILURE;

#ifdef USE_CPP_THUNDER_PLUGIN_ACCESS
	bool rpcResult = false;
	unsigned int retryCount = 0;
	//Shared memory pointer, key declared here,
	//Access token, content metadata and licence request will be passed to
	//secmanager via shared memory
	void * shmPt_accToken = nullptr;
	key_t shmKey_accToken = 0;
	void * shmPt_contMeta = nullptr;
	key_t shmKey_contMeta = 0;
	void * shmPt_licReq = nullptr;
	key_t shmKey_licReq = 0;
	const char* apiName = "openPlaybackSession";
	JsonObject param;
	JsonObject response;
	JsonObject sessionConfig;
	JsonObject aspectDimensions;

	sessionConfig["distributedTraceType"] = "money";
	sessionConfig["distributedTraceId"] = moneyTraceMetadata[0][1];
	//Start the playback session as inactive if the video mute is on
	sessionConfig["sessionState"] = isVideoMuted ? "inactive" : "active";
	// TODO: Remove hardcoded values
	aspectDimensions["width"] = 1920;
	aspectDimensions["height"] = 1080;

	param["clientId"] = "player";
	param["sessionConfiguration"] = sessionConfig;
	param["contentAspectDimensions"] = aspectDimensions;

	param["keySystem"] = keySystemId;
	param["mediaUsage"] = mediaUsage;

	// If sessionId is present, we are trying to acquire a new license within the same session
	if (session.isSessionValid())
	{
		apiName = "updatePlaybackSession";
		param["sessionId"] = session.getSessionID();
	}

#ifdef DEBUG_SECMAMANER
	std::string params;
	param.ToString(params);
	MW_LOG_WARN("SecManager %s param: %s",apiName, params.c_str());
#endif

	{
		std::lock_guard<std::mutex> lock(mSecMutex);
		if(accTokenLen > 0 && contMetaLen > 0 && licReqLen > 0)
		{
			shmPt_accToken = player_CreateSharedMem(accTokenLen, shmKey_accToken);
			shmPt_contMeta = player_CreateSharedMem(contMetaLen, shmKey_contMeta);
			shmPt_licReq = player_CreateSharedMem(licReqLen, shmKey_licReq);
		}

		//Set shared memory with the buffer
		//Set shared memory with the buffer
		if(nullptr != shmPt_accToken && nullptr != accessToken &&
			nullptr != shmPt_contMeta && nullptr != contentMetadata &&
		   nullptr != shmPt_licReq && nullptr != licenseRequest)
		{			
			//copy buffer to shm
			memcpy(shmPt_accToken, accessToken, accTokenLen);
			memcpy(shmPt_contMeta, contentMetadata, contMetaLen);
			memcpy(shmPt_licReq, licenseRequest, licReqLen);

			MW_LOG_INFO("Access token, Content metadata and license request are copied successfully, passing details with SecManager");
			//Set json params to be used by sec manager
			param["accessTokenBufferKey"] = shmKey_accToken;
			param["accessTokenLength"] = accTokenLen;

			param["contentMetadataBufferKey"] = shmKey_contMeta;
			param["contentMetadataLength"] = contMetaLen;

			param["licenseRequestBufferKey"] = shmKey_licReq;
			param["licenseRequestLength"] = licReqLen;

			//invoke "openPlaybackSession" or "updatePlaybackSession" with retries for specific error cases
			do
			{
				rpcResult = mSecManagerObj.InvokeJSONRPC(apiName, param, response, 10000);
				if (rpcResult)
				{
					PlayerSecManagerSession newSession;

				#ifdef DEBUG_SECMAMANER
					std::string output;
					response.ToString(output);
					MW_LOG_WARN("SecManager %s o/p: %s",apiName, output.c_str());
				#endif
					if (response["success"].Boolean())
					{
						/*
						* Ensure all sessions have a Session ID created to manage lifetime
						* multiple object creation is OK as an existing instance should be returned
						* where input data changes e.g. following a call to updatePlaybackSession
						* the input data to the shared session is updated here*/
						newSession = PlayerSecManagerSession(response["sessionId"].Number(), 
						getInputSummaryHash(moneyTraceMetadata, contentMetadata,
						contMetaLen, licenseRequest, keySystemId,
						mediaUsage, accessToken, isVideoMuted));

						std::string license = response["license"].String();
						MW_LOG_TRACE("SecManager obtained license with length: %d and data: %s",license.size(), license.c_str());
						if (!license.empty())
						{
							// Here license is base64 encoded
							unsigned char * licenseDecoded = nullptr;
							size_t licenseDecodedLen = 0;
							licenseDecoded = base64_Decode(license.c_str(), &licenseDecodedLen);
							MW_LOG_TRACE("SecManager license decoded len: %d and data: %p", licenseDecodedLen, licenseDecoded);

							if (licenseDecoded != nullptr && licenseDecodedLen != 0)
							{
								MW_LOG_INFO("SecManager license post base64 decode length: %d", *licenseResponseLength);
								*licenseResponse = (char*) malloc(licenseDecodedLen);
								if (*licenseResponse)
								{
									memcpy(*licenseResponse, licenseDecoded, licenseDecodedLen);
									*licenseResponseLength = licenseDecodedLen;
								}
								else
								{
									MW_LOG_ERR("SecManager failed to allocate memory for license!");
								}
								free(licenseDecoded);
								ret = true;
							}
							else
							{
								MW_LOG_ERR("SecManager license base64 decode failed!");
							}
						}
					}

					// Save session ID
					if (newSession.isSessionValid() && !session.isSessionValid())
					{
						session = newSession;
					}

				}
				// TODO: Sort these values out for backward compatibility
				if(response.HasLabel("secManagerResultContext"))
				{
					JsonObject resultContext = response["secManagerResultContext"].Object();

					if(resultContext.HasLabel("class"))
						*statusCode = resultContext["class"].Number();
					if(resultContext.HasLabel("reason"))
						*reasonCode = resultContext["reason"].Number();
					if(resultContext.HasLabel("businessStatus"))
						*businessStatus = resultContext["businessStatus"].Number();
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
					if((*statusCode == SECMANAGER_DRM_FAILURE || *statusCode == SECMANAGER_WM_FAILURE) &&
					   (*reasonCode == SECMANAGER_SERVICE_TIMEOUT ||
						*reasonCode == SECMANAGER_SERVICE_CON_FAILURE ||
						*reasonCode == SECMANAGER_SERVICE_BUSY ) && retryCount < MAX_LICENSE_REQUEST_ATTEMPT)
					{
						++retryCount;
						MW_LOG_WARN("SecManager license request failed, response for %s : statusCode: %d, reasonCode: %d, so retrying with delay %d, retry count : %u", apiName, *statusCode, *reasonCode, sleepTime, retryCount );
						ms_sleep(sleepTime);						
					}
					else
					{
						MW_LOG_ERR("SecManager license request failed, response for %s : statusCode: %d, reasonCode: %d", apiName, *statusCode, *reasonCode);
						break;
					}
				}
				else
				{
					MW_LOG_INFO("SecManager license request success, response for %s : statusCode: %d, reasonCode: %d, session status: %s", apiName, *statusCode, *reasonCode, isVideoMuted ? "inactive" : "active");
					break;
				}
			}
			while(retryCount < MAX_LICENSE_REQUEST_ATTEMPT);

			//Cleanup the shared memory after sharing it with secmanager
			player_CleanUpSharedMem( shmPt_accToken, shmKey_accToken, accTokenLen);
			player_CleanUpSharedMem( shmPt_contMeta, shmKey_contMeta, contMetaLen);
			player_CleanUpSharedMem( shmPt_licReq, shmKey_licReq, licReqLen);

		}
		else
		{
			MW_LOG_ERR("SecManager Failed to copy access token to the shared memory, %s is aborted statusCode: %d, reasonCode: %d", apiName, *statusCode, *reasonCode);
		}
	}
#endif
	return ret;
}

/**
 * @brief To update session state to SecManager
 */
bool PlayerSecManager::UpdateSessionState(int64_t sessionId, bool active)
{
	bool success = false;
#ifdef USE_CPP_THUNDER_PLUGIN_ACCESS
	bool rpcResult = false;
	JsonObject result;
	JsonObject param;
	param["clientId"] = "player";
	param["sessionId"] = sessionId;
	MW_LOG_INFO("%s:%d SecManager call setPlaybackSessionState for ID: %" PRId64 " and state: %d", __FUNCTION__, __LINE__, sessionId, active);
	if (active)
	{
		param["sessionState"] = "active";
	}
	else
	{
		param["sessionState"] = "inactive";
	}

	{
		std::lock_guard<std::mutex> lock(mSecMutex);
		rpcResult = mSecManagerObj.InvokeJSONRPC("setPlaybackSessionState", param, result);
	}

	if (rpcResult)
	{
		if (result["success"].Boolean())
		{
			success = true;
		}
		else
		{
			std::string responseStr;
			result.ToString(responseStr);
			MW_LOG_ERR("%s:%d SecManager setPlaybackSessionState failed for ID: %" PRId64 ", active:%d and result: %s", __FUNCTION__, __LINE__, sessionId, active, responseStr.c_str());
		}
	}
	else
	{
		MW_LOG_ERR("%s:%d SecManager setPlaybackSessionState failed for ID: %" PRId64 " and active: %d", __FUNCTION__, __LINE__, sessionId, active);
	}
#endif
	return success;
}

/**
 * @brief To notify SecManager to release a session
 */
void PlayerSecManager::ReleaseSession(int64_t sessionId)
{
#ifdef USE_CPP_THUNDER_PLUGIN_ACCESS
	bool rpcResult = false;
	JsonObject result;
	JsonObject param;
	param["clientId"] = "player";
	param["sessionId"] = sessionId;
	MW_LOG_INFO("%s:%d SecManager call closePlaybackSession for ID: %" PRId64 "", __FUNCTION__, __LINE__, sessionId);

	{
		std::lock_guard<std::mutex> lock(mSecMutex);
		rpcResult = mSecManagerObj.InvokeJSONRPC("closePlaybackSession", param, result);
	}

	if (rpcResult)
	{
		if (!result["success"].Boolean())
		{
			std::string responseStr;
			result.ToString(responseStr);
			MW_LOG_ERR("%s:%d SecManager closePlaybackSession failed for ID: %" PRId64 " and result: %s", __FUNCTION__, __LINE__, sessionId, responseStr.c_str());
		}
	}
	else
	{
		MW_LOG_ERR("%s:%d SecManager closePlaybackSession failed for ID: %" PRId64 "", __FUNCTION__, __LINE__, sessionId);
	}
#endif
}

/**
 * @brief To update session state to SecManager
 */
bool PlayerSecManager::setVideoWindowSize(int64_t sessionId, int64_t video_width, int64_t video_height)
{
       bool rpcResult = false;
#ifdef USE_CPP_THUNDER_PLUGIN_ACCESS
       JsonObject result;
       JsonObject param;

       param["sessionId"] = sessionId;
       param["videoWidth"] = video_width;
       param["videoHeight"] = video_height;

       MW_LOG_INFO("%s:%d SecManager call setVideoWindowSize for ID: %" PRId64 "", __FUNCTION__, __LINE__, sessionId);
       {
               std::lock_guard<std::mutex> lock(mSecMutex);
               rpcResult = mSecManagerObj.InvokeJSONRPC("setVideoWindowSize", param, result);
       }

       if (rpcResult)
       {
               if (!result["success"].Boolean())
               {
                       std::string responseStr;
                       result.ToString(responseStr);
                       MW_LOG_ERR("%s:%d SecManager setVideoWindowSize failed for ID: %" PRId64 " and result: %s", __FUNCTION__, __LINE__, sessionId, responseStr.c_str());
                       rpcResult = false;
               }

       }
       else
       {
		MW_LOG_ERR("%s:%d SecManager setVideoWindowSize failed for ID: %" PRId64 "", __FUNCTION__, __LINE__, sessionId);
       }
#endif
       return rpcResult;
}

/**
 * @brief To set Playback Speed State to SecManager
 */
bool PlayerSecManager::setPlaybackSpeedState(int64_t sessionId, int64_t playback_speed, int64_t playback_position)
{
       bool rpcResult = false;
#ifdef USE_CPP_THUNDER_PLUGIN_ACCESS
       JsonObject result;
       JsonObject param;
       //mSpeedStateMutex is used to avoid any speedstate event to go when a delayed event is in progress results change in order of event call (i.e, if user tries a trickplay within half a second of tune)
       std::lock_guard<std::mutex> lock(mSpeedStateMutex);

       param["sessionId"] = sessionId;
       param["playbackSpeed"] = playback_speed;
       param["playbackPosition"] = playback_position;

       MW_LOG_INFO("%s:%d SecManager call setPlaybackSpeedState for ID: %" PRId64 "", __FUNCTION__, __LINE__, sessionId);

       {
               std::lock_guard<std::mutex> lock(mSecMutex);
               rpcResult = mSecManagerObj.InvokeJSONRPC("setPlaybackSpeedState", param, result);
       }

       if (rpcResult)
       {
               if (!result["success"].Boolean())
               {
                       std::string responseStr;
                       result.ToString(responseStr);
                       MW_LOG_ERR("%s:%d SecManager setPlaybackSpeedState failed for ID: %" PRId64 " and result: %s", __FUNCTION__, __LINE__, sessionId, responseStr.c_str());
                       rpcResult = false;
               }
       }
       else
       {
			MW_LOG_ERR("%s:%d SecManager setPlaybackSpeedState failed for ID: %" PRId64 "", __FUNCTION__, __LINE__, sessionId);
       }
#endif
       return rpcResult;
}


/**
 * @brief To Load ClutWatermark
 */
bool PlayerSecManager::loadClutWatermark(int64_t sessionId, int64_t graphicId, int64_t watermarkClutBufferKey, int64_t watermarkImageBufferKey, int64_t clutPaletteSize,
                                                                       const char* clutPaletteFormat, int64_t watermarkWidth, int64_t watermarkHeight, float aspectRatio)
{
       bool rpcResult = false;
#ifdef USE_CPP_THUNDER_PLUGIN_ACCESS
      JsonObject result;
       JsonObject param;

       param["sessionId"] = sessionId;
       param["graphicId"] = graphicId;
       param["watermarkClutBufferKey"] = watermarkClutBufferKey;
       param["watermarkImageBufferKey"] = watermarkImageBufferKey;
       param["clutPaletteSize"] = clutPaletteSize;
       param["clutPaletteFormat"] = clutPaletteFormat;
       param["watermarkWidth"] = watermarkWidth;
       param["watermarkHeight"] = watermarkHeight;
       param["aspectRatio"] = std::to_string(aspectRatio).c_str();

       MW_LOG_INFO("%s:%d SecManager call loadClutWatermark for ID: %" PRId64 "", __FUNCTION__, __LINE__, sessionId);

       {
               std::lock_guard<std::mutex> lock(mSecMutex);
               rpcResult = mSecManagerObj.InvokeJSONRPC("loadClutWatermark", param, result);
       }

       if (rpcResult)
       {
               if (!result["success"].Boolean())
               {
                       std::string responseStr;
                       result.ToString(responseStr);
                       MW_LOG_ERR("%s:%d SecManager loadClutWatermark failed for ID: %" PRId64 " and result: %s", __FUNCTION__, __LINE__, sessionId, responseStr.c_str());
                       rpcResult = false;
               }
       }
       else
       {
               MW_LOG_ERR("%s:%d SecManager loadClutWatermark failed for ID: %" PRId64 "", __FUNCTION__, __LINE__, sessionId);
       }
#endif
       return rpcResult;

}

/**
 * @brief  Registers all Events to input plugin
 */
void PlayerSecManager::RegisterAllEvents ()
{
#ifdef USE_CPP_THUNDER_PLUGIN_ACCESS
	bool bSubscribed = false;
	std::function<void(const WPEFramework::Core::JSON::VariantContainer&)> watermarkSessionMethod = std::bind(watermarkSessionHandler, std::placeholders::_1) ;

	bSubscribed = mSecManagerObj.SubscribeEvent(_T("onWatermarkSession"), watermarkSessionMethod);
	if(bSubscribed)
		mRegisteredEvents.push_back("onWatermarkSession");

	std::function<void(const WPEFramework::Core::JSON::VariantContainer&)> addWatermarkMethod = std::bind(addWatermarkHandler, std::placeholders::_1);
	bSubscribed = mSecManagerObj.SubscribeEvent(_T("onAddWatermark"), addWatermarkMethod);
	if(bSubscribed)
		mRegisteredEvents.push_back("onAddWatermark");

	std::function<void(const WPEFramework::Core::JSON::VariantContainer&)> updateWatermarkMethod = std::bind(updateWatermarkHandler, std::placeholders::_1);
	bSubscribed = mSecManagerObj.SubscribeEvent(_T("onUpdateWatermark"), updateWatermarkMethod);
	if(bSubscribed)
		mRegisteredEvents.push_back("onUpdateWatermark");

	std::function<void(const WPEFramework::Core::JSON::VariantContainer&)> removeWatermarkMethod = std::bind(removeWatermarkHandler, std::placeholders::_1);
	bSubscribed = mSecManagerObj.SubscribeEvent(_T("onRemoveWatermark"), removeWatermarkMethod);
	if(bSubscribed)
		mRegisteredEvents.push_back("onRemoveWatermark");

	std::function<void(const WPEFramework::Core::JSON::VariantContainer&)> showWatermarkMethod = std::bind(showWatermarkHandler, std::placeholders::_1);
	bSubscribed = mSecManagerObj.SubscribeEvent(_T("onDisplayWatermark"), showWatermarkMethod);
	if(bSubscribed)
		mRegisteredEvents.push_back("onDisplayWatermark");
#endif
}

/**
 * @brief  UnRegisters all Events from plugin
 */
void PlayerSecManager::UnRegisterAllEvents ()
{
#ifdef USE_CPP_THUNDER_PLUGIN_ACCESS
	for (auto const& evtName : mRegisteredEvents) {
		mSecManagerObj.UnSubscribeEvent(_T(evtName));
	}
	mRegisteredEvents.clear();
#endif
}

/**
 * @brief To get scheduler status
 */
bool PlayerSecManager::getSchedulerStatus ()
{
	return mSchedulerStarted;
}

/**
 * @brief To set Watermark Session callback
 */
void PlayerSecManager::setWatermarkSessionEvent_CB(const std::function<void(uint32_t, uint32_t, const std::string&)>& callback)
{
	SendWatermarkSessionEvent_CB = callback;
	return;
}

/**
 * @brief To set Watermark Session callback
 */
std::function<void(uint32_t, uint32_t, const std::string&)>& PlayerSecManager::getWatermarkSessionEvent_CB( )
{
	return SendWatermarkSessionEvent_CB;
}

#ifdef USE_CPP_THUNDER_PLUGIN_ACCESS
/**
 * @brief  Detects watermarking session conditions
 */
void watermarkSessionHandler(const JsonObject& parameters)
{
	std::string param;
	parameters.ToString(param);
	MW_LOG_WARN("PlayerSecManager::%s:%d i/p params: %s", __FUNCTION__, __LINE__, param.c_str());
 	std::function<void(uint32_t, uint32_t, const std::string&)> sendWatermarkEvent_CB = PlayerSecManager::getWatermarkSessionEvent_CB();
	if (nullptr != sendWatermarkEvent_CB)
	{
		sendWatermarkEvent_CB( parameters["sessionId"].Number(),parameters["conditionContext"].Number(),parameters["watermarkingSystem"].String());
	}
}

/**
 * @brief  Gets watermark image details and manages watermark rendering
 */
void addWatermarkHandler(const JsonObject& parameters)
{
#ifdef DEBUG_SECMAMANER
	std::string param;
	parameters.ToString(param);

	MW_LOG_WARN("PlayerSecManager::%s:%d i/p params: %s", __FUNCTION__, __LINE__, param.c_str());
#endif
	if(PlayerSecManager::GetInstance()->getSchedulerStatus())
	{
		int graphicId = parameters["graphicId"].Number();
		int zIndex = parameters["zIndex"].Number();
		MW_LOG_WARN("PlayerSecManager::%s:%d graphicId : %d index : %d ", __FUNCTION__, __LINE__, graphicId, zIndex);
		PlayerSecManager::GetInstance()->ScheduleTask(PlayerAsyncTaskObj([graphicId, zIndex](void *data)
					  {
						PlayerSecManager *instance = static_cast<PlayerSecManager *>(data);
						instance->CreateWatermark(graphicId, zIndex);
					  }, (void *) PlayerSecManager::GetInstance()));

		int smKey = parameters["graphicImageBufferKey"].Number();
		int smSize = parameters["graphicImageSize"].Number();/*ToDo : graphicImageSize (long) long conversion*/
		MW_LOG_WARN("PlayerSecManager::%s:%d graphicId : %d smKey: %d smSize: %d", __FUNCTION__, __LINE__, graphicId, smKey, smSize);
		PlayerSecManager::GetInstance()->ScheduleTask(PlayerAsyncTaskObj([graphicId, smKey, smSize](void *data)
					  {
						PlayerSecManager *instance = static_cast<PlayerSecManager *>(data);
						instance->UpdateWatermark(graphicId, smKey, smSize);
					  }, (void *)PlayerSecManager::GetInstance()));
		if (parameters["adjustVisibilityRequired"].Boolean())
		{
			int sessionId = parameters["sessionId"].Number();
			MW_LOG_WARN("PlayerSecManager::%s:%d adjustVisibilityRequired is true, invoking GetWaterMarkPalette. graphicId : %d", __FUNCTION__, __LINE__, graphicId);
			PlayerSecManager::GetInstance()->ScheduleTask(PlayerAsyncTaskObj([sessionId, graphicId](void *data)
									  {
				PlayerSecManager *instance = static_cast<PlayerSecManager *>(data);
				instance->GetWaterMarkPalette(sessionId, graphicId);
			}, (void *)PlayerSecManager::GetInstance()));
		}
		else
		{
			MW_LOG_WARN("PlayerSecManager::%s:%d adjustVisibilityRequired is false, graphicId : %d", __FUNCTION__, __LINE__, graphicId);
		}

#if 0
		/*Method to be called only if RDKShell is used for rendering*/
		PlayerSecManager::GetInstance()->ScheduleTask(PlayerAsyncTaskObj([show](void *data)
					  {
						PlayerSecManager *instance = static_cast<PlayerSecManager *>(data);
						instance->AlwaysShowWatermarkOnTop(show);
					  }, (void *)PlayerSecManager::GetInstance()));
#endif
	}
	return;
}

/**
 * @brief  Gets updated watermark image details and manages watermark rendering
 */
void updateWatermarkHandler(const JsonObject& parameters)
{
	if(PlayerSecManager::GetInstance()->getSchedulerStatus())
	{
		int graphicId = parameters["graphicId"].Number();
		int clutKey = parameters["watermarkClutBufferKey"].Number();
		int imageKey = parameters["watermarkImageBufferKey"].Number();
		MW_LOG_TRACE("graphicId : %d ",graphicId);
		PlayerSecManager::GetInstance()->ScheduleTask(PlayerAsyncTaskObj([graphicId, clutKey, imageKey](void *data)
								  {
			PlayerSecManager *instance = static_cast<PlayerSecManager *>(data);
			instance->ModifyWatermarkPalette(graphicId, clutKey, imageKey);
		}, (void *) PlayerSecManager::GetInstance()));
	}
	return;
}

/**
 * @brief  Removes watermark image
 */
void removeWatermarkHandler(const JsonObject& parameters)
{
#ifdef DEBUG_SECMAMANER
	std::string param;
	parameters.ToString(param);
	MW_LOG_WARN("PlayerSecManager::%s:%d i/p params: %s", __FUNCTION__, __LINE__, param.c_str());
#endif
	if(PlayerSecManager::GetInstance()->getSchedulerStatus())
	{
		int graphicId = parameters["graphicId"].Number();
		MW_LOG_WARN("PlayerSecManager::%s:%d graphicId : %d ", __FUNCTION__, __LINE__, graphicId);
		PlayerSecManager::GetInstance()->ScheduleTask(PlayerAsyncTaskObj([graphicId](void *data)
					  {
						PlayerSecManager *instance = static_cast<PlayerSecManager *>(data);
						instance->DeleteWatermark(graphicId);
					  }, (void *) PlayerSecManager::GetInstance()));
#if 0
		/*Method to be called only if RDKShell is used for rendering*/
		PlayerSecManager::GetInstance()->ScheduleTask(PlayerAsyncTaskObj([show](void *data)
					  {
						PlayerSecManager *instance = static_cast<PlayerSecManager *>(data);
						instance->AlwaysShowWatermarkOnTop(show);
					  }, (void *) PlayerSecManager::GetInstance()));
#endif
	}
	return;
}

/**
 * @brief Handles watermark calls to be only once
 */
void showWatermarkHandler(const JsonObject& parameters)
{
	bool show = true;
	if(parameters["hideWatermark"].Boolean())
	{
		show = false;
	}
	MW_LOG_INFO("Received onDisplayWatermark, show: %d ", show);
	if(PlayerSecManager::GetInstance()->getSchedulerStatus())
	{
		PlayerSecManager::GetInstance()->ScheduleTask(PlayerAsyncTaskObj([show](void *data)
		{
			PlayerSecManager *instance = static_cast<PlayerSecManager *>(data);
			instance->ShowWatermark(show);
		}, (void *) PlayerSecManager::GetInstance()));
	}
	return;
}
#endif

/**
 * @brief Show watermark image
 */
void PlayerSecManager::ShowWatermark(bool show)
{
#ifdef USE_CPP_THUNDER_PLUGIN_ACCESS
	JsonObject param;
	JsonObject result;
	bool rpcResult = false;

	MW_LOG_ERR("PlayerSecManager %s:%d ", __FUNCTION__, __LINE__);
	param["show"] = show;
	{
		std::lock_guard<std::mutex> lock(mWatMutex);
		rpcResult =  mWatermarkPluginObj.InvokeJSONRPC("showWatermark", param, result);
	}
	if (rpcResult)
	{
		if (!result["success"].Boolean())
		{
			std::string responseStr;
			result.ToString(responseStr);
			MW_LOG_ERR("PlayerSecManager::%s failed and result: %s", __FUNCTION__, responseStr.c_str());
		}
	}
	else
	{
		MW_LOG_ERR("PlayerSecManager::%s thunder invocation failed!", __FUNCTION__);
	}
#endif
	return;
}

/**
 *  @brief Create Watermark
 */
void PlayerSecManager::CreateWatermark(int graphicId, int zIndex )
{
#ifdef USE_CPP_THUNDER_PLUGIN_ACCESS
	JsonObject param;
	JsonObject result;
	bool rpcResult = false;

	MW_LOG_ERR("PlayerSecManager %s:%d ", __FUNCTION__, __LINE__);
	param["id"] = graphicId;
	param["zorder"] = zIndex;
	{
		std::lock_guard<std::mutex> lock(mWatMutex);
		rpcResult =  mWatermarkPluginObj.InvokeJSONRPC("createWatermark", param, result);
	}
	if (rpcResult)
	{
		if (!result["success"].Boolean())
		{
			std::string responseStr;
			result.ToString(responseStr);
			MW_LOG_ERR("PlayerSecManager::%s failed and result: %s", __FUNCTION__, responseStr.c_str());
		}
	}
	else
	{
		MW_LOG_ERR("PlayerSecManager::%s thunder invocation failed!", __FUNCTION__);
	}
#endif
	return;
}

/**
 *  @brief Delete Watermark
 */
void PlayerSecManager::DeleteWatermark(int graphicId)
{
#ifdef USE_CPP_THUNDER_PLUGIN_ACCESS
	JsonObject param;
	JsonObject result;
	bool rpcResult = false;

	MW_LOG_ERR("PlayerSecManager %s:%d ", __FUNCTION__, __LINE__);
	param["id"] = graphicId;
	{
		std::lock_guard<std::mutex> lock(mWatMutex);
		rpcResult =  mWatermarkPluginObj.InvokeJSONRPC("deleteWatermark", param, result);
	}
	if (rpcResult)
	{
		if (!result["success"].Boolean())
		{
			std::string responseStr;
			result.ToString(responseStr);
			MW_LOG_ERR("PlayerSecManager::%s failed and result: %s", __FUNCTION__, responseStr.c_str());
		}
	}
	else
	{
		MW_LOG_ERR("PlayerSecManager::%s thunder invocation failed!", __FUNCTION__);
	}
#endif
	return;
}

/**
 *  @brief Update Watermark
 */
void PlayerSecManager::UpdateWatermark(int graphicId, int smKey, int smSize )
{
#ifdef USE_CPP_THUNDER_PLUGIN_ACCESS
	JsonObject param;
	JsonObject result;
	bool rpcResult = false;

	MW_LOG_ERR("PlayerSecManager %s:%d ", __FUNCTION__, __LINE__);
	param["id"] = graphicId;
	param["key"] = smKey;
	param["size"] = smSize;
	{
		std::lock_guard<std::mutex> lock(mWatMutex);
		rpcResult =  mWatermarkPluginObj.InvokeJSONRPC("updateWatermark", param, result);
	}
	if (rpcResult)
	{
		if (!result["success"].Boolean())
		{
			std::string responseStr;
			result.ToString(responseStr);
			MW_LOG_ERR("PlayerSecManager::%s failed and result: %s", __FUNCTION__, responseStr.c_str());
		}
	}
	else
	{
		MW_LOG_ERR("PlayerSecManager::%s thunder invocation failed!", __FUNCTION__);
	}
#endif
	return;
}

/**
 *   @brief Show watermark image
 *   This method need to be used only when RDKShell is used for rendering. Not supported by Watermark Plugin
 */
void PlayerSecManager::AlwaysShowWatermarkOnTop(bool show)
{
#ifdef USE_CPP_THUNDER_PLUGIN_ACCESS
	JsonObject param;
	JsonObject result;
	bool rpcResult = false;

	MW_LOG_ERR("PlayerSecManager %s:%d ", __FUNCTION__, __LINE__);
	param["show"] = show;
	{
		std::lock_guard<std::mutex> lock(mWatMutex);
		rpcResult =  mWatermarkPluginObj.InvokeJSONRPC("alwaysShowWatermarkOnTop", param, result);
	}
	if (rpcResult)
	{
		if (!result["success"].Boolean())
		{
			std::string responseStr;
			result.ToString(responseStr);
			MW_LOG_ERR("PlayerSecManager::%s failed and result: %s", __FUNCTION__, responseStr.c_str());
		}
	}
	else
	{
		MW_LOG_ERR("PlayerSecManager::%s thunder invocation failed!", __FUNCTION__);
	}
#endif
}

/**
 * @brief GetWaterMarkPalette
 */
void PlayerSecManager::GetWaterMarkPalette(int sessionId, int graphicId)
{
#ifdef USE_CPP_THUNDER_PLUGIN_ACCESS
	JsonObject param;
	JsonObject result;
	bool rpcResult = false;
	param["id"] = graphicId;
	MW_LOG_WARN("PlayerSecManager %s:%d Graphic id: %d ", __FUNCTION__, __LINE__, graphicId);
	{
		std::lock_guard<std::mutex> lock(mWatMutex);
		rpcResult =  mWatermarkPluginObj.InvokeJSONRPC("getPalettedWatermark", param, result);
	}

	if (rpcResult)
	{
		if (!result["success"].Boolean())
		{
			std::string responseStr;
			result.ToString(responseStr);
			MW_LOG_ERR("PlayerSecManager::%s failed and result: %s", __FUNCTION__, responseStr.c_str());
		}
		else //if success, request sec manager to load the clut into the clut shm
		{

			MW_LOG_WARN("PlayerSecManager::%s getWatermarkPalette invoke success for graphicId %d, calling loadClutWatermark", __FUNCTION__, graphicId);
			int clutKey = result["clutKey"].Number();
			int clutSize = result["clutSize"].Number();
			int imageKey = result["imageKey"].Number();
			int imageWidth = result["imageWidth"].Number();
			int imageHeight = result["imageHeight"].Number();
			float aspectRatio = (imageHeight != 0) ? (float)imageWidth/(float)imageHeight : 0.0;
			PlayerSecManager::GetInstance()->loadClutWatermark(sessionId, graphicId, clutKey, imageKey,
															 clutSize, "RGBA8888", imageWidth, imageHeight,
															 aspectRatio);
		}

	}
	else
	{
		MW_LOG_ERR("PlayerSecManager::%s thunder invocation failed!", __FUNCTION__);
	}
#endif
}

/**
 * @brief ModifyWatermarkPalette
 */
void PlayerSecManager::ModifyWatermarkPalette(int graphicId, int clutKey, int imageKey)
{
#ifdef USE_CPP_THUNDER_PLUGIN_ACCESS
	JsonObject param;
	JsonObject result;

	bool rpcResult = false;
	param["id"] = graphicId;
	param["clutKey"] = clutKey;
	param["imageKey"] = imageKey;
	{
		std::lock_guard<std::mutex> lock(mWatMutex);
		rpcResult =  mWatermarkPluginObj.InvokeJSONRPC("modifyPalettedWatermark", param, result);
	}
	if (rpcResult)
	{
		if (!result["success"].Boolean())
		{
			std::string responseStr;
			result.ToString(responseStr);
			MW_LOG_ERR("PlayerSecManager modifyPalettedWatermark failed with result: %s, graphic id: %d", responseStr.c_str(), graphicId);
		}
		else
		{
			MW_LOG_TRACE("PlayerSecManager modifyPalettedWatermark invoke success, graphic id: %d", graphicId);
		}
	}
	else
	{
		MW_LOG_ERR("PlayerSecManager Thunder invocation for modifyPalettedWatermark failed!, graphic id: %d", graphicId);
	}
#endif
}
