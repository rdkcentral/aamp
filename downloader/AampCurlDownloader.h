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
* @file AampDownloadManager.h
* @brief Curl Downloader for Aamp
**************************************/

#ifndef __AAMP_CURL_DOWNLOADER__
#define __AAMP_CURL_DOWNLOADER__

//#include "AampDefine.h"
#include <stdint.h>
#include <unordered_map>
#include <vector>
#include <iterator>
#include <algorithm>
#include <map>
#include <string>
#include <mutex>
#include <sys/time.h>
#include <iostream>
#include <string>
#include <curl/curl.h>
#include <chrono>
#include <memory>
#include "AampCurlDefine.h"



typedef std::map<int,std::string> RespHeader;
typedef std::map<int,std::string>::iterator RespHeaderIter;

/**
 * @struct _downloadConfig
 * @brief structure to store the download configuration
 */
typedef struct _downloadConfig
{
	uint32_t iDownloadTimeout;
	uint32_t iLowBWTimeout;
	uint32_t iStallTimeout;
	uint32_t iStartTimeout;
	uint32_t iCurlConnectionTimeout;
	uint32_t iDownloadRetryCount;
	uint32_t iDownloadRetryWaitMs;
	uint32_t iDownload502RetryCount; 		//Non zero value then use this for 502 retries

	CurlRequest eRequestType;
	long    lSupportedTLSVersion;
	bool	bSSLVerifyPeer;
	bool	bVerbose;
	bool 	bIgnoreResponseHeader;
	bool	bNeedDownloadMetrics;
	long 	iDnsCacheTimeOut;

	//AampMediaType mediaType;
	std::unordered_map<std::string, std::vector<std::string>> sCustomHeaders;
	std::string userAgentString;
	std::string postData;
	std::string proxyName;
	CURL *pCurl;
	
	_downloadConfig() : pCurl(nullptr),iDownloadTimeout(DEFAULT_CURL_TIMEOUT),iLowBWTimeout(0),iCurlConnectionTimeout(DEFAULT_CURL_CONNECTTIMEOUT),
			iStallTimeout(0),iStartTimeout(0),bSSLVerifyPeer(false),lSupportedTLSVersion(CURL_SSLVERSION_TLSv1_2),proxyName(""),userAgentString(""),sCustomHeaders(),
			bVerbose(false),bIgnoreResponseHeader(false),bNeedDownloadMetrics(false),eRequestType(eCURL_GET),postData(""),iDownloadRetryCount(0),iDownload502RetryCount(0),
			iDownloadRetryWaitMs(50),iDnsCacheTimeOut(DEFAULT_DNS_CACHE_TIMEOUT)
	{
	}
	
	 _downloadConfig(const _downloadConfig& other) 
	 	:pCurl(),iDownloadTimeout(),
			iLowBWTimeout(),iCurlConnectionTimeout(),
			iStallTimeout(),iStartTimeout(),
			bSSLVerifyPeer(),lSupportedTLSVersion(),
			proxyName(),userAgentString(),
			sCustomHeaders(),bVerbose(),
			bIgnoreResponseHeader(),bNeedDownloadMetrics(),
			eRequestType(),postData(),iDnsCacheTimeOut(),
			iDownloadRetryCount(),iDownloadRetryWaitMs()
    	{
			*this=other;
    	}

    _downloadConfig& operator=(const _downloadConfig& other) {
        _downloadConfig temp(other); // temp obj to copy
        std::swap(*this, temp); // swap the current obj with temp obj
        return *this;
    }	
public:
	void show();
}DownloadConfig;

/**
 * @struct _dnld_metrics
 * @brief structure to store the download metrics
 */

typedef struct _dnld_metrics
{
	double total, connect, startTransfer, resolve, appConnect, preTransfer, redirect, dlSize;
	long reqSize, downloadbps;
	
	_dnld_metrics():total(0), connect(0), startTransfer(0), resolve(0), appConnect(0), preTransfer(0), redirect(0), dlSize(0),
	reqSize(0), downloadbps(0)
	{
	}
	void clear()
	{
		total = 0;
		connect = 0;
		startTransfer = 0;
		resolve = 0;
		appConnect = 0;
		preTransfer = 0;
		redirect = 0;
		dlSize = 0;
		reqSize = 0;
		downloadbps = 0;
		
	}

}Dnld_Metrics;

/**
 * @struct _dnldprogress_metrics
 * @brief structure to store the download progress data 
 */

