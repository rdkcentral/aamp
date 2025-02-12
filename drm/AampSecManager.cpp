/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2021 RDK Management
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
 * @file AampSecManager.cpp
 * @brief Class impl for AampSecManager
 */

#include "AampSecManager.h"
#include <string.h>
#include "_base64.h"
#include <inttypes.h> // For PRId64

static AampSecManager *Instance = nullptr; /**< singleton instance*/

/* mutex GetInstance() & DestroyInstance() to improve thread safety
 * There is still a race between using the pointer returned from GetInstance() and calling DestroyInstance()*/
static std::mutex InstanceMutex;

/**
 * @brief To get AampSecManager instance
 */
AampSecManager* AampSecManager::GetInstance()
{
	std::lock_guard<std::mutex> lock{InstanceMutex};
	if (Instance == nullptr)
	{
		Instance = new AampSecManager();
	}
	return Instance;
}

/**
 * @brief To release AampSecManager singelton instance
 */
void AampSecManager::DestroyInstance()
{
	std::lock_guard<std::mutex> lock{InstanceMutex};
	if (Instance)
	{
		/* hide watermarking before secmanager shutdown */
		Instance->ShowWatermark(false);
		delete Instance;
		Instance = nullptr;
	}
}

/**
 * @brief AampScheduler Constructor
 */
AampSecManager::AampSecManager() : mSecManagerObj(SECMANAGER_CALL_SIGN), mSecMutex(), mSchedulerStarted(false),
				   mRegisteredEvents(), mWatermarkPluginObj(WATERMARK_PLUGIN_CALLSIGN), mWatMutex(), mSpeedStateMutex(),
				   mAamp(NULL)
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
		StartScheduler(-1); // pass dummy required playerId parameter; note that we don't yet have a valid mAamp to derive it from
		mSchedulerStarted = true;
	}

	/*
	* Release any unexpectedly open sessions.
	* These could exist if a previous aamp session crashed
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
		param["clientId"] = "aamp";
		param["sessionId"] = 0; //Instructs closePlaybackSession to close all open 'aamp' sessions
		AAMPLOG_WARN("SecManager call closePlaybackSession to ensure no old sessions exist.");
		bool rpcResult = mSecManagerObj.InvokeJSONRPC("closePlaybackSession", param, result);

		if (rpcResult && result["success"].Boolean())
		{
			AAMPLOG_WARN("old SecManager sessions removed");
		}
		else
		{
			AAMPLOG_WARN("no old SecManager sessions to remove");
		}
	}

	RegisterAllEvents();
}

/**
 * @brief AampScheduler Destructor
 */
AampSecManager::~AampSecManager()
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
	ss<<"SecManager input summary hash: "<<returnHash << "(" << InputSummary << ")";
	AAMPLOG_MIL("%s", ss.str().c_str());

	return returnHash;
}


bool AampSecManager::AcquireLicense(PrivateInstanceAAMP* aamp, const char* licenseUrl, const char* moneyTraceMetdata[][2],
					const char* accessAttributes[][2], const char* contentMetdata, size_t contMetaLen,
					const char* licenseRequest, size_t licReqLen, const char* keySystemId,
					const char* mediaUsage, const char* accessToken, size_t accTokenLen,
					AampSecManagerSession &session,
					char** licenseResponse, size_t* licenseResponseLength, int32_t* statusCode, int32_t* reasonCode, int32_t* businessStatus, bool isVideoMuted)
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
		success = AcquireLicenseOpenOrUpdate(aamp, licenseUrl, moneyTraceMetdata,
					accessAttributes, contentMetdata, contMetaLen,
					licenseRequest, licReqLen, keySystemId,
					mediaUsage, accessToken, accTokenLen,
					session,
					licenseResponse, licenseResponseLength, statusCode, reasonCode,
					businessStatus, isVideoMuted);
	}

	return success;
}


/**
 * @brief To acquire license from SecManager
 */
