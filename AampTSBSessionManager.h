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
 * @file AampTSBSessionManager.h
 * @brief AampTSBSessionManager for AAMP
 */

#ifndef AAMP_TSB_SESSION_MANAGER_H
#define AAMP_TSB_SESSION_MANAGER_H

#include <string>
#include <thread>
#include <mutex>
#include <uuid/uuid.h>
#include <map>
#include "tsb/api/TsbApi.h"
#include "AampMediaType.h"
#include "AampTsbDataManager.h"
#include "AampTsbMetaDataManager.h"
#include "AampTsbReader.h"
#include "MediaStreamContext.h"
#include <condition_variable>
#include <queue>

class CachedFragment;
class AampCacheHandler;
class AampTsbReader;

typedef std::shared_ptr<CachedFragment> CachedFragmentPtr;

/**
 * @class AampTSBSessionManager
 * @brief AampTSBSessionManager Class defn
 */
class AampTSBSessionManager
{
public:
	/**
	 * @brief AampTSBSessionManager Constructor
	 *
	 * @return None
	 */
	AampTSBSessionManager(PrivateInstanceAAMP *aamp);
	/**
	 * @brief AampTSBSessionManager Destructor
	 *
	 * @return None
	 */
	~AampTSBSessionManager();
	/**
	 * @brief AampTSBSessionManager Init function
	 *
	 * @return None
	 */
	void Init();
	/**
	 * @brief Write - function to enqueue data for writing to AAMP TSB
	 *
	 * @param[in] - URL
	 * @param[in] - cachedFragment
	 */
	void EnqueueWrite(std::string url, std::shared_ptr<CachedFragment> cachedFragment, std::string periodId);
	/**
	 * @brief Flush  - function to clear the TSB storage
	 *
	 * @return None
	 */
	void Flush();
	/**
	 * @brief Monitors the write queue and writes any pending data to AAMP TSB
	 */
	void ProcessWriteQueue();
	/**
	 * @brief Set TSB length
	 *
	 * @param[in] length
	 */
	void SetTsbLength(int tsbLength) { mTsbLength = tsbLength; }
	/**
	 * @brief Set TSB location
	 *
	 * @param[in] string - location
	 */
	void SetTsbLocation(const std::string &tsbLocation) { mTsbLocation = tsbLocation; }
	/**
	 * @brief SetTsbMinFreePercentage - minimum disc free percentage
	 *
	 * @param[in] tsbMinFreePercentage
	 */
	void SetTsbMinFreePercentage(int tsbMinFreePercentage) { mTsbMinFreePercentage = tsbMinFreePercentage; }
	/**
	 * @brief SetTsbMaxDiskStorage - maximum disk storage
	 *
	 * @param[in] tsbMaxDiskStorage
	 */
	void SetTsbMaxDiskStorage(unsigned int tsbMaxDiskStorage) { mTsbMaxDiskStorage = tsbMaxDiskStorage; }

	/**
	 * @brief ConvertMediaType - Convert to actual AampMediaType
	 *
	 * @param[in] mediatype - type to be converted
	 *
	 * @return AampMediaType - converted mediaType
	 */
	AampMediaType ConvertMediaType(AampMediaType mediatype);
	/**
	 * @brief GetTotalStoreDuration - Get total data store duration
	 *
	 * @return duration
	 */
	double GetTotalStoreDuration(AampMediaType mediaType);
	/**
	 * @brief Culling of Segments based on the Max TSB configuration
	 *
	 * @return total culled seconds
	 */
	double CullSegments();
	/**
	 * @brief Get TSBDataManager
	 * @param[in] AampMediaType media type or track type
	 * @return ptr of dataManager
	 */
	std::shared_ptr<AampTsbDataManager> GetTsbDataManager(AampMediaType mediaType);
	/**
	 * @brief Get TSBReader
	 * @param[in] AampMediaType media type or track type
	 *
	 * @return ptr of tsbReader
	 */
	std::shared_ptr<AampTsbReader> GetTsbReader(AampMediaType);
	/**
	 * @brief Invoke TSB Readers
	 * @param[in,out] startPosSec - Start absolute position, seconds since 1970; in: requested, out: selected
	 * @param[in] rate
	 * @param[in] tuneType
	 *
	 * @return AAMPSTatusType - OK if success
	 */
	AAMPStatusType InvokeTsbReaders(double &startPosSec, float rate, TuneType tuneType);
	/**
	 * @brief InitializeDataManagers
	 *
	 * @return None
	 */
	void InitializeDataManagers();
	/**
	 * @brief Initialize metadata manager and register types
	 *
	 * @return None
	 */
	void InitializeMetaDataManager();
	/**
	 * @brief Initialize TSB readers for different media type
	 *
	 * @return None
	 */
	void InitializeTsbReaders();
	/**
	 * @brief Read next fragment and push it to the injector loop
	 *
	 * @param[in] MediaStreamContext of appropriate track
	 * @param[in] numFreeFragments number of free fragment spaces in the cache
	 * @return bool - true if cached fragment
	 */
	bool PushNextTsbFragment(MediaStreamContext *pMediaStreamContext, uint32_t numFreeFragments);
	/**
	 * @brief UpdateProgress - Progress updates
	 *
	 * @param[in] manifestDuration - current manifest duration
	 * @param[in] manifestCulledSecondsFromStart - Culled duration of manifest
	 * @return void
	 */
	void UpdateProgress(double manifestDuration, double manifestCulledSecondsFromStart);

