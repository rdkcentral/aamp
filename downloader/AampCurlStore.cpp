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
 * @file AampCurlStore.cpp
 * @brief Advanced Adaptive Media Player (AAMP) Curl store
 */

#include "AampCurlStore.h"
#include "AampDefine.h"
#include "AampUtils.h"
#include <mutex>

// Curl callback functions
//
// VPAAMP-558: per-host embedded lock (curlstorestruct::mShareLock).
//
// Each curlstorestruct embeds a CurlDataShareLock as a value member.
// CURLSHOPT_USERDATA is set to &CurlSock->mShareLock so that DNS, SSL, and
// general libcurl-share operations for each CDN hostname use independent
// per-host mutexes, restoring the parallelism lost by the
// VPAAMP-139 single-static-lock fix.
//
// Safety: every cleanup path (RemoveCurlSock, ~CurlStore, FlushCurlSockForHost)
// follows the order:
//   1. curl_easy_cleanup all queued handles  (no further easy-handle callbacks)
//   2. curl_share_cleanup(mCurlShared)        (share teardown; lock may fire)
//   3. SAFE_DELETE(CurlSock)                  (destroys embedded mShareLock)
// The embedded lock is therefore always alive when curl_share_cleanup needs it.
//
// Non-store path: gCurlShLock (file-scope static, process lifetime) is passed
// as CURLSHOPT_USERDATA in CurlInit when the curl store is disabled, giving
// the same per-type DNS / SSL / general mutex granularity.
static CurlDataShareLock gCurlShLock; // process-lifetime; used by non-store path

/**
 * @brief
 * @param curl ptr to CURL instance
 * @param data curl data lock
 * @param access curl access lock
 * @param user_ptr CurlCallbackContext pointer
 * @retval void
 */
static void curl_lock_callback(CURL *curl, curl_lock_data data, curl_lock_access access, void *user_ptr)
{
	std::mutex *pCurlShareLock = NULL;
	CurlDataShareLock *locks =(CurlDataShareLock *)user_ptr;

	(void)access; /* unused */
	(void)curl; /* unused */

	if(locks)
	{
		switch ( data )
		{
			case CURL_LOCK_DATA_DNS:
				pCurlShareLock = &locks->mDnsCurlShareMutex;
				break;
			case CURL_LOCK_DATA_SSL_SESSION:
				pCurlShareLock = &locks->mSslCurlShareMutex;
				break;
			default:
				pCurlShareLock = &locks->mCurlSharedlock;
				break;
		}
		if( pCurlShareLock )
		{
			pCurlShareLock->lock();
		}
	}
	else
	{
		gCurlShLock.mCurlSharedlock.lock(); // defensive fallback; user_ptr should never be NULL
	}
}

/**
 * @brief
 * @param curl ptr to CURL instance
 * @param data curl data lock
 * @param user_ptr CurlCallbackContext pointer
 * @retval void
 */
static void curl_unlock_callback(CURL *curl, curl_lock_data data, void *user_ptr)
{
	std::mutex *pCurlShareLock = NULL;
	CurlDataShareLock *locks =(CurlDataShareLock *)user_ptr;

	(void)curl; /* unused */

	if(locks)
	{
		switch ( data )
		{
			case CURL_LOCK_DATA_DNS:
				pCurlShareLock = &locks->mDnsCurlShareMutex;
				break;
			case CURL_LOCK_DATA_SSL_SESSION:
				pCurlShareLock = &locks->mSslCurlShareMutex;
				break;
			default:
				pCurlShareLock = &locks->mCurlSharedlock;
				break;
		}
		if( pCurlShareLock )
		{
			pCurlShareLock->unlock();
		}
	}
	else
	{
		gCurlShLock.mCurlSharedlock.unlock(); // defensive fallback; user_ptr should never be NULL
	}
}

/**
 * @brief write callback to be used by CURL
 * @param ptr pointer to buffer containing the data
 * @param size size of the buffer
 * @param nmemb number of bytes
 * @param userdata CurlCallbackContext pointer
 * @retval size consumed or 0 if interrupted
 */
static size_t write_callback(char *ptr, size_t size, size_t nmemb, void *userdata)
{
	size_t ret = 0;
	CurlCallbackContext *context = static_cast<CurlCallbackContext *>(userdata);
	if(context)
	{
		ret = context->aamp->HandleSSLWriteCallback( ptr, size, nmemb, userdata);
	}
	return ret;
}

/**
 * @brief callback invoked on http header by curl
 * @param ptr pointer to buffer containing the data
 * @param size size of the buffer
 * @param nmemb number of bytes
 * @param user_data  CurlCallbackContext pointer
 * @retval
 */
static size_t header_callback(const char *ptr, size_t size, size_t nmemb, void *user_data)
{
	size_t ret = 0;
	CurlCallbackContext *context = static_cast<CurlCallbackContext *>(user_data);
	if(context)
	{
		ret = context->aamp->HandleSSLHeaderCallback(ptr, size, nmemb, user_data);
	}
	return ret;
}

