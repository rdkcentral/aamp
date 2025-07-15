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
 * @file DrmSessionManager.cpp
 * @brief Source file for DrmSessionManager.
 */

#include "DrmSessionManager.h"
#include "_base64.h"
#include <iostream>
#include "DrmHelper.h"
#include <inttypes.h>
#include "PlayerUtils.h"
#include "ContentSecurityManager.h"
#define DRM_METADATA_TAG_START "<ckm:policy xmlns:ckm=\"urn:ccp:ckm\">"
#define DRM_METADATA_TAG_END "</ckm:policy>"
#define SESSION_TOKEN_URL "http://localhost:50050/authService/getSessionToken"

#define INVALID_SESSION_SLOT -1
#define DEFAULT_CDM_WAIT_TIMEOUT_MS 2000

KeyID::KeyID() : creationTime(0), isFailedKeyId(false), isPrimaryKeyId(false), data()
{
}


/**
 *  @brief DrmSessionManager constructor.
 */
DrmSessionManager::DrmSessionManager(int maxDrmSessions, void *player, std::function<void(uint32_t, uint32_t, const std::string&)> watermarkSessionUpdateCallback) : drmSessionContexts(NULL),
		cachedKeyIDs(NULL), accessToken(NULL),
		accessTokenLen(0), sessionMgrState(SessionMgrState::eSESSIONMGR_ACTIVE), accessTokenMutex(),
		cachedKeyMutex()
		,mEnableAccessAttributes(true)
		,mDrmSessionLock()
		,mMaxDRMSessions(maxDrmSessions)
		,playerSecInstance(nullptr)
		,mContentSecurityManagerSession()
		,mIsVideoOnMute(false)
 		,mCurrentSpeed(0)
		,mFirstFrameSeen(false)
		,mPlayerSendWatermarkSessionUpdateEventCB(watermarkSessionUpdateCallback)
{
	drmSessionContexts	= new DrmSessionContext[mMaxDRMSessions];
	cachedKeyIDs		= new KeyID[mMaxDRMSessions];
	m_drmConfigParam = new configs();
	playerSecInstance = new PlayerSecInterface();

	registerCallback();

	MW_LOG_INFO("DrmSessionManager MaxSession:%d",mMaxDRMSessions);
}

/**
 *  @brief DrmSessionManager Destructor.
 */
DrmSessionManager::~DrmSessionManager()
{
	clearAccessToken();
	clearSessionData();
	MW_SAFE_DELETE_ARRAY(drmSessionContexts);
	MW_SAFE_DELETE_ARRAY(cachedKeyIDs);
	MW_SAFE_DELETE(playerSecInstance);
	ContentSecurityManager::setWatermarkSessionEvent_CB(nullptr);
}
void DrmSessionManager::UpdateDRMConfig(
                bool useSecManager,
		bool enablePROutputProtection,
		bool propagateURIParam,
		bool isFakeTune,
		bool wideVineKIDWorkaround)
{
        m_drmConfigParam->mUseSecManager = useSecManager;
	m_drmConfigParam->mEnablePROutputProtection = enablePROutputProtection;
	m_drmConfigParam->mPropagateURIParam = propagateURIParam;
	m_drmConfigParam->mIsFakeTune = isFakeTune;
	m_drmConfigParam->mIsWVKIDWorkaround = wideVineKIDWorkaround;

}

/**
 *  @brief  Clean up the memory used by session variables.
 */
void DrmSessionManager::clearSessionData()
{
	MW_LOG_WARN(" DrmSessionManager:: Clearing session data");
	for(int i = 0 ; i < mMaxDRMSessions; i++)
	{
		if (drmSessionContexts != NULL && drmSessionContexts[i].drmSession != NULL)
		{
			MW_SAFE_DELETE(drmSessionContexts[i].drmSession);
			drmSessionContexts[i] = DrmSessionContext();
		}

		{
			std::lock_guard<std::mutex> guard(cachedKeyMutex);
			if (cachedKeyIDs != NULL)
			{
				cachedKeyIDs[i] = KeyID();
			}
		}
	}
}

/**
 * @brief Set Session manager state
 */
