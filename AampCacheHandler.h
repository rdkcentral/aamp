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
#include <vector>
#include <assert.h>
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
	std::shared_ptr<std::vector<uint8_t>> buffer;
	AampMediaType mediaType;
	long seqNo;

	/**
	 * @brief Pointer to the "effectiveUrl" entry in the cache.
	 * For a Main URL entry, this points to the AampCachedData object
	 * keyed by the effectiveUrl. For an Alias entry, this is nullptr.
	 * use_count() on this shared_ptr provides an O(1) way to see
	 * how many URLs are currently redirected to the same effectiveUrl.
	 */
	std::shared_ptr<AampCachedData> eUrlCachedDataPtr;

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
	AampCachedData(const std::string &effectiveUrl, std::shared_ptr<std::vector<uint8_t>> buffer, AampMediaType mediaType)
		: effectiveUrl(effectiveUrl), buffer(buffer), mediaType(mediaType), seqNo()
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
	void reduceCacheSize(AampMediaType mediaType, size_t targetCacheSize)
	{
		// First pass - remove playlists only of specific type
		auto iter = cache.begin();
		AAMPLOG_WARN("removing %s playlists from cache", GetMediaTypeName(mediaType));
		while (iter != cache.end())
		{
			AampCachedData *cachedData = iter->second.get();
			if (cachedData->mediaType == eMEDIATYPE_MANIFEST || cachedData->mediaType != mediaType)
			{ // leave main manifest and alternate playlist types
				iter++;
			}
			else
			{
				iter = cache.erase(iter);
			}
		}

		// Second Pass - if more reduction needed, remove other playlist types, too
		if (totalCachedBytes <= targetCacheSize)
		{
			AAMPLOG_WARN("removing ALL playlists from cache");
			iter = cache.begin();
			while (iter != cache.end())
			{
				AampCachedData *cachedData = iter->second.get();
				if (cachedData->mediaType == eMEDIATYPE_MANIFEST)
				{ // leave main manifest
					iter++;
				}
				else
				{
					iter = cache.erase(iter);
				}
			}
		}
	}

	bool makeRoomForPlaylist(AampMediaType mediaType, size_t bytesNeeded)
	{
		bool ok = true;
		if (mediaType == eMEDIATYPE_MANIFEST)
		{ // flush and old playlist files (associated with different manifest)
			Clear();
		}
		else if (maxPlaylistCacheBytes != PLAYLIST_CACHE_SIZE_UNLIMITED)
		{ // cache size constraint to be enforced
			if (totalCachedBytes + bytesNeeded > maxPlaylistCacheBytes)
			{
				reduceCacheSize(mediaType, maxPlaylistCacheBytes - bytesNeeded);
				ok = totalCachedBytes + bytesNeeded <= maxPlaylistCacheBytes;
			}
		}
		return ok;
	}

	bool makeRoomForInitFragment(AampMediaType mediaType)
	{
		int count = 0;
		auto lru = cache.end();
		auto iter = cache.begin();
		while (iter != cache.end())
		{
			AampCachedData *cachedData = iter->second.get();
			if (cachedData->mediaType == mediaType && !cachedData->effectiveUrl.empty())
			{
				if (lru == cache.end() || cachedData->seqNo < lru->second->seqNo)
				{
					lru = iter;
				}
				count++;
			}
			iter++;
		}
		if (count >= maxCachedInitFragmentsPerTrack)
		{
			AAMPLOG_WARN("removing entry from %s init fragment cache", GetMediaTypeName(mediaType));
			Remove(lru->first);
		}
		return true; // success
	}

public:
	int maxCachedInitFragmentsPerTrack;
	int maxPlaylistCacheBytes;
	std::unordered_map<std::string, std::shared_ptr<AampCachedData>> cache;

	AampCache()
	{
	}

	AampCache(AampCacheType cacheType) : cacheType(cacheType), cache(), totalCachedBytes(), maxPlaylistCacheBytes(MAX_PLAYLIST_CACHE_SIZE * 1024), maxCachedInitFragmentsPerTrack(MAX_INIT_FRAGMENT_CACHE_PER_TRACK), seqNo()
	{
	}

	~AampCache()
	{
	}

