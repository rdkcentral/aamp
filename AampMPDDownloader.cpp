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
* @file AampMPDDownloader.cpp
* @brief MPD Downloader for Aamp
**************************************/


#include "AampCurlDownloader.h"
#include "AampMPDDownloader.h"
#include "AampUtils.h"
#include "AampLogManager.h"
#include <inttypes.h>



#define DEFAULT_INTERVAL_BETWEEN_MPD_UPDATES_MS 3000

void _manifestDownloadResponse::show()
{
	mMPDDownloadResponse->show();
	AAMPLOG_INFO("IsLive : %d", mIsLiveManifest);
	AAMPLOG_INFO("MPD RetStatus : %d", mMPDStatus);
	if(mMPDInstance)
	{
		if(mMPDInstance->GetLocations().size())
		{
			 AAMPLOG_INFO("location: %s", mMPDInstance->GetLocations()[0].c_str());
		}
		AAMPLOG_INFO("type: %s", mMPDInstance->GetType().c_str());
		auto periods = mMPDInstance->GetPeriods();
		AAMPLOG_INFO("Size of 'periods': %zu", periods.size());
		AAMPLOG_INFO("Minimum Update Period:  %s", mMPDInstance->GetMinimumUpdatePeriod().c_str());
	}
}

 _manifestDownloadResponse::~_manifestDownloadResponse()
{
	if(mRootNode != NULL)
	{
		SAFE_DELETE(mRootNode);
	}

	mMPDDownloadResponse	=	NULL;
	mDashMpdDoc	=	NULL;

}

void _manifestDownloadResponse::parseMpdDocument()
{
	AAMPLOG_TRACE("Enter");
	if(mDashMpdDoc == nullptr)
	{
		mDashMpdDoc = std::make_shared<DashMPDDocument>(mMPDDownloadResponse->getString());
	}
	AAMPLOG_TRACE("Exit");
}

std::shared_ptr<_manifestDownloadResponse> _manifestDownloadResponse::clone()
{
	AAMPLOG_TRACE("Enter");
	std::shared_ptr<_manifestDownloadResponse> clonedDoc = std::make_shared<_manifestDownloadResponse>(*this);
	if(this->mDashMpdDoc)
	{
		clonedDoc->mDashMpdDoc = this->mDashMpdDoc->clone(true);
	}
	clonedDoc->mMPDDownloadResponse = std::make_shared<DownloadResponse>(*this->mMPDDownloadResponse);
	clonedDoc->mMPDDownloadResponse->mDownloadData = mMPDDownloadResponse->mDownloadData;
	clonedDoc->mMPDParseHelper = std::make_shared<AampMPDParseHelper>(*this->mMPDParseHelper);
	clonedDoc->mRootNode = NULL;
	clonedDoc->parseMPD();
	AAMPLOG_TRACE("Exit");
	return clonedDoc;
}

/**
*   @fn parseMPD
*   @brief parseMPD function to parse the downloaded MPD file
*/
void _manifestDownloadResponse::parseMPD()
{
	std::string manifestStr;	
	xmlTextReaderPtr mXMLReader =   NULL; // Initialize to nullptr

	if(this->mMPDDownloadResponse)
	{
		manifestStr = mMPDDownloadResponse->getString();
	}

	if(!manifestStr.empty())
	{
		// Parse the MPD and create mpd object ;
		uint32_t fetchTime = Time::GetCurrentUTCTimeInSec();

		mXMLReader = xmlReaderForMemory( (char *)manifestStr.c_str(), (int) manifestStr.length(), NULL, NULL, 0);
		if (mXMLReader != NULL)
		{
			int retStatus = xmlTextReaderRead(mXMLReader);
			if (retStatus == 1)
			{
				if (mRootNode)
				{
					SAFE_DELETE(mRootNode);
				}

				mRootNode = MPDProcessNode(&mXMLReader, mMPDDownloadResponse->sEffectiveUrl);
				if(mRootNode != NULL)
				{
					MPD *mpd = mRootNode->ToMPD();
					if (mpd)
					{
						mpd->SetFetchTime(fetchTime);
						std::shared_ptr<dash::mpd::IMPD> tmp_ptr(mpd);
						mMPDInstance		=	tmp_ptr;
						mMPDStatus 		= 	AAMPStatusType::eAAMPSTATUS_OK;
						mMPDParseHelper->Initialize(mpd);
					}
					else
					{
						mMPDStatus = AAMPStatusType::eAAMPSTATUS_MANIFEST_CONTENT_ERROR;
					}
				}
				else if (mRootNode == NULL)
				{
					mMPDStatus = AAMPStatusType::eAAMPSTATUS_MANIFEST_PARSE_ERROR;
				}
			}
			else if (retStatus == -1)
			{
				mMPDStatus = AAMPStatusType::eAAMPSTATUS_MANIFEST_PARSE_ERROR;
			}
		}
	}
	if(mXMLReader != NULL)
	{
		xmlFreeTextReader(mXMLReader);
		mXMLReader = NULL;
	}

	AAMPLOG_INFO("Parse MPD Completed ...");
}

/**
*   @fn AampMPDDownloader
*   @brief Default Constructor
*/
AampMPDDownloader::AampMPDDownloader() :  mMPDBufferQ(),mMPDBufferSize(1),mMPDBufferMutex(),mRefreshMtx(),mRefreshCondVar(),
	mMPDDnldMutex(),mRefreshInterval(DEFAULT_INTERVAL_BETWEEN_PLAYLIST_UPDATES_MS),mLatencyValue(-1),mReleaseCalled(true),
	mMPDDnldCfg(NULL),mDownloaderThread_t1(),mDownloaderThread_t2(),mDownloader1(),mDownloader2(),mMPDData(nullptr),mAppName(""),
	mManifestUpdateCb(NULL),mManifestUpdateCbArg(NULL),mDownloadNotifierThread(),mCachedMPDData(nullptr),
	mCheckedLLDData(false),mMPDNotifierMtx(),mMPDNotifierCondVar(),mManifestRefreshCount(0),mIsLowLatency(false),
	mMPDDnldDataMtx(),mMPDDnldDataCondVar()
	,mLLDashData(),mCurrentposDeltaToManifestEnd(-1),mPublishTime(0),mMinimalRefreshRetryCount(0),mMPDNotifyPending(false)
{
}