void DrmSessionManager::setSessionMgrState(SessionMgrState state)
{
	sessionMgrState = state;
}

/**
 * @brief Get Session manager state
 */
SessionMgrState DrmSessionManager::getSessionMgrState()
{
	return sessionMgrState;
}

/**
 * @brief Clean up the failed keyIds.
 */
void DrmSessionManager::clearFailedKeyIds()
{
	std::lock_guard<std::mutex> guard(cachedKeyMutex);
	for(int i = 0 ; i < mMaxDRMSessions; i++)
	{
		if(cachedKeyIDs[i].isFailedKeyId)
		{
			if(!cachedKeyIDs[i].data.empty())
			{
				cachedKeyIDs[i].data.clear();
			}
			cachedKeyIDs[i].isFailedKeyId = false;
			cachedKeyIDs[i].creationTime = 0;
		}
		cachedKeyIDs[i].isPrimaryKeyId = false;
	}
}

/**
 *  @brief Clean up the memory for accessToken.
 */
void DrmSessionManager::clearAccessToken()
{
	if(accessToken)
	{
		free(accessToken);
		accessToken = NULL;
		accessTokenLen = 0;
	}
}

/**
 * @brief Clean up the Session Data if license key acquisition failed or if LicenseCaching is false.
 */
void DrmSessionManager::clearDrmSession(bool forceClearSession)
{
	for(int i = 0 ; i < mMaxDRMSessions; i++)
	{
		// Clear the session data if license key acquisition failed or if forceClearSession is true in the case of LicenseCaching is false.
		if((cachedKeyIDs[i].isFailedKeyId || forceClearSession) && drmSessionContexts != NULL)
		{
			std::lock_guard<std::mutex> guard(drmSessionContexts[i].sessionMutex);
			if (drmSessionContexts[i].drmSession != NULL)
			{
				MW_LOG_WARN("DrmSessionManager:: Clearing failed Session Data Slot : %d", i);
				MW_SAFE_DELETE(drmSessionContexts[i].drmSession);
			}
		}
	}
}


void DrmSessionManager::setVideoWindowSize(int width, int height)
{
	auto localSession = mContentSecurityManagerSession; //Remove potential isSessionValid(), getSessionID() race by using a local copy
	MW_LOG_WARN("In DrmSessionManager:: setting video window size w:%d x h:%d mMaxDRMSessions=%d sessionID=[%" PRId64 "]",width,height,mMaxDRMSessions,localSession.getSessionID());
	if(localSession.isSessionValid())
	{
		MW_LOG_WARN("In DrmSessionManager:: valid session ID. Calling setVideoWindowSize().");
		ContentSecurityManager::GetInstance()->setVideoWindowSize(localSession.getSessionID(), width, height);
	}
}
/**
 * @brief Deactivate the session while video on mute and then activate it and update the speed once video is unmuted
 */
void DrmSessionManager::setVideoMute(bool live, double currentLatency, bool livepoint , double liveOffsetMs,bool isVideoOnMute, double positionMs)
{
	MW_LOG_WARN("Video mute status (new): %d, state changed: %.1s, pos: %f", isVideoOnMute, (isVideoOnMute == mIsVideoOnMute) ? "N":"Y", positionMs);

	mIsVideoOnMute = isVideoOnMute;
	auto localSession = mContentSecurityManagerSession; //Remove potential isSessionValid(), getSessionID() race by using a local copy
	if(localSession.isSessionValid())
	{
		ContentSecurityManager::GetInstance()->UpdateSessionState(localSession.getSessionID(), !mIsVideoOnMute);
		if(!mIsVideoOnMute)
		{
			//this is required as secmanager waits for speed update to show wm once session is active
			int speed=mCurrentSpeed;
			MW_LOG_INFO("Setting speed after video unmute %d ", speed);
			setPlaybackSpeedState(live, currentLatency, livepoint, liveOffsetMs,mCurrentSpeed, positionMs);
		}
	}
}

/**
 * @brief De-activate watermark and prevent it from being re-enabled until we get a new first video frame at normal play speed
 */
