/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2023 RDK Management
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
#ifndef _AAMP_LICENSE_PREFETCHER_HPP
#define _AAMP_LICENSE_PREFETCHER_HPP

#include <thread>
#include <deque>
#include <string>
#include <vector>
#include <memory>
#include <array>
#include <mutex>
#include <condition_variable>
#include "DrmHelper.h"
#include "AampLogManager.h"
#include "AampDefine.h"
#include "AampEvent.h"
#include "AampDRMLicPreFetcherInterface.h"

#define SECCLIENT_RESULT_HTTP_FAILURE_TIMEOUT (-7) /**< License result is not returned to the device due to network failure */

class PrivateInstanceAAMP;

/**
 * @brief Structure for storing the pre-fetch data
 *
 */
struct LicensePreFetchObject
{
	DrmHelperPtr mHelper; /** drm helper for the content protection*/
	std::string mPeriodId;                  /** Period ID*/
	uint32_t mAdaptationIdx;                /** adaptation Index*/
	AampMediaType mType;                        /** Stream type*/
	int mId;                                /** Object ID*/
	static int staticId;
	bool mIsVssPeriod;

	/**
	 * @brief Construct a new License Pre Fetch Object object
	 * 
	 * @param drmHelper DrmHelper shared_ptr
	 * @param periodId ID of the period to which CP belongs to
	 * @param adapIdx Index of the adaptation to which CP belongs to
	 * @param isVssPeriod flag denotes if this is for a VSS period
	 * @param type media type
	 */
	LicensePreFetchObject(DrmHelperPtr drmHelper, std::string periodId, uint32_t adapIdx, AampMediaType type, bool isVssPeriod): mHelper(drmHelper),
															mPeriodId(periodId),
															mAdaptationIdx(adapIdx),
															mType(type),
															mId(staticId++),
															mIsVssPeriod(isVssPeriod)
	{
		AAMPLOG_TRACE("Creating new LicensePreFetchObject, id:%d", mId);
	}

	/**
	 * @brief Destroy the License Pre Fetch Object object
	 * 
	 */
	~LicensePreFetchObject()
	{
		AAMPLOG_TRACE("Deleting LicensePreFetchObject, id:%d", mId);
	}

	/**
	 * @brief Compare two LicensePreFetchObject objects
	 * 
	 * @param other LicensePreFetchObject to be compared to
	 * @return true if both objects are having same info
	 * @return false otherwise
	 */
	bool compare(std::shared_ptr<LicensePreFetchObject> other) const
	{
		return ((other != nullptr) && mHelper->compare(other->mHelper) && mType == other->mType);
	}
};

using LicensePreFetchObjectPtr = std::shared_ptr<LicensePreFetchObject>;

/**
 * @brief Class for License PreFetcher module.
 * Handles the license pre-fetching responsibilities in a playback for faster tune times
 */
class AampLicensePreFetcher
{
public:
	/**
	 * @brief Default constructor disabled
	 */
	AampLicensePreFetcher() = delete;

	/**
	 * @brief Construct a new Aamp License Pre Fetcher object
	 * 
	 * @param aamp PrivateInstanceAAMP instance
	 * @param fetcherInstance AampLicenseFetcher instance
	 */
	AampLicensePreFetcher(PrivateInstanceAAMP *aamp);

	/**
	 * @brief Copy constructor disabled
	 * 
	 */
	AampLicensePreFetcher(const AampLicensePreFetcher&) = delete;

	/**
	 * @brief Assignment operator disabled
	 * 
	 */
	void operator=(const AampLicensePreFetcher&) = delete;

	/**
	 * @brief Destroy the Aamp License Pre Fetcher object
	 * 
	 */
	~AampLicensePreFetcher();

	/**
	 * @brief Initialize resources
	 * 
	 * @return true if successfully initialized
	 * @return false if error occurred
	 */
	bool Init();