bool AampSecManager::AcquireLicenseOpenOrUpdate(PrivateInstanceAAMP* aamp, const char* licenseUrl, const char* moneyTraceMetdata[][2],
					const char* accessAttributes[][2], const char* contentMetdata, size_t contMetaLen,
					const char* licenseRequest, size_t licReqLen, const char* keySystemId,
					const char* mediaUsage, const char* accessToken, size_t accTokenLen,
					AampSecManagerSession &session,
					char** licenseResponse, size_t* licenseResponseLength, int32_t* statusCode, int32_t* reasonCode, int32_t* businessStatus, bool isVideoMuted)
{
	// licenseUrl un-used now
	(void) licenseUrl;

	bool ret = false;
	bool rpcResult = false;
	unsigned int retryCount = 0;
	
	//Initializing it with default error codes (which would be sent if there any jsonRPC
	//call failures to thunder)
	*statusCode = SECMANGER_DRM_FAILURE;
	*reasonCode = SECMANGER_DRM_GEN_FAILURE;
	
	//Shared memory pointer, key declared here,
	//Access token, content metadata and licence request will be passed to
	//secmanager via shared memory
	void * shmPt_accToken = NULL;
	key_t shmKey_accToken = 0;
	void * shmPt_contMeta = NULL;
	key_t shmKey_contMeta = 0;
	void * shmPt_licReq = NULL;
	key_t shmKey_licReq = 0;
		
	const char* apiName = "openPlaybackSession";
	JsonObject param;
	JsonObject response;
	JsonObject sessionConfig;
	JsonObject aspectDimensions;

	if(NULL != aamp)
		mAamp = aamp;

	sessionConfig["distributedTraceType"] = "money";
	sessionConfig["distributedTraceId"] = moneyTraceMetdata[0][1];
	//Start the playback session as inactive if the video mute is on
	sessionConfig["sessionState"] = isVideoMuted ? "inactive" : "active";
	// TODO: Remove hardcoded values
	aspectDimensions["width"] = 1920;
	aspectDimensions["height"] = 1080;

	param["clientId"] = "aamp";
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
	AAMPLOG_WARN("SecManager %s param: %s",apiName, params.c_str());
#endif
	
	{
		std::lock_guard<std::mutex> lock(mSecMutex);
		if(accTokenLen > 0 && contMetaLen > 0 && licReqLen > 0)
		{
			shmPt_accToken = aamp_CreateSharedMem(accTokenLen, shmKey_accToken);
			shmPt_contMeta = aamp_CreateSharedMem(contMetaLen, shmKey_contMeta);
			shmPt_licReq = aamp_CreateSharedMem(licReqLen, shmKey_licReq);
		}
		
		//Set shared memory with the buffer
		if(NULL != shmPt_accToken && NULL != accessToken &&
		   NULL != shmPt_contMeta && NULL != contentMetdata &&
		   NULL != shmPt_licReq && NULL != licenseRequest)
		{
			//copy buffer to shm
			memcpy(shmPt_accToken, accessToken, accTokenLen);
			memcpy(shmPt_contMeta, contentMetdata, contMetaLen);
			memcpy(shmPt_licReq, licenseRequest, licReqLen);
			
			AAMPLOG_INFO("Access token, Content metadata and license request are copied to the shared memory successfully, passing details with SecManager");
			
			//Set json params to be used by sec manager
			param["accessTokenBufferKey"] = shmKey_accToken;
			param["accessTokenLength"] = accTokenLen;
			
			param["contentMetadataBufferKey"] = shmKey_contMeta;
			param["contentMetadataLength"] = contMetaLen;
			
			param["licenseRequestBufferKey"] = shmKey_licReq;
			param["licenseRequestLength"] = licReqLen;
			
			//Retry delay
			int sleepTime = GETCONFIGVALUE(eAAMPConfig_LicenseRetryWaitTime);
			if(sleepTime<=0) sleepTime = 100;
			//invoke "openPlaybackSession" or "updatePlaybackSession" with retries for specific error cases
			do
			{
				rpcResult = mSecManagerObj.InvokeJSONRPC(apiName, param, response, 10000);
				if (rpcResult)
				{
					AampSecManagerSession newSession;

				#ifdef DEBUG_SECMAMANER
					std::string output;
					response.ToString(output);
					AAMPLOG_WARN("SecManager %s o/p: %s",apiName, output.c_str());
				#endif
					if (response["success"].Boolean())
					{
						/*
						* Ensure all sessions have a Session ID created to manage lifetime
						* multiple object creation is OK as an existing instance should be returned
						* where input data changes e.g. following a call to updatePlaybackSession
						* the input data to the shared session is updated here*/
						newSession = AampSecManagerSession(response["sessionId"].Number(), 
						getInputSummaryHash(moneyTraceMetdata, contentMetdata,
						contMetaLen, licenseRequest, keySystemId,
						mediaUsage, accessToken, isVideoMuted));

						std::string license = response["license"].String();
						AAMPLOG_TRACE("SecManager obtained license with length: %d and data: %s",license.size(), license.c_str());
						if (!license.empty())
						{
							// Here license is base64 encoded
							unsigned char * licenseDecoded = NULL;
							size_t licenseDecodedLen = 0;
							licenseDecoded = base64_Decode(license.c_str(), &licenseDecodedLen);
							AAMPLOG_TRACE("SecManager license decoded len: %d and data: %p", licenseDecodedLen, licenseDecoded);

							if (licenseDecoded != NULL && licenseDecodedLen != 0)
							{
								AAMPLOG_INFO("SecManager license post base64 decode length: %d", *licenseResponseLength);
								*licenseResponse = (char*) malloc(licenseDecodedLen);
								if (*licenseResponse)
								{
									memcpy(*licenseResponse, licenseDecoded, licenseDecodedLen);
									*licenseResponseLength = licenseDecodedLen;
								}
								else
								{
									AAMPLOG_ERR("SecManager failed to allocate memory for license!");
								}
								free(licenseDecoded);
								ret = true;
							}
							else
							{
								AAMPLOG_ERR("SecManager license base64 decode failed!");
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
					if((*statusCode == SECMANGER_DRM_FAILURE || *statusCode == SECMANGER_WM_FAILURE) &&
					   (*reasonCode == SECMANGER_SERVICE_TIMEOUT ||
						*reasonCode == SECMANGER_SERVICE_CON_FAILURE ||
						*reasonCode == SECMANGER_SERVICE_BUSY ) && retryCount < MAX_LICENSE_REQUEST_ATTEMPTS)
					{
						++retryCount;
						AAMPLOG_WARN("SecManager license request failed, response for %s : statusCode: %d, reasonCode: %d, so retrying with delay %d, retry count : %u", apiName, *statusCode, *reasonCode, sleepTime, retryCount );
						mssleep(sleepTime);						
					}
					else
					{
						AAMPLOG_ERR("SecManager license request failed, response for %s : statusCode: %d, reasonCode: %d", apiName, *statusCode, *reasonCode);
						break;
					}
				}
				else
				{
					AAMPLOG_INFO("SecManager license request success, response for %s : statusCode: %d, reasonCode: %d, session status: %s", apiName, *statusCode, *reasonCode, isVideoMuted ? "inactive" : "active");
					break;
				}
			}
			while(retryCount < MAX_LICENSE_REQUEST_ATTEMPTS);
			
			//Cleanup the shared memory after sharing it with secmanager
			aamp_CleanUpSharedMem( shmPt_accToken, shmKey_accToken, accTokenLen);
			aamp_CleanUpSharedMem( shmPt_contMeta, shmKey_contMeta, contMetaLen);
			aamp_CleanUpSharedMem( shmPt_licReq, shmKey_licReq, licReqLen);
		}
		else
		{
			AAMPLOG_ERR("SecManager Failed to copy access token to the shared memory, %s is aborted statusCode: %d, reasonCode: %d", apiName, *statusCode, *reasonCode);
		}
	}
	return ret;
}

/**
 * @brief To update session state to SecManager
 */
bool AampSecManager::UpdateSessionState(int64_t sessionId, bool active)
{
	bool success = false;
	bool rpcResult = false;
	JsonObject result;
	JsonObject param;
	param["clientId"] = "aamp";
	param["sessionId"] = sessionId;
	AAMPLOG_INFO("%s:%d SecManager call setPlaybackSessionState for ID: %" PRId64 " and state: %d", __FUNCTION__, __LINE__, sessionId, active);
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
			AAMPLOG_ERR("%s:%d SecManager setPlaybackSessionState failed for ID: %" PRId64 ", active:%d and result: %s", __FUNCTION__, __LINE__, sessionId, active, responseStr.c_str());
		}
	}
	else
	{
		AAMPLOG_ERR("%s:%d SecManager setPlaybackSessionState failed for ID: %" PRId64 " and active: %d", __FUNCTION__, __LINE__, sessionId, active);
	}

	return success;
}

/**
 * @brief To notify SecManager to release a session
 */
void AampSecManager::ReleaseSession(int64_t sessionId)
{
	bool rpcResult = false;
	JsonObject result;
	JsonObject param;
	param["clientId"] = "aamp";
	param["sessionId"] = sessionId;
	AAMPLOG_INFO("%s:%d SecManager call closePlaybackSession for ID: %" PRId64 "", __FUNCTION__, __LINE__, sessionId);

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
			AAMPLOG_ERR("%s:%d SecManager closePlaybackSession failed for ID: %" PRId64 " and result: %s", __FUNCTION__, __LINE__, sessionId, responseStr.c_str());
		}
	}
	else
	{
		AAMPLOG_ERR("%s:%d SecManager closePlaybackSession failed for ID: %" PRId64 "", __FUNCTION__, __LINE__, sessionId);
	}
	/*Clear aampInstance pointer*/
	mAamp = NULL;
}

