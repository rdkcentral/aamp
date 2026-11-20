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

#include "AampDRMLicPreFetcher.h"
#include "DrmSession.h"
#include "AampUtils.h"	// for aamp_GetDeferTimeMs
#include "priv_aamp.h"
#include "AampDRMLicManager.h"
#include "PlayerSecInterface.h"
/**
 * @brief For generating IDs for LicensePreFetchObject
 * 
 */
int LicensePreFetchObject::staticId = 1;

/**
 * @brief Construct a new Aamp License Pre Fetcher object
 * 
 * @param aamp PrivateInstanceAAMP instance
 * @param fetcherInstance AampLicenseFetcher instance
 */
AampLicensePreFetcher::AampLicensePreFetcher(PrivateInstanceAAMP *aamp) : mPreFetchThread(),
		mFetchQueue(),
		mQMutex(),
		mQCond(),
		mExitLoop(false),
		mCommonKeyDuration(0),
		mTrackStatus(),
		mSendErrorOnFailure(true),
		mPrivAAMP(aamp),
		mFetchInstance(nullptr),
		mLicenseAcquisitionMutex(),
		mVssPreFetchThread(),
		mVssFetchQueue(),
		mQVssMutex(),
		mQVssCond(),
		mIsSecClientError(false)
{
	mTrackStatus.fill(false);
	mIsSecClientError = isSecFeatureEnabled();
	AAMPLOG_MIL("[LicPreFetch] Constructed prefetcher:%p aamp:%p isSecFeatureEnabled:%d", static_cast<void*>(this), static_cast<void*>(mPrivAAMP), mIsSecClientError);
}

/**
 * @brief Wake a worker thread blocked on its queue and join it
 *
 * The caller must already have set the exit flag that the worker's wait
 * predicate checks.
 *
 * @param[in] thread Worker thread to join, if running
 * @param[in] name Thread name for logging
 * @param[in] mtx Mutex the worker holds while checking its wait predicate
 * @param[in] cv Condition variable the worker waits on
 */
static void StopWorkerThread(std::thread &thread, const char *name, std::mutex &mtx, std::condition_variable &cv)
{
	if (thread.joinable())
	{
		NotifyAllSynchronised(mtx, cv);
		AAMPLOG_MIL("Joining %s", name);
		thread.join();
		AAMPLOG_MIL("[LicPreFetch] Joined %s", name);
	}
	else
	{
		AAMPLOG_INFO("[LicPreFetch] %s not running, nothing to join", name);
	}
}

/**
 * @brief Destroy the Aamp License Pre Fetcher object
 * 
 */
AampLicensePreFetcher::~AampLicensePreFetcher()
{
	AAMPLOG_MIL("[LicPreFetch] Destroying prefetcher:%p", static_cast<void*>(this));
	Term();
	mExitLoop = true;
	StopWorkerThread(mPreFetchThread, "mPreFetchThread", mQMutex, mQCond);
	StopWorkerThread(mVssPreFetchThread, "mVssPreFetchThread", mQVssMutex, mQVssCond);
	AAMPLOG_MIL("[LicPreFetch] Prefetcher:%p destroyed", static_cast<void*>(this));
}

/**
 * @brief Initialize resources
 * 
 * @return true if successfully initialized
 * @return false if error occurred
 */
bool AampLicensePreFetcher::Init()
{
	bool ret = true;
	if (mPreFetchThread.joinable() || mVssPreFetchThread.joinable())
	{
		AAMPLOG_WARN("PreFetch thread is already started when calling Init!!");
	}
	mTrackStatus.fill(false);
	mExitLoop = false;
	AAMPLOG_MIL("[LicPreFetch] Init done, mTrackStatus cleared and mExitLoop reset");
	return ret;
}

/**
 * @brief Check to see if a key is already on the queue
 * 
 * @param fetchObject the key object to look for
 * @return true if key is on the queue
 * @return false if key is not on the queue
 */
