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
 * @file ContentSecurityManager.cpp
 * @brief Class impl for ContentSecurityManager
 */

#include "ContentSecurityManager.h"
#include "SecManagerThunder.h"
#include "PlayerLogManager.h"
#include <string.h>
#include "_base64.h"
#include <inttypes.h> // For PRId64
#include <fstream>
#include <sstream>
#include <string>
#include "ThunderAccessPlayer.h"

/**
 * @brief SecManagerThunder Constructor
 */
SecManagerThunder::SecManagerThunder() : mSecManagerObj(SECMANAGER_CALL_SIGN), mSecMutex(), mSchedulerStarted(false),
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
}

/**
 * @brief SecManagerThunder Destructor
 */
SecManagerThunder::~SecManagerThunder()
{
	std::lock_guard<std::mutex> lock(mSecMutex);
	/* hide watermarking before secmanager shutdown */
	ShowWatermark(false);

	/*Stop Scheduler used for handling RDKShell API invocation*/    
	if(true == mSchedulerStarted)
	{
		StopScheduler();
		mSchedulerStarted = false;
	}
	UnRegisterAllEvents();
}

/**
 * @brief To acquire license from SecManager
 */
bool SecManagerThunder::AcquireLicenseOpenOrUpdate( std::string clientId, std::string appId, const char* licenseUrl, const char* moneyTraceMetadata[][2],
		const char* accessAttributes[][2], const char* contentMetadata, size_t contMetaLen,
		const char* licenseRequest, size_t licReqLen, const char* keySystemId,
		const char* mediaUsage, const char* accessToken, size_t accTokenLen,
		ContentSecurityManagerSession &session,
		char** licenseResponse, size_t* licenseResponseLength, int32_t* statusCode, int32_t* reasonCode, int32_t* businessStatus, bool isVideoMuted, int sleepTime)
{
	// licenseUrl un-used now
	(void) licenseUrl;

	bool ret = false;
	bool rpcResult = false;
	unsigned int retryCount = 0;
	bool update = false;	
	//Initializing it with default error codes (which would be sent if there any jsonRPC
	//call failures to thunder)
	*statusCode = CONTENT_SECURITY_MANAGER_DRM_FAILURE;
	*reasonCode = CONTENT_SECURITY_MANAGER_DRM_GEN_FAILURE;

	std::string accessTokenStr = accessToken
		? std::string(accessToken, accTokenLen)
		: std::string();
	std::string contentMetaDataStr = contentMetadata
		? std::string(contentMetadata, contMetaLen)
		: std::string();
	std::string licenseRequestStr = licenseRequest
		? std::string(licenseRequest, licReqLen)
		: std::string();		

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

#ifdef DEBUG_SECMANAGER
	std::string params;
	param.ToString(params);
	MW_LOG_WARN("SecManager %s param: %s",apiName, params.c_str());
#endif

	{
		std::lock_guard<std::mutex> lock(mSecMutex);
		if(!accessTokenStr.empty() &&
				!contentMetaDataStr.empty() &&
				!licenseRequestStr.empty())
		{
			MW_LOG_INFO("Access token, Content metadata and license request are copied successfully, passing details with SecManager");

			//Set json params to be used by sec manager
			param["accessToken"] = accessTokenStr;
			param["contentMetadata"] = contentMetaDataStr;
			param["licenseRequest"] = licenseRequestStr;

#ifdef DEBUG_SECMANAGER
			{
				std::string params;
				param.ToString(params);
				MW_LOG_WARN("SecManager %s param: %s",apiName, params.c_str());
			}
#endif

			//invoke "openPlaybackSession" or "updatePlaybackSession" with retries for specific error cases
			do
			{
				rpcResult = mSecManagerObj.InvokeJSONRPC(apiName, param, response, 10000);
				if (rpcResult)
				{
					ContentSecurityManagerSession newSession;

#ifdef DEBUG_SECMANAGER
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
						newSession = ContentSecurityManagerSession(response["sessionId"].Number(), 
								ContentSecurityManager::getInputSummaryHash(moneyTraceMetadata, contentMetadata,
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
					if((*statusCode == CONTENT_SECURITY_MANAGER_DRM_FAILURE || *statusCode == CONTENT_SECURITY_MANAGER_WM_FAILURE) &&
							(*reasonCode == CONTENT_SECURITY_MANAGER_SERVICE_TIMEOUT ||
							 *reasonCode == CONTENT_SECURITY_MANAGER_SERVICE_CON_FAILURE ||
							 *reasonCode == CONTENT_SECURITY_MANAGER_SERVICE_BUSY ) && retryCount < MAX_LICENSE_REQUEST_ATTEMPTS)
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
			while(retryCount < MAX_LICENSE_REQUEST_ATTEMPTS);

		}
		else
		{
			MW_LOG_ERR("SecManager Failed to copy access token to the shared memory, %s is aborted statusCode: %d, reasonCode: %d", apiName, *statusCode, *reasonCode);
		}
	}
	return ret;
}

/**
 * @brief To update session state to SecManager
 */
bool SecManagerThunder::SetDrmSessionState(int64_t sessionId, bool active)
{
	bool success = false;
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
	return success;
}

/**
 * @brief To notify SecManager to release a session
 */
void SecManagerThunder::CloseDrmSession(int64_t sessionId)
{
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
}

/**
 * @brief To update session state to SecManager
 */
bool SecManagerThunder::setWindowSize(int64_t sessionId, int64_t video_width, int64_t video_height)
{
	bool rpcResult = false;
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
	return rpcResult;
}

/**
 * @brief To set Playback Speed State to SecManager
 */
bool SecManagerThunder::SetPlaybackPosition(int64_t sessionId, float playback_speed, int32_t playback_position)
{
	bool rpcResult = false;
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
	return rpcResult;
}


/**
 * @brief To Load ClutWatermark
 */
bool SecManagerThunder::loadClutWatermark(int64_t sessionId, int64_t graphicId, int64_t watermarkClutBufferKey, int64_t watermarkImageBufferKey, int64_t clutPaletteSize,
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
	return rpcResult;
}

/**
 * @brief  Registers all Events to input plugin
 */
void SecManagerThunder::RegisterAllEvents ()
{
	bool bSubscribed = false;
	std::function<void(const WPEFramework::Core::JSON::VariantContainer&)> watermarkSessionMethod = std::bind(&SecManagerThunder::watermarkSessionHandler, this, std::placeholders::_1);
	bSubscribed = mSecManagerObj.SubscribeEvent(_T("onWatermarkSession"), watermarkSessionMethod);
	if(bSubscribed)
		mRegisteredEvents.push_back("onWatermarkSession");

	std::function<void(const WPEFramework::Core::JSON::VariantContainer&)> addWatermarkMethod = std::bind(&SecManagerThunder::addWatermarkHandler, this, std::placeholders::_1);
	bSubscribed = mSecManagerObj.SubscribeEvent(_T("onAddWatermark"), addWatermarkMethod);
	if(bSubscribed)
		mRegisteredEvents.push_back("onAddWatermark");

	std::function<void(const WPEFramework::Core::JSON::VariantContainer&)> updateWatermarkMethod = std::bind(&SecManagerThunder::updateWatermarkHandler, this, std::placeholders::_1);
	bSubscribed = mSecManagerObj.SubscribeEvent(_T("onUpdateWatermark"), updateWatermarkMethod);
	if(bSubscribed)
		mRegisteredEvents.push_back("onUpdateWatermark");

	std::function<void(const WPEFramework::Core::JSON::VariantContainer&)> removeWatermarkMethod = std::bind(&SecManagerThunder::removeWatermarkHandler, this, std::placeholders::_1);
	bSubscribed = mSecManagerObj.SubscribeEvent(_T("onRemoveWatermark"), removeWatermarkMethod);
	if(bSubscribed)
		mRegisteredEvents.push_back("onRemoveWatermark");

	std::function<void(const WPEFramework::Core::JSON::VariantContainer&)> showWatermarkMethod = std::bind(&SecManagerThunder::showWatermarkHandler, this, std::placeholders::_1);
	bSubscribed = mSecManagerObj.SubscribeEvent(_T("onDisplayWatermark"), showWatermarkMethod);
	if(bSubscribed)
		mRegisteredEvents.push_back("onDisplayWatermark");
}

/**
 * @brief  UnRegisters all Events from plugin
 */
void SecManagerThunder::UnRegisterAllEvents ()
{
	for (auto const& evtName : mRegisteredEvents)
	{
		mSecManagerObj.UnSubscribeEvent(_T(evtName));
	}
	mRegisteredEvents.clear();
}

/**
 * @brief To get scheduler status
 */
bool SecManagerThunder::getSchedulerStatus ()
{
	return mSchedulerStarted;
}

/**
 * @brief  Detects watermarking session conditions
 */
void SecManagerThunder::watermarkSessionHandler(const JsonObject& parameters)
{
	std::string param;
	parameters.ToString(param);
	MW_LOG_WARN("ContentSecurityManager::%s:%d i/p params: %s", __FUNCTION__, __LINE__, param.c_str());
	std::function<void(uint32_t, uint32_t, const std::string&)> sendWatermarkEvent_CB = ContentSecurityManager::getWatermarkSessionEvent_CB();
	if (nullptr != sendWatermarkEvent_CB)
	{
		sendWatermarkEvent_CB( parameters["sessionId"].Number(),parameters["conditionContext"].Number(),parameters["watermarkingSystem"].String());
	}
}

/**
 * @brief  Gets watermark image details and manages watermark rendering
 */
void SecManagerThunder::addWatermarkHandler(const JsonObject& parameters)
{
	std::string param;
	parameters.ToString(param);

	MW_LOG_WARN("ContentSecurityManager::%s:%d i/p params: %s", __FUNCTION__, __LINE__, param.c_str());
	if(getSchedulerStatus())
	{
		int graphicId = parameters["graphicId"].Number();
		int zIndex = parameters["zIndex"].Number();
		MW_LOG_WARN("ContentSecurityManager::%s:%d graphicId : %d index : %d ", __FUNCTION__, __LINE__, graphicId, zIndex);
		ScheduleTask(PlayerAsyncTaskObj([graphicId, zIndex](void *data)
					{
					SecManagerThunder *instance = static_cast<SecManagerThunder *>(data);
					instance->CreateWatermark(graphicId, zIndex);
					}, (void *) ContentSecurityManager::GetInstance()));

		int smKey = parameters["graphicImageBufferKey"].Number();
		int smSize = parameters["graphicImageSize"].Number();/*ToDo : graphicImageSize (long) long conversion*/
		MW_LOG_WARN("ContentSecurityManager::%s:%d graphicId : %d smKey: %d smSize: %d", __FUNCTION__, __LINE__, graphicId, smKey, smSize);
		ContentSecurityManager::GetInstance()->ScheduleTask(PlayerAsyncTaskObj([graphicId, smKey, smSize](void *data)
					{
					SecManagerThunder *instance = static_cast<SecManagerThunder *>(data);
					instance->UpdateWatermark(graphicId, smKey, smSize);
					}, (void *)ContentSecurityManager::GetInstance()));

		if (parameters["adjustVisibilityRequired"].Boolean())
		{
			int sessionId = parameters["sessionId"].Number();
			MW_LOG_WARN("ContentSecurityManager::%s:%d adjustVisibilityRequired is true, invoking GetWaterMarkPalette. graphicId : %d", __FUNCTION__, __LINE__, graphicId);
			ContentSecurityManager::GetInstance()->ScheduleTask(PlayerAsyncTaskObj([sessionId, graphicId](void *data)
						{
						SecManagerThunder *instance = static_cast<SecManagerThunder *>(data);
						instance->GetWaterMarkPalette(sessionId, graphicId);
						}, (void *)ContentSecurityManager::GetInstance()));
		}
		else
		{
			MW_LOG_WARN("ContentSecurityManager::%s:%d adjustVisibilityRequired is false, graphicId : %d", __FUNCTION__, __LINE__, graphicId);
		}

#if 0
		/*Method to be called only if RDKShell is used for rendering*/
		ContentSecurityManager::GetInstance()->ScheduleTask(PlayerAsyncTaskObj([show](void *data)
					{
					SecManagerThunder *instance = static_cast<SecManagerThunder *>(data);
					instance->AlwaysShowWatermarkOnTop(show);
					}, (void *)ContentSecurityManager::GetInstance()));
#endif
	}
	return;
}

/**
 * @brief  Gets updated watermark image details and manages watermark rendering
 */
void SecManagerThunder::updateWatermarkHandler(const JsonObject& parameters)
{
	if(getSchedulerStatus())
	{
		int graphicId = parameters["graphicId"].Number();
		int clutKey = parameters["watermarkClutBufferKey"].Number();
		int imageKey = parameters["watermarkImageBufferKey"].Number();
		MW_LOG_TRACE("graphicId : %d ",graphicId);
		ContentSecurityManager::GetInstance()->ScheduleTask(PlayerAsyncTaskObj([graphicId, clutKey, imageKey](void *data)
					{
					SecManagerThunder *instance = static_cast<SecManagerThunder *>(data);
					instance->ModifyWatermarkPalette(graphicId, clutKey, imageKey);
					}, (void *) ContentSecurityManager::GetInstance()));
	}
	return;
}

/**
 * @brief  Removes watermark image
 */
void SecManagerThunder::removeWatermarkHandler(const JsonObject& parameters)
{
#ifdef DEBUG_SECMANAGER
	std::string param;
	parameters.ToString(param);
	MW_LOG_WARN("ContentSecurityManager::%s:%d i/p params: %s", __FUNCTION__, __LINE__, param.c_str());
#endif
	if(getSchedulerStatus())
	{
		int graphicId = parameters["graphicId"].Number();
		MW_LOG_WARN("ContentSecurityManager::%s:%d graphicId : %d ", __FUNCTION__, __LINE__, graphicId);
		ContentSecurityManager::GetInstance()->ScheduleTask(PlayerAsyncTaskObj([graphicId](void *data)
					{
					SecManagerThunder *instance = static_cast<SecManagerThunder *>(data);
					instance->DeleteWatermark(graphicId);
					}, (void *) ContentSecurityManager::GetInstance()));
#if 0
		/*Method to be called only if RDKShell is used for rendering*/
		ContentSecurityManager::GetInstance()->ScheduleTask(PlayerAsyncTaskObj([show](void *data)
					{
					SecManagerThunder *instance = static_cast<SecManagerThunder *>(data);
					instance->AlwaysShowWatermarkOnTop(show);
					}, (void *) ContentSecurityManager::GetInstance()));
#endif
	}
	return;
}

/**
 * @brief Handles watermark calls to be only once
 */
void SecManagerThunder::showWatermarkHandler(const JsonObject& parameters)
{
	bool show = true;
	if(parameters["hideWatermark"].Boolean())
	{
		show = false;
	}
	MW_LOG_INFO("Received onDisplayWatermark, show: %d ", show);
	if(getSchedulerStatus())
	{
		ContentSecurityManager::GetInstance()->ScheduleTask(PlayerAsyncTaskObj([show](void *data)
					{
					SecManagerThunder *instance = static_cast<SecManagerThunder *>(data);
					instance->ShowWatermark(show);
					}, (void *) ContentSecurityManager::GetInstance()));
	}
	return;
}

/**
 * @brief Show watermark image
 */
void SecManagerThunder::ShowWatermark(bool show)
{
	JsonObject param;
	JsonObject result;
	bool rpcResult = false;

	MW_LOG_ERR("ContentSecurityManager %s:%d ", __FUNCTION__, __LINE__);
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
			MW_LOG_ERR("ContentSecurityManager::%s failed and result: %s", __FUNCTION__, responseStr.c_str());
		}
	}
	else
	{
		MW_LOG_ERR("ContentSecurityManager::%s thunder invocation failed!", __FUNCTION__);
	}
	return;
}

/**
 *  @brief Create Watermark
 */
void SecManagerThunder::CreateWatermark(int graphicId, int zIndex )
{
	JsonObject param;
	JsonObject result;
	bool rpcResult = false;

	MW_LOG_ERR("ContentSecurityManager %s:%d ", __FUNCTION__, __LINE__);
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
			MW_LOG_ERR("ContentSecurityManager::%s failed and result: %s", __FUNCTION__, responseStr.c_str());
		}
	}
	else
	{
		MW_LOG_ERR("ContentSecurityManager::%s thunder invocation failed!", __FUNCTION__);
	}
	return;
}

