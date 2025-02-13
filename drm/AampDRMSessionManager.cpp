/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2018 RDK Management
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
 * @file AampDRMSessionManager.cpp
 * @brief Source file for DrmSessionManager.
 */

#include "AampDRMSessionManager.h"
#include "priv_aamp.h"
#include "_base64.h"
#include <iostream>
#include "DrmHelper.h"
#include "AampJsonObject.h"
#include "AampUtils.h"
#include <inttypes.h>
#ifdef USE_SECMANAGER
#include "AampSecManager.h"
#endif

#include "downloader/AampCurlStore.h"
#include "AampDRMLicPreFetcherInterface.h"
#include "AampDRMLicPreFetcher.h"
#include "AampStreamSinkManager.h"

//#define LOG_TRACE 1
#define LICENCE_REQUEST_HEADER_ACCEPT "Accept:"

#define LICENCE_REQUEST_HEADER_CONTENT_TYPE "Content-Type:"
#define LICENCE_REQUEST_USER_AGENT "User-Agent:"

#define LICENCE_RESPONSE_JSON_LICENCE_KEY "license"
#define DRM_METADATA_TAG_START "<ckm:policy xmlns:ckm=\"urn:ccp:ckm\">"
#define DRM_METADATA_TAG_END "</ckm:policy>"
#define SESSION_TOKEN_URL "http://localhost:50050/authService/getSessionToken"

#define INVALID_SESSION_SLOT -1
#define DEFAULT_CDM_WAIT_TIMEOUT_MS 2000

KeyID::KeyID() : creationTime(0), isFailedKeyId(false), isPrimaryKeyId(false), data()
{
}

/**
 *  @brief Get formatted URL of license server
 *
 *  @param[in] url URL of license server
 *  @return		formatted url for secclient license acquisition.
 */
std::string getFormattedLicenseServerURL( const std::string &url)
{
	size_t startpos = 0;
	size_t len = url.length();
	if( url.rfind( "https://",0)==0 )
	{
		startpos = 8;
	}
	else if( url.rfind( "http://",0)==0 )
	{
		startpos = 7;
	}
	if( startpos!=0 )
	{
		size_t endpos = url.find('/', startpos);
		if( endpos != std::string::npos )
		{
			len = endpos - startpos;
		}
	}
	return url.substr(startpos, len);
}

/**
 *  @brief AampDRMSessionManager constructor.
 */
AampDRMSessionManager::AampDRMSessionManager(int maxDrmSessions, PrivateInstanceAAMP *aamp) : drmSessionContexts(NULL),
		cachedKeyIDs(NULL), accessToken(NULL),
		accessTokenLen(0), sessionMgrState(SessionMgrState::eSESSIONMGR_ACTIVE), accessTokenMutex(),
		cachedKeyMutex()
		,mEnableAccessAttributes(true)
		,mDrmSessionLock(), licenseRequestAbort(false)
		,mMaxDRMSessions(maxDrmSessions)
		,mLicenseRenewalThreads()
		,mAccessTokenConnector()
		,aampInstance(aamp)
		,mLicensePrefetcher(nullptr)
#ifdef USE_SECMANAGER
		,mAampSecManagerSession()
		,mIsVideoOnMute(false)
		,mCurrentSpeed(0),
		mFirstFrameSeen(false)
#endif
{
	drmSessionContexts	= new DrmSessionContext[mMaxDRMSessions];
	cachedKeyIDs		= new KeyID[mMaxDRMSessions];
	mLicenseRenewalThreads.resize(mMaxDRMSessions);
	AAMPLOG_INFO("AampDRMSessionManager MaxSession:%d",mMaxDRMSessions);
	mLicensePrefetcher = new AampLicensePreFetcher(aamp);
	mLicensePrefetcher->Init();
}

/**
 *  @brief AampDRMSessionManager Destructor.
 */
AampDRMSessionManager::~AampDRMSessionManager()
{
	clearAccessToken();
	SAFE_DELETE(mLicensePrefetcher);
	clearSessionData();
	releaseLicenseRenewalThreads();
	SAFE_DELETE_ARRAY(drmSessionContexts);
	SAFE_DELETE_ARRAY(cachedKeyIDs);
}

/**
 *  @brief Clean up the license renewal threads created.
 */

void AampDRMSessionManager::releaseLicenseRenewalThreads()
{
        for(int i = 0 ; i < mLicenseRenewalThreads.size(); i++)
        {
		if(mLicenseRenewalThreads[i].joinable())
		{
			mLicenseRenewalThreads[i].join();
		}
	}
	mLicenseRenewalThreads.clear();
}

/*
 * @brief Set the Common Key Duration object
 * 
 * @param keyDuration key duration
 */
void AampDRMSessionManager::SetCommonKeyDuration(int keyDuration)
{
	mLicensePrefetcher->SetCommonKeyDuration(keyDuration);
}

/**
 * @brief Stop DRM session manager and terminate license fetcher
 *
 * @param none
 * @return none
 */
void AampDRMSessionManager::Stop()
{
	mLicensePrefetcher->Term();
}

/**
 * @brief Queue a content protection event to the pipeline
 * 
 * @param drmHelper DrmHelper shared_ptr
 * @param periodId ID of the period to which CP belongs to
 * @param adapId Index of the adaptation to which CP belongs to
 * @param type media type
 * @return none
 */
void AampDRMSessionManager::QueueProtectionEvent(DrmHelperPtr drmHelper, std::string periodId, uint32_t adapIdx, AampMediaType type)
{
	if (drmHelper && aampInstance)
	{
		StreamSink* sink = AampStreamSinkManager::GetInstance().GetActiveStreamSink(aampInstance);
		if (sink)
		{
			std::vector<uint8_t> data;
			const char* systemId = drmHelper->getUuid().c_str();
			drmHelper->createInitData(data);
			AAMPLOG_INFO("Queueing protection event in StreamSink for type:%d period id:%s and adaptation index:%u", type, periodId.c_str(), adapIdx);
			sink->QueueProtectionEvent(systemId, data.data(), data.size(), type);
		}
	}
}

/**
 * @brief set license fetcher object
 * 
 * @return none
 */
void AampDRMSessionManager::SetLicenseFetcher(AampLicenseFetcher *fetcherInstance)
{
	mLicensePrefetcher->SetLicenseFetcher(fetcherInstance);
}

/**
 * @brief Set to true if error event to be sent to application if any license request fails
 *  Otherwise, error event will be sent if a track doesn't have a successful or pending license request
 * 
 * @param sendErrorOnFailure key duration
 */
void AampDRMSessionManager::SetSendErrorOnFailure(bool sendErrorOnFailure)
{
	mLicensePrefetcher->SetSendErrorOnFailure(sendErrorOnFailure);
}

/**
 * @brief Queue a content protection info to be processed later
 * 
 * @param drmHelper DrmHelper shared_ptr
 * @param periodId ID of the period to which CP belongs to
 * @param adapId Index of the adaptation to which CP belongs to
 * @param type media type
 * @param isVssPeriod flag denotes if this is for a VSS period
 * @return true if successfully queued
 * @return false if error occurred
 */
bool AampDRMSessionManager::QueueContentProtection(DrmHelperPtr drmHelper, std::string periodId, uint32_t adapIdx, AampMediaType type, bool isVssPeriod)
{
	return mLicensePrefetcher->QueueContentProtection(drmHelper, periodId, adapIdx, type, isVssPeriod);
}

/**
 *  @brief  Clean up the memory used by session variables.
 */
