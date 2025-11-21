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
* @file AampCurlDownloader.cpp
* @brief Curl Downloader for Aamp
**************************************/

#include "AampCurlDownloader.h"
#include "AampUtils.h"
#include <vector>
#include "AampLogManager.h"
#include "AampDefine.h"

void _downloadConfig::show()
{
	AAMPLOG_INFO("iDownloadTimeout : %u", iDownloadTimeout);
	AAMPLOG_INFO("iLowBWTimeout : %u", iLowBWTimeout);
	AAMPLOG_INFO("iStallTimeout : %u", iStallTimeout);
	AAMPLOG_INFO("iStartTimeout : %u", iStartTimeout);
	AAMPLOG_INFO("iCurlConnectionTimeout : %u", iCurlConnectionTimeout);
	AAMPLOG_INFO("lSupportedTLSVersion : %ld", lSupportedTLSVersion);
	AAMPLOG_INFO("bSSLVerifyPeer : %d", bSSLVerifyPeer);
	AAMPLOG_INFO("curl : %p", pCurl);
	AAMPLOG_INFO("userAgentString :%s",userAgentString.c_str());
	AAMPLOG_INFO("proxyName :%s",proxyName.c_str());
	AAMPLOG_INFO("iDnsCacheTimeOut :%ld",iDnsCacheTimeOut);

	
	if (sCustomHeaders.size() > 0)
	{
		std::string customHeader;
		std::string headerValue;
		for (std::unordered_map<std::string, std::vector<std::string>>::iterator it = sCustomHeaders.begin();
			 it != sCustomHeaders.end(); it++)
		{
			customHeader.clear();
			headerValue.clear();
			customHeader.insert(0, it->first);
			headerValue = it->second.at(0);
			customHeader.push_back(' ');
			customHeader.append(headerValue);
			AAMPLOG_INFO("header : %s", customHeader.c_str());
		}
	}
}

void _downloadResponse::show()
{
	AAMPLOG_INFO("curlRetValue : %d", curlRetValue);
	AAMPLOG_INFO("iHttpRetValue : %d", iHttpRetValue);
	AAMPLOG_INFO("total : %lf msec", downloadCompleteMetrics.total*1000);
	AAMPLOG_INFO("connect : %lf msec", downloadCompleteMetrics.connect*1000);
	AAMPLOG_INFO("startTransfer : %lf msec", downloadCompleteMetrics.startTransfer*1000);
	AAMPLOG_INFO("resolve : %lf msec", downloadCompleteMetrics.resolve*1000);
	AAMPLOG_INFO("appConnect : %lf msec", downloadCompleteMetrics.appConnect*1000);
	AAMPLOG_INFO("preTransfer : %lf msec", downloadCompleteMetrics.preTransfer*1000);
	AAMPLOG_INFO("redirect : %lf msec", downloadCompleteMetrics.redirect*1000);
	AAMPLOG_INFO("dlSize : %f bytes", downloadCompleteMetrics.dlSize);
	
	AAMPLOG_INFO("reqSize : %ld bytes", downloadCompleteMetrics.reqSize);
	AAMPLOG_INFO("downloadbps : %ld bps", downloadCompleteMetrics.downloadbps);
	AAMPLOG_INFO("dataSize : %d bytes", (int)mDownloadData.size());
	AAMPLOG_INFO("effective Url : %s", sEffectiveUrl.c_str());
	
	for(auto it=mResponseHeader.begin();it < mResponseHeader.end();it++)
	{
		AAMPLOG_INFO("Header=>: %s",(*it).c_str());
	}
}

curl_off_t aamp_CurlEasyGetinfoOffset( CURL *handle, CURLINFO info )
{
	curl_off_t rc = 0;
	if( handle && curl_easy_getinfo(handle,info,&rc) != CURLE_OK )
	{
		AAMPLOG_WARN( "aamp_CurlEasyGetinfoOffset failure" );
	}
	return rc;
}

double aamp_CurlEasyGetinfoDouble( CURL *handle, CURLINFO info )
{
	double rc = 0.0;
	if( handle && curl_easy_getinfo(handle,info,&rc) != CURLE_OK )
	{
		AAMPLOG_WARN( "aamp_CurlEasyGetinfoDouble failure" );
	}
	return rc;
}

