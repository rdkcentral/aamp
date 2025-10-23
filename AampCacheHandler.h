/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2018 RDK Management
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
 * @file AampCacheHandler.h
 * @brief Cache handler for AAMP
 */
#ifndef __AAMP_CACHE_HANDLER_H__
#define __AAMP_CACHE_HANDLER_H__

#include <iostream>
#include <memory>
#include <unordered_map>
#include <exception>
#include <mutex>
#include <condition_variable>
#include "AampGrowableBuffer.h"
#include "AampMediaType.h"
#include "AampUtils.h"
#include "AampLogManager.h"
#include "AampDefine.h"

#define PLAYLIST_CACHE_SIZE_UNLIMITED -1

/**
 * @brief AampCacheData to cache Initialization Fragments, HLS main manifest, and HLS VOD playlists
 *
 * For DASH playback, this module is not involved.  Instead AampMPDDownloader handles caching the static VOD DASH manifest..
 * Caches are cleared upon exiting from aamp player or by an async thread that automatically purges after ~10 seconds of no active playback.
 */
class AampCachedData
{
public:
	std::string effectiveUrl;
	std::shared_ptr<AampGrowableBuffer> buffer;
	AampMediaType mediaType;
	long seqNo;

	~AampCachedData() {};

	/**
	 * @brief representation for a cache entry
	 *
	 * @param effectiveUrl if equal to url (primary key for cache), this is a self contained entry with no alternate effectiveUrl alias;
	 * if empty, this is a mirrored entry for the corresponding effectiveUrl, sharing storage with main entry;
	 * if populated and not same as url, this is main cache entry, with effectiveUrl also present in cache as an alias
	 *
	 * @param buffer data payload associated with cache entry (initialization fragment or playlist)
	 * @param mediaType type of cache entry
	 * @param seqNo bigger for more recent usage; used to drive LRU purging heuristic
	 */
	AampCachedData(const std::string &effectiveUrl, std::shared_ptr<AampGrowableBuffer> buffer, AampMediaType mediaType)
		: effectiveUrl(effectiveUrl)
		, buffer(buffer)
		, mediaType(mediaType)
		, seqNo()
	{
	}
};

typedef enum
{
	eCACHE_TYPE_INIT_FRAGMENT,
	eCACHE_TYPE_PLAYLIST
} AampCacheType;

class AampCache
{
private:
	AampCacheType cacheType;
	size_t totalCachedBytes;
	long seqNo;

	/**
	 *   @fn allocatePlaylistCacheSlot
	 *   @param[in] mediaType type of playlist caller wants to add to cache
	 *   @param[in] targetCacheSize  threshold (bytes) for needed cache size reduction
	 *
	 *   @return bool Success or Failure
	 */
	void reduceCacheSize( AampMediaType mediaType, size_t targetCacheSize )
	{
		// First pass - remove playlists only of specific type
		auto iter = cache.begin();
		AAMPLOG_WARN( "removing %s playlists from cache", GetMediaTypeName(mediaType) );
		while(iter != cache.end())
		{
			AampCachedData *cachedData = iter->second;
			if(cachedData->mediaType == eMEDIATYPE_MANIFEST || cachedData->mediaType != mediaType)
			{ // leave main manifest and alternate playlist types
				iter++;
			}
			else
			{
				if( !cachedData->effectiveUrl.empty() )
				{ // not alias; reclaim space
					totalCachedBytes -= cachedData->buffer->GetLen();
				}
				SAFE_DELETE(cachedData);
				iter = cache.erase(iter);
			}
		}

		//Second Pass - if more reduction needed, remove other playlist types, too
		if( totalCachedBytes <= targetCacheSize )
		{
			AAMPLOG_WARN( "removing ALL playlists from cache" );
			iter = cache.begin();
			while(iter != cache.end())
			{
				AampCachedData *cachedData = iter->second;
				if( cachedData->mediaType == eMEDIATYPE_MANIFEST )
				{ // leave main manifest
					iter++;
				}
				else
				{
					if( !cachedData->effectiveUrl.empty() )
					{
						totalCachedBytes -= cachedData->buffer->GetLen();
					}
					SAFE_DELETE(cachedData);
					iter = cache.erase(iter);
				}
			}
		}
	}

