/*
* If not stated otherwise in this file or this component's license file the
* following copyright and licenses apply:
*
* Copyright 2024 RDK Management
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
#ifndef AAMP_MOCK_TSB_READER_H
#define AAMP_MOCK_TSB_READER_H
#include <gmock/gmock.h>
#include "AampTsbReader.h"
using namespace TSB;
class MockTSBReader
{
public:
	MOCK_METHOD(AAMPStatusType, Init, (double, float, TuneType));
	MOCK_METHOD(TsbFragmentDataPtr, FindNext, (AampTime));
	MOCK_METHOD(void, ReadNext, (TsbFragmentDataPtr));
	MOCK_METHOD(bool, IsFirstDownload, ());
	MOCK_METHOD(float, GetPlaybackRate, ());
};

extern std::shared_ptr<MockTSBReader> g_mockTSBReader;

#endif /* AAMP_MOCK_TSB_READER_H */
