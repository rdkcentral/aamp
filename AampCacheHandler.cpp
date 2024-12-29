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

/**************************************
* @file AampCacheHandler.cpp
* @brief Cache handler operations for AAMP
**************************************/

#include "AampCacheHandler.h"


/**
 *  @brief Retrieve playlist from cache  
 */
void AampCacheHandler::InsertToPlaylistCache(const std::string url, const AampGrowableBuffer* buffer, std::string effectiveUrl,bool trackLiveStatus,AampMediaType mediaType)
{
	PlayListCachedData *tmpData,*newtmpData;
	std::lock_guard<std::recursive_mutex> lock(mMutex);

	if(buffer->GetLen()==0)
	{
		AAMPLOG_ERR("Insert to PlaylistCache Failed due to empty buffer ");
	}
	else
	{

		//Initialize AampCacheHandler
		Init();

		// First check point , Caching is allowed only if its VOD and for Main Manifest(HLS) for both VOD/Live
		// For Main manifest , mediaType will bypass storing for live content
		if(trackLiveStatus==false || mediaType==eMEDIATYPE_MANIFEST)
		{

			PlaylistCacheIter it = mPlaylistCache.find(url);
			if (it != mPlaylistCache.end())
			{
				AAMPLOG_INFO("playlist %s already present in cache", url.c_str());
			}
			// insert only if buffer size is less than Max size
			else
			{
				if(mediaType==eMEDIATYPE_MANIFEST && mPlaylistCache.size())
				{
					// If new Manifest is inserted which is not present in the cache , flush out other playlist files related with old manifest,
					ClearPlaylistCache();
				}
				// Dont check for CacheSize if Max is configured as unlimited 
				if(mMaxPlaylistCacheSize == PLAYLIST_CACHE_SIZE_UNLIMITED || (mMaxPlaylistCacheSize != PLAYLIST_CACHE_SIZE_UNLIMITED  && buffer->GetLen() < mMaxPlaylistCacheSize))
				{
					// Before inserting into cache, need to check if max cache size will exceed or not on adding new data
					// if more , need to pop out some from same type of playlist
					bool cacheStoreReady = true;
					if(mMaxPlaylistCacheSize != PLAYLIST_CACHE_SIZE_UNLIMITED  && ((mCacheStoredSize + buffer->GetLen()) > mMaxPlaylistCacheSize))
					{
						AAMPLOG_WARN("Count[%zu]Avail[%d]Needed[%zu] Reached max cache size", mPlaylistCache.size(),mCacheStoredSize,buffer->GetLen() );
						cacheStoreReady = AllocatePlaylistCacheSlot(mediaType,buffer->GetLen() );
					}
					if(cacheStoreReady)
					{
						tmpData = new PlayListCachedData();
						tmpData->mCachedBuffer = new AampGrowableBuffer("playlist-cachedBuffer");
						tmpData->mCachedBuffer->AppendBytes( buffer->GetPtr(), buffer->GetLen() );

						tmpData->mEffectiveUrl = effectiveUrl;
						tmpData->mMediaType = mediaType;
						mPlaylistCache[url] = tmpData;
						mCacheStoredSize += buffer->GetLen();
						AAMPLOG_INFO("Inserted. url %s", url.c_str());
						// There are cases where Main url and effective url will be different ( for Main manifest)
						// Need to store both the entries with same content data 
						// When retune happens within aamp due to failure , effective url wll be asked to read from cached manifest
						// When retune happens from JS , regular Main url will be asked to read from cached manifest. 
						// So need to have two entries in cache table but both pointing to same CachedBuffer (no space is consumed for storage)
						{
							// if n only there is diff in url , need to store both
							if(url != effectiveUrl)
							{
								newtmpData = new PlayListCachedData();
								// Not to allocate for Cachebuffer again , use the same buffer as above
								newtmpData->mCachedBuffer = tmpData->mCachedBuffer;
								newtmpData->mEffectiveUrl = effectiveUrl;
								// This is a duplicate entry
								newtmpData->mDuplicateEntry = true;
								newtmpData->mMediaType = mediaType;
								mPlaylistCache[effectiveUrl] = newtmpData;
								AAMPLOG_INFO("Added an effective url entry %s", effectiveUrl.c_str());
							}
						}
					}
				}
			}
		}

	}
}


/**
 *  @brief Retrieve playlist from cache
 */
