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
* @file AampDownloadManager.cpp
* @brief Curl Downloader for Aamp
**************************************/

#include "AampCurlDownloader.h"
#include "AampUtils.h"
#include <vector>
#include "AampLogManager.h"


void _downloadConfig::show()
{
}

void _downloadResponse::show()
{
}

curl_off_t aamp_CurlEasyGetinfoOffset( CURL *handle, CURLINFO info )
{
	curl_off_t rc = 0;
	return rc;
}

double aamp_CurlEasyGetinfoDouble( CURL *handle, CURLINFO info )
{
	double rc = 0.0;
	return rc;
}

long aamp_CurlEasyGetinfoLong( CURL *handle, CURLINFO info )
{
	long rc = -1;
	return rc;
}

char *aamp_CurlEasyGetinfoString( CURL *handle, CURLINFO info )
{
	char *rc = NULL;
	return rc;
}



AampCurlDownloader::AampCurlDownloader() : mCurlMutex(),m_threadName(""),mDownloadActive(false),mCreatedNewFd(false),
			mCurl(nullptr),mDownloadUpdatedTime(0),mDownloadStartTime(0),mDnldCfg(),mDownloadResponse(nullptr),mHeaders(NULL)

{
	
}


AampCurlDownloader::~AampCurlDownloader()
{
}

bool AampCurlDownloader::IsDownloadActive()
{
	return false;
}

int AampCurlDownloader::Download(const std::string &urlStr, std::shared_ptr<DownloadResponse> dnldData )
{
	return 0;
}

void AampCurlDownloader::updateResponseParams()
{
}

void AampCurlDownloader::Initialize(std::shared_ptr<DownloadConfig> dnldCfg)
{

}


void AampCurlDownloader::Release()
{
}


void AampCurlDownloader::Clear()
{
}


void AampCurlDownloader::updateCurlParams()
{
	return ;
}


size_t AampCurlDownloader::WriteCallback(void *buffer, size_t sz, size_t nmemb, void *userdata)
{
	// Call non-static member function.
	size_t ret = 0;
	return ret;
}

size_t AampCurlDownloader::write_callback(void *buffer, size_t sz, size_t nmemb)
{
	size_t retSize = sz * nmemb;
	return retSize;
}

size_t AampCurlDownloader::HeaderCallback(void *buffer, size_t sz, size_t nmemb, void *userdata)
{
	// Call non-static member function.
	size_t ret = 0;
	return ret;
}

size_t AampCurlDownloader::header_callback(void *buffer, size_t sz, size_t nmemb)
{
	size_t retSize = sz * nmemb;
	return retSize;
}

int AampCurlDownloader::ProgressCallback(
										 void *clientp, // app-specific as optionally set with CURLOPT_PROGRESSDATA
										 double dltotal, // total bytes expected to download
										 double dlnow, // downloaded bytes so far
										 double ultotal, // total bytes expected to upload
										 double ulnow // uploaded bytes so far
)
{
	int ret = 0;
	return ret;
}

int AampCurlDownloader::progress_callback(
					 double dltotal, // total bytes expected to download
					 double dlnow, // downloaded bytes so far
					 double ultotal, // total bytes expected to upload
					 double ulnow // uploaded bytes so far
)
{
	int rc = 0;
	return rc;
	
}

size_t AampCurlDownloader::GetDataString(std::string &dataStr)
{
	return 0;
}