/**
 *  @brief Delete Watermark
 */
void SecManagerThunder::DeleteWatermark(int graphicId)
{
	JsonObject param;
	JsonObject result;
	bool rpcResult = false;

	MW_LOG_ERR("ContentSecurityManager %s:%d ", __FUNCTION__, __LINE__);
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
			MW_LOG_ERR("ContentSecurityManager::%s failed and result: %s", __FUNCTION__, responseStr.c_str());
		}
	}
	else
	{
		MW_LOG_ERR("ContentSecurityManager::%s thunder invocation failed!", __FUNCTION__);
	}
	return;
}

/**
 *  @brief Update Watermark
 */
void SecManagerThunder::UpdateWatermark(int graphicId, int smKey, int smSize )
{
	JsonObject param;
	JsonObject result;
	bool rpcResult = false;

	MW_LOG_ERR("ContentSecurityManager %s:%d ", __FUNCTION__, __LINE__);
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
			MW_LOG_ERR("ContentSecurityManager::%s failed and result: %s", __FUNCTION__, responseStr.c_str());
		}
	}
	else
	{
		MW_LOG_ERR("ContentSecurityManager::%s thunder invocation failed!", __FUNCTION__);
	}
	return;
}

/**
 *   @brief Show watermark image
 *   This method need to be used only when RDKShell is used for rendering. Not supported by Watermark Plugin
 */