bool AampCacheHandler::RetrieveFromPlaylistCache(const std::string url, AampGrowableBuffer* buffer, std::string& effectiveUrl)
{
	AampGrowableBuffer* buf = NULL;
	bool ret; 
	std::string eUrl;
	std::lock_guard<std::recursive_mutex> lock(mMutex);
	PlaylistCacheIter it = mPlaylistCache.find(url);
	if (it != mPlaylistCache.end())
	{
		PlayListCachedData *tmpData = it->second;
		buf = tmpData->mCachedBuffer;
		eUrl = tmpData->mEffectiveUrl;
		buffer->Clear();
		buffer->AppendBytes( buf->GetPtr(), buf->GetLen() );
		effectiveUrl = eUrl;
		AAMPLOG_TRACE("url %s found", url.c_str());
		ret = true;
	}
	else
	{
		AAMPLOG_TRACE("url %s not found", url.c_str());
		ret = false;
	}
	return ret;
}

/**
 *  @brief Remove specific playlist cache
 */
void AampCacheHandler::RemoveFromPlaylistCache(const std::string url)
{
	std::lock_guard<std::recursive_mutex> lock(mMutex);
	PlaylistCacheIter it = mPlaylistCache.find(url);
	if (it != mPlaylistCache.end())
	{
		PlayListCachedData *tmpData = it->second;
		if(!tmpData->mDuplicateEntry)
		{
			SAFE_DELETE(tmpData->mCachedBuffer);
		}
		SAFE_DELETE(tmpData);

		AAMPLOG_INFO("Removing Playlist URL %s from Cache", it->first.c_str());
		mPlaylistCache.erase(it);
	}
	else
	{
		AAMPLOG_WARN("Playlist URL %s not found in cache", url.c_str());
	}
}

/**
 *  @brief Clear playlist cache
 */
void AampCacheHandler::ClearPlaylistCache()
{
	std::lock_guard<std::recursive_mutex> lock(mMutex);
	AAMPLOG_INFO("cache size %d", (int)mPlaylistCache.size());
	PlaylistCacheIter it = mPlaylistCache.begin();
	for (;it != mPlaylistCache.end(); it++)
	{
		PlayListCachedData *tmpData = it->second;
		if(!tmpData->mDuplicateEntry)
		{
			SAFE_DELETE(tmpData->mCachedBuffer);
		}
		SAFE_DELETE(tmpData);
	}
	mCacheStoredSize = 0;
	mPlaylistCache.clear();

}

/**
 *  @brief AllocatePlaylistCacheSlot Allocate Slot for adding new playlist
 */
bool AampCacheHandler::AllocatePlaylistCacheSlot(AampMediaType mediaType,size_t newLen)
{
	bool retVal = true;
	size_t freedSize=0;
	if(mPlaylistCache.size())
	{
		if(mediaType == eMEDIATYPE_MANIFEST)
		{
			// This case cannot happen, but for safety need to handle.
			// If for any reason Main Manifest is pushed after cache is full , better clear all the playlist cached .
			// As per new Main Manifest  ,new  playlist files need to be downloaded and cached
			ClearPlaylistCache();
		}
		else // for non main manifest
		{
		PlaylistCacheIter Iter = mPlaylistCache.begin();
		// Two pass to remove the item from cache to create space for caching
		// First pass : Search for same file type to clean, If Video need to be inserted , free another Video type
		// 				if audio type to be inserted , remove older audio type . Same for iframe .
		// Second pass : Even after removing same file type entry ,still not enough space to add new item then remove from other file type ( rare scenario)
		while(Iter != mPlaylistCache.end())
		{
			PlayListCachedData *tmpData = Iter->second;
			if(tmpData->mMediaType == eMEDIATYPE_MANIFEST || tmpData->mMediaType != mediaType)
			{ 	// Not to remove main manifest file and filetype which are different
				Iter++;
				continue;
			}
			if(!tmpData->mDuplicateEntry)
			{
				freedSize += tmpData->mCachedBuffer->GetLen();
				SAFE_DELETE(tmpData->mCachedBuffer);
			}
			SAFE_DELETE(tmpData);
			Iter = mPlaylistCache.erase(Iter);
		}
		//Second Pass - if still more cleanup required for space, remove  from other playlist types
		if(freedSize < newLen)
		{
			Iter = mPlaylistCache.begin();
			while(Iter != mPlaylistCache.end())
			{
				PlayListCachedData *tmpData = Iter->second;
				if(tmpData->mMediaType == eMEDIATYPE_MANIFEST)
				{ 	// Not to remove main manifest file
					Iter++;
					continue;
				}
				if(!tmpData->mDuplicateEntry)
				{
					freedSize += tmpData->mCachedBuffer->GetLen();
					SAFE_DELETE(tmpData->mCachedBuffer);
				}
				SAFE_DELETE(tmpData);
				Iter = mPlaylistCache.erase(Iter);
			}
		}
		mCacheStoredSize -= freedSize;
		// After all freeing still size is not enough to insert , better not allow to insert such huge file
		if(freedSize < newLen)
			retVal = false;
		}
	}
	return retVal;
}