	/**
	 * @brief Check to see if a key is already on the queue
	 * 
	 * @param fetchObject the key object to look for
	 * @return true if key is on the queue
	 * @return false if key is not on the queue
	 */
	bool KeyIsQueued(LicensePreFetchObjectPtr &fetchObject);

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
	 * @brief De-initialize/free resources
	 * 
	 * @return true if success
	 * @return false if failed
	 */
	bool Term();

	/**
	 * @brief set license prefetcher
	 * 
	 * @return none
	 */
	void SetLicenseFetcher(AampLicenseFetcher *fetcherInstance) { mFetchInstance = fetcherInstance; }

	/**
	 * @brief Set the Common Key Duration object
	 * 
	 * @param keyDuration key duration
	 */
	void SetCommonKeyDuration(int keyDuration) { mCommonKeyDuration = keyDuration; }

	/**
	 * @brief Thread for processing content protection queued using QueueContentProtection
	 * Thread will be joined when Term is called
	 *
	 */
	void PreFetchThread();

	/**
	 * @brief Set to true if error event to be sent to application if any license request fails
	 *  Otherwise, error event will be sent if a track doesn't have a successful or pending license request
	 * 
	 * @param sendErrorOnFailure key duration
	 */
	void SetSendErrorOnFailure(bool sendErrorOnFailure) { mSendErrorOnFailure = sendErrorOnFailure; }

	/**
         * @brief Thread for processing VSS content protection queued using QueueContentProtection
         * Thread will be joined when Term is called
         * 
         */

	void VssPreFetchThread();
private:

	/**
	 * @brief To notify DRM failure to player after proper checks
	 * 
	 * @param fetchObj object for which session creation failed
	 * @param event drm metadata event object with failure details
	 */
	void NotifyDrmFailure(LicensePreFetchObjectPtr fetchObj, DrmMetaDataEventPtr event);

	/**
	 * @brief Creates a DRM session for a content protection
	 * 
	 * @param fetchObj LicensePreFetchObject shared_ptr
	 * @return true if successfully created DRM session
	 * @return false if failed
	 */
	bool CreateDRMSession(LicensePreFetchObjectPtr fetchObj);

	std::thread mPreFetchThread;                        /** Thread for pre-fetching license*/
	std::deque<LicensePreFetchObjectPtr> mFetchQueue;   /** Queue for storing content protection objects*/
	std::mutex mQMutex;                                 /** Mutex for accessing the mFetchQueue*/
	std::condition_variable mQCond;                     /** Conditional variable to notify addition of an obj to mFetchQueue*/
	bool mPreFetchThreadStarted;                        /** Flag denotes if thread started*/
	bool mExitLoop;                                     /** Flag denotes if pre-fetch thread has to be exited*/
	int mCommonKeyDuration;                             /** Common key duration for deferred license acquisition*/
	std::array<bool, AAMP_TRACK_COUNT> mTrackStatus;    /** To mark the status of license acquisition for tracks*/
	bool mSendErrorOnFailure;                           /** To send error event when session creation fails without additional checks*/

	PrivateInstanceAAMP *mPrivAAMP;                     /** PrivateInstanceAAMP instance*/
	AampLicenseFetcher *mFetchInstance;                 /** AampLicenseFetcher instance for notifying DRM session status*/
	std::thread mVssPreFetchThread;                     /** Thread for pre-fetching VSS license*/
	std::deque<LicensePreFetchObjectPtr> mVssFetchQueue;/** Queue for storing VSS content protection objects*/
	std::mutex mQVssMutex;                              /** Mutex for accessing the mVssFetchQueue*/
	std::condition_variable mQVssCond;                  /** Conditional variable to notify addition of an obj to mVssFetchQueue*/
	bool mVssPreFetchThreadStarted;                     /** Flag denotes if Vss thread started*/
};

#endif /* _AAMP_LICENSE_PREFETCHER_HPP */
