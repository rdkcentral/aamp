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

#ifndef AAMP_MOCK_STREAM_ABSTRACTION_AAMP_H
#define AAMP_MOCK_STREAM_ABSTRACTION_AAMP_H

#include <gmock/gmock.h>
#include "StreamAbstractionAAMP.h"

class MockStreamAbstractionAAMP : public StreamAbstractionAAMP
{
public:

	MockStreamAbstractionAAMP(PrivateInstanceAAMP *aamp) : StreamAbstractionAAMP(aamp) { }

	MOCK_METHOD(void, NotifyPlaybackPaused, (bool paused), (override));

	MOCK_METHOD(AAMPStatusType, Init, (TuneType tuneType), (override));

	MOCK_METHOD(void, Start, (), (override));

	MOCK_METHOD(void, Stop, (bool clearChannelData), (override));

	MOCK_METHOD(void, GetStreamFormat, (StreamOutputFormat &primaryOutputFormat, StreamOutputFormat &audioOutputFormat, StreamOutputFormat &auxAudioOutputFormat, StreamOutputFormat &subtitleOutputFormat), (override));

	MOCK_METHOD(double, GetStreamPosition, (), (override));

	MOCK_METHOD(double, GetFirstPTS, (), (override));

	MOCK_METHOD(double, GetStartTimeOfFirstPTS, (), (override));

	MOCK_METHOD(MediaTrack*, GetMediaTrack, (TrackType type), (override));

	MOCK_METHOD(double, GetBufferedDuration, (), (override));

	MOCK_METHOD(int, GetBWIndex, (long bandwidth), (override));

	MOCK_METHOD(std::vector<long>, GetVideoBitrates, (), (override));

	MOCK_METHOD(std::vector<long>, GetAudioBitrates, (), (override));

	MOCK_METHOD(void, StartInjection, (), (override));

	MOCK_METHOD(void, StopInjection, (), (override));

	MOCK_METHOD(void, SeekPosUpdate, (double secondsRelativeToTuneTime), (override));

	MOCK_METHOD(std::vector<StreamInfo*>, GetAvailableVideoTracks, (), (override));

	MOCK_METHOD(std::vector<StreamInfo*>, GetAvailableThumbnailTracks, (), (override));

	MOCK_METHOD(bool, SetThumbnailTrack, (int), (override));

	MOCK_METHOD(std::vector<ThumbnailData>, GetThumbnailRangeData, (double, double, std::string*, int*, int*, int*, int*), (override));

	MOCK_METHOD(StreamInfo* , GetStreamInfo, (int idx), (override));

	MOCK_METHOD(bool , Is4KStream, (int &height, long &bandwidth), (override));

	MOCK_METHOD(bool , SetTextStyle, (const std::string &options), (override));

	MOCK_METHOD(void, UpdateFailedDRMStatus, (LicensePreFetchObject *object), (override));

	MOCK_METHOD(std::vector<AudioTrackInfo> &, GetAvailableAudioTracks, (bool allTrack), (override));

	MOCK_METHOD(std::vector<TextTrackInfo> &, GetAvailableTextTracks, (bool allTrack), (override));

	MOCK_METHOD(void, MuteSubtitles, (bool));

	MOCK_METHOD(void, SetVideoPlaybackRate, (float rate));

	MOCK_METHOD(bool, SelectPreferredTextTrack, (TextTrackInfo& selectedTextTrack), (override));

	MOCK_METHOD(bool, IsEOSReached, (), (override));
};

extern MockStreamAbstractionAAMP *g_mockStreamAbstractionAAMP;

#endif /* AAMP_MOCK_STREAM_ABSTRACTION_AAMP_H */