void DrmSessionManager::hideWatermarkOnDetach(void)
{
	MW_LOG_WARN("Clearing first frame flag and de-activating watermark.");
	auto localSession = mContentSecurityManagerSession; //Remove potential isSessionValid(), getSessionID() race by using a local copy
	if(localSession.isSessionValid())
	{
		ContentSecurityManager::GetInstance()->UpdateSessionState(localSession.getSessionID(), false);
	}
	mFirstFrameSeen = false;
}


void DrmSessionManager::setPlaybackSpeedState(bool live, double currentLatency, bool livepoint , double liveOffsetMs, int speed, double positionMs, bool firstFrameSeen)
{
	bool isVideoOnMute=mIsVideoOnMute;
	auto localSession = mContentSecurityManagerSession; //Remove potential isSessionValid(), getSessionID() race by using a local copy
	MW_LOG_WARN("In DrmSessionManager::after calling setPlaybackSpeedState speed=%d position=%f sessionID=[%" PRId64 "], mute: %d",speed, positionMs, localSession.getSessionID(), isVideoOnMute);
	mCurrentSpeed = speed;
	if(firstFrameSeen)
	{
		MW_LOG_INFO("First frame seen - latched");
		mFirstFrameSeen = true;
	}
	else if (mFirstFrameSeen)
	{
		MW_LOG_INFO("First frame has previously been seen, we will send speed updates");
	}

	if(localSession.isSessionValid() && !mIsVideoOnMute && mFirstFrameSeen)
	{
		MW_LOG_INFO("calling ContentSecurityManager::setPlaybackSpeedState()");

		double adjustedPos;
	        if(live)
		{ 
			// Live (not VOD) playback: SecManager expects zero for live, negative position if playhead in past
			// This is relative to the broadcast live so we can just return the latency here
		 	adjustedPos = -currentLatency;
		 	MW_LOG_INFO("setPlaybackSpeedState for live playback: position=%fms (at live %d, live offset %fms))", 
		 		adjustedPos, livepoint,liveOffsetMs);
		}
		else
		{ 
			// VOD - report position relative to start of VOD asset
			adjustedPos = positionMs;
		}

		MW_LOG_INFO("setPlaybackSpeedState pos=%fs speed=%d", adjustedPos/1000, speed );
		ContentSecurityManager::GetInstance()->setPlaybackSpeedState(localSession.getSessionID(), speed, adjustedPos);
	}
	else
	{
		bool firstFrameSeenCopy=mFirstFrameSeen;
		isVideoOnMute=mIsVideoOnMute;
		MW_LOG_INFO("Not calling ContentSecurityManager::setPlaybackSpeedState(), sessionID=[%" PRId64 "], mIsVideoOnMute=%d, firstFrameSeen=%d", localSession.getSessionID(), isVideoOnMute, firstFrameSeenCopy);
	}
}


/**
 *  @brief	Extract substring between (excluding) two string delimiters.
 *
 *  @param[in]	parentStr - Parent string from which substring is extracted.
 *  @param[in]	startStr, endStr - String delimiters.
 *  @return	Returns the extracted substring; Empty string if delimiters not found.
 */
string _extractSubstring(string parentStr, string startStr, string endStr)
{
	string ret = "";
	auto startPos = parentStr.find(startStr);
	if(string::npos != startPos)
	{
		auto offset = strlen(startStr.c_str());
		auto endPos = parentStr.find(endStr, startPos + offset + 1);
		if(string::npos != endPos)
		{
			ret = parentStr.substr(startPos + offset, endPos - (startPos + offset));
		}
	}
	return ret;
}


/**
 *  @fn		IsKeyIdProcessed
 *  @param[in]	keyIdArray - key Id extracted from pssh data
 *  @param[out]	status - processed status of the key id success/fail
 *  @return		bool - true if keyId is already marked as failed or cached,
 * 				false if key is not cached
 */
