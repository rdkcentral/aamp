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

#include "AampCacheHandler.h"

AampCacheHandler::AampCacheHandler( int playerId ): mAsyncCleanUpTaskThreadId(), mCacheActive(false),mAsyncCacheCleanUpThread(false), mCondVarMutex(), mCondVar(), mPlaylistCache(eCACHE_TYPE_PLAYLIST), mbCleanUpTaskInitialized(false), mInitFragmentCache(eCACHE_TYPE_INIT_FRAGMENT), mPlayerId(playerId)
{
}

AampCacheHandler::~AampCacheHandler( void )
{
	if( mbCleanUpTaskInitialized )
	{
		ClearCacheHandler();
	}
}

void AampCacheHandler::InsertToPlaylistCache( const std::string &url, const AampGrowableBuffer* buffer, const std::string &effectiveUrl, bool isLive, AampMediaType mediaType )
{
	std::lock_guard<std::mutex> lock(mCacheAccessMutex);
	assert( !effectiveUrl.empty() );
	if( mediaType==eMEDIATYPE_MANIFEST || !isLive )
	{
		InitializeIfNeeded();
		// First check point; Caching is allowed only if its VOD and for Main Manifest(HLS) for both VOD/Live
		// For Main manifest; mediaType will bypass storing for live content
		mPlaylistCache.Insert( url, buffer, effectiveUrl, mediaType );
		AAMPLOG_TRACE( "Inserted %s URL %s into cache", GetMediaTypeName(mediaType), url.c_str());
	}
}

void AampCacheHandler::StartPlaylistCache( void )
{
	mCacheActive = true;
	std::lock_guard<std::mutex> lock(mCondVarMutex);
	mCondVar.notify_one();
}

void AampCacheHandler::StopPlaylistCache( void )
{
	mCacheActive = false;
	std::lock_guard<std::mutex> lock(mCondVarMutex);
	mCondVar.notify_one();
}

bool AampCacheHandler::RetrieveFromPlaylistCache( const std::string &url, AampGrowableBuffer* buffer, std::string& effectiveUrl, AampMediaType mediaType  )
{
	bool ret = false;
	std::lock_guard<std::mutex> lock(mCacheAccessMutex);
	AampCachedData *cachedData = mPlaylistCache.Find(url);
	if( cachedData )
	{
		effectiveUrl = cachedData->effectiveUrl;
		if( effectiveUrl.empty() )
		{
			effectiveUrl = url;
		}
		buffer->Clear();
		buffer->AppendBytes( cachedData->buffer->GetPtr(), cachedData->buffer->GetLen() );
		// below fails when playing an HLS playlist directly, then seeking or retuning
		// assert( mediaType == cachedData->mediaType );
		AAMPLOG_TRACE( "%s %s found", GetMediaTypeName(cachedData->mediaType), url.c_str() );
		ret = true;
	}
	else
	{
		AAMPLOG_TRACE("%s %s not found", GetMediaTypeName(mediaType), url.c_str());
	}
	return ret;
}

void AampCacheHandler::SetMaxPlaylistCacheSize(int maxBytes)
{
	std::lock_guard<std::mutex> lock(mCacheAccessMutex);
	mPlaylistCache.maxPlaylistCacheBytes = maxBytes;
	AAMPLOG_MIL("Setting maxPlaylistCacheBytes to :%d", maxBytes);
}

void AampCacheHandler::SetMaxInitFragCacheSize( int maxFragmentsPerTrack )
{
	std::lock_guard<std::mutex> lock(mCacheAccessMutex);
	mInitFragmentCache.maxCachedInitFragmentsPerTrack = maxFragmentsPerTrack;
	AAMPLOG_MIL("Setting maxCachedInitFragmentsPerTrack to :%d",maxFragmentsPerTrack);
}

bool AampCacheHandler::IsPlaylistUrlCached( const std::string &playlistUrl )
{
	std::lock_guard<std::mutex> lock(mCacheAccessMutex);
	return mPlaylistCache.Find(playlistUrl) != NULL;
}

void AampCacheHandler::RemoveFromPlaylistCache( const std::string &url )
{
	std::lock_guard<std::mutex> lock(mCacheAccessMutex);
	mPlaylistCache.Remove( url );
}

void AampCacheHandler::InsertToInitFragCache( const std::string &url, const AampGrowableBuffer* buffer, const std::string &effectiveUrl, AampMediaType mediaType )
{
	std::lock_guard<std::mutex> lock(mCacheAccessMutex);
	assert( !effectiveUrl.empty() );
	InitializeIfNeeded();
	mInitFragmentCache.Insert( url, buffer, effectiveUrl, mediaType );
}

bool AampCacheHandler::RetrieveFromInitFragmentCache(const std::string &url, AampGrowableBuffer* buffer, std::string& effectiveUrl)
{
	bool ret = false;
	std::lock_guard<std::mutex> lock(mCacheAccessMutex);
	AampCachedData *cachedData = mInitFragmentCache.Find(url);
	if( cachedData )
	{
		AampGrowableBuffer * buf = cachedData->buffer;
		effectiveUrl = cachedData->effectiveUrl;
		if( effectiveUrl.empty() )
		{
			effectiveUrl = url;
		}
		buffer->Clear();
		buffer->AppendBytes( buf->GetPtr(), buf->GetLen() );
		AAMPLOG_INFO("%s %s found", GetMediaTypeName(cachedData->mediaType), url.c_str());
		ret = true;
	}
	else
	{
		AAMPLOG_INFO("%s %s not found.", GetMediaTypeName(eMEDIATYPE_DEFAULT), url.c_str());
	}
	return ret;
}

