/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2024 RDK Management
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
 * @file AampTSBSessionManager.cpp
 * @brief AampTSBSessionManager for AAMP
 */

#include "AampTSBSessionManager.h"
#include "AampConfig.h"
#include "StreamAbstractionAAMP.h"
#include "AampCacheHandler.h"
#include "isobmffhelper.h"
#include "AampTsbAdPlacementMetaData.h"
#include "AampTsbAdReservationMetaData.h"
#include <iostream>
#include <cmath>
#include <utility>

#define INIT_CHECK_RETURN_VAL(val) \
	if(!mInitialized_){ \
		AAMPLOG_ERR("Session Manager not initialized!"); \
		return val; \
	}

#define INIT_CHECK_RETURN_VOID() \
	if(!mInitialized_){ \
		AAMPLOG_ERR("Session Manager not initialized!"); \
		return; \
	}

/**
 * @brief AampTSBSessionManager Constructor
 *
 * @return None
 */
AampTSBSessionManager::AampTSBSessionManager(PrivateInstanceAAMP *aamp)
	: mInitialized_(false), mStopThread_(false), mAamp(aamp), mTSBStore(nullptr), mActiveTuneType(eTUNETYPE_NEW_NORMAL), mLastVideoPos(AAMP_PAUSE_POSITION_INVALID_POSITION)
		, mCulledDuration(0.0)
		, mStoreEndPosition(0.0)
		, mLiveEndPosition(0.0)
		, mTsbMaxDiskStorage(0)
		, mTsbMinFreePercentage(0)
		, mIsoBmffHelper(std::make_shared<IsoBmffHelper>())
		, mTsbLength(0)
		, mCurrentWritePosition(0)
		, mLastAdMetaDataProcessed(nullptr)  // Initialize to nullptr
{
}

/**
 * @brief AampTSBSessionManager Destructor
 *
 * @return None
 */
AampTSBSessionManager::~AampTSBSessionManager()
{
	Flush();
}

/**
 * @brief AampTSBSessionManager Init function
 *
 * @return None
 */
void AampTSBSessionManager::Init()
{
	if (!mInitialized_)
	{
		TSB::Store::Config config;

		// concatenate the location with session id
		config.location = mTsbLocation;
		config.minFreePercentage = mTsbMinFreePercentage;
		config.maxCapacity =  mTsbMaxDiskStorage;
		TSB::LogLevel level = static_cast<TSB::LogLevel>(ConvertTsbLogLevel(mAamp->mConfig->GetConfigValue(eAAMPConfig_TsbLogLevel)));
		AAMPLOG_INFO("[TSB Store] Initiating with config values { logLevel:%d maxCapacity : %d minFreePercentage : %d location : %s }",  static_cast<int>(level), config.maxCapacity, config.minFreePercentage, config.location.c_str());

		// All Configuration to TSBHandler to be set before calling Init

		mTSBStore = mAamp->GetTSBStore(config, AampLogManager::aampLogger, level);
		if (mTSBStore)
		{
			// Initialize datamanager for respective mediatype
			InitializeDataManagers();
			// Initialize metadata manager
			InitializeMetaDataManager();
			// Initialize TSB readers
			InitializeTsbReaders();
			mStopThread_.store(false);
			// Start monitoring the write queue in a separate thread
			mWriteThread = std::thread(&AampTSBSessionManager::ProcessWriteQueue, this);
			mInitialized_ = true;
		}
	}
}

/**
 * @brief Initialize Data Managers for different media type
 *
 * @return None
 */
void AampTSBSessionManager::InitializeDataManagers()
{
	for (int i = 0; i < AAMP_TRACK_COUNT; i++)
	{
		AampMediaType mediaType = static_cast<AampMediaType>(i);
		mDataManagers[mediaType] = {std::make_shared<AampTsbDataManager>(), 0.0};
	}
}

/**
 * @brief Initialize the metadata manager and register metadata types
 */
void AampTSBSessionManager::InitializeMetaDataManager()
{
	// Initialize the metadata manager
	mMetaDataManager.Initialize();

	// Register AD_METADATA_TYPE as transient
	if (mMetaDataManager.RegisterMetaDataType(AampTsbMetaData::Type::AD_METADATA_TYPE, true))
	{
		AAMPLOG_INFO("Successfully registered AD_METADATA_TYPE as transient");
	}
	else
	{
		AAMPLOG_ERR("Failed to register AD_METADATA_TYPE");
	}
}

/**
 * @brief Initialize TSB readers for different media type
 *
 * @return None
 */
void AampTSBSessionManager::InitializeTsbReaders()
{
	if (mTsbReaders.empty())
	{
		// Initialize readers if they are empty for all tracks
		for (int i = 0; i < AAMP_TRACK_COUNT; i++)
		{
			if (nullptr != GetTsbDataManager((AampMediaType)i).get())
			{
				std::shared_ptr<AampTsbDataManager> dataMgr = GetTsbDataManager((AampMediaType)i);
				mTsbReaders.emplace((AampMediaType)i, std::make_shared<AampTsbReader>(mAamp, dataMgr, (AampMediaType)i, mTsbSessionId));
			}
			else
			{
				AAMPLOG_ERR("Failed to find a dataManager for mediatype: %d", i);
			}
		}
	}
}

/**
 * @brief Reads from the TSB library based on the initialization fragment data.
 *
 * @param[in] initfragdata Init fragment data
 *
 * @return A shared pointer to the cached fragment if read successfully, otherwise a null pointer.
 */