long aamp_CurlEasyGetinfoLong( CURL *handle, CURLINFO info )
{
	long rc = -1;
	if( handle && curl_easy_getinfo(handle,info,&rc) != CURLE_OK )
	{
		AAMPLOG_WARN( "aamp_CurlEasyGetinfoLong failure" );
	}
	return rc;
}

char *aamp_CurlEasyGetinfoString( CURL *handle, CURLINFO info )
{
	char *rc = NULL;
	if( handle && curl_easy_getinfo(handle,info,&rc) != CURLE_OK )
	{
		AAMPLOG_WARN( "aamp_CurlEasyGetinfoString failure" );
	}
	return rc;
}



AampCurlDownloader::AampCurlDownloader() : mCurlMutex(),m_threadName(""),mDownloadActive(false),mCreatedNewFd(false),
			mCurl(nullptr),mDownloadUpdatedTime(0),mDownloadStartTime(0),mDnldCfg(),mDownloadResponse(nullptr),mHeaders(NULL),mWriteCallbackBufferSize(0),contentLength(0)

{
	// All download related configs are read here
	AAMPLOG_INFO("Create Curl Downloader Instance ");
}


AampCurlDownloader::~AampCurlDownloader()
{
	mDownloadActive = false;
	
	if(mCreatedNewFd && mCurl)
	{
		curl_easy_cleanup(mCurl);
	}
	if (mHeaders != NULL)
	{
		curl_slist_free_all(mHeaders);
		mHeaders = NULL;
	}
}

bool AampCurlDownloader::IsDownloadActive()
{
	return mDownloadActive;
}