	bool makeRoomForPlaylist( AampMediaType mediaType, size_t bytesNeeded )
	{
		bool ok = true;
		if( mediaType==eMEDIATYPE_MANIFEST )
		{ // flush and old playlist files (associated with different manifest)
			Clear();
		}
		else if( maxPlaylistCacheBytes != PLAYLIST_CACHE_SIZE_UNLIMITED )
		{ // cache size constraint to be enforced
			if( totalCachedBytes+bytesNeeded > maxPlaylistCacheBytes  )
			{
				reduceCacheSize( mediaType, maxPlaylistCacheBytes - bytesNeeded );
				ok = totalCachedBytes+bytesNeeded <= maxPlaylistCacheBytes;
			}
		}
		return ok;
	}

	bool makeRoomForInitFragment( AampMediaType mediaType )
	{
		int count = 0;
		std::unordered_map<std::string, AampCachedData *>::iterator lru = cache.end();
		auto iter = cache.begin();
		while( iter != cache.end() )
		{
			AampCachedData *cachedData = iter->second;
			if(cachedData->mediaType == mediaType && !cachedData->effectiveUrl.empty() )
			{
				if( lru==cache.end() || cachedData->seqNo < lru->second->seqNo )
				{
					lru = iter;
				}
				count++;
			}
			iter++;
		}
		if( count >= maxCachedInitFragmentsPerTrack )
		{
			AAMPLOG_WARN( "removing entry from %s init fragment cache", GetMediaTypeName(mediaType) );
			Remove( lru->first );
		}
		return true; // success
	}

	int countReferencesToEffectiveUrl( const std::string effectiveUrl )
	{
		int count = 0;
		for (auto& it: cache)
		{
			AampCachedData *cachedData = it.second;
			if(cachedData->effectiveUrl == effectiveUrl)
			{
				count++;
			}
		}
		return count;
	}

public:
	int maxCachedInitFragmentsPerTrack;
	int maxPlaylistCacheBytes;
	std::unordered_map<std::string, AampCachedData *> cache;

	AampCache()
	{
	}

	AampCache( AampCacheType cacheType ) : cacheType(cacheType), cache(), totalCachedBytes(), maxPlaylistCacheBytes(MAX_PLAYLIST_CACHE_SIZE*1024), maxCachedInitFragmentsPerTrack(MAX_INIT_FRAGMENT_CACHE_PER_TRACK), seqNo()
	{
	}

	~AampCache()
	{
	}

public:
	void Insert( const std::string &url, const AampGrowableBuffer* buffer, const std::string &effectiveUrl, AampMediaType mediaType )
	{
		if( buffer->GetLen()==0 )
		{
			AAMPLOG_ERR( "empty buffer" );
		}
		else if( Find(url) )
		{ // should never happen - caller has no business downloading if already cached
			AAMPLOG_ERR("%s %s already cached", GetMediaTypeName(mediaType), url.c_str());
		}
		else
		{
			bool ok = false;
			switch( cacheType )
			{
				case eCACHE_TYPE_INIT_FRAGMENT:
					ok = makeRoomForInitFragment( mediaType );
					break;
				case eCACHE_TYPE_PLAYLIST:
					ok = makeRoomForPlaylist( mediaType, buffer->GetLen() );
					break;
				default:
					break;
			}
			if( ok )
			{
				size_t len = buffer->GetLen();
				AampCachedData *cachedData = new AampCachedData( effectiveUrl, std::make_shared<AampGrowableBuffer>("cached-data"), mediaType );
				cachedData->buffer->AppendBytes( buffer->GetPtr(), len );

				cache[url] = cachedData;
				cachedData->seqNo = ++seqNo;
				totalCachedBytes += len;
				AAMPLOG_MIL( "inserted %s %s", GetMediaTypeName(mediaType), url.c_str() ); // used by l2tests
				// There are cases where main url and effective url will be different (often for main manifest)
				// Need to store both the entries with same content data
				// When retune happens within aamp due to failure, effective url wll be asked to read from cached manifest
				// When retune happens from JS, regular Main url will be asked to read from cached manifest.
				// So need to have two entries in cache table but both pointing to same CachedBuffer (no space is consumed for storage)
				if( url != effectiveUrl )
				{ // re-use buffer without extra copy
					if ( cache.find(effectiveUrl) != cache.end() )
					{	// effective url was already in cache, so delete the old one
						SAFE_DELETE(cache[effectiveUrl]);
					}
					AampCachedData *newData = new AampCachedData( "", cachedData->buffer, mediaType );
					cache[effectiveUrl] = newData;
					AAMPLOG_MIL( "duplicate %s %s", GetMediaTypeName(mediaType), effectiveUrl.c_str() );
				}
			}
		}
	}