/**
*   @fn AampMPDDownloader
*   @brief Destructor
*/
AampMPDDownloader::~AampMPDDownloader()
{
	// Clear the queue and release all the objects
	Release();
	// reset the pointers , its shared pointer, it will released automatically
	mMPDData	=	nullptr;
	mMPDDnldCfg	=	NULL;
	mCachedMPDData	=	nullptr;
}

/**
*   @fn Initialize
*   @brief Initialize with MPD Download Input
*/
void AampMPDDownloader::Initialize(ManifestDownloadConfigPtr mpdDnldCfg, std::string appName,std::function<std::string()> mpdPreProcessFuncptr)
{
	if(mpdDnldCfg == nullptr)
	{
		AAMPLOG_INFO("Need a valid MPD download config.");
		return;
	}

	mAppName	=	appName;

	// Release and reset and previously called values
	// Initialize to be called only once . If repeatedly called , then stored vars will be
	// reset
	Release();
	mReleaseCalled = false;

	std::lock_guard<std::recursive_mutex> lock(mMPDDnldMutex);
	mMPDDnldCfg = mpdDnldCfg;

	if(mpdPreProcessFuncptr)
	{
		mMpdPreProcessFuncptr = mpdPreProcessFuncptr;
	}

}

/**
*   @fn SetNetworkTimeout
*   @brief Set Network Timeout for Manifest download
*/
void AampMPDDownloader::SetNetworkTimeout(uint32_t iDurationSec)
{
	std::lock_guard<std::recursive_mutex> lock(mMPDDnldMutex);
	if(mMPDDnldCfg && iDurationSec > 0)
	{
		mMPDDnldCfg->mDnldConfig->iDownloadTimeout = iDurationSec;
	}
}
/**
*   @fn SetStallTimeout
*   @brief Set Stall Timeout for Manifest download
*/
void AampMPDDownloader::SetStallTimeout(uint32_t iDurationSec)
{
	std::lock_guard<std::recursive_mutex> lock(mMPDDnldMutex);
	if(mMPDDnldCfg)
	{
		mMPDDnldCfg->mDnldConfig->iStallTimeout = iDurationSec;
	}
}
/**
*   @fn SetStartTimeout
*   @brief Set Start Timeout for Manifest download
*/
void AampMPDDownloader::SetStartTimeout(uint32_t iDurationSec)
{
	std::lock_guard<std::recursive_mutex> lock(mMPDDnldMutex);
	if(mMPDDnldCfg)
	{
		mMPDDnldCfg->mDnldConfig->iStartTimeout = iDurationSec;
	}
}
/**
*   @fn SetBufferAvailability
*   @brief Set Buffer Value to calculate the manifest refresh rate
*/
void AampMPDDownloader::SetBufferAvailability(int iDurationMilliSec)
{
	std::lock_guard<std::recursive_mutex> lock(mMPDDnldMutex);
	mLatencyValue	=	iDurationMilliSec;
}

/**
*   @fn Release
*   @brief Release function to clear the allocation and join threads
*/
void AampMPDDownloader::Release()
{
	AAMPLOG_INFO("Release Called in MPD Downloader");

	if(!mReleaseCalled)
	{
		{
			std::lock_guard<std::recursive_mutex> lock(mMPDDnldMutex);
			mReleaseCalled = true;
			mRefreshCondVar.notify_all();
			mMPDDnldDataCondVar.notify_all();
			mMPDNotifierCondVar.notify_all();

		}

		mDownloader1.Release();
		mDownloader2.Release();

		if(mDownloaderThread_t1.joinable())
			mDownloaderThread_t1.join();

		if(mDownloaderThread_t2.joinable())
			mDownloaderThread_t2.join();

		if(mManifestUpdateCb != NULL)
		{
			UnRegisterCallback();
		}
		if(mDownloadNotifierThread.joinable())
			mDownloadNotifierThread.join();
		while (!mMPDBufferQ.empty())
		{
			mMPDBufferQ.pop();
		}

		/**< Reset LLD Data*/
		mLLDashData.clear();
		mMinimalRefreshRetryCount = 0; //Reset the refresh interval retry counter
		AAMPLOG_INFO("Release Called in MPD Downloader - Exit %ld %ld", mMPDData.use_count(),mMPDDnldCfg.use_count());

	}
}

/**
*   @fn Start
*   @brief Start the Manifest downloader
*/
void AampMPDDownloader::Start()
{
 	std::lock_guard<std::recursive_mutex> lock(mMPDDnldMutex);
	// Start the thread to initiate the download of manifest
	if(mMPDDnldCfg && !mMPDDnldCfg->mTuneUrl.empty())
	{
		// start the download of tune url
		try
		{
			mDownloaderThread_t1 = std::thread(&AampMPDDownloader::downloadMPDThread1, this);
			AAMPLOG_INFO("Thread created for MPD Downloader1 [%zx]", GetPrintableThreadID(mDownloaderThread_t1));
		}
		catch(std::exception &e)
		{
			AAMPLOG_WARN("Thread create failed for MPD Downloader1 : %s", e.what());
		}
	}
	else
	{
		AAMPLOG_ERR("No Tune Url provided for download ");
	}
}