/**
 * @brief To update session state to SecManager
 */
bool AampSecManager::setVideoWindowSize(int64_t sessionId, int64_t video_width, int64_t video_height)
{
       bool rpcResult = false;
       JsonObject result;
       JsonObject param;

       param["sessionId"] = sessionId;
       param["videoWidth"] = video_width;
       param["videoHeight"] = video_height;

       AAMPLOG_INFO("%s:%d SecManager call setVideoWindowSize for ID: %" PRId64 "", __FUNCTION__, __LINE__, sessionId);
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
                       AAMPLOG_ERR("%s:%d SecManager setVideoWindowSize failed for ID: %" PRId64 " and result: %s", __FUNCTION__, __LINE__, sessionId, responseStr.c_str());
                       rpcResult = false;
               }

       }
       else
       {
               AAMPLOG_ERR("%s:%d SecManager setVideoWindowSize failed for ID: %" PRId64 "", __FUNCTION__, __LINE__, sessionId);
       }
       return rpcResult;
}

/**
 * @brief To set Playback Speed State to SecManager
 */
bool AampSecManager::setPlaybackSpeedState(int64_t sessionId, int64_t playback_speed, int64_t playback_position)
{
       bool rpcResult = false;
       JsonObject result;
       JsonObject param;
       //mSpeedStateMutex is used to avoid any speedstate event to go when a delayed event is in progress results change in order of event call (i.e, if user tries a trickplay within half a second of tune)
       std::lock_guard<std::mutex> lock(mSpeedStateMutex);

       param["sessionId"] = sessionId;
       param["playbackSpeed"] = playback_speed;
       param["playbackPosition"] = playback_position;

       AAMPLOG_INFO("%s:%d SecManager call setPlaybackSpeedState for ID: %" PRId64 "", __FUNCTION__, __LINE__, sessionId);

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
                       AAMPLOG_ERR("%s:%d SecManager setPlaybackSpeedState failed for ID: %" PRId64 " and result: %s", __FUNCTION__, __LINE__, sessionId, responseStr.c_str());
                       rpcResult = false;
               }
       }
       else
       {
               AAMPLOG_ERR("%s:%d SecManager setPlaybackSpeedState failed for ID: %" PRId64 "", __FUNCTION__, __LINE__, sessionId);
       }
       return rpcResult;
}