void AampDRMSessionManager::clearSessionData()
{
	AAMPLOG_WARN(" AampDRMSessionManager:: Clearing session data");
	for(int i = 0 ; i < mMaxDRMSessions; i++)
	{
		if (drmSessionContexts != NULL && drmSessionContexts[i].drmSession != NULL)
		{
			drmSessionContexts[i].mLicenseDownloader.Release();
			SAFE_DELETE(drmSessionContexts[i].drmSession);
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
void AampDRMSessionManager::setSessionMgrState(SessionMgrState state)
{
	sessionMgrState = state;
}

/**
 * @brief Get Session manager state
 */
SessionMgrState AampDRMSessionManager::getSessionMgrState()
{
	return sessionMgrState;
}

/**
 * @brief Get Session abort flag
 */
void AampDRMSessionManager::setLicenseRequestAbort(bool isAbort)
{
	mAccessTokenConnector.Release();
	licenseRequestAbort = isAbort;
}

/**
 * @brief Clean up the failed keyIds.
 */
void AampDRMSessionManager::clearFailedKeyIds()
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
void AampDRMSessionManager::clearAccessToken()
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
void AampDRMSessionManager::clearDrmSession(bool forceClearSession)
{
	for(int i = 0 ; i < mMaxDRMSessions; i++)
	{
		// Clear the session data if license key acquisition failed or if forceClearSession is true in the case of LicenseCaching is false.
		if((cachedKeyIDs[i].isFailedKeyId || forceClearSession) && drmSessionContexts != NULL)
		{
			std::lock_guard<std::mutex> guard(drmSessionContexts[i].sessionMutex);
			if (drmSessionContexts[i].drmSession != NULL)
			{
				AAMPLOG_WARN("AampDRMSessionManager:: Clearing failed Session Data Slot : %d", i);
				SAFE_DELETE(drmSessionContexts[i].drmSession);
				drmSessionContexts[i].mLicenseDownloader.Clear();
			}
		}
	}
}


void AampDRMSessionManager::setVideoWindowSize(int width, int height)
{
#ifdef USE_SECMANAGER
	auto localSession = mAampSecManagerSession; //Remove potential isSessionValid(), getSessionID() race by using a local copy
	AAMPLOG_WARN("In AampDRMSessionManager:: setting video window size w:%d x h:%d mMaxDRMSessions=%d sessionID=[%" PRId64 "]",width,height,mMaxDRMSessions,localSession.getSessionID());
	if(localSession.isSessionValid())
	{
		AAMPLOG_WARN("In AampDRMSessionManager:: valid session ID. Calling setVideoWindowSize().");
		AampSecManager::GetInstance()->setVideoWindowSize(localSession.getSessionID(), width, height);
	}
#endif
}
/**
 * @brief Deactivate the session while video on mute and then activate it and update the speed once video is unmuted
 */
void AampDRMSessionManager::setVideoMute(bool isVideoOnMute, double positionMs)
{
#ifdef USE_SECMANAGER
	AAMPLOG_WARN("Video mute status (new): %d, state changed: %.1s, pos: %f", isVideoOnMute, (isVideoOnMute == mIsVideoOnMute) ? "N":"Y", positionMs);

	mIsVideoOnMute = isVideoOnMute;
	auto localSession = mAampSecManagerSession; //Remove potential isSessionValid(), getSessionID() race by using a local copy
	if(localSession.isSessionValid())
	{
		AampSecManager::GetInstance()->UpdateSessionState(localSession.getSessionID(), !mIsVideoOnMute);
		if(!mIsVideoOnMute)
		{
			//this is required as secmanager waits for speed update to show wm once session is active
			int speed=mCurrentSpeed;
			AAMPLOG_INFO("Setting speed after video unmute %d ", speed);
			setPlaybackSpeedState(mCurrentSpeed, positionMs);
		}
	}
#endif
}

/**
 * @brief De-activate watermark and prevent it from being re-enabled until we get a new first video frame at normal play speed
 */
void AampDRMSessionManager::hideWatermarkOnDetach(void)
{
#ifdef USE_SECMANAGER
	AAMPLOG_WARN("Clearing first frame flag and de-activating watermark.");
	auto localSession = mAampSecManagerSession; //Remove potential isSessionValid(), getSessionID() race by using a local copy
	if(localSession.isSessionValid())
	{
		AampSecManager::GetInstance()->UpdateSessionState(localSession.getSessionID(), false);
	}
	mFirstFrameSeen = false;
#endif
}


void AampDRMSessionManager::setPlaybackSpeedState(int speed, double positionMs, bool firstFrameSeen)
{
#ifdef USE_SECMANAGER
	bool isVideoOnMute=mIsVideoOnMute;
	auto localSession = mAampSecManagerSession; //Remove potential isSessionValid(), getSessionID() race by using a local copy
	AAMPLOG_WARN("In AampDRMSessionManager::after calling setPlaybackSpeedState speed=%d position=%f sessionID=[%" PRId64 "], mute: %d",speed, positionMs, localSession.getSessionID(), isVideoOnMute);
	mCurrentSpeed = speed;
	if(firstFrameSeen)
	{
		AAMPLOG_INFO("First frame seen - latched");
		mFirstFrameSeen = true;
	}
	else if (mFirstFrameSeen)
	{
		AAMPLOG_INFO("First frame has previously been seen, we will send speed updates");
	}

	if(localSession.isSessionValid() && !mIsVideoOnMute && mFirstFrameSeen)
	{
		AAMPLOG_INFO("calling AampSecManager::setPlaybackSpeedState()");

		double adjustedPos;
		if( aampInstance->IsLive() )
		{ 
			// Live (not VOD) playback: SecManager expects zero for live, negative position if playhead in past
			// This is relative to the broadcast live so we can just return the latency here
			adjustedPos = -aampInstance->GetCurrentLatency();
			AAMPLOG_INFO("setPlaybackSpeedState for live playback: position=%fms (at live %d, live offset %fms))", 
				adjustedPos, aampInstance->IsAtLivePoint(), aampInstance->GetLiveOffsetMs() );
		}
		else
		{ 
			// VOD - report position relative to start of VOD asset
			adjustedPos = positionMs;
		}

		AAMPLOG_INFO("setPlaybackSpeedState pos=%fs speed=%d", adjustedPos/1000, speed );
		AampSecManager::GetInstance()->setPlaybackSpeedState(localSession.getSessionID(), speed, adjustedPos);
	}
	else
	{
		bool firstFrameSeenCopy=mFirstFrameSeen;
		isVideoOnMute=mIsVideoOnMute;
		AAMPLOG_INFO("Not calling AampSecManager::setPlaybackSpeedState(), sessionID=[%" PRId64 "], mIsVideoOnMute=%d, firstFrameSeen=%d", localSession.getSessionID(), isVideoOnMute, firstFrameSeenCopy);
	}
#endif
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
 *  @brief Get the accessToken from authService.
 */
const char * AampDRMSessionManager::getAccessToken(int &tokenLen, int &error_code , bool bSslPeerVerify)
{	
	if(accessToken == NULL)
	{
		DownloadResponsePtr respData = std::make_shared<DownloadResponse> ();		
		// Initialize the Seesion Token Connector
		DownloadConfigPtr inpData = std::make_shared<DownloadConfig> ();
		inpData->bIgnoreResponseHeader	= true;
		inpData->eRequestType = eCURL_GET;
		inpData->iStallTimeout = 0; // 2sec
		inpData->iStartTimeout = 0; // 2sec
		inpData->iDownloadTimeout =  DEFAULT_CURL_TIMEOUT;
		inpData->bNeedDownloadMetrics = true;
		inpData->bSSLVerifyPeer		=	bSslPeerVerify;
		mAccessTokenConnector.Initialize(inpData);
		mAccessTokenConnector.Download(SESSION_TOKEN_URL, respData);

		if( respData->curlRetValue == CURLE_OK )
		{			
			if (respData->iHttpRetValue == 200 || respData->iHttpRetValue == 206)
			{		
				string tokenReplyStr;
				mAccessTokenConnector.GetDataString(tokenReplyStr);
				string tokenStatusCode = _extractSubstring(tokenReplyStr, "status\":", ",\"");
				if(tokenStatusCode.length() == 0)
				{
					//StatusCode could be last element in the json
					tokenStatusCode = _extractSubstring(tokenReplyStr, "status\":", "}");
				}
				if(tokenStatusCode.length() == 1 && tokenStatusCode.c_str()[0] == '0')
				{
					string token = _extractSubstring(tokenReplyStr, "token\":\"", "\"");
					size_t len = token.length();
					if(len > 0)
					{
						accessToken = (char*)malloc(len+1);
						if(accessToken)
						{
							accessTokenLen = (int)len;
							memcpy( accessToken, token.c_str(), len );
							accessToken[len] = 0x00;
							AAMPLOG_WARN(" Received session token from auth service in [%f]",respData->downloadCompleteMetrics.total);
						}
						else
						{
							AAMPLOG_WARN("accessToken is null");  //CID:83536 - Null Returns
						}
					}
					else
					{
						AAMPLOG_WARN(" Could not get access token from session token reply");
						error_code = eAUTHTOKEN_TOKEN_PARSE_ERROR;
					}
				}
				else
				{
					AAMPLOG_ERR(" Missing or invalid status code in session token reply");
					error_code = eAUTHTOKEN_INVALID_STATUS_CODE;
				}
			}
			else
			{
				AAMPLOG_ERR(" Get Session token call failed with http error %d", respData->iHttpRetValue);
				error_code = respData->iHttpRetValue;
			}
		}
		else
		{
			AAMPLOG_ERR(" Get Session token call failed with curl error %d", respData->curlRetValue);
			error_code = respData->curlRetValue;
		}
	}
	
	tokenLen = accessTokenLen;
	return accessToken;
}

/**
 *  @fn		IsKeyIdProcessed
 *  @param[in]	keyIdArray - key Id extracted from pssh data
 *  @param[out]	status - processed status of the key id success/fail
 *  @return		bool - true if keyId is already marked as failed or cached,
 * 				false if key is not cached
 */
bool AampDRMSessionManager::IsKeyIdProcessed(std::vector<uint8_t> keyIdArray, bool &status)
{
	bool ret = false;
	std::lock_guard<std::mutex> guard(cachedKeyMutex);
	for (int sessionSlot = 0; sessionSlot < mMaxDRMSessions; sessionSlot++)
	{
		auto keyIDSlot = cachedKeyIDs[sessionSlot].data;
		if (!keyIDSlot.empty() && keyIDSlot.end() != std::find(keyIDSlot.begin(), keyIDSlot.end(), keyIdArray))
		{
			std::string debugStr = AampLogManager::getHexDebugStr(keyIdArray);
			AAMPLOG_INFO("Session created/in progress with same keyID %s at slot %d", debugStr.c_str(), sessionSlot);
			status = !cachedKeyIDs[sessionSlot].isFailedKeyId;
			ret = true;
			break;
		}
	}
	return ret;
}


#if defined(USE_SECCLIENT) || defined(USE_SECMANAGER)

DrmData * AampDRMSessionManager::getLicenseSec(const LicenseRequest &licenseRequest, DrmHelperPtr drmHelper,
		const ChallengeInfo& challengeInfo, PrivateInstanceAAMP* aampInstance, int32_t *httpCode, int32_t *httpExtStatusCode, DrmMetaDataEventPtr eventHandle)
{
	DrmData *licenseResponse = nullptr;
	const char *mediaUsage = "stream";
	string contentMetaData = drmHelper->getDrmMetaData();
	char *encodedData = base64_Encode(reinterpret_cast<const unsigned char*>(contentMetaData.c_str()), contentMetaData.length());
	char *encodedChallengeData = base64_Encode(reinterpret_cast<const unsigned char*>(challengeInfo.data->getData().c_str()), challengeInfo.data->getDataLength());
	//Calculate the lengths using the logic in base64_Encode

	const char *keySystem = drmHelper->ocdmSystemId().c_str();
	const char *secclientSessionToken = challengeInfo.accessToken.empty() ? NULL : challengeInfo.accessToken.c_str();

	char *licenseResponseStr = NULL;
	size_t licenseResponseLength = 2;
	uint32_t refreshDuration = 3;
	const char *requestMetadata[1][2];
	uint8_t numberOfAccessAttributes = 0;
	const char *accessAttributes[2][2] = {NULL, NULL, NULL, NULL};
	long long tStartTime = 0, tEndTime = 0, downloadTimeMS=0;
	std::string serviceZone, streamID;
	if(aampInstance->mIsVSS)
	{
		if (aampInstance->GetEnableAccessAttributesFlag())
		{
			serviceZone = aampInstance->GetServiceZone();
			streamID = aampInstance->GetVssVirtualStreamID();
			if (!serviceZone.empty())
			{
				accessAttributes[numberOfAccessAttributes][0] = VSS_SERVICE_ZONE_KEY_STR;
				accessAttributes[numberOfAccessAttributes][1] = serviceZone.c_str();
				numberOfAccessAttributes++;
			}
			if (!streamID.empty())
			{
				accessAttributes[numberOfAccessAttributes][0] = VSS_VIRTUAL_STREAM_ID_KEY_STR;
				accessAttributes[numberOfAccessAttributes][1] = streamID.c_str();
				numberOfAccessAttributes++;
			}
		}
		AAMPLOG_INFO("accessAttributes : {\"%s\" : \"%s\", \"%s\" : \"%s\"}", accessAttributes[0][0], accessAttributes[0][1], accessAttributes[1][0], accessAttributes[1][1]);
	}
	std::string moneytracestr;
	requestMetadata[0][0] = "X-MoneyTrace";
	aampInstance->GetMoneyTraceString(moneytracestr);
	requestMetadata[0][1] = moneytracestr.c_str();

	AAMPLOG_WARN("[HHH] Before calling SecClient_AcquireLicense-----------");
	AAMPLOG_WARN("destinationURL is %s (drm server now used)", licenseRequest.url.c_str());
	AAMPLOG_WARN("MoneyTrace[%s]", requestMetadata[0][1]);
#if USE_SECMANAGER
	if(aampInstance->mConfig->IsConfigSet(eAAMPConfig_UseSecManager))
	{
		size_t encodedDataLen = ((contentMetaData.length() + 2) /3) * 4;
		size_t encodedChallengeDataLen = ((challengeInfo.data->getDataLength() + 2) /3) * 4;
		int32_t statusCode;
		int32_t reasonCode;
		int32_t businessStatus;

		if (!mAampSecManagerSession.isSessionValid())
		{
			// if we're about to get a licence and are not re-using a session, then we have not seen the first video frame yet. Do not allow watermarking to get enabled yet.
			bool videoMuteState = mIsVideoOnMute;
			AAMPLOG_WARN("First frame flag cleared before AcquireLicense, with mIsVideoOnMute=%d", videoMuteState);
			mFirstFrameSeen = false;
		}

		tStartTime = NOW_STEADY_TS_MS;
		bool res = AampSecManager::GetInstance()->AcquireLicense(aampInstance, licenseRequest.url.c_str(),
																 requestMetadata,
																 ((numberOfAccessAttributes == 0) ? NULL : accessAttributes),
																 encodedData, encodedDataLen,
																 encodedChallengeData, encodedChallengeDataLen,
																 keySystem,
																 mediaUsage,
																 secclientSessionToken, challengeInfo.accessToken.length(),
																 mAampSecManagerSession,
																 &licenseResponseStr, &licenseResponseLength,
																 &statusCode, &reasonCode, &businessStatus, mIsVideoOnMute);
		tEndTime = NOW_STEADY_TS_MS;
		downloadTimeMS = tEndTime - tStartTime;
		if (res)
		{
			AAMPLOG_WARN("acquireLicense via SecManager SUCCESS!");
			//TODO: Sort this out for backward compatibility
			*httpCode = 200;
			*httpExtStatusCode = 0;
			if (licenseResponseStr)
			{
				licenseResponse = new DrmData(licenseResponseStr, licenseResponseLength);
				free(licenseResponseStr);
			}
		}
		else
		{
			eventHandle->SetVerboseErrorCode( statusCode,  reasonCode, businessStatus);
			AAMPLOG_WARN("Verbose error set with class : %d, Reason Code : %d Business status: %d ", statusCode, reasonCode, businessStatus);
			*httpCode = statusCode;
			*httpExtStatusCode = reasonCode;

			if(licenseResponseStr != NULL)
			{
				std::string responseData(licenseResponseStr);
				eventHandle->setResponseData(responseData);
			}
			AAMPLOG_ERR("acquireLicense via SecManager FAILED!, httpCode %d, httpExtStatusCode %d", *httpCode, *httpExtStatusCode);
			//TODO: Sort this out for backward compatibility
		}
	}
#endif
#if USE_SECCLIENT
#if USE_SECMANAGER
	else
	{
#endif
		int32_t sec_client_result = SEC_CLIENT_RESULT_FAILURE;
		SecClient_ExtendedStatus statusInfo;
		unsigned int attemptCount = 0;
		int sleepTime = aampInstance->mConfig->GetConfigValue(eAAMPConfig_LicenseRetryWaitTime) ;
		if(sleepTime<=0) sleepTime = 100;
		tStartTime = NOW_STEADY_TS_MS;
		while (attemptCount < MAX_LICENSE_REQUEST_ATTEMPTS)
		{
			attemptCount++;
			sec_client_result = SecClient_AcquireLicense(licenseRequest.url.c_str(), 1,
														 requestMetadata, numberOfAccessAttributes,
														 ((numberOfAccessAttributes == 0) ? NULL : accessAttributes),
														 encodedData,
														 strlen(encodedData),
														 encodedChallengeData, strlen(encodedChallengeData), keySystem, mediaUsage,
														 secclientSessionToken,
														 &licenseResponseStr, &licenseResponseLength, &refreshDuration, &statusInfo);
			if (((sec_client_result >= 500 && sec_client_result < 600)||
				 (sec_client_result >= SEC_CLIENT_RESULT_HTTP_RESULT_FAILURE_TLS  && sec_client_result <= SEC_CLIENT_RESULT_HTTP_RESULT_FAILURE_GENERIC ))
				&& attemptCount < MAX_LICENSE_REQUEST_ATTEMPTS)
			{
				AAMPLOG_ERR(" acquireLicense FAILED! license request attempt : %d; response code : sec_client %d", attemptCount, sec_client_result);
				if (licenseResponseStr)
				{
					SecClient_FreeResource(licenseResponseStr);
					licenseResponseStr = NULL;
				}
				AAMPLOG_WARN(" acquireLicense : Sleeping %d milliseconds before next retry.", sleepTime);
				mssleep(sleepTime);
			}
			else
			{
				break;
			}
		}
		tEndTime = NOW_STEADY_TS_MS;
		downloadTimeMS = tEndTime - tStartTime;

		AAMPLOG_TRACE("licenseResponse is %s", licenseResponseStr);
		AAMPLOG_TRACE("licenseResponse len is %zd", licenseResponseLength);
		AAMPLOG_TRACE("accessAttributesStatus is %d", statusInfo.accessAttributeStatus);
		AAMPLOG_TRACE("refreshDuration is %d", refreshDuration);
		AAMPLOG_TRACE("total download time is %lld", downloadTimeMS);

		if (sec_client_result != SEC_CLIENT_RESULT_SUCCESS)
		{
			AAMPLOG_ERR(" acquireLicense FAILED! license request attempt : %d; response code : sec_client %d extStatus %d", attemptCount, sec_client_result, statusInfo.statusCode);

			eventHandle->ConvertToVerboseErrorCode( sec_client_result,  statusInfo.statusCode);

			if(licenseResponseStr != NULL)
			{
				std::string responseData(licenseResponseStr);
				eventHandle->setResponseData(responseData);
			}

			*httpCode = sec_client_result;
			*httpExtStatusCode = statusInfo.statusCode;

			AAMPLOG_WARN("Converted the secclient httpCode : %d, httpExtStatusCode: %d to verbose error with class : %d, Reason Code : %d Business status: %d ", *httpCode, *httpExtStatusCode, eventHandle->getSecManagerClassCode(), eventHandle->getSecManagerReasonCode(), eventHandle->getBusinessStatus());
		}
		else
		{
			AAMPLOG_WARN(" acquireLicense SUCCESS! license request attempt %d; response code : sec_client %d", attemptCount, sec_client_result);
			eventHandle->setAccessStatusValue(statusInfo.accessAttributeStatus);
			licenseResponse = new DrmData(licenseResponseStr, licenseResponseLength);
		}
		if (licenseResponseStr) SecClient_FreeResource(licenseResponseStr);
#if USE_SECMANAGER
	}
#endif
#endif
	UpdateLicenseMetrics(DRM_GET_LICENSE_SEC, *httpCode, licenseRequest.url.c_str(), downloadTimeMS, eventHandle, nullptr );

	free(encodedData);
	free(encodedChallengeData);
	return licenseResponse;
}
#endif

int AampDRMSessionManager::getSlotIdForSession(DrmSession* session)
{
	int slot = -1;
	std::lock_guard<std::mutex> guard(mDrmSessionLock);

	if (drmSessionContexts != NULL)
	{
		for (int i = 0; i < mMaxDRMSessions; i++)
		{
			if (drmSessionContexts[i].drmSession == session)
			{
				AAMPLOG_INFO("DRM Session found at slot:%d", i);
				slot = i;
				break;
			}
		}
	}

	if (slot == -1)
	{
		AAMPLOG_WARN("DRM Session not found");
	}

	return slot;
}

void AampDRMSessionManager::licenseRenewalThread(DrmHelperPtr drmHelper, int sessionSlot, PrivateInstanceAAMP* aampInstance)
{
	bool isSecClientError = false;
#if defined(USE_SECCLIENT) || defined(USE_SECMANAGER)
	isSecClientError = true;
#endif
	DrmMetaDataEventPtr e = std::make_shared<DrmMetaDataEvent>(AAMP_TUNE_FAILURE_UNKNOWN, "", 0, 0, isSecClientError, aampInstance->GetSessionId());
	int cdmError = -1;
	KeyState code = acquireLicense(drmHelper, sessionSlot, cdmError, e, aampInstance, eMEDIATYPE_LICENCE, true);
	if (code != KEY_READY)
	{
		aampInstance->SendAnomalyEvent(ANOMALY_WARNING, "License Renewal failed due to Key State %d", code);
		AAMPLOG_ERR("Unable to Renew License for DRM Session : Key State %d ", code);
	}
	else
	{
		AAMPLOG_INFO("License Renewal Done for sessionSlot : %d",sessionSlot);
	}
}

void AampDRMSessionManager::renewLicense(DrmHelperPtr drmHelper, void* userData, PrivateInstanceAAMP* aampInstance)
{
	DrmSession* session = static_cast<DrmSession*>(userData);
	int sessionSlot = getSlotIdForSession(session);
	if (sessionSlot >= 0)
	{
		if(mLicenseRenewalThreads.size() == 0)
		{
			mLicenseRenewalThreads.resize(mMaxDRMSessions);
		}
		if(mLicenseRenewalThreads[sessionSlot].joinable())
		{
			mLicenseRenewalThreads[sessionSlot].join();
		}
		try
		{
			mLicenseRenewalThreads[sessionSlot] = std::thread(&AampDRMSessionManager::licenseRenewalThread, this, drmHelper, sessionSlot, aampInstance);
			AAMPLOG_INFO("Thread created for LicenseRenewal [%zx]", GetPrintableThreadID(mLicenseRenewalThreads[sessionSlot]));
		}
		catch(const std::exception& e)
		{
			AAMPLOG_ERR("thread creation failed for CreateDRMSession: %s", e.what());
		}
	}
	else
	{
		aampInstance->SendAnomalyEvent(ANOMALY_WARNING, "Failed to renew license as slot not available");
		AAMPLOG_ERR("Failed to renew license as the requested DRM session slot is not available");
	}
}

/**
 *  @brief Get DRM license key from DRM server.
 */
DrmData * AampDRMSessionManager::getLicense(LicenseRequest &licenseRequest,
		int32_t *httpCode, AampMediaType streamType, PrivateInstanceAAMP* aamp, DrmMetaDataEventPtr eventHandle, AampCurlDownloader *pLicenseDownloader, std::string licenseProxy)
{

	CURLcode res;
	double totalTime = 0;
	DrmData * keyInfo = NULL;
	bool bNeedResponseHeadersTobeShared 	=	ISCONFIGSET(eAAMPConfig_SendLicenseResponseHeaders);
	DownloadResponsePtr respData 	=	std::make_shared<DownloadResponse> ();		
	// Initialize the Seesion Token Connector
	DownloadConfigPtr inpData 	=	std::make_shared<DownloadConfig> ();
	inpData->bIgnoreResponseHeader				=	!bNeedResponseHeadersTobeShared;
	inpData->iDownloadTimeout				=	aamp->mConfig->GetConfigValue(eAAMPConfig_DrmNetworkTimeout);
	inpData->iStallTimeout = aamp->mConfig->GetConfigValue(eAAMPConfig_DrmStallTimeout);
	inpData->iStartTimeout = aamp->mConfig->GetConfigValue(eAAMPConfig_DrmStartTimeout);
	inpData->iCurlConnectionTimeout =  aamp->mConfig->GetConfigValue(eAAMPConfig_Curl_ConnectTimeout);
	inpData->bNeedDownloadMetrics				=	true;
	inpData->proxyName							=	licenseProxy;		
	inpData->pCurl			=	CurlStore::GetCurlStoreInstance(aamp).GetCurlHandle(aamp, licenseRequest.url, eCURLINSTANCE_AES);
	inpData->sCustomHeaders	=	licenseRequest.headers;
	AAMPLOG_TRACE("DRMSession-getLicense download params - StallTimeout : %d StartTimeout : %d DownloadTimeout : %d CurlConnectionTimeout : %d ",inpData->iStallTimeout,inpData->iStartTimeout,inpData->iDownloadTimeout,inpData->iCurlConnectionTimeout);
	if (aamp->mConfig->IsConfigSet(eAAMPConfig_CurlLicenseLogging))
	{
		inpData->bVerbose		=	true;
		for (auto& header : licenseRequest.headers)
		{
			std::string customHeaderStr = header.first;
			customHeaderStr.push_back(' ');
			customHeaderStr.append(header.second.at(0));
			AAMPLOG_WARN("CustomHeader :%s",customHeaderStr.c_str());
		}
	}
	
	inpData->bSSLVerifyPeer		=	ISCONFIGSET(eAAMPConfig_SslVerifyPeer);	
	if(licenseRequest.method == LicenseRequest::POST)
	{
		inpData->eRequestType 	=	eCURL_POST;
		if(ISCONFIGSET(eAAMPConfig_Base64LicenseWrapping))
		{
			std::string WrapPayload;
			WrapPayload.append("{\"licenseChallenge\":\"");
			char *b64_licenseChallenge = base64_Encode(reinterpret_cast<const unsigned char*>(licenseRequest.payload.c_str()), licenseRequest.payload.length());
			if(b64_licenseChallenge)
			{
				WrapPayload.append(b64_licenseChallenge);
				free(b64_licenseChallenge);
			}
			WrapPayload.append("\"}");
			inpData->postData               =       WrapPayload;
			AAMPLOG_INFO(" Sending license request payload to server : %s ", WrapPayload.c_str());
		}
		else
		{
			inpData->postData             =       licenseRequest.payload;
		}
	}
	else
	{
		inpData->eRequestType = eCURL_GET;	
	}

	// Initialize the downloader for above config settings 	
	pLicenseDownloader->Initialize(inpData);
	
	AAMPLOG_WARN(" Sending license request to server : %s ", licenseRequest.url.c_str());

	unsigned int attemptCount = 0;
	long long tStartTimeWithRetry = NOW_STEADY_TS_MS;
	/* Check whether stopped or not before looping - download will be disabled */
	while(attemptCount < MAX_LICENSE_REQUEST_ATTEMPTS && !licenseRequestAbort)
	{
		bool loopAgain = false;
		attemptCount++;

		long long tStartTime = NOW_STEADY_TS_MS;
		pLicenseDownloader->Download(licenseRequest.url, respData);		
		res = (CURLcode)respData->curlRetValue;
		long long tEndTime = NOW_STEADY_TS_MS;
		long long downloadTimeMS = tEndTime - tStartTime;
		
		/** Restrict further processing license if stop called in between  */
		if(licenseRequestAbort)
		{
			AAMPLOG_ERR(" Aborting License acquisition");
			// Update httpCode as 42-curl abort, so that DRM error event will not be sent by upper layer
			*httpCode = CURLE_ABORTED_BY_CALLBACK;
			break;
		}
		
		if (res != CURLE_OK)
		{
			// To avoid scary logging
			if (res != CURLE_ABORTED_BY_CALLBACK && res != CURLE_WRITE_ERROR)
			{
				if (res == CURLE_OPERATION_TIMEDOUT || res == CURLE_COULDNT_CONNECT)
				{
					// Retry for curl 28 and curl 7 errors.
					loopAgain = true;
					pLicenseDownloader->Clear();
				}
				AAMPLOG_ERR(" curl_easy_perform() failed: %s", curl_easy_strerror(res));
				AAMPLOG_ERR(" acquireLicense FAILED! license request attempt : %d; response code : curl %d", attemptCount, res);
			}
			*httpCode = res;
		}
		else
		{
			*httpCode = respData->iHttpRetValue;
			totalTime = respData->downloadCompleteMetrics.total;
			if (*httpCode != 200 && *httpCode != 206)
			{
				std::string responseData;
				pLicenseDownloader->GetDataString(responseData);
				if(!responseData.empty())
				{
					eventHandle->setResponseData(responseData);
				}
				else
				{
					std::string defResponseData("undefined");
					eventHandle->setResponseData(defResponseData);
				}
				int  licenseRetryWaitTime = aamp->mConfig->GetConfigValue(eAAMPConfig_LicenseRetryWaitTime) ;
				AAMPLOG_ERR(" acquireLicense FAILED! license request attempt : %d; response code : http %d", attemptCount, *httpCode);
				if(*httpCode >= 500 && *httpCode < 600
					&& attemptCount < MAX_LICENSE_REQUEST_ATTEMPTS && licenseRetryWaitTime > 0)
				{			
					AAMPLOG_WARN("acquireLicense : Sleeping %d milliseconds before next retry.",licenseRetryWaitTime);
					mssleep(licenseRetryWaitTime);
					// TODO this is not enabled in old code ???? 
					//loopAgain = true;
					//mLicenseDownloader.Clear();
				}
			}
			else
			{
				AAMPLOG_WARN(" DRM Session Manager Received license data from server; Curl total time  = %.1f", totalTime);
				AAMPLOG_WARN(" acquireLicense SUCCESS! license request attempt %d; response code : http %d", attemptCount, *httpCode);
				keyInfo = new DrmData();
				std::string keyData;
				auto keyLen = pLicenseDownloader->GetDataString(keyData);
				keyInfo->setData(keyData.c_str(), keyLen);
			}
		}
		
		double totalPerformRequest = (double)(downloadTimeMS)/1000;
		std::string appName, timeoutClass;
		if (!aamp->GetAppName().empty())
		{
			// append app name with class data
			appName = aamp->GetAppName() + ",";
		}
		if (CURLE_OPERATION_TIMEDOUT == res || CURLE_PARTIAL_FILE == res || CURLE_COULDNT_CONNECT == res)
		{
			// introduce  extra marker for connection status curl 7/18/28,
			// example 18(0) if connection failure with PARTIAL_FILE code
			timeoutClass = "(" + to_string(respData->downloadCompleteMetrics.reqSize > 0) + ")";
		}
		AAMPLOG_WARN("HttpRequestEnd: %s%d,%d,%d%s,%2.4f,%2.4f,%2.4f,%2.4f,%2.4f,%2.4f,%2.4f,%2.4f,%g,%ld,%d,%d,%.500s",
						appName.c_str(),
						eMEDIATYPE_TELEMETRY_DRM,
						eMEDIATYPE_LICENCE,//streamType,
						*httpCode, timeoutClass.c_str(), totalPerformRequest, totalTime, respData->downloadCompleteMetrics.connect, 
						respData->downloadCompleteMetrics.startTransfer, respData->downloadCompleteMetrics.resolve, respData->downloadCompleteMetrics.appConnect,
						respData->downloadCompleteMetrics.preTransfer, respData->downloadCompleteMetrics.redirect, respData->downloadCompleteMetrics.dlSize,
						respData->downloadCompleteMetrics.reqSize,
						0, // downloadbps, include so we get consistent HttpRequestEnd format, but no urgency to populate here
						0, // video fragment bitrate, n/a
						licenseRequest.url.c_str());

		if(!loopAgain)
			break;
	}
	long long tEndTimeWithRetry = NOW_STEADY_TS_MS;
	long long totalDownloadTimeMS = tEndTimeWithRetry - tStartTimeWithRetry;

	UpdateLicenseMetrics(DRM_GET_LICENSE, *httpCode, licenseRequest.url.c_str(), totalDownloadTimeMS, eventHandle, respData );

	// TODO : Header Response to be set for failed DRM response also ??? 
	if(bNeedResponseHeadersTobeShared && !respData->mResponseHeader.empty())
	{
		eventHandle->setHeaderResponses(respData->mResponseHeader);
	}
	
	if(*httpCode == -1)
	{
		AAMPLOG_WARN(" Updating Curl Abort Response Code");
		// Update httpCode as 42-curl abort, so that DRM error event will not be sent by upper layer
		*httpCode = CURLE_ABORTED_BY_CALLBACK;
	}

	// Return the Curl instance back to Curl Store after use . 
	CurlStore::GetCurlStoreInstance(aamp).SaveCurlHandle(aamp, licenseRequest.url, eCURLINSTANCE_AES, inpData->pCurl);
	// Filled in KeyInfo is returned back 
	return keyInfo;
}



/**
 *  @brief      Creates and/or returns the DRM session corresponding to keyId (Present in initDataPtr)
 *              AampDRMSession manager has two static DrmSession objects.
 *              This method will return the existing DRM session pointer if any one of these static
 *              DRM session objects are created against requested keyId. Binds the oldest DRM Session
 *              with new keyId if no matching keyId is found in existing sessions.
 *  @return     Pointer to DrmSession for the given PSSH data; NULL if session creation/mapping fails.
 */
DrmSession * AampDRMSessionManager::createDrmSession(
		const char* systemId, MediaFormat mediaFormat, const unsigned char * initDataPtr,
		uint16_t initDataLen, AampMediaType streamType,
		PrivateInstanceAAMP* aamp, DrmMetaDataEventPtr e, const unsigned char* contentMetadataPtr,
		bool isPrimarySession)
{
	DrmInfo drmInfo;
	DrmHelperPtr drmHelper;
	DrmSession *drmSession = NULL;

	drmInfo.method = eMETHOD_AES_128;
	drmInfo.mediaFormat = mediaFormat;
	drmInfo.systemUUID = systemId;
	drmInfo.bPropagateUriParams = ISCONFIGSET(eAAMPConfig_PropagateURIParam);

	if (!DrmHelperEngine::getInstance().hasDRM(drmInfo))
	{
		AAMPLOG_ERR(" Failed to locate DRM helper");
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
			AAMPLOG_ERR(" Failed to Parse PSSH from the DRM InitData");
			e->setFailure(AAMP_TUNE_CORRUPT_DRM_METADATA);
		}
		else
		{
			drmSession = AampDRMSessionManager::createDrmSession(drmHelper, e, aamp, streamType);
		}
	}

	return drmSession;
}

/**
 *  @brief Create DrmSession by using the DrmHelper object
 */
DrmSession* AampDRMSessionManager::createDrmSession(DrmHelperPtr drmHelper, DrmMetaDataEventPtr eventHandle, PrivateInstanceAAMP* aampInstance, AampMediaType streamType)
{
	if (!drmHelper || !eventHandle || !aampInstance)
	{
		/* This should never happen, since the caller should have already
		ensure the provided DRMInfo is supported using hasDRM */
		AAMPLOG_ERR(" Failed to create DRM Session invalid parameters ");
		return nullptr;
	}

	// protect createDrmSession multi-thread calls; found during PR 4.0 DRM testing
	std::lock_guard<std::mutex> guard(mDrmSessionLock);

	int cdmError = -1;
	KeyState code = KEY_ERROR;

	if (SessionMgrState::eSESSIONMGR_INACTIVE == sessionMgrState)
	{
		AAMPLOG_ERR(" SessionManager state inactive, aborting request");
		return nullptr;
	}

	int selectedSlot = INVALID_SESSION_SLOT;

	AAMPLOG_INFO("StreamType :%d keySystem is %s",streamType, drmHelper->ocdmSystemId().c_str());

	/**
	 * Create drm session without primaryKeyId markup OR retrieve old DRM session.
	 */
	code = getDrmSession(drmHelper, selectedSlot, eventHandle, aampInstance);
	/* To fetch correct codec type in tune time metrics when drm data is not given in manifest*/
	aampInstance->setCurrentDrm(drmHelper);
	/**
	 * KEY_READY code indicates that a previously created session is being reused.
	 */
	if (code == KEY_READY)
	{
		return drmSessionContexts[selectedSlot].drmSession;
	}

	if ((code != KEY_INIT) || (selectedSlot == INVALID_SESSION_SLOT))
	{
		AAMPLOG_WARN(" Unable to get DrmSession : Key State %d ", code);
		return nullptr;
	}

	std::vector<uint8_t> keyId;
	drmHelper->getKey(keyId);
	bool RuntimeDRMConfigSupported = aampInstance->mConfig->IsConfigSet(eAAMPConfig_RuntimeDRMConfig);
	if(RuntimeDRMConfigSupported && aampInstance->IsEventListenerAvailable(AAMP_EVENT_CONTENT_PROTECTION_DATA_UPDATE) && (streamType < 4))
	{
		aampInstance->mcurrent_keyIdArray = keyId;
		AAMPLOG_INFO("App registered the ContentProtectionDataEvent to send new drm config");
		ContentProtectionDataUpdate(aampInstance, keyId, streamType);
		aampInstance->mcurrent_keyIdArray.clear();
	}

	code = initializeDrmSession(drmHelper, selectedSlot, eventHandle, aampInstance);
	if (code != KEY_INIT)
	{
		AAMPLOG_WARN(" Unable to initialize DrmSession : Key State %d ", code);
		std::lock_guard<std::mutex> guard(cachedKeyMutex);
 		cachedKeyIDs[selectedSlot].isFailedKeyId = true;
		return nullptr;
	}

	if(aampInstance->mIsFakeTune)
	{
		AAMPLOG_MIL( "Exiting fake tune after DRM initialization.");
		std::lock_guard<std::mutex> guard(cachedKeyMutex);
		cachedKeyIDs[selectedSlot].isFailedKeyId = true;
		return nullptr;
	}

	code = acquireLicense(drmHelper, selectedSlot, cdmError, eventHandle, aampInstance, streamType);
	if (code != KEY_READY)
	{
		AAMPLOG_WARN(" Unable to get Ready Status DrmSession : Key State %d ", code);
		std::lock_guard<std::mutex> guard(cachedKeyMutex);
		cachedKeyIDs[selectedSlot].isFailedKeyId = true;
		return nullptr;
	}

#ifdef USE_SECMANAGER
	// License acquisition was done, so mAampSecManagerSession will be populated now
	auto localSession = mAampSecManagerSession; //Remove potential isSessionValid(), getSessionID() race by using a local copy
	if (localSession.isSessionValid())
	{
		AAMPLOG_WARN(" Setting sessionId[%" PRId64 "] to current drmSession", localSession.getSessionID());
		drmSessionContexts[selectedSlot].drmSession->setSecManSession(localSession);
	}
#endif

	return drmSessionContexts[selectedSlot].drmSession;
}

/**
 * @brief Create a DRM Session using the Drm Helper
 *        Determine a slot in the drmSession Contexts which can be used
 */
KeyState AampDRMSessionManager::getDrmSession(DrmHelperPtr drmHelper, int &selectedSlot, DrmMetaDataEventPtr eventHandle, PrivateInstanceAAMP* aampInstance, bool isPrimarySession)
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
		eventHandle->setFailure(AAMP_TUNE_FAILED_TO_GET_KEYID);
		return code;
	}

	if (keyIdArrays.empty())
	{
		// Insert keyId into map
		keyIdArrays[0] = keyIdArray;
	}

	std::string keyIdDebugStr = AampLogManager::getHexDebugStr(keyIdArray);

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
				AAMPLOG_INFO("Session created/in progress with same keyID %s at slot %d", keyIdDebugStr.c_str(), sessionSlot);
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
				AAMPLOG_WARN("  Unable to find keySlot for keyId %s ", keyIdDebugStr.c_str());
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
			AAMPLOG_WARN("  Selected slot %d for keyId %s", sessionSlot, keyIdDebugStr.c_str());
		}
		else
		{
			// Already same session Slot is marked failed , not to proceed again .
			if(cachedKeyIDs[sessionSlot].isFailedKeyId)
			{
				AAMPLOG_WARN(" Found FailedKeyId at sesssionSlot :%d, return key error",sessionSlot);
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
				std::string debugStr = AampLogManager::getHexDebugStr(keyId.second);
				AAMPLOG_INFO("Insert[%d] - slot:%d keyID %s", keyId.first, sessionSlot, debugStr.c_str());
				data.push_back(keyId.second);
			}

			cachedKeyIDs[sessionSlot].data = data;
		}
		cachedKeyIDs[sessionSlot].creationTime = aamp_GetCurrentTimeMS();
		cachedKeyIDs[sessionSlot].isPrimaryKeyId = isPrimarySession;
	}

	selectedSlot = sessionSlot;
	const std::string systemId = drmHelper->ocdmSystemId();
	std::lock_guard<std::mutex> guard(drmSessionContexts[sessionSlot].sessionMutex);
	if (drmSessionContexts[sessionSlot].drmSession != NULL)
	{
		if (drmHelper->ocdmSystemId() != drmSessionContexts[sessionSlot].drmSession->getKeySystem())
		{
			AAMPLOG_WARN("changing DRM session for %s to %s", drmSessionContexts[sessionSlot].drmSession->getKeySystem().c_str(), drmHelper->ocdmSystemId().c_str());
		}
		else if (cachedKeyIDs[sessionSlot].data.end() != std::find(cachedKeyIDs[sessionSlot].data.begin(), cachedKeyIDs[sessionSlot].data.end() ,drmSessionContexts[sessionSlot].data))
		{
			KeyState existingState = drmSessionContexts[sessionSlot].drmSession->getState();
			if (existingState == KEY_READY)
			{
				AAMPLOG_INFO("Found drm session READY with same keyID %s - Reusing drm session", keyIdDebugStr.c_str());
#ifdef USE_SECMANAGER
				// Cached session is re-used, set its session ID to active.
				// State management will be done from getLicenseSec function in case of KEY_INIT
				auto slotSession = drmSessionContexts[sessionSlot].drmSession->getSecManSession();
				if (slotSession.isSessionValid() && (!mAampSecManagerSession.isSessionValid()) )
				{
					// Set the drmSession's ID as mAampSecManagerSession so that this code will not be repeated for multiple calls for createDrmSession					
					mAampSecManagerSession = slotSession;
					bool videoMuteState = mIsVideoOnMute;
					AAMPLOG_WARN("Activating re-used DRM, sessionId[%" PRId64 "], with video mute = %d", slotSession.getSessionID(), videoMuteState);
					AampSecManager::GetInstance()->UpdateSessionState(slotSession.getSessionID(), true);
				}
#endif
				return KEY_READY;
			}
			if (existingState == KEY_INIT)
			{
				AAMPLOG_WARN("Found drm session in INIT state with same keyID %s - Reusing drm session", keyIdDebugStr.c_str());
				return KEY_INIT;
			}
			else if (existingState <= KEY_READY)
			{
				if (drmSessionContexts[sessionSlot].drmSession->waitForState(KEY_READY, drmHelper->keyProcessTimeout()))
				{
					AAMPLOG_WARN("Waited for drm session READY with same keyID %s - Reusing drm session", keyIdDebugStr.c_str());
					return KEY_READY;
				}
				AAMPLOG_WARN("key was never ready for %s ", drmSessionContexts[sessionSlot].drmSession->getKeySystem().c_str());
				//CID-164094 : Added the mutex lock due to overriding the isFailedKeyId variable
				std::lock_guard<std::mutex> guard(cachedKeyMutex);
				cachedKeyIDs[selectedSlot].isFailedKeyId = true;
				return KEY_ERROR;
			}
			else
			{
				AAMPLOG_WARN("existing DRM session for %s has error state %d", drmSessionContexts[sessionSlot].drmSession->getKeySystem().c_str(), existingState);
				//CID-164094 : Added the mutex lock due to overriding the isFailedKeyId variable
				std::lock_guard<std::mutex> guard(cachedKeyMutex);
				cachedKeyIDs[selectedSlot].isFailedKeyId = true;
				return KEY_ERROR;
			}
		}
		else
		{
			AAMPLOG_WARN("existing DRM session for %s has different key in slot %d", drmSessionContexts[sessionSlot].drmSession->getKeySystem().c_str(), sessionSlot);
		}
		AAMPLOG_WARN("deleting existing DRM session for %s ", drmSessionContexts[sessionSlot].drmSession->getKeySystem().c_str());
		SAFE_DELETE(drmSessionContexts[sessionSlot].drmSession);
	}

	aampInstance->profiler.ProfileBegin(PROFILE_BUCKET_LA_PREPROC);

	drmSessionContexts[sessionSlot].drmSession = DrmSessionFactory::GetDrmSession(drmHelper, aampInstance);
	if (drmSessionContexts[sessionSlot].drmSession != NULL)
	{
		AAMPLOG_INFO("Created new DrmSession for DrmSystemId %s", systemId.c_str());
		drmSessionContexts[sessionSlot].data = keyIdArray;
		code = drmSessionContexts[sessionSlot].drmSession->getState();
		// exception : by default for all types of drm , outputprotection is not handled in player
		// for playready , its configured within player
		if (systemId == PLAYREADY_KEY_SYSTEM_STRING && aampInstance->mConfig->IsConfigSet(eAAMPConfig_EnablePROutputProtection))
		{
			drmSessionContexts[sessionSlot].drmSession->setOutputProtection(true);
			drmHelper->setOutputProtectionFlag(true);
		}
	}
	else
	{
		AAMPLOG_WARN("Unable to Get DrmSession for DrmSystemId %s", systemId.c_str());
		eventHandle->setFailure(AAMP_TUNE_DRM_INIT_FAILED);
	}