std::shared_ptr<CachedFragment> AampTSBSessionManager::Read(TsbInitDataPtr initfragdata)
{
	INIT_CHECK_RETURN_VAL(nullptr);

	CachedFragmentPtr cachedFragment = std::make_shared<CachedFragment>();
	std::string url = initfragdata->GetUrl();
	std::string effectiveUrl;
	bool readFromAampCache = mAamp->getAampCacheHandler()->RetrieveFromInitFragmentCache(url, &cachedFragment->fragment, effectiveUrl);
	cachedFragment->type = initfragdata->GetMediaType();
	cachedFragment->cacheFragStreamInfo = initfragdata->GetCacheFragStreamInfo();
	cachedFragment->profileIndex = initfragdata->GetProfileIndex();
	cachedFragment->initFragment = true;
	if (!readFromAampCache)
	{
		// Read from TSBLibrary
		std::string uniqueUrl = ToUniqueUrl(url,initfragdata->GetAbsolutePosition().inSeconds());
		std::size_t len = mTSBStore->GetSize(uniqueUrl);
		if (len > 0)
		{
			cachedFragment->fragment.ReserveBytes(len);
			UnlockReadMutex();
			TSB::Status status = mTSBStore->Read(uniqueUrl, cachedFragment->fragment.GetPtr(), len);
			cachedFragment->fragment.SetLen(len);
			LockReadMutex();
			if (status != TSB::Status::OK)
			{
				AAMPLOG_WARN("Failure in read from TSBLibrary");
				return nullptr;
			}
		}
		else
		{
			AAMPLOG_WARN("TSBLibrary returned zero length for URL: %s", uniqueUrl.c_str());
			return nullptr;
		}
	}

	return cachedFragment;
}

/**
 * @brief Reads from the TSB library based on fragment data.
 *
 * @param[in] fragmentdata TSB fragment data
 * @param[out] pts  of the fragment.
 * @return A shared pointer to the cached fragment if read successfully, otherwise a null pointer.
 */
std::shared_ptr<CachedFragment> AampTSBSessionManager::Read(TsbFragmentDataPtr fragment, double &pts)
{
	INIT_CHECK_RETURN_VAL(nullptr);

	std::string url {fragment->GetUrl()};
	std::string uniqueUrl = ToUniqueUrl(url,fragment->GetAbsolutePosition().inSeconds());
	TSB::Status status = TSB::Status::FAILED; // Initialize status as FAILED
	CachedFragmentPtr cachedFragment = std::make_shared<CachedFragment>();

	std::size_t len = mTSBStore->GetSize(uniqueUrl);
	if (len > 0)
	{
		// PTS restamping must be enabled to use AAMP Local TSB.
		// 'position' has the restamped PTS value, however, the PTS value in the ISO BMFF boxes
		// (baseMediaDecodeTime) will be restamped later, in the injector thread.
		cachedFragment->position = (fragment->GetPTS() + fragment->GetPTSOffsetSec()).inSeconds();
		cachedFragment->absPosition = fragment->GetAbsolutePosition().inSeconds();
		cachedFragment->duration = fragment->GetDuration().inSeconds();
		cachedFragment->discontinuity = fragment->IsDiscontinuous();
		cachedFragment->type = fragment->GetInitFragData()->GetMediaType();
		cachedFragment->PTSOffsetSec = fragment->GetPTSOffsetSec().inSeconds();
		cachedFragment->timeScale = fragment->GetTimeScale();
		cachedFragment->uri = std::move(url);
		pts = fragment->GetPTS().inSeconds();
		AAMPLOG_INFO("[%s] Read fragment from AAMP TSB: position (restamped PTS) %fs absPosition %fs pts %fs duration %fs discontinuity %d ptsOffset %fs timeScale %u url %s",
			GetMediaTypeName(cachedFragment->type), cachedFragment->position, cachedFragment->absPosition, pts, cachedFragment->duration,
			cachedFragment->discontinuity, cachedFragment->PTSOffsetSec, cachedFragment->timeScale, uniqueUrl.c_str());

		if (fragment->GetInitFragData())
		{
			cachedFragment->cacheFragStreamInfo = fragment->GetInitFragData()->GetCacheFragStreamInfo();
			cachedFragment->profileIndex = fragment->GetInitFragData()->GetProfileIndex();
		}
		else
		{
			// Handle the case where GetInitFragData returns nullptr
			AAMPLOG_WARN("Fragment's InitFragData is nullptr.");
			return nullptr;
		}

		cachedFragment->fragment.ReserveBytes(len);
		UnlockReadMutex();

		status = mTSBStore->Read(uniqueUrl, cachedFragment->fragment.GetPtr(), len);
		cachedFragment->fragment.SetLen(len);
		LockReadMutex();
		if (status == TSB::Status::OK)
		{
			return cachedFragment;
		}
		else
		{
			AAMPLOG_WARN("Read failure from TSBLibrary");
			return nullptr;
		}
	}
	else
	{
		AAMPLOG_WARN("TSBLibrary returned zero length for URL: %s", url.c_str());
		return nullptr;
	}
}

/**
 * @brief Write - function to enqueue data for writing to AAMP TSB
 *
 * @param[in] - URL
 * @param[in] - cachedFragment
 */
void AampTSBSessionManager::EnqueueWrite(std::string url, std::shared_ptr<CachedFragment> cachedFragment, std::string periodId)
{
	INIT_CHECK_RETURN_VOID();

	// Protect this section with the write queue mutex
	std::unique_lock<std::mutex> guard(mWriteQueueMutex);

	AampMediaType mediaType = ConvertMediaType(cachedFragment->type);
	if (!IsTrackStoredInTsb(mediaType))
	{
		AAMPLOG_WARN("Track not stored in TSB for media type: %d", mediaType);
	}
	else
	{
		// Read the PTS from the ISOBMFF boxes (baseMediaDecodeTime / timescale) before applying the PTS offset.
		// The PTS value will be restamped by the injector thread.
		// This function is called in the context of the fetcher thread before the fragment is added to the list to be injected, to avoid
		// any race conditions; so it cannot be moved to ProcessWriteQueue() or any other functions called from a different context.
		double pts = RecalculatePTS(static_cast<AampMediaType>(cachedFragment->type), cachedFragment->fragment.GetPtr(), cachedFragment->fragment.GetLen(), mAamp);

		// Get or create the datamanager for the mediatype
		std::shared_ptr<AampTsbDataManager> dataManager = GetTsbDataManager(mediaType);
		if (!dataManager)
		{
			AAMPLOG_WARN("Failed to get data manager for media type: %d", mediaType);
		}
		else
		{
			// TBD : Is there any possibility for TSBData add fragment failure ????
			TSBWriteData writeData = {url, cachedFragment, pts, std::move(periodId)};
			AAMPLOG_TRACE("Enqueueing Write Data discontinuity %d for URL: %s",cachedFragment->discontinuity, url.c_str());

			mCurrentWritePosition = cachedFragment->absPosition;
			// TODO :Need to add the same data on Addfragment and AddInitfragment of AampTsbDataManager
			mWriteQueue.push(writeData);
			// Notify the monitoring thread that there is data in the queue
			guard.unlock();
			mWriteThreadCV.notify_one();
		}
	}
}

