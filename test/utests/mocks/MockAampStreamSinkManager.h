/*
* If not stated otherwise in this file or this component's license file the
* following copyright and licenses apply:
*
* Copyright 2023 RDK Management
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

#ifndef AAMP_MOCK_AAMP_STREAMSINK_MANAGER_H
#define AAMP_MOCK_AAMP_STREAMSINK_MANAGER_H

#include <gmock/gmock.h>
#include "AampStreamSinkManager.h"

class MockAampStreamSinkManager : public AampStreamSinkManager
{
public:

	MOCK_METHOD(void, SetSinglePipelineMode, (PlayerInstanceAAMP *));
	MOCK_METHOD(void, CreateStreamSink, (PlayerInstanceAAMP *, id3_callback_t , (std::function< void(uint8_t *, int, int, int) >)));
	MOCK_METHOD(void, SetStreamSink, (PlayerInstanceAAMP *, StreamSink *));
	MOCK_METHOD(void, DeleteStreamSink, (PlayerInstanceAAMP *));
	MOCK_METHOD(void, ActivatePlayer, (PlayerInstanceAAMP *));
	MOCK_METHOD(void, DeactivatePlayer, (PlayerInstanceAAMP *, bool));
	MOCK_METHOD(StreamSink*, GetActiveStreamSink, (PlayerInstanceAAMP *));
	MOCK_METHOD(StreamSink*, GetStreamSink, (PlayerInstanceAAMP *));
	MOCK_METHOD(StreamSink*, GetStoppingStreamSink, (PlayerInstanceAAMP *));

	MOCK_METHOD(void, SetEncryptedHeaders, (PlayerInstanceAAMP *, (std::map<int, std::string>)& ));
	MOCK_METHOD(void, GetEncryptedHeaders, ((std::map<int, std::string>)&));
	MOCK_METHOD(void, SetActive, (PlayerInstanceAAMP *));
	MOCK_METHOD(void, UpdateTuningPlayer, (PlayerInstanceAAMP *));
};

extern MockAampStreamSinkManager *g_mockAampStreamSinkManager;

#endif /* AAMP_MOCK_AAMP_STREAMSINK_MANAGER_H */