bool DrmSessionManager::IsKeyIdProcessed(std::vector<uint8_t> keyIdArray, bool &status)
{
	bool ret = false;
	std::lock_guard<std::mutex> guard(cachedKeyMutex);
	for (int sessionSlot = 0; sessionSlot < mMaxDRMSessions; sessionSlot++)
	{
		auto keyIDSlot = cachedKeyIDs[sessionSlot].data;
		if (!keyIDSlot.empty() && keyIDSlot.end() != std::find(keyIDSlot.begin(), keyIDSlot.end(), keyIdArray))
		{
			std::string debugStr = PlayerLogManager::getHexDebugStr(keyIdArray);
			MW_LOG_INFO("Session created/in progress with same keyID %s at slot %d", debugStr.c_str(), sessionSlot);
			status = !cachedKeyIDs[sessionSlot].isFailedKeyId;
			ret = true;
			break;
		}
	}
	return ret;
}


int DrmSessionManager::getSlotIdForSession(DrmSession* session)
{
	int slot = -1;
	std::lock_guard<std::mutex> guard(mDrmSessionLock);

	if (drmSessionContexts != NULL)
	{
		for (int i = 0; i < mMaxDRMSessions; i++)
		{
			if (drmSessionContexts[i].drmSession == session)
			{
				MW_LOG_INFO("DRM Session found at slot:%d", i);
				slot = i;
				break;
			}
		}
	}

	if (slot == -1)
	{
		MW_LOG_WARN("DRM Session not found");
	}

	return slot;
}

/**
 *  @brief      Creates and/or returns the DRM session corresponding to keyId (Present in initDataPtr)
 *              DRMSession manager has two static DrmSession objects.
 *              This method will return the existing DRM session pointer if any one of these static
 *              DRM session objects are created against requested keyId. Binds the oldest DRM Session
 *              with new keyId if no matching keyId is found in existing sessions.
 *  @return     Pointer to DrmSession for the given PSSH data; NULL if session creation/mapping fails.
 */
DrmSession * DrmSessionManager::createDrmSession(
		int &err, const char* systemId, MediaFormat mediaFormat, const unsigned char * initDataPtr,
		uint16_t initDataLen, int streamType,
		DrmCallbacks* player, void *metaDataPtr, const unsigned char* contentMetadataPtr,
		 bool isPrimarySession)
{
	DrmInfo drmInfo;
	std::shared_ptr<DrmHelper> drmHelper;
	DrmSession *drmSession = NULL;

	drmInfo.method = eMETHOD_AES_128;
	drmInfo.mediaFormat = mediaFormat;
	drmInfo.systemUUID = systemId;
	drmInfo.bPropagateUriParams = m_drmConfigParam->mPropagateURIParam;

	if (!DrmHelperEngine::getInstance().hasDRM(drmInfo))
	{
		MW_LOG_ERR(" Failed to locate DRM helper");
	}
	else
	{
		drmHelper = DrmHelperEngine::getInstance().createHelper(drmInfo);

		if(contentMetadataPtr)
		{
			std::string contentMetadataPtrString = reinterpret_cast<const char*>(contentMetadataPtr);
			drmHelper->setDrmMetaData(contentMetadataPtrString);
		}

		if (!drmHelper->parsePssh(initDataPtr, initDataLen))
		{
			MW_LOG_ERR(" Failed to Parse PSSH from the DRM InitData");
			err =MW_CORRUPT_DRM_METADATA;
		}
		else
		{
			drmSession = DrmSessionManager::createDrmSession(err, drmHelper, player, streamType, metaDataPtr);
		}
	}

	return drmSession;
}
/**
 *  @brief Create DrmSession by using the DrmHelper object
 */
