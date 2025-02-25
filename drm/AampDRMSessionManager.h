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
* @file AampDRMSessionManager.h
* @brief Header file for DRM session manager
*/
#ifndef AampDRMSessionManager_h
#define AampDRMSessionManager_h

#include "drmsessionfactory.h"
#include "DrmSession.h"
#include "DrmUtils.h"
#include <string>
#include <atomic>
#include "AampCurlDownloader.h"
#include "DrmHelper.h"

#ifdef USE_SECCLIENT
#include "sec_client.h"
#endif

#include "AampDRMLicPreFetcher.h"

class PrivateInstanceAAMP;

#define VIDEO_SESSION 0
#define AUDIO_SESSION 1

/**
 *  @struct	DrmSessionContext
 *  @brief	To store drmSession and keyId data.
 */
struct DrmSessionContext
{
	std::vector<uint8_t> data;
	std::mutex sessionMutex;
	DrmSession * drmSession;
	AampCurlDownloader mLicenseDownloader;

	DrmSessionContext() : sessionMutex(), drmSession(NULL),data(),mLicenseDownloader()
	{
	}
	DrmSessionContext(const DrmSessionContext& other) : data(other.data), drmSession(other.drmSession),mLicenseDownloader()
	{
		// Releases memory allocated after destructing any of these objects
	}
	DrmSessionContext& operator=(const DrmSessionContext& other)
	{
		data = other.data;
		drmSession = other.drmSession;
		return *this;
	}
	~DrmSessionContext()
	{
	}
};

/**
 *  @struct	KeyID
 *  @brief	Structure to hold, keyId and session creation time for
 *  		keyId
 */
struct KeyID
{
	std::vector<std::vector<uint8_t>> data;
	long long creationTime;
	bool isFailedKeyId;
	bool isPrimaryKeyId;

	KeyID();
};

/**
 *  @brief	Enum to represent session manager state.
 *  		Session manager would abort any createDrmSession
 *  		request if in eSESSIONMGR_INACTIVE state.
 */
typedef enum{
	eSESSIONMGR_INACTIVE, /**< DRM Session mgr is inactive */
	eSESSIONMGR_ACTIVE    /**< DRM session mgr is active */	
}SessionMgrState;

/**
 *  @brief	Enum to represent DRM request type.
 */
typedef enum{
	DRM_GET_LICENSE, /**< DRM get license request */
	DRM_GET_LICENSE_SEC    /**< DRM get license SEC request */
}DrmRequestType;

/**
 *  @class	AampDRMSessionManager
 *  @brief	Controller for managing DRM sessions.
 */
class AampDRMSessionManager
{

private:
	DrmSessionContext *drmSessionContexts;
	KeyID *cachedKeyIDs;
	char* accessToken;
	int accessTokenLen;
	SessionMgrState sessionMgrState;
	std::mutex accessTokenMutex;
	std::mutex cachedKeyMutex;
	std::mutex mDrmSessionLock;
	bool licenseRequestAbort;
	bool mEnableAccessAttributes;
	int mMaxDRMSessions;
	std::vector<std::thread> mLicenseRenewalThreads;
	AampCurlDownloader mAccessTokenConnector;
	AampLicensePreFetcher* mLicensePrefetcher; /**< DRM license prefetcher instance */
	PrivateInstanceAAMP *aampInstance; /** AAMP instance **/
#ifdef USE_SECMANAGER
	AampSecManagerSession mAampSecManagerSession;
	std::atomic<bool> mIsVideoOnMute;
	std::atomic<int> mCurrentSpeed;
	std::atomic<bool> mFirstFrameSeen;
#endif
	/**     
     	 * @brief Copy constructor disabled
     	 *
     	 */
	AampDRMSessionManager(const AampDRMSessionManager &) = delete;
	/**
 	 * @brief assignment operator disabled
	 *
 	 */
	AampDRMSessionManager& operator=(const AampDRMSessionManager &) = delete;
	/**
	 *  @fn write_callback
	 *  @param[in]	ptr - Pointer to received data.
	 *  @param[in]	size, nmemb - Size of received data (size * nmemb).
	 *  @param[out]	userdata - Pointer to buffer where the received data is copied.
	 *  @return		returns the number of bytes processed.
	 */
	static size_t write_callback(char *ptr, size_t size, size_t nmemb,
			void *userdata);
	/**
	 * @brief
	 * @param clientp app-specific as optionally set with CURLOPT_PROGRESSDATA
	 * @param dltotal total bytes expected to download
	 * @param dlnow downloaded bytes so far
	 * @param ultotal total bytes expected to upload
	 * @param ulnow uploaded bytes so far
	 * @retval Return non-zero for CURLE_ABORTED_BY_CALLBACK
	 */
	static int progress_callback(void *clientp,	double dltotal, 
			double dlnow, double ultotal, double ulnow );