bool AampLicensePreFetcher::KeyIsQueued(LicensePreFetchObjectPtr &fetchObject)
{
	std::vector<uint8_t> fetchKeyIdArray;
	std::vector<uint8_t> queuedKeyIdArray;

	fetchObject->mHelper->getKey(fetchKeyIdArray);
	AAMPLOG_INFO("[LicPreFetch] Checking if key already queued for type:%d keyId:%s queueSize:%zu", fetchObject->mType, AampLogManager::getHexDebugStr(fetchKeyIdArray).c_str(), mFetchQueue.size());
	for (auto queuedObject : mFetchQueue)
	{
		queuedObject->mHelper->getKey(queuedKeyIdArray);
		if ((queuedObject->mType == fetchObject->mType) &&
			(queuedKeyIdArray == fetchKeyIdArray))
		{
			AAMPLOG_INFO("[LicPreFetch] Key match found in queue, queued object id:%d period:%s adaptationIdx:%u", queuedObject->mId, queuedObject->mPeriodId.c_str(), queuedObject->mAdaptationIdx);
			return true;
		}
	}
	AAMPLOG_INFO("[LicPreFetch] Key not found in queue");
	return false;
}


/**
 * @brief Queue a content protection info to be processed later
 * 
 * @param drmHelper DrmHelper shared_ptr
 * @param periodId ID of the period to which CP belongs to
 * @param adapId Index of the adaptation to which CP belongs to
 * @param type media type
 * @return true if successfully queued
 * @return false if error occurred
 */
bool AampLicensePreFetcher::QueueContentProtection(DrmHelperPtr drmHelper, std::string periodId, uint32_t adapIdx, AampMediaType type, bool isVssPeriod)
{
	bool ret = true;
	AAMPLOG_MIL("[LicPreFetch] QueueContentProtection type:%d period:%s adaptationIdx:%u isVss:%d helper:%s exitLoop:%d", type, periodId.c_str(), adapIdx, isVssPeriod, drmHelper ? drmHelper->getUuid().c_str() : "null", mExitLoop.load());
	if(!mExitLoop)
	{
		LicensePreFetchObjectPtr fetchObject = std::make_shared<LicensePreFetchObject>(drmHelper, periodId, adapIdx, type, isVssPeriod);
		if (fetchObject)
		{
			if(isVssPeriod)
			{
				std::lock_guard<std::mutex>lock(mQVssMutex);
				mVssFetchQueue.push_back(std::move(fetchObject));
				AAMPLOG_MIL("[LicPreFetch] Queued VSS object, mVssFetchQueue size:%zu", mVssFetchQueue.size());
				if (!mVssPreFetchThread.joinable())
				{
					AAMPLOG_MIL("Starting mVssPreFetchThread");
					mVssPreFetchThread = std::thread(&AampLicensePreFetcher::VssPreFetchThread, this);
				}
				else
				{
					AAMPLOG_WARN("Notify mVssPreFetchThread");
					mQVssCond.notify_one();
				}
			}

			else
			{
				std::lock_guard<std::mutex>lock(mQMutex);

				// Don't add the key if it is already on the queue
				if (KeyIsQueued(fetchObject))
				{
					AAMPLOG_MIL("Key already queued for %d, object id:%d dropped", fetchObject->mType, fetchObject->mId);
					return true;
				}

				mFetchQueue.push_back(std::move(fetchObject));
				AAMPLOG_MIL("[LicPreFetch] Queued object, mFetchQueue size:%zu", mFetchQueue.size());
				if (!mPreFetchThread.joinable())
				{
					AAMPLOG_MIL("Starting mPreFetchThread");
					mPreFetchThread = std::thread(&AampLicensePreFetcher::PreFetchThread, this);
				}
				else
				{
					AAMPLOG_MIL("Notify mPreFetchThread");
					mQCond.notify_one();
				}
			}

		}
		else
		{
			AAMPLOG_ERR("[LicPreFetch] Failed to allocate LicensePreFetchObject for type:%d period:%s", type, periodId.c_str());
		}
	}
	else
	{
		AAMPLOG_WARN("Skipping creation of prefetcher threads as the license prefetcher object has already been de-initialized/freed");	
	}
	return ret;
}