/**
 * @brief To Load ClutWatermark
 */
bool AampSecManager::loadClutWatermark(int64_t sessionId, int64_t graphicId, int64_t watermarkClutBufferKey, int64_t watermarkImageBufferKey, int64_t clutPaletteSize,
                                                                       const char* clutPaletteFormat, int64_t watermarkWidth, int64_t watermarkHeight, float aspectRatio)
{
       bool rpcResult = false;
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

       AAMPLOG_INFO("%s:%d SecManager call loadClutWatermark for ID: %" PRId64 "", __FUNCTION__, __LINE__, sessionId);

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
                       AAMPLOG_ERR("%s:%d SecManager loadClutWatermark failed for ID: %" PRId64 " and result: %s", __FUNCTION__, __LINE__, sessionId, responseStr.c_str());
                       rpcResult = false;
               }
       }
       else
       {
               AAMPLOG_ERR("%s:%d SecManager loadClutWatermark failed for ID: %" PRId64 "", __FUNCTION__, __LINE__, sessionId);
       }
       return rpcResult;

}

/**
 * @brief  Registers  Event to input plugin and to mRegisteredEvents list for later use.
 */
void AampSecManager::RegisterEvent (string eventName, std::function<void(const WPEFramework::Core::JSON::VariantContainer&)> functionHandler)
{
	bool bSubscribed;
	bSubscribed = mSecManagerObj.SubscribeEvent(_T(eventName), functionHandler);
	if(bSubscribed)
	{
		mRegisteredEvents.push_back(eventName);
	}
}