#if LIBCURL_VERSION_NUM >= 0x072000 // CURL version >= 7.32.0
static int xferinfo_callback(
		void *clientp,
		curl_off_t dltotal,
		curl_off_t dlnow,
		curl_off_t ultotal,
		curl_off_t ulnow)
{
	int ret = 0;
	CurlProgressCbContext *context = (CurlProgressCbContext *)clientp;
	if(context)
	{
		ret = context->aamp->HandleSSLProgressCallback( clientp, dltotal, dlnow, ultotal, ulnow );
	}
	return ret;
}
#else
#warning CURL version < 7.32.0
/**
 * @brief
 * @param clientp app-specific as optionally set with CURLOPT_PROGRESSDATA
 * @param dltotal total bytes expected to download
 * @param dlnow downloaded bytes so far
 * @param ultotal total bytes expected to upload
 * @param ulnow uploaded bytes so far
 * @retval
 */
static int progress_callback(
	void *clientp, // app-specific as optionally set with CURLOPT_PROGRESSDATA
	double dltotal, // total bytes expected to download
	double dlnow, // downloaded bytes so far
	double ultotal, // total bytes expected to upload
	double ulnow // uploaded bytes so far
	)
{
	int ret = 0;
	CurlProgressCbContext *context = (CurlProgressCbContext *)clientp;
	if(context)
	{
		ret = context->aamp->HandleSSLProgressCallback ( clientp, dltotal, dlnow, ultotal, ulnow );
	}
	return ret;
}
#endif

/**
 * @brief
 * @param curl ptr to CURL instance
 * @param ssl_ctx SSL context used by CURL
 * @param user_ptr data pointer set as param to CURLOPT_SSL_CTX_DATA
 * @retval CURLcode CURLE_OK if no errors, otherwise corresponding CURL code
 */
CURLcode ssl_callback(CURL *curl, void *ssl_ctx, void *user_ptr)
{
	PrivateInstanceAAMP *context = (PrivateInstanceAAMP *)user_ptr;
	AAMPLOG_TRACE("priv aamp :%p", context);
	CURLcode rc = CURLE_OK;
	std::lock_guard<std::recursive_mutex> guard(context->mLock);
	if (!context->mDownloadsEnabled)
	{
		rc = CURLE_ABORTED_BY_CALLBACK ; // CURLE_ABORTED_BY_CALLBACK
	}
	return rc;
}

/**
 * @brief
 * @param handle ptr to CURL instance
 * @param type type of data passed in the callback
 * @param data data pointer, NOT null terminated
 * @param size size of the data
 * @param userp user pointer set with CURLOPT_DEBUGDATA
 * @retval return 0
 */
static int eas_curl_debug_callback(CURL *handle, curl_infotype type, char *data, size_t size, void *userp)
{
	(void)handle;
	(void)userp;
	if(type == CURLINFO_TEXT || type == CURLINFO_HEADER_IN)
	{
		//limit log spam to only TEXT and HEADER_IN
		size_t len = size;
		while( len>0 && data[len-1]<' ' ) len--;
		std::string printable(data,len);
		AAMPLOG_MIL("curl debug type:%d info:%s", type, printable.c_str() );
	}
	return 0;
}

/**
 * @fn CreateCurlStore
 * @brief CreateCurlStore - Create a new curl store for given host
 */
CurlSocketStoreStruct *CurlStore::CreateCurlStore ( const std::string &hostname )
{
	CurlSocketStoreStruct *CurlSock = new curlstorestruct(); // throws std::bad_alloc on failure; no NULL check needed
	CurlDataShareLock *locks = &CurlSock->mShareLock; // per-host embedded lock (VPAAMP-558)

	CurlSock->timestamp = aamp_GetCurrentTimeMS();
	CurlSock->mCurlStoreUserCount += 1;

	CurlSock->mCurlShared = curl_share_init();
	CURL_SHARE_SETOPT(CurlSock->mCurlShared, CURLSHOPT_USERDATA, (void*)locks);
	CURL_SHARE_SETOPT(CurlSock->mCurlShared, CURLSHOPT_LOCKFUNC, curl_lock_callback);
	CURL_SHARE_SETOPT(CurlSock->mCurlShared, CURLSHOPT_UNLOCKFUNC, curl_unlock_callback);
	CURL_SHARE_SETOPT(CurlSock->mCurlShared, CURLSHOPT_SHARE, CURL_LOCK_DATA_DNS);
	CURL_SHARE_SETOPT(CurlSock->mCurlShared, CURLSHOPT_SHARE, CURL_LOCK_DATA_SSL_SESSION);

	if ( umCurlSockDataStore.size() >= MaxCurlSockStore )
	{
		// Remove not recently used handle.
		RemoveCurlSock();
	}

	umCurlSockDataStore[hostname]=CurlSock;
	AAMPLOG_INFO("Curl store for %s created, Added shared ctx %p in curlstore %p, size:%zu maxsize:%d", hostname.c_str(),
					CurlSock->mCurlShared, CurlSock, umCurlSockDataStore.size(), MaxCurlSockStore);
	return CurlSock;
}

/**
 * @fn GetCurlHandle
 * @brief GetCurlHandle - Get a free curl easy handle for given url & curl index
 */
CURL* CurlStore::GetCurlHandle(PrivateInstanceAAMP *aamp,std::string url, AampCurlInstance startIdx )
{
	CURL * curl = NULL;
	assert (startIdx <= eCURLINSTANCE_MAX);

	std::string HostName;
	HostName = aamp_getHostFromURL(std::move(url));

	if (ISCONFIGSET(eAAMPConfig_EnableCurlStore) && !( aamp_IsLocalHost(HostName) ))
	{
		GetFromCurlStore ( HostName, startIdx, &curl );
	}
	else
	{
		curl = curl_easy_init();
	}

	return curl;
}