#if defined(USE_OPENCDM_ADAPTER)
	drmSessionContexts[sessionSlot].drmSession->setKeyId(keyIdArray);
#endif

	return code;
}

/**
 * @brief Initialize the Drm System with InitData(PSSH)
 */
KeyState AampDRMSessionManager::initializeDrmSession(DrmHelperPtr drmHelper, int sessionSlot, DrmMetaDataEventPtr eventHandle,  PrivateInstanceAAMP* aampInstance)
{
	KeyState code = KEY_ERROR;

	std::vector<uint8_t> drmInitData;
	drmHelper->createInitData(drmInitData);

	std::lock_guard<std::mutex> guard(drmSessionContexts[sessionSlot].sessionMutex);
	std::string customData = aampInstance->GetLicenseCustomData();
	AAMPLOG_INFO("DRM session Custom Data - %s ", customData.empty()?"NULL":customData.c_str());
	drmSessionContexts[sessionSlot].drmSession->generateDRMSession(drmInitData.data(), (uint32_t)drmInitData.size(), customData);

	code = drmSessionContexts[sessionSlot].drmSession->getState();
	if (code != KEY_INIT)
	{
		AAMPLOG_ERR("DRM session was not initialized : Key State %d ", code);
		if (code == KEY_ERROR_EMPTY_SESSION_ID)
		{
			AAMPLOG_ERR("DRM session ID is empty: Key State %d ", code);
			eventHandle->setFailure(AAMP_TUNE_DRM_SESSIONID_EMPTY);
		}
		else
		{
			eventHandle->setFailure(AAMP_TUNE_DRM_DATA_BIND_FAILED);
		}
	}

	return code;
}