/**
 * @brief  Registers all Events to input plugin
 */
void AampSecManager::RegisterAllEvents ()
{
	std::function<void(const WPEFramework::Core::JSON::VariantContainer&)> watermarkSessionMethod = std::bind(&AampSecManager::watermarkSessionHandler, this, std::placeholders::_1);

	RegisterEvent("onWatermarkSession",watermarkSessionMethod);

	std::function<void(const WPEFramework::Core::JSON::VariantContainer&)> addWatermarkMethod = std::bind(&AampSecManager::addWatermarkHandler, this, std::placeholders::_1);

	RegisterEvent("onAddWatermark",addWatermarkMethod);

	std::function<void(const WPEFramework::Core::JSON::VariantContainer&)> updateWatermarkMethod = std::bind(&AampSecManager::updateWatermarkHandler, this, std::placeholders::_1);

	RegisterEvent("onUpdateWatermark",updateWatermarkMethod);

	std::function<void(const WPEFramework::Core::JSON::VariantContainer&)> removeWatermarkMethod = std::bind(&AampSecManager::removeWatermarkHandler, this, std::placeholders::_1);

	RegisterEvent("onRemoveWatermark",removeWatermarkMethod);

	std::function<void(const WPEFramework::Core::JSON::VariantContainer&)> showWatermarkMethod = std::bind(&AampSecManager::showWatermarkHandler, this, std::placeholders::_1);

	RegisterEvent("onDisplayWatermark",showWatermarkMethod);

}

/**
 * @brief  UnRegisters all Events from plugin
 */
void AampSecManager::UnRegisterAllEvents ()
{
	for (auto const& evtName : mRegisteredEvents) {
		mSecManagerObj.UnSubscribeEvent(_T(evtName));
	}
	mRegisteredEvents.clear();
}

/**
 * @brief  Detects watermarking session conditions
 */
void AampSecManager::watermarkSessionHandler(const JsonObject& parameters)
{
	std::string param;
	parameters.ToString(param);
	AAMPLOG_WARN("AampSecManager::%s:%d i/p params: %s", __FUNCTION__, __LINE__, param.c_str());
	if(NULL != mAamp)
	{
		mAamp->SendWatermarkSessionUpdateEvent( parameters["sessionId"].Number(),parameters["conditionContext"].Number(),parameters["watermarkingSystem"].String());
	}
}

/**
 * @brief  Gets watermark image details and manages watermark rendering
 */
void AampSecManager::addWatermarkHandler(const JsonObject& parameters)
{
#ifdef DEBUG_SECMAMANER
	std::string param;
	parameters.ToString(param);

	AAMPLOG_WARN("AampSecManager::%s:%d i/p params: %s", __FUNCTION__, __LINE__, param.c_str());
#endif
	if(mSchedulerStarted)
	{
		int graphicId = parameters["graphicId"].Number();
		int zIndex = parameters["zIndex"].Number();
		AAMPLOG_WARN("AampSecManager::%s:%d graphicId : %d index : %d ", __FUNCTION__, __LINE__, graphicId, zIndex);
		ScheduleTask(AsyncTaskObj([graphicId, zIndex](void *data)
					  {
						AampSecManager *instance = static_cast<AampSecManager *>(data);
						instance->CreateWatermark(graphicId, zIndex);
					  }, (void *) this));

		int smKey = parameters["graphicImageBufferKey"].Number();
		int smSize = parameters["graphicImageSize"].Number();/*ToDo : graphicImageSize (long) long conversion*/
		AAMPLOG_WARN("AampSecManager::%s:%d graphicId : %d smKey: %d smSize: %d", __FUNCTION__, __LINE__, graphicId, smKey, smSize);
		ScheduleTask(AsyncTaskObj([graphicId, smKey, smSize](void *data)
					  {
						AampSecManager *instance = static_cast<AampSecManager *>(data);
						instance->UpdateWatermark(graphicId, smKey, smSize);
					  }, (void *) this));
		
		if (parameters["adjustVisibilityRequired"].Boolean())
		{
			int sessionId = parameters["sessionId"].Number();
			AAMPLOG_WARN("AampSecManager::%s:%d adjustVisibilityRequired is true, invoking GetWaterMarkPalette. graphicId : %d", __FUNCTION__, __LINE__, graphicId);
			ScheduleTask(AsyncTaskObj([sessionId, graphicId](void *data)
									  {
				AampSecManager *instance = static_cast<AampSecManager *>(data);
				instance->GetWaterMarkPalette(sessionId, graphicId);
			}, (void *) this));
		}
		else
		{
			AAMPLOG_WARN("AampSecManager::%s:%d adjustVisibilityRequired is false, graphicId : %d", __FUNCTION__, __LINE__, graphicId);
		}

#if 0
		/*Method to be called only if RDKShell is used for rendering*/
		ScheduleTask(AsyncTaskObj([show](void *data)
					  {
						AampSecManager *instance = static_cast<AampSecManager *>(data);
						instance->AlwaysShowWatermarkOnTop(show);
					  }, (void *) this));
#endif
	}
}

