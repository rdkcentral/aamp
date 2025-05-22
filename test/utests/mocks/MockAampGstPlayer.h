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

#ifndef AAMP_MOCK_AAMP_GST_PLAYER_H
#define AAMP_MOCK_AAMP_GST_PLAYER_H

#include <gmock/gmock.h>
#include "aampgstplayer.h"

static auto mock_id3_callback = [](AampMediaType , const uint8_t * , size_t , const SegmentInfo_t &, const char * scheme_uri){ };

class MockAAMPGstPlayer : public AAMPGstPlayer
{
public:

    MockAAMPGstPlayer( PrivateInstanceAAMP *aamp) : AAMPGstPlayer( aamp, mock_id3_callback) { }

    MOCK_METHOD( long long, GetPositionMilliseconds, (), (override));

    MOCK_METHOD(bool, Pause, (bool pause, bool forceStopGstreamerPreBuffering), (override));

    MOCK_METHOD(bool , SetTextStyle, (const std::string &options), (override));

    MOCK_METHOD(void, ChangeAamp, (PrivateInstanceAAMP *, id3_callback_t));

    MOCK_METHOD(void, Flush, (double position, int rate, bool shouldTearDown), (override));

    MOCK_METHOD(void, SetEncryptedAamp, (PrivateInstanceAAMP *));

    MOCK_METHOD(bool, IsCodecSupported, (const std::string &codecName));

	MOCK_METHOD(void, Stop, (bool), (override));

    MOCK_METHOD(void, SetSubtitleMute, (bool), (override));

    MOCK_METHOD(void, SetPauseOnStartPlayback, (bool enable), (override));

    MOCK_METHOD(bool, SetPlayBackRate, (double), (override));

    MOCK_METHOD(void, SeekStreamSink, (double , double ), (override));
};

extern MockAAMPGstPlayer *g_mockAampGstPlayer;

#endif /* AAMP_MOCK_AAMP_GST_PLAYER_H */