DrmSession* DrmSessionManager::createDrmSession(int &err, std::shared_ptr<DrmHelper> drmHelper,  DrmCallbacks* Instance, int streamType,void* metaDataPtr)
{
	if (!drmHelper || !Instance)
	{
		/* This should never happen, since the caller should have already
		ensure the provided DRMInfo is supported using hasDRM */
		MW_LOG_ERR(" Failed to create DRM Session invalid parameters ");
		return nullptr;
	}

	// protect createDrmSession multi-thread calls; found during PR 4.0 DRM testing
	std::lock_guard<std::mutex> guard(mDrmSessionLock);

	int cdmError = -1;
	KeyState code = KEY_ERROR;

	if (SessionMgrState::eSESSIONMGR_INACTIVE == sessionMgrState)
	{
		MW_LOG_ERR(" SessionManager state inactive, aborting request");
		return nullptr;
	}

	int selectedSlot = INVALID_SESSION_SLOT;

	MW_LOG_INFO("StreamType :%d keySystem is %s",streamType, drmHelper->ocdmSystemId().c_str());

	/**
	 * Create drm session without primaryKeyId markup OR retrieve old DRM session.
	 */
	code = getDrmSession(err, drmHelper, selectedSlot,  Instance);
	/**
	 * KEY_READY code indicates that a previously created session is being reused.
	 */
	int isContentProcess = -1;
	if((code == KEY_READY) || ((code != KEY_INIT) || (selectedSlot == INVALID_SESSION_SLOT)))
	{
		isContentProcess =0;
	}
	std::vector<uint8_t> keyId;
	drmHelper->getKey(keyId);
	/* callback to initiate content protection data update */
	mCustomData = ContentUpdateCb(drmHelper, streamType , keyId, isContentProcess);
	if (code == KEY_READY)
	{
		return drmSessionContexts[selectedSlot].drmSession;
	}

	if ((code != KEY_INIT) || (selectedSlot == INVALID_SESSION_SLOT))
	{
		MW_LOG_WARN(" Unable to get DrmSession : Key State %d ", code);
		return nullptr;
	}
	code = initializeDrmSession(drmHelper, selectedSlot,  err);
	if (code != KEY_INIT)
	{
		MW_LOG_WARN(" Unable to initialize DrmSession : Key State %d ", code);
		std::lock_guard<std::mutex> guard(cachedKeyMutex);
		if (cachedKeyIDs)
		{
			cachedKeyIDs[selectedSlot].isFailedKeyId = true;
		}
		return nullptr;
	}

	if(m_drmConfigParam->mIsFakeTune)
	{
		MW_LOG_MIL( "Exiting fake tune after DRM initialization.");
		std::lock_guard<std::mutex> guard(cachedKeyMutex);
		if (cachedKeyIDs)
		{
			cachedKeyIDs[selectedSlot].isFailedKeyId = true;
		}
		return nullptr;
	}
	code =this->AcquireLicenseCb(drmHelper, selectedSlot, cdmError,  (GstMediaType)streamType, metaDataPtr, false);
	if (code != KEY_READY)
	{
		MW_LOG_WARN(" Unable to get Ready Status DrmSession : Key State %d ", code);
		std::lock_guard<std::mutex> guard(cachedKeyMutex);
		if (cachedKeyIDs)
		{
			cachedKeyIDs[selectedSlot].isFailedKeyId = true;
		}
		return nullptr;
	}

	// License acquisition was done, so mContentSecurityManagerSession will be populated now
	auto localSession = mContentSecurityManagerSession; //Remove potential isSessionValid(), getSessionID() race by using a local copy
	if (localSession.isSessionValid())
	{
		MW_LOG_WARN(" Setting sessionId[%" PRId64 "] to current drmSession", localSession.getSessionID());
		drmSessionContexts[selectedSlot].drmSession->setSecManagerSession(localSession);
	}

	return drmSessionContexts[selectedSlot].drmSession;
}

/**
 * @brief Create a DRM Session using the Drm Helper
 *        Determine a slot in the drmSession Contexts which can be used
 */