/**
 * @brief  Gets updated watermark image details and manages watermark rendering
 */
void AampSecManager::updateWatermarkHandler(const JsonObject& parameters)
{
	if(mSchedulerStarted)
	{
		int graphicId = parameters["graphicId"].Number();
		int clutKey = parameters["watermarkClutBufferKey"].Number();
		int imageKey = parameters["watermarkImageBufferKey"].Number();
		AAMPLOG_TRACE("graphicId : %d ",graphicId);
		ScheduleTask(AsyncTaskObj([graphicId, clutKey, imageKey](void *data)
								  {
			AampSecManager *instance = static_cast<AampSecManager *>(data);
			instance->ModifyWatermarkPalette(graphicId, clutKey, imageKey);
		}, (void *) this));
	}
}

/**
 * @brief  Removes watermark image
 */
void AampSecManager::removeWatermarkHandler(const JsonObject& parameters)
{
#ifdef DEBUG_SECMAMANER
	std::string param;
	parameters.ToString(param);
	AAMPLOG_WARN("AampSecManager::%s:%d i/p params: %s", __FUNCTION__, __LINE__, param.c_str());
#endif
	if(mSchedulerStarted)
	{
		int graphicId = parameters["graphicId"].Number();
		AAMPLOG_WARN("AampSecManager::%s:%d graphicId : %d ", __FUNCTION__, __LINE__, graphicId);
		ScheduleTask(AsyncTaskObj([graphicId](void *data)
					  {
						AampSecManager *instance = static_cast<AampSecManager *>(data);
						instance->DeleteWatermark(graphicId);
					  }, (void *) this));
#if 0
		/*Method to be called only if RDKShell is used for rendering*/
		ScheduleTask(AsyncTaskObj([show](void *data)
					  {
						AampSecManager *instance = static_cast<AampSecManager *>(data);
						instance->AlwaysShowWatermarkOnTop(show);
					  }, (void *) this));
#endif
	}

}

/**
 * @brief Handles watermark calls to be only once
 */
void AampSecManager::showWatermarkHandler(const JsonObject& parameters)
{
	bool show = true;
	if(parameters["hideWatermark"].Boolean())
	{
		show = false;
	}
	AAMPLOG_INFO("Received onDisplayWatermark, show: %d ", show);
	if(mSchedulerStarted)
	{
		ScheduleTask(AsyncTaskObj([show](void *data)
		{
			AampSecManager *instance = static_cast<AampSecManager *>(data);
			instance->ShowWatermark(show);
		}, (void *) this));
	}
	
	return;
}


/**
 * @brief Show watermark image
 */
void AampSecManager::ShowWatermark(bool show)
{
	JsonObject param;
	JsonObject result;
	bool rpcResult = false;

	AAMPLOG_ERR("AampSecManager %s:%d ", __FUNCTION__, __LINE__);
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
			AAMPLOG_ERR("AampSecManager::%s failed and result: %s", __FUNCTION__, responseStr.c_str());
		}
	}
	else
	{
		AAMPLOG_ERR("AampSecManager::%s thunder invocation failed!", __FUNCTION__);
	}

	return;
}

/**
 *  @brief Create Watermark
 */