/**
*   @fn downloadMPDThread1
*   @brief downloadMPDThread1 thread function to download the Manifest 1
*/
void AampMPDDownloader::downloadMPDThread1()
{
	UsingPlayerId playerId(mMPDDnldCfg->mPlayerId);
	bool refreshNeeded = false;
	std::string tuneUrl = mMPDDnldCfg->mTuneUrl;
	bool firstDownload	=	true;
	ManifestDownloadResponsePtr cachedBackupData = nullptr;
	do
	{
		std::unordered_map<std::string, std::vector<std::string>> Headers = mMPDDnldCfg->mDnldConfig->sCustomHeaders;
		bool doPush = true;
		long long tStartTime = NOW_STEADY_TS_MS;
		{
			std::lock_guard<std::recursive_mutex> lock(mMPDDnldMutex);
			if(mReleaseCalled)
				break;
			if(mMPDDnldCfg->mCMCDCollector)
			{
				std::unordered_map<std::string, std::vector<std::string>> CMCDHeaders = getCMCDHeader();
				Headers.insert(CMCDHeaders.begin(), CMCDHeaders.end());
			}
			mMPDDnldCfg->mDnldConfig->sCustomHeaders = Headers;
			mMPDDnldCfg->mDnldConfig->iDownload502RetryCount = MANIFEST_DOWNLOAD_502_RETRY_COUNT;
			mDownloader1.Initialize(mMPDDnldCfg->mDnldConfig);
			refreshNeeded = false;
			//mDownloader1.Clear();
			AAMPLOG_INFO("aamp url:%d,%d,%d,%f,%s", eMEDIATYPE_TELEMETRY_MANIFEST, eMEDIATYPE_MANIFEST,eCURLINSTANCE_VIDEO,0.000000, tuneUrl.c_str());
			mMPDData = MakeSharedManifestDownloadResponsePtr();
		}
		//If Manifest data already provided use it ,not required to download the Manifest
		if (!mMPDDnldCfg->mPreProcessedManifest.empty())
		{
			AAMPLOG_WARN("PreProcessed manifest provided");
			mMPDData->mMPDDownloadResponse->replaceDownloadData(mMPDDnldCfg->mPreProcessedManifest);
			mMPDData->mMPDDownloadResponse->iHttpRetValue = 200;
			mMPDData->mMPDDownloadResponse->sEffectiveUrl.assign(tuneUrl);
			mMPDDnldCfg->mPreProcessedManifest.clear();
		}
		else
		{
			if( NULL != mMpdPreProcessFuncptr)
			{
				std::string updatedManifest = mMpdPreProcessFuncptr();
				if(!updatedManifest.empty())
				{
					mMPDData->mMPDDownloadResponse->replaceDownloadData(updatedManifest);
					mMPDData->mMPDDownloadResponse->iHttpRetValue = 200;
				}
				else
				{
					mMPDData->mMPDDownloadResponse->iHttpRetValue = CURLE_OPERATION_TIMEDOUT;
				}
			}
			else
			{
				mDownloader1.Download(tuneUrl, mMPDData->mMPDDownloadResponse);
			}
		}

		if(mMPDData->mMPDDownloadResponse->curlRetValue == 0 && IS_HTTP_SUCCESS(mMPDData->mMPDDownloadResponse->iHttpRetValue))
		{
			if(!mMPDData->mMPDDownloadResponse->getString().empty())
			{
				//std::string dataStr =  std::string( mMPDData->mMPDDownloadResponse->mDownloadData.begin(), mMPDData->mMPDDownloadResponse->mDownloadData.end());
				//mMPDData->show();
				// store the last manifestdownloadTime
				mMPDData->mLastPlaylistDownloadTimeMs	=	aamp_GetCurrentTimeMS();
				mMPDData->parseMPD();
				if(firstDownload)
				{
					// Check for LLD Manifest for first manifest download only . This is needed to determine the refresh parameters
					mIsLowLatency = isMPDLowLatency(mMPDData, mLLDashData);
				}

				if(mMPDData->mMPDStatus == AAMPStatusType::eAAMPSTATUS_OK)
				{
					doPush = readMPDData(mMPDData);
				}
				AAMPLOG_INFO("Successfully parsed Manifest ...IsLive[%d]",mMPDData->mIsLiveManifest);

				// Update the effective url , so that next refresh uses the effective url
				tuneUrl = mMPDData->mMPDDownloadResponse->sEffectiveUrl;

				// first time download complete . Do what need to be done . ....
				if(firstDownload && mMPDData->mIsLiveManifest)
				{
					// For very first tune , if mpd need to be truncated ( Cloud TSB Support )
					if(mMPDDnldCfg->mCullManifestAtTuneStart)
					{
						// Cull mMPDData to mStartPosnToTSB
						//truncateMPDStartPosition();
					}

					if(!mMPDDnldCfg->mStichUrl.empty())
					{
						mCachedMPDData	=	mMPDData;
						tuneUrl	=	mMPDDnldCfg->mStichUrl;
						AAMPLOG_INFO("Update the Cached MPD Data. New URL:%s ", tuneUrl.c_str());
					}
					firstDownload	=	 false;
				}
				else
				{
					// Stich API to merge two manifest
					if(cachedBackupData != nullptr)
					{
						mCachedMPDData = cachedBackupData;
						stichToCachedManifest(mMPDData);
					}
				}
			}
			else
			{
				AAMPLOG_INFO("Ignoring MPD processing for empty manifest, Response Code : %d..!", mMPDData->mMPDDownloadResponse->iHttpRetValue);
			}
		}
		else
		{
			// Failure in request
			std::string mEffectiveUrl = mMPDData->mMPDDownloadResponse->sEffectiveUrl;	//Effective URL could be different than tuneURL after redirection

			if(mEffectiveUrl.empty())
			{
				mEffectiveUrl = mMPDDnldCfg->mTuneUrl;
			}

			AAMPLOG_ERR("curl request %s %s Error Code [%u]",mEffectiveUrl.c_str(), (mMPDData->mMPDDownloadResponse->iHttpRetValue < 100) ? "Curl" : "HTTP", mMPDData->mMPDDownloadResponse->iHttpRetValue);

			mMPDData->mMPDStatus	=	AAMPStatusType::eAAMPSTATUS_MANIFEST_DOWNLOAD_ERROR;
			if(mMPDData->mMPDDownloadResponse->iHttpRetValue != 200 && mMPDData->mMPDDownloadResponse->iHttpRetValue != 204 && mMPDData->mMPDDownloadResponse->iHttpRetValue != 206)
			{ 
				AampLogManager::LogNetworkError (mEffectiveUrl.c_str(), AAMPNetworkErrorHttp, mMPDData->mMPDDownloadResponse->iHttpRetValue, eMEDIATYPE_MANIFEST);
				//Use DownloadResponse Show call instead of printheaderresponse fn -since it is not scope
				mMPDData->mMPDDownloadResponse->show();
			}
		}
		long long tEndTime = NOW_STEADY_TS_MS;
		showDownloadMetrics(mMPDData->mMPDDownloadResponse, (int)(tEndTime - tStartTime));
		if(doPush)
		{
			// Push the output to Queue for Consumer to take
			pushDownloadDataToQueue();
		}
		// harvest downloaded manifest 
		harvestManifest();

		if(mCachedMPDData != nullptr)
		{
			cachedBackupData = mCachedMPDData->clone();
			AAMPLOG_TRACE("Created copy of cached:%p backup:%p", mCachedMPDData.get(), cachedBackupData.get());
			AAMPLOG_TRACE("Created copy of cachedMPD:%p backupMPD:%p", mCachedMPDData->mDashMpdDoc.get(), cachedBackupData->mDashMpdDoc.get());
			AAMPLOG_TRACE("Created copy of cachedDwnResp:%p backupDwnldResp:%p", mCachedMPDData->mMPDDownloadResponse.get(), cachedBackupData->mMPDDownloadResponse.get());
			AAMPLOG_TRACE("Created copy of cachedMPDInst:%p backupMPDInst:%p", mCachedMPDData->mMPDInstance.get(), cachedBackupData->mMPDInstance.get());
		}
		//Wait for duration before refresh
		if(mMPDData->mIsLiveManifest && !mReleaseCalled)
		{
			refreshNeeded = waitForRefreshInterval();
		}

		//Timeout case during live refresh
		if(!firstDownload && (CURLE_OPERATION_TIMEDOUT == mMPDData->mMPDDownloadResponse->iHttpRetValue  || CURLE_COULDNT_CONNECT == mMPDData->mMPDDownloadResponse->iHttpRetValue))
		{
			AAMPLOG_WARN("Refresh every 500ms to handle a manifest timeout error.");
			//Forcefully go with 500 ms refresh
			mRefreshInterval = MIN_DELAY_BETWEEN_PLAYLIST_UPDATE_MS;
			refreshNeeded = waitForRefreshInterval();
		}

	}while(refreshNeeded && !mReleaseCalled);
	AAMPLOG_INFO("Out of Manifest Download loop ...");
}