int AampCurlDownloader::Download(const std::string &urlStr, std::shared_ptr<DownloadResponse> dnldData )
{
	AAMPLOG_WARN("DEBUG --> Entering AampCurlDownloader::Download ");
	int httpRetVal=0;
	int curlRetVal=0;
	int numDownloadAttempts=0;
	int numRetriesAllowed = mDnldCfg?mDnldCfg->iDownloadRetryCount:0;
	if(urlStr.size() == 0 || dnldData == nullptr)
	{
		AAMPLOG_ERR("Invalid inputs provided for download . Check the arguments. Url[%s] dnldData is Null[%d]", urlStr.c_str(), (dnldData == nullptr));
	}
	else if(mCurl)
	{
		if(!mDownloadActive)
		{
			{
				AAMPLOG_WARN("DEBUG--> Locking mCurlMutex in AampCurlDownloader::Download()");
				std::lock_guard<std::mutex> lock(mCurlMutex);
				mDownloadActive = true;
				mDownloadResponse = dnldData;
				mDownloadResponse->sEffectiveUrl = urlStr;
				CURL_EASY_SETOPT_STRING(mCurl, CURLOPT_URL, urlStr.c_str());
				AAMPLOG_WARN("DEBUG --> Unlocking mCurlMutex in AampCurlDownloader::Download()");
			}
			bool loopAgain = false;
			do{
				mDownloadStartTime = mDownloadUpdatedTime = NOW_STEADY_TS_MS;
				if( mDnldCfg && mDnldCfg->bCurlThroughput )
				{
					AAMPLOG_MIL( "curl-begin type=%d", eMEDIATYPE_MANIFEST);
				}
				AAMPLOG_WARN("DEBUG--> Setting download timeout to %d ms", mDnldCfg->iDownloadTimeout);
				AAMPLOG_WARN("DEBUG--> Setting connection timeout to %d ms", mDnldCfg->iCurlConnectionTimeout);
				curlRetVal = curl_easy_perform(mCurl);
				AAMPLOG_WARN("DEBUG--> curl_easy_perform returned %d", curlRetVal);
				loopAgain = false;
				numDownloadAttempts++;
				if(curlRetVal == CURLE_OK)
				{
					if( memcmp(urlStr.c_str(), "file:", 5) == 0 )
					{ // file uri scheme
						// libCurl does not provide CURLINFO_RESPONSE_CODE for 'file:' protocol.
						// Handle CURL_OK to http_code mapping here, other values handled below (see http_code = res).
						httpRetVal = mDownloadResponse->iHttpRetValue = 200;
					}
					else
					{
						httpRetVal = mDownloadResponse->iHttpRetValue = (int)aamp_CurlEasyGetinfoLong( mCurl, CURLINFO_RESPONSE_CODE );
						AAMPLOG_WARN("DEBUG--> HTTP Response code %d", httpRetVal);
					}

					numRetriesAllowed = mDnldCfg?mDnldCfg->iDownloadRetryCount:0;
					if(mDownloadResponse->iHttpRetValue == 408 && numRetriesAllowed == 0)
					{
						numRetriesAllowed++;
					}
					else if (mDownloadResponse->iHttpRetValue == 502 &&  mDnldCfg && mDnldCfg->iDownload502RetryCount)
					{
						numRetriesAllowed = mDnldCfg->iDownload502RetryCount;
					}
					AAMPLOG_WARN("DEBUG --> Number of Download attempts , Max Retries Allowed : %d %d",numDownloadAttempts,numRetriesAllowed);
					AAMPLOG_INFO("Download Status Ret:%d %d %s",mDownloadResponse->curlRetValue,mDownloadResponse->iHttpRetValue, urlStr.c_str());
					if ( numDownloadAttempts <= numRetriesAllowed )
					{
						//make http 408 retry-worthy as well
						if(mDownloadResponse->iHttpRetValue == 408 ||
								(mDownloadResponse->iHttpRetValue != 200 &&
								mDownloadResponse->iHttpRetValue != 204 &&
								mDownloadResponse->iHttpRetValue != 206 &&
								mDownloadResponse->iHttpRetValue >= 500 ))
						{
							AAMPLOG_WARN("Download failed due to Server error http-%d numDownloadAttempts %d numRetriesAllowed %d",mDownloadResponse->iHttpRetValue,numDownloadAttempts,numRetriesAllowed);
							int retryDelayMs = (mDownloadResponse->iHttpRetValue == 502) ? MIN_DELAY_BETWEEN_MANIFEST_UPDATE_FOR_502_MS : mDnldCfg->iDownloadRetryWaitMs;
							std::this_thread::sleep_for(std::chrono::milliseconds(retryDelayMs));
							loopAgain = true; //retry on manifest download failure
							// In the unlikely event that we get http failure status and also a http body then the
							// body will have got downloaded. We are not interested in it so clear the data.
							this->mDownloadResponse->mDownloadData.clear();
							mWriteCallbackBufferSize = 0;
						}
					}
				}
				//NETWORK_ERROR
				else
				{
					AAMPLOG_WARN("Inside Download failed due to Network error curl-%d ",curlRetVal);
					if(numDownloadAttempts <= numRetriesAllowed)
					{
						//Attempt retry for partial downloads, which have a higher chance to succeed
						if (curlRetVal == CURLE_COULDNT_CONNECT || curlRetVal == CURLE_OPERATION_TIMEDOUT || curlRetVal  == CURLE_PARTIAL_FILE)
						{
							AAMPLOG_WARN("DEBUG--> loopAgain called due to CURLE_COULDNT_CONNECT or CURLE_OPERATION_TIMEDOUT or CURLE_PARTIAL_FILE");
							loopAgain = true;
						}
					}
				}
			}while(loopAgain);

			/*
			 * Assigning curl error to http_code, for sending the error code as
			 * part of error event if required
			 * We can distinguish curl error and http error based on value
			 * curl errors are below 100 and http error starts from 100
			 */
			if(curlRetVal !=  CURLE_OK)
			{
				if( curlRetVal == CURLE_FILE_COULDNT_READ_FILE )
				{
					mDownloadResponse->iHttpRetValue = httpRetVal = 404; // translate file not found to URL not found
				}
				else if(mDownloadResponse->mAbortReason == eCURL_ABORT_REASON_LOW_BANDWIDTH_TIMEDOUT)
				{
					mDownloadResponse->iHttpRetValue = httpRetVal = CURLE_OPERATION_TIMEDOUT; // Timed out wrt configured low bandwidth timeout.
				}
				else
				{
					mDownloadResponse->iHttpRetValue = httpRetVal = curlRetVal;
				}
			}
			// update the download response metrics for success and failure case 
			// and for last attempt only (if retries enabled)
			AAMPLOG_WARN("DEBUG --> Calling updateResponseParams in AampCurlDownloader::Download()");
			updateResponseParams();
			mDownloadActive = false;		
			mDownloadResponse->curlRetValue = curlRetVal;
			if( mDnldCfg && mDnldCfg->bCurlThroughput )
			{
				AAMPLOG_MIL( "curl-end type=%d appConnect=%f redirect=%f error=%d",
					   eMEDIATYPE_MANIFEST,
					   mDownloadResponse->downloadCompleteMetrics.appConnect,
					   mDownloadResponse->downloadCompleteMetrics.redirect,
					   mDownloadResponse->iHttpRetValue );
			}
		}
		else
		{
			AAMPLOG_ERR("Already download in progress.Ignore new request for download %s",urlStr.c_str());
		}
	}
	else
	{
		AAMPLOG_ERR("Failed to Initialize CurlDownloader. mCurl is Null ");
	}
	return httpRetVal;
}