/**
 * @brief sent license challenge to the DRM server and provide the response to CDM
 */
KeyState AampDRMSessionManager::acquireLicense(DrmHelperPtr drmHelper, int sessionSlot, int &cdmError,
		DrmMetaDataEventPtr eventHandle, PrivateInstanceAAMP* aampInstance, AampMediaType streamType, bool isLicenseRenewal)
{
	shared_ptr<DrmData> licenseResponse;
	int32_t httpResponseCode = -1;
	int32_t httpExtendedStatusCode = -1;
	KeyState code = KEY_ERROR;
	if (drmHelper->isExternalLicense() && !isLicenseRenewal)
	{
		// External license, assuming the DRM system is ready to proceed
		code = KEY_PENDING;
		if(drmHelper->friendlyName().compare("Verimatrix") == 0)
		{
			licenseResponse = std::make_shared<DrmData>();
			drmHelper->transformLicenseResponse(licenseResponse);
		}
		aampInstance->profiler.ProfileEnd(PROFILE_BUCKET_LA_PREPROC);
	}
	else
	{
		std::lock_guard<std::mutex> guard(drmSessionContexts[sessionSlot].sessionMutex);

		/**
		 * Generate a License challenge from the CDM
		 */
		AAMPLOG_INFO("Request to generate license challenge to the aampDRMSession(CDM)");

		ChallengeInfo challengeInfo;
		challengeInfo.data.reset(drmSessionContexts[sessionSlot].drmSession->generateKeyRequest(challengeInfo.url, drmHelper->licenseGenerateTimeout()));
		code = drmSessionContexts[sessionSlot].drmSession->getState();

		if (code != KEY_PENDING)
		{
			AAMPLOG_ERR("Error in getting license challenge : Key State %d ", code);
			if(!isLicenseRenewal)
			{
				aampInstance->profiler.ProfileError(PROFILE_BUCKET_LA_PREPROC, AAMP_TUNE_DRM_CHALLENGE_FAILED);
				aampInstance->profiler.ProfileEnd(PROFILE_BUCKET_LA_PREPROC);
			}
			eventHandle->setFailure(AAMP_TUNE_DRM_CHALLENGE_FAILED);
		}
		else
		{
			/** flag for authToken set externally by app **/
			bool usingAppDefinedAuthToken = !aampInstance->mSessionToken.empty();
			bool anonymousLicenceReq 	=	 aampInstance->mConfig->IsConfigSet(eAAMPConfig_AnonymousLicenseRequest);

			if(!isLicenseRenewal)
			{
				aampInstance->profiler.ProfileEnd(PROFILE_BUCKET_LA_PREPROC);
			}

			if (!(drmHelper->getDrmMetaData().empty() || anonymousLicenceReq))
			{
				std::lock_guard<std::mutex> guard(accessTokenMutex);

				int tokenLen = 0;
				int tokenError = 0;
				const char *sessionToken = NULL;
				if(!usingAppDefinedAuthToken)
				{ /* authToken not set externally by app */
					sessionToken = getAccessToken(tokenLen, tokenError , aampInstance->mConfig->IsConfigSet(eAAMPConfig_SslVerifyPeer));
					AAMPLOG_WARN("Access Token from AuthServer");
				}
				else
				{
					sessionToken = aampInstance->mSessionToken.c_str();
					tokenLen = (int)aampInstance->mSessionToken.size();
					AAMPLOG_WARN("Got Access Token from External App");
				}
				if (NULL == sessionToken)
				{
					// Failed to get access token
					// licenseAnonymousRequest is not set, Report failure
					AAMPLOG_WARN("failed to get access token, Anonymous request not enabled");
					eventHandle->setFailure(AAMP_TUNE_FAILED_TO_GET_ACCESS_TOKEN);
					eventHandle->setResponseCode(tokenError);
					if(!licenseRequestAbort)
					{
						// report error
						return KEY_ERROR;
					}
				}
				else
				{
					AAMPLOG_INFO("access token is available");
					challengeInfo.accessToken = std::string(sessionToken, tokenLen);
				}
			}
			if(licenseRequestAbort)
			{
				AAMPLOG_ERR("Error!! License request was aborted. Resetting session slot %d", sessionSlot);
				eventHandle->setFailure(AAMP_TUNE_DRM_SELF_ABORT);
				eventHandle->setResponseCode(CURLE_ABORTED_BY_CALLBACK);
				return KEY_ERROR;
			}

			LicenseRequest licenseRequest;
			DRMSystems drmType = GetDrmSystem(drmHelper->getUuid());
			licenseRequest.url = aampInstance->GetLicenseServerUrlForDrm(drmType);
			licenseRequest.licenseAnonymousRequest = anonymousLicenceReq;
			drmHelper->generateLicenseRequest(challengeInfo, licenseRequest);
			if (code != KEY_PENDING || ((licenseRequest.method == LicenseRequest::POST) && (!challengeInfo.data.get())))
			{
				AAMPLOG_ERR("Error!! License challenge was not generated by the CDM : Key State %d", code);
				eventHandle->setFailure(AAMP_TUNE_DRM_CHALLENGE_FAILED);
			}
			else
			{
				/**
				 * Configure the License acquisition parameters
				 */
				std::string licenseServerProxy;
				/**
				 * Perform License acquisition by invoking http license request to license server
				 */
				AAMPLOG_WARN("Request License from the Drm Server %s", licenseRequest.url.c_str());
				if(!isLicenseRenewal)
				{
					aampInstance->profiler.ProfileBegin(PROFILE_BUCKET_LA_NETWORK);
				}
#if defined(USE_SECCLIENT) || defined(USE_SECMANAGER)
				bool isContentMetadataAvailable = configureLicenseServerParameters(drmHelper, licenseRequest, licenseServerProxy, challengeInfo, aampInstance);
				if (isContentMetadataAvailable)
				{
					eventHandle->setSecclientError(true);
					licenseResponse.reset(getLicenseSec(licenseRequest, drmHelper, challengeInfo, aampInstance, &httpResponseCode, &httpExtendedStatusCode, eventHandle));

					bool sec_accessTokenExpired = aampInstance->mConfig->IsConfigSet(eAAMPConfig_UseSecManager) && SECMANAGER_DRM_FAILURE == httpResponseCode && SECMANAGER_ACCTOKEN_EXPIRED == httpExtendedStatusCode;
					// Reload Expired access token only on http error code 412 with status code 401
					if (((412 == httpResponseCode && 401 == httpExtendedStatusCode) || sec_accessTokenExpired) && !usingAppDefinedAuthToken)
					{
						AAMPLOG_INFO("License Req failure by Expired access token httpResCode %d statusCode %d", httpResponseCode, httpExtendedStatusCode);
						if(accessToken)
						{
							free(accessToken);
							accessToken = NULL;
							accessTokenLen = 0;
						}
						int tokenLen = 0;
						int tokenError = 0;
						const char *sessionToken = getAccessToken(tokenLen, tokenError,aampInstance->mConfig->IsConfigSet(eAAMPConfig_SslVerifyPeer));
						if (NULL != sessionToken)
						{
							AAMPLOG_INFO("Requesting License with new access token");
							challengeInfo.accessToken = std::string(sessionToken, tokenLen);
							httpResponseCode = httpExtendedStatusCode = -1;
							licenseResponse.reset(getLicenseSec(licenseRequest, drmHelper, challengeInfo, aampInstance, &httpResponseCode, &httpExtendedStatusCode, eventHandle));
						}
					}
				}
				else
#endif
				{
					if(usingAppDefinedAuthToken)
					{
						AAMPLOG_WARN("Ignore  AuthToken Provided for non-ContentMetadata DRM license request");
					}
					eventHandle->setSecclientError(false);
					licenseResponse.reset(getLicense(licenseRequest, &httpResponseCode, streamType, aampInstance, eventHandle, &drmSessionContexts[sessionSlot].mLicenseDownloader,licenseServerProxy));
				}

			}
		}
	}

	if (code == KEY_PENDING)
	{
		code = handleLicenseResponse(drmHelper, sessionSlot, cdmError, httpResponseCode, httpExtendedStatusCode, licenseResponse, eventHandle, aampInstance, isLicenseRenewal);
	}

	return code;
}