bool AampTSBSessionManager::IsTrackStoredInTsb(AampMediaType mediatype)
{
	return mDataManagers.find(mediatype) != mDataManagers.end();
}

std::string AampTSBSessionManager::ToUniqueUrl(std::string url, double absPosition)
{
	int idx = static_cast<int>(absPosition) % 10000;
	return url + "." + std::to_string(idx);
}

TsbFragmentDataPtr AampTSBSessionManager::RemoveFragmentDeleteInit(AampMediaType mediatype)
{
	bool deleteInit = false;
	TsbFragmentDataPtr removedFragment = GetTsbDataManager(mediatype)->RemoveFragment(deleteInit);
	if (removedFragment && deleteInit)
	{
		TsbInitDataPtr removedFragmentInit = removedFragment->GetInitFragData();
		if (removedFragmentInit)
		{
			std::string initUrl = ToUniqueUrl(removedFragmentInit->GetUrl(),
										  	  removedFragmentInit->GetAbsolutePosition().inSeconds());
			mTSBStore->Delete(initUrl);
		}
	}
	return removedFragment;
}

/**
 * @brief Monitors the write queue and writes any pending data to AAMP TSB
 */
void AampTSBSessionManager::ProcessWriteQueue()
{
	std::unique_lock<std::mutex> lock(mWriteQueueMutex);
	AAMPLOG_INFO("Enter AAMP TSB write thread");
	while (!mStopThread_.load())
	{
		mWriteThreadCV.wait(lock, [this]()
							{ return !mWriteQueue.empty() || mStopThread_.load(); });

		if (!mStopThread_.load() && !mWriteQueue.empty())
		{
			TSBWriteData writeData = mWriteQueue.front();
			mWriteQueue.pop();
			lock.unlock(); // Release the lock before writing to AAMP TSB

			bool writeSucceeded = false;
			AampMediaType mediatype = ConvertMediaType(writeData.cachedFragment->type);
			while (!writeSucceeded && !mStopThread_.load())
			{
				long long tStartTime = NOW_STEADY_TS_MS;
				// If an Ad gets repeated then we need to generate a unique URL for each fragment,
				// so that during culling fragments are not deleted for a later instance of the
				// repeated Ad
				std::string uniqueUrl = ToUniqueUrl(writeData.url, writeData.cachedFragment->absPosition);

				// Call TSBHandler Write operation
				TSB::Status status = mTSBStore->Write(uniqueUrl, writeData.cachedFragment->fragment.GetPtr(), writeData.cachedFragment->fragment.GetLen());
				if (status == TSB::Status::OK)
				{
					writeSucceeded = true;
					bool TSBDataAddStatus = false;
					AAMPLOG_TRACE("TSBWrite Metrics...OK...time taken (%lldms)...buffer (%zu)....BW(%ld)...mediatype(%s)...disc(%d)...pts(%f)...periodId(%s)..URL (%s)",
						NOW_STEADY_TS_MS - tStartTime, writeData.cachedFragment->fragment.GetLen(), writeData.cachedFragment->cacheFragStreamInfo.bandwidthBitsPerSecond, GetMediaTypeName(writeData.cachedFragment->type),
						writeData.cachedFragment->discontinuity, writeData.pts, writeData.periodId.c_str(), writeData.url.c_str());
					LockReadMutex();
					if (writeData.cachedFragment->initFragment)
					{
						TSBDataAddStatus = GetTsbDataManager(mediatype)->AddInitFragment(writeData.url,
																						 mediatype,
																						 writeData.cachedFragment->cacheFragStreamInfo,
																						 writeData.periodId,
																						 writeData.cachedFragment->absPosition,
																						 writeData.cachedFragment->profileIndex);
					}
					else
					{
						TSBDataAddStatus = GetTsbDataManager(mediatype)->AddFragment(writeData,
																					mediatype,
																					writeData.cachedFragment->discontinuity);
						if(GetTsbReader(mediatype))
						{
							GetTsbReader(mediatype)->SetNewInitWaiting(false);
						}
					}
					UpdateTotalStoreDuration(mediatype, writeData.cachedFragment->duration);
					if (TSBDataAddStatus)
					{
						if (GetTsbReader(mediatype))
						{
							if(writeData.cachedFragment->initFragment)
							{
								GetTsbReader(mediatype)->SetNewInitWaiting(true);
								AAMPLOG_INFO("[%s] New init active at live edge %s", GetMediaTypeName(mediatype), writeData.url.c_str());
							}
							else if(eTUNETYPE_SEEKTOLIVE != mActiveTuneType)
							{
								// Reset EOS for all other tune types except seek to live
								// For seek to live, segment injection has to go through chunked transfer and reader has to exit
								GetTsbReader(mediatype)->ResetEos();
								AAMPLOG_INFO("[%s] Resetting EOS", GetMediaTypeName(mediatype));
							}
						}
					}
					UnlockReadMutex();
				}
				else if (status == TSB::Status::ALREADY_EXISTS)
				{
					// Init fragments & Fragments should have a unique url for each absPosition
					writeSucceeded = true;
					AAMPLOG_WARN("TSBWrite Metrics...FILE ALREADY EXISTS...time taken (%lldms)...buffer (%zu)....BW(%ld)...mediatype(%s)...disc(%d)...pts(%f)...Period-Id(%s)...URL (%s)",
								 NOW_STEADY_TS_MS - tStartTime, writeData.cachedFragment->fragment.GetLen(), writeData.cachedFragment->cacheFragStreamInfo.bandwidthBitsPerSecond, GetMediaTypeName(writeData.cachedFragment->type), writeData.cachedFragment->discontinuity, writeData.pts, writeData.periodId.c_str(), writeData.url.c_str());
				}
				else
				{
					if (status != TSB::Status::NO_SPACE) /** Flood the log when storage full so added check*/
					{
						AAMPLOG_ERR("[%s] TSB Write Operation FAILED...time taken (%lldms)...buffer (%zu)....BW(%ld)...disc(%d)...pts(%.02lf)...URL (%s)", GetMediaTypeName(writeData.cachedFragment->type), NOW_STEADY_TS_MS - tStartTime, writeData.cachedFragment->fragment.GetLen(), writeData.cachedFragment->cacheFragStreamInfo.bandwidthBitsPerSecond,  writeData.cachedFragment->discontinuity, writeData.pts, writeData.url.c_str()); // log metrics for failed case also.
					}
					else
					{
						AAMPLOG_TRACE("[%s] TSB Write Operation FAILED...time taken (%lldms)...buffer (%zu)....BW(%ld)...disc(%d)...pts(%.02lf)...URL (%s)", GetMediaTypeName(writeData.cachedFragment->type), NOW_STEADY_TS_MS - tStartTime, writeData.cachedFragment->fragment.GetLen(), writeData.cachedFragment->cacheFragStreamInfo.bandwidthBitsPerSecond,  writeData.cachedFragment->discontinuity, writeData.pts, writeData.url.c_str()); // log metrics for failed case also.
					}
					LockReadMutex();
					if(writeData.cachedFragment->fragment.GetLen() == 0) //Buffer 0 case ,no need to run this loop untill it get success
					{
						writeSucceeded = true;
					}
					else
					{
						TsbFragmentDataPtr removedFragment = RemoveFragmentDeleteInit(mediatype);
						if (removedFragment)
						{
							UpdateTotalStoreDuration(mediatype, -removedFragment->GetDuration().inSeconds());
							std::string removedFragmentUrl = ToUniqueUrl(removedFragment->GetUrl(),removedFragment->GetAbsolutePosition().inSeconds());
							mTSBStore->Delete(removedFragmentUrl);
							AAMPLOG_INFO("[%s] Removed  %.02lf sec, AbsPosition: %.02lfs ,pts %.02lf, Url : %s", GetMediaTypeName(mediatype), removedFragment->GetDuration().inSeconds(), removedFragment->GetAbsolutePosition().inSeconds(), removedFragment->GetPTS().inSeconds(), removedFragmentUrl.c_str());
						}
					}
					UnlockReadMutex();
				}
			}
			lock.lock(); // Reacquire the lock for next iter
		}
	}
	AAMPLOG_INFO("Exit AAMP TSB write thread");
}