/**
 * @brief Harvest the manifest.
 *
 * This function is responsible for harvesting the manifest file to the specified harvest path
 * 
 * return void
 */
void AampMPDDownloader::harvestManifest()
{
	if(mMPDData->mMPDDownloadResponse->curlRetValue == 0 && 
		(mMPDData->mMPDDownloadResponse->iHttpRetValue == 200 || mMPDData->mMPDDownloadResponse->iHttpRetValue == 206))
	{
			AampMediaType mediaType	=	eMEDIATYPE_MANIFEST	;
			if((mMPDDnldCfg->mHarvestCountLimit > 0) && (mMPDDnldCfg->mHarvestConfig & getHarvestConfigForMedia(mediaType)))
			{
				/* Avoid chance of overwriting , in case of manifest and playlist, name will be always same */
				mManifestRefreshCount++;
				AAMPLOG_WARN("aamp harvestCountLimit: %d mManifestRefreshCount %d", mMPDDnldCfg->mHarvestCountLimit,mManifestRefreshCount);
				std::string harvestPath = mMPDDnldCfg->mHarvestPathConfigured;
				if(harvestPath.empty() )
				{
					getDefaultHarvestPath(harvestPath);
					AAMPLOG_WARN("Harvest path has not configured, taking default path %s", harvestPath.c_str());
				}
				std::string dataStr =  mMPDData->mMPDDownloadResponse->getString(); 
				if(!dataStr.empty() )
				{
					if(aamp_WriteFile(mMPDData->mMPDDownloadResponse->sEffectiveUrl, dataStr.c_str(),(int) dataStr.length(), mediaType, mManifestRefreshCount,harvestPath.c_str()))
					{
						mMPDDnldCfg->mHarvestCountLimit--;
					}
				}  //CID:168113 - forward null
			}
	}
}


void AampMPDDownloader::stichToCachedManifest(ManifestDownloadResponsePtr mpdToAppend)
{
	// check if any big manifest already downloaded ,
	// If downloaded only , stich the current one to that , if not ignore
	AAMPLOG_INFO("Stitching [%s] to [%s]",mMPDDnldCfg->mTuneUrl.c_str(), mMPDDnldCfg->mStichUrl.c_str());
	if(mCachedMPDData != nullptr)
	{
		// call API to Merge
		if(mCachedMPDData->mDashMpdDoc == nullptr)
		{
			mCachedMPDData->parseMpdDocument();
		}
		if(mpdToAppend->mDashMpdDoc == nullptr)
		{
			mpdToAppend->parseMpdDocument();
		}

		if(mCachedMPDData->mDashMpdDoc && mpdToAppend->mDashMpdDoc)
		{
			mCachedMPDData->mDashMpdDoc->mergeDocuments(mpdToAppend->mDashMpdDoc);
			std::string manifestData = mCachedMPDData->mDashMpdDoc->toString();
			mCachedMPDData->mMPDDownloadResponse->mDownloadData.clear();
			mCachedMPDData->mMPDDownloadResponse->mDownloadData = std::vector<uint8_t>(manifestData.begin(), manifestData.end());
			mCachedMPDData->parseMPD();
		}
	}
}