/**
 *  @brief Initialization Function
 */
void AampCacheHandler::Init()
{
	//Check if already initialized
	if(true == mInitialized)
		return;

	try 
	{
		mAsyncCleanUpTaskThreadId = std::thread(&AampCacheHandler::AsyncCacheCleanUpTask, this);
		pthread_mutex_lock(&mCondVarMutex);
		mAsyncCacheCleanUpThread = true;
		pthread_mutex_unlock(&mCondVarMutex);  //CID:168111 - Missing lock
		AAMPLOG_INFO("Thread created AsyncCacheCleanUpTask[%zx]", GetPrintableThreadID(mAsyncCleanUpTaskThreadId));
	}
	catch(std::exception &e)
	{
		AAMPLOG_ERR("Failed to create AampCacheHandler thread : %s", e.what());
	}
	mInitialized = true;
}

/**
 *  @brief Clear Cache Handler. Exit clean up thread.
 */
void AampCacheHandler::ClearCacheHandler()
{
	//Check if already uninitialized
	if(false == mInitialized)
		return;

	mCacheActive = true;
	pthread_mutex_lock(&mCondVarMutex);
	mAsyncCacheCleanUpThread = false;
	pthread_cond_signal(&mCondVar);
	pthread_mutex_unlock(&mCondVarMutex);
	if(mAsyncCleanUpTaskThreadId.joinable())
	{
		mAsyncCleanUpTaskThreadId.join();
	}
	ClearPlaylistCache();

	//Clear init fragment & track queue
	ClearInitFragCache();
	mInitialized = false;
}

/**
 *  @brief Default Constructor
 */
AampCacheHandler::AampCacheHandler(int playerId):
	mCacheStoredSize(0),mAsyncCleanUpTaskThreadId(),mCacheActive(false),
	mAsyncCacheCleanUpThread(false),mMutex(),mCondVarMutex(),mCondVar(),mPlaylistCache()
	,mMaxPlaylistCacheSize(MAX_PLAYLIST_CACHE_SIZE*1024),mInitialized(false)
	,umInitFragCache(),umCacheTrackQ(),bInitFragCache(false),mInitFragMutex()
	,MaxInitCacheSlot(MAX_INIT_FRAGMENT_CACHE_PER_TRACK),mPlayerId(playerId)
{
	pthread_mutex_init(&mCondVarMutex, NULL);
	pthread_cond_init(&mCondVar, NULL);

	pthread_mutex_init(&mInitFragMutex, NULL);
}


/**
 *  @brief Destructor Function
 */
AampCacheHandler::~AampCacheHandler()
{
	if(true == mInitialized)
		ClearCacheHandler();
	pthread_mutex_destroy(&mCondVarMutex);
	pthread_cond_destroy(&mCondVar);

	pthread_mutex_destroy(&mInitFragMutex);
}

/**
 *  @brief Start playlist caching
 */
void AampCacheHandler::StartPlaylistCache()
{
	mCacheActive = true;
	pthread_mutex_lock(&mCondVarMutex);
	pthread_cond_signal(&mCondVar);
	pthread_mutex_unlock(&mCondVarMutex );
}

/**
 *  @brief Stop playlist caching
 */
void AampCacheHandler::StopPlaylistCache()
{
	mCacheActive = false;
	pthread_mutex_lock(&mCondVarMutex);
	pthread_cond_signal(&mCondVar);
	pthread_mutex_unlock(&mCondVarMutex );
}

/**
 *  @brief Thread function for Async Cache clean
 */
void AampCacheHandler::AsyncCacheCleanUpTask()
{
	UsingPlayerId playerId( mPlayerId );
	pthread_mutex_lock(&mCondVarMutex);
	while (mAsyncCacheCleanUpThread)
	{
		pthread_cond_wait(&mCondVar, &mCondVarMutex);
		if(!mCacheActive)
		{
			struct timespec ts;
			ts = aamp_GetTimespec(10000);

			if(ETIMEDOUT == pthread_cond_timedwait(&mCondVar, &mCondVarMutex, &ts))
			{
				AAMPLOG_INFO("[%p] Cacheflush timed out", this);
				ClearPlaylistCache();

				//Clear init fragment & track queue
				ClearInitFragCache();
			}
		}
	}
	pthread_mutex_unlock(&mCondVarMutex);
}