void AampSecManager::CreateWatermark(int graphicId, int zIndex )
{
	JsonObject param;
	JsonObject result;
	bool rpcResult = false;

	AAMPLOG_ERR("AampSecManager %s:%d ", __FUNCTION__, __LINE__);
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
			AAMPLOG_ERR("AampSecManager::%s failed and result: %s", __FUNCTION__, responseStr.c_str());
		}
	}
	else
	{
		AAMPLOG_ERR("AampSecManager::%s thunder invocation failed!", __FUNCTION__);
	}
	return;
}

/**
 *  @brief Delete Watermark
 */
void AampSecManager::DeleteWatermark(int graphicId)
{
	JsonObject param;
	JsonObject result;
	bool rpcResult = false;

	AAMPLOG_ERR("AampSecManager %s:%d ", __FUNCTION__, __LINE__);
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
			AAMPLOG_ERR("AampSecManager::%s failed and result: %s", __FUNCTION__, responseStr.c_str());
		}
	}
	else
	{
		AAMPLOG_ERR("AampSecManager::%s thunder invocation failed!", __FUNCTION__);
	}

	return;
}

/**
 *  @brief Update Watermark
 */
void AampSecManager::UpdateWatermark(int graphicId, int smKey, int smSize )
{
	JsonObject param;
	JsonObject result;
	bool rpcResult = false;

	AAMPLOG_ERR("AampSecManager %s:%d ", __FUNCTION__, __LINE__);
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
			AAMPLOG_ERR("AampSecManager::%s failed and result: %s", __FUNCTION__, responseStr.c_str());
		}
	}
	else
	{
		AAMPLOG_ERR("AampSecManager::%s thunder invocation failed!", __FUNCTION__);
	}

	return;
}

/**
 *   @brief Show watermark image
 *   This method need to be used only when RDKShell is used for rendering. Not supported by Watermark Plugin
 */
void AampSecManager::AlwaysShowWatermarkOnTop(bool show)
{
	JsonObject param;
	JsonObject result;
	bool rpcResult = false;

	AAMPLOG_ERR("AampSecManager %s:%d ", __FUNCTION__, __LINE__);
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
			AAMPLOG_ERR("AampSecManager::%s failed and result: %s", __FUNCTION__, responseStr.c_str());
		}
	}
	else
	{
		AAMPLOG_ERR("AampSecManager::%s thunder invocation failed!", __FUNCTION__);
	}
}

/**
 * @brief GetWaterMarkPalette
 */
void AampSecManager::GetWaterMarkPalette(int sessionId, int graphicId)
{
	JsonObject param;
	JsonObject result;
	bool rpcResult = false;
	param["id"] = graphicId;
	AAMPLOG_WARN("AampSecManager %s:%d Graphic id: %d ", __FUNCTION__, __LINE__, graphicId);
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
			AAMPLOG_ERR("AampSecManager::%s failed and result: %s", __FUNCTION__, responseStr.c_str());
		}
		else //if success, request sec manager to load the clut into the clut shm
		{

			AAMPLOG_WARN("AampSecManager::%s getWatermarkPalette invoke success for graphicId %d, calling loadClutWatermark", __FUNCTION__, graphicId);
			int clutKey = result["clutKey"].Number();
			int clutSize = result["clutSize"].Number();
			int imageKey = result["imageKey"].Number();
			int imageWidth = result["imageWidth"].Number();
			int imageHeight = result["imageHeight"].Number();
			float aspectRatio = (imageHeight != 0) ? (float)imageWidth/(float)imageHeight : 0.0;
			AampSecManager::GetInstance()->loadClutWatermark(sessionId, graphicId, clutKey, imageKey,
															 clutSize, "RGBA8888", imageWidth, imageHeight,
															 aspectRatio);
		}

	}
	else
	{
		AAMPLOG_ERR("AampSecManager::%s thunder invocation failed!", __FUNCTION__);
	}
}

/**
 * @brief ModifyWatermarkPalette
 */
void AampSecManager::ModifyWatermarkPalette(int graphicId, int clutKey, int imageKey)
{
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
			AAMPLOG_ERR("AampSecManager modifyPalettedWatermark failed with result: %s, graphic id: %d", responseStr.c_str(), graphicId);
		}
		else
		{
			AAMPLOG_TRACE("AampSecManager modifyPalettedWatermark invoke success, graphic id: %d", graphicId);
		}
	}
	else
	{
		AAMPLOG_ERR("AampSecManager Thunder invocation for modifyPalettedWatermark failed!, graphic id: %d", graphicId);
	}
}
