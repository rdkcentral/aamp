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
 * @file AampLicManager.cpp
 * @brief Source file for LicenseManager.
 */
#include "AampDRMLicManager.h"
#include "priv_aamp.h"   
#include "DrmHelper.h"
#include <pthread.h>
#include "downloader/AampCurlStore.h"
#include "_base64.h"
#include "PlayerSecManager.h"
#include "AampStreamSinkManager.h"
#include "AampJsonObject.h"
#include "AampConfig.h"




#define SESSION_TOKEN_URL "http://localhost:50050/authService/getSessionToken"

#define LICENCE_REQUEST_HEADER_ACCEPT "Accept:"

#define LICENCE_REQUEST_HEADER_CONTENT_TYPE "Content-Type:"
#define LICENCE_REQUEST_USER_AGENT "User-Agent:"

#define LICENCE_RESPONSE_JSON_LICENCE_KEY "license"
/** 
 * registerCb - register callbacks between player and middleware DRM component 
 */
static void  registerCb(AampDRMLicenseManager* _this, DrmSessionManager* instance)
{
      /* Register the callback for acquire license data */
    instance->RegisterLicenseDataCb([_this](std::shared_ptr<DrmHelper> drmHelper, int sessionSlot, int &cdmError, int streamType,void *metaDataPtr, bool isLicenseRenewal = false ) -> KeyState {
        return _this->acquireLicense(drmHelper, sessionSlot, cdmError,
                                      (AampMediaType)streamType,metaDataPtr, false);
    });

    /** Profiler update callback */
    instance->RegisterProfUpdate([_this](){
      _this->ProfilerUpdate();
      });
    /** Content Protection Callback */
    instance->RegisterContentUpdateCallback([_this](std::shared_ptr<DrmHelper> drmHelper, int streamType, std::vector<uint8_t> keyId, int contentProtectionUpd)->std::string    {
		    return _this->HandleContentProtectionData(drmHelper, streamType, keyId, contentProtectionUpd);
     });
}
/**
 *  getConfigs - To feed the configs to middleware DRM 
 * */
void getConfigs(DrmSessionManager *mDrmSessionManager , PrivateInstanceAAMP *aampInstance)
{
	mDrmSessionManager->UpdateDRMConfig(
        aampInstance->mConfig->IsConfigSet(eAAMPConfig_UseSecManager),
        aampInstance->mConfig->GetConfigValue(eAAMPConfig_LicenseRetryWaitTime),
        aampInstance->mConfig->GetConfigValue(eAAMPConfig_DrmNetworkTimeout),
        aampInstance->mConfig->GetConfigValue(eAAMPConfig_Curl_ConnectTimeout),
        aampInstance->mConfig->IsConfigSet(eAAMPConfig_CurlLicenseLogging),
        aampInstance->mConfig->IsConfigSet(eAAMPConfig_RuntimeDRMConfig),
        aampInstance->mConfig->GetConfigValue(eAAMPConfig_ContentProtectionDataUpdateTimeout),
        aampInstance->mConfig->IsConfigSet(eAAMPConfig_EnablePROutputProtection),
        aampInstance->mConfig->IsConfigSet(eAAMPConfig_PropagateURIParam),
        aampInstance->mIsFakeTune
    );
}
/**
 *  @brief AampDRMLicenseManager constructor.
 */