KeyState AampDRMSessionManager::handleLicenseResponse(DrmHelperPtr drmHelper, int sessionSlot, int &cdmError, int32_t httpResponseCode, int32_t httpExtendedStatusCode, shared_ptr<DrmData> licenseResponse, DrmMetaDataEventPtr eventHandle, PrivateInstanceAAMP* aamp, bool isLicenseRenewal)
{
	if (!drmHelper->isExternalLicense())
	{
		if ((licenseResponse != NULL) && (licenseResponse->getDataLength() != 0))
		{
			if(!isLicenseRenewal)
			{
				aamp->profiler.ProfileEnd(PROFILE_BUCKET_LA_NETWORK);
			}
#if !defined(USE_SECCLIENT) && !defined(USE_SECMANAGER)
			if (!drmHelper->getDrmMetaData().empty() || ISCONFIGSET(eAAMPConfig_Base64LicenseWrapping))
			{
				/*
					Licence response from MDS server is in JSON form
					Licence to decrypt the data can be found by extracting the contents for JSON key licence
					Format : {"licence":"b64encoded licence","accessAttributes":"0"}
				*/
				string jsonStr(licenseResponse->getData().c_str(), licenseResponse->getDataLength());

				try
				{
					AampJsonObject jsonObj(jsonStr);

					std::vector<uint8_t> keyData;
					if (!jsonObj.get(LICENCE_RESPONSE_JSON_LICENCE_KEY, keyData, AampJsonObject::ENCODING_BASE64))
					{
						AAMPLOG_WARN("Unable to retrieve license from JSON response (%s)", jsonStr.c_str());
					}
					else
					{
						licenseResponse = make_shared<DrmData>((char *)keyData.data(), keyData.size());
					}
				}
				catch (AampJsonParseException& e)
				{
					AAMPLOG_WARN("Failed to parse JSON response (%s)", jsonStr.c_str());
				}
			}
#endif
			AAMPLOG_INFO("license acquisition completed");
			drmHelper->transformLicenseResponse(licenseResponse);
		}
		else
		{
			if(!isLicenseRenewal)
			{
				aamp->profiler.ProfileError(PROFILE_BUCKET_LA_NETWORK, httpResponseCode);
				aamp->profiler.ProfileEnd(PROFILE_BUCKET_LA_NETWORK);
			}
			AAMPLOG_ERR("Error!! Invalid License Response was provided by the Server");
			
			//Handle secmanager specific error codes here
			if(ISCONFIGSET(eAAMPConfig_UseSecManager))
			{
				eventHandle->setResponseCode(httpResponseCode);
				eventHandle->setSecManagerReasonCode(httpExtendedStatusCode);
				if(SECMANAGER_DRM_FAILURE == httpResponseCode && SECMANAGER_ENTITLEMENT_FAILURE == httpExtendedStatusCode)
				{
					if (eventHandle->getFailure() != AAMP_TUNE_FAILED_TO_GET_ACCESS_TOKEN)
					{
						eventHandle->setFailure(AAMP_TUNE_AUTHORIZATION_FAILURE);
					}
					AAMPLOG_WARN("DRM session for %s, Authorization failed", drmSessionContexts[sessionSlot].drmSession->getKeySystem().c_str());
				}
				else if(SECMANAGER_DRM_FAILURE == httpResponseCode && SECMANAGER_SERVICE_TIMEOUT == httpExtendedStatusCode)
				{
					eventHandle->setFailure(AAMP_TUNE_LICENCE_TIMEOUT);
				}
				else
				{
					eventHandle->setFailure(AAMP_TUNE_LICENCE_REQUEST_FAILED);
				}
			}
			else if (412 == httpResponseCode)
			{
				if (eventHandle->getFailure() != AAMP_TUNE_FAILED_TO_GET_ACCESS_TOKEN)
				{
					eventHandle->setFailure(AAMP_TUNE_AUTHORIZATION_FAILURE);
				}
				AAMPLOG_WARN("DRM session for %s, Authorization failed", drmSessionContexts[sessionSlot].drmSession->getKeySystem().c_str());

			}
			else if (CURLE_OPERATION_TIMEDOUT == httpResponseCode)
			{
				eventHandle->setFailure(AAMP_TUNE_LICENCE_TIMEOUT);
			}
			else if(CURLE_ABORTED_BY_CALLBACK == httpResponseCode || CURLE_WRITE_ERROR == httpResponseCode)
			{
				// Set failure reason as AAMP_TUNE_DRM_SELF_ABORT to avoid unnecessary error reporting.
				eventHandle->setFailure(AAMP_TUNE_DRM_SELF_ABORT);
				eventHandle->setResponseCode(httpResponseCode);
			}
			else
			{
				eventHandle->setFailure(AAMP_TUNE_LICENCE_REQUEST_FAILED);
				eventHandle->setResponseCode(httpResponseCode);
			}
			return KEY_ERROR;
		}
	}

	return processLicenseResponse(drmHelper, sessionSlot, cdmError, licenseResponse, eventHandle, aamp, isLicenseRenewal);
}