/**
 * @fn SaveCurlHandle
 * @brief SaveCurlHandle - Save a curl easy handle for given host & curl index
 */
void CurlStore::SaveCurlHandle (PrivateInstanceAAMP *aamp, std::string url, AampCurlInstance startIdx, CURL *curl )
{
	assert (startIdx <= eCURLINSTANCE_MAX);

	std::string HostName;
	HostName = aamp_getHostFromURL(std::move(url));

	if (ISCONFIGSET(eAAMPConfig_EnableCurlStore) && !( aamp_IsLocalHost(HostName) ))
	{
		KeepInCurlStore ( HostName, startIdx, curl );
	}
	else
	{
		curl_easy_cleanup(curl);
		// no need to set to nullptr here since passed by value and not subsequently used
	}
}

/**
 * @fn CurlEasyInitWithOpt
 * @brief CurlEasyInitWithOpt - Create a curl easy handle with set of aamp opts
 */
CURL* CurlStore::CurlEasyInitWithOpt ( PrivateInstanceAAMP *aamp, const std::string &proxyName, int instId )
{
	std::string UserAgentString;
	UserAgentString=aamp->mConfig->GetUserAgentString();
	uint32_t CurlConnectTimeout =  GETCONFIGVALUE(eAAMPConfig_Curl_ConnectTimeout);
	CURL *curlEasyhdl = curl_easy_init();
	if (ISCONFIGSET(eAAMPConfig_CurlLogging))
	{
		CURL_EASY_SETOPT_LONG(curlEasyhdl, CURLOPT_VERBOSE, 1 );
	}
	CURL_EASY_SETOPT_LONG(curlEasyhdl, CURLOPT_NOSIGNAL, 1 );
	
#if LIBCURL_VERSION_NUM >= 0x072000 // CURL version >= 7.32.0
	CURL_EASY_SETOPT_FUNC(curlEasyhdl, CURLOPT_XFERINFOFUNCTION, xferinfo_callback )
#else
#warning LIBCURL_VERSION<7.32.0
	CURL_EASY_SETOPT_FUNC(curlEasyhdl, CURLOPT_PROGRESSFUNCTION, progress_callback )
#endif
	CURL_EASY_SETOPT_FUNC(curlEasyhdl, CURLOPT_HEADERFUNCTION, header_callback);
	CURL_EASY_SETOPT_FUNC(curlEasyhdl, CURLOPT_WRITEFUNCTION, write_callback);
	CURL_EASY_SETOPT_LONG(curlEasyhdl, CURLOPT_TIMEOUT,DEFAULT_CURL_TIMEOUT);
	CURL_EASY_SETOPT_LONG(curlEasyhdl, CURLOPT_CONNECTTIMEOUT,CurlConnectTimeout );
	CURL_EASY_SETOPT_LONG(curlEasyhdl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_WHATEVER);
	CURL_EASY_SETOPT_LONG(curlEasyhdl, CURLOPT_FOLLOWLOCATION, 1 );
	CURL_EASY_SETOPT_LONG(curlEasyhdl, CURLOPT_NOPROGRESS, 0 ); // enable progress meter (off by default)

	CURL_EASY_SETOPT_STRING(curlEasyhdl, CURLOPT_USERAGENT, UserAgentString.c_str());
	CURL_EASY_SETOPT_STRING(curlEasyhdl, CURLOPT_ACCEPT_ENCODING, "");//Enable all the encoding formats supported by client
	CURL_EASY_SETOPT_FUNC(curlEasyhdl, CURLOPT_SSL_CTX_FUNCTION, ssl_callback); //Check for downloads disabled in btw ssl handshake
	CURL_EASY_SETOPT_POINTER(curlEasyhdl, CURLOPT_SSL_CTX_DATA, aamp);
	long dns_cache_timeout = GETCONFIGVALUE(eAAMPConfig_Dns_CacheTimeout);
	CURL_EASY_SETOPT_LONG(curlEasyhdl, CURLOPT_DNS_CACHE_TIMEOUT, dns_cache_timeout);
	CURL_EASY_SETOPT_POINTER(curlEasyhdl, CURLOPT_SHARE, aamp->mCurlShared);

	aamp->curlDLTimeout[instId] = DEFAULT_CURL_TIMEOUT * 1000;

	AAMPLOG_TRACE("CurlConnectTimeout : %d CurlTimeout : %ld curlDLTimeout : %ld instId : %d set for curlEasyhdl : %p",CurlConnectTimeout,DEFAULT_CURL_TIMEOUT,aamp->curlDLTimeout[instId],instId,curlEasyhdl);
	if (!proxyName.empty())
	{
		/* use this proxy */
		CURL_EASY_SETOPT_STRING(curlEasyhdl, CURLOPT_PROXY, proxyName.c_str());
		/* allow whatever auth the proxy speaks */
		CURL_EASY_SETOPT_LONG(curlEasyhdl, CURLOPT_PROXYAUTH, CURLAUTH_ANY);
	}

	if(aamp->IsEASContent())
	{
		//enable verbose logs so we can debug field issues
		CURL_EASY_SETOPT_LONG(curlEasyhdl, CURLOPT_VERBOSE, 1);
		CURL_EASY_SETOPT_FUNC(curlEasyhdl, CURLOPT_DEBUGFUNCTION, eas_curl_debug_callback);
		//set eas specific timeouts to handle faster cycling through bad hosts and faster total timeout
		CURL_EASY_SETOPT_LONG(curlEasyhdl, CURLOPT_TIMEOUT, EAS_CURL_TIMEOUT);
		CURL_EASY_SETOPT_LONG(curlEasyhdl, CURLOPT_CONNECTTIMEOUT, EAS_CURL_CONNECTTIMEOUT);

		aamp->curlDLTimeout[instId] = EAS_CURL_TIMEOUT * 1000;

		//on ipv6 box force curl to use ipv6 mode only
		struct stat tmpStat;
		bool isv6(::stat( "/tmp/estb_ipv6", &tmpStat) == 0);
		if(isv6)
		{
			CURL_EASY_SETOPT_LONG(curlEasyhdl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V6);
		}
		AAMPLOG_WARN("aamp eas curl config: timeout=%ld, connecttimeout%ld, ipv6=%d", EAS_CURL_TIMEOUT, EAS_CURL_CONNECTTIMEOUT, isv6);
	}
	//log_current_time("curl initialized");

	return curlEasyhdl;
}

