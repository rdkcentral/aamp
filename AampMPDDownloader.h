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

/**************************************
* @file AampMPDDownloader.h
* @brief MPD Downloader for Aamp
**************************************/

#ifndef __AAMP_MPD_DOWNLOADER_H__
#define __AAMP_MPD_DOWNLOADER_H__

#include <stdint.h>
#include <unordered_map>
#include <vector>
#include <iterator>
#include <algorithm>
#include <map>
#include <string>
#include <mutex>
#include <queue>
#include <sys/time.h>
#include <iostream>
#include <string>
#include <curl/curl.h>
#include <chrono>
#include <condition_variable> // std::condition_variable, std::cv_status
#include <memory>
#include <string>
#include <atomic>
#include <stdint.h>
#include "libdash/IMPD.h"
#include "libdash/INode.h"
#include "libdash/IDASHManager.h"
#include "libdash/IProducerReferenceTime.h"
#include "libdash/xml/Node.h"
#include "libdash/helpers/Time.h"
#include "libdash/xml/DOMParser.h"
#include <libxml/xmlreader.h>
#include <thread>
#include "AampCurlDefine.h"
#include "AampCurlDownloader.h"
#include "AampDefine.h"
#include "AampLogManager.h"
#include "AampMPDParseHelper.h"
#include "AampCMCDCollector.h"
#include "dash/mpd/MPDModel.h"
#include "dash/mpd/MPDSegmenter.h"
#include "AampLLDASHData.h"
#include "AampMPDUtils.h"

typedef void (*ManifestUpdateCallbackFunc)(void *);

/**
 * @struct _manifestDownloadConfig
 * @brief structure to store the download configuration
 */
typedef struct _manifestDownloadConfig
{
	DownloadConfigPtr mDnldConfig;
	std::string mTuneUrl; 		// url used for tuning the stream
	std::string mStichUrl; 		// url mpd to be stitched before the tuneUrl
	MPDStichOptions	mMPDStichOption;
	bool mIsLLDConfigEnabled;
	bool mCullManifestAtTuneStart;	// Remove the Start of the Manifest to the liveOffset
	int  mTSBDuration;			// pass the TSB duration of the manifest to be managed
	int  mStartPosnToTSB;		// Position where MPD has to be truncated at the start of playback

	int mHarvestCountLimit;			/**< Harvest count */
	int mHarvestConfig;				/**< Harvest config */
	std::string mHarvestPathConfigured;    // Harvest Path
	AampCMCDCollector* mCMCDCollector; // new variable for cmcd header collector
	std::string mPreProcessedManifest; // provided pre-processed manifest file
	int mPlayerId;


	_manifestDownloadConfig( int playerId ) :mDnldConfig(std::make_shared<DownloadConfig> ()),mTuneUrl(),mStichUrl(),
									mIsLLDConfigEnabled(false),	mCullManifestAtTuneStart(false),mTSBDuration(-1),
									mStartPosnToTSB(-1),mCMCDCollector(nullptr),mMPDStichOption(OPT_1_FULL_MANIFEST_TUNE),
									mHarvestCountLimit(0),mHarvestConfig(0),mHarvestPathConfigured(),mPreProcessedManifest(),mPlayerId(playerId) {}

	_manifestDownloadConfig(const _manifestDownloadConfig& other): mDnldConfig(other.mDnldConfig),mTuneUrl(other.mTuneUrl),
								mStichUrl(other.mStichUrl),mIsLLDConfigEnabled(other.mIsLLDConfigEnabled),
								mCullManifestAtTuneStart(other.mCullManifestAtTuneStart), mTSBDuration(other.mTSBDuration),
								mStartPosnToTSB(other.mStartPosnToTSB),mCMCDCollector(other.mCMCDCollector),
								mMPDStichOption(other.mMPDStichOption),mHarvestCountLimit(other.mHarvestCountLimit),
								mHarvestConfig(other.mHarvestConfig),mHarvestPathConfigured(other.mHarvestPathConfigured),mPreProcessedManifest(other.mPreProcessedManifest),mPlayerId(other.mPlayerId) {}


	_manifestDownloadConfig& operator=(const _manifestDownloadConfig& other)
	{
		_manifestDownloadConfig temp(other);
		std::swap(*this, temp);
		return *this;
	}
	~_manifestDownloadConfig()
	{
		mDnldConfig		=	NULL;
	}

public:
	void show();
}ManifestDownloadConfig;