public:
	void Insert(const std::string &url, const std::vector<uint8_t> &buffer, const std::string &effectiveUrl, AampMediaType mediaType)
	{
		// High hit early exit scenario
		if (cache.find(url) != cache.end())
		{
			AAMPLOG_ERR("%s %s already cached", GetMediaTypeName(mediaType), url.c_str());
			return;
		}

		if (buffer.empty())
		{
			AAMPLOG_ERR("empty buffer");
			return;
		}

		bool ok = false;
		switch (cacheType)
		{
		case eCACHE_TYPE_INIT_FRAGMENT:
			ok = makeRoomForInitFragment(mediaType);
			break;
		case eCACHE_TYPE_PLAYLIST:
			ok = makeRoomForPlaylist(mediaType, buffer.size());
			break;
		default:
			break;
		}

		if (ok)
		{
			try
			{ // Do the allocations up front to avoid partial state updates on failure
				// auto cachedBuf = std::make_shared<std::vector<uint8_t>>(buffer);
				size_t bytes = buffer.size();
				auto cachedBuf = std::shared_ptr<std::vector<uint8_t>>(
					new std::vector<uint8_t>(buffer),
					[this, bytes](std::vector<uint8_t> *ptr)
					{
						this->totalCachedBytes -= bytes;
						delete ptr;
					});
				// Update total cached bytes for the buffer as if a latter allocation fails the dtor will then decrement it again leaving it correct.
				totalCachedBytes += cachedBuf->size();

				auto cachedData = std::make_shared<AampCachedData>(effectiveUrl, cachedBuf, mediaType);

				std::shared_ptr<AampCachedData> aliasData = nullptr;
				if (url != effectiveUrl)
				{
					auto itEff = cache.find(effectiveUrl);
					if (itEff == cache.end())
					{
						// only allocate when alias key not present
						aliasData = std::make_shared<AampCachedData>("", cachedBuf,
																	 mediaType);
						cache.insert_or_assign(effectiveUrl, std::move(aliasData));
						AAMPLOG_MIL("inserted eUrl %s %s",
									GetMediaTypeName(mediaType), effectiveUrl.c_str());
					}
					else
					{
						// update existing alias to point to new buffer and metadata
						aliasData = itEff->second;
						aliasData->buffer = cachedBuf;
						aliasData->mediaType = mediaType;
						AAMPLOG_MIL("updated eUrl %s %s",
									GetMediaTypeName(mediaType), effectiveUrl.c_str());
					}
					cachedData->eUrlCachedDataPtr = aliasData;
				}

				cachedData->seqNo = ++seqNo;
				cache[url] = std::move(cachedData);

				AAMPLOG_MIL("inserted %s %s", GetMediaTypeName(mediaType), url.c_str()); // used by l2tests
			}
			catch (const std::bad_alloc &e)
			{
				AAMPLOG_ERR("Memory allocation failed: %s", e.what());
			}
		}
	}

	void Remove(const std::string &url)
	{
		auto iter = cache.find(url);
		assert(iter != cache.end());

		AampCachedData *cachedData = iter->second.get();
		assert(!cachedData->effectiveUrl.empty());

		if (cachedData->eUrlCachedDataPtr)
		{
			if (cachedData->eUrlCachedDataPtr.use_count() == 2) // only this URL and effectiveUrl alias point to it
			{
				cache.erase(cachedData->effectiveUrl);
			}
		}

		cache.erase(iter);
	}

	void Clear(void)
	{
		// unique_ptr owned entries are destructed automatically
		cache.clear();
		totalCachedBytes = 0;
	}

	AampCachedData *Find(const std::string &url)
	{
		if (auto it = cache.find(url); it != cache.end())
		{
			it->second->seqNo = ++seqNo; // Update LRU priority
			return it->second.get();
		}
		return nullptr;
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
	void AsyncCacheCleanUpTask(void)
	{
		UsingPlayerId playerId(mPlayerId);
		std::unique_lock<std::mutex> lock(mCondVarMutex);

		while (mAsyncCacheCleanUpThread)
		{
			mCondVar.wait(lock);
			if (!mCacheActive)
			{
				std::cv_status status = mCondVar.wait_for(lock, std::chrono::seconds(10));
				if (status == std::cv_status::timeout)
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
	void InitializeIfNeeded(void)
	{
		if (!mbCleanUpTaskInitialized)
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
			catch (std::exception &e)
			{
				AAMPLOG_ERR("Failed to create AampCacheHandler thread : %s", e.what());
			}
			mbCleanUpTaskInitialized = true;
		}
	}

	/**
	 *  @brief Clear Cache Handler. Exit clean up thread.
	 */
	void ClearCacheHandler(void)
	{
		if (mbCleanUpTaskInitialized)
		{
			mCacheActive = true;
			{
				std::lock_guard<std::mutex> guard(mCondVarMutex);
				mAsyncCacheCleanUpThread = false;
				mCondVar.notify_one();
			}
			if (mAsyncCleanUpTaskThreadId.joinable())
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
	AampCacheHandler(int playerId);

	/**
	 *  @brief destructor
	 */
	~AampCacheHandler(void);

	/**
	 *  @brief Start playlist caching
	 */
	void StartPlaylistCache(void);

	/**
	 *  @brief Stop playlist caching
	 */
	void StopPlaylistCache(void);

	/**
	 * @brief Add playlist to cache
	 * @param[in] url URL identifying the playlist to cache
	 * @param[in] buffer Reference to the payload buffer (std::vector<uint8_t>) to store in cache
	 * @param[in] effectiveUrl Final/effective URL for the resource (used as aliasing key)
	 * @param[in] isLive True if this is a live playlist
	 * @param[in] mediaType Type of the file inserted (see AampMediaType)
	 * @return void
	 */
	void InsertToPlaylistCache(const std::string &url, const std::vector<uint8_t> &buffer, const std::string &effectiveUrl, bool isLive, AampMediaType mediaType);

	/**
	 * @brief Find playlist in cache
	 * @param[in] url URL to look up in cache
	 * @param[out] buffer Reference to a vector which will be replaced with the cached payload on a hit
	 * @param[out] effectiveUrl The effective URL associated with the cached entry (returned on hit)
	 * @param[in] mediaType Expected media type for this lookup (guards cache matching)
	 * @return true if entry found and buffer/effectiveUrl were set, false otherwise
	 */
	bool RetrieveFromPlaylistCache(std::string url, std::vector<uint8_t> &buffer, std::string &effectiveUrl, AampMediaType mediaType);

	/**
	 * @brief Remove playlist from cache
	 * @param[in] url - URL
	 */
	void RemoveFromPlaylistCache(const std::string &url);

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
	bool IsPlaylistUrlCached(const std::string &playlistUrl);

	/**
	 * @brief Add initialization fragment to cache
	 * @param[in] url URL identifying the initialization fragment
	 * @param[in] buffer Reference to the payload buffer (std::vector<uint8_t>) to store in cache
	 * @param[in] effectiveUrl Final/effective URL for the resource
	 * @param[in] mediaType Type of the file inserted (initial fragment media type)
	 * @return void
	 */
	void InsertToInitFragCache(const std::string &url, const std::vector<uint8_t> &buffer, const std::string &effectiveUrl, AampMediaType mediaType);

	/**
	 * @brief Find initialization fragment in cache
	 * @param[in] url URL to look up in cache
	 * @param[out] buffer Reference to a vector which will be replaced with the cached payload on a hit
	 * @param[out] effectiveUrl The effective URL associated with the cached entry (returned on hit)
	 * @return true if entry found and buffer/effectiveUrl were set, false otherwise
	 */
	bool RetrieveFromInitFragmentCache(std::string url, std::vector<uint8_t> &buffer, std::string &effectiveUrl);

	/**
	 *   @brief set max initialization fragments allowed in cache (per track)
	 *
	 *   @param[in] maxInitFragCacheSz - CacheSize
	 *
	 *   @return None
	 */
	void SetMaxInitFragCacheSize(int maxFragmentsPerTrack);

	/**
	 *   @brief GetMaxPlaylistCacheSize - Get present CacheSize
	 *
	 *   @return int - maxCacheSize
	 */
	int GetMaxInitFragCacheSize() { return mInitFragmentCache.maxCachedInitFragmentsPerTrack; }

	/**
	 * @brief Copy constructor disabled
	 */
	AampCacheHandler(const AampCacheHandler &) = delete;
	/**
	 * @brief assignment operator disabled
	 */
	AampCacheHandler &operator=(const AampCacheHandler &) = delete;
};

#endif
