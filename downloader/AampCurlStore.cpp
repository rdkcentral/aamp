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
static std::mutex gCurlShMutex;


/*
 * IMPORTANT: Static curl share lock is required.
 *
 * libcurl stores the pointer passed via CURLSHOPT_USERDATA internally and may
 * invoke curl_lock_callback / curl_unlock_callback asynchronously and from
 * different threads (e.g., during curl_easy_cleanup, curl_share_cleanup, or
 * other shared-state operations triggered by unrelated easy handles).
 *
 * libcurl has no knowledge of the caller’s object lifetime and does not
 * revalidate the USERDATA pointer before invoking callbacks. If the lock
 * object is dynamically allocated and freed by the caller, libcurl may later
 * dereference a dangling pointer, leading to use-after-free, deadlocks, or
 * process aborts.
 *
 * libcurl does not provide internal synchronization for data shared via
 * CURLSH handles; the application is responsible for providing thread-safe
 * locking for the entire duration libcurl may invoke these callbacks.
 *
 * NOTE: This shared lock is used only to protect libcurl shared state such as
 * DNS cache, SSL session cache, and connection bookkeeping. It does NOT
 * serialize actual media data transfers (audio/video/manifest downloads),
 * which continue to run in parallel.
 *
 * To guarantee correctness and thread safety, the lock object must have
 * process lifetime. Therefore, a single static CurlDataShareLock is used
 * here and must never be deleted.
 */
CurlDataShareLock CurlStore::gSharedCurlLock;

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
	AAMPLOG_WARN("[curl_debug ]curl :%p locks :%p locks->mCurlSharedlock :%p locks->mDnsCurlShareMutex :%p locks->mSslCurlShareMutex :%p", curl, locks, locks ? &locks->mCurlSharedlock : NULL, locks ? &locks->mDnsCurlShareMutex : NULL, locks ? &locks->mSslCurlShareMutex : NULL);
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
		gCurlShMutex.lock();
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
	AAMPLOG_WARN("[curl_debug ]curl :%p locks :%p locks->mCurlSharedlock :%p locks->mDnsCurlShareMutex :%p locks->mSslCurlShareMutex :%p", curl, locks, locks ? &locks->mCurlSharedlock : NULL, locks ? &locks->mDnsCurlShareMutex : NULL, locks ? &locks->mSslCurlShareMutex : NULL);
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
		gCurlShMutex.unlock();
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
	CurlCallbackContext *context = (CurlCallbackContext *)userdata;
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
		switch (type)
		{
		case CURLINFO_TEXT:
			AAMPLOG_WARN("curl: %s", printable.c_str() );
			break;
		case CURLINFO_HEADER_IN:
			AAMPLOG_WARN("curl header: %s", printable.c_str() );
			break;
		default:
			break; //CID:94999 - Resolve deadcode
		}
	}
	return 0;
}

/**
 * @fn CreateCurlStore
 * @brief CreateCurlStore - Create a new curl store for given host
 */