KeyState DrmSessionManager::getDrmSession(int &err, std::shared_ptr<DrmHelper> drmHelper, int &selectedSlot, DrmCallbacks* Instance, bool isPrimarySession)
{
	KeyState code = KEY_ERROR;
	bool keySlotFound = false;
	bool isCachedKeyId = false;

	std::vector<uint8_t> keyIdArray;
	std::map<int, std::vector<uint8_t>> keyIdArrays;
	drmHelper->getKeys(keyIdArrays);

	drmHelper->getKey(keyIdArray);

	//Need to Check , Are all Drm Schemes/Helpers capable of providing a non zero keyId?
	if (keyIdArray.empty())
	{
		err = MW_FAILED_TO_GET_KEYID;
		return code;
	}

	if (keyIdArrays.empty())
	{
		// Insert keyId into map
		keyIdArrays[0] = keyIdArray;
	}

	std::string keyIdDebugStr = PlayerLogManager::getHexDebugStr(keyIdArray);

	/* Slot Selection
	* Find drmSession slot by going through cached keyIds
	* Check if requested keyId is already cached
	*/
	int sessionSlot = 0;

	{
		std::lock_guard<std::mutex> guard(cachedKeyMutex);

		for (; sessionSlot < mMaxDRMSessions; sessionSlot++)
		{
			auto keyIDSlot = cachedKeyIDs[sessionSlot].data;
			if (!keyIDSlot.empty() && keyIDSlot.end() != std::find(keyIDSlot.begin(), keyIDSlot.end(), keyIdArray))
			{
				MW_LOG_INFO("Session created/in progress with same keyID %s at slot %d", keyIdDebugStr.c_str(), sessionSlot);
				keySlotFound = true;
				isCachedKeyId = true;
				break;
			}
		}

		if (!keySlotFound)
		{
			/* Key Id not in cached list so we need to find out oldest slot to use;
			 * Oldest slot may be used by current playback which is marked primary
			 * Avoid selecting that slot
			 * */
			/*select the first slot that is not primary*/
			for (int index = 0; index < mMaxDRMSessions; index++)
			{
				if (!cachedKeyIDs[index].isPrimaryKeyId)
				{
					keySlotFound = true;
					sessionSlot = index;
					break;
				}
			}

			if (!keySlotFound)
			{
				MW_LOG_WARN("  Unable to find keySlot for keyId %s ", keyIdDebugStr.c_str());
				return KEY_ERROR;
			}

			/*Check if there's an older slot */
			for (int index= sessionSlot + 1; index< mMaxDRMSessions; index++)
			{
				if (cachedKeyIDs[index].creationTime < cachedKeyIDs[sessionSlot].creationTime)
				{
					sessionSlot = index;
				}
			}
			MW_LOG_WARN("  Selected slot %d for keyId %s", sessionSlot, keyIdDebugStr.c_str());
		}
		else
		{
			// Already same session Slot is marked failed , not to proceed again .
			if(cachedKeyIDs[sessionSlot].isFailedKeyId)
			{
				MW_LOG_WARN(" Found FailedKeyId at sesssionSlot :%d, return key error",sessionSlot);
				return KEY_ERROR;
			}
		}


		if (!isCachedKeyId)
		{
			if(cachedKeyIDs[sessionSlot].data.size() != 0)
			{
				cachedKeyIDs[sessionSlot].data.clear();
			}

			cachedKeyIDs[sessionSlot].isFailedKeyId = false;

			std::vector<std::vector<uint8_t>> data;
			for(auto& keyId : keyIdArrays)
			{
				std::string debugStr = PlayerLogManager::getHexDebugStr(keyId.second);
				MW_LOG_INFO("Insert[%d] - slot:%d keyID %s", keyId.first, sessionSlot, debugStr.c_str());
				data.push_back(keyId.second);
			}

			cachedKeyIDs[sessionSlot].data = data;
		}
		cachedKeyIDs[sessionSlot].creationTime = GetCurrentTimeMS();
		cachedKeyIDs[sessionSlot].isPrimaryKeyId = isPrimarySession;
	}

	selectedSlot = sessionSlot;
	const std::string systemId = drmHelper->ocdmSystemId();
	std::lock_guard<std::mutex> guard(drmSessionContexts[sessionSlot].sessionMutex);
	if (drmSessionContexts[sessionSlot].drmSession != NULL)
	{
		if (drmHelper->ocdmSystemId() != drmSessionContexts[sessionSlot].drmSession->getKeySystem())
		{
			MW_LOG_WARN("changing DRM session for %s to %s", drmSessionContexts[sessionSlot].drmSession->getKeySystem().c_str(), drmHelper->ocdmSystemId().c_str());
		}
		else if (cachedKeyIDs[sessionSlot].data.end() != std::find(cachedKeyIDs[sessionSlot].data.begin(), cachedKeyIDs[sessionSlot].data.end() ,drmSessionContexts[sessionSlot].data))
		{
			KeyState existingState = drmSessionContexts[sessionSlot].drmSession->getState();
			if (existingState == KEY_READY)
			{
				MW_LOG_INFO("Found drm session READY with same keyID %s - Reusing drm session", keyIdDebugStr.c_str());
				auto slotSession = drmSessionContexts[sessionSlot].drmSession->getSecManagerSession();
				if (slotSession.isSessionValid() && (!mContentSecurityManagerSession.isSessionValid()) )
				{
					// Set the drmSession's ID as mContentSecurityManagerSession so that this code will not be repeated for multiple calls for createDrmSession					
					mContentSecurityManagerSession = slotSession;
 					bool videoMuteState = mIsVideoOnMute;
					MW_LOG_WARN("Activating re-used DRM, sessionId[%" PRId64 "], with video mute = %d", slotSession.getSessionID(), videoMuteState);
					ContentSecurityManager::GetInstance()->UpdateSessionState(slotSession.getSessionID(), true);
				}
				return KEY_READY;
			}
			if (existingState == KEY_INIT)
			{
				MW_LOG_WARN("Found drm session in INIT state with same keyID %s - Reusing drm session", keyIdDebugStr.c_str());
				return KEY_INIT;
			}
			else if (existingState <= KEY_READY)
			{
				if (drmSessionContexts[sessionSlot].drmSession->waitForState(KEY_READY, drmHelper->keyProcessTimeout()))
				{
					MW_LOG_WARN("Waited for drm session READY with same keyID %s - Reusing drm session", keyIdDebugStr.c_str());
					return KEY_READY;
				}
				MW_LOG_WARN("key was never ready for %s ", drmSessionContexts[sessionSlot].drmSession->getKeySystem().c_str());
				//CID-164094 : Added the mutex lock due to overriding the isFailedKeyId variable
				std::lock_guard<std::mutex> guard(cachedKeyMutex);
				cachedKeyIDs[selectedSlot].isFailedKeyId = true;
				return KEY_ERROR;
			}
			else
			{
				MW_LOG_WARN("existing DRM session for %s has error state %d", drmSessionContexts[sessionSlot].drmSession->getKeySystem().c_str(), existingState);
				//CID-164094 : Added the mutex lock due to overriding the isFailedKeyId variable
				std::lock_guard<std::mutex> guard(cachedKeyMutex);
				cachedKeyIDs[selectedSlot].isFailedKeyId = true;
				return KEY_ERROR;
			}
		}
		else
		{
			MW_LOG_WARN("existing DRM session for %s has different key in slot %d", drmSessionContexts[sessionSlot].drmSession->getKeySystem().c_str(), sessionSlot);
		}
		MW_LOG_WARN("deleting existing DRM session for %s ", drmSessionContexts[sessionSlot].drmSession->getKeySystem().c_str());
		MW_SAFE_DELETE(drmSessionContexts[sessionSlot].drmSession);
	}
        this->ProfileUpdateCb();

	drmSessionContexts[sessionSlot].drmSession = DrmSessionFactory::GetDrmSession(drmHelper, Instance);
	if (drmSessionContexts[sessionSlot].drmSession != NULL)
	{
		MW_LOG_INFO("Created new DrmSession for DrmSystemId %s", systemId.c_str());
		drmSessionContexts[sessionSlot].data = keyIdArray;
		code = drmSessionContexts[sessionSlot].drmSession->getState();
		// exception : by default for all types of drm , outputprotection is not handled in player
		// for playready , its configured within player
		if (systemId == PLAYREADY_KEY_SYSTEM_STRING && m_drmConfigParam->mEnablePROutputProtection)
		{
			drmSessionContexts[sessionSlot].drmSession->setOutputProtection(true);
			drmHelper->setOutputProtectionFlag(true);
		}
	}
	else
	{
		MW_LOG_WARN("Unable to Get DrmSession for DrmSystemId %s", systemId.c_str());
		err = MW_DRM_INIT_FAILED ;
	}

#if defined(USE_OPENCDM_ADAPTER)
	drmSessionContexts[sessionSlot].drmSession->setKeyId(keyIdArray);
#endif

	return code;
}

