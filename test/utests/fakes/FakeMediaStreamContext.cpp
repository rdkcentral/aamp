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

#include "MediaStreamContext.h"
#include "MockMediaStreamContext.h"

MockMediaStreamContext *g_mockMediaStreamContext = nullptr;

bool MediaStreamContext::CacheFragmentChunk(AampMediaType actualType, const char *ptr, size_t size, std::string remoteUrl,long long dnldStartTime)
{
    return false;
}

bool MediaStreamContext::CacheFragment(std::string fragmentUrl, unsigned int curlInstance, double position, double duration, const char *range, bool initSegment, bool discontinuity, bool playingAd, double pto, uint32_t scale, bool overWriteTrackId)
{
	bool rv = true;

	if (g_mockMediaStreamContext != nullptr)
	{
		rv = g_mockMediaStreamContext->CacheFragment(fragmentUrl, curlInstance, position, duration, range, initSegment, discontinuity, playingAd, pto, scale, overWriteTrackId);
	}

    return rv;
}

void MediaStreamContext::InjectFragmentInternal(CachedFragment* cachedFragment, bool &fragmentDiscarded, bool isDiscontinuity)
{
}

void MediaStreamContext::ABRProfileChanged(void)
{
}

double MediaStreamContext::GetBufferedDuration()
{
	return 0.0;
}

void MediaStreamContext::SignalTrickModeDiscontinuity()
{
}

bool MediaStreamContext::IsAtEndOfTrack()
{
	return false;
}

std::string& MediaStreamContext::GetPlaylistUrl()
{
	return mPlaylistUrl;
}

std::string& MediaStreamContext::GetEffectivePlaylistUrl()
{
	return mEffectiveUrl;
}

void MediaStreamContext::SetEffectivePlaylistUrl(std::string url)
{
	mEffectiveUrl = url;
}

long long MediaStreamContext::GetLastPlaylistDownloadTime()
{
	return 0;
}

void MediaStreamContext::SetLastPlaylistDownloadTime(long long time)
{
}

long MediaStreamContext::GetMinUpdateDuration()
{
	return 0;
}

int MediaStreamContext::GetDefaultDurationBetweenPlaylistUpdates()
{
	return 0;
}
void MediaStreamContext::updateSkipPoint(double skipPoint, double skipDuration)
{
}
void MediaStreamContext::setDiscontinuityState(bool isDiscontinuity)
{
}
void MediaStreamContext::abortWaitForVideoPTS()
{
}

bool MediaStreamContext::CacheTsbFragment(std::shared_ptr<CachedFragment> fragment)
{
	if (g_mockMediaStreamContext != nullptr)
	{
		return g_mockMediaStreamContext->CacheTsbFragment(fragment);
	}
	else
	{
		return false;
	}
}