void AampCurlDownloader::updateResponseParams()
{
	std::lock_guard<std::mutex> lock(mCurlMutex);
	if(mCurl)
	{
		if(mDnldCfg->bNeedDownloadMetrics)
		{
			mDownloadResponse->downloadCompleteMetrics.total 	=	aamp_CurlEasyGetinfoDouble(mCurl, CURLINFO_TOTAL_TIME );
			mDownloadResponse->downloadCompleteMetrics.connect	=	aamp_CurlEasyGetinfoDouble(mCurl, CURLINFO_CONNECT_TIME);
			mDownloadResponse->downloadCompleteMetrics.resolve	=	aamp_CurlEasyGetinfoDouble(mCurl, CURLINFO_NAMELOOKUP_TIME);
			mDownloadResponse->downloadCompleteMetrics.appConnect	=	aamp_CurlEasyGetinfoDouble(mCurl, CURLINFO_APPCONNECT_TIME);
			mDownloadResponse->downloadCompleteMetrics.preTransfer	=	aamp_CurlEasyGetinfoDouble(mCurl, CURLINFO_PRETRANSFER_TIME);
			mDownloadResponse->downloadCompleteMetrics.startTransfer	=	aamp_CurlEasyGetinfoDouble(mCurl, CURLINFO_STARTTRANSFER_TIME);
			mDownloadResponse->downloadCompleteMetrics.redirect		=	aamp_CurlEasyGetinfoDouble(mCurl, CURLINFO_REDIRECT_TIME);
#if LIBCURL_VERSION_NUM >= 0x073700 // CURL version >= 7.55.0
			mDownloadResponse->downloadCompleteMetrics.dlSize = aamp_CurlEasyGetinfoOffset(mCurl, CURLINFO_SIZE_DOWNLOAD_T);
#else
#warning LIBCURL_VERSION<7.55.0
			mDownloadResponse->downloadCompleteMetrics.dlSize = aamp_CurlEasyGetinfoDouble(mCurl, CURLINFO_SIZE_DOWNLOAD);
#endif
			mDownloadResponse->downloadCompleteMetrics.reqSize	=	aamp_CurlEasyGetinfoLong(mCurl, CURLINFO_REQUEST_SIZE);
			mDownloadResponse->downloadCompleteMetrics.downloadbps = (long)(mDownloadResponse->downloadCompleteMetrics.dlSize*8) / mDownloadResponse->downloadCompleteMetrics.total;
		}
		
		if(mDownloadResponse->iHttpRetValue == 204)
		{		
			// check if any response available to search 	
			if(mDownloadResponse->mResponseHeader.size())
			{
				for ( std::string header : mDownloadResponse->mResponseHeader )
				{					
					if(STARTS_WITH_IGNORE_CASE(header.c_str(), LOCATION_HEADER_STRING))
					{
						mDownloadResponse->sEffectiveUrl =  	header.substr(std::string(LOCATION_HEADER_STRING).length() + 1);
						trim(mDownloadResponse->sEffectiveUrl);
						break;
					}
				}
			}
		}
		else
		{
			char *effectiveUrlStr = aamp_CurlEasyGetinfoString(mCurl, CURLINFO_EFFECTIVE_URL);
			if(effectiveUrlStr	!=	NULL)
			{
				mDownloadResponse->sEffectiveUrl.assign(effectiveUrlStr);
			}						
		}		
	}
	
}

