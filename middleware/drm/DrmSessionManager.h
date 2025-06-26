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
* @file DrmSessionManager.h
* @brief Header file for DRM session manager
*/
#ifndef DrmSessionManager_h
#define DrmSessionManager_h

#include "DrmSessionFactory.h"
#include "DrmSession.h"
#include "DrmUtils.h"
#include "GstUtils.h"
#include <string>
#include <atomic>
#include "DrmHelper.h"

#include "PlayerSecInterface.h"
#include "PlayerSecManagerSession.h"

#include <functional>


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

	DrmSessionContext() : sessionMutex(), drmSession(NULL),data()
	{
	}
	DrmSessionContext(const DrmSessionContext& other) : data(other.data), drmSession(other.drmSession)
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

struct configs{
    bool mUseSecManager;
    int mLicenseRetryWaitTime;
    int mDrmNetworkTimeout;
    int mDrmStallTimeout;
    int mCurlConnectTimeout;
    bool mCurlLicenseLogging;
    bool mRuntimeDRMConfig;
    int mContentProtectionDataUpdateTimeout;
    bool mEnablePROutputProtection;
    bool  mPropagateURIParam;
    bool mIsFakeTune;
    bool mIsWVKIDWorkaround;
};
/**
 *  @class	DrmSessionManager
 *  @brief	Controller for managing DRM sessions.
 */
class DrmSessionManager
{
	public:

	DrmSessionContext *drmSessionContexts;
	configs *m_drmConfigParam;
	PlayerSecInterface *playerSecInstance;/** PlayerSecInterface instance **/
	PlayerSecManagerSession mPlayerSecManagerSession;
private:
	KeyID *cachedKeyIDs;
	char* accessToken;
	int accessTokenLen;
	SessionMgrState sessionMgrState;
	std::mutex accessTokenMutex;
	std::mutex cachedKeyMutex;
	std::mutex mDrmSessionLock;
	bool mEnableAccessAttributes;
	int mMaxDRMSessions;
	std::atomic<bool> mIsVideoOnMute;
	std::atomic<int> mCurrentSpeed;
	std::atomic<bool> mFirstFrameSeen;
	std::function<void(uint32_t, uint32_t, const std::string&)> mPlayerSendWatermarkSessionUpdateEventCB;
	/**     
	 * @brief Copy constructor disabled
	 *
	 */
	DrmSessionManager(const DrmSessionManager &) = delete;
	/**
 	 * @brief assignment operator disabled
	 *
 	 */
	DrmSessionManager& operator=(const DrmSessionManager &) = delete;
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
	 *  @fn DrmSessionManager
	 */
	DrmSessionManager(int maxDrmSessions, void *player, std::function<void(uint32_t, uint32_t, const std::string&)> watermarkSessionUpdateCallback);

	void initializeDrmSessions();

	/**
	 *  @fn watermarkSessionHandlerWrapper
	 *  @brief Wrapper function to handle session watermark.
	 *  @param[in]	sessionHandle - Session handle.
	 *  @param[in]	status - Status of the session.
	 *  @param[in]	systemData - System data.
	 */
	void watermarkSessionHandlerWrapper(uint32_t sessionHandle, uint32_t status, const std::string &systemData);

	/**
	 *  @fn registerCallback
	 */
	void registerCallback( );

	/**
	 * @brief Set the Common Key Duration object
	 *
	 * @param keyDuration key duration
	 */
	void SetCommonKeyDuration(int keyDuration);

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
	bool QueueContentProtection(DrmHelperPtr drmHelper, std::string periodId, uint32_t adapIdx, GstMediaType type, bool isVssPeriod = false);

	/**
	 * @brief Queue a content protection event to the pipeline
	 * 
	 * @param drmHelper DrmHelper shared_ptr
	 * @param periodId ID of the period to which CP belongs to
	 * @param adapId Index of the adaptation to which CP belongs to
	 * @param type media type
	 * @return none
	 */
	void QueueProtectionEvent(DrmHelperPtr drmHelper, std::string periodId, uint32_t adapIdx, GstMediaType type);


	/**
	 *  @fn ~DrmSessionManager
	 */
	~DrmSessionManager();
	/**
	 *  @fn 	createDrmSession
	 *  @param[in]	err - To retrieve the error case  and to report to application 
	 *  @param[in]	systemId - UUID of the DRM system.
	 *  @param[in]	initDataPtr - Pointer to PSSH data.
	 *  @param[in]	dataLength - Length of PSSH data.
	 *  @param[in]	streamType - Whether audio or video.
	 *  @param[in]	contentMetadata - Pointer to content meta data, when content meta data
	 *  			is already extracted during manifest parsing. Used when content meta data
	 *  			is available as part of another PSSH header, like DRM Agnostic PSSH
	 *  			header.
	 *  @param[in]	Player - Pointer to player, for DRM related profiling.
	 *  @retval  	error_code - Gets updated with proper error code, if session creation fails.
	 *  			No NULL checks are done for error_code, caller should pass a valid pointer.
	 */
	DrmSession * createDrmSession(int &err, const char* systemId, MediaFormat mediaFormat,
			const unsigned char * initDataPtr, uint16_t dataLength, int streamType,
			DrmCallbacks* player, void *ptr, const unsigned char *contentMetadata = nullptr, 
	                	bool isPrimarySession = false );
	/**
	 * @fn createDrmSession
	 * @return drmSession
	 */
	DrmSession* createDrmSession( int &err, DrmHelperPtr drmHelper,  DrmCallbacks* Instance, int streamType, void *metaDataPtr);

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
	void setPlaybackSpeedState(bool live, double currentLatency, bool livepoint , double liveOffsetMs,int speed, double positionMs, bool firstFrameSeen = false);
	