	/**
	 * @brief callback invoked on http header by curl
	 * @param ptr pointer to buffer containing the data
	 * @param size size of the buffer
	 * @param nmemb number of bytes
	 * @param user_data  CurlCallbackContext pointer
	 * @retval
	 */
	static size_t header_callback(const char *ptr, size_t size, size_t nmemb, void *user_data);
public:
	
	/**
	 *  @fn AampDRMSessionManager
	 */
	AampDRMSessionManager(int maxDrmSessions, PrivateInstanceAAMP *aamp);

	void initializeDrmSessions();

	/**
	 * @brief Set the Common Key Duration object
	 * 
	 * @param keyDuration key duration
	 */
	void SetCommonKeyDuration(int keyDuration);

	/**
	 * @brief set license prefetcher
	 * 
	 * @return none
	 */
	void SetLicenseFetcher(AampLicenseFetcher *fetcherInstance);

	/**
	 * @brief Set to true if error event to be sent to application if any license request fails
	 *  Otherwise, error event will be sent if a track doesn't have a successful or pending license request
	 * 
	 * @param sendErrorOnFailure key duration
	 */
	void SetSendErrorOnFailure(bool sendErrorOnFailure);

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
	bool QueueContentProtection(DrmHelperPtr drmHelper, std::string periodId, uint32_t adapIdx, AampMediaType type, bool isVssPeriod = false);

	/**
	 * @brief Queue a content protection event to the pipeline
	 * 
	 * @param drmHelper DrmHelper shared_ptr
	 * @param periodId ID of the period to which CP belongs to
	 * @param adapId Index of the adaptation to which CP belongs to
	 * @param type media type
	 * @return none
	 */
	void QueueProtectionEvent(DrmHelperPtr drmHelper, std::string periodId, uint32_t adapIdx, AampMediaType type);

	/**
	 * @brief Stop DRM session manager and terminate license fetcher
	 * 
	 * @param none
	 * @return none
	 */
	void Stop();