typedef struct _dnldprogress_metrics
{
	double dlnow, dlTotal;
	double downloadbps;
	
	_dnldprogress_metrics():dlnow(0), dlTotal(0), downloadbps(0)
	{
	}
	void clear()
	{
		dlnow = 0;
		dlTotal = 0;
		downloadbps = 0;
	}
}DnldProgress_Metrics;

/**
 * @struct _downloadResponse
 * @brief structure to store the download response data 
 */

typedef struct _downloadResponse
{
	int curlRetValue;
	int iHttpRetValue;
	CurlAbortReason mAbortReason;
	Dnld_Metrics downloadCompleteMetrics;
	DnldProgress_Metrics progressMetrics;
	
	std::string sEffectiveUrl;
	std::vector<std::string>  mResponseHeader;
	std::vector<std::uint8_t> mDownloadData;
	
	_downloadResponse() : curlRetValue(0), iHttpRetValue(0), mAbortReason(eCURL_ABORT_REASON_NONE), downloadCompleteMetrics(),progressMetrics(), sEffectiveUrl(""), mResponseHeader(), mDownloadData() {}

public:
	void clear()
	{
		mDownloadData.clear();
		sEffectiveUrl.clear();
		downloadCompleteMetrics.clear();
		progressMetrics.clear();
		curlRetValue = 0;
		iHttpRetValue = 0;
		mAbortReason = eCURL_ABORT_REASON_NONE;
		mResponseHeader.clear();		
	}

	size_t size() { return mDownloadData.size();}
	std::string getString() { return std::string( mDownloadData.begin(), mDownloadData.end()); }
	void show();
	void replaceDownloadData(const std::string& data) {
		if(!data.empty())
		{
			mDownloadData.clear();
			mDownloadData.assign(data.begin(), data.end());
		}
	}
}DownloadResponse;

typedef std::shared_ptr<DownloadResponse> DownloadResponsePtr;
typedef std::shared_ptr<DownloadConfig> DownloadConfigPtr;
/**
 * @class AampCurlDownloader
 * @brief Class to handle Curl download functionality
 */

class AampCurlDownloader
{
public:
	/**
	* @brief Constructor function 
	* @param[in] dnldCfg - configuration for download
	*/
	AampCurlDownloader();
	/**
	* @brief Destructor function 
	*/
	~AampCurlDownloader();

	void Initialize(std::shared_ptr<DownloadConfig> dnldCfg);
	/**
	* @brief Release - function to stop the download and reset the download parameters
	* @param[in] dnldCfg - configuration for download
	*/	
	void Release();
	void Clear();
	/**
	* @brief Download - function to start  download 
	* @param[in] urlStr - URL to download 
	* @param[out] dnldData - structure to store download data and metrics 
	*/
	int Download(const std::string &urlStr, std::shared_ptr<DownloadResponse> dnldData );
	/**
	* @brief IsDownloadActive - function to check if download is active with the session
	*/
	bool IsDownloadActive();
	/**
	* @brief GetDataString - function to get downloaded data as string from Vector 
	*/	
	size_t GetDataString(std::string &dataStr);

private:
	void updateCurlParams();
	void updateResponseParams();
	static size_t WriteCallback( void *contents, size_t size, size_t nmemb, void *userp );
	size_t write_callback(void *buffer, size_t sz, size_t n);
	static size_t HeaderCallback( void *contents, size_t size, size_t nmemb, void *userp );
	size_t header_callback(void *buffer, size_t sz, size_t n);
	static int ProgressCallback(
								 void *clientp, // app-specific as optionally set with CURLOPT_PROGRESSDATA
								 double dltotal, // total bytes expected to download
								 double dlnow, // downloaded bytes so far
								 double ultotal, // total bytes expected to upload
								 double ulnow // uploaded bytes so far
	);
	int progress_callback(
								double dltotal, // total bytes expected to download
								double dlnow, // downloaded bytes so far
								double ultotal, // total bytes expected to upload
								double ulnow // uploaded bytes so far
	);
	
private:
	AampCurlDownloader(const AampCurlDownloader&) = delete;
	AampCurlDownloader& operator=(const AampCurlDownloader&) = delete;
	std::mutex mCurlMutex;
	std::string m_threadName;
	long long mDownloadStartTime;
	long long mDownloadUpdatedTime;
	size_t mWriteCallbackBufferSize;
	bool mDownloadActive;
	bool mCreatedNewFd;
	std::shared_ptr<DownloadConfig> mDnldCfg;
	std::shared_ptr<DownloadResponse> mDownloadResponse;
	CURL *mCurl;
	struct curl_slist *mHeaders;
};

#endif 
