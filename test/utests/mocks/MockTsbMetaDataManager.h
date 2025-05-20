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

/**
 * @file MockTSbMetaDataManager.h
 * @brief Mock implementation of AampTsbMetaDataManager for unit testing
 */

#ifndef MOCK_TSB_METADATA_MANAGER_H
#define MOCK_TSB_METADATA_MANAGER_H

#include <gmock/gmock.h>
#include "AampTsbMetaDataManager.h"

/**
 * @class MockAampTsbMetaDataManager
 * @brief Mock implementation of AampTsbMetaDataManager
 */
class MockAampTsbMetaDataManager : public AampTsbMetaDataManager {
public:
	MOCK_METHOD(void, Initialize, (), ());
	MOCK_METHOD(void, Cleanup, (), ());

	MOCK_METHOD(std::list<std::shared_ptr<AampTsbMetaData>>, GetMetaDataByType,
		(AampTsbMetaData::Type type, const AampTime& rangeStart, const AampTime& rangeEnd), ());

	MOCK_METHOD(bool, AddMetaData, (const std::shared_ptr<AampTsbMetaData>&), ());
	MOCK_METHOD(bool, RemoveMetaData, (const std::shared_ptr<AampTsbMetaData>&), ());
	MOCK_METHOD(size_t, RemoveMetaData, (const AampTime&), ());
	MOCK_METHOD(bool, ChangeMetaDataPosition, (const std::list<std::shared_ptr<AampTsbMetaData>>&, const AampTime&), ());

	MOCK_METHOD(size_t, RemoveMetaDataIf, (std::function<bool(const std::shared_ptr<AampTsbMetaData>&)>), ());
	MOCK_METHOD(size_t, GetSize, (), (const));
	MOCK_METHOD(void, Dump, (), ());
	MOCK_METHOD(bool, RegisterMetaDataType, (AampTsbMetaData::Type, bool));
	MOCK_METHOD(bool, IsRegisteredType, (AampTsbMetaData::Type, bool&, std::list<std::shared_ptr<AampTsbMetaData>>**), (const));
};

/**
 * @brief Static instance of the MockAampTsbMetaDataManager for global access
 */
extern MockAampTsbMetaDataManager* g_mockAampTsbMetaDataManager;

#endif // MOCK_TSB_METADATA_MANAGER_H