	/**
	 *  @fn ~AampDRMSessionManager
	 */
	~AampDRMSessionManager();
	/**
	 *  @fn 	createDrmSession
	 *  @param[in]	systemId - UUID of the DRM system.
	 *  @param[in]	initDataPtr - Pointer to PSSH data.
	 *  @param[in]	dataLength - Length of PSSH data.
	 *  @param[in]	streamType - Whether audio or video.
	 *  @param[in]	contentMetadata - Pointer to content meta data, when content meta data
	 *  			is already extracted during manifest parsing. Used when content meta data
	 *  			is available as part of another PSSH header, like DRM Agnostic PSSH
	 *  			header.
	 *  @param[in]	aamp - Pointer to PrivateInstanceAAMP, for DRM related profiling.
	 *  @retval  	error_code - Gets updated with proper error code, if session creation fails.
	 *  			No NULL checks are done for error_code, caller should pass a valid pointer.
	 */
	DrmSession * createDrmSession(const char* systemId, MediaFormat mediaFormat,
			const unsigned char * initDataPtr, uint16_t dataLength, AampMediaType streamType,
			PrivateInstanceAAMP* aamp, DrmMetaDataEventPtr e, const unsigned char *contentMetadata = nullptr,
			bool isPrimarySession = false);
	/**
	 * @fn createDrmSession
	 * @return AampdrmSession
	 */
	DrmSession* createDrmSession(DrmHelperPtr drmHelper, DrmMetaDataEventPtr eventHandle, PrivateInstanceAAMP* aampInstance, AampMediaType streamType);

#if defined(USE_SECCLIENT) || defined(USE_SECMANAGER)
	DrmData * getLicenseSec(const LicenseRequest &licenseRequest, DrmHelperPtr drmHelper,
			const ChallengeInfo& challengeInfo, PrivateInstanceAAMP* aampInstance, int32_t *httpCode, int32_t *httpExtStatusCode, DrmMetaDataEventPtr eventHandle);
#endif
	/**
	 *  @fn 	getLicense
	 *
	 *  @param[in]	licRequest - License request details (URL, headers etc.)
	 *  @param[out]	httpError - Gets updated with http error; default -1.
	 *  @param[in]	eventHandle - DRM Metadata event handle
	 *  @param[in]	isContentMetadataAvailable - Flag to indicate whether MSO specific headers
	 *  			are to be used.
	 *  @param[in]	licenseProxy - Proxy to use for license requests.
	 *  @return		Structure holding DRM license key and it's length; NULL and 0 if request fails
	 *  @note		Memory for license key is dynamically allocated, deallocation
	 *				should be handled at the caller side.
	 *			customHeader ownership should be taken up by getLicense function
	 *
	 */
	DrmData * getLicense(LicenseRequest &licRequest, int32_t *httpError, AampMediaType streamType, PrivateInstanceAAMP* aamp, DrmMetaDataEventPtr eventHandle,AampCurlDownloader *pLicenseDownloader,std::string licenseProxy="");
	/**
	 *  @fn		IsKeyIdProcessed
	 *  @param[in]	keyIdArray - key Id extracted from pssh data
	 *  @param[out]	status - processed status of the key id success/fail
	 *  @return		bool - true if keyId is already marked as failed or processed,
	 * 				false if key is not cached
	 */
	bool IsKeyIdProcessed(std::vector<uint8_t> keyIdArray, bool &status);
	/**
	 *  @fn         clearSessionData
	 *
	 *  @return	void.
	 */
	void clearSessionData();
	/**
	 *  @fn 	clearAccessToken
	 *
	 *  @return	void.
	 */
	void clearAccessToken();
	/**
	 * @fn		clearFailedKeyIds
	 *
	 * @return	void.
	 */
	void clearFailedKeyIds();
	/**
	 * @fn		clearDrmSession
	 *
	 * @param 	forceClearSession clear the drm session irrespective of failed keys if LicenseCaching is false.
	 * @return	void.
	 */
	void clearDrmSession(bool forceClearSession = false);

	void setVideoWindowSize(int width, int height);

	/**
	 * @fn 	De-activate watermark and prevent it from being re-enabled until we get a new first video frame at normal play speed
	 * @return	void.
 	 */
	void hideWatermarkOnDetach();

	/**
	 * @fn 	Update tracking of speed status and send watermarking requests as required. This is based on video presence, video mute state, and speed
	 * @param	speed playback speed
	 * @param	position indicates the playback position at which most recent playback activity began
	 * @param   firstFrameSeen set to true the first time we see a video frame after tune
	 * @return	void.
 	 */
	void setPlaybackSpeedState(int speed, double positionMs, bool firstFrameSeen = false);
	
	/**
	 * @fn 	Update tracking of video mute status and update watermarking requests as required, based on video presence, video mute state, and speed
	 * @param	videoMuteStatus video mute state to be set
	 * @param	seek_pos_seconds indicates the playback position at which most recent playback activity began
	 * @return	void.
 	 */
 	void setVideoMute(bool videoMuteStatus, double positionMs);
	/**
	 * @fn    	setSessionMgrState
	 * @param	state session manager sate to be set
	 * @return	void.
	 */
	void setSessionMgrState(SessionMgrState state);
	
