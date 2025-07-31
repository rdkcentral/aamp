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
* @file AampDRMLicManager.h
* @brief Header file for DRM License manager
*/



#include "priv_aamp.h"
#include "AampDRMLicPreFetcher.h"
#include "DrmSession.h"
#include "AampCurlDownloader.h"
#include "DrmSessionManager.h"
#include "DrmUtils.h"
class AampDRMLicenseManager
{
public:
	/**
	 *  @fn AampDRMLicenseManager
	 */
	AampDRMLicenseManager(int maxDrmSessions, PrivateInstanceAAMP *aamp);

	/**
	 *  @fn ~AampDRMLicenseManager
	 */
	~AampDRMLicenseManager();
	DrmSessionManager *mDRMSessionManager;
	AampCurlDownloader* mLicenseDownloader;

	char* accessToken;
	int accessTokenLen;
	std::mutex accessTokenMutex;
	std::mutex cachedKeyMutex;
	bool licenseRequestAbort;
	int mMaxDRMSessions;
	std::vector<std::thread> mLicenseRenewalThreads;
	AampCurlDownloader mAccessTokenConnector;
	AampLicensePreFetcher* mLicensePrefetcher; /**< DRM license prefetcher instance */
	PrivateInstanceAAMP *aampInstance; /** AAMP instance **/
#ifdef 0
	AampSecManagerSession mAampSecManagerSession;
	std::atomic<bool> mIsVideoOnMute;
	std::atomic<int> mCurrentSpeed;
	std::atomic<bool> mFirstFrameSeen;
#endif
	/**
	 * @fn          setLicenseRequestAbort
	 * @param       isAbort bool flag to curl abort
	 * @return      void
	 */
	void setLicenseRequestAbort(bool isAbort);
	/**
	 *  @fn getAccessToken
	 *
	 *  @param[out] tokenLength - Gets updated with accessToken length.
	 *  @return             Pointer to accessToken.
	 *  @note               AccessToken memory is dynamically allocated, deallocation
	 *                              should be handled at the caller side.
	 */
	const char* getAccessToken(int &tokenLength, int &error_code ,bool bSslPeerVerify);
	/**
	 * @fn acquireLicense
	 */
	KeyState acquireLicense(std::shared_ptr<DrmHelper> drmHelper, int sessionSlot, int &cdmError,  
			AampMediaType streamType, void *metaDataPtr,  bool isLicenseRenewal = false);


	/**
	 * @fn handleLicenseResponse
	 */
	KeyState handleLicenseResponse(std::shared_ptr<DrmHelper> drmHelper, int sessionSlot, int &cdmError,
			int32_t httpResponseCode, int32_t httpExtResponseCode, shared_ptr<DrmData> licenseResponse, DrmMetaDataEventPtr eventHandle,  bool isLicenseRenewal = false);

	/**
	 * @fn processLicenseResponse
	 */
	KeyState processLicenseResponse(std::shared_ptr<DrmHelper> drmHelper, int sessionSlot, int &cdmError,
			shared_ptr<DrmData> licenseResponse, DrmMetaDataEventPtr eventHandle, bool isLicenseRenewal = false);
	/**
	 * @fn configureLicenseServerParameters
	 */
	bool configureLicenseServerParameters(std::shared_ptr<DrmHelper> drmHelper, LicenseRequest& licRequest,
			std::string &licenseServerProxy, const ChallengeInfo& challengeInfo, PrivateInstanceAAMP* aampInstance);

	/**
	 * @fn 	Update tracking of speed status and send watermarking requests as required. This is based on video presence, video mute state, and speed
	 * @param	speed playback speed
	 * @param	position indicates the playback position at which most recent playback activity began
	 * @param   firstFrameSeen set to true the first time we see a video frame after tune
	 * @return	void.
	 */
	void setPlaybackSpeedState(bool live, double currentLatency, bool livepoint , double liveOffsetMs,int speed, double positionMs, bool firstFrameSeen = false);

	/**
	 * @fn 	De-activate watermark and prevent it from being re-enabled until we get a new first video frame at normal play speed
	* @return	void.
	*/
	void hideWatermarkOnDetach();

	/**
	* @fn 	Update tracking of video mute status and update watermarking requests as required, based on video presence, video mute state, and speed
	* @param	videoMuteStatus video mute state to be set
	* @param	seek_pos_seconds indicates the playback position at which most recent playback activity began
	* @return	void.
	*/
	void setVideoMute(bool live, double currentLatency, bool livepoint , double liveOffsetMs,bool videoMuteStatus, double positionMs);

	void setVideoWindowSize(int width, int height);

	/**
	 * @brief To update the max DRM sessions supported
	 *
	 * @param[in] maxSessions max DRM Sessions
	 */
	void UpdateMaxDRMSessions(int maxSessions);

	/**
	 * @fn		clearDrmSession
	 *
	 * @param	forceClearSession clear the drm session irrespective of failed keys if LicenseCaching is false.
	 * @return	void.
	 */
	void clearDrmSession(bool forceClearSession = false);

	/**
	* @fn		clearFailedKeyIds
	*
	* @return	void.
	*/
	void clearFailedKeyIds();
			
