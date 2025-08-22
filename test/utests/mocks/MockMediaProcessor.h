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

#ifndef AAMP_MOCK_MEDIA_PROCESSOR_H
#define AAMP_MOCK_MEDIA_PROCESSOR_H

#include <gmock/gmock.h>
#include "mediaprocessor.h"

class MockMediaProcessor : public MediaProcessor
{
public:
	MockMediaProcessor() : MediaProcessor() {}
	MOCK_METHOD(double, getFirstPts, (AampGrowableBuffer* pBuffer), (override));
	MOCK_METHOD(void, setPtsOffset, (double ptsOffset), (override));
	MOCK_METHOD(bool, sendSegment, (AampGrowableBuffer* pBuffer, double position, double duration, double fragmentPTSoffset, bool discontinuous, bool isInit, process_fcn_t processor, bool &ptsError), (override));
	MOCK_METHOD(void, setRate, (double rate, PlayMode mode), (override));
	MOCK_METHOD(void, setThrottleEnable, (bool enable), (override));
	MOCK_METHOD(void, setFrameRateForTM, (int frameRate), (override));
	MOCK_METHOD(void, abort, (), (override));
	MOCK_METHOD(void, reset, (), (override));
	MOCK_METHOD(void, ChangeMuxedAudioTrack, (unsigned char index), (override));
	MOCK_METHOD(void, SetAudioGroupId, (std::string& id), (override));
	MOCK_METHOD(void, setApplyOffsetFlag, (bool enable), (override));
	MOCK_METHOD(void, abortInjectionWait, (), (override));
	MOCK_METHOD(void, enable, (bool enable), (override));
	MOCK_METHOD(void, setTrackOffset, (double offset), (override));
};

#endif /* AAMP_MOCK_MEDIA_PROCESSOR_H */