/**
 *  @brief SetMaxPlaylistCacheSize - Set Max Cache Size
 */
void AampCacheHandler::SetMaxPlaylistCacheSize(int maxPlaylistCacheSz)
{
	std::lock_guard<std::recursive_mutex> lock(mMutex);
	mMaxPlaylistCacheSize = maxPlaylistCacheSz;
	AAMPLOG_WARN("Setting mMaxPlaylistCacheSize to :%d",maxPlaylistCacheSz);
}

/**
 *  @brief IsUrlCached - Check if URL is already cached
 */
bool AampCacheHandler::IsUrlCached(std::string url)
{
	bool retval = false;
	std::lock_guard<std::recursive_mutex> lock(mMutex);
	PlaylistCacheIter it = mPlaylistCache.find(url);
	if (it != mPlaylistCache.end())
		retval = true;

	return retval;
}


/**
 *  @brief Insert init fragment into cache table
 */
void AampCacheHandler::InsertToInitFragCache(const std::string url, const AampGrowableBuffer* buffer,
						std::string effectiveUrl, AampMediaType mediaType)
{
	pthread_mutex_lock(&mInitFragMutex);

	if(buffer->GetLen()==0)
	{
		AAMPLOG_ERR("Insert to InitFragCache Failed due to empty buffer ");
	}

	else
	{

		InitFragCacheStruct *NewInitData;
		InitFragTrackStruct *NewTrackQueueCache;



		//Initialize AampCacheHandler
		Init();

		InitFragCacheIter Iter = umInitFragCache.find(url);
		if ( Iter != umInitFragCache.end() )
		{
			AAMPLOG_INFO("playlist %s already present in cache", url.c_str());
		}
		else
		{
			NewInitData = new playlistcacheddata();
			NewInitData->mCachedBuffer = new AampGrowableBuffer("playlist-cached-data");
			NewInitData->mCachedBuffer->AppendBytes( buffer->GetPtr(), buffer->GetLen() );

			NewInitData->mEffectiveUrl = effectiveUrl;
			NewInitData->mMediaType = mediaType;
			NewInitData->mDuplicateEntry = (effectiveUrl.length() && (effectiveUrl!=url));

			/*
			* Check if queue created for given track type, if not create a queue
			* and add a corresponding url into a track queue.
			*/
			CacheTrackQueueIter IterCq = umCacheTrackQ.find(mediaType);
			if(IterCq == umCacheTrackQ.end())
			{
				NewTrackQueueCache = new initfragtrackstruct();
				NewTrackQueueCache->trackQueue.push(url);

				umCacheTrackQ[mediaType]=NewTrackQueueCache;
			}
			else
			{
				NewTrackQueueCache = IterCq->second;
				NewTrackQueueCache->trackQueue.push(url);
			}


			/*
			* If track queue of given filetype reached maximum limit, remove the very first
			* inserted entry & its duplicate entry from cache table.
			*/
			if ( NewTrackQueueCache->trackQueue.size() > MaxInitCacheSlot )
			{
				RemoveInitFragCacheEntry ( mediaType );
			}

			umInitFragCache[url] = NewInitData;
			AAMPLOG_INFO("Inserted init url %s", url.c_str());


			/*
			* If current init fragment has any redirected url which is different, that must be
			* inserted in cache table with same init fragment data of url and which will not inserted in track queue.
			*/
			if ( NewInitData->mDuplicateEntry )
			{
				InitFragCacheStruct *DupInitData;
				DupInitData = new playlistcacheddata();
				DupInitData->mCachedBuffer = NewInitData->mCachedBuffer;
				DupInitData->mEffectiveUrl = effectiveUrl;
				DupInitData->mMediaType = mediaType;
				umInitFragCache[effectiveUrl] = DupInitData;
				AAMPLOG_INFO("Inserted effective init url %s", url.c_str());
			}

			AAMPLOG_INFO("Size [CacheTable:%d,TrackQ:%d,CurrentTrack:%d,MaxLimit:%d]\n", (int)umInitFragCache.size(), (int)umCacheTrackQ.size(), (int)NewTrackQueueCache->trackQueue.size(), MaxInitCacheSlot );
		}

	}
	pthread_mutex_unlock(&mInitFragMutex);
}

/**
 *  @brief Retrieve init fragment from cache
 */