/**
* @fn showDownloadMetrics - fn to log downloadresponse
* @params DownloadResponse pointer ,totalPerformanceTime
* @return void
*/
void AampMPDDownloader::showDownloadMetrics(DownloadResponsePtr dnldPtr, int totalPerformanceTime)
{
	CURLcode res 			=	static_cast<CURLcode>(dnldPtr->curlRetValue);
	int http_code			=	dnldPtr->iHttpRetValue;
	double total			=	dnldPtr->downloadCompleteMetrics.total;
	double totalPerformRequest	= (double)(totalPerformanceTime)/1000;	// in sec
	AAMP_LogLevel reqEndLogLevel	=	eLOGLEVEL_INFO;

	std::string appName, timeoutClass;
	if (!mAppName.empty())
	{
		// append app name with class data
		appName = mAppName + ",";
	}

	if (CURLE_OPERATION_TIMEDOUT == res || CURLE_PARTIAL_FILE == res || CURLE_COULDNT_CONNECT == res)
	{
		// introduce  extra marker for connection status curl 7/18/28,
		// example 18(0) if connection failure with PARTIAL_FILE code
		timeoutClass = "(" + std::to_string(dnldPtr->downloadCompleteMetrics.reqSize > 0) + ")";
	}
	if(res != CURLE_OK || http_code == 0 || http_code >= 400 || totalPerformRequest > 2.0 /*seconds*/)
	{
		reqEndLogLevel = eLOGLEVEL_WARN;
	}
	AAMPLOG( reqEndLogLevel, "HttpRequestEnd: %s%d,%d,%d%s,%2.4f,%2.4f,%2.4f,%2.4f,%2.4f,%2.4f,%2.4f,%2.4f,%g,%ld,%ld,%d,%.500s",
			appName.c_str(), eMEDIATYPE_TELEMETRY_MANIFEST, eMEDIATYPE_MANIFEST, http_code, timeoutClass.c_str(), totalPerformRequest, total,
			dnldPtr->downloadCompleteMetrics.connect, dnldPtr->downloadCompleteMetrics.startTransfer, dnldPtr->downloadCompleteMetrics.resolve,
			dnldPtr->downloadCompleteMetrics.appConnect, dnldPtr->downloadCompleteMetrics.preTransfer, dnldPtr->downloadCompleteMetrics.redirect,
			dnldPtr->downloadCompleteMetrics.dlSize, dnldPtr->downloadCompleteMetrics.reqSize, dnldPtr->downloadCompleteMetrics.downloadbps,
			0, dnldPtr->sEffectiveUrl.c_str());
}

/**
*   @fn pushDownloadDataToQueue
*   @brief pushDownloadDataToQueue push the downloaded data to queue
*/
void AampMPDDownloader::pushDownloadDataToQueue()
{
	std::lock_guard<std::mutex> lock(mMPDBufferMutex);
	if (mMPDBufferQ.size() >= mMPDBufferSize)
	{
		// Replace the old instance in the queue with the new one
		mMPDBufferQ.pop();
	}
	// Add the new item to the end of the queue - 1st iteration
	// If Cached MPD ( Stitched MPD is present, then push that to Q) , else push single downloaded MPD
	if(mCachedMPDData != nullptr)
		mMPDBufferQ.push(mCachedMPDData);
	else
		mMPDBufferQ.push(mMPDData);
	// inform the consumers if anyone is waiting for the data
	mMPDDnldDataCondVar.notify_all(); //signal to getmanifest
	if(mManifestUpdateCb)
	{
		mMPDNotifyPending.store(true);
		mMPDNotifierCondVar.notify_all(); //signal Notifier Thread
	}
	AAMPLOG_INFO("Pushed new Manifest Data to Q...");
}

/**
*   @fn GetManifest
*   @brief GetManifest Application to read the Manifest stored in MPD Downloader
*/
ManifestDownloadResponsePtr AampMPDDownloader::GetManifest(bool bWait, int iWaitDurationMs,int errorSimulation)
{
	ManifestDownloadResponsePtr respPtr = MakeSharedManifestDownloadResponsePtr();
	respPtr->mMPDStatus = AAMPStatusType::eAAMPSTATUS_MANIFEST_DOWNLOAD_ERROR;
	// Check if anything available in the Q to return
	// If last downloaded manifest present , return it to the application
	// if Q is empty (during tune) , if application asked for wait ( sync ) if so wait for download to complete and return
	// If Q is empty (during tune) and if application asked not to wait , then return failure .
	if(!mReleaseCalled)
	{

		if(errorSimulation != -1)
		{
			AAMPLOG_INFO("GetManifest Simulating Http Error:%d",errorSimulation);
			respPtr->mMPDDownloadResponse->iHttpRetValue	=	errorSimulation;
			return respPtr;
		}

		{
			std::unique_lock<std::mutex> lck1(mMPDBufferMutex);
			if(mMPDBufferQ.size())
			{
				return mMPDBufferQ.front();
			}
		}


		// if Q is not available with any data ( for very first tune )
		if(bWait)
		{
			std::unique_lock<std::mutex> lck2(mMPDDnldDataMtx);
			if (mMPDDnldDataCondVar.wait_for(lck2, std::chrono::milliseconds(iWaitDurationMs)) == std::cv_status::timeout)
			{
				// Timed out
				respPtr->mMPDDownloadResponse->iHttpRetValue = CURLE_OPERATION_TIMEDOUT;
				AAMPLOG_INFO("GetManifest timer exited after timeout ...%d",iWaitDurationMs);
				return respPtr;
			}
			else
			{
				// check if it exited the timer due to Release call
				if(mReleaseCalled)
				{
					// Release called , so send error
					respPtr->mMPDDownloadResponse->iHttpRetValue = CURLE_ABORTED_BY_CALLBACK;
					AAMPLOG_INFO("GetManifest timer exited after Release call ...");
					return respPtr;
				}
				else
				{
					// data received
					AAMPLOG_INFO("GetManifest timer exited after new data received ...");
					std::unique_lock<std::mutex> lck3(mMPDBufferMutex);
					if(mMPDBufferQ.size())
					{
						return mMPDBufferQ.front();
					}
				}
			}
		}
		else
		{
			// No wait
			respPtr->mMPDDownloadResponse->iHttpRetValue = CURLE_OPERATION_TIMEDOUT;
			AAMPLOG_INFO("GetManifest exited with no Wait call ...");
			return respPtr;
		}
	}

	return respPtr;
}