	/**
	* @fn    	setSessionMgrState
	* @param	state session manager sate to be set
	* @return	void.
	*/
	void setSessionMgrState(SessionMgrState state);
        SessionMgrState getSessionMgrState();
	

	/**
	 * @fn notifyCleanup
	 */
	void notifyCleanup();

	/**
         * @fn renewLicense
         *
         * @param[in] drmHelper - Current drm helper
         * @param[in] userData - Data holds the current drm Session
         * @param[in] aampInstance - Aamp instance
         * @return void
         */

        void renewLicense(std::shared_ptr<DrmHelper> drmHelper, void* userData, PrivateInstanceAAMP* aampInstance);
        /**
         * @fn licenseRenewalThread
         *
         * @param[in] drmHelper - Current drm helper
         * @param[in] sessionSlot - Session slot that holds the current drm Session
         * @param[in] aampInstance - Aamp instance
         * @return void
         */
        void licenseRenewalThread(std::shared_ptr<DrmHelper> drmHelper, int sessionSlot);
         /**
         * @fn releaseLicenseRenewalThreads
         */
        void releaseLicenseRenewalThreads();

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
	/**
	 * @fn ContentProtectionDataUpdate
	 */
	void ContentProtectionDataUpdate(PrivateInstanceAAMP* aampInstance, std::vector<uint8_t> keyId, AampMediaType streamType);
	/**
	 * @brief Set the Common Key Duration object
	 * 
	 * @param keyDuration key duration
	 */
	void SetCommonKeyDuration(int keyDuration);
	/**
	 * @brief Stop DRM session manager and terminate license fetcher
	 * 
	 * @param none
	 * @return none
	 */
	void Stop();

	void SetLicenseFetcher(AampLicenseFetcher *fetcherInstance);
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
	bool QueueContentProtection(std::shared_ptr<DrmHelper> drmHelper, std::string periodId, uint32_t adapIdx, AampMediaType type, bool isVssPeriod = false);

	/**
	 * @brief Queue a content protection event to the pipeline
	 * 
	 * @param drmHelper DrmHelper shared_ptr
	 * @param periodId ID of the period to which CP belongs to
	 * @param adapId Index of the adaptation to which CP belongs to
	 * @param type media type
	 * @return none
	 */
	void QueueProtectionEvent(std::shared_ptr<DrmHelper> drmHelper, std::string periodId, uint32_t adapIdx, AampMediaType type);


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
	DrmData * getLicense(LicenseRequest &licRequest, int32_t *httpError, AampMediaType streamType, void* aamp, DrmMetaDataEventPtr eventHandle,AampCurlDownloader *pLicenseDownloader,std::string licenseProxy="");
#if defined(USE_SECCLIENT) || defined(USE_SECMANAGER)
	DrmData * getLicenseSec(const LicenseRequest &licenseRequest, std::shared_ptr<DrmHelper> drmHelper,
			const ChallengeInfo& challengeInfo, void* aampInstance, int32_t *httpCode, int32_t *httpExtStatusCode, DrmMetaDataEventPtr eventHandle);
#endif
	/**
	 * @fn ProfilerUpdate 
	 * @return void 
	 * */
        void ProfilerUpdate();
	/** 
	 * @fn HandleContentProtectionData
	 * @return string 
	 */
	std::string HandleContentProtectionData(std::shared_ptr<DrmHelper> drmHelper, int streamType, std::vector<uint8_t> keyId, int contentProtectionUpd);

	/** @fn 	Create the DRM session with DRM helper.
	 * @param[in]   drmHelper Pointer 
	 * @param[in]   instance of aamp
	 * @param[in]   event handle for capturing errors
	 * @param[in]   input stream type
	 */
	DrmSession* createDrmSession(std::shared_ptr<DrmHelper> drmHelper, DrmCallbacks* aampInstance,  DrmMetaDataEventPtr eventHandle, int streamTypeIn);
         /**
         *  @fn         createDrmSession
         *  @param[in]  systemId - UUID of the DRM system.
         *  @param[in]  initDataPtr - Pointer to PSSH data.
         *  @param[in]  dataLength - Length of PSSH data.
         *  @param[in]  streamType - Whether audio or video.
         *  @param[in]  contentMetadata - Pointer to content meta data, when content meta data
         *                      is already extracted during manifest parsing. Used when content meta data
         *                      is available as part of another PSSH header, like DRM Agnostic PSSH
         *                      header.
         *  @param[in]  aamp - Pointer to PrivateInstanceAAMP, for DRM related profiling.
         *  @retval     error_code - Gets updated with proper error code, if session creation fails.
         *                      No NULL checks are done for error_code, caller should pass a valid pointer.
         */
	DrmSession * createDrmSession(
		 const char* systemId, MediaFormat mediaFormat, const unsigned char * initDataPtr,
		uint16_t initDataLen, int streamType,
		DrmCallbacks* aamp, DrmMetaDataEventPtr eventHandle, const unsigned char* contentMetadataPtr = nullptr,
		 bool isPrimarySession = false);
	AAMPTuneFailure MapDrmToAampTuneFailure(DrmTuneFailure drmError);

};