/**
 * @fn CurlInit
 * @brief CurlInit - Initialize or get easy handles for given host & curl index from curl store
 */
void CurlStore::CurlInit(PrivateInstanceAAMP *aamp, AampCurlInstance startIdx, unsigned int instanceCount, std::string proxyName, const std::string &RemoteHost)
{
	int instanceEnd = startIdx + instanceCount;
	assert (instanceEnd <= eCURLINSTANCE_MAX);

	std::string HostName;
	bool IsRemotehost = true, CurlFdHost=false;
	AampCurlStoreErrorCode CurlStoreErrCode=eCURL_STORE_HOST_NOT_AVAILABLE;

	if(RemoteHost.size())
	{
		HostName = RemoteHost;
		CurlFdHost = true;
	}
	else
	{
		HostName = aamp->mOrigManifestUrl.hostname;
		IsRemotehost = aamp->mOrigManifestUrl.isRemotehost;
	}

	if ( IsRemotehost )
	{
		if (ISCONFIGSET(eAAMPConfig_EnableCurlStore))
		{
			AAMPLOG_INFO("Check curl store for host:%s inst:%d-%d Fds[%p:%p]", HostName.c_str(), startIdx, instanceEnd, aamp->curl[startIdx], aamp->curlhost[startIdx]->curl );
			CurlStoreErrCode = GetFromCurlStoreBulk(HostName, startIdx, instanceEnd, aamp, CurlFdHost );
			AAMPLOG_TRACE("From curl store for inst:%d-%d Fds[%p:%p] ShHdl:%p", startIdx, instanceEnd, aamp->curl[startIdx], aamp->curlhost[startIdx]->curl, aamp->mCurlShared );
		}
		else
		{
			if(NULL==aamp->mCurlShared)
			{
				aamp->mCurlShared = curl_share_init();
				CURL_SHARE_SETOPT(aamp->mCurlShared, CURLSHOPT_USERDATA, (void*)&gCurlShLock);
				CURL_SHARE_SETOPT(aamp->mCurlShared, CURLSHOPT_LOCKFUNC, curl_lock_callback);
				CURL_SHARE_SETOPT(aamp->mCurlShared, CURLSHOPT_UNLOCKFUNC, curl_unlock_callback);
				CURL_SHARE_SETOPT(aamp->mCurlShared, CURLSHOPT_SHARE, CURL_LOCK_DATA_DNS);

				if (ISCONFIGSET(eAAMPConfig_EnableSharedSSLSession))
				{
					CURL_SHARE_SETOPT(aamp->mCurlShared, CURLSHOPT_SHARE, CURL_LOCK_DATA_SSL_SESSION);
				}
			}
		}
	}
	else
	{
		// When the store is disabled, aamp->mCurlShared was allocated above and thus reset will cause a mem leak
		// When store is enabled, it is reusing from the store instance.
		if (ISCONFIGSET(eAAMPConfig_EnableCurlStore))
		{
			aamp->mCurlShared = NULL;
		}
	}

	if(eCURL_STORE_HOST_SOCK_AVAILABLE != CurlStoreErrCode)
	{
		CURL **CurlFd = NULL;
		for (unsigned int i = startIdx; i < instanceEnd; i++)
		{
			if(CurlFdHost)
			{
				CurlFd = &aamp->curlhost[i]->curl;
			}
			else
			{
				CurlFd = &aamp->curl[i];
			}

			if (NULL == *CurlFd )
			{
				*CurlFd = CurlEasyInitWithOpt(aamp, proxyName, i);
				AAMPLOG_INFO("Created new curl handle:%p for inst:%d", *CurlFd, i );
			}
		}
	}
}

/**
 * @fn CurlTerm
 * @brief CurlTerm - Terminate or store easy handles in curlstore
 */
