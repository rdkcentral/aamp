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
#include "ContentSecurityManager.h"
#include "AampStreamSinkManager.h"
#include "AampJsonObject.h"
#include "AampConfig.h"
#include "PlayerUtils.h"
#include "PlayerSecInterface.h"

#include "AAMPAnomalyMessageType.h"
#include "AuthTokenErrors.h"

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
	AAMPLOG_MIL("[LicMgr] Registering DRM session manager callbacks, licMgr:%p sessionMgr:%p", static_cast<void*>(_this), static_cast<void*>(instance));
	/* Register the callback for acquire license data */
	instance->RegisterLicenseDataCb([_this](int &responseCode, const std::shared_ptr<DrmHelper>& drmHelper, int sessionSlot, int &cdmError, int streamType,void *metaDataPtr, bool isLicenseRenewal = false ) -> KeyState {
			return _this->acquireLicense(responseCode, drmHelper, sessionSlot, cdmError,
					(AampMediaType)streamType,metaDataPtr, false);
			});

	/** Profiler update callback */
	instance->RegisterProfilingUpdateCb([_this](){
			_this->ProfilerUpdate();
			});

	/** Content Protection Callback */
	instance->RegisterHandleContentProtectionCb([_this](const std::shared_ptr<DrmHelper>& drmHelper, int streamType, const std::vector<uint8_t>& keyId, int contentProtectionUpd)->std::string{
			return _this->HandleContentProtectionData(drmHelper, streamType, keyId, contentProtectionUpd);
			});
	/**  Register the profiler update callback for TriggerProfileBeginCb */
	instance->RegisterDecryptProfile([_this](int streamType, int action, int result /* = 0 */){
			_this->TriggerDecryptProfile(streamType, action, result);
			});

	/** Register the profiler update callback for TriggerLAProfileBeginCb */
	instance->RegisterLAProfBegin([_this](int streamType){
			_this->TriggerLAProfileBeginCb(streamType);
			});

	/**  Register the profiler end callback for TriggerLAProfileEndCb */
	instance->RegisterLAProfEnd([_this](int streamType){
			_this->TriggerLAProfileEndCb(streamType);
			});

	/** Register the profiler error callback for TriggerLAProfileErrorCb */
	instance->RegisterLAProfError([_this](void* ptr){
			_this->TriggerLAProfileErrorCb(ptr);
			});

	/**  Register the SetFailure callback for TriggerSetFailureCb */
	instance->RegisterSetFailure([_this](void* ptr, int err){
			_this->TriggerSetFailure(ptr,err);
			});
	/** Register the MetaData callback for TriggerDrmMetaDataEvent */
	instance->RegisterMetaDataCb([_this]() -> std::shared_ptr<void> {
			return _this->TriggerDrmMetaDataEvent();
			});
	AAMPLOG_MIL("[LicMgr] DRM session manager callbacks registered");
}
/**
 *  getConfigs - To feed the configs to middleware DRM 
 * */
void getConfigs(DrmSessionManager *mDrmSessionManager , PrivateInstanceAAMP *aampInstance)
{
	AAMPLOG_MIL("[LicMgr] Feeding configs to DRM session manager: UseSecManager:%d EnablePROutputProtection:%d PropagateURIParam:%d isFakeTune:%d WideVineKIDWorkaround:%d",
			aampInstance->mConfig->IsConfigSet(eAAMPConfig_UseSecManager),
			aampInstance->mConfig->IsConfigSet(eAAMPConfig_EnablePROutputProtection),
			aampInstance->mConfig->IsConfigSet(eAAMPConfig_PropagateURIParam),
			aampInstance->mIsFakeTune,
			aampInstance->mConfig->IsConfigSet(eAAMPConfig_WideVineKIDWorkaround));
	mDrmSessionManager->UpdateDRMConfig(
        		aampInstance->mConfig->IsConfigSet(eAAMPConfig_UseSecManager),
			aampInstance->mConfig->IsConfigSet(eAAMPConfig_EnablePROutputProtection),
			aampInstance->mConfig->IsConfigSet(eAAMPConfig_PropagateURIParam),
			aampInstance->mIsFakeTune,
			aampInstance->mConfig->IsConfigSet(eAAMPConfig_WideVineKIDWorkaround));
			
}
/**
 *  @brief AampDRMLicenseManager constructor.
 */