/**
*   @fn waitForRefreshInterval
*   @brief Wait function for the duration of Refresh interval before next download
*/
bool AampMPDDownloader::waitForRefreshInterval()
{
	bool refreshNeeded = false;

	std::unique_lock<std::mutex> lck(mRefreshMtx);
	if(mRefreshCondVar.wait_for(lck,std::chrono::milliseconds(mRefreshInterval))==std::cv_status::timeout) {
		refreshNeeded = true;
	}
	else
	{
		AAMPLOG_INFO("Manifest Refresh interval wait interrupted ...");
	}

	return refreshNeeded;
}


/**
*   @fn readMPDData
*   @brief readMPDData function to read the mpd and pull the parameters like isLive/Refresh Interval
*/
bool AampMPDDownloader::readMPDData(ManifestDownloadResponsePtr dnldManifest)
{
	bool retVal = true;
	dnldManifest->mIsLiveManifest   =       !(dnldManifest->mMPDInstance->GetType() == "static");
	uint64_t publishTimeMSec = 0;
	std::string publishTimeStr;
	auto attributesMap = dnldManifest->mMPDInstance->GetRawAttributes();
	if(attributesMap.find("publishTime") != attributesMap.end())
	{
		publishTimeStr = attributesMap["publishTime"];
	}
	if(!publishTimeStr.empty())
	{
		publishTimeMSec = (uint64_t)ISO8601DateTimeToUTCSeconds(publishTimeStr.c_str()) * 1000;
	}
	AAMPLOG_TRACE("Publish Time of Updated manifest %" PRIu64 ", Previous manifest update time %" PRIu64, publishTimeMSec, mPublishTime);

	/* If there is no update in the manifest and publish time is not zero, Set the refresh interval to a minimal value (500ms). This is done for a maximum of two times to avoid frequent manifest refresh.*/
	if (publishTimeMSec == mPublishTime && publishTimeMSec != 0) 
	{
		if (mMinimalRefreshRetryCount < 2) 
		{
			mRefreshInterval = (uint32_t)(MIN_DELAY_BETWEEN_MPD_UPDATE_MS);
			AAMPLOG_INFO("No update detected in manifest. Setting refresh interval to minimal refresh interval %u", mRefreshInterval);
			mMinimalRefreshRetryCount++;
			/* To avoid race condition when GetNetworkTime is executed ,meanwhile manifest refresh is done for next attempt */
			retVal = false;
		} 
		else
		{
			mRefreshInterval = getMeNextManifestDownloadWaitTime(dnldManifest);
		}
	} 
	else 
	{
		mPublishTime = publishTimeMSec;
		mRefreshInterval = getMeNextManifestDownloadWaitTime(dnldManifest);
		mMinimalRefreshRetryCount = 0;  // Reset the retry count on detecting a new publish time
	}
	return retVal;
}

/**
* @fn IsMPDLowLatency
* @brief Checks if the MPD has low latency mode enabled.
* @param LLDashData Reference to AampLLDashServiceData object 
* @return True if low latency mode is enabled in the MPD, false otherwise.
*/
bool AampMPDDownloader::IsMPDLowLatency(AampLLDashServiceData &LLDashData)
{
	bool retVal = false;
	if(mMPDData != nullptr)
	{
		retVal 		= 	mLLDashData.lowLatencyMode;
		LLDashData	=	mLLDashData;
	}
	return retVal;
}


bool AampMPDDownloader::isMPDLowLatency(ManifestDownloadResponsePtr dnldManifest, AampLLDashServiceData &LLDashData)
{
	bool isSuccess=false;
	LLDashData.lowLatencyMode	=	 false;
	if(mMPDDnldCfg->mIsLLDConfigEnabled)
	{
		dash::mpd::IMPD *mpd			=	dnldManifest->mMPDInstance.get();
		if(mpd != NULL)
		{
			size_t numPeriods = mpd->GetPeriods().size();
			for (unsigned iPeriod = 0; iPeriod < numPeriods; iPeriod++)
			{
				IPeriod *period = mpd->GetPeriods().at(iPeriod);
				if(NULL != period )
				{
					const std::vector<IAdaptationSet *> adaptationSets = period->GetAdaptationSets();
					if (adaptationSets.size() > 0)
					{
						const IAdaptationSet * pFirstAdaptation = adaptationSets.at(0);
						if ( NULL != pFirstAdaptation )
						{
							const ISegmentTemplate *pSegmentTemplate = pFirstAdaptation->GetSegmentTemplate();
							if(pSegmentTemplate == NULL)
							{
								const std::vector<IRepresentation *> representations = pFirstAdaptation->GetRepresentation();
								if( representations.size()>0 )
								{
									const IRepresentation *representation = representations.at(0);
									pSegmentTemplate = representation->GetSegmentTemplate();
								}
							}
							if( NULL != pSegmentTemplate )
							{
								std::map<std::string, std::string> attributeMap = pSegmentTemplate->GetRawAttributes();
								if(attributeMap.find("availabilityTimeOffset") != attributeMap.end())
								{
									LLDashData.availabilityTimeOffset = pSegmentTemplate->GetAvailabilityTimeOffset();
									LLDashData.availabilityTimeComplete = pSegmentTemplate->GetAvailabilityTimeComplete();
									AAMPLOG_INFO("AvailabilityTimeOffset=%lf AvailabilityTimeComplete=%d",
										LLDashData.availabilityTimeOffset,LLDashData.availabilityTimeComplete);
									if (LLDashData.availabilityTimeOffset > 0.0)
									{
										LLDashData.lowLatencyMode	=	isSuccess	=	true;
									}
									if( isSuccess )
									{
										uint32_t timeScale=0;
										uint32_t duration =0;
										const ISegmentTimeline *segmentTimeline = pSegmentTemplate->GetSegmentTimeline();
										if (segmentTimeline)
										{
											timeScale = pSegmentTemplate->GetTimescale();
											std::vector<ITimeline *>&timelines = segmentTimeline->GetTimelines();
											ITimeline *timeline = timelines.at(0);
											duration = timeline->GetDuration();
											LLDashData.fragmentDuration = ComputeFragmentDuration(duration,timeScale);
											LLDashData.isSegTimeLineBased = true;
										}
										else
										{
											timeScale = pSegmentTemplate->GetTimescale();
											duration = pSegmentTemplate->GetDuration();
											LLDashData.fragmentDuration = ComputeFragmentDuration(duration,timeScale);
											LLDashData.isSegTimeLineBased = false;
										}
										AAMPLOG_INFO("timeScale=%u duration=%u fragmentDuration=%lf",
													timeScale,duration,LLDashData.fragmentDuration);
									}
									break;
								}
							}
				
						}
					}
				}
			}
			
			if (!isSuccess)
			{
				AAMPLOG_INFO("Latency availabilityTimeOffset attribute not available");
			}
		}
	}
	return isSuccess;
}