/**
 * @brief Initialize the Drm System with InitData(PSSH)
 */
KeyState DrmSessionManager::initializeDrmSession(std::shared_ptr<DrmHelper> drmHelper, int sessionSlot, int &err )
{
	KeyState code = KEY_ERROR;

	std::vector<uint8_t> drmInitData;
	drmHelper->createInitData(drmInitData);

	std::lock_guard<std::mutex> guard(drmSessionContexts[sessionSlot].sessionMutex);
	MW_LOG_INFO("DRM session Custom Data - %s ", mCustomData.empty()?"NULL":mCustomData.c_str());
	drmSessionContexts[sessionSlot].drmSession->generateDRMSession(drmInitData.data(), (uint32_t)drmInitData.size(), mCustomData);

	code = drmSessionContexts[sessionSlot].drmSession->getState();
	if (code != KEY_INIT)
	{
		MW_LOG_ERR("DRM session was not initialized : Key State %d ", code);
		if (code == KEY_ERROR_EMPTY_SESSION_ID)
		{
			MW_LOG_ERR("DRM session ID is empty: Key State %d ", code);
			err = MW_DRM_SESSIONID_EMPTY;
		}
		else
		{
			err= MW_DRM_DATA_BIND_FAILED;
		}
	}

	return code;
}
/**
 * @brief Re-use the current seesion ID, watermarking variables and de-activate watermarking session status
 */
