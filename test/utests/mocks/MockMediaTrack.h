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

#ifndef AAMP_MOCK_MEDIA_TRACK_H
#define AAMP_MOCK_MEDIA_TRACK_H

#include <gmock/gmock.h>
#include "StreamAbstractionAAMP.h"

class MockMediaTrack : public MediaTrack
{
public:
	MockMediaTrack(TrackType type, PrivateInstanceAAMP *aamp, const char *name)
		: MediaTrack(type, aamp, name) {}
	MockMediaTrack()
		: MediaTrack(eTRACK_VIDEO, nullptr, "mock") {}
	MOCK_METHOD(CachedFragment*, GetFetchBuffer, (bool initialize));
	MOCK_METHOD(CachedFragment*, GetFetchChunkBuffer, (bool initialize));
	MOCK_METHOD(void, UpdateTSAfterFetch, (bool isInitSegment));
	MOCK_METHOD(void, UpdateTSAfterChunkFetch, ());
	MOCK_METHOD(void, UpdateTSAfterInject, ());
	MOCK_METHOD(bool, IsInjectionFromCachedFragmentChunks, ());
	MOCK_METHOD(bool, IsLocalTSBInjection, ());
	MOCK_METHOD(bool, Enabled, ());
	MOCK_METHOD(void, ProcessPlaylist, (AampGrowableBuffer& newPlaylist, int http_error), (override));
	MOCK_METHOD(std::string&, GetPlaylistUrl, (), (override));
	MOCK_METHOD(std::string&, GetEffectivePlaylistUrl, (), (override));
	MOCK_METHOD(void, SetEffectivePlaylistUrl, (std::string url), (override));
	MOCK_METHOD(long long, GetLastPlaylistDownloadTime, (), (override));
	MOCK_METHOD(void, SetLastPlaylistDownloadTime, (long long time), (override));
	MOCK_METHOD(long, GetMinUpdateDuration, (), (override));
	MOCK_METHOD(int, GetDefaultDurationBetweenPlaylistUpdates, (), (override));
	MOCK_METHOD(void, ABRProfileChanged, (), (override));
	MOCK_METHOD(void, updateSkipPoint, (double position, double duration ), (override));
	MOCK_METHOD(void, setDiscontinuityState, (bool isDiscontinuity), (override));
	MOCK_METHOD(void, abortWaitForVideoPTS, (), (override));
	MOCK_METHOD(double, GetBufferedDuration, (), (override));
	MOCK_METHOD(class StreamAbstractionAAMP*, GetContext, (), (override));
	MOCK_METHOD(void, InjectFragmentInternal, (CachedFragment* cachedFragment, bool &fragmentDiscarded,bool isDiscontinuity), (override));
	MOCK_METHOD(double, GetTotalInjectedDuration, (), (override));
};

extern MockMediaTrack *g_mockMediaTrack;

#endif /* AAMP_MOCK_MEDIA_TRACK_H */