/**
 * @struct _manifestDownloadResponse
 * @brief structure to store the downloaded ManifestData
 */
typedef struct _manifestDownloadResponse
{
	DownloadResponsePtr mMPDDownloadResponse;
	std::shared_ptr<dash::mpd::IMPD> mMPDInstance;
	bool mIsLiveManifest;
	bool mRefreshRequired;
	AAMPStatusType mMPDStatus;
	Node *mRootNode;
	std::shared_ptr<DashMPDDocument> mDashMpdDoc;
	uint64_t mLastPlaylistDownloadTimeMs; // Last playlist refresh time
private:
	AampMPDParseHelperPtr	mMPDParseHelper;

public:
	_manifestDownloadResponse() : mMPDDownloadResponse(std::make_shared<DownloadResponse>()), mMPDInstance(nullptr), mIsLiveManifest(false), mRefreshRequired(false), mMPDStatus(AAMPStatusType::eAAMPSTATUS_OK), mRootNode(NULL), mDashMpdDoc(nullptr), mLastPlaylistDownloadTimeMs(0), mMPDParseHelper(std::make_shared<AampMPDParseHelper>()) {}

	_manifestDownloadResponse& operator=(const _manifestDownloadResponse& other)
	{
		_manifestDownloadResponse temp(other);
		std::swap(*this, temp);
		return *this;
	}

	~_manifestDownloadResponse();

	_manifestDownloadResponse(const _manifestDownloadResponse& other)
	: mMPDDownloadResponse(other.mMPDDownloadResponse),
	  mMPDInstance(other.mMPDInstance),
	  mIsLiveManifest(other.mIsLiveManifest),
	  mMPDStatus(other.mMPDStatus),
	  mRootNode(other.mRootNode),
	  mRefreshRequired(other.mRefreshRequired),
	  mDashMpdDoc(other.mDashMpdDoc),
	  mMPDParseHelper(std::make_shared<AampMPDParseHelper>(*(other.mMPDParseHelper))), // Copy the content
	  mLastPlaylistDownloadTimeMs(other.mLastPlaylistDownloadTimeMs){}


public:
	/**
	 * @brief Displays the manifest download response.
	 */
	void show();
	/**
	 * @brief Retrieves the headers from the manifest download response.
	 */
	std::vector<std::string> GetManifestDownloadHeaders() { return mMPDDownloadResponse->mResponseHeader;	}
	/**
	 * @brief Parses the MPD document.
	 */
	void parseMpdDocument();
	/**
	 *   @fn parseMPD
	 *   @brief parseMPD function to parse the downloaded MPD file
	 */
	void parseMPD();
	/**
	 * @brief Creates a clone of the manifest download response.
	 *
	 * @return A shared pointer to the cloned manifest download response.
	 */
	std::shared_ptr<_manifestDownloadResponse> clone();
	/**
	 * @brief Retrieves the MPD parse helper.
	 *
	 * @return A shared pointer to the AampMPDParseHelper.
	 */
	AampMPDParseHelperPtr 	GetMPDParseHelper() { return mMPDParseHelper;}
}ManifestDownloadResponse;

typedef std::shared_ptr<ManifestDownloadResponse> ManifestDownloadResponsePtr;
#define MakeSharedManifestDownloadResponsePtr std::make_shared<ManifestDownloadResponse>

typedef std::shared_ptr<ManifestDownloadConfig> ManifestDownloadConfigPtr;


class AampMPDDownloader
{
public:
	/**
	*   @fn AampMPDDownloader
	*   @brief Default Constructor
	*/
	AampMPDDownloader();
	/**
	*   @fn ~AampMPDDownloader
	*   @brief  Destructor
	*/
	~AampMPDDownloader();

	/**
	*	@fn Initialize
	*	@brief Function to initialize MPD Downloader
	*/
	void Initialize(ManifestDownloadConfigPtr mpdDnldCfg, std::string appName="",std::function<std::string()> mpdPreProcessFuncptr = nullptr);