/**
 * @brief De-initialize/free resources
 * 
 * @return true if success
 * @return false if failed
 */
bool AampLicensePreFetcher::Term()
{
	bool ret = true;
	AAMPLOG_MIL("[LicPreFetch] Term called");
	/** Clear the queue **/
	{
		std::lock_guard<std::mutex>lock(mQMutex);
		AAMPLOG_MIL("[LicPreFetch] Clearing mFetchQueue, size:%zu", mFetchQueue.size());

		while (!mFetchQueue.empty())
		{
			mFetchQueue.pop_front();
		}
	}
	{
		std::lock_guard<std::mutex>lock(mQVssMutex);
		AAMPLOG_MIL("[LicPreFetch] Clearing mVssFetchQueue, size:%zu", mVssFetchQueue.size());

		while (!mVssFetchQueue.empty())
		{
			mVssFetchQueue.pop_front();
		}
	}

	mTrackStatus.fill(false);
	SetLicenseFetcher(nullptr);
	AAMPLOG_MIL("[LicPreFetch] Term done, queues cleared, mTrackStatus reset and license fetcher detached");
	return ret;
}

/**
 * @brief Set license fetcher instance in a thread-safe manner.
 *
 * Sets the license fetcher instance used by the prefetcher.
 * This method is thread-safe and uses a mutex (mLicenseAcquisitionMutex)
 * to protect mFetchInstance from concurrent access. This helps to Serialize
 * license acquisition with start/stop and failure flows.
 *
 * @param fetcherInstance Pointer to the AampLicenseFetcher instance to set.
 * @note Thread-safe: uses mutual exclusion to protect mFetchInstance.
 */
void AampLicensePreFetcher::SetLicenseFetcher(AampLicenseFetcher *fetcherInstance)
{
	AAMPLOG_INFO("[LicPreFetch] SetLicenseFetcher waiting for mLicenseAcquisitionMutex, old:%p new:%p", static_cast<void*>(mFetchInstance), static_cast<void*>(fetcherInstance));
	std::lock_guard<std::mutex> lock(mLicenseAcquisitionMutex);
	mFetchInstance = fetcherInstance;
	AAMPLOG_MIL("License fetcher set to %p", static_cast<void*>(mFetchInstance));
}

/**
 * @brief Thread for processing content protection queued using QueueContentProtection
 * Thread will be joined when Term is called
 * 
 */