void AampCurlDownloader::Initialize(std::shared_ptr<DownloadConfig> dnldCfg)
{
	AAMPLOG_WARN("DEBUG--> Inside Initialize in AampCurlDownloader");
	if(dnldCfg == nullptr)
		return;
	
	// Release and reset and previously called values
	AAMPLOG_WARN("DEBUG--> Calling Release in Initialize in AampCurlDownloader");
	Release();
	AAMPLOG_WARN("DEBUG--> Locking mCurlMutex in Initialize()");

	std::lock_guard<std::mutex> lock(mCurlMutex);
	mDnldCfg = std::move(dnldCfg);
	//mDnldCfg->show();
	if (!mDnldCfg->pCurl)
	{
		if(mCurl == NULL)
		{
			AAMPLOG_WARN("DEBUG--> Calling curl_easy_init in AampCurlDownloader");
			mCurl = curl_easy_init();
			mCreatedNewFd = true;
		}

	}
	else
	{
		if(mCreatedNewFd && mCurl)
		{
			AAMPLOG_WARN("DEBUG--> Calling curl_easy_cleanup in AampCurlDownloader");
			// Whatever created by this module should be freed by this module
			// AampCurlDownloader is not responsible for the curl handles provided for download
			curl_easy_cleanup(mCurl);
			mCreatedNewFd = false;
		}
		mCurl =	mDnldCfg->pCurl;
	}
	AAMPLOG_WARN("DEBUG-->  mCurl value in Initialize(): %p", mCurl);
	AAMPLOG_WARN("DEBUG--> Calling updateCurlParams in Initialize()");
	updateCurlParams();
	AAMPLOG_WARN("DEBUG--> Unlocking mCurlMutex in Initialize()");

}


void AampCurlDownloader::Release()
{
	AAMPLOG_WARN("DEBUG--> Inside Release in AampCurlDownloader");
	AAMPLOG_WARN("DEBUG--> Locking mCurlMutex in Release()");
	std::lock_guard<std::mutex> lock(mCurlMutex);
	mDownloadActive = false;
	mDownloadUpdatedTime = 0 ;
	mDownloadStartTime =  0;
	mWriteCallbackBufferSize = 0;
	if (mHeaders != NULL)
	{
		curl_slist_free_all(mHeaders);
		mHeaders = NULL;
	}
	AAMPLOG_WARN("DEBUG--> Unlocking mCurlMutex in Release()");
	AAMPLOG_WARN("DEBUG--> Exit Release in AampCurlDownloader()");
}


void AampCurlDownloader::Clear()
{
	std::lock_guard<std::mutex> lock(mCurlMutex);
	mDownloadActive = false;
	mDownloadUpdatedTime = 0 ;
	mDownloadStartTime =  0;	

	// Clear all the partially stored data before retry attempt
	if(mDownloadResponse)
		mDownloadResponse->clear();
}