	/**
	*	@fn Release
	*	@brief Function to clear/release all the allocation for MPD Downloader
	*/
	void Release();

	/**
	*	@fn Start
	*	@brief Function to start the download based on the Initialization set
	*/
	void Start();

	/**
	*	@fn Pause
	*	@brief Function to Pause the live refresh / MPD downloader
	*/
	void Pause();
	/**
	*	@fn GetManifest
	*	@brief Function to Get the Manifest data from MPD Downloader
	*/
	ManifestDownloadResponsePtr GetManifest(bool bWait=true , int iWaitDuration=50, int errorSimulation=-1);
	/**
	*	@fn SetNetworkTimeout
	*	@brief Function to Set NetworkTimeout in Sec
	*/
	void SetNetworkTimeout(uint32_t iDurationSec);
	/**
	*	@fn SetStallTimeout
	*	@brief Function to Set SetStallTimeout in Sec
	*/
	void SetStallTimeout(uint32_t iDurationSec);
	/**
	*	@fn SetStartTimeout
	*	@brief Function to Set StartTimeout in Sec
	*/
	void SetStartTimeout(uint32_t iDurationSec);
	/**
	*	@fn SetBufferAvailability
	*	@brief Function to Set Buffer Value ( needed for Manifest refresh)
	*/
	void SetBufferAvailability(int iLatencyVal);
	/**
	*	@fn GetEffectiveUrl
	*	@brief Function to Get Effective Url
	*/
	std::string GetEffectiveUrl();
	/**
	*	@fn GetManifestError
	*	@brief Function to Get Manifest Download/Parse error in String format
	*/
	std::string GetManifestError();
	/**
	 * @fn RegisterCallback
	 * @brief Registers a callback function for manifest update notifications.
	 */
	void RegisterCallback(ManifestUpdateCallbackFunc fnPtr, void *);
	/**
	 * @fn UnRegisterCallback
	 * @brief Unregister the callback function for manifest update notifications.
	 */
	void UnRegisterCallback();
	/**
	 * @fn IsMPDLowLatency
	 * @brief Checks if the MPD has low latency mode enabled.
	 */
	bool IsMPDLowLatency(AampLLDashServiceData &LLDashData);
	/**
	 * @fn IsDownloaderDisabled
	 * @brief Return the Downloader disabled status
	 */
	bool IsDownloaderDisabled() {return mReleaseCalled;}
	/**
	 * @fn GetLastDownloadMPDSize
	 * @brief Function to last downloaded manifest data
	 */
	void GetLastDownloadedManifest(std::string& manifestBuffer);

	//copy constructor
	AampMPDDownloader(const AampMPDDownloader&)=delete;
	//copy assignment operator
	AampMPDDownloader& operator=(const AampMPDDownloader&) = delete;

	/**
	 * @fn SetCurrentPositionDeltaToManifestEnd
	 * @brief function to set the delta between current position and manifestEnd
	 */
	void SetCurrentPositionDeltaToManifestEnd(int delta) {mCurrentposDeltaToManifestEnd = delta;}

	/*
	 * @fn GetPublishTime
	 * @brief function to get the manifest publish time
	 * @return publish time in milliseconds
	 */
	uint64_t GetPublishTime() { return mPublishTime;}

private:

	/**
	*	@fn getManifestRefreshWait
	*	@brief Function to calculate manifest refresh rate
	*/
	int getManifestRefreshWait();
	/**
	*	@fn sleepForTimeBeforeNextRefresh
	*	@brief Function to sleep for the duration passed and get interrupted based on the
	*	cond variable trigger
	*/
	void sleepForTimeBeforeNextRefresh(int iWaitIntervalSec);
	/**
	*	@fn decodeManifestError
	*	@brief Function to decode Manifest Error to String
	*/
	void decodeManifestError(AAMPStatusType eMPDErr);
	/**
	*	@fn downloadMPD
	*	@brief Function to download manifest file
	*/
	void downloadMPD();
	/**
	*	@fn downloadMPDThread1
	*	@brief Thread Function to download manifest file and refresh it if live
	*/
	void downloadMPDThread1();