uint32_t AampMPDDownloader::getMeNextManifestDownloadWaitTime(ManifestDownloadResponsePtr dnldManifest)
{
	uint32_t minUpdateDuration		=       DEFAULT_INTERVAL_BETWEEN_MPD_UPDATES_MS;
	uint32_t minDelayBetweenPlaylistUpdates = DEFAULT_INTERVAL_BETWEEN_MPD_UPDATES_MS;
	if(dnldManifest->mIsLiveManifest)
	{
		bool eventStreamFound	=	 false;
		dnldManifest->mRefreshRequired		=		true;
		std::string tempStr = dnldManifest->mMPDInstance->GetMinimumUpdatePeriod();
		if(!tempStr.empty())
		{
			minUpdateDuration = ParseISO8601Duration( tempStr.c_str() );
		}
		auto periods = dnldManifest->mMPDInstance->GetPeriods();
		for (int iter = 0; iter < periods.size(); iter++)
		{
			auto period = periods.at(iter);
			auto eventStream = period->GetEventStreams();
			if(!(eventStream.empty()))
			{
				eventStreamFound = true;
				break;
			}
		}
		AAMPLOG_INFO("Min Update Period from Manifest %u Latency Value %d lowLatencyMode %d",minUpdateDuration,mLatencyValue,mIsLowLatency);

		// playTarget value will vary if TSB is full and trickplay is attempted. Cant use for buffer calculation
		// So using the endposition in playlist - Current playing position to get the buffer availability
		int bufferAvailable = mLatencyValue;

		// when target duration is high value(>Max delay)  but buffer is available just above the max update inteval,then go with max delay between playlist refresh.
		if(bufferAvailable != -1 && !mIsLowLatency)
		{
			if(bufferAvailable < (2* MAX_DELAY_BETWEEN_MPD_UPDATE_MS))
			{
				if ((minUpdateDuration > 0) && (bufferAvailable  > minUpdateDuration))
				{
					//1.If buffer Available is > 2*minUpdateDuration , may be 1.0 times also can be set ???
					//2.If buffer is between 2*target & mMinUpdateDurationMs
					float mFactor=0.0f;
					if (mIsLowLatency)
					{
						mFactor = (bufferAvailable  > (minUpdateDuration * 2)) ? 1.0 : 0.5;
					}
					else
					{
						mFactor = (bufferAvailable  > (minUpdateDuration * 2)) ? 1.5 : 0.5;
					}
					minDelayBetweenPlaylistUpdates = (int)(mFactor * minUpdateDuration);
				}
				// if buffer < targetDuration && buffer < MaxDelayInterval
				else
				{
					// if bufferAvailable is less than targetDuration ,its in RED alert . Close to freeze
					// need to refresh soon ..
					minDelayBetweenPlaylistUpdates = (bufferAvailable) ? (int)(bufferAvailable / 3) : MIN_DELAY_BETWEEN_MPD_UPDATE_MS; //500ms
					// limit the logs when buffer is low
					if(bufferAvailable < DEFAULT_INTERVAL_BETWEEN_MPD_UPDATES_MS)
					{
						static int bufferlowCnt;
						if((bufferlowCnt++ & 5) == 0)
						{
							AAMPLOG_WARN("Buffer is running low(%d).Refreshing playlist(%u)",bufferAvailable,minDelayBetweenPlaylistUpdates);
						}
					}
				}
			}
			else if(bufferAvailable > (2* MAX_DELAY_BETWEEN_PLAYLIST_UPDATE_MS))
			{
				minDelayBetweenPlaylistUpdates = MAX_DELAY_BETWEEN_MPD_UPDATE_MS;
			}
		}

		// If any CDAI entries present in playlist, then refresh with update duration specified in playlist
		// For lld ,honour min  update duration specified in manifest
		if ((eventStreamFound || mIsLowLatency) && minUpdateDuration >0 && minUpdateDuration < minDelayBetweenPlaylistUpdates)
		{
			minDelayBetweenPlaylistUpdates = (int)minUpdateDuration;
		}

		// restrict to Max delay interval
		if (minDelayBetweenPlaylistUpdates > MAX_DELAY_BETWEEN_MPD_UPDATE_MS)
		{
			minDelayBetweenPlaylistUpdates = MAX_DELAY_BETWEEN_MPD_UPDATE_MS;
		}

		if(minDelayBetweenPlaylistUpdates < MIN_DELAY_BETWEEN_MPD_UPDATE_MS)
		{
			if (mIsLowLatency)
			{
				long availTimeOffMs = (long)((mLLDashData.availabilityTimeOffset)*1000);
				long maxSegDuration = (long)((mLLDashData.fragmentDuration)*1000);
				if(minUpdateDuration > 0 && minUpdateDuration < maxSegDuration)
				{
					minDelayBetweenPlaylistUpdates = (uint32_t)minUpdateDuration;
				}
				else if(minUpdateDuration > 0 && minUpdateDuration > availTimeOffMs)
				{
					minDelayBetweenPlaylistUpdates = (uint32_t)(minUpdateDuration-availTimeOffMs);
				}
				else if (maxSegDuration > 0 && maxSegDuration > availTimeOffMs)
				{
					minDelayBetweenPlaylistUpdates = (uint32_t)(maxSegDuration-availTimeOffMs);
				}
				else
				{
					// minimum of 500 mSec needed to avoid too frequent download.
					minDelayBetweenPlaylistUpdates = (uint32_t)MIN_DELAY_BETWEEN_MPD_UPDATE_MS;
				}
				if(minDelayBetweenPlaylistUpdates < MIN_DELAY_BETWEEN_MPD_UPDATE_MS)
				{
						// minimum of 500 mSec needed to avoid too frequent download.
					minDelayBetweenPlaylistUpdates = (uint32_t)MIN_DELAY_BETWEEN_MPD_UPDATE_MS;
				}
			}
			else
			{
				// minimum of 500 mSec needed to avoid too frequent download.
				minDelayBetweenPlaylistUpdates = (uint32_t)MIN_DELAY_BETWEEN_MPD_UPDATE_MS;
			}
		}

		//When you have content to download in manifest ,no need to do frequent 500msec refresh
		if (mIsLowLatency && minDelayBetweenPlaylistUpdates <= (uint32_t)MIN_DELAY_BETWEEN_MPD_UPDATE_MS && mCurrentposDeltaToManifestEnd > (long)((mLLDashData.fragmentDuration)*1000)*2)
		{
			minDelayBetweenPlaylistUpdates = (uint32_t)minUpdateDuration;
		}
		// When the buffer hits zero, it is worth refreshing frequently in order to rebuild the buffer
		if (bufferAvailable <= 0.5 && mIsLowLatency) 
		{
			// Set the minimum delay between playlist updates 
			minDelayBetweenPlaylistUpdates = (uint32_t)(MIN_DELAY_BETWEEN_MPD_UPDATE_MS);
		}

		AAMPLOG_INFO("aamp playlist end refresh bufferMs(%d) delay(%u)", bufferAvailable,minDelayBetweenPlaylistUpdates);
	}
	return minDelayBetweenPlaylistUpdates;
}