void AampCurlDownloader::updateCurlParams()
{
	AAMPLOG_WARN("DEBUG--> Inside updateCurlParams in AampCurlDownloader");
	if(mDnldCfg->bVerbose)
	{
		CURL_EASY_SETOPT_LONG(mCurl, CURLOPT_VERBOSE, 1L);
	}

	if(mDnldCfg->eRequestType != eCURL_GET)
	{
		AAMPLOG_WARN("DEBUG--> Setting request type to %d",mDnldCfg->eRequestType);
		if(eCURL_DELETE == mDnldCfg->eRequestType)
		{
			CURL_EASY_SETOPT_STRING(mCurl, CURLOPT_CUSTOMREQUEST, "DELETE");
		}
		else if(eCURL_POST == mDnldCfg->eRequestType)
		{
			CURL_EASY_SETOPT_LONG(mCurl, CURLOPT_POSTFIELDSIZE, mDnldCfg->postData.size());
			CURL_EASY_SETOPT_STRING(mCurl, CURLOPT_POSTFIELDS,(uint8_t * )mDnldCfg->postData.c_str());
		}
	}
		
	CURL_EASY_SETOPT_LONG(mCurl, CURLOPT_NOSIGNAL, 1L);
	//curl_easy_setopt(curl, CURLOPT_READFUNCTION, read_callback); // unused
	CURL_EASY_SETOPT_POINTER(mCurl, CURLOPT_WRITEDATA, this);
	CURL_EASY_SETOPT_POINTER(mCurl, CURLOPT_PROGRESSDATA, this);
	CURL_EASY_SETOPT_FUNC(mCurl, CURLOPT_XFERINFOFUNCTION, AampCurlDownloader::ProgressCallback);
	//CURL_EASY_SETOPT(mCurl, CURLOPT_PROGRESSFUNCTION, AampCurlDownloader::ProgressCallback);
	if(!mDnldCfg->bIgnoreResponseHeader)
	{
		CURL_EASY_SETOPT_POINTER(mCurl, CURLOPT_HEADERDATA, this);
		CURL_EASY_SETOPT_FUNC(mCurl, CURLOPT_HEADERFUNCTION, AampCurlDownloader::HeaderCallback);
	}
	CURL_EASY_SETOPT_FUNC(mCurl, CURLOPT_WRITEFUNCTION, AampCurlDownloader::WriteCallback);
	CURL_EASY_SETOPT_LONG(mCurl, CURLOPT_TIMEOUT, mDnldCfg->iDownloadTimeout);
	CURL_EASY_SETOPT_LONG(mCurl, CURLOPT_CONNECTTIMEOUT, mDnldCfg->iCurlConnectionTimeout);
	CURL_EASY_SETOPT_LONG(mCurl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_WHATEVER);
	CURL_EASY_SETOPT_LONG(mCurl, CURLOPT_FOLLOWLOCATION, 1L);
	CURL_EASY_SETOPT_LONG(mCurl, CURLOPT_NOPROGRESS, 0L); // enable progress meter (off by default)
	
	CURL_EASY_SETOPT_STRING(mCurl, CURLOPT_USERAGENT, mDnldCfg->userAgentString.c_str());
	CURL_EASY_SETOPT_STRING(mCurl, CURLOPT_ACCEPT_ENCODING, "");//Enable all the encoding formats supported by client
	//CURL_EASY_SETOPT(curlEasyhdl, CURLOPT_SSL_CTX_FUNCTION, ssl_callback); //Check for downloads disabled in btw ssl handshake
	//CURL_EASY_SETOPT(curlEasyhdl, CURLOPT_SSL_CTX_DATA, aamp);
	CURL_EASY_SETOPT_LONG(mCurl, CURLOPT_DNS_CACHE_TIMEOUT,mDnldCfg->iDnsCacheTimeOut);
	
	if (!mDnldCfg->proxyName.empty())
	{
		/* use this proxy */
		CURL_EASY_SETOPT_STRING(mCurl, CURLOPT_PROXY, mDnldCfg->proxyName.c_str());
		/* allow whatever auth the proxy speaks */
		CURL_EASY_SETOPT_LONG(mCurl, CURLOPT_PROXYAUTH, CURLAUTH_ANY);
	}
	
	if(!mDnldCfg->bSSLVerifyPeer)
	{
		CURL_EASY_SETOPT_LONG(mCurl, CURLOPT_SSL_VERIFYHOST, 0L);
		CURL_EASY_SETOPT_LONG(mCurl, CURLOPT_SSL_VERIFYPEER, 0L);
	}
	else
	{
		CURL_EASY_SETOPT_LONG(mCurl, CURLOPT_SSLVERSION, mDnldCfg->lSupportedTLSVersion);
		CURL_EASY_SETOPT_LONG(mCurl, CURLOPT_SSL_VERIFYPEER, 1L);
	}

	if (mDnldCfg->sCustomHeaders.size() > 0)
	{
		std::string customHeader;
		std::string headerValue;
		for (std::unordered_map<std::string, std::vector<std::string>>::iterator it = mDnldCfg->sCustomHeaders.begin();
			 it != mDnldCfg->sCustomHeaders.end(); it++)
		{
			customHeader.clear();
			headerValue.clear();
			customHeader.insert(0, it->first);
			headerValue = it->second.at(0);
			customHeader.push_back(' ');
			customHeader.append(headerValue);
			mHeaders = curl_slist_append(mHeaders, customHeader.c_str());
		}
		CURL_EASY_SETOPT_LIST(mCurl, CURLOPT_HTTPHEADER, mHeaders);
	}
	
	return ;
}


size_t AampCurlDownloader::WriteCallback(void *buffer, size_t sz, size_t nmemb, void *userdata)
{
	// Call non-static member function.
	AAMPLOG_WARN("DEBUG--> Inside WriteCallback in AampCurlDownloader");
	size_t ret = 0;
	AampCurlDownloader *context = static_cast<AampCurlDownloader *>(userdata);
	if(context != NULL)
	{
		if( context->mDnldCfg && context->mDnldCfg->bCurlThroughput )
		{
			AAMPLOG_MIL( "curl-write type=%d size=%zu total=%zu", eMEDIATYPE_MANIFEST, sz*nmemb, context->contentLength );
		}
		ret = context->write_callback(buffer, sz, nmemb);
	}
	return ret;
}