void AampLicensePreFetcher::PreFetchThread()
{
	aamp_setThreadName("aampfMP4DRM");
	AAMPLOG_MIL("[LicPreFetch] PreFetchThread started");
	std::unique_lock<std::mutex>queueLock(mQMutex);
	while (!mExitLoop)
	{
		if (mFetchQueue.empty())
		{
			AAMPLOG_INFO("Waiting for new entry in mFetchQueue");
			mQCond.wait(queueLock, [this] { return mExitLoop || !mFetchQueue.empty(); });
		}
		else
		{
			LicensePreFetchObjectPtr obj = mFetchQueue.front(); // Leave the request on the queue
			queueLock.unlock();
			AAMPLOG_MIL("[LicPreFetch] PreFetchThread picked object id:%d type:%d period:%s adaptationIdx:%u", obj->mId, obj->mType, obj->mPeriodId.c_str(), obj->mAdaptationIdx);

			if (!mExitLoop)
			{
				bool skip = false;
				bool keyStatus = false;
				std::vector<uint8_t> keyIdArray;
				obj->mHelper->getKey(keyIdArray);
				AAMPLOG_INFO("[LicPreFetch] Object id:%d keyId:%s keyIdEmpty:%d", obj->mId, AampLogManager::getHexDebugStr(keyIdArray).c_str(), keyIdArray.empty());
				if (!keyIdArray.empty() && mPrivAAMP->mDRMLicenseManager->mDrmSessionManager->IsKeyIdProcessed(keyIdArray, keyStatus))
				{
					AAMPLOG_WARN("Key already processed [status:%s] for type:%d adaptationSetIdx:%u !", keyStatus ? "SUCCESS" : "FAIL", obj->mType, obj->mAdaptationIdx);
					if(!keyStatus)
					{
						AAMPLOG_INFO("Notifying DRM failure for type:%d adaptationSetIdx:%u", obj->mType, obj->mAdaptationIdx);
						bool isSecClientError = isSecFeatureEnabled();
						DrmMetaDataEventPtr e = std::make_shared<DrmMetaDataEvent>(AAMP_TUNE_FAILURE_UNKNOWN, "", 0, 0, isSecClientError, mPrivAAMP->GetSessionId());
						/*
						 * mLicenseAcquisitionMutex shouldn't be locked before calling
						 * NotifyDrmFailure, as it is used inside NotifyDrmFailure.
						 */
						NotifyDrmFailure(obj, std::move(e));
					}
					mPrivAAMP->setCurrentDrm(obj->mHelper);
					skip = true;
				}
				if (!skip)
				{
					AAMPLOG_INFO("[LicPreFetch] Key not yet processed, proceeding to create DRM session for object id:%d", obj->mId);
					if (!keyIdArray.empty())
					{
						std::string keyIdDebugStr = AampLogManager::getHexDebugStr(keyIdArray);
						AAMPLOG_INFO("Creating DRM session for type:%d period ID:%s and Key ID:%s", obj->mType, obj->mPeriodId.c_str(), keyIdDebugStr.c_str());
					}
					if (CreateDRMSession(obj))
					{
						keyStatus = true;
					}
				}
				AAMPLOG_MIL("[LicPreFetch] Object id:%d type:%d result keyStatus:%d skip:%d", obj->mId, obj->mType, keyStatus, skip);
				if (keyStatus)
				{
					AAMPLOG_INFO("Updating mTrackStatus to true for type:%d", obj->mType);
					try
					{
						mTrackStatus.at(obj->mType) = true;
					}
					catch (std::out_of_range const& exc)
					{
						AAMPLOG_ERR("Unable to set the mTrackStatus for type:%d, caught exception: %s", obj->mType, exc.what());
					}
				}
			}
			else
			{
				AAMPLOG_WARN("[LicPreFetch] PreFetchThread exit requested, dropping object id:%d type:%d without processing", obj->mId, obj->mType);
			}
			queueLock.lock();

			// Remove the request now we have processed it
			if (!mFetchQueue.empty())
			{
				mFetchQueue.pop_front();
				AAMPLOG_INFO("[LicPreFetch] Removed processed object id:%d, mFetchQueue size:%zu", obj->mId, mFetchQueue.size());
			}
		}
	}
	AAMPLOG_MIL("[LicPreFetch] PreFetchThread exiting");
}

/**
 * @brief Thread for processing VSS content protection queued using QueueContentProtection
 * Thread will be joined when Term is called
 *
 */