void CurlStore::CurlTerm(PrivateInstanceAAMP *aamp, AampCurlInstance startIdx, unsigned int instanceCount, bool isFlushFds,const std::string &RemoteHost )
{
	int instanceEnd = startIdx + instanceCount;
	std::string HostName;
	bool IsRemotehost = true, CurlFdHost=false;

	if(RemoteHost.size())
	{
		HostName = RemoteHost;
		CurlFdHost = true;
	}
	else
	{
		HostName = aamp->mOrigManifestUrl.hostname;
		IsRemotehost = aamp->mOrigManifestUrl.isRemotehost;
	}
	
	if( ISCONFIGSET(eAAMPConfig_EnableCurlStore)  && ( IsRemotehost ))
	{
		AAMPLOG_INFO("Store unused curl handle:%p in Curlstore for inst:%d-%d", aamp->curl[startIdx], startIdx, instanceEnd );
		KeepInCurlStoreBulk ( HostName, startIdx, instanceEnd, aamp, CurlFdHost);
		if( true == isFlushFds )
		{
			FlushCurlSockForHost(HostName);
		}
		ShowCurlStoreData(ISCONFIGSET(eAAMPConfig_TraceLogging));
	}
	else
	{
		for (unsigned int i = startIdx; i < instanceEnd; i++)
		{
			if (aamp->curl[i])
			{
				curl_easy_cleanup(aamp->curl[i]);
				aamp->curl[i] = NULL;
				aamp->curlDLTimeout[i] = 0;
			}
		}
	}
}

/**
 * @fn CurlStore
 * @brief CurlStore constructor
 */
CurlStore::CurlStore( PrivateInstanceAAMP *aamp ):
	umCurlSockDataStore(),
	MaxCurlSockStore(MAX_CURL_SOCK_STORE)
{
	MaxCurlSockStore = GETCONFIGVALUE(eAAMPConfig_MaxCurlSockStore);
	AAMPLOG_INFO("Max sock store size:%d", MaxCurlSockStore);
}

/**
 * @fn ~curlStore
 * @brief CurlStore destructor
 */
CurlStore::~CurlStore()
{
	for( auto& it : umCurlSockDataStore )
	{
		CurlSocketStoreStruct *CurlSock {it.second};
		AAMPLOG_INFO("Removing host:%s lastused:%lld UserCount:%d Hits:%llu Misses:%llu",
		             (it.first).c_str(), CurlSock->timestamp, CurlSock->mCurlStoreUserCount,
		             CurlSock->mCacheHits, CurlSock->mCacheMisses);

		for (auto &[slotId, slotDeque] : CurlSock->mFreeSlots)
		{
			for (auto &[hdl, ts] : slotDeque)
			{
				if (hdl) curl_easy_cleanup(hdl);
				// no field reset needed: CurlSock is deleted immediately after
			}
		}

		if(CurlSock->mCurlShared)
		{
			(void)curl_share_cleanup(CurlSock->mCurlShared);
			CurlSock->mCurlShared = nullptr;
		}

		SAFE_DELETE(CurlSock);
	}
}

/**
 * @fn GetCurlStoreInstance
 * @brief GetCurlStoreInstance - Get static curlstore singleton object
 */
CurlStore& CurlStore::GetCurlStoreInstance ( PrivateInstanceAAMP *aamp )
{
	static CurlStore instance(aamp);

	return instance;
}

/**
 * @fn GetCurlHandleFromSlot
 * @brief GetCurlHandleFromSlot - Get a curl easy handle from the per-curlId slot
 *
 * O(1) per-slot lookup.  Stale handles (inserted before eCURL_MAX_AGE_TIME ago)
 * are evicted from the front of the slot deque before attempting retrieval.
 * On success, CURLOPT_SHARE is re-applied so recycled handles always reference
 * the current (live) mCurlShared pointer.
 *
 * Replaces the old GetCurlHandleFromFreeQ which used a single mixed-curlId
 * deque and required an O(n) linear scan to find a handle for the requested
 * curlId, and left non-matching front elements stuck in place indefinitely.
 */
CURL *CurlStore::GetCurlHandleFromSlot ( CurlSocketStoreStruct *CurlSock, int curlId )
{
	auto slotIt = CurlSock->mFreeSlots.find(curlId);
	if (slotIt == CurlSock->mFreeSlots.end() || slotIt->second.empty())
	{
		return nullptr;
	}

	auto &slot = slotIt->second;
	long long maxAgeThreshold = CurlSock->timestamp - eCURL_MAX_AGE_TIME;

	// Evict stale entries from the front (oldest entries pushed first → oldest at front)
	while (!slot.empty() && slot.front().second < maxAgeThreshold)
	{
		AAMPLOG_TRACE("Evicting stale curl hdl:%p age:%lld", slot.front().first, slot.front().second);
		curl_easy_cleanup(slot.front().first);
		slot.pop_front();
	}

	if (slot.empty())
	{
		return nullptr;
	}

	CURL *hdl = slot.front().first;
	slot.pop_front();

	// Re-apply CURLOPT_SHARE: ensures the recycled handle always binds to the
	// current live mCurlShared pointer, guarding against any future
	// flush/recreate cycle that might change the pointer.
	CURL_EASY_SETOPT_POINTER(hdl, CURLOPT_SHARE, CurlSock->mCurlShared);

	AAMPLOG_TRACE("Recycled curl hdl:%p for slot:%d", hdl, curlId);
	return hdl;
}

/**
 * @fn GetFromCurlStoreBulk
 * @brief GetFromCurlStoreBulk - Get free curl easy handle in bulk for given host & curl indices
 */
