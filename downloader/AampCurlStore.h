/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2022 RDK Management
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
 * @file AampCurlStore.h
 * @brief Advanced Adaptive Media Player (AAMP) Curl store
 */

#ifndef AAMPCURLSTORE_H
#define AAMPCURLSTORE_H

#include "AampCurlDefine.h"
#include "priv_aamp.h"
#include <map>
#include <iterator>
#include <vector>
#include <glib.h>
#include <mutex>

#define eCURL_MAX_AGE_TIME			( (300) * (1000) )			/**< 5 mins - 300 secs - Max age for a connection */

/**
 * @enum AampCurlStoreErrorCode
 * @brief Error codes returned by curlstore
 */
enum AampCurlStoreErrorCode
{
	eCURL_STORE_HOST_NOT_AVAILABLE,
	eCURL_STORE_SOCK_NOT_AVAILABLE,
	eCURL_STORE_HOST_SOCK_AVAILABLE
};

/**
 * @struct curldatasharelock
 * @brief locks used when lock/unlock callback occurs for different shared data
 */
typedef struct curldatasharelock
{
	std::mutex mCurlSharedlock;
	std::mutex mDnsCurlShareMutex;
	std::mutex mSslCurlShareMutex;

	curldatasharelock():mCurlSharedlock(), mDnsCurlShareMutex(),mSslCurlShareMutex(){}
}CurlDataShareLock;

typedef struct curlstruct
{
	CURL *curl;
	int curlId;
	long long eHdlTimestamp;

	curlstruct():curl(NULL), eHdlTimestamp(0),curlId(0){}
}CurlHandleStruct;

/**
 * @struct curlstorestruct
 * @brief structure to store curl easy, shared handle & locks for a host
 */
typedef struct curlstorestruct
{
	std::deque<CurlHandleStruct> mFreeQ;
	CURLSH* mCurlShared;
	CurlDataShareLock *pstShareLocks;

	unsigned int mCurlStoreUserCount;
	long long timestamp;

	curlstorestruct():mCurlShared(NULL), pstShareLocks(NULL), timestamp(0), mCurlStoreUserCount(0), mFreeQ()
	{}

	//Disabled for now
	curlstorestruct(const curlstorestruct&) = delete;
	curlstorestruct& operator=(const curlstorestruct&) = delete;

}CurlSocketStoreStruct;

/**
 * @class CurlStore
 * @brief Singleton curlstore to save/reuse curl handles
 */
class CurlStore
{
private:
	std::mutex mCurlInstLock{};
	int MaxCurlSockStore;

	typedef std::unordered_map <std::string, CurlSocketStoreStruct*> CurlSockData ;
	typedef std::unordered_map <std::string, CurlSocketStoreStruct*>::iterator CurlSockDataIter;
	CurlSockData umCurlSockDataStore;

	/**
	 * @param[in] hostname - hostname part from url
	 */
	void FlushCurlSockForHost(const std::string &hostname);

protected:
	CurlStore(PrivateInstanceAAMP *pAamp);
	~CurlStore();

public:
	/**
	 * @param[in] hostname - hostname part from url
	 * @param[in] CurlIndex - Index of Curl instance
	 * @param[out] curl - curl easy handle from curl store.
	 * @return AampCurlStoreErrorCode enum type
	 */
	AampCurlStoreErrorCode GetFromCurlStore ( const std::string &hostname, AampCurlInstance CurlIndex, CURL **curl );

	/**
	 * @param[in] hostname - hostname part from url
	 * @param[in] CurlIndex - Index of Curl instance
	 * @param[in] count - No of curl handles
	 * @param[out] priv - curl easy handle from curl store will get stored in priv instance
	 * @return AampCurlStoreErrorCode enum type
	 */
	AampCurlStoreErrorCode GetFromCurlStoreBulk ( const std::string &hostname, AampCurlInstance CurlIndex, int count, PrivateInstanceAAMP *pAamp, bool HostCurlFd );