	/**
	 * @fn 	Update tracking of video mute status and update watermarking requests as required, based on video presence, video mute state, and speed
	 * @param	videoMuteStatus video mute state to be set
	 * @param	seek_pos_seconds indicates the playback position at which most recent playback activity began
	 * @return	void.
 	 */
 	void setVideoMute(bool live, double currentLatency, bool livepoint , double liveOffsetMs,bool videoMuteStatus, double positionMs);
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
	KeyState getDrmSession(int &err, DrmHelperPtr drmHelper, int &selectedSlot, DrmCallbacks* Instance, bool isPrimarySession = false );
	/**
	 * @fn getSlotIdForSession
	 * @return index to the session slot for selected drmSessionContext 
	 */
	int getSlotIdForSession(DrmSession* session);
	/**
	 * @fn releaseLicenseRenewalThreads
	 */
	void releaseLicenseRenewalThreads();
	/**
	 * @fn initializeDrmSession
	 */
	KeyState initializeDrmSession(DrmHelperPtr drmHelper, int sessionSlot,  int &err );
	/**
	 * @fn notifyCleanup
	 */
	void notifyCleanup();


	/**
	 * @brief To update the max DRM sessions supported
	 *
	 * @param[in] maxSessions max DRM Sessions
	 */
	void UpdateMaxDRMSessions(int maxSessions);

        /*
         *@brief Type definition for acquireLicense callback from application 
         */
        using LicenseCallback = std::function<KeyState(DrmHelperPtr drmHelper, int sessionSlot, int &cdmError,
                        GstMediaType streamType,void *metaDataPtr, bool isLicenseRenewal)>;
        LicenseCallback AcquireLicenseCb;
        /*
         *@brief Registers acquireLicense callback from application 
         */
        void RegisterLicenseDataCb(const LicenseCallback Callback)      {
               AcquireLicenseCb = Callback;
        };
	/*
	 * @brief Register Profile update callback to application 
	 */
        using ProfileUpdateCallback =	std::function<void()>;
	ProfileUpdateCallback ProfileUpdateCb;

	void RegisterProfilingUpdateCb(const ProfileUpdateCallback callback)
        {
              ProfileUpdateCb = callback;
        };

	using ProfileBeginCallback = std::function<void(int)>;
	ProfileBeginCallback profileBeginCb;
	void RegisterProfBegin(const ProfileBeginCallback callback)
	{
		profileBeginCb = callback;
	};

	using ProfileEndCallback = std::function<void(int streamType)>;
	ProfileEndCallback profileEndCb;
	void RegisterProfEnd(const ProfileEndCallback callback)
	{
		profileEndCb = callback;
	};

	using ProfileErrorCallback = std::function<void(int streamType, int result)>;
	ProfileErrorCallback profileErrorCb;
	void RegisterProfError(const ProfileErrorCallback callback)
	{
		profileErrorCb = callback;
	};

	using LAProfileBeginCallback = std::function<void(int)>;
	LAProfileBeginCallback laprofileBeginCb;
	void RegisterLAProfBegin(const LAProfileBeginCallback callback)
	{
		laprofileBeginCb = callback;
	};

	using LAProfileEndCallback = std::function<void(int streamType)>;
	LAProfileEndCallback laprofileEndCb;
	void RegisterLAProfEnd(const LAProfileEndCallback callback)
	{
		laprofileEndCb = callback;
	};

	using LAProfileErrorCallback = std::function<void(void *ptr)>;
	LAProfileErrorCallback laprofileErrorCb;
	void RegisterLAProfError(const LAProfileErrorCallback callback)
	{
		laprofileErrorCb = callback;
	};

	using SetFailureCallback = std::function<void(void *ptr, int err)>;
	SetFailureCallback setfailureCb;
	void RegisterSetFailure(const SetFailureCallback callback)
	{
		setfailureCb = callback;
	};

	//using DrmMetaDataCallback =	std::function<void *()>;
	using DrmMetaDataCallback = std::function<std::shared_ptr<void>()>;
	DrmMetaDataCallback DrmMetaDataCb;
	void RegisterMetaDataCb(const DrmMetaDataCallback callback)
	{
		DrmMetaDataCb = callback;
	};
	/*
	 * @brief Register Content Protection Update callback to application 
	 */
	using ContentUpdateCallback = std::function<std::string(DrmHelperPtr drmHelper, int streamType, std::vector<uint8_t> keyId, int contentProtectionUpd)>;
	ContentUpdateCallback ContentUpdateCb;
	void RegisterHandleContentProtectionCb(const ContentUpdateCallback callback)
	{
	    ContentUpdateCb = callback;
	};

	/*
	 * @brief Custom data stores  
	 */
        std::string mCustomData;
        /**
	 * @brief Configuration parameters needed from Player
	 */
        void UpdateDRMConfig(
                       bool useSecManager,
                       bool enablePROutputProtection,
                       bool propagateURIParam,
                       bool isFakeTune,
		       bool wideVineKIDWorkaround);


};

/**
 * @struct writeCallbackData
 * @brief structure to hold DRM data to write
 */

typedef struct writeCallbackData{
	DrmData *data ;
	DrmSessionManager* mDrmSessionManager;
}writeCallbackData;

#endif