	/**
	 * @fn getSessionMgrState
	 * @return session manager state.
	 */
	SessionMgrState getSessionMgrState();
	/**
	 * @fn		setLicenseRequestAbort
	 * @param	isAbort bool flag to curl abort
	 * @return	void
	 */
	void setLicenseRequestAbort(bool isAbort);
	/**
	 *  @fn getAccessToken
	 *
	 *  @param[out]	tokenLength - Gets updated with accessToken length.
	 *  @return		Pointer to accessToken.
	 *  @note		AccessToken memory is dynamically allocated, deallocation
	 *				should be handled at the caller side.
	 */
	const char* getAccessToken(int &tokenLength, int &error_code ,bool bSslPeerVerify);
	/**
	 * @fn getDrmSession
	 * @return index to the selected drmSessionContext which has been selected
	 */
	KeyState getDrmSession(DrmHelperPtr drmHelper, int &selectedSlot, DrmMetaDataEventPtr eventHandle, PrivateInstanceAAMP* aampInstance, bool isPrimarySession = false);
	/**
	 * @fn getSlotIdForSession
	 * @return index to the session slot for selected drmSessionContext 
	 */
	int getSlotIdForSession(DrmSession* session);
	/**
	 * @fn renewLicense
	 *
	 * @param[in] drmHelper - Current drm helper
	 * @param[in] userData - Data holds the current drm Session
	 * @param[in] aampInstance - Aamp instance
	 * @return void
	 */

	void renewLicense(DrmHelperPtr drmHelper, void* userData, PrivateInstanceAAMP* aampInstance);
	/**
	 * @fn licenseRenewalThread
	 *
	 * @param[in] drmHelper - Current drm helper
	 * @param[in] sessionSlot - Session slot that holds the current drm Session
	 * @param[in] aampInstance - Aamp instance
	 * @return void
 	 */
	void licenseRenewalThread(DrmHelperPtr drmHelper, int sessionSlot, PrivateInstanceAAMP* aampInstance);
	/**
	 * @fn releaseLicenseRenewalThreads
	 */
	void releaseLicenseRenewalThreads();
	/**
	 * @fn initializeDrmSession
	 */
	KeyState initializeDrmSession(DrmHelperPtr drmHelper, int sessionSlot, DrmMetaDataEventPtr eventHandle, PrivateInstanceAAMP* aampInstance);
	/**
	 * @fn acquireLicense
	 */
	KeyState acquireLicense(DrmHelperPtr drmHelper, int sessionSlot, int &cdmError,
			DrmMetaDataEventPtr eventHandle, PrivateInstanceAAMP* aampInstance, AampMediaType streamType, bool isLicenseRenewal = false);

	KeyState handleLicenseResponse(DrmHelperPtr drmHelper, int sessionSlot, int &cdmError,
			int32_t httpResponseCode, int32_t httpExtResponseCode, shared_ptr<DrmData> licenseResponse, DrmMetaDataEventPtr eventHandle, PrivateInstanceAAMP* aampInstance, bool isLicenseRenewal = false);

	KeyState processLicenseResponse(DrmHelperPtr drmHelper, int sessionSlot, int &cdmError,
			shared_ptr<DrmData> licenseResponse, DrmMetaDataEventPtr eventHandle, PrivateInstanceAAMP* aampInstance, bool isLicenseRenewal = false);
	/**
	 * @fn configureLicenseServerParameters
	 */
	bool configureLicenseServerParameters(DrmHelperPtr drmHelper, LicenseRequest& licRequest,
			std::string &licenseServerProxy, const ChallengeInfo& challengeInfo, PrivateInstanceAAMP* aampInstance);
	/**
	 * @fn notifyCleanup
	 */
	void notifyCleanup();

	/**
	 * @fn ContentProtectionDataUpdate
	 */
	void ContentProtectionDataUpdate(PrivateInstanceAAMP* aampInstance, std::vector<uint8_t> keyId, AampMediaType streamType);

	/**
	 * @brief To update the max DRM sessions supported
	 *
	 * @param[in] maxSessions max DRM Sessions
	 */
	void UpdateMaxDRMSessions(int maxSessions);

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
	void UpdateLicenseMetrics(DrmRequestType requestType, int32_t statusCode, std::string licenseRequestUrl, long long downloadTimeMS, DrmMetaDataEventPtr eventHandle, DownloadResponsePtr respData );
};

/**
 * @struct writeCallbackData
 * @brief structure to hold DRM data to write
 */

typedef struct writeCallbackData{
	DrmData *data ;
	AampDRMSessionManager* mDRMSessionManager;
}writeCallbackData;

#endif