/**
 * @brief Flush - function to clear the TSB storage
 *
 * @return None
 */
void AampTSBSessionManager::Flush()
{
	AAMPLOG_INFO("Flush AAMP TSB");
	// Call TSBHandler Flush to clear the TSB
	// Clear all the data structure within AampTSBSessionManager
	// Stop the monitor thread
	mStopThread_.store(true);

	if (mInitialized_)
	{
		// Notify the monitor thread in case it's waiting
		mWriteThreadCV.notify_one();
		if (mWriteThread.joinable())
		{
			mWriteThread.join();
		}
		// TODO: Need to take flush performance metrics
		mTSBStore->Flush();
		for (auto &it : mDataManagers)
		{
			it.second.first->Flush();
		}
		mInitialized_ = false;
	}
	mStoreEndPosition = 0.0;
	mLiveEndPosition = 0.0;
}

/**
 * @brief Culling of Segments based on the Max TSB configuration
 * @param[in] AampMediaType
 *
 * @return double - Total culled duration in seconds
 */
double AampTSBSessionManager::CullSegments()
{
	LockReadMutex();
	double culledduration = 0;
	double lastVideoPos = mLastVideoPos;
	int iter = eMEDIATYPE_VIDEO;
	while (iter < AAMP_TRACK_COUNT)
	{
		if (GetTotalStoreDuration((AampMediaType)iter) == 0)
		{
			iter++;
			continue;
		}
		// Get the first position of both audio and video
		double videoFirstPosition = GetTsbDataManager(eMEDIATYPE_VIDEO)->GetFirstFragmentPosition();

		// Check if video position has changed
		if ((eMEDIATYPE_VIDEO == iter) && (AAMP_PAUSE_POSITION_INVALID_POSITION != mLastVideoPos))
		{
			culledduration += (videoFirstPosition - lastVideoPos); // Adjust culledduration for write failures
		}
		lastVideoPos = videoFirstPosition; // Update lastVideoPos

		// Track sync logic
		double trackFirstPosition = GetTsbDataManager((AampMediaType)iter)->GetFirstFragmentPosition();
		double trackLastPosition = GetTsbDataManager((AampMediaType)iter)->GetLastFragmentPosition();
		bool eos;
		TsbFragmentDataPtr firstFragment = GetTsbDataManager((AampMediaType)iter)->GetFragment(trackFirstPosition, eos);
		// Calculate the next fragment position from the eldest part of TSB
		double adjacentFragmentPosition = trackFirstPosition;
		if (firstFragment)
		{
			// Take the next eldest position incase this particular fragment gets removed
			adjacentFragmentPosition = firstFragment->GetDuration().inSeconds() + trackFirstPosition;
		}

		// Check if we need to cull any segments
		if (GetTotalStoreDuration(eMEDIATYPE_VIDEO) <= mTsbLength && (videoFirstPosition < adjacentFragmentPosition))
		{
			AAMPLOG_TRACE("[%s]Total Store duration (%lf / %d), firstFragment:%lf last:%lf, next:%lf, videoFirstFrag:%lf", GetMediaTypeName((AampMediaType) iter), GetTotalStoreDuration((AampMediaType) iter), mTsbLength, trackFirstPosition, trackLastPosition, adjacentFragmentPosition, videoFirstPosition);
			iter++;
			continue; // No need to cull segments for this mediaType
		}

		// Determine which segments to remove based on first PTS
		AampMediaType mediaTypeToRemove = (GetTotalStoreDuration(eMEDIATYPE_VIDEO) > mTsbLength) ? eMEDIATYPE_VIDEO : (AampMediaType)iter;

		bool skip = false;
		// Check if removing from video can keep audio ahead
		if (mediaTypeToRemove == iter)
		{
			TsbFragmentDataPtr nearestFragment = GetTsbDataManager(mediaTypeToRemove)->GetNearestFragment(trackFirstPosition);
			if (nearestFragment && nearestFragment->GetDuration() > (videoFirstPosition - trackFirstPosition))
			{
				skip = true;
			}
		}
		if (!skip)
		{
			// Remove the oldest segment
			TsbFragmentDataPtr removedFragment = RemoveFragmentDeleteInit(mediaTypeToRemove);
			if (removedFragment)
			{
				double durationInSeconds = removedFragment->GetDuration().inSeconds();
				if (eMEDIATYPE_VIDEO == mediaTypeToRemove)
					culledduration += durationInSeconds;
				std::string removedFragmentUrl = ToUniqueUrl(removedFragment->GetUrl(),removedFragment->GetAbsolutePosition().inSeconds());
				UnlockReadMutex();
				mTSBStore->Delete(removedFragmentUrl);
				LockReadMutex();
				AAMPLOG_INFO("[%s] Removed %lf fragment duration seconds, Url: %s, AbsPosition: %lf, pts %lf", GetMediaTypeName(mediaTypeToRemove), durationInSeconds, removedFragmentUrl.c_str(), removedFragment->GetAbsolutePosition().inSeconds(), removedFragment->GetPTS().inSeconds());

				if (eMEDIATYPE_VIDEO == mediaTypeToRemove)
				{
					(void)mMetaDataManager.RemoveMetaData(removedFragment->GetAbsolutePosition() + removedFragment->GetDuration());
				}

				// Update total stored duration
				UpdateTotalStoreDuration(mediaTypeToRemove, -durationInSeconds);
			}
			else
			{
				AAMPLOG_ERR("[%s] No fragments to remove", GetMediaTypeName(mediaTypeToRemove));
				iter++;
			}
		}
		else
		{
			iter++;
		}
	}

	// Update mLastVideoPos
	if(0 != GetTotalStoreDuration(eMEDIATYPE_VIDEO))
	{
		mLastVideoPos = lastVideoPos;
	}
	if(culledduration > 0)
	{
		mCulledDuration += culledduration;
	}
	UnlockReadMutex();
	return culledduration;
}