size_t AampCurlDownloader::write_callback(void *buffer, size_t sz, size_t nmemb)
{
	size_t retSize = sz * nmemb;
    AAMPLOG_WARN("DEBUG--> Inside write_callback in AampCurlDownloader");
	if(retSize)
	{
		std::lock_guard<std::mutex> lock(mCurlMutex);
		std::vector<std::uint8_t> op1;
		std::uint8_t *bufferS = static_cast<std::uint8_t*>( buffer );
		std::uint8_t *bufferE = bufferS + retSize;
		std::copy(bufferS, bufferE, std::back_inserter(this->mDownloadResponse->mDownloadData));
		AAMPLOG_WARN("DEBUG--> Copied %zu bytes to mDownloadResponse->mDownloadData", retSize);
		mDownloadUpdatedTime = NOW_STEADY_TS_MS;
		mWriteCallbackBufferSize += retSize;
	}

	return retSize;
}

size_t AampCurlDownloader::HeaderCallback(char *ptr, size_t size, size_t nmemb, void *userdata)
{ // Call non-static member function.
	AAMPLOG_WARN("DEBUG--> Inside HeaderCallback in AampCurlDownloader");
	size_t len = nmemb * size;
	AampCurlDownloader *context = static_cast<AampCurlDownloader *>(userdata);
	if(context != NULL)
	{
		context->header_callback(ptr, len);
		if( len>=2 && ptr[len-2]=='\r' && ptr[len-1]=='\n' )
		{ // CRLF terminated curl header as expected
			if( STARTS_WITH_IGNORE_CASE(ptr, CONTENTLENGTH_STRING) )
			{
				int contentLengthStartPosition = STRLEN_LITERAL(CONTENTLENGTH_STRING);
				const char * contentLengthStr = ptr + contentLengthStartPosition;
				context->contentLength = atoi(contentLengthStr);
			}
		}
	}
	return len;
}

void AampCurlDownloader::header_callback(char *ptr, size_t len )
{
	AAMPLOG_WARN("DEBUG--> Inside header_callback in AampCurlDownloader");
	if(len)
	{
		std::lock_guard<std::mutex> lock(mCurlMutex);
		std::string str;
		str.assign(ptr, ptr+len);
		size_t pos = str.find('\n');
		AAMPLOG_WARN("DEBUG--> Extracted header line: %s", str.c_str());
		if( pos != std::string::npos)
		{
			str.erase(pos);
		}
		if(str.size())
		{
			this->mDownloadResponse->mResponseHeader.push_back(std::move(str));
		}
	}
	AAMPLOG_WARN("DEBUG--> Exiting header_callback in AampCurlDownloader");
}

int AampCurlDownloader::ProgressCallback(
										 void *clientp, // app-specific as optionally set with CURLOPT_PROGRESSDATA
										 double dltotal, // total bytes expected to download
										 double dlnow, // downloaded bytes so far
										 double ultotal, // total bytes expected to upload
										 double ulnow // uploaded bytes so far
)
{

	AAMPLOG_WARN("DEBUG--> Inside ProgressCallback in AampCurlDownloader");
	int ret = 0;
	AampCurlDownloader *context = (AampCurlDownloader *)clientp;
	
	if(context)
	{
		AAMPLOG_WARN("DEBUG--> Calling progress_callback in ProgressCallback in AampCurlDownloader");
		ret = context->progress_callback ( dltotal, dlnow, ultotal, ulnow );
	}
	return ret;
}