KeyState AampDRMSessionManager::processLicenseResponse(DrmHelperPtr drmHelper, int sessionSlot, int &cdmError,
		shared_ptr<DrmData> licenseResponse, DrmMetaDataEventPtr eventHandle, PrivateInstanceAAMP* aampInstance, bool isLicenseRenewal)
{
	/**
	 * Provide the acquired License response from the DRM license server to the CDM.
	 * For DRMs with external license acquisition, we will provide a NULL response
	 * for processing and the DRM session should await the key from the DRM implementation
	 */
	AAMPLOG_INFO("Updating the license response to the aampDRMSession(CDM)");
	if(!isLicenseRenewal)
	{
		aampInstance->profiler.ProfileBegin(PROFILE_BUCKET_LA_POSTPROC);
	}

	cdmError = drmSessionContexts[sessionSlot].drmSession->processDRMKey(licenseResponse.get(), drmHelper->keyProcessTimeout());

	if(!isLicenseRenewal)
	{
		aampInstance->profiler.ProfileEnd(PROFILE_BUCKET_LA_POSTPROC);
	}

	KeyState code = drmSessionContexts[sessionSlot].drmSession->getState();

	if (code == KEY_ERROR)
	{
		if (AAMP_TUNE_FAILURE_UNKNOWN == eventHandle->getFailure())
		{
			// check if key failure is due to HDCP , if so report it appropriately instead of Failed to get keys
			if (cdmError == HDCP_OUTPUT_PROTECTION_FAILURE || cdmError == HDCP_COMPLIANCE_CHECK_FAILURE)
			{
				eventHandle->setFailure(AAMP_TUNE_HDCP_COMPLIANCE_ERROR);
			}
			else
			{
				eventHandle->setFailure(AAMP_TUNE_DRM_KEY_UPDATE_FAILED);
			}
		}
	}
	else if (code == KEY_PENDING)
	{
		AAMPLOG_ERR(" Failed to get DRM keys");
		if (AAMP_TUNE_FAILURE_UNKNOWN == eventHandle->getFailure())
		{
			eventHandle->setFailure(AAMP_TUNE_INVALID_DRM_KEY);
		}
	}

	return code;
}