void AampLicensePreFetcher::VssPreFetchThread()
{
	aamp_setThreadName("aampfMP4VSSDRM");
	AAMPLOG_MIL("[LicPreFetch] VssPreFetchThread started");
	std::unique_lock<std::mutex>queueLock(mQVssMutex);
	while (!mExitLoop)
	{
		if (mVssFetchQueue.empty())
		{
			AAMPLOG_INFO("Waiting for new entry in mVssFetchQueue");
			mQVssCond.wait(queueLock, [this] { return mExitLoop || !mVssFetchQueue.empty(); });
		}
		else
		{
			LicensePreFetchObjectPtr obj = mVssFetchQueue.front();
			mVssFetchQueue.pop_front();
			queueLock.unlock();
			AAMPLOG_MIL("[LicPreFetch] VssPreFetchThread picked object id:%d type:%d period:%s adaptationIdx:%u", obj->mId, obj->mType, obj->mPeriodId.c_str(), obj->mAdaptationIdx);

			if (!mExitLoop)
			{
				bool skip = false;
				bool keyStatus = false;
				std::vector<uint8_t> keyIdArray;
				obj->mHelper->getKey(keyIdArray);
				AAMPLOG_INFO("[LicPreFetch] Object id:%d keyId:%s keyIdEmpty:%d", obj->mId, AampLogManager::getHexDebugStr(keyIdArray).c_str(), keyIdArray.empty());
				if (!keyIdArray.empty() && mPrivAAMP->mDRMLicenseManager->mDrmSessionManager->IsKeyIdProcessed(keyIdArray, keyStatus))
				{
					AAMPLOG_WARN("Key already processed [status:%s] for type:%d adaptationSetIdx:%u !", keyStatus ? "SUCCESS" : "FAIL", obj->mType, obj->mAdaptationIdx);
					skip = true;
				}
				if (!skip)
				{
					AAMPLOG_INFO("[LicPreFetch] VSS key not yet processed, mCommonKeyDuration:%d for object id:%d", mCommonKeyDuration, obj->mId);
                                        if (mCommonKeyDuration > 0)
                                        {
                                                int deferTime = aamp_GetDeferTimeMs(static_cast<long>(mCommonKeyDuration));
                                                // Going to sleep for deferred key process
                                                AAMPLOG_MIL("[LicPreFetch] Deferring VSS key processing by %dms (mCommonKeyDuration:%d)", deferTime, mCommonKeyDuration);
                                                mPrivAAMP->interruptibleMsSleep(deferTime);
                                                AAMPLOG_MIL("[LicPreFetch] Sleep over for deferred time:%d", deferTime);
                                        }
					if(mExitLoop)
					{
						AAMPLOG_WARN("[LicPreFetch] Exit requested, skipping DRM session creation for VSS object id:%d", obj->mId);
					}
					if(!mExitLoop)
					{
						if (!keyIdArray.empty())
						{
							std::string keyIdDebugStr = AampLogManager::getHexDebugStr(keyIdArray);
							AAMPLOG_INFO("Creating DRM session for type:%d period ID:%s and Key ID:%s", obj->mType, obj->mPeriodId.c_str(), keyIdDebugStr.c_str());
						}
						if (CreateDRMSession(obj))
						{
							keyStatus = true;
						}
					}
				}
				AAMPLOG_MIL("[LicPreFetch] Object id:%d type:%d result keyStatus:%d skip:%d", obj->mId, obj->mType, keyStatus, skip);
				if (keyStatus)
				{
					AAMPLOG_INFO("Updating mTrackStatus to true for type:%d", obj->mType);
					try
					{
						mTrackStatus.at(obj->mType) = true;
					}
					catch (std::out_of_range const& exc)
					{
						AAMPLOG_ERR("Unable to set the mTrackStatus for type:%d, caught exception: %s", obj->mType, exc.what());
					}
				}
			}
			else
			{
				AAMPLOG_WARN("[LicPreFetch] VssPreFetchThread exit requested, dropping object id:%d type:%d without processing", obj->mId, obj->mType);
			}
			queueLock.lock();
		}
	}
	AAMPLOG_MIL("[LicPreFetch] VssPreFetchThread exiting");
}
/**
 * @brief To notify DRM failure to player after proper checks
 * 
 * @param fetchObj object for which session creation failed
 * @param event drm metadata event object with failure details
 */