/**
 * @brief ConvertMediaType - Convert to actual AampMediaType
 *
 * @param[in] mediatype - type to be converted
 *
 * @return AampMediaType - converted mediaType
 */
AampMediaType AampTSBSessionManager::ConvertMediaType(AampMediaType actualMediatype)
{
	AampMediaType mediaType = actualMediatype;

	if (mediaType == eMEDIATYPE_INIT_VIDEO)
	{
		mediaType = eMEDIATYPE_VIDEO;
	}
	else if (mediaType == eMEDIATYPE_INIT_AUDIO)
	{
		mediaType = eMEDIATYPE_AUDIO;
	}

	else if (mediaType == eMEDIATYPE_INIT_SUBTITLE)
	{
		mediaType = eMEDIATYPE_SUBTITLE;
	}
	else if (mediaType == eMEDIATYPE_INIT_AUX_AUDIO)
	{
		mediaType = eMEDIATYPE_AUX_AUDIO;
	}
	else if (mediaType == eMEDIATYPE_INIT_IFRAME)
	{
		mediaType = eMEDIATYPE_IFRAME;
	}

	return mediaType;
}

/**
 * @brief Get Total TSB duration
 * @param[in] AampMediaType media type or track type
 *
 * @return total duration
 */
double AampTSBSessionManager::GetTotalStoreDuration(AampMediaType mediaType)
{
	double totalDuration = -1;
	std::shared_ptr<AampTsbDataManager> dataMgr = GetTsbDataManager(mediaType);
	if(nullptr != dataMgr)
	{
		if(dataMgr->GetLastFragment())
		{
			totalDuration = (dataMgr->GetLastFragmentPosition() + dataMgr->GetLastFragment()->GetDuration().inSeconds()) - dataMgr->GetFirstFragmentPosition();
		}
		else
		{
			totalDuration = 0;
		}
	}
	else
	{
		AAMPLOG_ERR("%s:%d No dataManager available for mediaType:%d", __FUNCTION__, __LINE__, mediaType);
	}
	return totalDuration;
}

/**
 * @brief Get TSBDataManager
 * @param[in] AampMediaType media type or track type
 *
 * @return ptr of dataManager
 */
std::shared_ptr<AampTsbDataManager> AampTSBSessionManager::GetTsbDataManager(AampMediaType mediaType)
{
	std::shared_ptr<AampTsbDataManager> dataMgr;
	if (mDataManagers.find(mediaType) != mDataManagers.end())
	{
		dataMgr = mDataManagers.at(mediaType).first;
	}
	else
	{
		AAMPLOG_ERR("%s:%d No dataManager available for mediaType:%d", __FUNCTION__, __LINE__, mediaType);
	}

	return dataMgr;
}

/**
 * @brief Get TSBReader
 * @param[in] AampMediaType media type or track type
 *
 * @return ptr of tsbReader
 */
std::shared_ptr<AampTsbReader> AampTSBSessionManager::GetTsbReader(AampMediaType mediaType)
{
	std::shared_ptr<AampTsbReader> reader;
	if (mTsbReaders.find(mediaType) != mTsbReaders.end())
	{
		reader = mTsbReaders[mediaType];
	}
	else
	{
		AAMPLOG_ERR("%s:%d No TsbReader available for mediaType:%d", __FUNCTION__, __LINE__, mediaType);
	}

	return reader;
}

/**
 * @brief Invoke TSB Readers
 * @param[in,out] startPosSec - Start absolute position, seconds since 1970; in: requested, out: selected
 * @param[in] rate
 * @param[in] tuneType
 *
 * @return AAMPSTatusType - OK if success
 */
