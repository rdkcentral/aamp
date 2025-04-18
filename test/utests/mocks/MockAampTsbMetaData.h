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

#ifndef MOCK_AAMP_TSB_METADATA_H
#define MOCK_AAMP_TSB_METADATA_H

#include <gmock/gmock.h>
#include "AampTsbMetaData.h"

/**
 * @class MockAampTsbMetaData
 * @brief Mock implementation of AampTsbMetaData
 */
class MockAampTsbMetaData : public AampTsbMetaData
{
public:
	MockAampTsbMetaData() : AampTsbMetaData(0) {}

	MOCK_METHOD(Type, GetType, (), (const, override));
	MOCK_METHOD(AampTime , GetPosition, (), (const, override));
	MOCK_METHOD(void, SetPosition, (const AampTime& position), (override));
	MOCK_METHOD(void, Dump, (const std::string& message), (const, override));
	MOCK_METHOD(uint32_t, GetOrderAdded, (), (const, override));
	MOCK_METHOD(void, SetOrderAdded, (uint32_t), (override));
};

extern MockAampTsbMetaData* g_mockAampTsbMetaData;

#endif // MOCK_AAMP_TSB_METADATA_H