bool AampCacheHandler::RetrieveFromInitFragCache(const std::string url, AampGrowableBuffer* buffer,
									std::string& effectiveUrl)
{
	AampGrowableBuffer* buf = NULL;
	bool ret;
	std::string eUrl;
	pthread_mutex_lock(&mInitFragMutex);
	InitFragCacheIter it = umInitFragCache.find(url);
	if (it != umInitFragCache.end())
	{
		InitFragCacheStruct *findFragData = it->second;
		buf = findFragData->mCachedBuffer;
		eUrl = findFragData->mEffectiveUrl;
		buffer->Clear();
		buffer->AppendBytes( buf->GetPtr(), buf->GetLen() );
		effectiveUrl = eUrl;
		AAMPLOG_INFO("url %s found", url.c_str());
		ret = true;
	}
	else
	{
		AAMPLOG_INFO("url %s not found", url.c_str());
		ret = false;
	}
	pthread_mutex_unlock(&mInitFragMutex);
	return ret;
}

/**
 *  @brief Removes very first inserted entry ( and duplicate entry, if present)  of given filetype
 *         from fragment cache table in FIFO order, also removes the corresponding url from track queue.
 */ 
void AampCacheHandler::RemoveInitFragCacheEntry ( AampMediaType mediaType )
{
	CacheTrackQueueIter IterCq = umCacheTrackQ.find(mediaType);
	if(IterCq == umCacheTrackQ.end())
	{
		return;
	}

	InitFragTrackStruct *QueuePerTrack = IterCq->second;

	/* Delete data in FIFO order
	 * Get url from Track Queue, which is used to
	 * remove fragment entry from cache table in FIFO order.
	 */
	InitFragCacheIter Iter = umInitFragCache.find(QueuePerTrack->trackQueue.front());
	if (Iter != umInitFragCache.end())
	{
		InitFragCacheStruct *removeCacheData = Iter->second;

		/* Remove duplicate entry
		 * Check if the first inserted data has any duplicate entry, If so
		 * then delete the duplicate entry as well.
		 */
		if ( removeCacheData->mDuplicateEntry )
		{
			InitFragCacheStruct *dupData;
			InitFragCacheIter IterDup = umInitFragCache.find(removeCacheData->mEffectiveUrl);
			if(IterDup != umInitFragCache.end())
			{
				dupData = IterDup->second;
				SAFE_DELETE(dupData);
				umInitFragCache.erase(IterDup);
				AAMPLOG_INFO("Removed dup url:%s",removeCacheData->mEffectiveUrl.c_str());
			}
		}
		SAFE_DELETE(removeCacheData->mCachedBuffer);
		SAFE_DELETE(removeCacheData);
		umInitFragCache.erase(Iter);
		AAMPLOG_INFO("Removed main url:%s",QueuePerTrack->trackQueue.front().c_str());

		// Remove the url entry from corresponding track queue.
		QueuePerTrack->trackQueue.pop();
	}
}

/**
 *  @brief Clear init fragment cache & track queue table
 */
void AampCacheHandler::ClearInitFragCache()
{
	AAMPLOG_INFO("Fragment cache size %d", (int)umInitFragCache.size());
	InitFragCacheIter it = umInitFragCache.begin();
	for (;it != umInitFragCache.end(); it++)
	{
		InitFragCacheStruct *delData = it->second;
		if(!delData->mDuplicateEntry)
		{
			SAFE_DELETE(delData->mCachedBuffer);
		}
		SAFE_DELETE(delData);
	}
	umInitFragCache.clear();

	AAMPLOG_INFO("Track queue size %d", (int)umCacheTrackQ.size());
	CacheTrackQueueIter IterCq = umCacheTrackQ.begin();
	for (;IterCq != umCacheTrackQ.end(); IterCq++)
	{
		InitFragTrackStruct *delTrack = IterCq->second;

		std::queue<std::string>().swap(delTrack->trackQueue);
		SAFE_DELETE(delTrack);
	}
	umCacheTrackQ.clear();
}

/**
 *  @brief SetMaxInitFragCacheSize - Set Max Cache Size
 */
void AampCacheHandler::SetMaxInitFragCacheSize(int maxInitFragCacheSz)
{
	pthread_mutex_lock(&mInitFragMutex);
	MaxInitCacheSlot = maxInitFragCacheSz;
	AAMPLOG_WARN("Setting mMaxPlaylistCacheSize to :%d",maxInitFragCacheSz);
	pthread_mutex_unlock(&mInitFragMutex);	
}