AAMPStatusType AampTSBSessionManager::InvokeTsbReaders(double &startPosSec, float rate, TuneType tuneType)
{
	INIT_CHECK_RETURN_VAL(eAAMPSTATUS_GENERIC_ERROR);

	LockReadMutex();
	AAMPStatusType ret = eAAMPSTATUS_OK;
	if (!mTsbReaders.empty())
	{
		// Re-Invoke TSB readers to new position
		mActiveTuneType = tuneType;
		mLastAdMetaDataProcessed = nullptr;
		GetTsbReader(eMEDIATYPE_VIDEO)->Term();
		ret = GetTsbReader(eMEDIATYPE_VIDEO)->Init(startPosSec, rate, tuneType);
		if (eAAMPSTATUS_OK != ret)
		{
			UnlockReadMutex();
			return ret;
		}

		// Sync tracks with relative seek position
		for (int i = (AAMP_TRACK_COUNT - 1); i > eMEDIATYPE_VIDEO; i--)
		{
			// Re-initialize reader with synchronized values
			double startPosOtherTracks = startPosSec;
			GetTsbReader((AampMediaType)i)->Term();
			if(AAMP_NORMAL_PLAY_RATE == rate)
			{
				ret = GetTsbReader((AampMediaType)i)->Init(startPosOtherTracks, rate, tuneType, GetTsbReader(eMEDIATYPE_VIDEO));
			}
		}
	}
	UnlockReadMutex();
	return ret;
}

/**
 * @brief Skip the frames based on playback rate on trickplay
 */
void AampTSBSessionManager::SkipFragment(std::shared_ptr<AampTsbReader>& reader, TsbFragmentDataPtr& nextFragmentData)
{
	if (nextFragmentData && reader && !reader->IsEos())
	{
		AampTime skippedDuration = 0.0;
		if(eMEDIATYPE_VIDEO == reader->GetMediaType())
		{
			AampTime startPos = nextFragmentData->GetAbsolutePosition();
			int vodTrickplayFPS = mAamp->mConfig->GetConfigValue(eAAMPConfig_VODTrickPlayFPS);
			float rate = reader->GetPlaybackRate();
			AampTime delta = 0.0;
			if(mAamp->playerStartedWithTrickPlay)
			{
				AAMPLOG_WARN("Played switched in trickplay, delta set to zero");
				delta = 0.0;
				mAamp->playerStartedWithTrickPlay = false;
			}
			else if (vodTrickplayFPS == 0)
			{
				AAMPLOG_WARN("vodTrickplayFPS is zero, delta set to zero");
			}
			else
			{
				delta = static_cast<AampTime>(std::abs(static_cast<double>(rate))) / static_cast<double>(vodTrickplayFPS);
			}
			while(delta > nextFragmentData->GetDuration())
			{
				delta -= nextFragmentData->GetDuration();
				skippedDuration += nextFragmentData->GetDuration();
				TsbFragmentDataPtr tmp = reader->FindNext(skippedDuration);
				if (!tmp)
				{
					// At end of stream, break out of loop
					break;
				}
				nextFragmentData = tmp;

			}
			AAMPLOG_INFO("Skipped frames [rate=%.02f] from %.02lf to %.02lf total duration = %.02lf",
					rate, startPos.inSeconds(), nextFragmentData->GetAbsolutePosition().inSeconds(), skippedDuration.inSeconds());
		}
	}
	return;
}
/**
 * @brief Read next fragment and push it to the injector loop
 *
 * @param[in] MediaStreamContext of appropriate track
 * @param[in] numFreeFragments number of free fragment spaces in the cache
 * @return bool - true if cached fragment
 * @brief Fetches and caches audio fragment in parallel with video fragment.
 */
