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

AampCacheHandler::AampCacheHandler(int playerId)
{
}

AampCacheHandler::~AampCacheHandler()
{
}

void AampCacheHandler::InsertToPlaylistCache(const std::string &url, const AampGrowableBuffer* buffer, const std::string &effectiveUrl, bool trackLiveStatus, AampMediaType mediaType)
{
}

void AampCacheHandler::StartPlaylistCache()
{
}

void AampCacheHandler::StopPlaylistCache()
{
}

bool AampCacheHandler::RetrieveFromPlaylistCache(const std::string &url, AampGrowableBuffer* buffer, std::string& effectiveUrl, AampMediaType mediaType)
{
    return false;
}

void AampCacheHandler::SetMaxPlaylistCacheSize(int maxPlaylistCacheSz)
{
}

void AampCacheHandler::SetMaxInitFragCacheSize(int maxInitFragCacheSz)
{
}

bool AampCacheHandler::IsPlaylistUrlCached(const std::string &url)
{
    return false;
}

void AampCacheHandler::RemoveFromPlaylistCache(const std::string &url)
{
}

void AampCacheHandler::InsertToInitFragCache( const std::string &url, const AampGrowableBuffer* buffer, const std::string &effectiveUrl, AampMediaType mediaType )
{
}

bool AampCacheHandler::RetrieveFromInitFragmentCache(const std::string &url, AampGrowableBuffer* buffer, std::string& effectiveUrl)
{
    return false;
}