/**
* @fn RegisterCallback
* @brief Registers a callback function for manifest update notifications.
* @param fnPtr Pointer to the callback function.
* @param cbArg Pointer to data passed to the callback function.
* return void 
*/
void AampMPDDownloader::RegisterCallback(ManifestUpdateCallbackFunc fnPtr, void *cbArg)
{
	std::unique_lock<std::mutex> lck2(mMPDNotifierMtx);
	AAMPLOG_INFO("Register for Callback ");
	if(fnPtr !=NULL && mManifestUpdateCb == NULL)
	{	
		lck2.unlock(); 
		if(mDownloadNotifierThread.joinable())
		{
			AAMPLOG_INFO("Joining MPD Download thread [%zx]", GetPrintableThreadID(mDownloadNotifierThread));
			mDownloadNotifierThread.join();
		}
		lck2.lock();

		mManifestUpdateCb       =		fnPtr;
		mManifestUpdateCbArg	=		cbArg;
		// Start thread for sending manifest updates
		mDownloadNotifierThread = std::thread(&AampMPDDownloader::downloadNotifierThread, this);
		AAMPLOG_INFO("Thread created for MPD Download notification [%zx]", GetPrintableThreadID(mDownloadNotifierThread));
	}
}

/**
* @fn UnRegisterCallback
* @brief Unregister the callback function for manifest update notifications.
*/
void AampMPDDownloader::UnRegisterCallback()
{
	std::unique_lock<std::mutex> lck2(mMPDNotifierMtx);
	AAMPLOG_INFO("UnRegister for Callback ");
	if(mManifestUpdateCb)
	{
		mManifestUpdateCb		=	NULL;
		mManifestUpdateCbArg	=	NULL;
		// Send notification to exit from notifier Thread
		mMPDNotifierCondVar.notify_all();
	}
	else
	{
		AAMPLOG_WARN("Notify not sent. mManifestUpdateCb is NULL, mManifestUpdateCbArg = %p", mManifestUpdateCbArg);
	}
}

/**
*   @fn downloadNotifier
*   @brief downloadNotifier thread function to notify Download
*/
void AampMPDDownloader::downloadNotifierThread()
{
	UsingPlayerId playerId(mMPDDnldCfg->mPlayerId);
	std::unique_lock<std::mutex> lck2(mMPDNotifierMtx);

	// infinite wait for download notification
	while (!mReleaseCalled && mManifestUpdateCb)
	{
		if(!mMPDNotifyPending.load())
		{
			mMPDNotifierCondVar.wait(lck2);
		}

		if(!mReleaseCalled && mManifestUpdateCb)
		{
			mMPDNotifyPending.store(false);
			// if its not the notification from Release call
			long long tStartTime = NOW_STEADY_TS_MS;
			mManifestUpdateCb(mManifestUpdateCbArg);
			long long tEndTime = NOW_STEADY_TS_MS;
			AAMPLOG_INFO("Time taken for MPD Download notification %u", (unsigned int)(tEndTime-tStartTime));
		}
	}
	AAMPLOG_INFO("Exited Download Notifier Thread");
}

/**
* @fn GetCMCDHeader
* @brief Retrieves CMCD headers for manifest requests.
* @return Unordered map with header names as keys and vector of header values as values.
*/
std::unordered_map<std::string, std::vector<std::string>> AampMPDDownloader::getCMCDHeader()
{
	std::unordered_map<std::string, std::vector<std::string>> cmcd;
	{
		std::vector<std::string> cmcdCustomHeader;
		mMPDDnldCfg->mCMCDCollector->CMCDGetHeaders(eMEDIATYPE_MANIFEST, cmcdCustomHeader);
		for (const auto& header : cmcdCustomHeader) {
			size_t colon_pos = header.find(':');
			if (colon_pos != std::string::npos) {
				std::string header_name = header.substr(0, colon_pos + 1); // include the colon
				std::string header_value = header.substr(colon_pos + 1);
				trim(header_value); // remove any whitespace
				cmcd[header_name].push_back(header_value);
			}
		}
	}
	return cmcd;


}
/**
 * @fn 	GetLastDownloadMPDSize
 * @brief
 */
void AampMPDDownloader::GetLastDownloadedManifest(std::string& manifestBuffer)
{
	manifestBuffer = mMPDData->mMPDDownloadResponse->getString();
}