AampDRMLicenseManager::AampDRMLicenseManager(int maxDrmSessions, PrivateInstanceAAMP *aamp,
							 DrmSessionCreator creator) : mMaxDRMSessions(maxDrmSessions),
		aampInstance(aamp), mDrmSessionManager(NULL), accessToken()
{
    aampInstance = aamp; 
	AAMPLOG_MIL("[LicMgr] AampDRMLicenseManager constructing, maxDrmSessions:%d aamp:%p UseFireboltSDK:%d", maxDrmSessions, static_cast<void*>(aamp), aampInstance->mConfig->IsConfigSet(eAAMPConfig_UseFireboltSDK));
	std::function<void(uint32_t,uint32_t,const std::string&)> waterMarkSessionUpdateCB = std::bind(&PrivateInstanceAAMP::SendWatermarkSessionUpdateEvent, aampInstance, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
    mDrmSessionManager = new DrmSessionManager(maxDrmSessions, aampInstance, std::move(waterMarkSessionUpdateCB), std::move(creator));
    registerCb(this, mDrmSessionManager);
    getConfigs(mDrmSessionManager, aampInstance);
    mLicenseDownloader = new AampCurlDownloader[mMaxDRMSessions];
    mLicensePrefetcher = new AampLicensePreFetcher(aampInstance);
    ContentSecurityManager::UseFireboltSDK(aampInstance->mConfig->IsConfigSet(eAAMPConfig_UseFireboltSDK));
    mLicensePrefetcher->Init();
	mWatermarkWorker = std::thread(&AampDRMLicenseManager::WatermarkWorkerThread, this);
	AAMPLOG_MIL("[LicMgr] AampDRMLicenseManager constructed, sessionMgr:%p prefetcher:%p", static_cast<void*>(mDrmSessionManager), static_cast<void*>(mLicensePrefetcher));
}

/**
 *  @brief AampDRMLicenseManager destructor
 */
AampDRMLicenseManager::~AampDRMLicenseManager()
{
	AAMPLOG_MIL("[LicMgr] AampDRMLicenseManager destructing, licMgr:%p", static_cast<void*>(this));
	StopWatermarkWorker();
	SAFE_DELETE(mLicensePrefetcher);
	AAMPLOG_INFO("[LicMgr] License prefetcher deleted");
	SAFE_DELETE(mDrmSessionManager);
	AAMPLOG_INFO("[LicMgr] DRM session manager deleted");
	releaseLicenseRenewalThreads();
	for(int i = 0 ; i < mMaxDRMSessions;i++)  
	{
		AAMPLOG_INFO("[LicMgr] Releasing license downloader for slot:%d", i);
		mLicenseDownloader[i].Release();
		mLicenseDownloader[i].CleanupCurlHeaderResources();
	}
	SAFE_DELETE_ARRAY( mLicenseDownloader );
	AAMPLOG_MIL("[LicMgr] AampDRMLicenseManager destructed");
}

/**
 *  @brief Clean up the license renewal threads created.
 */
void AampDRMLicenseManager::releaseLicenseRenewalThreads()
{
        AAMPLOG_INFO("[LicMgr] Releasing license renewal threads, count:%zu", mLicenseRenewalThreads.size());
        for(int i = 0 ; i < mLicenseRenewalThreads.size(); i++)
        {
                if(mLicenseRenewalThreads[i].joinable())
                {
                        AAMPLOG_MIL("[LicMgr] Joining license renewal thread for slot:%d", i);
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
	MW_LOG_INFO("isAbort : %s", isAbort ? "true" : "false");
	AAMPLOG_MIL("[LicMgr] setLicenseRequestAbort:%d", isAbort);
	licenseRequestAbort.store(isAbort, std::memory_order_release);
}

void AampDRMLicenseManager::licenseRenewalThread(std::shared_ptr<DrmHelper> drmHelper, int sessionSlot, PrivateInstanceAAMP* aampInstance)
{
	bool isSecClientError = false;
	//isSecClientError = true; //for secmanager
	DrmMetaDataEventPtr e = std::make_shared<DrmMetaDataEvent>(AAMP_TUNE_FAILURE_UNKNOWN, "", 0, 0, isSecClientError, aampInstance->GetSessionId());
	int cdmError = -1;
	int responseCode = -1;
	AAMPLOG_MIL("[LicMgr] licenseRenewalThread started for sessionSlot:%d", sessionSlot);
	KeyState code = acquireLicense(responseCode, std::move(drmHelper), sessionSlot, cdmError,  eMEDIATYPE_LICENCE,(void*)e.get() ,true);
	AAMPLOG_MIL("[LicMgr] licenseRenewalThread acquireLicense returned keyState:%d responseCode:%d cdmError:%d sessionSlot:%d", code, responseCode, cdmError, sessionSlot);
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
	AAMPLOG_MIL("[LicMgr] renewLicense requested, session:%p sessionSlot:%d", static_cast<void*>(session), sessionSlot);
	if (sessionSlot >= 0)
	{
		if(mLicenseRenewalThreads.size() == 0)
		{
			AAMPLOG_INFO("[LicMgr] Resizing mLicenseRenewalThreads to %d", mMaxDRMSessions);
			mLicenseRenewalThreads.resize(mMaxDRMSessions);
		}
		if(mLicenseRenewalThreads[sessionSlot].joinable())
		{
			AAMPLOG_INFO("[LicMgr] Previous renewal thread for slot:%d still active, joining", sessionSlot);
			mLicenseRenewalThreads[sessionSlot].join();
		}
		try
		{
			mLicenseRenewalThreads[sessionSlot] = std::thread([this, drmHelper, sessionSlot, aampInstance] 
			{
				this->licenseRenewalThread(std::move(drmHelper), sessionSlot, aampInstance);
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
KeyState AampDRMLicenseManager::acquireLicense( int& responseCode, const std::shared_ptr<DrmHelper>& drmHelper, int sessionSlot, int &cdmError,
	 AampMediaType streamType, void *metaDataPtr,  bool isLicenseRenewal)
{
	DrmMetaDataEventPtr* eventHandlePtr = static_cast<DrmMetaDataEventPtr*>(metaDataPtr);
	DrmMetaDataEventPtr& eventHandle = *eventHandlePtr;

	shared_ptr<DrmData> licenseResponse;
	int32_t httpResponseCode = -1;
	int32_t httpExtendedStatusCode = -1;
	KeyState code = KEY_ERROR;
	AAMPLOG_MIL("[LicMgr] acquireLicense begin: sessionSlot:%d streamType:%d isLicenseRenewal:%d uuid:%s isExternalLicense:%d", sessionSlot, streamType, isLicenseRenewal, drmHelper->getUuid().c_str(), drmHelper->isExternalLicense());
#ifdef USE_PREINIT_DECODING
	AAMPLOG_INFO("[LicMgr] USE_PREINIT_DECODING enabled, manifestUrl is fake tune url:%d", aampInstance->mManifestUrl == FAKE_TUNE_URL);
	if(aampInstance->mManifestUrl == FAKE_TUNE_URL)
	{
		AAMPLOG_MIL("[LicMgr] Fake tune, skipping license acquisition and returning KEY_ERROR");
		return code;
	}
#else
	AAMPLOG_INFO("[LicMgr] USE_PREINIT_DECODING not enabled, fake tune check skipped in acquireLicense");
#endif
	if (drmHelper->isExternalLicense() && !isLicenseRenewal)
	{
		// External license, assuming the DRM system is ready to proceed
		AAMPLOG_MIL("[LicMgr] External license, DRM system assumed ready, friendlyName:%s", drmHelper->friendlyName().c_str());
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
		AAMPLOG_INFO("[LicMgr] Waiting for sessionMutex of slot:%d", sessionSlot);
                std::lock_guard<std::mutex> guard(mDrmSessionManager->drmSessionContexts[sessionSlot].sessionMutex);
		/**
		 * Generate a License challenge from the CDM
		 */
		AAMPLOG_INFO("Request to generate license challenge to the aampDRMSession(CDM)");

		ChallengeInfo challengeInfo;
		challengeInfo.data.reset(mDrmSessionManager->drmSessionContexts[sessionSlot].drmSession->generateKeyRequest(challengeInfo.url, drmHelper->licenseGenerateTimeout()));
		code = mDrmSessionManager->drmSessionContexts[sessionSlot].drmSession->getState();
		AAMPLOG_MIL("[LicMgr] generateKeyRequest done, keyState:%d challengeDataAvailable:%d challengeUrl:%s", code, (challengeInfo.data != nullptr), challengeInfo.url.c_str());

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
			bool anonymousLicenceReq = aampInstance->mConfig->IsConfigSet(eAAMPConfig_AnonymousLicenseRequest);
			AAMPLOG_MIL("[LicMgr] Challenge generated: usingAppDefinedAuthToken:%d anonymousLicenceReq:%d drmMetaDataAvailable:%d", usingAppDefinedAuthToken, anonymousLicenceReq, !drmHelper->getDrmMetaData().empty());
			if(!isLicenseRenewal)
			{
				aampInstance->profiler.ProfileEnd(PROFILE_BUCKET_LA_PREPROC);
			}

			if (!(drmHelper->getDrmMetaData().empty() || anonymousLicenceReq))
			{
				std::lock_guard<std::mutex> guard(accessTokenMutex);

				int tokenError = 0;
				std::string sessionToken;
				if(!usingAppDefinedAuthToken)
				{ /* authToken not set externally by app */
					sessionToken = getAccessToken(tokenError);
					AAMPLOG_MIL("Access Token from AuthServer");
				}
				else
				{
					sessionToken = aampInstance->mSessionToken;
					AAMPLOG_MIL("Got Access Token from External App");
				}
				if( sessionToken.empty() )
				{
					// Failed to get access token
					// licenseAnonymousRequest is not set, Report failure
					AAMPLOG_WARN("failed to get access token, Anonymous request not enabled");
					eventHandle->setFailure(AAMP_TUNE_FAILED_TO_GET_ACCESS_TOKEN);
				 	eventHandle->setResponseCode(tokenError);
					if(!licenseRequestAbort.load(std::memory_order_acquire))
					{
						// report error
						return KEY_ERROR;
					}
				}
				else
				{
					AAMPLOG_INFO("access token is available");
					challengeInfo.accessToken = std::move(sessionToken);
				}
			}
			else
			{
				AAMPLOG_INFO("[LicMgr] Access token not required (no drm metadata or anonymous license request)");
			}
			if(licenseRequestAbort.load(std::memory_order_acquire))
			{
				AAMPLOG_ERR("Error!! License request was aborted. Resetting session slot %d", sessionSlot);
				eventHandle->setFailure(AAMP_TUNE_DRM_SELF_ABORT);
				eventHandle->setResponseCode(CURLE_ABORTED_BY_CALLBACK);
				responseCode = int(CURLE_ABORTED_BY_CALLBACK);
				return KEY_ERROR;
			}

			LicenseRequest licenseRequest;
			DRMSystems drmType = GetDrmSystem(drmHelper->getUuid());
			licenseRequest.url = aampInstance->GetLicenseServerUrlForDrm(drmType);
			licenseRequest.licenseAnonymousRequest = anonymousLicenceReq;
			AAMPLOG_MIL("[LicMgr] Generating license request: drmType:%d serverUrl:%s", (int)drmType, licenseRequest.url.c_str());
			drmHelper->generateLicenseRequest(challengeInfo, licenseRequest);
			AAMPLOG_MIL("[LicMgr] License request generated: method:%s url:%s anonymous:%d", (licenseRequest.method == LicenseRequest::POST) ? "POST" : "GET", licenseRequest.url.c_str(), licenseRequest.licenseAnonymousRequest);
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
				AAMPLOG_MIL("[LicMgr] License server params configured: isContentMetadataAvailable:%d (SecClient/SecManager path:%d, direct curl path:%d) proxy:%s finalUrl:%s", isContentMetadataAvailable, isContentMetadataAvailable, !isContentMetadataAvailable, licenseServerProxy.c_str(), licenseRequest.url.c_str());
				if (isContentMetadataAvailable)
				{
					eventHandle->setSecclientError(true);
					licenseResponse.reset(getLicenseSec(licenseRequest, drmHelper, challengeInfo, aampInstance, &httpResponseCode, &httpExtendedStatusCode, eventHandle));
					AAMPLOG_MIL("[LicMgr] getLicenseSec returned httpResponseCode:%d httpExtendedStatusCode:%d responseAvailable:%d", httpResponseCode, httpExtendedStatusCode, (licenseResponse != nullptr));
					bool sec_accessTokenExpired = (aampInstance->mConfig->IsConfigSet(eAAMPConfig_UseSecManager) || aampInstance->mConfig->IsConfigSet(eAAMPConfig_UseFireboltSDK)) && CONTENT_SECURITY_MANAGER_DRM_FAILURE == httpResponseCode && CONTENT_SECURITY_MANAGER_ACCTOKEN_EXPIRED == httpExtendedStatusCode;
					// Reload Expired access token only on http error code 412 with status code 401
					if (((412 == httpResponseCode && 401 == httpExtendedStatusCode) || sec_accessTokenExpired) && !usingAppDefinedAuthToken)
					{
						AAMPLOG_INFO("License Req failure by Expired access token httpResCode %d statusCode %d", httpResponseCode, httpExtendedStatusCode);
						AAMPLOG_MIL("[LicMgr] Access token expired (secManagerExpired:%d), clearing cached token and refetching", sec_accessTokenExpired);
						accessToken.clear();
						int tokenError = 0;
						std::string sessionToken = getAccessToken(tokenError);
						if( !sessionToken.empty() )
						{
							AAMPLOG_INFO("Requesting License with new access token");
							challengeInfo.accessToken = std::move(sessionToken);
							httpResponseCode = httpExtendedStatusCode = -1;
							licenseResponse.reset(getLicenseSec(licenseRequest, drmHelper, challengeInfo, aampInstance, &httpResponseCode, &httpExtendedStatusCode, eventHandle));
							AAMPLOG_MIL("[LicMgr] Retry getLicenseSec returned httpResponseCode:%d httpExtendedStatusCode:%d responseAvailable:%d", httpResponseCode, httpExtendedStatusCode, (licenseResponse != nullptr));
						}
						else
						{
							AAMPLOG_WARN("[LicMgr] Unable to refetch access token, tokenError:%d", tokenError);
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
			              AAMPLOG_MIL("[LicMgr] No content metadata, using direct getLicense (curl) for slot:%d", sessionSlot);
			              licenseResponse.reset(getLicense(licenseRequest, &httpResponseCode, streamType, aampInstance, eventHandle, &mLicenseDownloader[sessionSlot],std::move(licenseServerProxy)));
			              AAMPLOG_MIL("[LicMgr] getLicense returned httpResponseCode:%d responseAvailable:%d", httpResponseCode, (licenseResponse != nullptr));
				}

				//Check if license req is aborted. If yes, ignore the response.
				if(licenseRequestAbort.load(std::memory_order_acquire))
				{
					AAMPLOG_ERR("Error!! License request was aborted, so ignoring the license response. Resetting session slot %d", sessionSlot);
					eventHandle->setFailure(AAMP_TUNE_DRM_SELF_ABORT);
					eventHandle->setResponseCode(CURLE_ABORTED_BY_CALLBACK);
					responseCode = int(CURLE_ABORTED_BY_CALLBACK);
					return KEY_ERROR;
				}
			}
		}
	}

	if (code == KEY_PENDING)
	{
		AAMPLOG_MIL("[LicMgr] acquireLicense key state pending, handling license response");
		code = handleLicenseResponse(responseCode, std::move(drmHelper), sessionSlot, cdmError, httpResponseCode, httpExtendedStatusCode, std::move(licenseResponse), eventHandle,  isLicenseRenewal);
	}
	AAMPLOG_MIL("[LicMgr] acquireLicense end: sessionSlot:%d keyState:%d responseCode:%d cdmError:%d failure:%d", sessionSlot, code, responseCode, cdmError, (int)eventHandle->getFailure());
	return code;
}
KeyState AampDRMLicenseManager::handleLicenseResponse(int &responseCode,std::shared_ptr<DrmHelper> drmHelper, int sessionSlot, int &cdmError, int32_t httpResponseCode, int32_t httpExtendedStatusCode, shared_ptr<DrmData> licenseResponse, DrmMetaDataEventPtr eventHandle,  bool isLicenseRenewal)
{
	AAMPLOG_MIL("[LicMgr] handleLicenseResponse: sessionSlot:%d httpResponseCode:%d httpExtendedStatusCode:%d responseLen:%zu isLicenseRenewal:%d isExternalLicense:%d", sessionSlot, httpResponseCode, httpExtendedStatusCode, (licenseResponse ? (size_t)licenseResponse->getDataLength() : (size_t)0), isLicenseRenewal, drmHelper->isExternalLicense());
	if (!drmHelper->isExternalLicense())
	{
		if ((licenseResponse != NULL) && (licenseResponse->getDataLength() != 0))
		{
			if(!isLicenseRenewal)
			{
				aampInstance->profiler.ProfileEnd(PROFILE_BUCKET_LA_NETWORK);
			}
			if (!isSecFeatureEnabled() && (!drmHelper->getDrmMetaData().empty() || aampInstance->mConfig->IsConfigSet(eAAMPConfig_Base64LicenseWrapping)))
			{
				AAMPLOG_INFO("[LicMgr] License response may be JSON-wrapped, attempting to extract license");
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
							AAMPLOG_MIL("[LicMgr] License extracted from JSON response, license length:%zu", keyData.size());
						}
					}
					catch (AampJsonParseException& e)
					{
						AAMPLOG_WARN("Failed to parse JSON response (%s)", jsonStr.c_str());
					}
				}
			}
			AAMPLOG_INFO("license acquisition completed");
			AAMPLOG_MIL("[LicMgr] Transforming license response via helper, length:%zu", (size_t)licenseResponse->getDataLength());
			drmHelper->transformLicenseResponse(licenseResponse);
		}
		else
		{
			AAMPLOG_WARN("[LicMgr] Invalid/empty license response: responseAvailable:%d httpResponseCode:%d httpExtendedStatusCode:%d, mapping to failure", (licenseResponse != NULL), httpResponseCode, httpExtendedStatusCode);
			if(!isLicenseRenewal)
			{
				aampInstance->profiler.ProfileError(PROFILE_BUCKET_LA_NETWORK, httpResponseCode);
				aampInstance->profiler.ProfileEnd(PROFILE_BUCKET_LA_NETWORK);
			}
			AAMPLOG_ERR("Error!! Invalid License Response was provided by the Server");
			
			//Handle secmanager specific error codes here
			if( aampInstance->mConfig->IsConfigSet(eAAMPConfig_UseSecManager) || aampInstance->mConfig->IsConfigSet(eAAMPConfig_UseFireboltSDK))
			{
				AAMPLOG_INFO("[LicMgr] Mapping failure using SecManager/Firebolt error codes");
				eventHandle->setResponseCode(httpResponseCode);
				eventHandle->setSecManagerReasonCode(httpExtendedStatusCode);
				if(CONTENT_SECURITY_MANAGER_DRM_FAILURE == httpResponseCode && CONTENT_SECURITY_MANAGER_ENTITLEMENT_FAILURE == httpExtendedStatusCode)
				{
					if (eventHandle->getFailure() != AAMP_TUNE_FAILED_TO_GET_ACCESS_TOKEN)
					{
						eventHandle->setFailure(AAMP_TUNE_AUTHORIZATION_FAILURE);
					}
					AAMPLOG_WARN("DRM session for %s, Authorization failed", mDrmSessionManager->drmSessionContexts[sessionSlot].drmSession->getKeySystem().c_str());
				}
				else if(CONTENT_SECURITY_MANAGER_DRM_FAILURE == httpResponseCode && CONTENT_SECURITY_MANAGER_SERVICE_TIMEOUT == httpExtendedStatusCode)
				{
					AAMPLOG_WARN("[LicMgr] SecManager service timeout, failure set to AAMP_TUNE_LICENCE_TIMEOUT");
					eventHandle->setFailure(AAMP_TUNE_LICENCE_TIMEOUT);
				}
				else
				{
					AAMPLOG_WARN("[LicMgr] SecManager license request failed, failure set to AAMP_TUNE_LICENCE_REQUEST_FAILED");
					eventHandle->setFailure(AAMP_TUNE_LICENCE_REQUEST_FAILED);
				}
			}
			else if (412 == httpResponseCode)
			{
				AAMPLOG_WARN("[LicMgr] HTTP 412 from license server");
				if (eventHandle->getFailure() != AAMP_TUNE_FAILED_TO_GET_ACCESS_TOKEN)
				{
					eventHandle->setFailure(AAMP_TUNE_AUTHORIZATION_FAILURE);
				}
				AAMPLOG_WARN("DRM session for %s, Authorization failed",mDrmSessionManager->drmSessionContexts[sessionSlot].drmSession->getKeySystem().c_str());

			}
			else if (IsCurlTimeoutFailure ( httpResponseCode ))
			{
				AAMPLOG_WARN("[LicMgr] Curl timeout failure httpResponseCode:%d, failure set to AAMP_TUNE_LICENCE_TIMEOUT", httpResponseCode);
				eventHandle->setFailure(AAMP_TUNE_LICENCE_TIMEOUT);
			}
			else if(CURLE_ABORTED_BY_CALLBACK == httpResponseCode || CURLE_WRITE_ERROR == httpResponseCode)
			{
				AAMPLOG_WARN("[LicMgr] License request aborted (httpResponseCode:%d), treated as self abort", httpResponseCode);
				/** Set failure reason as AAMP_TUNE_DRM_SELF_ABORT to avoid unnecessary error reporting. */
				eventHandle->setFailure(AAMP_TUNE_DRM_SELF_ABORT);
				eventHandle->setResponseCode(httpResponseCode);
				responseCode = httpResponseCode;
			}
			else
			{
				AAMPLOG_WARN("[LicMgr] Generic license request failure httpResponseCode:%d", httpResponseCode);
				eventHandle->setFailure(AAMP_TUNE_LICENCE_REQUEST_FAILED);
				eventHandle->setResponseCode(httpResponseCode);
			}
			AAMPLOG_MIL("[LicMgr] handleLicenseResponse returning KEY_ERROR, failure:%d", (int)eventHandle->getFailure());
			return KEY_ERROR;
		}
	}
	else
	{
		AAMPLOG_INFO("[LicMgr] External license, no license response processing, passing to CDM");
	}
	return processLicenseResponse(std::move(drmHelper), sessionSlot, cdmError, std::move(licenseResponse), std::move(eventHandle), isLicenseRenewal);
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
	AAMPLOG_MIL("[LicMgr] processLicenseResponse: sessionSlot:%d responseAvailable:%d isLicenseRenewal:%d keyProcessTimeout:%d", sessionSlot, (licenseResponse != nullptr), isLicenseRenewal, drmHelper->keyProcessTimeout());
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
	AAMPLOG_MIL("[LicMgr] processDRMKey done: sessionSlot:%d cdmError:%d keyState:%d", sessionSlot, cdmError, code);

	if (code == KEY_ERROR)
	{
		if (AAMP_TUNE_FAILURE_UNKNOWN == eventHandle->getFailure())
		{
			// check if key failure is due to HDCP , if so report it appropriately instead of Failed to get keys
			if (cdmError == HDCP_OUTPUT_PROTECTION_FAILURE || cdmError == HDCP_COMPLIANCE_CHECK_FAILURE)
			{
				AAMPLOG_WARN("[LicMgr] Key error due to HDCP, cdmError:%d", cdmError);
				eventHandle->setFailure(AAMP_TUNE_HDCP_COMPLIANCE_ERROR);
			}
			else
			{
				AAMPLOG_WARN("[LicMgr] Key update failed, cdmError:%d", cdmError);
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
	else
	{
		AAMPLOG_INFO("[LicMgr] processLicenseResponse finished with keyState:%d", code);
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
	AAMPLOG_INFO("[LicMgr] UpdateLicenseMetrics requestType:%d statusCode:%d downloadTimeMS:%lld url:%s", (int)requestType, statusCode, downloadTimeMS, licenseRequestUrl.c_str());
	if( nullptr == eventHandle)
	{
		AAMPLOG_ERR("event Handle - NULL");
		return;
	}
	item = cJSON_CreateObject();
	if( nullptr != item)
	{
		cJSON_AddNumberToObject(item, "req", requestType);   // DRM license request type
		cJSON_AddNumberToObject(item, "res", statusCode);    // HTTP response code from the license server (HTTP/curl/SecClient response code)
		cJSON_AddNumberToObject(item, "tot", downloadTimeMS); // Total wall-clock time for DRM license acquisition (including retries)
		cJSON_AddStringToObject(item, "url", licenseRequestUrl.c_str()); // License server URL

		if( (nullptr != respData) && (DRM_GET_LICENSE == requestType))
		{
			cJSON_AddNumberToObject(item, "con", respData->downloadCompleteMetrics.connect);       // TCP connection establishment time
			cJSON_AddNumberToObject(item, "str", respData->downloadCompleteMetrics.startTransfer); // Time from request start to first byte received
			cJSON_AddNumberToObject(item, "dns", respData->downloadCompleteMetrics.resolve);       // DNS resolution time
			cJSON_AddNumberToObject(item, "acn", respData->downloadCompleteMetrics.appConnect);    // TLS/SSL handshake completion time
			cJSON_AddNumberToObject(item, "ptr", respData->downloadCompleteMetrics.preTransfer);   // Pre-transfer overhead time
			cJSON_AddNumberToObject(item, "rdt", respData->downloadCompleteMetrics.redirect);      // Time spent following HTTP redirects
			cJSON_AddNumberToObject(item, "dls", respData->downloadCompleteMetrics.dlSize);        // Number of bytes downloaded in the response body
			cJSON_AddNumberToObject(item, "rqs", respData->downloadCompleteMetrics.reqSize);       // Number of bytes sent in the license request
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
 *
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
const std::string &AampDRMLicenseManager::getAccessToken(int &error_code)
{
	if (accessToken.empty())
	{
		AAMPLOG_MIL("[LicMgr] getAccessToken: no cached token, requesting from ContentSecurityManager");
		if (ContentSecurityManager::GetInstance()->getSessionToken(accessToken))
		{
			if(accessToken.length() > 0)
			{
				AAMPLOG_MIL(" Received session token from auth service");
			}
			else
			{
				AAMPLOG_WARN("Invalid access token from ContentSecurityManager");
				error_code = eAUTHTOKEN_TOKEN_PARSE_ERROR;
			}
		}
		else
		{
			AAMPLOG_ERR("ContentSecurityManager failed to get access token");
			error_code = eAUTHTOKEN_TOKEN_PARSE_ERROR;
		}
	}
	else
	{
		AAMPLOG_INFO("[LicMgr] getAccessToken: using cached access token");
	}
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
	AAMPLOG_INFO("[LicMgr] configureLicenseServerParameters: isContentMetadataAvailable:%d initialUrl:%s isSecFeatureEnabled:%d", isContentMetadataAvailable, licenseRequest.url.c_str(), isSecFeatureEnabled());

	if (!contentMetaData.empty())
	{
		if (!licenseRequest.url.empty())
		{
			if( isSecFeatureEnabled() )
			{
				licenseRequest.url = getFormattedLicenseServerURL(licenseRequest.url);
				AAMPLOG_INFO("[LicMgr] License server URL formatted for SecClient:%s", licenseRequest.url.c_str());
			}			
		}
		else
		{
			AAMPLOG_WARN("[LicMgr] Content metadata available but license server URL is empty");
		}
	}

	// 1. Add any custom License Headers for with and without ContentMetadata license request
	// 2. In addition for ContentMetadata license, add additional headers if present
	{
		std::unordered_map<std::string, std::vector<std::string>> customHeaders;
		aampInstance->GetCustomLicenseHeaders(customHeaders);
		AAMPLOG_INFO("[LicMgr] Custom license headers count:%zu", customHeaders.size());
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
				AAMPLOG_INFO("[LicMgr] No custom headers, clearing helper-provided license headers for content metadata request");
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

	AAMPLOG_INFO("[LicMgr] configureLicenseServerParameters done: headersCount:%zu proxy:%s", licenseRequest.headers.size(), licenseServerProxy.c_str());
	return isContentMetadataAvailable;
}
void AampDRMLicenseManager::ContentProtectionDataUpdate(PrivateInstanceAAMP* aampInstance, const std::vector<uint8_t>& keyId, AampMediaType streamType)
{
	std::string contentType = GetMediaTypeName(streamType);
	int iter1 = 0;
	bool DRM_Config_Available = false;
	AAMPLOG_MIL("[LicMgr] ContentProtectionDataUpdate: keyId:%s streamType:%s cachedEntries:%zu", AampLogManager::getHexDebugStr(keyId).c_str(), contentType.c_str(), aampInstance->vDynamicDrmData.size());
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
			AAMPLOG_INFO("[LicMgr] Cached DRM config applied from slot:%d", iter1);
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
			AAMPLOG_MIL("[LicMgr] App did not respond within %dms with DRM config, using default config (endpoints:%zu)", drmUpdateTimeout, aampInstance->mDynamicDrmDefaultconfig.licenseEndPoint.size());
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
			AAMPLOG_WARN("cond_timedwait(dynamicDrmUpdate) returned success!" );
			AAMPLOG_MIL("[LicMgr] App supplied DRM config for keyId:%s", keyIdDebugStr.c_str());
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
	AAMPLOG_MIL("[LicMgr] SetCommonKeyDuration:%d", keyDuration);
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
	AAMPLOG_MIL("[LicMgr] Stop called, terminating license prefetcher");
	mLicensePrefetcher->Term();
}
/**
 * @brief set license fetcher object
 * 
 * @return none
 */
void AampDRMLicenseManager::SetLicenseFetcher(AampLicenseFetcher *fetcherInstance)
{
	AAMPLOG_MIL("[LicMgr] SetLicenseFetcher:%p", static_cast<void*>(fetcherInstance));
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
	AAMPLOG_MIL("[LicMgr] SetSendErrorOnFailure:%d", sendErrorOnFailure);
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
	AAMPLOG_MIL("[LicMgr] QueueContentProtection type:%d period:%s adaptationIdx:%u isVss:%d", type, periodId.c_str(), adapIdx, isVssPeriod);
	return mLicensePrefetcher->QueueContentProtection(std::move(drmHelper), std::move(periodId), adapIdx, type, isVssPeriod);
}

bool AampDRMLicenseManager::IsKeyIdProcessed(const std::vector<uint8_t>& keyId, bool& keyStatus)
{
	bool processed = mDrmSessionManager && mDrmSessionManager->IsKeyIdProcessed(keyId, keyStatus);
	AAMPLOG_INFO("[LicMgr] IsKeyIdProcessed keyId:%s processed:%d keyStatus:%d", AampLogManager::getHexDebugStr(keyId).c_str(), processed, keyStatus);
	return processed;
}

/**
 *  @brief Get DRM license key from DRM server.
 */
DrmData* AampDRMLicenseManager::getLicense(LicenseRequest &licenseRequest,
		int32_t *httpCode, AampMediaType streamType, void* aampI, DrmMetaDataEventPtr eventHandle, AampCurlDownloader *pLicenseDownloader, std::string licenseProxy)
{

	double totalTime = 0;
	DrmData * keyInfo = NULL;
	bool bNeedResponseHeadersTobeShared = aampInstance->mConfig->IsConfigSet(eAAMPConfig_SendLicenseResponseHeaders);
	AAMPLOG_MIL("[LicMgr] getLicense begin: url:%s method:%s streamType:%d proxy:%s shareResponseHeaders:%d", licenseRequest.url.c_str(), (licenseRequest.method == LicenseRequest::POST) ? "POST" : "GET", streamType, licenseProxy.c_str(), bNeedResponseHeadersTobeShared);
	DownloadResponsePtr respData = std::make_shared<DownloadResponse> ();		
	// Initialize the Seesion Token Connector
	DownloadConfigPtr inpData = std::make_shared<DownloadConfig> ();
	inpData->bIgnoreResponseHeader = !bNeedResponseHeadersTobeShared;
	inpData->iDownloadTimeout = aampInstance->mConfig->GetConfigValue(eAAMPConfig_DrmNetworkTimeout);
	inpData->iStallTimeout = aampInstance->mConfig->GetConfigValue(eAAMPConfig_DrmStallTimeout);
	inpData->iStartTimeout = aampInstance->mConfig->GetConfigValue(eAAMPConfig_DrmStartTimeout);
	inpData->iCurlConnectionTimeout = aampInstance->mConfig->GetConfigValue(eAAMPConfig_Curl_ConnectTimeout);
	inpData->bNeedDownloadMetrics = true;
	inpData->proxyName = std::move(licenseProxy);
	inpData->pCurl = CurlStore::GetCurlStoreInstance(aampInstance).GetCurlHandle(aampInstance, licenseRequest.url, eCURLINSTANCE_AES);
	inpData->sCustomHeaders = licenseRequest.headers;
	AAMPLOG_TRACE("DRMSession-getLicense download params - StallTimeout : %d StartTimeout : %d DownloadTimeout : %d CurlConnectionTimeout : %d ", inpData->iStallTimeout, inpData->iStartTimeout, inpData->iDownloadTimeout, inpData->iCurlConnectionTimeout);
	if (aampInstance->mConfig->IsConfigSet(eAAMPConfig_CurlLicenseLogging))
	{
		inpData->bVerbose = true;
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
		AAMPLOG_INFO("[LicMgr] getLicense using POST, payloadLen:%zu base64Wrapping:%d", licenseRequest.payload.length(), aampInstance->mConfig->IsConfigSet(eAAMPConfig_Base64LicenseWrapping));
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
		AAMPLOG_INFO("[LicMgr] getLicense using GET");
	}

	// Initialize the downloader for above config settings 	
	pLicenseDownloader->Initialize(inpData);
	
	AAMPLOG_WARN(" Sending license request to server : %s ", licenseRequest.url.c_str());

	unsigned int attemptCount = 0;
	long long tStartTimeWithRetry = NOW_STEADY_TS_MS;
	/* Check whether stopped or not before looping - download will be disabled */
	while(attemptCount < MAX_LICENSE_REQUEST_ATTEMPTS && !licenseRequestAbort.load(std::memory_order_acquire))
	{
		bool loopAgain = false;
		attemptCount++;
		AAMPLOG_MIL("[LicMgr] getLicense attempt %u/%u for url:%s", attemptCount, (unsigned int)MAX_LICENSE_REQUEST_ATTEMPTS, licenseRequest.url.c_str());

		long long tStartTime = NOW_STEADY_TS_MS;
		pLicenseDownloader->Download(licenseRequest.url, respData);
		long long tEndTime = NOW_STEADY_TS_MS;
		long long downloadTimeMS = tEndTime - tStartTime;
		
		/** Restrict further processing license if stop called in between  */
		if(licenseRequestAbort.load(std::memory_order_acquire))
		{
			AAMPLOG_ERR(" Aborting License acquisition");
			// Update httpCode as 42-curl abort, so that DRM error event will not be sent by upper layer
			*httpCode = CURLE_ABORTED_BY_CALLBACK;
			break;
		}
		
		
		*httpCode = respData->iHttpRetValue;
		AAMPLOG_MIL("[LicMgr] getLicense download finished: httpCode:%d downloadTimeMS:%lld attempt:%u", *httpCode, downloadTimeMS, attemptCount);
		if ( IsCurlTimeoutFailure(respData->iHttpRetValue) )
		{ // Retry for curl 28 and curl 7 errors.
			AAMPLOG_WARN("[LicMgr] getLicense curl timeout/connect failure (%d) on attempt %u, will retry", respData->iHttpRetValue, attemptCount);
			loopAgain = true;
			pLicenseDownloader->Clear();
		}
		else
		{
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
		if ( IsCurlTimeoutFailure( respData->iHttpRetValue ) ||
			CURLE_PARTIAL_FILE == respData->iHttpRetValue ||
			CURLE_COULDNT_CONNECT == respData->iHttpRetValue)
		{
			// introduce  extra marker for connection status curl 7/18/28,
			// example 18(0) if connection failure with PARTIAL_FILE code
			timeoutClass = "(" + std::to_string(respData->downloadCompleteMetrics.reqSize > 0) + ")";
		}
		AAMPLOG_WARN("HttpRequestEnd: %s%d,%d,%d%s,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%g,%ld,%d,%d,%.500s",
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
		AAMPLOG_INFO("[LicMgr] getLicense looping again after attempt %u", attemptCount);
	}
	long long tEndTimeWithRetry = NOW_STEADY_TS_MS;
	long long totalDownloadTimeMS = tEndTimeWithRetry - tStartTimeWithRetry;
	AAMPLOG_MIL("[LicMgr] getLicense finished: httpCode:%d attempts:%u totalDownloadTimeMS:%lld keyInfoAvailable:%d abort:%d", *httpCode, attemptCount, totalDownloadTimeMS, (keyInfo != NULL), licenseRequestAbort.load(std::memory_order_acquire));

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
	AAMPLOG_INFO("[LicMgr] getLicense returning curl handle to CurlStore");
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
	AAMPLOG_MIL("[LicMgr] getLicenseSec begin: url:%s keySystem:%s isVSS:%d contentMetaDataLen:%zu challengeLen:%zu sessionTokenAvailable:%d retryWaitMs:%d", licenseRequest.url.c_str(), keySystem, aampInstance->mIsVSS, contentMetaData.length(), (size_t)challengeInfo.data->getDataLength(), (secclientSessionToken != NULL), sleepTime);
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
	if(aampInstance->mConfig->IsConfigSet(eAAMPConfig_UseSecManager) || aampInstance->mConfig->IsConfigSet(eAAMPConfig_UseFireboltSDK))
	{
		AAMPLOG_MIL("[LicMgr] getLicenseSec using ContentSecurityManager (UseSecManager:%d UseFireboltSDK:%d)", aampInstance->mConfig->IsConfigSet(eAAMPConfig_UseSecManager), aampInstance->mConfig->IsConfigSet(eAAMPConfig_UseFireboltSDK));
		size_t encodedDataLen = ((contentMetaData.length() + 2) /3) * 4;
		size_t encodedChallengeDataLen = ((challengeInfo.data->getDataLength() + 2) /3) * 4;
		int32_t statusCode;
		int32_t reasonCode;
		int32_t businessStatus;
		bool videoMuteState = mDrmSessionManager->mIsVideoOnMute.load();
		if (!mDrmSessionManager->mContentSecurityManagerSession.isSessionValid())
		{
			// if we're about to get a licence and are not re-using a session, then we have not seen the first video frame yet. Do not allow watermarking to get enabled yet.
			AAMPLOG_WARN("First frame flag cleared before AcquireLicense, with mIsVideoOnMute=%d", videoMuteState);
			mDrmSessionManager->mFirstFrameSeen.store(false);
		}

		std::string clientId = "aamp";
		std::string appId = aampInstance->GetAppName();
		if(appId.empty()) 
		{
			appId = clientId;

		}
		AAMPLOG_INFO("Client ID %s App ID %s", clientId.c_str(), appId.c_str());
		AAMPLOG_MIL("[LicMgr] Calling ContentSecurityManager::AcquireLicense, videoMuteState:%d sessionValid:%d", videoMuteState, mDrmSessionManager->mContentSecurityManagerSession.isSessionValid());
		tStartTime = NOW_STEADY_TS_MS;
		bool res = ContentSecurityManager::GetInstance()->AcquireLicense(std::move(clientId), std::move(appId), licenseRequest.url.c_str(),
																 requestMetadata,
																 ((numberOfAccessAttributes == 0) ? NULL : accessAttributes),
																 encodedData, encodedDataLen,
																 encodedChallengeData, encodedChallengeDataLen,
																 keySystem,
																 mediaUsage,
																 secclientSessionToken, challengeInfo.accessToken.length(),
																 mDrmSessionManager->mContentSecurityManagerSession,
																 &licenseResponseStr, &licenseResponseLength,
																 &statusCode, &reasonCode, &businessStatus, videoMuteState, sleepTime);
		tEndTime = NOW_STEADY_TS_MS;
		downloadTimeMS = tEndTime - tStartTime;
		AAMPLOG_MIL("[LicMgr] ContentSecurityManager::AcquireLicense returned result:%d statusCode:%d reasonCode:%d businessStatus:%d downloadTimeMS:%lld", res, statusCode, reasonCode, businessStatus, downloadTimeMS);
		if (res)
		{
			AAMPLOG_WARN("acquireLicense via ContentSecurityManager SUCCESS!");
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
			AAMPLOG_ERR("acquireLicense via ContentSecurityManager FAILED!, httpCode %d, httpExtStatusCode %d", *httpCode, *httpExtStatusCode);
			//TODO: Sort this out for backward compatibility
		}
	}
	else
	{
		int32_t sec_client_result = 0;
		PlayerSecExtendedStatus statusInfo;
		unsigned int attemptCount = 0;
		AAMPLOG_MIL("[LicMgr] getLicenseSec using PlayerSec_AcquireLicense (SecClient) path, maxAttempts:%u", (unsigned int)MAX_LICENSE_REQUEST_ATTEMPTS);
		tStartTime = NOW_STEADY_TS_MS;
		while (attemptCount < MAX_LICENSE_REQUEST_ATTEMPTS)
		{
			attemptCount++;
			AAMPLOG_MIL("[LicMgr] PlayerSec_AcquireLicense attempt %u", attemptCount);
			sec_client_result = mDrmSessionManager->playerSecInstance->PlayerSec_AcquireLicense(licenseRequest.url.c_str(), 1,
														 requestMetadata, numberOfAccessAttributes,
														 ((numberOfAccessAttributes == 0) ? NULL : accessAttributes),
														 encodedData,
														 (encodedData ? strlen(encodedData) : 0),
														 encodedChallengeData, 
														 (encodedChallengeData ? strlen(encodedChallengeData) : 0), keySystem, mediaUsage,
														 secclientSessionToken,
														 &licenseResponseStr, &licenseResponseLength, &refreshDuration, &statusInfo);
			if (((sec_client_result >= 500 && sec_client_result < 600)||
				  ( mDrmSessionManager->playerSecInstance->isSecResultInRange(sec_client_result))) && attemptCount < MAX_LICENSE_REQUEST_ATTEMPTS)
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
		AAMPLOG_MIL("[LicMgr] PlayerSec_AcquireLicense finished: sec_client_result:%d extStatus:%d accessAttributeStatus:%d attempts:%u downloadTimeMS:%lld", sec_client_result, statusInfo.statusCode, statusInfo.accessAttributeStatus, attemptCount, downloadTimeMS);

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
				
			if(eventHandle)
			{
				
			    eventHandle->setAccessStatusValue(statusInfo.accessAttributeStatus);
			}
			licenseResponse = new DrmData(licenseResponseStr, licenseResponseLength);
				
		}
		if (licenseResponseStr) mDrmSessionManager->playerSecInstance->PlayerSec_FreeResource(licenseResponseStr);
	}
	UpdateLicenseMetrics(DRM_GET_LICENSE_SEC, *httpCode, licenseRequest.url.c_str(), downloadTimeMS, std::move(eventHandle), nullptr);

	free(encodedData);
	free(encodedChallengeData);
	AAMPLOG_MIL("[LicMgr] getLicenseSec end: httpCode:%d httpExtStatusCode:%d licenseResponseAvailable:%d downloadTimeMS:%lld", *httpCode, *httpExtStatusCode, (licenseResponse != nullptr), downloadTimeMS);
	return licenseResponse;
}
/*
 * @brief callback for to do profiling from middleware 
 */
void AampDRMLicenseManager::ProfilerUpdate()
{
  AAMPLOG_INFO("[LicMgr] ProfilerUpdate: LA_PREPROC profile begin");
  aampInstance->profiler.ProfileBegin(PROFILE_BUCKET_LA_PREPROC);
}

ProfilerBucketType AampDRMLicenseManager::GetDecryptProfileBucket(int streamType)
{
    switch (static_cast<AampMediaType>(streamType))
    {
        case eMEDIATYPE_AUDIO:
            return PROFILE_BUCKET_DECRYPT_AUDIO;
        case eMEDIATYPE_VIDEO:
            return PROFILE_BUCKET_DECRYPT_VIDEO;
        default:
            return PROFILE_BUCKET_INVALID;
    }
}
/*
 * @brief callback to do profiling from gst-plugins to application 
 */
void AampDRMLicenseManager::TriggerDecryptProfile(int streamType, int action, int result /* = 0 */)
{
    ProfilerBucketType bucket = GetDecryptProfileBucket(streamType);
    if (bucket == PROFILE_BUCKET_INVALID)
    {
        AAMPLOG_TRACE("[LicMgr] TriggerDecryptProfile ignored for streamType:%d action:%d", streamType, action);
        return;
    }
    AAMPLOG_TRACE("[LicMgr] TriggerDecryptProfile streamType:%d action:%d result:%d", streamType, action, result);

    switch ((ProfilerAction)action)
    {
        case PROFILE_ACTION_BEGIN:
            aampInstance->profiler.ProfileBegin(bucket);
            break;
        case PROFILE_ACTION_END:
            aampInstance->profiler.ProfileEnd(bucket);
            break;
        case PROFILE_ACTION_ERROR:
            aampInstance->profiler.ProfileError(bucket, result);
            break;
    }
}
void AampDRMLicenseManager::TriggerLAProfileBeginCb(int streamType)
{
	AAMPLOG_INFO("[LicMgr] TriggerLAProfileBeginCb streamType:%d licenceFromManifest:%d", streamType, aampInstance->licenceFromManifest);
	if(!aampInstance->licenceFromManifest)
	{
		aampInstance->profiler.ProfileBegin(PROFILE_BUCKET_LA_TOTAL);
	}
}

void AampDRMLicenseManager::TriggerLAProfileEndCb(int streamType)
{
	AAMPLOG_INFO("[LicMgr] TriggerLAProfileEndCb streamType:%d licenceFromManifest:%d", streamType, aampInstance->licenceFromManifest);
	if(!aampInstance->licenceFromManifest)
	{
		aampInstance->profiler.ProfileEnd(PROFILE_BUCKET_LA_TOTAL);
	}
}

void AampDRMLicenseManager::TriggerLAProfileErrorCb(void *ptr)
{
	AAMPLOG_INFO("[LicMgr] TriggerLAProfileErrorCb licenceFromManifest:%d", aampInstance->licenceFromManifest);
	if(!aampInstance->licenceFromManifest)
	{
		DrmMetaDataEventPtr e = *static_cast<DrmMetaDataEventPtr*>(ptr);
		AAMPTuneFailure failure = e->getFailure();
		AAMPLOG_MIL("[LicMgr] LA profile error, failure:%d", (int)failure);
		if (AAMP_TUNE_FAILURE_UNKNOWN != failure)
		{
			if (failure != AAMP_TUNE_DRM_SELF_ABORT)
			{
				AAMPLOG_MIL("[LicMgr] Sending error event for failure:%d", (int)failure);
				aampInstance->SendErrorEvent(failure);
			}
			else
			{
				AAMPLOG_INFO("[LicMgr] Self abort, error event not sent");
			}
			aampInstance->profiler.ProfileError(PROFILE_BUCKET_LA_TOTAL, (int)failure);
			aampInstance->profiler.SetDrmErrorCode((int)failure);
		}
		else
		{
			aampInstance->profiler.ProfileError(PROFILE_BUCKET_LA_TOTAL);
		}
	}
}

void AampDRMLicenseManager::TriggerSetFailure(void *ptr, int err)
{
	DrmMetaDataEventPtr e = *static_cast<DrmMetaDataEventPtr*>(ptr);
	AAMPLOG_MIL("[LicMgr] TriggerSetFailure err:%d", err);
	e->setFailure((AAMPTuneFailure)err);
}

std::shared_ptr<void> AampDRMLicenseManager::TriggerDrmMetaDataEvent()
{
	AAMPLOG_INFO("[LicMgr] TriggerDrmMetaDataEvent creating new DrmMetaDataEvent");
	using DrmMetaDataEventPtr = std::shared_ptr<DrmMetaDataEvent>;
	auto drmEvent = std::make_shared<DrmMetaDataEvent>(AAMP_TUNE_FAILURE_UNKNOWN, "", 0, 0, false, std::string{});
	auto drmEventPtrWrapper = std::make_shared<DrmMetaDataEventPtr>(drmEvent);
	return drmEventPtrWrapper;
}
std::string  AampDRMLicenseManager::HandleContentProtectionData(const std::shared_ptr<DrmHelper>& drmHelper, int streamType, const std::vector<uint8_t>& keyId, int isContentProtectionSupported)
{
	 /* To fetch correct codec type in tune time metrics when drm data is not given in manifest*/
	 aampInstance->setCurrentDrm(drmHelper);

	bool RuntimeDRMConfigSupported = aampInstance->mConfig->IsConfigSet(eAAMPConfig_RuntimeDRMConfig);
	AAMPLOG_MIL("[LicMgr] HandleContentProtectionData: streamType:%d keyId:%s isContentProtectionSupported:%d RuntimeDRMConfigSupported:%d", streamType, AampLogManager::getHexDebugStr(keyId).c_str(), isContentProtectionSupported, RuntimeDRMConfigSupported);
	if(isContentProtectionSupported)
	{
	    if(RuntimeDRMConfigSupported && aampInstance->IsEventListenerAvailable(AAMP_EVENT_CONTENT_PROTECTION_DATA_UPDATE) && (streamType < 4))
	    {
	    	aampInstance->mcurrent_keyIdArray = keyId;
	    	AAMPLOG_INFO("App registered the ContentProtectionDataEvent to send new drm config");
			ContentProtectionDataUpdate(aampInstance, keyId, (AampMediaType)streamType);
			aampInstance->mcurrent_keyIdArray.clear();
	    }
	    else
	    {
	    	AAMPLOG_INFO("[LicMgr] Runtime DRM config update skipped: RuntimeDRMConfigSupported:%d streamType:%d", RuntimeDRMConfigSupported, streamType);
	    }
	}
	std::string customData = aampInstance->GetLicenseCustomData();
	AAMPLOG_INFO("[LicMgr] HandleContentProtectionData returning customData length:%zu", customData.length());
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
			AAMPLOG_MIL("[LicMgr] QueueProtectionEvent: uuid:%s initDataLen:%zu", systemId, data.size());
			AAMPLOG_INFO("Queueing protection event in StreamSink for type:%d period id:%s and adaptation index:%u", type, periodId.c_str(), adapIdx);
			sink->QueueProtectionEvent(systemId, data.data(), data.size(), type);
		}
		else
		{
			AAMPLOG_WARN("[LicMgr] QueueProtectionEvent: no active StreamSink for type:%d period id:%s", type, periodId.c_str());
		}
	}
	else
	{
		AAMPLOG_WARN("[LicMgr] QueueProtectionEvent skipped: drmHelper:%d aampInstance:%d", (drmHelper != nullptr), (aampInstance != nullptr));
	}
}

/**
 *  @brief To Set Playback Speed State
 */
void AampDRMLicenseManager::setPlaybackSpeedState(bool live, double currentLatency, bool livepoint , double liveOffsetMs,int speed, double positionMs, bool firstFrameSeen)
{
	AAMPLOG_INFO("[LicMgr] setPlaybackSpeedState live:%d latency:%f livepoint:%d liveOffsetMs:%f speed:%d positionMs:%f firstFrameSeen:%d", live, currentLatency, livepoint, liveOffsetMs, speed, positionMs, firstFrameSeen);
	if (!EnqueueWatermarkTask([this, live, currentLatency, livepoint, liveOffsetMs, speed, positionMs, firstFrameSeen]() {
		mDrmSessionManager->setPlaybackSpeedState(live, currentLatency, livepoint, liveOffsetMs, speed, positionMs, firstFrameSeen);
	}))
	{
		AAMPLOG_INFO("[LicMgr] Watermark worker stopping, running setPlaybackSpeedState inline");
		mDrmSessionManager->setPlaybackSpeedState(live, currentLatency, livepoint, liveOffsetMs, speed, positionMs, firstFrameSeen);
	}
}

/**
 * @brief De-activate watermark and prevent it from being re-enabled until we get a new first video frame at normal play speed
 */
void AampDRMLicenseManager::hideWatermarkOnDetach(void)
{
	AAMPLOG_MIL("[LicMgr] hideWatermarkOnDetach");
	if (!EnqueueWatermarkTask([this]() {
		mDrmSessionManager->hideWatermarkOnDetach();
	}))
	{
		AAMPLOG_INFO("[LicMgr] Watermark worker stopping, running hideWatermarkOnDetach inline");
		mDrmSessionManager->hideWatermarkOnDetach();
	}
}

/**
 * @brief Deactivate the session while video on mute and then activate it and update the speed once video is unmuted
 */
void AampDRMLicenseManager::setVideoMute(bool live, double currentLatency, bool livepoint , double liveOffsetMs,bool isVideoOnMute, double positionMs)
{
	AAMPLOG_MIL("[LicMgr] setVideoMute live:%d latency:%f livepoint:%d liveOffsetMs:%f isVideoOnMute:%d positionMs:%f", live, currentLatency, livepoint, liveOffsetMs, isVideoOnMute, positionMs);
	if (!EnqueueWatermarkTask([this, live, currentLatency, livepoint, liveOffsetMs, isVideoOnMute, positionMs]() {
		mDrmSessionManager->setVideoMute(live, currentLatency, livepoint, liveOffsetMs, isVideoOnMute, positionMs);
	}))
	{
		AAMPLOG_INFO("[LicMgr] Watermark worker stopping, running setVideoMute inline");
		mDrmSessionManager->setVideoMute(live, currentLatency, livepoint, liveOffsetMs, isVideoOnMute, positionMs);
	}
}

void AampDRMLicenseManager::setVideoWindowSize(int width, int height)
{
	AAMPLOG_MIL("[LicMgr] setVideoWindowSize width:%d height:%d", width, height);
	if (!EnqueueWatermarkTask([this, width, height]() {
		mDrmSessionManager->setVideoWindowSize(width, height);
	}))
	{
		AAMPLOG_INFO("[LicMgr] Watermark worker stopping, running setVideoWindowSize inline");
		mDrmSessionManager->setVideoWindowSize(width, height);
	}
}

/**
 * @brief Queue and process watermarking state updates serially away from caller threads.
 */
bool AampDRMLicenseManager::EnqueueWatermarkTask(std::function<void()> task)
{
	{
		std::lock_guard<std::mutex> lock(mWatermarkTaskMutex);
		if (mWatermarkWorkerStopping)
		{
			AAMPLOG_INFO("[LicMgr] EnqueueWatermarkTask rejected, worker is stopping");
			return false;
		}
		mWatermarkTaskQueue.emplace_back(std::move(task));
		AAMPLOG_TRACE("[LicMgr] Watermark task queued, queue size:%zu", mWatermarkTaskQueue.size());
	}
	mWatermarkTaskCondition.notify_one();
	return true;
}

void AampDRMLicenseManager::WatermarkWorkerThread()
{
	AAMPLOG_MIL("[LicMgr] WatermarkWorkerThread started");
	for (;;)
	{
		std::function<void()> task;
		{
			std::unique_lock<std::mutex> lock(mWatermarkTaskMutex);
			mWatermarkTaskCondition.wait(lock, [this]() {
				return mWatermarkWorkerStopping || !mWatermarkTaskQueue.empty();
			});
			if (mWatermarkTaskQueue.empty())
			{
				if (mWatermarkWorkerStopping)
				{
					AAMPLOG_MIL("[LicMgr] WatermarkWorkerThread exiting");
					return;
				}
				continue;
			}
			task = std::move(mWatermarkTaskQueue.front());
			mWatermarkTaskQueue.pop_front();
		}

		try
		{
			AAMPLOG_TRACE("[LicMgr] Running watermark task");
			task();
		}
		catch (...)
		{
			MW_LOG_ERR("Watermark worker task threw an exception");
		}
	}
}

void AampDRMLicenseManager::StopWatermarkWorker()
{
	AAMPLOG_MIL("[LicMgr] StopWatermarkWorker joinable:%d", mWatermarkWorker.joinable());
	{
		std::lock_guard<std::mutex> lock(mWatermarkTaskMutex);
		mWatermarkWorkerStopping = true;
	}
	mWatermarkTaskCondition.notify_all();
	if (mWatermarkWorker.joinable())
	{
		mWatermarkWorker.join();
	}
}

/**
 * @brief To update the max DRM sessions supported
 *
 * @param[in] maxSessions max DRM Sessions
 */
void AampDRMLicenseManager::UpdateMaxDRMSessions(int maxSessions)
{
	AAMPLOG_MIL("[LicMgr] UpdateMaxDRMSessions old:%d new:%d", mMaxDRMSessions, maxSessions);
	mDrmSessionManager->UpdateMaxDRMSessions(maxSessions);
	SAFE_DELETE_ARRAY( mLicenseDownloader );
        mLicenseDownloader = new AampCurlDownloader[maxSessions];
        mLicenseRenewalThreads.resize(maxSessions);
}

/**
 * @brief Clear DRM sessions
 *
 * @param[in] forceClearSession if true, clear all sessions including valid ones
 */
void AampDRMLicenseManager::clearDrmSession(bool forceClearSession)
{
	AAMPLOG_MIL("[LicMgr] clearDrmSession forceClearSession:%d maxSessions:%d", forceClearSession, mMaxDRMSessions);
	mDrmSessionManager->clearDrmSession(forceClearSession);
	for (int i = 0; i < mMaxDRMSessions; i++)
	{
		bool isFailedKeyEntries = mDrmSessionManager->getFailedKeyIdStatus(i);
		if ((mDrmSessionManager->drmSessionContexts != NULL && (isFailedKeyEntries || forceClearSession)))
		{
			if (mDrmSessionManager->drmSessionContexts[i].drmSession != NULL)
			{
				AAMPLOG_INFO("Clearing Session %d, isFailedKeyEntries=%d, forceClearSession=%d", i, isFailedKeyEntries, forceClearSession);
				mLicenseDownloader[i].Clear();
			}
		}
	}
}

/**
 * @brief Clean up the failed keyIds.
 */
void AampDRMLicenseManager::clearFailedKeyIds()
{
	AAMPLOG_MIL("[LicMgr] clearFailedKeyIds");
	mDrmSessionManager->clearFailedKeyIds();
}

/**
 * @brief Set Session manager state
 */
void AampDRMLicenseManager::setSessionMgrState(SessionMgrState state)
{
	AAMPLOG_MIL("[LicMgr] setSessionMgrState state:%d", (int)state);
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
	    AAMPLOG_INFO("[LicMgr] getSessionMgrState: no session manager, returning eSESSIONMGR_INACTIVE");
	    return SessionMgrState::eSESSIONMGR_INACTIVE;
	}
}
/**
 * @brief Re-use the current seesion ID, watermarking variables and de-activate watermarking session status
 */
void AampDRMLicenseManager::notifyCleanup()
{
	AAMPLOG_MIL("[LicMgr] notifyCleanup");
	mDrmSessionManager->notifyCleanup();
}
/**
 *  @brief Create DrmSession by using the AampDrmHelper object
 */
DrmSession* AampDRMLicenseManager::createDrmSession( std::shared_ptr<DrmHelper> drmHelper, DrmCallbacks* aampInstance, DrmMetaDataEventPtr eventHandle, int streamTypeIn)
{
	int err = -1;
	void *ptr= static_cast<void*>(&eventHandle);
	int responseCode =-1;

	AAMPLOG_MIL("[LicMgr] createDrmSession(helper) streamType:%d uuid:%s", streamTypeIn, drmHelper ? drmHelper->getUuid().c_str() : "null");
	DrmSession* session = mDrmSessionManager->createDrmSession(responseCode, err , drmHelper, aampInstance, streamTypeIn,ptr );
	AAMPLOG_MIL("[LicMgr] createDrmSession(helper) returned session:%p err:%d responseCode:%d", static_cast<void*>(session), err, responseCode);
	// Check if session creation failed
	if(err != -1)
	{
		// Map middleware DRM error to AAMP tune failure
		eventHandle->setFailure(MapDrmToAampTuneFailure(static_cast<DrmTuneFailure>(err)));
		AAMPLOG_ERR("DRM session creation failed with middleware error: %d", err);
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
	int responseCode =-1;
	AAMPLOG_MIL("[LicMgr] createDrmSession(initData) systemId:%s mediaFormat:%d initDataLen:%u streamType:%d isPrimarySession:%d contentMetadataAvailable:%d", systemId ? systemId : "null", (int)mediaFormat, (unsigned int)initDataLen, streamType, isPrimarySession, (contentMetadataPtr != nullptr));
    DrmSession * session = mDrmSessionManager->createDrmSession(responseCode, err,  systemId,  mediaFormat,  initDataPtr,initDataLen,  streamType, aamp, ptr,  contentMetadataPtr,isPrimarySession);
	AAMPLOG_MIL("[LicMgr] createDrmSession(initData) returned session:%p err:%d responseCode:%d", static_cast<void*>(session), err, responseCode);

	if(err != -1)
	{
		// Map middleware DRM error to AAMP tune failure
		eventHandle->setFailure(MapDrmToAampTuneFailure(static_cast<DrmTuneFailure>(err)));
		AAMPLOG_ERR("DRM session creation failed with middleware error: %d", err);
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
        case MW_DRM_SESSION_CREATE_FAILED:  return AAMP_TUNE_DRM_SESSION_CREATE_FAILED;
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
        default:
            AAMPLOG_WARN("[LicMgr] MapDrmToAampTuneFailure: unmapped middleware DRM error:%d", (int)drmError);
            return AAMP_TUNE_UNTRACKED_DRM_ERROR;
    }
}