CurlSocketStoreStruct *CurlStore::CreateCurlStore ( const std::string &hostname )
{
	CurlSocketStoreStruct *CurlSock = new curlstorestruct();
	CurlDataShareLock *locks = &CurlStore::gSharedCurlLock;
	if ( NULL == CurlSock )
	{
		AAMPLOG_WARN("Failed to alloc memory for curl store");
		return NULL;
	}
	AAMPLOG_WARN("[curl_debug ]CreateCurlStore for host:%s CurlSock:%p locks:%p CurlSock->mCurlStoreUserCount:%d", hostname.c_str(), CurlSock, locks,CurlSock->mCurlStoreUserCount);
	CurlSock->timestamp = aamp_GetCurrentTimeMS();
	CurlSock->pstShareLocks = locks;
	CurlSock->mCurlStoreUserCount += 1;

	CurlSock->mCurlShared = curl_share_init();
	CURL_SHARE_SETOPT(CurlSock->mCurlShared, CURLSHOPT_USERDATA, (void*)locks);
	CURL_SHARE_SETOPT(CurlSock->mCurlShared, CURLSHOPT_LOCKFUNC, curl_lock_callback);
	CURL_SHARE_SETOPT(CurlSock->mCurlShared, CURLSHOPT_UNLOCKFUNC, curl_unlock_callback);
	CURL_SHARE_SETOPT(CurlSock->mCurlShared, CURLSHOPT_SHARE, CURL_LOCK_DATA_DNS);
	CURL_SHARE_SETOPT(CurlSock->mCurlShared, CURLSHOPT_SHARE, CURL_LOCK_DATA_SSL_SESSION);
	AAMPLOG_WARN("[curl_debug ]CurlSock->mCurlShared:%p for host:%s", CurlSock->mCurlShared, hostname.c_str());

	if ( umCurlSockDataStore.size() >= MaxCurlSockStore )
	{
		AAMPLOG_WARN("[curl_debug ]umCurlSockDataStore.size(): %zu, MaxCurlSockStore: %d", umCurlSockDataStore.size(), MaxCurlSockStore);
		// Remove not recently used handle.
		RemoveCurlSock();
	}

	umCurlSockDataStore[hostname]=CurlSock;
	AAMPLOG_WARN("[curl_debug ]Curl store for %s created, Added shared ctx %p in curlstore %p, size:%zu maxsize:%d", hostname.c_str(),
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
	HostName = aamp_getHostFromURL ( url );
	AAMPLOG_WARN("[curl_debug ]aamp: %p, startIdx: %d, url: %s, HostName: %s", aamp, startIdx, url.c_str(), HostName.c_str());

	if (ISCONFIGSET(eAAMPConfig_EnableCurlStore) && !( aamp_IsLocalHost(HostName) ))
	{
		GetFromCurlStore ( HostName, startIdx, &curl );
		AAMPLOG_WARN("[curl_debug ]GetCurlHandle from store for host:%s inst:%d curl:%p", HostName.c_str(), startIdx, curl);
	}
	else
	{
		curl = curl_easy_init();
		AAMPLOG_WARN("[curl_debug ]GetCurlHandle created new curl for host:%s inst:%d curl:%p", HostName.c_str(), startIdx, curl);
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
	HostName = aamp_getHostFromURL ( url );
	AAMPLOG_WARN("[curl_debug ]aamp: %p, startIdx: %d, url: %s, HostName: %s", aamp, startIdx, url.c_str(), HostName.c_str());

	if (ISCONFIGSET(eAAMPConfig_EnableCurlStore) && !( aamp_IsLocalHost(HostName) ))
	{
		KeepInCurlStore ( HostName, startIdx, curl );
		AAMPLOG_WARN("[curl_debug ]SaveCurlHandle to store for host:%s inst:%d curl:%p", HostName.c_str(), startIdx, curl);
	}
	else
	{
		AAMPLOG_WARN("[curl_debug ]	curl_easy_cleanup(curl) for host:%s inst:%d curl:%p", HostName.c_str(), startIdx, curl);
		curl_easy_cleanup(curl);
		curl = nullptr;
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
	long dns_cache_timeout = 3*60;
	CURL_EASY_SETOPT_LONG(curlEasyhdl, CURLOPT_DNS_CACHE_TIMEOUT, dns_cache_timeout);
	CURL_EASY_SETOPT_POINTER(curlEasyhdl, CURLOPT_SHARE, aamp->mCurlShared);

	aamp->curlDLTimeout[instId] = DEFAULT_CURL_TIMEOUT * 1000;

	AAMPLOG_WARN("[curl_debug ]CurlConnectTimeout : %d CurlTimeout : %ld curlDLTimeout : %ld instId : %d set for curlEasyhdl : %p",CurlConnectTimeout,DEFAULT_CURL_TIMEOUT,aamp->curlDLTimeout[instId],instId,curlEasyhdl);
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
		AAMPLOG_WARN("[curl_debug ]aamp eas curl config: timeout=%ld, connecttimeout%ld, ipv6=%d", EAS_CURL_TIMEOUT, EAS_CURL_CONNECTTIMEOUT, isv6);
	}
	//log_current_time("curl initialized");
	AAMPLOG_WARN("[curl_debug ]aamp: %p, instId: %d, proxyName: %s, curlEasyhdl: %p", aamp, instId, proxyName.c_str(), curlEasyhdl);
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
	AAMPLOG_WARN("[curl_debug ]aamp: %p, startIdx: %d, instanceCount: %d, proxyName: %s, RemoteHost: %s", aamp, startIdx, instanceCount, proxyName.c_str(), RemoteHost.c_str());
	if(RemoteHost.size())
	{
		HostName = RemoteHost;
		CurlFdHost = true;
		AAMPLOG_WARN("[curl_debug ]RemoteHost: %s, HostName: %s, CurlFdHost: %d", RemoteHost.c_str(), HostName.c_str(), CurlFdHost);
	}
	else
	{
		HostName = aamp->mOrigManifestUrl.hostname;
		IsRemotehost = aamp->mOrigManifestUrl.isRemotehost;
		AAMPLOG_WARN("[curl_debug ]HostName: %s, IsRemotehost: %d", HostName.c_str(), IsRemotehost);
	}

	if ( IsRemotehost )
	{
		if (ISCONFIGSET(eAAMPConfig_EnableCurlStore))
		{
			AAMPLOG_WARN("[curl_debug ]Check curl store for host:%s inst:%d-%d Fds[%p:%p]", HostName.c_str(), startIdx, instanceEnd, aamp->curl[startIdx], aamp->curlhost[startIdx]->curl );
			CurlStoreErrCode = GetFromCurlStoreBulk(HostName, startIdx, instanceEnd, aamp, CurlFdHost );
			AAMPLOG_WARN("[curl_debug ]From curl store for inst:%d-%d Fds[%p:%p] ShHdl:%p", startIdx, instanceEnd, aamp->curl[startIdx], aamp->curlhost[startIdx]->curl, aamp->mCurlShared );
		}
		else
		{
			if(NULL==aamp->mCurlShared)
			{
				aamp->mCurlShared = curl_share_init();
				CURL_SHARE_SETOPT(aamp->mCurlShared, CURLSHOPT_USERDATA, (void*)NULL);
				CURL_SHARE_SETOPT(aamp->mCurlShared, CURLSHOPT_LOCKFUNC, curl_lock_callback);
				CURL_SHARE_SETOPT(aamp->mCurlShared, CURLSHOPT_UNLOCKFUNC, curl_unlock_callback);
				CURL_SHARE_SETOPT(aamp->mCurlShared, CURLSHOPT_SHARE, CURL_LOCK_DATA_DNS);

				if (ISCONFIGSET(eAAMPConfig_EnableSharedSSLSession))
				{
					CURL_SHARE_SETOPT(aamp->mCurlShared, CURLSHOPT_SHARE, CURL_LOCK_DATA_SSL_SESSION);
				}
				AAMPLOG_WARN("[curl_debug ]Created shared ctx %p for host:%s in CurlInit", aamp->mCurlShared, HostName.c_str());
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
			AAMPLOG_WARN("[curl_debug ]Reset shared ctx to NULL for host:%s in CurlInit when store enabled", HostName.c_str());
		}
	}

	if(eCURL_STORE_HOST_SOCK_AVAILABLE != CurlStoreErrCode)
	{
		CURL **CurlFd = NULL;
		AAMPLOG_WARN("[curl_debug ]Initialize curl handle for host:%s inst:%d-%d CurlFdHost:%d", HostName.c_str(), startIdx, instanceEnd, CurlFdHost);
		for (unsigned int i = startIdx; i < instanceEnd; i++)
		{
			if(CurlFdHost)
			{
				CurlFd = &aamp->curlhost[i]->curl;
				AAMPLOG_WARN("[curl_debug ]CurlFdHost is true for host:%s inst:%d CurlFd:%p", HostName.c_str(), i, CurlFd ? *CurlFd : NULL);
			}
			else
			{
				CurlFd = &aamp->curl[i];
				AAMPLOG_WARN("[curl_debug ]CurlFdHost is false for host:%s inst:%d CurlFd:%p", HostName.c_str(), i, CurlFd ? *CurlFd : NULL);
			}

			if (NULL == *CurlFd )
			{
				*CurlFd = CurlEasyInitWithOpt(aamp, proxyName, i);
				AAMPLOG_WARN("[curl_debug ]Created new curl handle:%p for inst:%d", *CurlFd, i );
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
	AAMPLOG_WARN("[curl_debug ]aamp: %p, startIdx: %d, instanceCount: %d, isFlushFds: %d, RemoteHost: %s", aamp, startIdx, instanceCount, isFlushFds, RemoteHost.c_str());
	if(RemoteHost.size())
	{
		HostName = RemoteHost;
		CurlFdHost = true;
		AAMPLOG_WARN("[curl_debug ]RemoteHost: %s, HostName: %s, CurlFdHost: %d", RemoteHost.c_str(), HostName.c_str(), CurlFdHost);
	}
	else
	{
		HostName = aamp->mOrigManifestUrl.hostname;
		IsRemotehost = aamp->mOrigManifestUrl.isRemotehost;
		AAMPLOG_WARN("[curl_debug ]HostName: %s, IsRemotehost: %d", HostName.c_str(), IsRemotehost);
	}
	
	if( ISCONFIGSET(eAAMPConfig_EnableCurlStore)  && ( IsRemotehost ))
	{
		AAMPLOG_WARN("[curl_debug ]Store unused curl handle:%p in Curlstore for inst:%d-%d Fds[%p:%p] ShHdl:%p", aamp->curl[startIdx], startIdx, instanceEnd, aamp->curl[startIdx], aamp->curlhost[startIdx]->curl, aamp->mCurlShared );
		KeepInCurlStoreBulk ( HostName, startIdx, instanceEnd, aamp, CurlFdHost);
		if( true == isFlushFds )
		{
			AAMPLOG_WARN("[curl_debug ]Flush curl fds for host:%s in CurlTerm", HostName.c_str());
			FlushCurlSockForHost(HostName);
		}
		//ShowCurlStoreData(ISCONFIGSET(eAAMPConfig_TraceLogging));
		ShowCurlStoreData(true);
	}
	else
	{
		AAMPLOG_WARN("[curl_debug ]Cleanup curl handle:%p for host:%s inst:%d-%d", aamp->curl[startIdx], HostName.c_str(), startIdx, instanceEnd);
		for (unsigned int i = startIdx; i < instanceEnd; i++)
		{
			if (aamp->curl[i])
			{
				AAMPLOG_WARN("[curl_debug ]curl_easy_cleanup for curl handle:%p for host:%s inst:%d", aamp->curl[i], HostName.c_str(), i);
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
	AAMPLOG_WARN("[curl_debug ]CurlStore constructor called with aamp: %p, MaxCurlSockStore: %d", aamp, MaxCurlSockStore);
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

		
		AAMPLOG_WARN("[curl_debug ]Removing host:%s lastused:%lld UserCount:%d mCurlShared:%p pstShareLocks:%p",
				(it.first).c_str(), CurlSock->timestamp, CurlSock->mCurlStoreUserCount,
				CurlSock->mCurlShared, CurlSock->pstShareLocks);
		AAMPLOG_WARN("[curl_debug ]pstShareLocks->mCurlSharedlock:%p pstShareLocks->mDnsCurlShareMutex:%p pstShareLocks->mSslCurlShareMutex:%p",
				CurlSock->pstShareLocks ? &CurlSock->pstShareLocks->mCurlSharedlock : nullptr,
				CurlSock->pstShareLocks ? &CurlSock->pstShareLocks->mDnsCurlShareMutex : nullptr,
				CurlSock->pstShareLocks ? &CurlSock->pstShareLocks->mSslCurlShareMutex : nullptr);
		for( auto& itFreeQ : CurlSock->mFreeQ )
		{
			if(itFreeQ.curl)
			{
				AAMPLOG_WARN("[curl_debug ]curl_easy_cleanup for curl handle:%p for host:%s curlId:%d eHdlTimestamp:%lld", itFreeQ.curl, (it.first).c_str(), itFreeQ.curlId, itFreeQ.eHdlTimestamp);
				curl_easy_cleanup(itFreeQ.curl);
				itFreeQ.curl = nullptr;
				itFreeQ.eHdlTimestamp = 0;
			}
		}

		if(CurlSock->mCurlShared)
		{
			AAMPLOG_WARN("[curl_debug ]curl_share_cleanup for shared ctx %p for host:%s in CurlStore destructor", CurlSock->mCurlShared, (it.first).c_str());
			(void)curl_share_cleanup(CurlSock->mCurlShared);
			CurlSock->mCurlShared = nullptr;
			CurlSock->pstShareLocks = nullptr;
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
	AAMPLOG_WARN("[curl_debug ]curl_store instance : %p", &instance);
	return instance;
}

/**
 * @fn GetCurlHandleFromFreeQ
 * @brief GetCurlHandleFromFreeQ - Get curl handle from free queue
 */
CURL *CurlStore::GetCurlHandleFromFreeQ ( CurlSocketStoreStruct *CurlSock, int instId )
{
	CURL *curlhdl = NULL;
	long long MaxAge = CurlSock->timestamp-eCURL_MAX_AGE_TIME;

	AAMPLOG_WARN("[curl_debug ]GetCurlHandleFromFreeQ for instId:%d MaxAge:%lld FreeQSize:%zu", instId, MaxAge, CurlSock->mFreeQ.size());
	AAMPLOG_WARN("[curl_debug ]GetCurlHandleFromFreeQ for instId:%d MaxAge:%lld FreeQFrontCurlId:%d FreeQFrontEhdlTimestamp:%lld",instId, MaxAge, CurlSock->mFreeQ.empty() ? -1 : CurlSock->mFreeQ.front().curlId, CurlSock->mFreeQ.empty() ? -1 : CurlSock->mFreeQ.front().eHdlTimestamp);
	AAMPLOG_WARN("[curl_debug ]mCurlShared:%p pstShareLocks:%p pstShareLocks->mCurlSharedlock:%p pstShareLocks->mDnsCurlShareMutex:%p pstShareLocks->mSslCurlShareMutex:%p", CurlSock->mCurlShared, CurlSock->pstShareLocks, CurlSock->pstShareLocks ? &CurlSock->pstShareLocks->mCurlSharedlock : nullptr, CurlSock->pstShareLocks ? &CurlSock->pstShareLocks->mDnsCurlShareMutex : nullptr, CurlSock->pstShareLocks ? &CurlSock->pstShareLocks->mSslCurlShareMutex : nullptr);
	for (int i = instId; i < instId+1 && !CurlSock->mFreeQ.empty(); )
	{
		CurlHandleStruct mObj = CurlSock->mFreeQ.front();
	
		AAMPLOG_WARN("[curl_debug ]Check curl handle:%p for curlId:%d eHdlTimestamp:%lld MaxAge:%lld instId:%d", mObj.curl, mObj.curlId, mObj.eHdlTimestamp, MaxAge, instId);
		AAMPLOG_WARN("[curl_debug ]Check curl handle:%p for curlId:%d eHdlTimestamp:%lld MaxAge:%lld instId:%d mCurlShared:%p pstShareLocks:%p pstShareLocks->mCurlSharedlock:%p pstShareLocks->mDnsCurlShareMutex:%p pstShareLocks->mSslCurlShareMutex:%p",
				mObj.curl, mObj.curlId, mObj.eHdlTimestamp, MaxAge, instId,
				CurlSock->mCurlShared, CurlSock->pstShareLocks, CurlSock->pstShareLocks ? &CurlSock->pstShareLocks->mCurlSharedlock : nullptr, CurlSock->pstShareLocks ? &CurlSock->pstShareLocks->mDnsCurlShareMutex : nullptr, CurlSock->pstShareLocks ? &CurlSock->pstShareLocks->mSslCurlShareMutex : nullptr);
		if( MaxAge > mObj.eHdlTimestamp )
		{
			CurlSock->mFreeQ.pop_front();
			AAMPLOG_WARN("[curl_debug ]Remove old curl hdl:%p curlId:%d eHdlTimestamp:%lld", mObj.curl, mObj.curlId, mObj.eHdlTimestamp);
			curl_easy_cleanup(mObj.curl);
			mObj.curl = NULL;
			continue;
		}

		if ( mObj.curlId == i )
		{
			CurlSock->mFreeQ.pop_front();
			curlhdl = mObj.curl;
			AAMPLOG_WARN("[curl_debug ]Get curl handle:%p for  curlId:%d eHdlTimestamp:%lld MaxAge:%lld instId:%d mCurlShared:%p pstShareLocks:%p pstShareLocks->mCurlSharedlock:%p pstShareLocks->mDnsCurlShareMutex:%p pstShareLocks->mSslCurlShareMutex:%p",
					curlhdl, mObj.curlId, mObj.eHdlTimestamp, MaxAge, instId,
					CurlSock->mCurlShared, CurlSock->pstShareLocks, CurlSock->pstShareLocks ? &CurlSock->pstShareLocks->mCurlSharedlock : nullptr, CurlSock->pstShareLocks ? &CurlSock->pstShareLocks->mDnsCurlShareMutex : nullptr, CurlSock->pstShareLocks ? &CurlSock->pstShareLocks->mSslCurlShareMutex : nullptr);
			break;
		}

		for(auto it=CurlSock->mFreeQ.begin()+1; it!=CurlSock->mFreeQ.end(); ++it)
		{
			if (( MaxAge < it->eHdlTimestamp ) && ( it->curlId == i ))
			{
				curlhdl=it->curl;
				AAMPLOG_WARN("[curl_debug ]Get curl handle:%p for curlId:%d eHdlTimestamp:%lld MaxAge:%lld instId:%d mCurlShared:%p pstShareLocks:%p pstShareLocks->mCurlSharedlock:%p pstShareLocks->mDnsCurlShareMutex:%p pstShareLocks->mSslCurlShareMutex:%p",
						curlhdl, it->curlId, it->eHdlTimestamp, MaxAge, instId,
						CurlSock->mCurlShared, CurlSock->pstShareLocks, CurlSock->pstShareLocks ? &CurlSock->pstShareLocks->mCurlSharedlock : nullptr, CurlSock->pstShareLocks ? &CurlSock->pstShareLocks->mDnsCurlShareMutex : nullptr, CurlSock->pstShareLocks ? &CurlSock->pstShareLocks->mSslCurlShareMutex : nullptr);
				CurlSock->mFreeQ.erase(it);
				break;
			}
		}

		++i;
	}
	AAMPLOG_WARN("[curl_debug ]GetCurlHandleFromFreeQ returned curl handle:%p  instId:%d", curlhdl,instId);
	return curlhdl;
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

	AAMPLOG_WARN("[curl_debug ]GetFromCurlStoreBulk for host:%s CurlIndex:%d count:%d CurlFdHost:%d", hostname.c_str(), CurlIndex, count, CurlFdHost);
	if (it != umCurlSockDataStore.end())
	{
		
		AAMPLOG_WARN("[curl_debug ]Curl store found for host:%s CurlIndex:%d count:%d CurlFdHost:%d", hostname.c_str(), CurlIndex, count, CurlFdHost);
		int CurlFdCount=0,loop=0;
		CURL **CurlFd=NULL;
		CurlSocketStoreStruct *CurlSock = it->second;
		CurlSock->mCurlStoreUserCount += 1;
		CurlSock->timestamp = aamp_GetCurrentTimeMS();
		aamp->mCurlShared = CurlSock->mCurlShared;
		
		AAMPLOG_WARN("[curl_debug ]Curl store timestamp updated for host:%s CurlIndex:%d count:%d CurlFdHost:%d", hostname.c_str(), CurlIndex, count, CurlFdHost);
		AAMPLOG_WARN("[curl_debug ]mcurlShared:%p pstShareLocks:%p pstShareLocks->mCurlSharedlock:%p pstShareLocks->mDnsCurlShareMutex:%p pstShareLocks->mSslCurlShareMutex:%p", CurlSock->mCurlShared, CurlSock->pstShareLocks, CurlSock->pstShareLocks ? &CurlSock->pstShareLocks->mCurlSharedlock : nullptr, CurlSock->pstShareLocks ? &CurlSock->pstShareLocks->mDnsCurlShareMutex : nullptr, CurlSock->pstShareLocks ? &CurlSock->pstShareLocks->mSslCurlShareMutex : nullptr);

		for( loop = (int)CurlIndex; loop < count; )
		{
			if(CurlFdHost)
			{
				CurlFd=&aamp->curlhost[loop]->curl;
				AAMPLOG_WARN("[curl_debug ]CurlFdHost is true for host:%s inst:%d CurlFd:%p", hostname.c_str(), loop, CurlFd ? *CurlFd : NULL);
			}
			else
			{
				CurlFd=&aamp->curl[loop];
				AAMPLOG_WARN("[curl_debug ]CurlFdHost is false for host:%s inst:%d CurlFd:%p", hostname.c_str(), loop, CurlFd ? *CurlFd : NULL);
			}

			if (!CurlSock->mFreeQ.empty())
			{
				*CurlFd= GetCurlHandleFromFreeQ ( CurlSock, loop );
				if(NULL!=*CurlFd)
				{
					CURL_EASY_SETOPT_POINTER(*CurlFd, CURLOPT_SSL_CTX_DATA, aamp);
					++CurlFdCount;
					AAMPLOG_WARN("[curl_debug ]Curl handle:%p for host:%s inst:%d CurlFdCount:%d", *CurlFd, hostname.c_str(), loop, CurlFdCount);
				}
				else
				{
					ret = eCURL_STORE_SOCK_NOT_AVAILABLE;
					AAMPLOG_WARN("[curl_debug ]No curl handle available in store for host:%s inst:%d", hostname.c_str(), loop);
				}
				++loop;
			}
			else
			{
				AAMPLOG_WARN("[curl_debug ]Queue is empty for host:%s inst:%d", hostname.c_str(), loop);
				ret = eCURL_STORE_SOCK_NOT_AVAILABLE;
				break;
			}
		}

		AAMPLOG_WARN("[curl_debug ]%d fd(s) got from CurlStore User count:%d ret:%d for host:%s", CurlFdCount, CurlSock->mCurlStoreUserCount, ret, hostname.c_str());

		AAMPLOG_WARN("[curl_debug ]umCurlSockDataStore size:%zu MaxCurlSockStore:%d", umCurlSockDataStore.size(), MaxCurlSockStore);
		if ( umCurlSockDataStore.size() > MaxCurlSockStore )
		{
			AAMPLOG_WARN("[curl_debug ] remove not recently used handle as store size:%zu exceeded max size:%d", umCurlSockDataStore.size(), MaxCurlSockStore);
			// Remove not recently used handle.
			RemoveCurlSock();
		}
	}
	else
	{
		AAMPLOG_WARN("[curl_debug ]Curl Inst %d for %s not in store", CurlIndex, hostname.c_str());
		ret = eCURL_STORE_HOST_NOT_AVAILABLE;

		AAMPLOG_WARN("[curl_debug ]Creating Curl store for host:%s as it is not in store", hostname.c_str());
		CurlSocketStoreStruct *CurlSock = CreateCurlStore(hostname);

		if(NULL != CurlSock)
		{
			
			aamp->mCurlShared = CurlSock->mCurlShared;
			AAMPLOG_WARN("[curl_debug ]mcurlShared:%p pstShareLocks:%p pstShareLocks->mCurlSharedlock:%p pstShareLocks->mDnsCurlShareMutex:%p pstShareLocks->mSslCurlShareMutex:%p", CurlSock->mCurlShared, CurlSock->pstShareLocks, CurlSock->pstShareLocks ? &CurlSock->pstShareLocks->mCurlSharedlock : nullptr, CurlSock->pstShareLocks ? &CurlSock->pstShareLocks->mDnsCurlShareMutex : nullptr, CurlSock->pstShareLocks ? &CurlSock->pstShareLocks->mSslCurlShareMutex : nullptr);
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

	AAMPLOG_WARN("[curl_debug ]GetFromCurlStore for host:%s CurlIndex:%d", hostname.c_str(), CurlIndex);
	const std::lock_guard<std::mutex> lock(mCurlInstLock);
	CurlSockDataIter it = umCurlSockDataStore.find(hostname);

	if (it != umCurlSockDataStore.end())
	{
		AAMPLOG_WARN("[curl_debug ]Curl store found for host:%s CurlIndex:%d", hostname.c_str(), CurlIndex);
		AAMPLOG_WARN("[curl_debug ]mcurlShared:%p pstShareLocks:%p pstShareLocks->mCurlSharedlock:%p pstShareLocks->mDnsCurlShareMutex:%p pstShareLocks->mSslCurlShareMutex:%p", it->second->mCurlShared, it->second->pstShareLocks, it->second->pstShareLocks ? &it->second->pstShareLocks->mCurlSharedlock : nullptr, it->second->pstShareLocks ? &it->second->pstShareLocks->mDnsCurlShareMutex : nullptr, it->second->pstShareLocks ? &it->second->pstShareLocks->mSslCurlShareMutex : nullptr);
		CurlSock = it->second;
		CurlSock->mCurlStoreUserCount += 1;
		AAMPLOG_WARN("[curl_debug ]Curl store user count updated to %d for host:%s CurlIndex:%d", CurlSock->mCurlStoreUserCount, hostname.c_str(), CurlIndex);
		CurlSock->timestamp = aamp_GetCurrentTimeMS();

		for( int loop = (int)CurlIndex; loop < CurlIndex+1; )
		{
			if (!CurlSock->mFreeQ.empty())
			{
				*curl = GetCurlHandleFromFreeQ ( CurlSock, loop);
				AAMPLOG_WARN("[curl_debug ]GetFromCurlStore GetCurlHandleFromFreeQ returned curl handle:%p for host:%s inst:%d mCurlShared:%p pstShareLocks:%p pstShareLocks->mCurlSharedlock:%p pstShareLocks->mDnsCurlShareMutex:%p pstShareLocks->mSslCurlShareMutex:%p",
						*curl, hostname.c_str(), loop,
						CurlSock->mCurlShared, CurlSock->pstShareLocks, CurlSock->pstShareLocks ? &CurlSock->pstShareLocks->mCurlSharedlock : nullptr, CurlSock->pstShareLocks ? &CurlSock->pstShareLocks->mDnsCurlShareMutex : nullptr, CurlSock->pstShareLocks ? &CurlSock->pstShareLocks->mSslCurlShareMutex : nullptr);

				if(NULL==*curl)
				{
					AAMPLOG_WARN("[curl_debug ]GetFromCurlStore GetCurlHandleFromFreeQ returned NULL for host:%s inst:%d", hostname.c_str(), loop);
					ret = eCURL_STORE_SOCK_NOT_AVAILABLE;
				}
				++loop;
			}
			else
			{
				AAMPLOG_WARN("[curl_debug ]Queue is empty for host:%s inst:%d", hostname.c_str(), loop);
				ret = eCURL_STORE_SOCK_NOT_AVAILABLE;
				break;
			}
		}
	}

	if ( NULL == *curl )
	{
		AAMPLOG_WARN("[curl_debug ]Curl Inst %d for %s not available", CurlIndex, hostname.c_str());

		if(NULL == CurlSock)
		{
			ret = eCURL_STORE_HOST_NOT_AVAILABLE;
			AAMPLOG_WARN("[curl_debug ]Creating Curl store for host:%s as it is not in store", hostname.c_str());
			CurlSock = CreateCurlStore(hostname);
		}

		*curl = curl_easy_init();
		AAMPLOG_WARN("[curl_debug ]Created new curl handle:%p for host:%s inst:%d curlShared:%p pstShareLocks:%p pstShareLocks->mCurlSharedlock:%p pstShareLocks->mDnsCurlShareMutex:%p pstShareLocks->mSslCurlShareMutex:%p", *curl, hostname.c_str(), CurlIndex, CurlSock->mCurlShared, CurlSock->pstShareLocks, CurlSock->pstShareLocks ? &CurlSock->pstShareLocks->mCurlSharedlock : nullptr, CurlSock->pstShareLocks ? &CurlSock->pstShareLocks->mDnsCurlShareMutex : nullptr, CurlSock->pstShareLocks ? &CurlSock->pstShareLocks->mSslCurlShareMutex : nullptr);
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
	CurlSocketStoreStruct *CurlSock = NULL;

	AAMPLOG_WARN("[curl_debug ]KeepInCurlStoreBulk for host:%s CurlIndex:%d count:%d CurlFdHost:%d", hostname.c_str(), CurlIndex, count, CurlFdHost);
	const std::lock_guard<std::mutex> lock(mCurlInstLock);
	CurlSockDataIter it = umCurlSockDataStore.find(hostname);

	if(it != umCurlSockDataStore.end())
	{
		AAMPLOG_WARN("[curl_debug ]Curl store found for host:%s CurlIndex:%d count:%d CurlFdHost:%d", hostname.c_str(), CurlIndex, count, CurlFdHost);
		AAMPLOG_WARN("[curl_debug ]mcurlShared:%p pstShareLocks:%p pstShareLocks->mCurlSharedlock:%p pstShareLocks->mDnsCurlShareMutex:%p pstShareLocks->mSslCurlShareMutex:%p", it->second->mCurlShared, it->second->pstShareLocks, it->second->pstShareLocks ? &it->second->pstShareLocks->mCurlSharedlock : nullptr, it->second->pstShareLocks ? &it->second->pstShareLocks->mDnsCurlShareMutex : nullptr, it->second->pstShareLocks ? &it->second->pstShareLocks->mSslCurlShareMutex : nullptr);
		CurlSock = it->second;
		CurlSock->timestamp = aamp_GetCurrentTimeMS();
		CurlSock->mCurlStoreUserCount -= 1;

		for( int loop = (int)CurlIndex; loop < count; ++loop)
		{
			CurlHandleStruct mObj;
			if(CurlFdHost)
			{
				AAMPLOG_WARN("[curl_debug ]KeepInCurlStoreBulk CurlFdHost is true for host:%s inst:%d CurlFd:%p", hostname.c_str(), loop, aamp->curlhost[loop] ? aamp->curlhost[loop]->curl : NULL);
				mObj.curl = aamp->curlhost[loop]->curl;
				aamp->curlhost[loop]->curl = NULL;
			}
			else
			{
				AAMPLOG_WARN("[curl_debug ]KeepInCurlStoreBulk CurlFdHost is false for host:%s inst:%d CurlFd:%p", hostname.c_str(), loop, aamp->curl[loop]);
				mObj.curl = aamp->curl[loop];
				aamp->curl[loop] = NULL;
			}

			mObj.eHdlTimestamp = CurlSock->timestamp;
			mObj.curlId = loop;
			CurlSock->mFreeQ.push_back(mObj);
			AAMPLOG_WARN("[curl_debug ]Curl Inst %d for %s CurlCtx:%p stored at %zu, User:%d mCurlShared:%p pstShareLocks:%p pstShareLocks->mCurlSharedlock:%p pstShareLocks->mDnsCurlShareMutex:%p pstShareLocks->mSslCurlShareMutex:%p",
					loop, hostname.c_str(), mObj.curl, CurlSock->mFreeQ.size(), CurlSock->mCurlStoreUserCount,
					CurlSock->mCurlShared, CurlSock->pstShareLocks, CurlSock->pstShareLocks ? &CurlSock->pstShareLocks->mCurlSharedlock : nullptr, CurlSock->pstShareLocks ? &CurlSock->pstShareLocks->mDnsCurlShareMutex : nullptr, CurlSock->pstShareLocks ? &CurlSock->pstShareLocks->mSslCurlShareMutex : nullptr);
		}

		AAMPLOG_WARN("[curl_debug ] User count:%d for:%s", CurlSock->mCurlStoreUserCount, hostname.c_str());
		AAMPLOG_WARN("[curl_debug ]umCurlSockDataStore size:%zu MaxCurlSockStore:%d", umCurlSockDataStore.size(), MaxCurlSockStore);
		if ( umCurlSockDataStore.size() > MaxCurlSockStore )
		{
			// Remove not recently used handle.
			RemoveCurlSock();
		}
	}
	else
	{
		AAMPLOG_WARN("[curl_debug ]Host %s not in store, Curl Inst %d-%d", hostname.c_str(), CurlIndex,count);
	}
}

/**
 * @fn KeepInCurlStore
 * @brief KeepInCurlStore - Store a curl easy handle for given host & curl index
 */
void CurlStore::KeepInCurlStore ( const std::string &hostname, AampCurlInstance CurlIndex, CURL *curl )
{
	CurlSocketStoreStruct *CurlSock = NULL;
	AAMPLOG_WARN("[curl_debug ]KeepInCurlStore for host:%s CurlIndex:%d Curlfd:%p", hostname.c_str(), CurlIndex, curl);
	const std::lock_guard<std::mutex> lock(mCurlInstLock);
	CurlSockDataIter it = umCurlSockDataStore.find(hostname);
	if(it != umCurlSockDataStore.end())
	{
		CurlSock = it->second;
		CurlSock->timestamp = aamp_GetCurrentTimeMS();
		CurlSock->mCurlStoreUserCount -= 1;

		CurlHandleStruct mObj;
		mObj.curl = curl;
		mObj.eHdlTimestamp = CurlSock->timestamp;
		mObj.curlId = (int)CurlIndex;
		CurlSock->mFreeQ.push_back(mObj);
		AAMPLOG_WARN("[curl_debug ]Curl Inst %d for %s CurlCtx:%p stored at %zu, User:%d", CurlIndex, hostname.c_str(),
						curl,CurlSock->mFreeQ.size(), CurlSock->mCurlStoreUserCount);
		AAMPLOG_WARN("[curl_debug ]umCurlSockDataStore size:%zu MaxCurlSockStore:%d", umCurlSockDataStore.size(), MaxCurlSockStore);
		AAMPLOG_WARN("[curl_debug ] User count:%d for:%s", CurlSock->mCurlStoreUserCount, hostname.c_str());
		AAMPLOG_WARN("[curl_debug ]mcurlShared:%p pstShareLocks:%p pstShareLocks->mCurlSharedlock:%p pstShareLocks->mDnsCurlShareMutex:%p pstShareLocks->mSslCurlShareMutex:%p", CurlSock->mCurlShared, CurlSock->pstShareLocks, CurlSock->pstShareLocks ? &CurlSock->pstShareLocks->mCurlSharedlock : nullptr, CurlSock->pstShareLocks ? &CurlSock->pstShareLocks->mDnsCurlShareMutex : nullptr, CurlSock->pstShareLocks ? &CurlSock->pstShareLocks->mSslCurlShareMutex : nullptr);
	}
	else
	{
		AAMPLOG_WARN("[curl_debug ]Host %s not in store, Curlfd:%p", hostname.c_str(), curl);
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
		AAMPLOG_WARN("[curl_debug ]RemoveCurlSock checking host:%s lastused:%lld UserCount:%d time:%lld", (it->first).c_str(), CurlSock->timestamp, CurlSock->mCurlStoreUserCount, time);
		AAMPLOG_WARN("[curl_debug ]RemoveCurlSock checking host:%s lastused:%lld UserCount:%d time:%lld mCurlShared:%p pstShareLocks:%p pstShareLocks->mCurlSharedlock:%p pstShareLocks->mDnsCurlShareMutex:%p pstShareLocks->mSslCurlShareMutex:%p",
				(it->first).c_str(), CurlSock->timestamp, CurlSock->mCurlStoreUserCount, time,
				CurlSock->mCurlShared, CurlSock->pstShareLocks, CurlSock->pstShareLocks ? &CurlSock->pstShareLocks->mCurlSharedlock : nullptr, CurlSock->pstShareLocks ? &CurlSock->pstShareLocks->mDnsCurlShareMutex : nullptr, CurlSock->pstShareLocks ? &CurlSock->pstShareLocks->mSslCurlShareMutex : nullptr);
		if( !CurlSock->mCurlStoreUserCount && ( time > CurlSock->timestamp ) )
		{
			time = CurlSock->timestamp;
			RemIt = it;
		}
	}

	if( umCurlSockDataStore.end() != RemIt )
	{
		CurlSocketStoreStruct *RmCurlSock = RemIt->second;
		AAMPLOG_WARN("[curl_debug ]Removing host:%s lastused:%lld UserCount:%d", (RemIt->first).c_str(), RmCurlSock->timestamp, RmCurlSock->mCurlStoreUserCount);

		for(auto it = RmCurlSock->mFreeQ.begin(); it != RmCurlSock->mFreeQ.end(); )
		{
			AAMPLOG_WARN("[curl_debug ]Removing host:%s curlInstance:%d:%p", (RemIt->first).c_str(), it->curlId, it->curl);
			if(it->curl)
			{
				curl_easy_cleanup(it->curl);
				it->curl = nullptr;
				it->eHdlTimestamp = 0;
			}
			it=RmCurlSock->mFreeQ.erase(it);
		}
		std::deque<CurlHandleStruct>().swap(RmCurlSock->mFreeQ);

		AAMPLOG_WARN("[curl_debug ]Removing host:%s curl shared ctx:%p", (RemIt->first).c_str(), RmCurlSock->mCurlShared);
		if(RmCurlSock->mCurlShared)
		{
			curl_share_cleanup(RmCurlSock->mCurlShared);
			RmCurlSock->mCurlShared = nullptr;
			RmCurlSock->pstShareLocks = nullptr;
		}
		AAMPLOG_WARN("[curl_debug ]Cleaning up curl store struct for host:%s", (RemIt->first).c_str());
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
 */
void CurlStore::FlushCurlSockForHost(const std::string &hostname)
{
	const std::lock_guard<std::mutex> lock(mCurlInstLock);
	CurlSockDataIter removeIter = umCurlSockDataStore.find(hostname);
	if( umCurlSockDataStore.end() != removeIter )
	{
		CurlSocketStoreStruct *RmCurlSock = removeIter->second;
		AAMPLOG_WARN("[curl_debug ]Removing host:%s UserCount:%d", (removeIter->first).c_str(), RmCurlSock->mCurlStoreUserCount);
		AAMPLOG_WARN("[curl_debug ]Removing host:%s UserCount:%d mCurlShared:%p pstShareLocks:%p pstShareLocks->mCurlSharedlock:%p pstShareLocks->mDnsCurlShareMutex:%p pstShareLocks->mSslCurlShareMutex:%p",
				(removeIter->first).c_str(), RmCurlSock->mCurlStoreUserCount,
				RmCurlSock->mCurlShared, RmCurlSock->pstShareLocks, RmCurlSock->pstShareLocks ? &RmCurlSock->pstShareLocks->mCurlSharedlock : nullptr, RmCurlSock->pstShareLocks ? &RmCurlSock->pstShareLocks->mDnsCurlShareMutex : nullptr, RmCurlSock->pstShareLocks ? &RmCurlSock->pstShareLocks->mSslCurlShareMutex : nullptr);

		for(auto it = RmCurlSock->mFreeQ.begin(); it != RmCurlSock->mFreeQ.end(); )
		{
			if(it->curl)
			{
				AAMPLOG_WARN("[curl_debug ]Removing host:%s curlInstance:%d:%p", (removeIter->first).c_str(), it->curlId,it->curl);
				curl_easy_cleanup(it->curl);
				it->curl = nullptr;
			}
			it=RmCurlSock->mFreeQ.erase(it);
		}
		std::deque<CurlHandleStruct>().swap(RmCurlSock->mFreeQ);

		AAMPLOG_WARN("[curl_debug ]Removing host:%s UserCount:%d mCurlShared:%p pstShareLocks:%p pstShareLocks->mCurlSharedlock:%p pstShareLocks->mDnsCurlShareMutex:%p pstShareLocks->mSslCurlShareMutex:%p",
				(removeIter->first).c_str(), RmCurlSock->mCurlStoreUserCount,
				RmCurlSock->mCurlShared, RmCurlSock->pstShareLocks, RmCurlSock->pstShareLocks ? &RmCurlSock->pstShareLocks->mCurlSharedlock : nullptr, RmCurlSock->pstShareLocks ? &RmCurlSock->pstShareLocks->mDnsCurlShareMutex : nullptr, RmCurlSock->pstShareLocks ? &RmCurlSock->pstShareLocks->mSslCurlShareMutex : nullptr);
		if(RmCurlSock->mCurlStoreUserCount <=  0 ) 
		{
			if(RmCurlSock->mCurlShared)
			{
				AAMPLOG_WARN("[curl_debug ]Cleaning up curl shared context %p",RmCurlSock->mCurlShared);
				curl_share_cleanup(RmCurlSock->mCurlShared);
				RmCurlSock->mCurlShared = nullptr;
				RmCurlSock->pstShareLocks = nullptr;
			}
			else
			{
				AAMPLOG_WARN("[curl_debug ]no curl shared context available for %s",(removeIter->first).c_str());
			}
			SAFE_DELETE(RmCurlSock);
			umCurlSockDataStore.erase(removeIter);
		}
		else
		{
			AAMPLOG_WARN("mCurlStoreUserCount is still %d.someone is using wait for them to complete the task",RmCurlSock->mCurlStoreUserCount);
		}
	}
	else
	{
		AAMPLOG_WARN("Either hostname %s is removed already or not present", hostname.c_str());
	}
}

/**
 * @fn ShowCurlStoreData
 * @brief ShowCurlStoreData - Print curl store details
 */
void CurlStore::ShowCurlStoreData ( bool trace )
{
	if(trace)
	{
		AAMPLOG_WARN("[curl_debug ]Curl Store Size:%zu, MaxSize:%d", umCurlSockDataStore.size(), MaxCurlSockStore);

		CurlSockDataIter it=umCurlSockDataStore.begin();
		for(int loop=1; it != umCurlSockDataStore.end(); ++it,++loop )
		{
			CurlSocketStoreStruct *CurlSock = it->second;
			AAMPLOG_WARN("[curl_debug ]%d.Host:%s ShHdl:%p LastUsed:%lld UserCount:%d", loop, (it->first).c_str(), CurlSock->mCurlShared, CurlSock->timestamp, CurlSock->mCurlStoreUserCount);
			AAMPLOG_WARN("[curl_debug ]%d.Total Curl fds:%zu,", loop, CurlSock->mFreeQ.size());
			AAMPLOG_WARN("[curl_debug ]%d.mcurlShared:%p pstShareLocks:%p pstShareLocks->mCurlSharedlock:%p pstShareLocks->mDnsCurlShareMutex:%p pstShareLocks->mSslCurlShareMutex:%p", loop, CurlSock->mCurlShared, CurlSock->pstShareLocks, CurlSock->pstShareLocks ? &CurlSock->pstShareLocks->mCurlSharedlock : nullptr, CurlSock->pstShareLocks ? &CurlSock->pstShareLocks->mDnsCurlShareMutex : nullptr, CurlSock->pstShareLocks ? &CurlSock->pstShareLocks->mSslCurlShareMutex : nullptr);


			for(auto it = CurlSock->mFreeQ.begin(); it != CurlSock->mFreeQ.end(); ++it)
			{
				AAMPLOG_WARN("[curl_debug ]CurlFd:%p Time:%lld Inst:%d", it->curl, it->eHdlTimestamp, it->curlId);
				AAMPLOG_WARN("[curl_debug ]CurlFd:%p Time:%lld Inst:%d mCurlShared:%p pstShareLocks:%p pstShareLocks->mCurlSharedlock:%p pstShareLocks->mDnsCurlShareMutex:%p pstShareLocks->mSslCurlShareMutex:%p",
						it->curl, it->eHdlTimestamp, it->curlId,
						CurlSock->mCurlShared, CurlSock->pstShareLocks, CurlSock->pstShareLocks ? &CurlSock->pstShareLocks->mCurlSharedlock : nullptr, CurlSock->pstShareLocks ? &CurlSock->pstShareLocks->mDnsCurlShareMutex : nullptr, CurlSock->pstShareLocks ? &CurlSock->pstShareLocks->mSslCurlShareMutex : nullptr);
			}
		}
	}
}

int GetCurlResponseCode( CURL *curlhandle )
{
	return (int)aamp_CurlEasyGetinfoLong( curlhandle, CURLINFO_RESPONSE_CODE );
}