/**
 * @brief Configure the Drm license server parameters for URL/proxy and custom http request headers
 */
bool AampDRMSessionManager::configureLicenseServerParameters(DrmHelperPtr drmHelper, LicenseRequest &licenseRequest,
		std::string &licenseServerProxy, const ChallengeInfo& challengeInfo, PrivateInstanceAAMP* aampInstance)
{
	string contentMetaData = drmHelper->getDrmMetaData();
	bool isContentMetadataAvailable = !contentMetaData.empty();

	if (!contentMetaData.empty())
	{
		if (!licenseRequest.url.empty())
		{
#if defined(USE_SECCLIENT) || defined(USE_SECMANAGER)
			licenseRequest.url = getFormattedLicenseServerURL(licenseRequest.url);
#endif
		}
	}

	// 1. Add any custom License Headers for with and without ContentMetadata license request
	// 2. In addition for ContentMetadata license, add additional headers if present
	{
		std::unordered_map<std::string, std::vector<std::string>> customHeaders;
		aampInstance->GetCustomLicenseHeaders(customHeaders);

		if (!customHeaders.empty())
		{
			// update headers with custom headers
			for ( auto& entry : customHeaders)
			{
				licenseRequest.headers[entry.first] = entry.second;

			}
		}

		if (isContentMetadataAvailable)
		{
			std::string customData;
			if (customHeaders.empty())
			{
				// Not using custom headers, These headers will also override any headers from the helper
				licenseRequest.headers.clear();
			}
			// read License request Headers
			customData = aampInstance->mConfig->GetConfigValue(eAAMPConfig_LRHAcceptValue);
			if (!customData.empty())
			{
				licenseRequest.headers.insert({LICENCE_REQUEST_HEADER_ACCEPT, {customData.c_str()}});
			}

			// read license request content type
			customData = aampInstance->mConfig->GetConfigValue(eAAMPConfig_LRHContentType);
			if (!customData.empty())
			{
				licenseRequest.headers.insert({LICENCE_REQUEST_HEADER_CONTENT_TYPE, {customData.c_str()}});
			}
		}

		// send user agent
		if(aampInstance->mConfig->IsConfigSet(eAAMPConfig_SendUserAgent))
		{
			std::string customData = aampInstance->mConfig->GetUserAgentString();
			if (!customData.empty())
			{
				licenseRequest.headers.insert({LICENCE_REQUEST_USER_AGENT, {customData.c_str()}});
			}
		}

		// license Server Proxy need to be applied for both request , with and without contentMetadata
		licenseServerProxy = aampInstance->GetLicenseReqProxy();
	}

	return isContentMetadataAvailable;
}

