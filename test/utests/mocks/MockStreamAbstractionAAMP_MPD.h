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

#ifndef AAMP_MOCK_STREAM_ABSTRACTION_AAMP_MPD_H
#define AAMP_MOCK_STREAM_ABSTRACTION_AAMP_MPD_H

#include <gmock/gmock.h>
#include "fragmentcollector_mpd.h"

class MockStreamAbstractionAAMP_MPD : public StreamAbstractionAAMP_MPD
{
public:

	MockStreamAbstractionAAMP_MPD(PrivateInstanceAAMP *aamp, double seek_pos, float rate) : StreamAbstractionAAMP_MPD(aamp, seek_pos, rate) { }

	MOCK_METHOD(AAMPStatusType, Init, (TuneType tuneType), (override));
	MOCK_METHOD(AAMPStatusType, InitTsbReader, (TuneType tuneType), (override));
	MOCK_METHOD(BitsPerSecond, GetMaxBitrate, (), (override));
	MOCK_METHOD(void, SeekPosUpdate, (double secondsRelativeToTuneTime), (override) );
	MOCK_METHOD(double, GetMidSeekPosOffset, (), (override));
  };

extern MockStreamAbstractionAAMP_MPD *g_mockStreamAbstractionAAMP_MPD;

#endif /* AAMP_MOCK_STREAM_ABSTRACTION_AAMP_MPD_H */