bool AampTSBSessionManager::PushNextTsbFragment(MediaStreamContext *pMediaStreamContext,
												uint32_t numFreeFragments)
{
	// FN_TRACE_F_MPD( __FUNCTION__ );
	INIT_CHECK_RETURN_VAL(false);

	bool ret = true;
	AampMediaType mediaType = pMediaStreamContext->mediaType;
	LockReadMutex();
	uint32_t numNeededFragments = 1;
	std::shared_ptr<AampTsbReader> reader = GetTsbReader(mediaType);

	if (reader->TrackEnabled())
	{
		if (numFreeFragments)
		{
			TsbFragmentDataPtr nextFragmentData = reader->FindNext();
			float rate = reader->GetPlaybackRate();
			// Slow motion is handled in GST layer with SetPlaybackRate
			if(AAMP_NORMAL_PLAY_RATE !=  rate && AAMP_RATE_PAUSE != rate && AAMP_SLOWMOTION_RATE != rate && eMEDIATYPE_VIDEO == mediaType)
			{
				SkipFragment(reader, nextFragmentData);
			}

			if (nextFragmentData)
			{
				TsbInitDataPtr initFragmentData = nextFragmentData->GetInitFragData();
				bool injectInitFragmentData = false;
				double bandwidth = initFragmentData->GetBandWidth();
				if (initFragmentData && (initFragmentData != reader->mLastInitFragmentData))
				{
					AAMPLOG_TRACE("[%s] Previous init fragment data is different from current init fragment data, injecting", GetMediaTypeName(mediaType));
					numNeededFragments = 2;
					injectInitFragmentData = true;
				}

				if (numFreeFragments >= numNeededFragments)
				{
					// Going to cache the fragment so update the reader with the next fragment
					reader->ReadNext(nextFragmentData);

					if (injectInitFragmentData)
					{
						reader->mLastInitFragmentData = initFragmentData;
						CachedFragmentPtr initFragment = Read(std::move(initFragmentData));
						if (initFragment)
						{
							if(reader->IsDiscontinuous())
							{
								initFragment->discontinuity = true;
							}

							// For init fragment use next fragment PTS as position for injection,
							// as the PTS value is required for overriding events in qtdemux
							initFragment->position = nextFragmentData->GetPTS().inSeconds();

							AAMPLOG_INFO("[%s] Cache init fragment CurrentBandwidth: %.02lf Previous Bandwidth: %.02lf IsDiscontinuous: %d",
								GetMediaTypeName(mediaType), bandwidth, reader->mCurrentBandwidth, initFragment->discontinuity);

							if (pMediaStreamContext->CacheTsbFragment(std::move(initFragment)))
							{
								AAMPLOG_TRACE("[%s] Successfully cached init fragment", GetMediaTypeName(mediaType));
								reader->mCurrentBandwidth = bandwidth;
							}
							else
							{
								AAMPLOG_ERR("[%s] Failed to cache init fragment", GetMediaTypeName(mediaType));
								reader->mLastInitFragmentData = nullptr;
								ret = false;
							}
						}
						else
						{
							AAMPLOG_ERR("[%s] Failed to read init fragment at %lf", GetMediaTypeName(mediaType), nextFragmentData->GetAbsolutePosition().inSeconds());
							ret = false;
						}
					}

					if (ret)
					{
						double pts = 0;
						CachedFragmentPtr nextFragment = Read(nextFragmentData, pts);
						if (nextFragment)
						{
							// Slow motion is like a normal playback with audio (volume set to 0) and handled in GST layer with SetPlaybackRate
							if(mAamp->IsIframeExtractionEnabled() && AAMP_NORMAL_PLAY_RATE !=  rate && AAMP_RATE_PAUSE != rate && eMEDIATYPE_VIDEO == mediaType && AAMP_SLOWMOTION_RATE != rate )
							{
								if(!mIsoBmffHelper->ConvertToKeyFrame(nextFragment->fragment))
								{
									AAMPLOG_ERR("[%s] Failed to generate iFrame track from video track at %lf", GetMediaTypeName(mediaType), nextFragmentData->GetAbsolutePosition().inSeconds());
								}
							}
							UnlockReadMutex();

							ProcessAdMetadata(mediaType, nextFragmentData, rate);

							if (pMediaStreamContext->CacheTsbFragment(std::move(nextFragment)))
							{
								AAMPLOG_TRACE("[%s] Successfully cached fragment", GetMediaTypeName(mediaType));
								if(reader->IsEos())
								{
									// Unblock live downloader if it is waiting for end fragment injection
									reader->AbortCheckForWaitIfReaderDone();
								}
							}
							else
							{
								AAMPLOG_ERR("[%s] Failed to cache fragment", GetMediaTypeName(mediaType));
								ret = false;
							}
							LockReadMutex();
						}
						else
						{
							AAMPLOG_ERR("[%s] Failed to read fragment at %lf", GetMediaTypeName(mediaType), nextFragmentData->GetAbsolutePosition().inSeconds());
							ret = false;
						}
					}
				}
				else
				{
					AAMPLOG_TRACE("[%s] Insufficient space, free %u needed %u", GetMediaTypeName(mediaType), numFreeFragments, numNeededFragments);
					ret = false;
				}
			}
			else
			{
				AAMPLOG_WARN("[%s] Failed to read next fragment", GetMediaTypeName(mediaType));
				ret = false;
			}
		}
		else
		{
			AAMPLOG_TRACE("[%s] Insufficient space, free %u", GetMediaTypeName(mediaType), numFreeFragments);
			ret = false;
		}
	}
	else
	{
		AAMPLOG_WARN("[%s] Track not enabled", GetMediaTypeName(mediaType));
		ret = false;
	}
	UnlockReadMutex();
	return ret;
}

/**
 * @brief GetManifestEndDelta - Get manifest delta with live downloader end
 *
 * @return void
 */
double AampTSBSessionManager::GetManifestEndDelta()
{
	double manifestEndDelta = 0.0;
	LockReadMutex();
	if(mStoreEndPosition > 0 && mAamp->mAbsoluteEndPosition > 0  )
	{
		manifestEndDelta = mStoreEndPosition - mAamp->mAbsoluteEndPosition > 0;
	}
	else
	{
		AAMPLOG_WARN("TSB SEssion manager progress has not yet updated!!! returning..  %.02lf", manifestEndDelta);
	}
	UnlockReadMutex();

	return manifestEndDelta;
}
/**
 * @brief UpdateProgress - Progress updates
 *
 * @param[in] manifestDuration - current manifest duration
 * @param[in] manifestCulledSecondsFromStart - Culled duration of manifest
 * @return void
 */
void AampTSBSessionManager::UpdateProgress(double manifestDuration, double manifestCulledSecondsFromStart)
{
	INIT_CHECK_RETURN_VOID();

	double culledSeconds = 0.0;
	culledSeconds = CullSegments();
	if (culledSeconds > 0)
	{
		// Update culled seconds based on seconds culled in store
		AAMPLOG_TRACE("Updating culled seconds: %lf", culledSeconds);
		mAamp->UpdateCullingState(culledSeconds);
	}
	mAamp->culledSeconds = GetTsbDataManager(eMEDIATYPE_VIDEO)->GetFirstFragmentPosition();
	LockReadMutex();
	AAMPLOG_TRACE("LiveDownloader:: Manifest total duration:%lf, ManifestCulledSeconds:%lf", manifestDuration, manifestCulledSecondsFromStart);
	mStoreEndPosition = mAamp->culledSeconds + GetTotalStoreDuration(eMEDIATYPE_VIDEO);
	if (mAamp->mConfig->IsConfigSet(eAAMPConfig_ProgressLogging))
	{
		AAMPLOG_INFO("tsb pos: [%lf..[X]..%lf]", mAamp->culledSeconds, mAamp->mAbsoluteEndPosition);
	}
	UnlockReadMutex();
	double duration = mAamp->mAbsoluteEndPosition -mAamp->culledSeconds;
	AAMPLOG_TRACE("Updating duration: %lf", duration);
	mAamp->UpdateDuration(duration);
}

/**
 * @brief Get the current video bitrate from the TSB reader.
 * @return The current video bitrate in bps, or 0.0 if unavailable.
 */

BitsPerSecond AampTSBSessionManager::GetVideoBitrate()
{
	BitsPerSecond bitrate = 0.0;
	std::shared_ptr<AampTsbReader> reader = GetTsbReader(eMEDIATYPE_VIDEO);
	if(reader)
	{
		bitrate = static_cast<BitsPerSecond> (reader->mCurrentBandwidth);
	}
	return bitrate;
}