AampCurlStoreErrorCode CurlStore::GetFromCurlStoreBulk ( const std::string &hostname, AampCurlInstance CurlIndex, int count, PrivateInstanceAAMP *aamp, bool CurlFdHost )
{
	AampCurlStoreErrorCode ret = eCURL_STORE_HOST_SOCK_AVAILABLE;

	const std::lock_guard<std::mutex> lock(mCurlInstLock);
	CurlSockDataIter it = umCurlSockDataStore.find(hostname);

	if (it != umCurlSockDataStore.end())
	{
		int CurlFdCount=0,loop=0;
		CURL **CurlFd=NULL;
		CurlSocketStoreStruct *CurlSock = it->second;
		CurlSock->mCurlStoreUserCount += 1;
		CurlSock->timestamp = aamp_GetCurrentTimeMS();
		aamp->mCurlShared = CurlSock->mCurlShared;

		for( loop = (int)CurlIndex; loop < count; ++loop )
		{
			if(CurlFdHost)
			{
				CurlFd=&aamp->curlhost[loop]->curl;
			}
			else
			{
				CurlFd=&aamp->curl[loop];
			}

			*CurlFd = GetCurlHandleFromSlot ( CurlSock, loop );
			if(NULL != *CurlFd)
			{
				CURL_EASY_SETOPT_POINTER(*CurlFd, CURLOPT_SSL_CTX_DATA, aamp);
				++CurlFdCount;
				++CurlSock->mCacheHits;
			}
			else
			{
				++CurlSock->mCacheMisses;
				ret = eCURL_STORE_SOCK_NOT_AVAILABLE;
			}
		}

		AAMPLOG_INFO ("%d fd(s) got from CurlStore User count:%d", CurlFdCount, CurlSock->mCurlStoreUserCount);

		if ( umCurlSockDataStore.size() > (size_t)MaxCurlSockStore )
		{
			// Remove not recently used handle.
			RemoveCurlSock();
		}
	}
	else
	{
		AAMPLOG_TRACE("Curl Inst %d for %s not in store", CurlIndex, hostname.c_str());
		ret = eCURL_STORE_HOST_NOT_AVAILABLE;

		CurlSocketStoreStruct *CurlSock = CreateCurlStore(hostname);

		if(NULL != CurlSock)
		{
			aamp->mCurlShared = CurlSock->mCurlShared;
		}
	}

	return ret;
}

/**
 * @fn GetFromCurlStore
 * @brief GetFromCurlStore - Get a free curl easy handle for given host & curl index
 */
AampCurlStoreErrorCode CurlStore::GetFromCurlStore ( const std::string &hostname, AampCurlInstance CurlIndex, CURL **curl )
{
	AampCurlStoreErrorCode ret = eCURL_STORE_HOST_SOCK_AVAILABLE;
	CurlSocketStoreStruct *CurlSock = NULL;
	*curl = NULL;

	const std::lock_guard<std::mutex> lock(mCurlInstLock);
	CurlSockDataIter it = umCurlSockDataStore.find(hostname);

	if (it != umCurlSockDataStore.end())
	{
		CurlSock = it->second;
		CurlSock->mCurlStoreUserCount += 1;
		CurlSock->timestamp = aamp_GetCurrentTimeMS();

		*curl = GetCurlHandleFromSlot ( CurlSock, (int)CurlIndex );
		if (NULL == *curl)
		{
			++CurlSock->mCacheMisses;
			ret = eCURL_STORE_SOCK_NOT_AVAILABLE;
			AAMPLOG_TRACE("Slot %d empty for %s", (int)CurlIndex, hostname.c_str());
		}
		else
		{
			++CurlSock->mCacheHits;
		}
	}

	if ( NULL == *curl )
	{
		AAMPLOG_TRACE("Curl Inst %d for %s not available", CurlIndex, hostname.c_str());

		if(NULL == CurlSock)
		{
			ret = eCURL_STORE_HOST_NOT_AVAILABLE;

			CurlSock = CreateCurlStore(hostname);
		}

		*curl = curl_easy_init();
		CURL_EASY_SETOPT_POINTER(*curl, CURLOPT_SHARE, CurlSock->mCurlShared);
	}

	return ret;
}

/**
 * @fn KeepInCurlStoreBulk
 * @brief KeepInCurlStoreBulk - Store curl easy handle in bulk for given host & curl index
 */
