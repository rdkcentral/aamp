
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
#ifndef AAMP_MOCK_TSB_STORE_H
#define AAMP_MOCK_TSB_STORE_H

#include <gmock/gmock.h>
#include "TsbApi.h"

using namespace TSB;

class MockTSBStore
{
public:
	MOCK_METHOD(TSB::Status, Write, (const std::string& url, const void* buffer, std::size_t size));
	MOCK_METHOD(TSB::Status, Read, (const std::string& url, void* buffer, std::size_t size), (const));
	MOCK_METHOD(std::size_t, GetSize, (const std::string& url), (const));
	MOCK_METHOD(void, Delete, (const std::string& url));
	MOCK_METHOD(void, Flush, ());
};

extern MockTSBStore *g_mockTSBStore;

#endif /* AAMP_MOCK_TSB_STORE_H */