	/**
	 * @param[in] hostname - hostname part from url
	 * @param[in] CurlIndex - Index of Curl instance
	 * @param[in] curl - curl easy handle to save in curl store.
	 * @return void
	 */
	void KeepInCurlStore ( const std::string &hostname, AampCurlInstance CurlIndex, CURL *curl );

	/**
	 * @param[in] hostname - hostname part from url
	 * @param[in] CurlIndex - Index of Curl instance
	 * @param[in] count - No of curl handles
	 * @param[out] priv - curl easy handles in priv instance, saved in curl store
	 * @return void
	 */
	void KeepInCurlStoreBulk ( const std::string &hostname, AampCurlInstance CurlIndex, int count, PrivateInstanceAAMP *pAamp, bool HostCurlFd );

	/**
	 * @param void
	 * @return void
	 */
	void RemoveCurlSock ( void );

	/**
	 * @param trace - true to print curl store data, otherwise false.
	 * @return void
	 */
	void ShowCurlStoreData ( bool trace = true );

	/**
	 * @param[out] privContext - priv aamp instance in which created curl handles will be assigned
	 * @param[in] startIdx - Index of Curl instance
	 * @param[in] instanceCount - No of curl handles
	 * @param[in] proxyName - proxy name
	 * @return void
	 */
	void CurlInit(PrivateInstanceAAMP *pAamp, AampCurlInstance startIdx, unsigned int instanceCount, std::string proxyName, const std::string &remotehost=std::string("") );

	/**
	 * @param[out] privContext - priv aamp instance from which curl handles will be terminated or stored
	 * @param[in] startIdx - Index of Curl instance
	 * @param[in] instanceCount - No of curl handles
	 * @param[in] isFlushFds - indicates to flush the fds corresponding to remotehost
	 * @param[in] remotehost - remote host address
	 * @return void
	 */
	void CurlTerm(PrivateInstanceAAMP *pAamp, AampCurlInstance startIdx, unsigned int instanceCount, bool isFlushFds=false, const std::string &remotehost=std::string(""));

	/**
	 * @param[in] pAamp - Private aamp instance
	 * @param[in] url - request url
	 * @param[in] startIdx - Index of curl instance.
	 * @return - curl easy handle
	 */
	CURL* GetCurlHandle(PrivateInstanceAAMP *pAamp, std::string url, AampCurlInstance startIdx );

	/**
	 * @param[in] pAamp - Private aamp instance
	 * @param[in] url - request url
	 * @param[in] startIdx - Index of curl instance.
	 * @param[in] curl - curl handle to be saved
	 * @return void
	 */
	void SaveCurlHandle ( PrivateInstanceAAMP *pAamp, std::string url, AampCurlInstance startIdx, CURL *curl );

	/**
	 * @param[in] hostname - Host name to create a curl store
	 * @return - Curl store struct pointer
	 */
	CurlSocketStoreStruct *CreateCurlStore ( const std::string &hostname );

	/**
	 * @param[in] privContext - Aamp context
	 * @param[in] proxyName - Network proxy Name
	 * @param[in] instId - Curl instance id
	 * @return - Curl easy handle
	 */
	CURL* CurlEasyInitWithOpt ( PrivateInstanceAAMP *pAamp, const std::string &proxyName, int instId );

	/**
	 * @param[in] CurlSock - Curl socket struct
	 * @param[in] instId - Curl instance id
	 * @return - Curl easy handle
	 */
	CURL* GetCurlHandleFromFreeQ ( CurlSocketStoreStruct *CurlSock, int instId );

	// Copy constructor and Copy assignment disabled
	CurlStore(const CurlStore&) = delete;
	CurlStore& operator=(const CurlStore&) = delete;

	/**
	 * @param[in] pContext - Private aamp instance
	 * @return CurlStore - Singleton instance object
	 */
	static CurlStore& GetCurlStoreInstance(PrivateInstanceAAMP *pAamp);
};

/**
 * @struct CurlCallbackContext
 * @brief context during curl callbacks
 */