	void Remove( const std::string &url )
	{
		auto iter = cache.find(url);
		assert( iter != cache.end() );
		AampCachedData *cachedData = iter->second;
		totalCachedBytes -= cachedData->buffer->GetLen();
		assert( !cachedData->effectiveUrl.empty() );
		if(( url != cachedData->effectiveUrl ) &&
			(countReferencesToEffectiveUrl(cachedData->effectiveUrl) == 1))
		{ // remove main entry with payload
			auto iter2 = cache.find(cachedData->effectiveUrl);
			assert( iter2 != cache.end() );
			AampCachedData *cachedData2 = iter2->second;
			assert( cachedData2->effectiveUrl.empty() );
			SAFE_DELETE(cachedData2);
			cache.erase(iter2);
		}
		SAFE_DELETE(cachedData);
		cache.erase(iter);
	}

	void Clear( void )
	{
		for( auto it = cache.begin(); it != cache.end(); it++)
		{
			AampCachedData *cachedData = it->second;
			SAFE_DELETE(cachedData);
		}
		cache.clear();
		totalCachedBytes = 0;
	}

	AampCachedData * Find( const std::string &url )
	{
		AampCachedData *cachedData = NULL;
		auto it = cache.find(url);
		if( it != cache.end() )
		{ // cache hit
			cachedData = it->second;
			cachedData->seqNo = ++seqNo;
		}
		return cachedData;
	}
};

/**
 * @class AampCacheHandler
 * @brief Handles Aamp cache operations
 */

class AampCacheHandler
{
private:
	int mPlayerId;
	std::mutex mCacheAccessMutex;
	AampCache mPlaylistCache;
	AampCache mInitFragmentCache;
	bool mbCleanUpTaskInitialized;
	bool mCacheActive;
	bool mAsyncCacheCleanUpThread;
	std::mutex mCondVarMutex;
	std::condition_variable mCondVar;
	std::thread mAsyncCleanUpTaskThreadId;

protected:
	/**
	 *  @brief Thread function for Async Cache clean
	 */
	void AsyncCacheCleanUpTask( void )
	{
		UsingPlayerId playerId(mPlayerId);
		std::unique_lock<std::mutex> lock(mCondVarMutex);

		while( mAsyncCacheCleanUpThread )
		{
			mCondVar.wait(lock);
			if(!mCacheActive)
			{
				std::cv_status status = mCondVar.wait_for(lock, std::chrono::seconds(10));
				if( status == std::cv_status::timeout )
				{
					AAMPLOG_MIL("[%p] Cacheflush timed out", this);
					mPlaylistCache.Clear();
					mInitFragmentCache.Clear();
				}
			}
		}
	}

	/**
	* @fn Init
	*/
	void InitializeIfNeeded( void )
	{
		if( !mbCleanUpTaskInitialized )
		{
			try
			{
				mAsyncCleanUpTaskThreadId = std::thread(&AampCacheHandler::AsyncCacheCleanUpTask, this);
				{
					std::lock_guard<std::mutex> guard(mCondVarMutex);
					mAsyncCacheCleanUpThread = true;
				}
				AAMPLOG_INFO("Thread created AsyncCacheCleanUpTask[%zx]", GetPrintableThreadID(mAsyncCleanUpTaskThreadId));
			}
			catch(std::exception &e)
			{
				AAMPLOG_ERR( "Failed to create AampCacheHandler thread : %s", e.what() );
			}
			mbCleanUpTaskInitialized = true;
		}
	}