void CurlStore::KeepInCurlStoreBulk ( const std::string &hostname, AampCurlInstance CurlIndex, int count, PrivateInstanceAAMP *aamp, bool CurlFdHost )
{
	const std::lock_guard<std::mutex> lock(mCurlInstLock);
	CurlSockDataIter it = umCurlSockDataStore.find(hostname);

	if(it != umCurlSockDataStore.end())
	{
		CurlSocketStoreStruct *CurlSock = it->second;
		long long now = aamp_GetCurrentTimeMS();
		CurlSock->timestamp = now;
		CurlSock->mCurlStoreUserCount -= 1;
		if (CurlSock->mCurlStoreUserCount < 0)
		{
			AAMPLOG_WARN("mCurlStoreUserCount underflow for host %s; clamping to 0", hostname.c_str());
			CurlSock->mCurlStoreUserCount = 0;
		}

		for( int loop = (int)CurlIndex; loop < count; ++loop)
		{
			CURL *hdl = nullptr;
			if(CurlFdHost)
			{
				hdl = aamp->curlhost[loop]->curl;
				aamp->curlhost[loop]->curl = nullptr;
			}
			else
			{
				hdl = aamp->curl[loop];
				aamp->curl[loop] = nullptr;
			}

			if (!hdl) continue;

			if (CurlSock->mPendingFlush)
			{
				// This entry was flushed due to a network error while handles were
				// in flight.  Dispose the returned handle instead of re-pooling it
				// so that poisoned connections are never reused.
				AAMPLOG_TRACE("PendingFlush: disposing returned curl hdl:%p slot:%d host:%s",
				              hdl, loop, hostname.c_str());
				curl_easy_cleanup(hdl);
			}
			else
			{
				CurlSock->mFreeSlots[loop].push_back({hdl, now});
				AAMPLOG_TRACE("Curl Inst %d CurlCtx:%p stored in slot, total:%zu",
				              loop, hdl, CurlSock->mFreeSlots[loop].size());
			}
		}

		if (CurlSock->mPendingFlush && CurlSock->mCurlStoreUserCount <= 0)
		{
			// All in-flight users have returned — complete the deferred flush now
			AAMPLOG_WARN("PendingFlush: completing deferred cleanup for host %s", hostname.c_str());
			if (CurlSock->mCurlShared)
			{
				curl_share_cleanup(CurlSock->mCurlShared);
				CurlSock->mCurlShared = nullptr;
			}
			SAFE_DELETE(CurlSock);
			umCurlSockDataStore.erase(it);
			return;
		}

		AAMPLOG_TRACE("CurlStore User count:%d for:%s", CurlSock->mCurlStoreUserCount, hostname.c_str());
		if ( umCurlSockDataStore.size() > (size_t)MaxCurlSockStore )
		{
			// Remove not recently used handle.
			RemoveCurlSock();
		}
	}
	else
	{
		AAMPLOG_INFO("Host %s not in store, Curl Inst %d-%d", hostname.c_str(), CurlIndex, count);
	}
}

/**
 * @fn KeepInCurlStore
 * @brief KeepInCurlStore - Store a curl easy handle for given host & curl index
 */
void CurlStore::KeepInCurlStore ( const std::string &hostname, AampCurlInstance CurlIndex, CURL *curl )
{
	const std::lock_guard<std::mutex> lock(mCurlInstLock);
	CurlSockDataIter it = umCurlSockDataStore.find(hostname);
	if(it != umCurlSockDataStore.end())
	{
		CurlSocketStoreStruct *CurlSock = it->second;
		long long now = aamp_GetCurrentTimeMS();
		CurlSock->timestamp = now;
		CurlSock->mCurlStoreUserCount -= 1;
		if (CurlSock->mCurlStoreUserCount < 0)
		{
			AAMPLOG_WARN("mCurlStoreUserCount underflow for host %s; clamping to 0", hostname.c_str());
			CurlSock->mCurlStoreUserCount = 0;
		}

		if (CurlSock->mPendingFlush)
		{
			AAMPLOG_TRACE("PendingFlush: disposing returned curl hdl:%p slot:%d host:%s",
			              curl, (int)CurlIndex, hostname.c_str());
			curl_easy_cleanup(curl);

			if (CurlSock->mCurlStoreUserCount <= 0)
			{
				AAMPLOG_WARN("PendingFlush: completing deferred cleanup for host %s", hostname.c_str());
				if (CurlSock->mCurlShared)
				{
					curl_share_cleanup(CurlSock->mCurlShared);
					CurlSock->mCurlShared = nullptr;
				}
				SAFE_DELETE(CurlSock);
				umCurlSockDataStore.erase(it);
			}
			return;
		}

		CurlSock->mFreeSlots[(int)CurlIndex].push_back({curl, now});
		AAMPLOG_TRACE("Curl Inst %d for %s CurlCtx:%p stored in slot, slots total:%zu, User:%d",
		              CurlIndex, hostname.c_str(), curl,
		              CurlSock->mFreeSlots[(int)CurlIndex].size(), CurlSock->mCurlStoreUserCount);
	}
	else
	{
		AAMPLOG_INFO("Host %s not in store, Curlfd:%p", hostname.c_str(), curl);
	}
}

/**
 * @fn RemoveCurlSock
 * @brief RemoveCurlSock - Remove not recently used entry from curl store
 */
void CurlStore::RemoveCurlSock ( void )
{
	unsigned long long time=aamp_GetCurrentTimeMS() + 1;

	CurlSockDataIter it=umCurlSockDataStore.begin();
	CurlSockDataIter RemIt=umCurlSockDataStore.end();

	for(; it != umCurlSockDataStore.end(); ++it )
	{
		CurlSocketStoreStruct *CurlSock = it->second;
		if( !CurlSock->mCurlStoreUserCount && ( time > CurlSock->timestamp ) )
		{
			time = CurlSock->timestamp;
			RemIt = it;
		}
	}

	if( umCurlSockDataStore.end() != RemIt )
	{
		CurlSocketStoreStruct *RmCurlSock = RemIt->second;
		AAMPLOG_INFO("Removing host:%s lastused:%lld UserCount:%d Hits:%llu Misses:%llu",
		             (RemIt->first).c_str(), RmCurlSock->timestamp, RmCurlSock->mCurlStoreUserCount,
		             RmCurlSock->mCacheHits, RmCurlSock->mCacheMisses);

		for (auto &[slotId, slotDeque] : RmCurlSock->mFreeSlots)
		{
			for (auto &[hdl, ts] : slotDeque)
			{
				if (hdl) curl_easy_cleanup(hdl);
			}
		}
		RmCurlSock->mFreeSlots.clear();

		if(RmCurlSock->mCurlShared)
		{
			curl_share_cleanup(RmCurlSock->mCurlShared);
			RmCurlSock->mCurlShared = nullptr;
		}

		SAFE_DELETE(RmCurlSock);
		umCurlSockDataStore.erase(RemIt);

		ShowCurlStoreData();
	}
	else
	{
		/**
		 * Lets extend the size of curlstore, since all entries are busy..
		 * Later it will get shrunk to user configured size.
		 */
	}
}