int AampCurlDownloader::progress_callback(
					 double dltotal, // total bytes expected to download
					 double dlnow, // downloaded bytes so far
					 double ultotal, // total bytes expected to upload
					 double ulnow // uploaded bytes so far
)
{
	AAMPLOG_WARN("DEBUG--> Inside progress_callback in AampCurlDownloader");
	int rc = 0;
	AAMPLOG_WARN("DEBUG--> Locking mCurlMutex in progress_callback()");
	std::lock_guard<std::mutex> lock(mCurlMutex);
	if (!mDownloadActive)
	{
		rc = -1; // CURLE_ABORTED_BY_CALLBACK
		AAMPLOG_WARN("Abort download... Release called");
	}
	else
	{
		AAMPLOG_WARN("DEBUG--> Inside else loop of progress_callback in AampCurlDownloader");
		AAMPLOG_WARN("DEBUG--> dlnow:%f startTimeout:%d stallTimeout:%d Time:%lld StartTime:%lld",dlnow,mDnldCfg->iStartTimeout,mDnldCfg->iStallTimeout,NOW_STEADY_TS_MS,mDownloadStartTime);
		if (this->mWriteCallbackBufferSize == 0 && mDnldCfg->iStartTimeout > 0)
		{ // check to handle scenario where <startTimeout> seconds delay occurs without any bytes having been downloaded (stall at start)
			double timeElapsedInSec = (double)(NOW_STEADY_TS_MS - mDownloadStartTime) /1000;
			AAMPLOG_WARN("DEBUG--> timeElapsedInSec: %.2f", timeElapsedInSec);
			if (timeElapsedInSec >= (mDnldCfg->iStartTimeout))
			{
				AAMPLOG_WARN("Abort download as no data received for %.2f seconds", timeElapsedInSec);
				mDownloadResponse->mAbortReason = eCURL_ABORT_REASON_START_TIMEDOUT;
				rc = -1;
			}

		}
		else if( this->mWriteCallbackBufferSize > 0 && mDnldCfg->iStallTimeout > 0)
		{
			//if(this->mDownloadResponse->mDownloadData.size())
			{
				double timeElapsedSinceLastUpdate = (double)(NOW_STEADY_TS_MS - mDownloadUpdatedTime) / 1000; //in secs
				AAMPLOG_WARN("DEBUG--> timeElapsedSinceLastUpdate: %.2f", timeElapsedSinceLastUpdate);
				if (timeElapsedSinceLastUpdate >= (mDnldCfg->iStallTimeout))
				{ // no change for at least <stallTimeout> seconds - consider download stalled and abort
					AAMPLOG_WARN("Abort download as mid-download stall detected for %.2f seconds, download size:%.2f bytes", timeElapsedSinceLastUpdate, dlnow);
					mDownloadResponse->mAbortReason = eCURL_ABORT_REASON_STALL_TIMEDOUT;
					rc = -1;
				}
			}
			if ( mDownloadResponse->progressMetrics.dlnow != dlnow)
			{
				AAMPLOG_WARN("DEBUG--> Updating progressMetrics: dlnow=%.2f, dltotal=%.2f", dlnow, dltotal);
				mDownloadResponse->progressMetrics.dlnow  	= dlnow;
				mDownloadResponse->progressMetrics.dlTotal  = dltotal;
			}
		}
		else if((this->mWriteCallbackBufferSize > 0 && mDnldCfg->iLowBWTimeout > 0))
		{
			AAMPLOG_WARN("DEBUG--> Inside low bandwidth timeout check in progress_callback in AampCurlDownloader");
			double elapsedTimeMs = (double)(NOW_STEADY_TS_MS - mDownloadStartTime);
			AAMPLOG_WARN("DEBUG--> elapsedTimeMs: %.2f", elapsedTimeMs);
			if( elapsedTimeMs >= mDnldCfg->iLowBWTimeout*1000 )
			{
				if(dltotal)
				{
					double predictedTotalDownloadTimeMs = elapsedTimeMs*dltotal/dlnow;
					if( predictedTotalDownloadTimeMs > (mDnldCfg->iDownloadTimeout * 1000) )
					{
						AAMPLOG_WARN("lowBWTimeout=%u predictedTotalDownloadTime=%fs>%us (download timeout)",
								mDnldCfg->iLowBWTimeout,
								predictedTotalDownloadTimeMs/1000.0,
								mDnldCfg->iDownloadTimeout);
						mDownloadResponse->mAbortReason = eCURL_ABORT_REASON_LOW_BANDWIDTH_TIMEDOUT;
						rc = -1;
					}
				}
			}

		}

	}
	
	AAMPLOG_WARN("DEBUG--> Exiting progress_callback in AampCurlDownloader");
	return rc;
	
}

size_t AampCurlDownloader::GetDataString(std::string &dataStr)
{
	size_t ret=0;
	if(mDownloadResponse != nullptr)
	{
		dataStr =	std::string( mDownloadResponse->mDownloadData.begin(), mDownloadResponse->mDownloadData.end());
		ret 	=	mDownloadResponse->mDownloadData.size();
	}
	return ret;
}