	/**
	 *  @brief Clear Cache Handler. Exit clean up thread.
	 */
	void ClearCacheHandler( void )
	{
		if( mbCleanUpTaskInitialized )
		{
			mCacheActive = true;
			{
				std::lock_guard<std::mutex> guard(mCondVarMutex);
				mAsyncCacheCleanUpThread = false;
				mCondVar.notify_one();
			}
			if(mAsyncCleanUpTaskThreadId.joinable())
			{
				mAsyncCleanUpTaskThreadId.join();
			}
			mPlaylistCache.Clear();
			mInitFragmentCache.Clear();
			mbCleanUpTaskInitialized = false;
		}
	}

public:
	/**
	 * @brief constructor
	 */
	AampCacheHandler( int playerId );

	/**
	 *  @brief destructor
	 */
	~AampCacheHandler( void );

	/**
	 *  @brief Start playlist caching
	 */
	void StartPlaylistCache( void );

	/**
	 *  @brief Stop playlist caching
	 */
	void StopPlaylistCache( void );

	/**
	 *   @brief Add playlist to cache
	 *   @param[in] url - URL
	 *   @param[in] buffer - Pointer to growable buffer
	 *   @param[in] effectiveUrl - Final URL
	 *   @param[in] isLive
	 *   @param[in] mediaType - Type of the file inserted
	 *
	 *   @return void
	 */
	void InsertToPlaylistCache( const std::string &url, const AampGrowableBuffer* buffer, const std::string &effectiveUrl, bool isLive, AampMediaType mediaType );

	/**
	 *   @brief Find playlist in cache
	 *   @param[in] url - URL
	 *   @param[out] buffer - Pointer to growable buffer
	 *   @param[out] effectiveUrl - Final URL
	 *   @return true: found, false: not found
	 */
	bool RetrieveFromPlaylistCache( const std::string &url, AampGrowableBuffer* buffer, std::string& effectiveUrl, AampMediaType mediaType );

	/**
	 * @brief Remove playlist from cache
	 * @param[in] url - URL
	 */
	void RemoveFromPlaylistCache( const std::string &url );

	/**
	 *  @brief set max playlist cache size (bytes)
	 */
	void SetMaxPlaylistCacheSize(int maxBytes);

	/**
	 * @brief get max playlist cache size (bytes)
	 *
	 * @return int - maxCacheSize
	 */
	int GetMaxPlaylistCacheSize() { return mPlaylistCache.maxPlaylistCacheBytes; }

	/**
	 *  @brief check if playlist in cache
	 */
	bool IsPlaylistUrlCached( const std::string &playlistUrl );

	/**
	 *   @brief add initialization fragment to cache
	 *
	 *   @param[in] url - URL
	 *   @param[in] buffer - Pointer to growable buffer
	 *   @param[in] effectiveUrl - Final URL
	 *   @param[in] mediaType - Type of the file inserted
	 *
	 *   @return void
	 */
	void InsertToInitFragCache( const std::string &url, const AampGrowableBuffer* buffer, const std::string &effectiveUrl, AampMediaType mediaType );

	/**
	 *   @brief Find initialization fragment in cache
	 *
	 *   @param[in] url - URL
	 *   @param[out] buffer - Pointer to growable buffer
	 *   @param[out] effectiveUrl - Final URL
	 *
	 *   @return true: found, false: not found
	 */
	bool RetrieveFromInitFragmentCache(const std::string &url, AampGrowableBuffer* buffer, std::string& effectiveUrl);

	/**
	*   @brief set max initialization fragments allowed in cache (per track)
	*
	*   @param[in] maxInitFragCacheSz - CacheSize
	*
	*   @return None
	*/
	void SetMaxInitFragCacheSize( int maxFragmentsPerTrack );

	/**
	*   @brief GetMaxPlaylistCacheSize - Get present CacheSize
	*
	*   @return int - maxCacheSize
	*/
	int GetMaxInitFragCacheSize() { return mInitFragmentCache.maxCachedInitFragmentsPerTrack; }

	/**
	 * @brief Copy constructor disabled
	 */
	AampCacheHandler(const AampCacheHandler&) = delete;
	/**
	 * @brief assignment operator disabled
	 */
	AampCacheHandler& operator=(const AampCacheHandler&) = delete;
};

#endif