	/**
	 * @brief GetManifestEndDelta - Get manifest delta with live downloader end
	 *
	 * @param[in] manifestDuration - current manifest duration
	 * @param[in] manifestCulledSecondsFromStart - Culled duration of manifest
	 * @return double diff with manifest end
	 */
	double GetManifestEndDelta();

	/**
	 * @brief LockReadMutex - Protect read operations
	 */
	void LockReadMutex() { mReadMutex.lock(); }
	/**
	 * @brief UnlockReadMutex - Unlock acquire mutex
	 */
	void UnlockReadMutex() { mReadMutex.unlock(); }

	/**
	 *   @fn GetVideoBitrate
	 *   @return bitrate of video profile
	 */
	BitsPerSecond GetVideoBitrate();

	/**
	 *   @fn IsActive - If TSBSessionManager is active
	 *   @return bool true if TSBSessionManager is active
	 */
	bool IsActive() { return mInitialized_; }

	/**
	 * @brief Start an ad reservation
	 * @param[in] adBreakId - ID of the ad break
	 * @param[in] periodPosition - event position in terms of channel's timeline
	 * @param[in] absPosition - event absolute position
	 * @return bool - true if success
	 */
	bool StartAdReservation(const std::string &adBreakId, uint64_t periodPosition, AampTime absPosition);

	/**
	 * @brief End an ad reservation
	 * @param[in] adBreakId - ID of the ad break
	 * @param[in] periodPosition - event position in terms of channel's timeline
	 * @param[in] absPosition - event absolute position of the ad reservation end
	 * @return bool - true if success
	 */
	bool EndAdReservation(const std::string &adBreakId, uint64_t periodPosition, AampTime absPosition);

	/**
	 * @brief Start an ad placement
	 * @param[in] adId - ID of the ad
	 * @param[in] relativePosition - event position wrt to the corresponding adbreak start
	 * @param[in] absPosition - event absolute position
	 * @param[in] duration - duration of the current ad
	 * @param[in] offset - offset point of the current ad
	 * @return bool - true if success
	 */
	bool StartAdPlacement(const std::string &adId, uint32_t relativePosition, AampTime absPosition, double duration, uint32_t offset);

	/**
	 * @brief End an ad placement
	 * @param[in] adId - ID of the ad
	 * @param[in] relativePosition - event position wrt to the corresponding adbreak start
	 * @param[in] absPosition - event absolute position
	 * @param[in] duration - duration of the current ad
	 * @param[in] offset - offset point of the current ad
	 * @return bool - true if success
	 */
	bool EndAdPlacement(const std::string &adId, uint32_t relativePosition, AampTime absPosition, double duration, uint32_t offset);

	/**
	 * @brief End an ad placement with error
	 * @param[in] adId - ID of the ad
	 * @param[in] relativePosition - event position wrt to the corresponding adbreak start
	 * @param[in] absPosition - event absolute position
	 * @param[in] duration - duration of the current ad
	 * @param[in] offset - offset point of the current ad
	 * @return bool - true if success
	 */
	bool EndAdPlacementWithError(const std::string &adId, uint32_t relativePosition, AampTime absPosition, double duration, uint32_t offset);