/**
 * @brief Start an ad reservation
 * @param[in] adBreakId - ID of the ad break
 * @param[in] periodPosition - position of the ad reservation
 * @param[in] absPosition - absolute position
 * @return bool - true if success
 */
bool AampTSBSessionManager::StartAdReservation(const std::string &adBreakId, uint64_t periodPosition, AampTime absPosition)
{
	auto metaData = std::make_shared<AampTsbAdReservationMetaData>(
		AampTsbAdMetaData::EventType::START,
		absPosition,
		adBreakId,
		periodPosition);
	return mMetaDataManager.AddMetaData(metaData);
}

/**
 * @brief End an ad reservation
 * @param[in] adBreakId - ID of the ad break
 * @param[in] periodPosition - position of the ad reservation
 * @param[in] absPosition - absolute position
 * @return bool - true if success
 */
bool AampTSBSessionManager::EndAdReservation(const std::string &adBreakId, uint64_t periodPosition, AampTime absPosition)
{
	auto metaData = std::make_shared<AampTsbAdReservationMetaData>(
		AampTsbAdMetaData::EventType::END,
		absPosition,
		adBreakId,
		periodPosition);
	return mMetaDataManager.AddMetaData(metaData);
}

/**
 * @brief Start an ad placement
 * @param[in] adId - ID of the ad
 * @param[in] relativePosition - position of the ad placement
 * @param[in] absPosition - absolute position
 * @param[in] duration - duration of the ad placement
 * @param[in] offset - offset of the ad placement
 * @return bool - true if success
 */
bool AampTSBSessionManager::StartAdPlacement(const std::string &adId, uint32_t relativePosition, AampTime absPosition, double duration, uint32_t offset)
{
	auto metaData = std::make_shared<AampTsbAdPlacementMetaData>(
		AampTsbAdMetaData::EventType::START,
		absPosition,
		duration,
		adId,
		relativePosition,
		offset);
	return mMetaDataManager.AddMetaData(metaData);
}

/**
 * @brief End an ad placement
 * @param[in] adId - ID of the ad
 * @param[in] relativePosition - position of the ad placement
 * @param[in] absPosition - absolute position
 * @param[in] duration - duration of the ad placement
 * @param[in] offset - offset of the ad placement
 * @return bool - true if success
 */
bool AampTSBSessionManager::EndAdPlacement(const std::string &adId, uint32_t relativePosition, AampTime absPosition, double duration, uint32_t offset)
{
	auto metaData = std::make_shared<AampTsbAdPlacementMetaData>(
		AampTsbAdMetaData::EventType::END,
		absPosition,
		duration,
		adId,
		relativePosition,
		offset);
	return mMetaDataManager.AddMetaData(metaData);
}

/**
 * @brief End an ad placement with error
 * @param[in] adId - ID of the ad
 * @param[in] relativePosition - position of the ad placement
 * @param[in] absPosition - absolute position
 * @param[in] duration - duration of the ad placement
 * @param[in] offset - offset of the ad placement
 * @return bool - true if success
 */
bool AampTSBSessionManager::EndAdPlacementWithError(const std::string &adId, uint32_t relativePosition, AampTime absPosition, double duration, uint32_t offset)
{
	auto metaData = std::make_shared<AampTsbAdPlacementMetaData>(
		AampTsbAdMetaData::EventType::ERROR,
		absPosition,
		duration,
		adId,
		relativePosition,
		offset);
	return mMetaDataManager.AddMetaData(metaData);
}

// Need a method that simulates the send immediate flag for ads
// Shifts all current and future positions to the current position.
void AampTSBSessionManager::ShiftFutureAdEvents()
{
	// Protect this section with the write queue mutex
	std::unique_lock<std::mutex> guard(mWriteQueueMutex);
	AampTime currentWritePosition = mCurrentWritePosition;
	guard.unlock();

	// Get only AD type metadata using the template method with explicit type
	auto result = mMetaDataManager.GetMetaDataByType<AampTsbMetaData>(AampTsbMetaData::Type::AD_METADATA_TYPE, currentWritePosition, currentWritePosition + mTsbLength);
	(void)mMetaDataManager.ChangeMetaDataPosition(result, currentWritePosition);
}

void AampTSBSessionManager::ProcessAdMetadata(AampMediaType mediaType, TsbFragmentDataPtr nextFragmentData, float rate)
{
	if ((AAMP_NORMAL_PLAY_RATE == rate) && (eMEDIATYPE_VIDEO == mediaType))
	{
		AampTime rangeStart;
		if (mLastAdMetaDataProcessed != nullptr)
		{
			rangeStart = mLastAdMetaDataProcessed->GetPosition();
		}
		else
		{
			rangeStart = nextFragmentData->GetAbsolutePosition();
		}
		AampTime rangeEnd = nextFragmentData->GetAbsolutePosition() + nextFragmentData->GetDuration();

		AAMPLOG_DEBUG("rangeStart = %" PRIu64 "ms, rangeEnd = %" PRIu64 "ms",
			rangeStart.milliseconds(), rangeEnd.milliseconds());

		// Get all ad metadata within the fragment's time range
		auto adMetadataItems = mMetaDataManager.GetMetaDataByType<AampTsbAdMetaData>(
			AampTsbMetaData::Type::AD_METADATA_TYPE, rangeStart, rangeEnd);

		// Process metadata items in chronological order
		bool skip = mLastAdMetaDataProcessed != nullptr;
		for (const auto& adMetadata : adMetadataItems)
		{
			// Skip until we have reached the last processed metadata
			if (skip)
			{
				if (adMetadata == mLastAdMetaDataProcessed)
				{
					skip = false;
				}
			}
			else
			{
				AAMPLOG_INFO("Processing ad metadata type %d event %d at position: %" PRIu64 "ms",
							static_cast<int>(adMetadata->GetAdType()),
							static_cast<int>(adMetadata->GetEventType()),
							adMetadata->GetPosition().milliseconds());

				// Let the metadata object handle sending the appropriate event
				adMetadata->SendEvent(mAamp);
				mLastAdMetaDataProcessed = std::static_pointer_cast<AampTsbMetaData>(adMetadata);
			}
		}
	}
}
