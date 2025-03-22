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
#include "MockAampMPDDownloader.h"

#define DEFAULT_INTERVAL_BETWEEN_MPD_UPDATES_MS 3000

MockAampMPDDownloader *g_mockAampMPDDownloader = nullptr;

void _manifestDownloadResponse::show()
{
}

_manifestDownloadResponse::~_manifestDownloadResponse()
{

}

void _manifestDownloadResponse::parseMpdDocument()
{
}

std::shared_ptr<_manifestDownloadResponse> _manifestDownloadResponse::clone()
{
	return nullptr;
}

/**
 *   @fn parseMPD
 *   @brief parseMPD function to parse the downloaded MPD file
 */
void _manifestDownloadResponse::parseMPD()
{
}

/**
 *   @fn AampMPDDownloader
 *   @brief Default Constructor
 */
AampMPDDownloader::AampMPDDownloader() :  mMPDBufferQ(),mMPDBufferSize(1),mMPDBufferMutex(),mRefreshMtx(),mRefreshCondVar(),
	mMPDDnldMutex(),mRefreshInterval(DEFAULT_INTERVAL_BETWEEN_PLAYLIST_UPDATES_MS),mLatencyValue(-1),mReleaseCalled(false),
	mMPDDnldCfg(NULL),mDownloaderThread_t1(),mDownloaderThread_t2(),mDownloader1(),mDownloader2(),mMPDData(nullptr),mAppName(""),
	mManifestUpdateCb(NULL),mManifestUpdateCbArg(NULL),mDownloadNotifierThread(),mCachedMPDData(nullptr),
	mCheckedLLDData(false),mMPDNotifierMtx(),mMPDNotifierCondVar(),mManifestRefreshCount(0)
{
}

/**
 *   @fn AampMPDDownloader
 *   @brief Destructor
 */
AampMPDDownloader::~AampMPDDownloader()
{
}

/**
 *   @fn Initialize
 *   @brief Initialize with MPD Download Input
 */
void AampMPDDownloader::Initialize(std::shared_ptr<ManifestDownloadConfig> mpdDnldCfg, std::string appName,std::function<std::string()> mpdPreProcessFuncptr)
{
}
/**
 *   @fn SetNetworkTimeout
 *   @brief Set Network Timeout for Manifest download
 */
void AampMPDDownloader::SetNetworkTimeout(uint32_t iDurationSec)
{
}
/**
 *   @fn SetStallTimeout
 *   @brief Set Stall Timeout for Manifest download
 */
void AampMPDDownloader::SetStallTimeout(uint32_t iDurationSec)
{
}
/**
 *   @fn SetStartTimeout
 *   @brief Set Start Timeout for Manifest download
 */
void AampMPDDownloader::SetStartTimeout(uint32_t iDurationSec)
{
}
/**
 *   @fn SetBufferAvailability
 *   @brief Set Buffer Value to calculate the manifest refresh rate
 */
void AampMPDDownloader::SetBufferAvailability(int iDurationMilliSec)
{
}

/**
 *   @fn Release
 *   @brief Release function to clear the allocation and join threads
 */
void AampMPDDownloader::Release()
{
}

/**
 *   @fn Start
 *   @brief Start the Manifest downloader
 */
void AampMPDDownloader::Start()
{
}


/**
 *   @fn GetManifest
 *   @brief GetManifest Application to read the Manifest stored in MPD Downloader
 */
ManifestDownloadResponsePtr AampMPDDownloader::GetManifest(bool bWait, int iWaitDurationMs,int errorSimulation)
{
	if (g_mockAampMPDDownloader != nullptr)
	{
		return g_mockAampMPDDownloader->GetManifest(bWait, iWaitDurationMs, errorSimulation);
	}
	else
	{
		return nullptr;
	}
}


/**
 * @fn IsMPDLowLatency
 * @brief Checks if the MPD has low latency mode enabled.
 * @param LLDashData Reference to AampLLDashServiceData object 
 * @return True if low latency mode is enabled in the MPD, false otherwise.
 */
bool AampMPDDownloader::IsMPDLowLatency(AampLLDashServiceData &LLDashData)
{
	if (g_mockAampMPDDownloader != nullptr)
	{
		return g_mockAampMPDDownloader->IsMPDLowLatency(LLDashData);
	}
	else
	{
		return false;
	}

}


/* @fn RegisterCallback
 * @brief Registers a callback function for manifest update notifications.
 * @param fnPtr Pointer to the callback function.
 * @param cbArg Pointer to data passed to the callback function.
 * return void 
 */
void AampMPDDownloader::RegisterCallback(ManifestUpdateCallbackFunc fnPtr, void *cbArg)
{
}

/**
 * @fn UnRegisterCallback
 * @brief Unregister the callback function for manifest update notifications.
 */
void AampMPDDownloader::UnRegisterCallback()
{
}

/**
 * @fn 	GetLastDownloadMPDSize
 * @brief
 */
void AampMPDDownloader::GetLastDownloadedManifest(std::string& manifestBuffer)
{
}