/**
 * @fn FlushCurlSockForHost
 * @brief FlushCurlSockForHost - remove entry of host upon certain network error
 *
 * Clears all pooled (idle) handles for the host immediately.  If there are
 * active users (mCurlStoreUserCount > 0), the CURLSH cannot be cleaned up yet
 * because in-flight easy handles still reference it via CURLOPT_SHARE.
 * In that case mPendingFlush is set so that handles returned by in-flight
 * downloads are disposed (not re-pooled) and cleanup is completed automatically
 * when the last user calls KeepInCurlStore*.
 */
void CurlStore::FlushCurlSockForHost(const std::string &hostname)
{
	const std::lock_guard<std::mutex> lock(mCurlInstLock);
	CurlSockDataIter removeIter = umCurlSockDataStore.find(hostname);
	if( umCurlSockDataStore.end() != removeIter )
	{
		CurlSocketStoreStruct *RmCurlSock = removeIter->second;
		AAMPLOG_WARN("Flushing host:%s UserCount:%d Hits:%llu Misses:%llu",
		             hostname.c_str(), RmCurlSock->mCurlStoreUserCount,
		             RmCurlSock->mCacheHits, RmCurlSock->mCacheMisses);

		// Dispose all idle (pooled) handles
		for (auto &[slotId, slotDeque] : RmCurlSock->mFreeSlots)
		{
			for (auto &[hdl, ts] : slotDeque)
			{
				if (hdl)
				{
					AAMPLOG_INFO("Flushing host:%s curlSlot:%d hdl:%p", hostname.c_str(), slotId, hdl);
					curl_easy_cleanup(hdl);
				}
			}
		}
		RmCurlSock->mFreeSlots.clear();

		if(RmCurlSock->mCurlStoreUserCount <= 0)
		{
			// No live users — safe to fully clean up
			if(RmCurlSock->mCurlShared)
			{
				AAMPLOG_INFO("Cleaning up curl shared context %p for host %s",
				             RmCurlSock->mCurlShared, hostname.c_str());
				curl_share_cleanup(RmCurlSock->mCurlShared);
				RmCurlSock->mCurlShared = nullptr;
			}
			else
			{
				AAMPLOG_WARN("No curl shared context for host %s", hostname.c_str());
			}
			SAFE_DELETE(RmCurlSock);
			umCurlSockDataStore.erase(removeIter);
		}
		else
		{
			// In-flight handles reference mCurlShared — defer cleanup.
			// KeepInCurlStore* will dispose returned handles and finish cleanup
			// when mCurlStoreUserCount reaches 0.
			RmCurlSock->mPendingFlush = true;
			AAMPLOG_WARN("PendingFlush set for host %s (%d live users); "
			             "deferred cleanup when they return handles",
			             hostname.c_str(), RmCurlSock->mCurlStoreUserCount);
		}
	}
	else
	{
		AAMPLOG_WARN("Either hostname %s is removed already or not present", hostname.c_str());
	}
}

/**
 * @fn ShowCurlStoreData
 * @brief ShowCurlStoreData - Print curl store details including hit/miss telemetry
 */
void CurlStore::ShowCurlStoreData ( bool trace )
{
	if(trace)
	{
		AAMPLOG_INFO("Curl Store Size:%zu, MaxSize:%d", umCurlSockDataStore.size(), MaxCurlSockStore);

		CurlSockDataIter it=umCurlSockDataStore.begin();
		for(int loop=1; it != umCurlSockDataStore.end(); ++it,++loop )
		{
			CurlSocketStoreStruct *CurlSock = it->second;
			uint64_t total = CurlSock->mCacheHits + CurlSock->mCacheMisses;
			double hitRate = total ? (100.0 * CurlSock->mCacheHits / total) : 0.0;
			AAMPLOG_INFO("%d.Host:%s ShHdl:%p LastUsed:%lld UserCount:%d PendingFlush:%s",
			             loop, (it->first).c_str(), CurlSock->mCurlShared,
			             CurlSock->timestamp, CurlSock->mCurlStoreUserCount,
			             CurlSock->mPendingFlush ? "yes" : "no");
			AAMPLOG_INFO("%d.CacheHits:%llu Misses:%llu HitRate:%.1f%%",
			             loop, CurlSock->mCacheHits, CurlSock->mCacheMisses, hitRate);

			// Log per-slot handle counts
			for (const auto &[slotId, slotDeque] : CurlSock->mFreeSlots)
			{
				AAMPLOG_INFO("  Slot:%d PooledHandles:%zu", slotId, slotDeque.size());
				for (const auto &[hdl, ts] : slotDeque)
				{
					AAMPLOG_INFO("    CurlFd:%p InsertedAt:%lld", hdl, ts);
				}
			}
		}
	}
}

int GetCurlResponseCode( CURL *curlhandle )
{
	return (int)aamp_CurlEasyGetinfoLong( curlhandle, CURLINFO_RESPONSE_CODE );
}