	/**
	 * @brief Shift future ad events to the position of mCurrentWritePosition
	 * This simulates the send immediate flag for ads by shifting all existing 
	 * metadata events whose position is greater than mCurrentWritePosition to 
	 * mCurrentWritePosition.
	 */
	void ShiftFutureAdEvents();

protected:
	/**
	 * @brief Reads from the TSB library based on the initialization fragment data.
	 *
	 * @param[in] initfragdata Init fragment data
	 * @return A shared pointer to the cached fragment if read successfully, otherwise a null pointer.
	 */
	std::shared_ptr<CachedFragment> Read(TsbInitDataPtr initfragdata);
	/**
	 * @brief Reads from the TSB library based on fragment data.
	 *
	 * @param[in] fragmentdata TSB fragment data
	 * @param[out] pts  of the fragment.
	 * @return A shared pointer to the cached fragment if read successfully, otherwise a null pointer.
	 */
	std::shared_ptr<CachedFragment> Read(TsbFragmentDataPtr fragmentdata, double &pts);
	/**
	 * @brief Skip Fragment based on rate
	 * @param reader Reader object
	 * @param nextFragmentData Fragment Data
	 */
	void SkipFragment(std::shared_ptr<AampTsbReader> &reader, TsbFragmentDataPtr& nextFragmentData);

	/**
	 * @brief Process ad metadata events for the current fragment
	 *
	 * This function processes ad metadata events that occur within the time range
	 * of the current fragment.
	 *
	 * @param[in] mediaType Type of media track being processed
	 * @param[in] nextFragmentData Fragment data containing timing information
	 * @param[in] rate Current playback rate
	 *
	 * @note Ad events are only processed for video fragments during normal speed playback
	 */
	void ProcessAdMetadata(AampMediaType mediaType, TsbFragmentDataPtr nextFragmentData, float rate);

private:
	/**
	 * @brief GenerateId - function to generate unique id
	 *
	 * @return string
	 */
	std::string GenerateId();

	/**
	 * @brief UpdateTotalStoreDuration - Update total TSB data duration
	 * @param[in] mediaType - track type
	 * @param[in] durationInSeconds - total duration updated
	 */
	void UpdateTotalStoreDuration(AampMediaType mediaType, double durationInSeconds)
	{
		mDataManagers[mediaType].second += durationInSeconds;
	}

	/**
	 * @brief Generate unique URL for TSB store
	 * @param[in] url - url of segment
	 * @param[in] absPosition - abs position of segment
	 * @return string - unique url
	 */
	std::string ToUniqueUrl(std::string url, double absPosition);

	/**
	 * @brief Remove fragment from list and delete init fragment from TSB store if no longer referenced
	 * @param[in] mediaType - track type
	 * @return shared ptr to fragment removed is any
	 */
	TsbFragmentDataPtr RemoveFragmentDeleteInit(AampMediaType mediatype);

	/**
	 * @brief Check if a track for the given media type is stored in TSB
	 * @param[in] mediaType - track type
	 * @return true if track is stored in TSB, false otherwise
	 */
	bool IsTrackStoredInTsb(AampMediaType mediatype);

	bool mInitialized_;
	std::atomic_bool mStopThread_;			// This variable is atomic because it can be accessed from multiple threads

	int mTsbLength; // duration in seconds , default of 1500
	int mTsbMinFreePercentage;
	unsigned int mTsbMaxDiskStorage;
	std::string mTsbLocation;
	std::shared_ptr<TSB::Store> mTSBStore;
	std::string mTsbSessionId;
	std::thread mWriteThread;
	std::unordered_map<AampMediaType, std::pair<std::shared_ptr<AampTsbDataManager>, double>> mDataManagers; // AampMediaType -> {AampTsbDataManager, totalStoreDuration in seconds}
	std::unordered_map<AampMediaType, std::shared_ptr<AampTsbReader>> mTsbReaders;
	AampTsbMetaDataManager mMetaDataManager;
	double mCulledDuration;
	TuneType mActiveTuneType;
	std::mutex mWriteQueueMutex;			// Mutex to synchronize access to the write queue.
	std::mutex mReadMutex;					// Mutex to synchronize access to the data manager from reader and writer.
	std::condition_variable mWriteThreadCV; // Condition variable to signal when data is available in the write queue
	std::queue<TSBWriteData> mWriteQueue;	// Queue to store write data.
	double mLastVideoPos;
	double mStoreEndPosition; 		/**< Last reported TSB Store end position*/
	double mLiveEndPosition;		/**< Last reported Live end position*/
	AampTime  mCurrentWritePosition; /**< The last fragment position written to the TSB */
	std::shared_ptr<AampTsbMetaData> mLastAdMetaDataProcessed; /**< Last ad metadata processed */
public:
	PrivateInstanceAAMP *mAamp; /**< AAMP player's private instance */
	std::shared_ptr<IsoBmffHelper> mIsoBmffHelper; /**< ISO BMFF helper object */
};

#endif // AAMP_TSB_SESSION_MANAGER_H