struct CurlCallbackContext
{
	// HTTP/1.1 Chunked Transfer Protocol
	typedef enum
	{
		eTRANSFER_STATE_READING_CHUNK_SIZE, // reading hascii chunk size
		eTRANSFER_STATE_PENDING_CHUNK_START_LF, // chunk size read, along with following CR delimiter - waiting for LF
		eTRANSFER_STATE_READING_CHUNK_DATA, // collecting binary payload data for chunk
		eTRANSFER_STATE_PENDING_CHUNK_END_CR, // chunk payload has been read, next byte expected to be chunk-end CR
		eTRANSFER_STATE_PENDING_CHUNK_END_LF // chunk payload and first CR delimiter read; waiting for LF
	} TransferState;
	struct
	{
		size_t remaining;
		TransferState state;
	} mTransferState;
	
	PrivateInstanceAAMP *aamp;
	AampMediaType mediaType;
	std::vector<std::string> allResponseHeaders;
	AampGrowableBuffer *buffer;
	httpRespHeaderData *responseHeaderData;
	long bitrate;
	bool downloadIsEncoded;
	//represents transfer-encoding based download
	bool chunkedDownload;
	std::string remoteUrl;
	size_t contentLength;
	long long downloadStartTime;
	long long processDelay; /**< Indicate the external process delay in curl operation; especially for lld*/

	CurlCallbackContext() : aamp(NULL), buffer(NULL), responseHeaderData(NULL),bitrate(0),downloadIsEncoded(false), chunkedDownload(false),  mediaType(eMEDIATYPE_DEFAULT), remoteUrl(""), allResponseHeaders{""}, contentLength(0),downloadStartTime(-1), processDelay(0), mTransferState{}
	{

	}
	CurlCallbackContext(PrivateInstanceAAMP *_aamp, AampGrowableBuffer *_buffer) : aamp(_aamp), buffer(_buffer), responseHeaderData(NULL),bitrate(0),downloadIsEncoded(false),  chunkedDownload(false), mediaType(eMEDIATYPE_DEFAULT), remoteUrl(""), allResponseHeaders{""},  contentLength(0),downloadStartTime(-1){}

	~CurlCallbackContext() {}

	CurlCallbackContext(const CurlCallbackContext &other) = delete;
	CurlCallbackContext& operator=(const CurlCallbackContext& other) = delete;
};

/**
 * @struct CurlProgressCbContext
 * @brief context during curl progress callbacks
 */
struct CurlProgressCbContext
{
	PrivateInstanceAAMP *aamp;
	AampMediaType mediaType;
	CurlProgressCbContext() : aamp(NULL), mediaType(eMEDIATYPE_DEFAULT), downloadStartTime(-1), abortReason(eCURL_ABORT_REASON_NONE), downloadUpdatedTime(-1), startTimeout(-1), stallTimeout(-1), downloadSize(-1), downloadNow(-1), downloadNowUpdatedTime(-1), dlStarted(false), fragmentDurationMs(-1), remoteUrl(""), lowBWTimeout(-1) {}
	CurlProgressCbContext(PrivateInstanceAAMP *_aamp, long long _downloadStartTime) : aamp(_aamp), mediaType(eMEDIATYPE_DEFAULT),downloadStartTime(_downloadStartTime), abortReason(eCURL_ABORT_REASON_NONE), downloadUpdatedTime(-1), startTimeout(-1), stallTimeout(-1), downloadSize(-1), downloadNow(-1), downloadNowUpdatedTime(-1), dlStarted(false), fragmentDurationMs(-1), remoteUrl(""), lowBWTimeout(-1) {}

	~CurlProgressCbContext() {}

	CurlProgressCbContext(const CurlProgressCbContext &other) = delete;
	CurlProgressCbContext& operator=(const CurlProgressCbContext& other) = delete;

	long long downloadStartTime;
	long long downloadUpdatedTime;
	int startTimeout;
	int stallTimeout;
	int lowBWTimeout;
	double downloadSize;
	CurlAbortReason abortReason;
	double downloadNow;
	long long downloadNowUpdatedTime;
	bool dlStarted;
	int fragmentDurationMs;
	std::string remoteUrl;
};

int GetCurlResponseCode( CURL *curlhandle );

#endif //AAMPCURLSTORE_H
