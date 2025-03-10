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

#ifndef AAMP_TSBSSESSIONMANAGER_H
#define AAMP_TSBSSESSIONMANAGER_H

#include <string>
#include <thread>
#include <mutex>
#include <uuid/uuid.h>
#include <map>
#include "tsb/api/TsbApi.h"
#include "priv_aamp.h"
#include "AampMediaType.h"
#include "AampTsbDataManager.h"
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
	 * @brief Initialize TSB readers for different media type
	 *
	 * @return None
	 */
	void InitializeTsbReaders();
	/**
	 * @brief Read next fragment and push it to the injector loop
	 *
	 * @param[in] MediaStreamContext of appropriate track
	 * @return bool - true if success
	 */
	bool PushNextTsbFragment(MediaStreamContext *pMediaStreamContext);
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
	 * @return duration of skipped frames
	 */
	void SkipFragment(std::shared_ptr<AampTsbReader> &reader, TsbFragmentDataPtr& nextFragmentData);

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
	double mCulledDuration;
	TuneType mActiveTuneType;
	std::mutex mWriteQueueMutex;			// Mutex to synchronize access to the write queue.
	std::mutex mReadMutex;					// Mutex to synchronize access to the data manager from reader and writer.
	std::condition_variable mWriteThreadCV; // Condition variable to signal when data is available in the write queue
	std::queue<TSBWriteData> mWriteQueue;	// Queue to store write data.
	double mLastVideoPos;
	double mStoreEndPosition; 		/**< Last reported TSB Store end position*/
	double mLiveEndPosition;		/**< Last reported Live end position*/

public:
	PrivateInstanceAAMP *mAamp; /**< AAMP player's private instance */
	std::shared_ptr<IsoBmffHelper> mIsoBmffHelper; /**< ISO BMFF helper object */
};

#endif // AAMP_TSBSSESSIONMANAGER_H