void AampLicensePreFetcher::NotifyDrmFailure(LicensePreFetchObjectPtr fetchObj, DrmMetaDataEventPtr event)
{
	AAMPTuneFailure failure = event->getFailure();
	bool isRetryEnabled = false;
	bool selfAbort = (failure == AAMP_TUNE_DRM_SELF_ABORT);
	bool skipErrorEvent = false;
	AAMPLOG_MIL("[LicPreFetch] NotifyDrmFailure failure:%d responseCode:%d selfAbort:%d mSendErrorOnFailure:%d fetchObj:%d type:%d", (int)failure, event->getResponseCode(), selfAbort, mSendErrorOnFailure, fetchObj ? fetchObj->mId : -1, fetchObj ? (int)fetchObj->mType : -1);
#ifdef USE_PREINIT_DECODING
	AAMPLOG_INFO("[LicPreFetch] USE_PREINIT_DECODING enabled, mIsFakeTune:%d", mPrivAAMP->mIsFakeTune);
	if(mPrivAAMP->mIsFakeTune)
	{
		AAMPLOG_WARN("[LicPreFetch] Fake tune in progress, DRM failure event will be skipped");
		skipErrorEvent = true;
	}
#else
	AAMPLOG_INFO("[LicPreFetch] USE_PREINIT_DECODING not enabled, no fake tune handling in NotifyDrmFailure");
#endif
	// Skip these additional checks and send error event if mSendErrorOnFailure is set
	// For a non-intra asset playback with KR, if a future license fails, we should send the error
	// and skip below check. Maybe introduce a better data structure for mTrackStatus based on periodId
	if (fetchObj && !mSendErrorOnFailure)
	{
		try
		{
			// Check if license already acquired for this track, then skip the error event broadcast
			AAMPLOG_INFO("[LicPreFetch] Checking mTrackStatus for type:%d status:%d", fetchObj->mType, (int)mTrackStatus.at(fetchObj->mType));
			if (mTrackStatus.at(fetchObj->mType) == true)
			{
				skipErrorEvent = true;
				AAMPLOG_WARN("Skipping DRM failure event, since license already acquired for this track type:%d", fetchObj->mType);
			}
		}
		catch (std::out_of_range const& exc)
		{
			AAMPLOG_ERR("Unable to check the mTrackStatus for type:%d, caught exception: %s", fetchObj->mType, exc.what());
		}

		if (!skipErrorEvent)
		{
			// Check if the mFetchQueue has a request for this track type queued
			// TODO: Check for race conditions between license acquisition and adding into fetch queue
			std::lock_guard<std::mutex>lock(mQMutex);

			for (auto obj : mFetchQueue)
			{
				if (obj == fetchObj)
				{
					// The current key is still on the queue so if pointers match we will ignore this check
					// (this shoud be the first item in the queue)
					continue;
				}

				if (obj->mType == fetchObj->mType)
				{
					skipErrorEvent = true;
					AAMPLOG_WARN("Skipping DRM failure event, since a pending request exists for this track type:%d", fetchObj->mType);
					break;
				}
			}
		}
	}

	{
		std::lock_guard<std::mutex> lock(mLicenseAcquisitionMutex);
		AAMPLOG_INFO("[LicPreFetch] NotifyDrmFailure holding mLicenseAcquisitionMutex, skipErrorEvent:%d mFetchInstance:%p", skipErrorEvent, static_cast<void*>(mFetchInstance));
		if (skipErrorEvent && mFetchInstance)
		{
			AAMPLOG_MIL("[LicPreFetch] Error event skipped, updating failed DRM status on license fetcher for object:%d", fetchObj ? fetchObj->mId : -1);
			mFetchInstance->UpdateFailedDRMStatus(fetchObj.get());
		}
		else
		{
			if (skipErrorEvent)
			{
				AAMPLOG_WARN("[LicPreFetch] Error event skipped but no license fetcher set, failure not reported");
			}
			if (selfAbort)
			{
				AAMPLOG_MIL("[LicPreFetch] DRM self abort, no error event sent");
			}
			if (!selfAbort)
			{
				//Set the isRetryEnabled flag to true if the failure is due to
				//SEC_CLIENT_RESULT_HTTP_RESULT_FAILURE_TIMEOUT (error -7). This
				//error is caused by a network failure, so the tune may succeed
				//on a retry attempt.
				//For other DRM failures, the flag should be set to false.
				isRetryEnabled = ((failure == AAMP_TUNE_LICENCE_REQUEST_FAILED) && (event->getResponseCode() == SECCLIENT_RESULT_HTTP_FAILURE_TIMEOUT))
				      || ((failure != AAMP_TUNE_AUTHORIZATION_FAILURE)
				      && (failure != AAMP_TUNE_LICENCE_REQUEST_FAILED)
				      && (failure != AAMP_TUNE_LICENCE_TIMEOUT)
				      && (failure != AAMP_TUNE_DEVICE_NOT_PROVISIONED)
				      && (failure != AAMP_TUNE_HDCP_COMPLIANCE_ERROR));
				AAMPLOG_WARN("Drm failure:%d response: %d isRetryEnabled:%d ",(int)failure,event->getResponseCode(),isRetryEnabled);
				mPrivAAMP->SendDRMMetaData(event);	//Send Header response first for failure case.
				AAMPLOG_ERR("Failed DRM Session sending error event");
				mPrivAAMP->SendDrmErrorEvent(event, std::move(isRetryEnabled));
				mPrivAAMP->profiler.SetDrmErrorCode((int)failure);
				mPrivAAMP->profiler.ProfileError(PROFILE_BUCKET_LA_TOTAL, (int)failure);
			}
		}
	}
}