AampDRMLicenseManager::AampDRMLicenseManager(int maxDrmSessions, PrivateInstanceAAMP *aamp) : mMaxDRMSessions(maxDrmSessions),
		aampInstance(aamp), mDrmSessionManager(NULL)
{
    aampInstance = aamp; 
	std::function<void(uint32_t,uint32_t,const std::string&)> waterMarkSessionUpdateCB = std::bind(&PrivateInstanceAAMP::SendWatermarkSessionUpdateEvent, aampInstance, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
    mDrmSessionManager = new DrmSessionManager(maxDrmSessions ,aampInstance, waterMarkSessionUpdateCB);
    registerCb(this, mDrmSessionManager);
    getConfigs(mDrmSessionManager, aampInstance);
    
    mLicenseDownloader = new AampCurlDownloader[mMaxDRMSessions];
    mLicensePrefetcher = new AampLicensePreFetcher(aampInstance);
    mLicensePrefetcher->Init();
}

/**
 *  @brief AampDRMLicenseManager destructor
 */
AampDRMLicenseManager::~AampDRMLicenseManager()
{
	SAFE_DELETE(mDrmSessionManager);
        SAFE_DELETE(mLicensePrefetcher);
	releaseLicenseRenewalThreads();
	for(int i = 0 ; i < mMaxDRMSessions;i++)  
        {

             mLicenseDownloader[i].Release();
	}
}

/**
 *  @brief Clean up the license renewal threads created.
 */
void AampDRMLicenseManager::releaseLicenseRenewalThreads()
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

/**
 * @brief Get Session abort flag
 */
void AampDRMLicenseManager::setLicenseRequestAbort(bool isAbort)
{
	mAccessTokenConnector.Release();
	licenseRequestAbort = isAbort;
}
void AampDRMLicenseManager::licenseRenewalThread(std::shared_ptr<DrmHelper> drmHelper, int sessionSlot, PrivateInstanceAAMP* aampInstance)
{
	bool isSecClientError = false;
	//isSecClientError = true; //for secmanager
	DrmMetaDataEventPtr e = std::make_shared<DrmMetaDataEvent>(AAMP_TUNE_FAILURE_UNKNOWN, "", 0, 0, isSecClientError, aampInstance->GetSessionId());
	int cdmError = -1;
	KeyState code = acquireLicense(drmHelper, sessionSlot, cdmError,  eMEDIATYPE_LICENCE,(void*)e.get() ,true);
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
void AampDRMLicenseManager::renewLicense(std::shared_ptr<DrmHelper> drmHelper, void* userData, PrivateInstanceAAMP* aampInstance)
{
	DrmSession* session = static_cast<DrmSession*>(userData);
	int sessionSlot = mDrmSessionManager->getSlotIdForSession(session);
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
			mLicenseRenewalThreads[sessionSlot] = std::thread([this, drmHelper, sessionSlot, aampInstance] {
        this->licenseRenewalThread(drmHelper, sessionSlot, aampInstance);
    });

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
 * @brief sent license challenge to the DRM server and provide the response to CDM
 */
KeyState AampDRMLicenseManager::acquireLicense(std::shared_ptr<DrmHelper> drmHelper, int sessionSlot, int &cdmError,
	 AampMediaType streamType, void *metaDataPtr,  bool isLicenseRenewal)
{
	DrmMetaDataEventPtr* eventHandlePtr = static_cast<DrmMetaDataEventPtr*>(metaDataPtr);
	DrmMetaDataEventPtr& eventHandle = *eventHandlePtr;

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
                std::lock_guard<std::mutex> guard(mDrmSessionManager->drmSessionContexts[sessionSlot].sessionMutex);
		/**
		 * Generate a License challenge from the CDM
		 */
		AAMPLOG_INFO("Request to generate license challenge to the aampDRMSession(CDM)");

		ChallengeInfo challengeInfo;
		challengeInfo.data.reset(mDrmSessionManager->drmSessionContexts[sessionSlot].drmSession->generateKeyRequest(challengeInfo.url, drmHelper->licenseGenerateTimeout()));
		code = mDrmSessionManager->drmSessionContexts[sessionSlot].drmSession->getState();

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
				{
					if(usingAppDefinedAuthToken)
					{
						AAMPLOG_WARN("Ignore  AuthToken Provided for non-ContentMetadata DRM license request");
					}
					
				      eventHandle->setSecclientError(false);
			              licenseResponse.reset(getLicense(licenseRequest, &httpResponseCode, streamType, aampInstance, eventHandle, &mLicenseDownloader[sessionSlot],licenseServerProxy));
				}

			}
		}
	}

	if (code == KEY_PENDING)
	{
		code = handleLicenseResponse(drmHelper, sessionSlot, cdmError, httpResponseCode, httpExtendedStatusCode, licenseResponse, eventHandle,  isLicenseRenewal);
	}
	return code;
}
KeyState AampDRMLicenseManager::handleLicenseResponse(std::shared_ptr<DrmHelper> drmHelper, int sessionSlot, int &cdmError, int32_t httpResponseCode, int32_t httpExtendedStatusCode, shared_ptr<DrmData> licenseResponse, DrmMetaDataEventPtr eventHandle,  bool isLicenseRenewal)
{
	if (!drmHelper->isExternalLicense())
	{
		if ((licenseResponse != NULL) && (licenseResponse->getDataLength() != 0))
		{
			if(!isLicenseRenewal)
			{
				aampInstance->profiler.ProfileEnd(PROFILE_BUCKET_LA_NETWORK);
			}
			if (!drmHelper->getDrmMetaData().empty() || aampInstance->mConfig->IsConfigSet(eAAMPConfig_Base64LicenseWrapping))
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
			AAMPLOG_INFO("license acquisition completed");
			drmHelper->transformLicenseResponse(licenseResponse);
		}
		else
		{
			if(!isLicenseRenewal)
			{
				aampInstance->profiler.ProfileError(PROFILE_BUCKET_LA_NETWORK, httpResponseCode);
				aampInstance->profiler.ProfileEnd(PROFILE_BUCKET_LA_NETWORK);
			}
			AAMPLOG_ERR("Error!! Invalid License Response was provided by the Server");
			
			//Handle secmanager specific error codes here
			if( aampInstance->mConfig->IsConfigSet(eAAMPConfig_UseSecManager))
			{
				eventHandle->setResponseCode(httpResponseCode);
				eventHandle->setSecManagerReasonCode(httpExtendedStatusCode);
				if(SECMANAGER_DRM_FAILURE == httpResponseCode && SECMANAGER_ENTITLEMENT_FAILURE == httpExtendedStatusCode)
				{
					if (eventHandle->getFailure() != AAMP_TUNE_FAILED_TO_GET_ACCESS_TOKEN)
					{
						eventHandle->setFailure(AAMP_TUNE_AUTHORIZATION_FAILURE);
					}
					AAMPLOG_WARN("DRM session for %s, Authorization failed", mDrmSessionManager->drmSessionContexts[sessionSlot].drmSession->getKeySystem().c_str());
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
				AAMPLOG_WARN("DRM session for %s, Authorization failed",mDrmSessionManager->drmSessionContexts[sessionSlot].drmSession->getKeySystem().c_str());

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
      return processLicenseResponse(drmHelper, sessionSlot, cdmError, licenseResponse, eventHandle, isLicenseRenewal);
}
KeyState AampDRMLicenseManager::processLicenseResponse(std::shared_ptr<DrmHelper> drmHelper, int sessionSlot, int &cdmError,
		shared_ptr<DrmData> licenseResponse, DrmMetaDataEventPtr eventHandle, bool isLicenseRenewal)
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

	cdmError = mDrmSessionManager->drmSessionContexts[sessionSlot].drmSession->processDRMKey(licenseResponse.get(), drmHelper->keyProcessTimeout());

	if(!isLicenseRenewal)
	{
		aampInstance->profiler.ProfileEnd(PROFILE_BUCKET_LA_POSTPROC);
	}

	KeyState code = mDrmSessionManager->drmSessionContexts[sessionSlot].drmSession->getState();

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
 * @brief To update the max DRM sessions supported
 *
 * @param[in] requestType DRM License type
 * @param[in] statusCode response code
 * @param[in] licenseRequestUrl max DRM Sessions
 * @param[in] downloadTimeMS total download time
 * @param[in] eventHandle - DRM Metadata event handle
 * @param[in] respData - download response data
 */
void AampDRMLicenseManager::UpdateLicenseMetrics(DrmRequestType requestType, int32_t statusCode, std::string licenseRequestUrl, long long downloadTimeMS, DrmMetaDataEventPtr eventHandle, DownloadResponsePtr respData )
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
/**
 *  @brief	Extract substring between (excluding) two string delimiters.
 *
 *  @param[in]	parentStr - Parent string from which substring is extracted.
 *  @param[in]	startStr, endStr - String delimiters.
 *  @return	Returns the extracted substring; Empty string if delimiters not found.
 */
string extractSubstring(string parentStr, string startStr, string endStr)
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
const char * AampDRMLicenseManager::getAccessToken(int &tokenLen, int &error_code , bool bSslPeerVerify)
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
				string tokenStatusCode = extractSubstring(tokenReplyStr, "status\":", ",\"");
				if(tokenStatusCode.length() == 0)
				{
					//StatusCode could be last element in the json
					tokenStatusCode = extractSubstring(tokenReplyStr, "status\":", "}");
				}
				if(tokenStatusCode.length() == 1 && tokenStatusCode.c_str()[0] == '0')
				{
					string token = extractSubstring(tokenReplyStr, "token\":\"", "\"");
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
 * @brief Configure the Drm license server parameters for URL/proxy and custom http request headers
 */
bool AampDRMLicenseManager::configureLicenseServerParameters(std::shared_ptr<DrmHelper> drmHelper, LicenseRequest &licenseRequest,
		std::string &licenseServerProxy, const ChallengeInfo& challengeInfo, PrivateInstanceAAMP* aampInstance)
{
	string contentMetaData = drmHelper->getDrmMetaData();
	bool isContentMetadataAvailable = !contentMetaData.empty();

	if (!contentMetaData.empty())
	{
		if (!licenseRequest.url.empty())
		{
			if( isSecFeatureEnabled() )
			{
				licenseRequest.url = getFormattedLicenseServerURL(licenseRequest.url);
			}			
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
void AampDRMLicenseManager::ContentProtectionDataUpdate(PrivateInstanceAAMP* aampInstance, std::vector<uint8_t> keyId, AampMediaType streamType)
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
/*
 * @brief Set the Common Key Duration object
 * 
 * @param keyDuration key duration
 */
void AampDRMLicenseManager::SetCommonKeyDuration(int keyDuration)
{
	mLicensePrefetcher->SetCommonKeyDuration(keyDuration);
}

/**
 * @brief Stop DRM session manager and terminate license fetcher
 *
 * @param none
 * @return none
 */
void AampDRMLicenseManager::Stop()
{
	mLicensePrefetcher->Term();
}
/**
 * @brief set license fetcher object
 * 
 * @return none
 */
void AampDRMLicenseManager::SetLicenseFetcher(AampLicenseFetcher *fetcherInstance)
{
	mLicensePrefetcher->SetLicenseFetcher(fetcherInstance);
}

/**
 * @brief Set to true if error event to be sent to application if any license request fails
 *  Otherwise, error event will be sent if a track doesn't have a successful or pending license request
 * 
 * @param sendErrorOnFailure key duration
 */
void AampDRMLicenseManager::SetSendErrorOnFailure(bool sendErrorOnFailure)
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
bool AampDRMLicenseManager::QueueContentProtection(std::shared_ptr<DrmHelper> drmHelper, std::string periodId, uint32_t adapIdx, AampMediaType type, bool isVssPeriod)
{
	return mLicensePrefetcher->QueueContentProtection(drmHelper, periodId, adapIdx, type, isVssPeriod);
}

/**
 *  @brief Get DRM license key from DRM server.
 */
DrmData * AampDRMLicenseManager::getLicense(LicenseRequest &licenseRequest,
		int32_t *httpCode, AampMediaType streamType, void* aampI, DrmMetaDataEventPtr eventHandle, AampCurlDownloader *pLicenseDownloader, std::string licenseProxy)
{

	CURLcode res;
	double totalTime = 0;
	DrmData * keyInfo = NULL;
	bool bNeedResponseHeadersTobeShared 	= aampInstance->mConfig->IsConfigSet(eAAMPConfig_SendLicenseResponseHeaders);
	DownloadResponsePtr respData 	=	std::make_shared<DownloadResponse> ();		
	// Initialize the Seesion Token Connector
	DownloadConfigPtr inpData 	=	std::make_shared<DownloadConfig> ();
	inpData->bIgnoreResponseHeader				=	!bNeedResponseHeadersTobeShared;
	inpData->iDownloadTimeout				=	aampInstance->mConfig->GetConfigValue(eAAMPConfig_DrmNetworkTimeout);
	inpData->iStallTimeout = aampInstance->mConfig->GetConfigValue(eAAMPConfig_DrmStallTimeout);
	inpData->iStartTimeout = aampInstance->mConfig->GetConfigValue(eAAMPConfig_DrmStartTimeout);
	inpData->iCurlConnectionTimeout =  aampInstance->mConfig->GetConfigValue(eAAMPConfig_Curl_ConnectTimeout);
	inpData->bNeedDownloadMetrics				=	true;
	inpData->proxyName							=	licenseProxy;		
	inpData->pCurl			=	CurlStore::GetCurlStoreInstance(aampInstance).GetCurlHandle(aampInstance, licenseRequest.url, eCURLINSTANCE_AES);
	inpData->sCustomHeaders	=	licenseRequest.headers;
	AAMPLOG_TRACE("DRMSession-getLicense download params - StallTimeout : %d StartTimeout : %d DownloadTimeout : %d CurlConnectionTimeout : %d ",inpData->iStallTimeout,inpData->iStartTimeout,inpData->iDownloadTimeout,inpData->iCurlConnectionTimeout);
	if (aampInstance->mConfig->IsConfigSet(eAAMPConfig_CurlLicenseLogging))
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
	
	inpData->bSSLVerifyPeer		=	aampInstance->mConfig->IsConfigSet(eAAMPConfig_SslVerifyPeer);	
	if(licenseRequest.method == LicenseRequest::POST)
	{
		inpData->eRequestType 	=	eCURL_POST;
		if(aampInstance->mConfig->IsConfigSet(eAAMPConfig_Base64LicenseWrapping))
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
				int  licenseRetryWaitTime = aampInstance->mConfig->GetConfigValue(eAAMPConfig_LicenseRetryWaitTime) ;
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
				if(responseData.empty() != true  && respData != NULL && bNeedResponseHeadersTobeShared )
				{
					AAMPLOG_WARN("Setting Body Response for  acquireLicense FAILED response %s:%zu", responseData.c_str(), responseData.length());
					eventHandle->setBodyResponse(responseData);
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
		if (!aampInstance->GetAppName().empty())
		{
			// append app name with class data
			appName = aampInstance->GetAppName() + ",";
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
	CurlStore::GetCurlStoreInstance(aampInstance).SaveCurlHandle(aampInstance, licenseRequest.url, eCURLINSTANCE_AES, inpData->pCurl);
	// Filled in KeyInfo is returned back 
	return keyInfo;
}

DrmData * AampDRMLicenseManager::getLicenseSec(const LicenseRequest &licenseRequest, std::shared_ptr<DrmHelper> drmHelper,
		const ChallengeInfo& challengeInfo, void* aampI, int32_t *httpCode, int32_t *httpExtStatusCode, DrmMetaDataEventPtr eventHandle)
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

	int sleepTime = aampInstance->mConfig->GetConfigValue(eAAMPConfig_LicenseRetryWaitTime);
	if(sleepTime<=0) sleepTime = 100;

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
	if(aampInstance->mConfig->IsConfigSet(eAAMPConfig_UseSecManager))
	{
		size_t encodedDataLen = ((contentMetaData.length() + 2) /3) * 4;
		size_t encodedChallengeDataLen = ((challengeInfo.data->getDataLength() + 2) /3) * 4;
		int32_t statusCode;
		int32_t reasonCode;
		int32_t businessStatus;

		if (!mDrmSessionManager->mPlayerSecManagerSession.isSessionValid())
		{
			// if we're about to get a licence and are not re-using a session, then we have not seen the first video frame yet. Do not allow watermarking to get enabled yet.
			bool videoMuteState = mIsVideoOnMute;
			AAMPLOG_WARN("First frame flag cleared before AcquireLicense, with mIsVideoOnMute=%d", videoMuteState);
			mFirstFrameSeen = false;
		}

		tStartTime = NOW_STEADY_TS_MS;
		bool res = PlayerSecManager::GetInstance()->AcquireLicense(licenseRequest.url.c_str(),
																 requestMetadata,
																 ((numberOfAccessAttributes == 0) ? NULL : accessAttributes),
																 encodedData, encodedDataLen,
																 encodedChallengeData, encodedChallengeDataLen,
																 keySystem,
																 mediaUsage,
																 secclientSessionToken, challengeInfo.accessToken.length(),
																 mDrmSessionManager->mPlayerSecManagerSession,
																 &licenseResponseStr, &licenseResponseLength,
																 &statusCode, &reasonCode, &businessStatus, mIsVideoOnMute, sleepTime);
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
	else
	{
		int32_t sec_client_result = 0;
		PlayerSecExtendedStatus statusInfo;
		unsigned int attemptCount = 0;
		tStartTime = NOW_STEADY_TS_MS;
		while (attemptCount < MAX_LICENSE_REQUEST_ATTEMPTS)
		{
			attemptCount++;
			sec_client_result = mDrmSessionManager->playerSecInstance->PlayerSec_AcquireLicense(licenseRequest.url.c_str(), 1,
														 requestMetadata, numberOfAccessAttributes,
														 ((numberOfAccessAttributes == 0) ? NULL : accessAttributes),
														 encodedData,
														 strlen(encodedData),
														 encodedChallengeData, strlen(encodedChallengeData), keySystem, mediaUsage,
														 secclientSessionToken,
														 &licenseResponseStr, &licenseResponseLength, &refreshDuration, &statusInfo);
			if (((sec_client_result >= 500 && sec_client_result < 600)||
				 ( mDrmSessionManager->playerSecInstance->isSecResultInRange(sec_client_result)))
				&& attemptCount < MAX_LICENSE_REQUEST_ATTEMPTS)
			{
				AAMPLOG_ERR(" acquireLicense FAILED! license request attempt : %d; response code : sec_client %d", attemptCount, sec_client_result);
				if (licenseResponseStr)
				{
					mDrmSessionManager->playerSecInstance->PlayerSec_FreeResource(licenseResponseStr);
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

		if ( mDrmSessionManager->playerSecInstance-> isSecRequestFailed(sec_client_result))
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
		if (licenseResponseStr) mDrmSessionManager->playerSecInstance->PlayerSec_FreeResource(licenseResponseStr);
	}
	UpdateLicenseMetrics(DRM_GET_LICENSE_SEC, *httpCode, licenseRequest.url.c_str(), downloadTimeMS, eventHandle, nullptr );

	free(encodedData);
	free(encodedChallengeData);
	return licenseResponse;
}
/*
 * @brief callback for to do profiling from middleware 
 */
void AampDRMLicenseManager::ProfilerUpdate()
{

  aampInstance->profiler.ProfileBegin(PROFILE_BUCKET_LA_PREPROC);
}
std::string  AampDRMLicenseManager::HandleContentProtectionData(std::shared_ptr<DrmHelper> drmHelper, int streamType, std::vector<uint8_t> keyId, int contentProtectionUpd)
{
	 /* To fetch correct codec type in tune time metrics when drm data is not given in manifest*/
	 aampInstance->setCurrentDrm(drmHelper);

	bool RuntimeDRMConfigSupported = aampInstance->mConfig->IsConfigSet(eAAMPConfig_RuntimeDRMConfig);
	if(contentProtectionUpd)
	{
	    if(RuntimeDRMConfigSupported && aampInstance->IsEventListenerAvailable(AAMP_EVENT_CONTENT_PROTECTION_DATA_UPDATE) && (streamType < 4))
	    {
	    	aampInstance->mcurrent_keyIdArray = keyId;
	    	AAMPLOG_INFO("App registered the ContentProtectionDataEvent to send new drm config");
	    	ContentProtectionDataUpdate(aampInstance, keyId, (AampMediaType)streamType);
	    	aampInstance->mcurrent_keyIdArray.clear();
	    }
	}
	std::string customData = aampInstance->GetLicenseCustomData();
    return customData;
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
void AampDRMLicenseManager::QueueProtectionEvent(std::shared_ptr<DrmHelper> drmHelper, std::string periodId, uint32_t adapIdx, AampMediaType type)
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
 *  @brief To Set Playback Speed State
 */
void AampDRMLicenseManager::setPlaybackSpeedState(bool live, double currentLatency, bool livepoint , double liveOffsetMs,int speed, double positionMs, bool firstFrameSeen)
{
	mDrmSessionManager->setPlaybackSpeedState(live, currentLatency, livepoint, liveOffsetMs, speed, positionMs, firstFrameSeen);
}

/**
 * @brief De-activate watermark and prevent it from being re-enabled until we get a new first video frame at normal play speed
 */
void AampDRMLicenseManager::hideWatermarkOnDetach(void)
{
	mDrmSessionManager->hideWatermarkOnDetach( );
}

/**
 * @brief Deactivate the session while video on mute and then activate it and update the speed once video is unmuted
 */
void AampDRMLicenseManager::setVideoMute(bool live, double currentLatency, bool livepoint , double liveOffsetMs,bool isVideoOnMute, double positionMs)
{
	mDrmSessionManager->setVideoMute(live, currentLatency, livepoint, liveOffsetMs,isVideoOnMute, positionMs);
}

void AampDRMLicenseManager::setVideoWindowSize(int width, int height)
{
	mDrmSessionManager->setVideoWindowSize(width, height);
}

/**
 * @brief To update the max DRM sessions supported
 *
 * @param[in] maxSessions max DRM Sessions
 */
void AampDRMLicenseManager::UpdateMaxDRMSessions(int maxSessions)
{
	mDrmSessionManager->UpdateMaxDRMSessions(maxSessions);
        mLicenseRenewalThreads.resize(maxSessions);
}

void AampDRMLicenseManager::clearDrmSession(bool forceClearSession)
{
	mDrmSessionManager->clearDrmSession(forceClearSession);
}

/**
 * @brief Clean up the failed keyIds.
 */
void AampDRMLicenseManager::clearFailedKeyIds()
{
	mDrmSessionManager->clearFailedKeyIds();
}

/**
 * @brief Set Session manager state
 */
void AampDRMLicenseManager::setSessionMgrState(SessionMgrState state)
{
	mDrmSessionManager->setSessionMgrState(state);
}

SessionMgrState AampDRMLicenseManager::getSessionMgrState()
{
	if(mDrmSessionManager)
	{
	    return mDrmSessionManager->getSessionMgrState();
	}
	else
	{
	    return SessionMgrState::eSESSIONMGR_INACTIVE;
	}
}
/**
 * @brief Re-use the current seesion ID, watermarking variables and de-activate watermarking session status
 */
void AampDRMLicenseManager::notifyCleanup()
{
	mDrmSessionManager->notifyCleanup();
}
/**
 *  @brief Create DrmSession by using the AampDrmHelper object
 */
DrmSession* AampDRMLicenseManager::createDrmSession( std::shared_ptr<DrmHelper> drmHelper, DrmCallbacks* aampInstance, DrmMetaDataEventPtr eventHandle, int streamTypeIn)
{
	int err = -1;
	void *ptr= static_cast<void*>(&eventHandle);
	DrmSession* session = mDrmSessionManager->createDrmSession(err , drmHelper, aampInstance, streamTypeIn,ptr );
   

	 if(err != -1)
	 {
		 eventHandle->setFailure((AAMPTuneFailure)err);
	 }
	 return session;
}

/**
 *  @brief      Creates and/or returns the DRM session corresponding to keyId (Present in initDataPtr)
 *              AampDRMSession manager has two static AampDrmSession objects.
 *              This method will return the existing DRM session pointer if any one of these static
 *              DRM session objects are created against requested keyId. Binds the oldest DRM Session
 *              with new keyId if no matching keyId is found in existing sessions.
 *  @return     Pointer to DrmSession for the given PSSH data; NULL if session creation/mapping fails.
 */
DrmSession * AampDRMLicenseManager::createDrmSession(
		 const char* systemId, MediaFormat mediaFormat, const unsigned char * initDataPtr,
		uint16_t initDataLen, int streamType,
		DrmCallbacks* aamp, DrmMetaDataEventPtr eventHandle, const unsigned char* contentMetadataPtr,
		 bool isPrimarySession)
{
	int err = -1;
	void *ptr= static_cast<void*>(&eventHandle);
        DrmSession * session = mDrmSessionManager->createDrmSession(err,  systemId,  mediaFormat,  initDataPtr,initDataLen,  streamType, aamp, ptr,  contentMetadataPtr,isPrimarySession);

	if(err != -1)
	{
		 eventHandle->setFailure((AAMPTuneFailure)err);
	}
	return session;
}

AAMPTuneFailure AampDRMLicenseManager::MapDrmToAampTuneFailure(DrmTuneFailure drmError)
{
    switch (drmError)
    {
        case MW_DRM_INIT_FAILED:            return AAMP_TUNE_DRM_INIT_FAILED;
        case MW_DRM_DATA_BIND_FAILED:       return AAMP_TUNE_DRM_DATA_BIND_FAILED;
        case MW_DRM_SESSIONID_EMPTY:        return AAMP_TUNE_DRM_SESSIONID_EMPTY;
        case MW_DRM_CHALLENGE_FAILED:       return AAMP_TUNE_DRM_CHALLENGE_FAILED;
        case MW_INVALID_DRM_KEY:            return AAMP_TUNE_INVALID_DRM_KEY;
        case MW_CORRUPT_DRM_DATA:           return AAMP_TUNE_CORRUPT_DRM_DATA;
        case MW_CORRUPT_DRM_METADATA:       return AAMP_TUNE_CORRUPT_DRM_METADATA;
        case MW_DRM_DECRYPT_FAILED:         return AAMP_TUNE_DRM_DECRYPT_FAILED;
        case MW_DRM_UNSUPPORTED:            return AAMP_TUNE_DRM_UNSUPPORTED;
        case MW_DRM_SELF_ABORT:             return AAMP_TUNE_DRM_SELF_ABORT;
        case MW_DRM_KEY_UPDATE_FAILED:      return AAMP_TUNE_DRM_KEY_UPDATE_FAILED;
        case MW_FAILED_TO_GET_KEYID:        return AAMP_TUNE_FAILED_TO_GET_KEYID;
        case MW_UNTRACKED_DRM_ERROR:        return AAMP_TUNE_UNTRACKED_DRM_ERROR;
        default:                            return AAMP_TUNE_UNTRACKED_DRM_ERROR;
    }
}