void DrmSessionManager::notifyCleanup()
{
	auto localSession = mContentSecurityManagerSession; //Remove potential isSessionValid(), getSessionID() race by using a local copy
	if(localSession.isSessionValid())
	{
		// Set current session to inactive
		MW_LOG_WARN("De-activate DRM session [%" PRId64 "] and watermark", localSession.getSessionID() );
		ContentSecurityManager::GetInstance()->UpdateSessionState(localSession.getSessionID(), false);
		// Reset the session ID, the session ID is preserved within DrmSession instances
		mContentSecurityManagerSession.setSessionInvalid();	//note this doesn't necessarily close the session as the session ID is also saved in the slot
		mCurrentSpeed = 0;
		mFirstFrameSeen = false;
	}
}

/**
 * @brief To update the max DRM sessions supported
 *
 * @param[in] maxSessions max DRM Sessions
 */
void DrmSessionManager::UpdateMaxDRMSessions(int maxSessions)
{
	if (mMaxDRMSessions != maxSessions)
	{
		// Clean up the current sessions
		clearSessionData();
		MW_SAFE_DELETE_ARRAY(drmSessionContexts);
		MW_SAFE_DELETE_ARRAY(cachedKeyIDs);

		//Update to new session count
		mMaxDRMSessions = maxSessions;
		drmSessionContexts      = new DrmSessionContext[mMaxDRMSessions];
		cachedKeyIDs            = new KeyID[mMaxDRMSessions];
		MW_LOG_INFO("Updated DrmSessionManager MaxSession to:%d", mMaxDRMSessions);
	}
}

/**
 * @brief To register the callback for watermark session update
 */
void DrmSessionManager::registerCallback() {
	auto instance = this;
	static std::function<void(uint32_t, uint32_t, const std::string&)> watermarkCallBack =
	[instance](uint32_t sessionHandle, uint32_t status, const std::string& system) {
		instance->watermarkSessionHandlerWrapper(sessionHandle, status, system);
	};
	ContentSecurityManager::setWatermarkSessionEvent_CB(watermarkCallBack);
	MW_LOG_INFO("WatermarkSessionEvent Callback registered");
}

/**
 * @brief To wrap the callback for watermark session update
 * @param[in] sessionHandle session handle
 * @param[in] status status of the session
 * @param[in] systemData system data
 * @retval void
 */
void DrmSessionManager::watermarkSessionHandlerWrapper(uint32_t sessionHandle, uint32_t status, const std::string &systemData)
{
	if(NULL != mPlayerSendWatermarkSessionUpdateEventCB)
	{
		mPlayerSendWatermarkSessionUpdateEventCB( sessionHandle, status, systemData);
	}
}