/**
 * @brief Creates a DRM session for a content protection
 * 
 * @param fetchObj LicensePreFetchObject shared_ptr
 * @return true if successfully created DRM session
 * @return false if failed
 */
bool AampLicensePreFetcher::CreateDRMSession(LicensePreFetchObjectPtr fetchObj)
{
	bool ret = false;
	const int objId = fetchObj->mId;
	bool isSecClientError = isSecFeatureEnabled();
	AAMPLOG_MIL("[LicPreFetch] CreateDRMSession begin, object id:%d type:%d period:%s adaptationIdx:%u isVss:%d isSecClientError:%d", objId, fetchObj->mType, fetchObj->mPeriodId.c_str(), fetchObj->mAdaptationIdx, fetchObj->mIsVssPeriod, isSecClientError);
	DrmMetaDataEventPtr e = std::make_shared<DrmMetaDataEvent>(AAMP_TUNE_FAILURE_UNKNOWN, "", 0, 0, isSecClientError, mPrivAAMP->GetSessionId());

	if (mPrivAAMP == nullptr)
	{
		AAMPLOG_ERR("no PrivateInstanceAAMP instance available");
		return ret;
	}
	if (fetchObj->mHelper == nullptr)
	{
		AAMPLOG_ERR("Failed DRM Session Creation,  no helper");
		/*
		 * Do not hold mLicenseAcquisitionMutex when calling NotifyDrmFailure,
		 * as NotifyDrmFailure acquires mLicenseAcquisitionMutex internally.
		 */
		NotifyDrmFailure(std::move(fetchObj), std::move(e));
		AAMPLOG_MIL("[LicPreFetch] CreateDRMSession end (no helper), object id:%d result:%d", objId, ret);
		return ret;
	}
	AampDRMLicenseManager* licenseManger= mPrivAAMP->mDRMLicenseManager;

	if (licenseManger == nullptr)
	{
		AAMPLOG_ERR("no mPrivAAMP->mDrmSessionManager available");
		AAMPLOG_MIL("[LicPreFetch] CreateDRMSession end (no license manager), object id:%d result:%d", objId, ret);
		return ret;
	}
	mPrivAAMP->setCurrentDrm(fetchObj->mHelper);
	AAMPLOG_INFO("[LicPreFetch] Current DRM set to helper uuid:%s", fetchObj->mHelper->getUuid().c_str());

	DrmSession *drmSession = nullptr;
	mPrivAAMP->profiler.ProfileBegin(PROFILE_BUCKET_LA_TOTAL);
	{
		/*
		 * Serialize license acquisition with start/stop and failure flows.
		 * mLicenseAcquisitionMutex is shared between the start/stop flows and
		 * NotifyDrmFailure(). NotifyDrmFailure() may be invoked from the same
		 * CreateDRMSession flow (directly or indirectly), so calling it while
		 * already holding this mutex would risk deadlock if it also attempted
		 * to acquire mLicenseAcquisitionMutex.
		 *
		 * By reusing this mutex around the licenseManager->createDrmSession()
		 * call, we ensure that, in fast channel-change scenarios, the stop
		 * path can wait until the PrefetchThread has exited the license
		 * acquisition flow before moving to a new tune. This helps to avoid
		 * applying a previous tune's license to the CDM after a new tune
		 * has been initiated.
		 *
		 * Callers of NotifyDrmFailure() and any code that may call it
		 * from within the CreateDRMSession flow must not already hold
		 * mLicenseAcquisitionMutex. That contract is required to avoid recursive
		 * locking and potential deadlocks.
		 */
		AAMPLOG_INFO("[LicPreFetch] CreateDRMSession waiting for mLicenseAcquisitionMutex, object id:%d", fetchObj->mId);
		std::lock_guard<std::mutex> lock(mLicenseAcquisitionMutex);
		AAMPLOG_MIL("[LicPreFetch] Calling createDrmSession, object id:%d type:%d uuid:%s", fetchObj->mId, fetchObj->mType, fetchObj->mHelper->getUuid().c_str());
		drmSession = licenseManger->createDrmSession( fetchObj->mHelper, mPrivAAMP, e, (int)fetchObj->mType);
		AAMPLOG_MIL("[LicPreFetch] createDrmSession returned session:%p for object id:%d accessStatusValue:%d", static_cast<void*>(drmSession), fetchObj->mId, e->getAccessStatusValue());
	}


	//set failures here 
	if(drmSession == nullptr)
	{
		AAMPLOG_ERR("Failed DRM Session Creation for systemId = %s", fetchObj->mHelper->getUuid().c_str());
		/*
		 * mLicenseAcquisitionMutex shouldn't be locked before calling
		 * NotifyDrmFailure, as it is used inside NotifyDrmFailure.
		 */
		NotifyDrmFailure(std::move(fetchObj), std::move(e));
	}
	else
	{
		ret = true;
		AAMPLOG_MIL("[LicPreFetch] DRM session created for object id:%d, accessStatusValue:%d", fetchObj->mId, e->getAccessStatusValue());
		if(e->getAccessStatusValue() != 3)
		{
			AAMPLOG_INFO("Sending DRMMetaData");
			mPrivAAMP->SendDRMMetaData(std::move(e));
		}
		else
		{
			AAMPLOG_INFO("[LicPreFetch] accessStatusValue is 3, DRMMetaData not sent");
		}
	}
	mPrivAAMP->profiler.ProfileEnd(PROFILE_BUCKET_LA_TOTAL);
	if(mPrivAAMP->mIsFakeTune)
	{
		AAMPLOG_MIL("[LicPreFetch] Fake tune: setting state COMPLETE and sending EOS");
		mPrivAAMP->SetState(eSTATE_COMPLETE);
		mPrivAAMP->SendEvent(std::make_shared<AAMPEventObject>(AAMP_EVENT_EOS, mPrivAAMP->GetSessionId()));
	}
	AAMPLOG_MIL("[LicPreFetch] CreateDRMSession end, object id:%d result:%d", objId, ret);
	return ret;
}