void SecManagerThunder::AlwaysShowWatermarkOnTop(bool show)
{
	JsonObject param;
	JsonObject result;
	bool rpcResult = false;

	MW_LOG_ERR("ContentSecurityManager %s:%d ", __FUNCTION__, __LINE__);
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
			MW_LOG_ERR("ContentSecurityManager::%s failed and result: %s", __FUNCTION__, responseStr.c_str());
		}
	}
	else
	{
		MW_LOG_ERR("ContentSecurityManager::%s thunder invocation failed!", __FUNCTION__);
	}
}

/**
 * @brief GetWaterMarkPalette
 */
void SecManagerThunder::GetWaterMarkPalette(int sessionId, int graphicId)
{
	JsonObject param;
	JsonObject result;
	bool rpcResult = false;
	param["id"] = graphicId;
	MW_LOG_WARN("ContentSecurityManager %s:%d Graphic id: %d ", __FUNCTION__, __LINE__, graphicId);
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
			MW_LOG_ERR("ContentSecurityManager::%s failed and result: %s", __FUNCTION__, responseStr.c_str());
		}
		else //if success, request sec manager to load the clut into the clut shm
		{

			MW_LOG_WARN("ContentSecurityManager::%s getWatermarkPalette invoke success for graphicId %d, calling loadClutWatermark", __FUNCTION__, graphicId);
			int clutKey = result["clutKey"].Number();
			int clutSize = result["clutSize"].Number();
			int imageKey = result["imageKey"].Number();
			int imageWidth = result["imageWidth"].Number();
			int imageHeight = result["imageHeight"].Number();
			float aspectRatio = (imageHeight != 0) ? (float)imageWidth/(float)imageHeight : 0.0;
			loadClutWatermark(sessionId, graphicId, clutKey, imageKey,
					clutSize, "RGBA8888", imageWidth, imageHeight,
					aspectRatio);
		}

	}
	else
	{
		MW_LOG_ERR("ContentSecurityManager::%s thunder invocation failed!", __FUNCTION__);
	}
}

/**
 * @brief ModifyWatermarkPalette
 */
void SecManagerThunder::ModifyWatermarkPalette(int graphicId, int clutKey, int imageKey)
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
			MW_LOG_ERR("ContentSecurityManager modifyPalettedWatermark failed with result: %s, graphic id: %d", responseStr.c_str(), graphicId);
		}
		else
		{
			MW_LOG_TRACE("ContentSecurityManager modifyPalettedWatermark invoke success, graphic id: %d", graphicId);
		}
	}
	else
	{
		MW_LOG_ERR("ContentSecurityManager Thunder invocation for modifyPalettedWatermark failed!, graphic id: %d", graphicId);
	}
}

