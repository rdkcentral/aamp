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

#ifndef DRM_MOCK_MEMORY_SYSTEM_H
#define DRM_MOCK_MEMORY_SYSTEM_H

#include <gmock/gmock.h>
#include "DrmMemorySystem.h"

class MockDrmMemorySystem  : public DRMMemorySystem
{
public:

    MockDrmMemorySystem() : DRMMemorySystem()
    {

	}

    MOCK_METHOD(bool, encode, (const uint8_t *dataIn, uint32_t dataInSz, std::vector<uint8_t>& dataOut), (override));

    MOCK_METHOD(bool, decode, (const uint8_t* dataIn, uint32_t dataInSz, uint8_t *dataOut, uint32_t dataOutSz), (override));

    MOCK_METHOD(void, terminateEarly, ());
};


#endif /* DRM_MOCK_MEMORY_SYSTEM_H */