	/**
	*	@fn downloadNotifierThread
	*	@brief Thread Function to notify the registered user fo manifest refresh. This will avoid any delays in
	*			main loop function
	*/
	void downloadNotifierThread();
	/**
	*	@fn readMPDData
	*	@brief Function to parse the downloaded manifest response from curl downloader
	*/
	bool readMPDData(ManifestDownloadResponsePtr mMPD);
	/**
	*	@fn waitForRefreshInterval
	*	@brief Function to wait for refresh interval before next download
	*/
	bool waitForRefreshInterval();
	/**
	*	@fn pushDownloadDataToQueue
	*	@brief Function to push the download MPD to Queue for collector to read it
	*/
	void pushDownloadDataToQueue();
	/**
	*	@fn showDownloadMetrics
	*	@brief Function to show download Metrics
	*/
	void showDownloadMetrics(DownloadResponsePtr dnldPtr, int totalPerformanceTime);
	/**
	*	@fn stichToCachedManifest
	*	@brief Function called to Stich the cached manifest with downloaded manifest
	*/
	void stichToCachedManifest(ManifestDownloadResponsePtr mpdToAppend);
	/**
	*	@fn isMPDLowLatency
	*	@brief Function to parse the manifest and check if DASH Low latency is supported in the manifest and read parameters
	*/
	bool isMPDLowLatency(ManifestDownloadResponsePtr mMPD, AampLLDashServiceData &LLDashData);
	/**
	*	@fn getMeNextManifestDownloadWaitTime
	*	@brief Function to calculate the download refresh interval
	*/
	uint32_t getMeNextManifestDownloadWaitTime(ManifestDownloadResponsePtr mMPD);
	/**
	*	@fn GetCMCDHeader
	*	@brief Function to get CMCD Headers to pack during download
	*/
	std::unordered_map<std::string, std::vector<std::string>> getCMCDHeader();
	/**
	*	@fn harvestManifest
	*	@brief Function to harvest the downloaded manifest
	*/
	void harvestManifest();
private:

	std::queue<ManifestDownloadResponsePtr> mMPDBufferQ;
	uint32_t mMPDBufferSize; // maximum size of buffer
	std::mutex mMPDBufferMutex; // mutex to protect buffer

	std::recursive_mutex mMPDDnldMutex;
	// Download network configuration
	ManifestDownloadConfigPtr mMPDDnldCfg;
	// Download data
	ManifestDownloadResponsePtr mMPDData;
	ManifestDownloadResponsePtr mCachedMPDData;

	uint32_t mRefreshInterval ; 		// refresh interval in mSec
	int mLatencyValue;			// buffer value to be considered for manifest refresh

	std::thread mDownloaderThread_t1;
	std::thread mDownloaderThread_t2;
	std::thread mDownloadNotifierThread;

	AampCurlDownloader mDownloader1;
	AampCurlDownloader mDownloader2;

	std::mutex mRefreshMtx;
	std::condition_variable mRefreshCondVar;

	std::mutex mMPDDnldDataMtx;
	std::condition_variable mMPDDnldDataCondVar;

	std::mutex mMPDNotifierMtx;
	std::condition_variable mMPDNotifierCondVar;

	bool mReleaseCalled;
	bool mCheckedLLDData;
	std::string mAppName;
	ManifestUpdateCallbackFunc mManifestUpdateCb;
	void *mManifestUpdateCbArg;

	unsigned int mManifestRefreshCount; 	/**< counter which keeps the count of manifest/Playlist success refresh */

	bool mIsLowLatency;  /**< Flag indicating whether it is a low latency stream or not.*/
	AampLLDashServiceData mLLDashData; /**< Parsed LLDash Data*/
	int mCurrentposDeltaToManifestEnd; /* Delta between current pos and ManifestEnd */
	uint64_t mPublishTime; 		   /* Publish time of updated manifest*/
	int mMinimalRefreshRetryCount;  /* A counter to checks if the publication time remains the same for 2 consecutive refresh*/
	std::atomic_bool mMPDNotifyPending ; /*To allow wait for downloadNotifier based on NotifyPending Status */
	std::function<std::string()> mMpdPreProcessFuncptr; /* function invoked to read the available preprocessed manifest data or to send event if manifest data is not available */
};

#endif /* __AAMP_MPD_DOWNLOADER_H__ */
