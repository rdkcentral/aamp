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
#include "AampDRMSessionManager.h"
#include "AampUtils.h"	// for aamp_GetDeferTimeMs
#include "priv_aamp.h"

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
		mPreFetchThreadStarted(false),
		mExitLoop(false),
		mCommonKeyDuration(0),
		mTrackStatus(),
		mSendErrorOnFailure(true),
		mPrivAAMP(aamp),
		mFetchInstance(nullptr),
		mVssPreFetchThread(),
		mVssFetchQueue(),
		mQVssMutex(),
		mQVssCond(),
		mVssPreFetchThreadStarted(false)
{
	mTrackStatus.fill(false);
}

/**
 * @brief Destroy the Aamp License Pre Fetcher object
 * 
 */
AampLicensePreFetcher::~AampLicensePreFetcher()
{
	Term();
	{
		std::lock_guard<std::mutex>lock(mQMutex);
		mExitLoop = true;
	}
	if (mPreFetchThreadStarted)
	{
		mQCond.notify_one();
		AAMPLOG_WARN("Joining mPreFetchThread");
		mPreFetchThread.join();
		mPreFetchThreadStarted = false;
	}
	if (mVssPreFetchThreadStarted)
	{
		mQVssCond.notify_one();
		AAMPLOG_WARN("Joining mVssFetchThread");
		mVssPreFetchThread.join();
		mVssPreFetchThreadStarted = false;
	}
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
	if (mPreFetchThreadStarted || mVssPreFetchThreadStarted)
	{
		AAMPLOG_WARN("PreFetch thread is already started when calling Init!!");
	}
	mTrackStatus.fill(false);
	mExitLoop = false;
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
	for (auto queuedObject : mFetchQueue)
	{
		queuedObject->mHelper->getKey(queuedKeyIdArray);
		if ((queuedObject->mType == fetchObject->mType) &&
			(queuedKeyIdArray == fetchKeyIdArray))
		{
			return true;
		}
	}
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
	if(!mExitLoop)
	{
		LicensePreFetchObjectPtr fetchObject = std::make_shared<LicensePreFetchObject>(drmHelper, periodId, adapIdx, type, isVssPeriod);
		if (fetchObject)
		{
			if(isVssPeriod)
			{
				std::lock_guard<std::mutex>lock(mQVssMutex);
				mVssFetchQueue.push_back(fetchObject);
				if (!mVssPreFetchThreadStarted)
				{
					AAMPLOG_WARN("Starting mVssPreFetchThread");
					mVssPreFetchThread = std::thread(&AampLicensePreFetcher::VssPreFetchThread, this);
					mVssPreFetchThreadStarted = true;
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
					AAMPLOG_INFO("Key already queued for %d", fetchObject->mType);
					return true;
				}

				mFetchQueue.push_back(fetchObject);
				if (!mPreFetchThreadStarted)
				{
					AAMPLOG_WARN("Starting mPreFetchThread");
					mPreFetchThread = std::thread(&AampLicensePreFetcher::PreFetchThread, this);
					mPreFetchThreadStarted = true;
				}
				else
				{
					AAMPLOG_WARN("Notify mPreFetchThread");
					mQCond.notify_one();
				}
			}

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
	/** Clear the queue **/
	{
		std::lock_guard<std::mutex>lock(mQMutex);

		while (!mFetchQueue.empty())
		{
			mFetchQueue.pop_front();
		}
		while (!mVssFetchQueue.empty())
		{
			mVssFetchQueue.pop_front();
		}
	}
	
	mTrackStatus.fill(false);
	mFetchInstance = nullptr;
	return ret;
}

/**
 * @brief Thread for processing content protection queued using QueueContentProtection
 * Thread will be joined when Term is called
 * 
 */
void AampLicensePreFetcher::PreFetchThread()
{
	if(aamp_pthread_setname(pthread_self(), "aampfMP4DRM"))
	{
		AAMPLOG_ERR("aamp_pthread_setname failed");
	}
	std::unique_lock<std::mutex>queueLock(mQMutex);
	while (!mExitLoop)
	{
		if (mFetchQueue.empty())
		{
			AAMPLOG_INFO("Waiting for new entry in mFetchQueue");
			mQCond.wait(queueLock);
		}
		else
		{
			LicensePreFetchObjectPtr obj = mFetchQueue.front(); // Leave the request on the queue
			queueLock.unlock();

			if (!mExitLoop)
			{
				bool skip = false;
				bool keyStatus = false;
				std::vector<uint8_t> keyIdArray;
				obj->mHelper->getKey(keyIdArray);
				if (!keyIdArray.empty() && mPrivAAMP->mDRMSessionManager->IsKeyIdProcessed(keyIdArray, keyStatus))
				{
					AAMPLOG_WARN("Key already processed [status:%s] for type:%d adaptationSetIdx:%u !", keyStatus ? "SUCCESS" : "FAIL", obj->mType, obj->mAdaptationIdx);
					mPrivAAMP->setCurrentDrm(obj->mHelper);
					skip = true;
				}
				if (!skip)
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
			queueLock.lock();

			// Remove the request now we have processed it
			if (!mFetchQueue.empty())
			{
				mFetchQueue.pop_front();
			}
		}
	}
}

/**
 * @brief Thread for processing VSS content protection queued using QueueContentProtection
 * Thread will be joined when Term is called
 *
 */
void AampLicensePreFetcher::VssPreFetchThread()
{
	if(aamp_pthread_setname(pthread_self(), "aampfMP4DRM"))
	{
		AAMPLOG_ERR("aamp_pthread_setname failed");
	}
	std::unique_lock<std::mutex>queueLock(mQVssMutex);
	while (!mExitLoop)
	{
		if (mVssFetchQueue.empty())
		{
			AAMPLOG_INFO("Waiting for new entry in mFetchQueue");
			mQVssCond.wait(queueLock);
		}
		else
		{
			LicensePreFetchObjectPtr obj = mVssFetchQueue.front();
			mVssFetchQueue.pop_front();
			queueLock.unlock();

			if (!mExitLoop)
			{
				bool skip = false;
				bool keyStatus = false;
				std::vector<uint8_t> keyIdArray;
				obj->mHelper->getKey(keyIdArray);
				if (!keyIdArray.empty() && mPrivAAMP->mDRMSessionManager->IsKeyIdProcessed(keyIdArray, keyStatus))
				{
					AAMPLOG_WARN("Key already processed [status:%s] for type:%d adaptationSetIdx:%u !", keyStatus ? "SUCCESS" : "FAIL", obj->mType, obj->mAdaptationIdx);
					skip = true;
				}
				if (!skip)
				{
                                        if (mCommonKeyDuration > 0)
                                        {
                                                int deferTime = aamp_GetDeferTimeMs(static_cast<long>(mCommonKeyDuration));
                                                // Going to sleep for deferred key process
                                                mPrivAAMP->interruptibleMsSleep(deferTime);
                                                AAMPLOG_TRACE("Sleep over for deferred time:%d", deferTime);
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
			queueLock.lock();
		}
	}
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
	// Skip these additional checks and send error event if mSendErrorOnFailure is set
	// For a non-intra asset playback with KR, if a future license fails, we should send the error
	// and skip below check. Maybe introduce a better data structure for mTrackStatus based on periodId
	if (fetchObj && !mSendErrorOnFailure)
	{
		try
		{
			// Check if license already acquired for this track, then skip the error event broadcast
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

	if (skipErrorEvent && mFetchInstance)
	{
		mFetchInstance->UpdateFailedDRMStatus(fetchObj.get());
	}
	else
	{
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
			mPrivAAMP->SendDrmErrorEvent(event, isRetryEnabled);
			mPrivAAMP->profiler.SetDrmErrorCode((int)failure);
			mPrivAAMP->profiler.ProfileError(PROFILE_BUCKET_LA_TOTAL, (int)failure);
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
#if defined(USE_SECCLIENT) || defined(USE_SECMANAGER)
	bool isSecClientError = true;
#else
	bool isSecClientError = false;
#endif
	DrmMetaDataEventPtr e = std::make_shared<DrmMetaDataEvent>(AAMP_TUNE_FAILURE_UNKNOWN, "", 0, 0, isSecClientError, mPrivAAMP->GetSessionId());

	if (mPrivAAMP == nullptr)
	{
		AAMPLOG_ERR("no PrivateInstanceAAMP instance available");
		return ret;
	}
	if (fetchObj->mHelper == nullptr)
	{
		AAMPLOG_ERR("Failed DRM Session Creation,  no helper");
		NotifyDrmFailure(fetchObj, e);
		return ret;
	}
	AampDRMSessionManager* sessionManger = mPrivAAMP->mDRMSessionManager;

	if (sessionManger == nullptr)
	{
		AAMPLOG_ERR("no mPrivAAMP->mDrmSessionManager available");
		return ret;
	}
	mPrivAAMP->setCurrentDrm(fetchObj->mHelper);

	mPrivAAMP->profiler.ProfileBegin(PROFILE_BUCKET_LA_TOTAL);
	DrmSession *drmSession = sessionManger->createDrmSession(fetchObj->mHelper, e, mPrivAAMP, fetchObj->mType);

	if(NULL == drmSession)
	{
		AAMPLOG_ERR("Failed DRM Session Creation for systemId = %s", fetchObj->mHelper->getUuid().c_str());
		NotifyDrmFailure(fetchObj, e);
	}
	else
	{
		ret = true;
		if(e->getAccessStatusValue() != 3)
		{
			AAMPLOG_INFO("Sending DRMMetaData");
			mPrivAAMP->SendDRMMetaData(e);
		}
	}
	mPrivAAMP->profiler.ProfileEnd(PROFILE_BUCKET_LA_TOTAL);
	if(mPrivAAMP->mIsFakeTune)
	{
		mPrivAAMP->SetState(eSTATE_COMPLETE);
		mPrivAAMP->SendEvent(std::make_shared<AAMPEventObject>(AAMP_EVENT_EOS, mPrivAAMP->GetSessionId()));
	}
	return ret;
}
