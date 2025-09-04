/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2025 RDK Management
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

#ifndef _CURL_INTERFACE_H_
#define _CURL_INTERFACE_H_

#include <stddef.h>
#include "AampConfig.h"
#include "AampLLDASHData.h"
#include "AampEvent.h"
#include "AampCurlDefine.h"
#include <HybridABRManager.h>

/**
 * @brief To store Set Cookie: headers and X-Reason headers in HTTP Response
 */
struct httpRespHeaderData {
	httpRespHeaderData() : type(0), data("")
	{
	}
	int type;             /**< Header type */
	std::string data;     /**< Header value */
};

/**
 * @struct SpeedCache
 * @brief Stores the information for cache speed
 */
struct SpeedCache
{
	long last_sample_time_val;
	long prev_dlnow;
	long prevSampleTotalDownloaded;
	long totalDownloaded;
	long speed_now;
	long start_val;
	bool bStart;

	double totalWeight;
	double weightedBitsPerSecond;
	std::vector< std::pair<double,long> > mChunkSpeedData;

	SpeedCache() : last_sample_time_val(0), prev_dlnow(0), prevSampleTotalDownloaded(0), totalDownloaded(0), speed_now(0), start_val(0), bStart(false) , totalWeight(0), weightedBitsPerSecond(0), mChunkSpeedData()
	{
	}
};

/**
 * @brief Struct to store parsed url hostname & its type
 */
typedef struct AampUrlInfo
{
	std::string hostname;
	bool isRemotehost;

	AampUrlInfo():hostname(""),isRemotehost(true)
	{}

	//Disabled
	AampUrlInfo(const AampUrlInfo&) = delete;
	AampUrlInfo& operator=(const AampUrlInfo&) = delete;
}AampURLInfoStruct;

class CurlCallbacks
{
public:
	AampConfig *mConfig;
	std::recursive_mutex mLock;
	CURLSH* mCurlShared;
	bool mDownloadsEnabled;
	std::map<AampMediaType, bool> mMediaDownloadsEnabled;
	std::vector<std::string> manifestHeadersNeeded;
	std::map<std::string, std::string> httpHeaderResponses;
	std::string mTsbRecordingId;
	bool mProfileCappedStatus;
	long curlDLTimeout[eCURLINSTANCE_MAX];
	HybridABRManager mhAbrManager;
	AampURLInfoStruct mOrigManifestUrl;
	
public:
	virtual class MediaStreamContext* GetMediaStreamContext(AampMediaType type) = 0;
	virtual AampLLDashServiceData*  GetLLDashServiceData(void) = 0;
	virtual bool GetLLDashChunkMode() = 0;
	virtual bool IsFirstRequestToFog() = 0;
	virtual bool IsEventListenerAvailable(AAMPEventType eventType) = 0;
	virtual bool CheckABREnabled() = 0;
	virtual struct SpeedCache * GetLLDashSpeedCache() = 0;
	virtual bool IsEASContent() = 0;
	virtual long getCurrentContentDownloadSpeed(CurlCallbacks *aamp, AampMediaType mediaType, bool bDownloadStart, long start, double dlnow) = 0;
	
	/**
	 * @fn HandleSSLWriteCallback
	 *
	 * @param ptr pointer to buffer containing the data
	 * @param size size of the buffer
	 * @param nmemb number of bytes
	 * @param userdata CurlCallbackContext pointer
	 * @retval size consumed or 0 if interrupted
	 */
	virtual size_t HandleSSLWriteCallback( const char *ptr, size_t size, size_t nmemb, void* userdata ) = 0;

	/**
	 * @fn HandleSSLProgressCallback
	 *
	 * @param clientp app-specific as optionally set with CURLOPT_PROGRESSDATA
	 * @param dltotal total bytes expected to download
	 * @param dlnow downloaded bytes so far
	 * @param ultotal total bytes expected to upload
	 * @param ulnow uploaded bytes so far
	 * @retval negative value to abort, zero otherwise
	 */
	virtual int HandleSSLProgressCallback( void *clientp, double dltotal, double dlnow, double ultotal, double ulnow ) = 0;

	/**
	 * @fn HandleSSLHeaderCallback
	 *
	 * @param ptr pointer to buffer containing the data
	 * @param size size of the buffer
	 * @param nmemb number of bytes
	 * @param user_data  CurlCallbackContext pointer
	 * @retval returns size * nmemb
	 */
	virtual size_t HandleSSLHeaderCallback( const char *ptr, size_t size, size_t nmemb, void* userdata ) = 0;
};

#endif // _CURL_INTERFACE_H_