/**
 * @brief Re-use the current seesion ID, watermarking variables and de-activate watermarking session status
 */
void AampDRMSessionManager::notifyCleanup()
{
#ifdef USE_SECMANAGER
	auto localSession = mAampSecManagerSession; //Remove potential isSessionValid(), getSessionID() race by using a local copy
	if(localSession.isSessionValid())
	{
		// Set current session to inactive
		AAMPLOG_WARN("De-activate DRM session [%" PRId64 "] and watermark", localSession.getSessionID() );
		AampSecManager::GetInstance()->UpdateSessionState(localSession.getSessionID(), false);
		// Reset the session ID, the session ID is preserved within DrmSession instances
		mAampSecManagerSession.setSessionInvalid();	//note this doesn't necessarily close the session as the session ID is also saved in the slot
		mCurrentSpeed = 0;
		mFirstFrameSeen = false;
	}
#endif
}

void AampDRMSessionManager::ContentProtectionDataUpdate(PrivateInstanceAAMP* aampInstance, std::vector<uint8_t> keyId, AampMediaType streamType)
{
	std::string contentType = GetMediaTypeName(streamType);
	int iter1 = 0;
	bool DRM_Config_Available = false;

	while (iter1 < aampInstance->vDynamicDrmData.size())
	{
		DynamicDrmInfo dynamicDrmCache = aampInstance->vDynamicDrmData.at(iter1);
		if(keyId == dynamicDrmCache.keyID)
		{
			AAMPLOG_WARN("DrmConfig already present in cached data in slot : %d applying config",iter1);
			std::map<std::string,std::string>::iterator itr;
			for(itr = dynamicDrmCache.licenseEndPoint.begin();itr!=dynamicDrmCache.licenseEndPoint.end();itr++)
			{
				if(strcasecmp("com.microsoft.playready",itr->first.c_str())==0)
				{
					aampInstance->mConfig->SetConfigValue(AAMP_APPLICATION_SETTING,eAAMPConfig_PRLicenseServerUrl,itr->second);
				}
				if(strcasecmp("com.widevine.alpha",itr->first.c_str())==0)
				{
					aampInstance->mConfig->SetConfigValue(AAMP_APPLICATION_SETTING,eAAMPConfig_WVLicenseServerUrl,itr->second);
				}
				if(strcasecmp("org.w3.clearkey",itr->first.c_str())==0)
				{
					aampInstance->mConfig->SetConfigValue(AAMP_APPLICATION_SETTING,eAAMPConfig_CKLicenseServerUrl,itr->second);
				}
			}
			aampInstance->mConfig->SetConfigValue(AAMP_APPLICATION_SETTING,eAAMPConfig_AuthToken,dynamicDrmCache.authToken);
			aampInstance->mConfig->SetConfigValue(AAMP_APPLICATION_SETTING,eAAMPConfig_CustomLicenseData,dynamicDrmCache.customData);
			DRM_Config_Available = true;
			break;
		}
		iter1++;
	}
	if(!DRM_Config_Available) {
		ContentProtectionDataEventPtr eventData = std::make_shared<ContentProtectionDataEvent>(keyId, contentType, aampInstance->GetSessionId());
		std::string keyIdDebugStr = AampLogManager::getHexDebugStr(keyId);
		std::unique_lock<std::recursive_mutex> lock(aampInstance->mDynamicDrmUpdateLock);
		int drmUpdateTimeout = aampInstance->mConfig->GetConfigValue(eAAMPConfig_ContentProtectionDataUpdateTimeout);
		AAMPLOG_WARN("Timeout Wait for DRM config message from application :%d",drmUpdateTimeout);
		AAMPLOG_INFO("Found new KeyId %s and not in drm config cache, sending ContentProtectionDataEvent to App", keyIdDebugStr.c_str());
		aampInstance->SendEvent(eventData);
		if( std::cv_status::timeout ==
		   aampInstance->mWaitForDynamicDRMToUpdate.wait_for(
															 lock,
															std::chrono::milliseconds(drmUpdateTimeout) ) )
		{
			AAMPLOG_WARN("cond_timedwait returned TIMEOUT, Applying Default config");
			DynamicDrmInfo dynamicDrmCache = aampInstance->mDynamicDrmDefaultconfig;
			std::map<std::string,std::string>::iterator itr;
			for(itr = dynamicDrmCache.licenseEndPoint.begin();itr!=dynamicDrmCache.licenseEndPoint.end();itr++) {
				if(strcasecmp("com.microsoft.playready",itr->first.c_str())==0) {
					aampInstance->mConfig->SetConfigValue(AAMP_APPLICATION_SETTING,eAAMPConfig_PRLicenseServerUrl,itr->second);
				}
				if(strcasecmp("com.widevine.alpha",itr->first.c_str())==0) {
					aampInstance->mConfig->SetConfigValue(AAMP_APPLICATION_SETTING,eAAMPConfig_WVLicenseServerUrl,itr->second);
				}
				if(strcasecmp("org.w3.clearkey",itr->first.c_str())==0) {
					aampInstance->mConfig->SetConfigValue(AAMP_APPLICATION_SETTING,eAAMPConfig_CKLicenseServerUrl,itr->second);
				}
			}
			aampInstance->mConfig->SetConfigValue(AAMP_APPLICATION_SETTING,eAAMPConfig_AuthToken,dynamicDrmCache.authToken);
			aampInstance->mConfig->SetConfigValue(AAMP_APPLICATION_SETTING,eAAMPConfig_CustomLicenseData,dynamicDrmCache.customData);
		}
		else
		{
			AAMPLOG_WARN("%s:%d [WARN] cond_timedwait(dynamicDrmUpdate) returned success!", __FUNCTION__, __LINE__);
		}
	}
}

/**
 * @brief To update the max DRM sessions supported
 *
 * @param[in] maxSessions max DRM Sessions
 */
void AampDRMSessionManager::UpdateMaxDRMSessions(int maxSessions)
{
	if (mMaxDRMSessions != maxSessions)
	{
		// Clean up the current sessions
		clearSessionData();
		SAFE_DELETE_ARRAY(drmSessionContexts);
		SAFE_DELETE_ARRAY(cachedKeyIDs);

		//Update to new session count
		mMaxDRMSessions = maxSessions;
		drmSessionContexts      = new DrmSessionContext[mMaxDRMSessions];
		cachedKeyIDs            = new KeyID[mMaxDRMSessions];
		mLicenseRenewalThreads.resize(mMaxDRMSessions);
		AAMPLOG_INFO("Updated AampDRMSessionManager MaxSession to:%d", mMaxDRMSessions);
	}
}

/**
 * @brief To update the max DRM sessions supported
 *
 * @param[in] requestType DRM License type
 * @param[in] statusCode response code
 * @param[in] licenseRequestUrl max DRM Sessions
 * @param[in] downloadTimeMS total download time
 * @param[in] eventHandle - DRM Metadata event handle
 * @param[in] respData - download response data
 */
void AampDRMSessionManager::UpdateLicenseMetrics(DrmRequestType requestType, int32_t statusCode, std::string licenseRequestUrl, long long downloadTimeMS, DrmMetaDataEventPtr eventHandle, DownloadResponsePtr respData )
{
	//Convert to JSON format
	cJSON *item = nullptr;
	if( nullptr == eventHandle)
	{
		return;
	}
	item = cJSON_CreateObject();
	if( nullptr != item)
	{
		cJSON_AddNumberToObject(item, "req",requestType);
		cJSON_AddNumberToObject(item, "res", statusCode);
		cJSON_AddNumberToObject(item, "tot",downloadTimeMS);
		cJSON_AddStringToObject(item, "url",licenseRequestUrl.c_str());

		if( (nullptr != respData) && (DRM_GET_LICENSE == requestType))
		{
			cJSON_AddNumberToObject(item, "con", respData->downloadCompleteMetrics.connect);
			cJSON_AddNumberToObject(item, "str", respData->downloadCompleteMetrics.startTransfer);
			cJSON_AddNumberToObject(item, "res", respData->downloadCompleteMetrics.resolve);
			cJSON_AddNumberToObject(item, "acn", respData->downloadCompleteMetrics.appConnect);
			cJSON_AddNumberToObject(item, "ptr", respData->downloadCompleteMetrics.preTransfer);
			cJSON_AddNumberToObject(item, "rdt", respData->downloadCompleteMetrics.redirect);
			cJSON_AddNumberToObject(item, "dls", respData->downloadCompleteMetrics.dlSize);
			cJSON_AddNumberToObject(item, "rqs", respData->downloadCompleteMetrics.reqSize);
		}

		char *jsonStr = cJSON_Print(item);
		if (jsonStr)
		{
			eventHandle->setNetworkMetricData(jsonStr);
			free(jsonStr);
		}
		cJSON_Delete(item);
	}
}

