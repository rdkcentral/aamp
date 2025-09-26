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

#ifndef AAMP_MOCK_CJSON_MANAGER_H
#define AAMP_MOCK_CJSON_MANAGER_H

#include <gmock/gmock.h>
#include <cjson/cJSON.h>
#include <string>

/**
 * @brief Mock interface for cJSON operations that can be controlled with EXPECT_CALL
 */
class MockCJsonManager 
{
public:
	virtual ~MockCJsonManager() = default;
	
	// Parsing and memory management
	MOCK_METHOD(cJSON*, Parse, (const char *value));
	MOCK_METHOD(void, Free, (void *object));
	MOCK_METHOD(void, Delete, (cJSON *item));
	
	// Object access
	MOCK_METHOD(cJSON*, GetObjectItem, (const cJSON *object, const char *string));
	MOCK_METHOD(int, GetArraySize, (const cJSON *array));
	MOCK_METHOD(cJSON*, GetArrayItem, (const cJSON *array, int index));
	
	// Type checking
	MOCK_METHOD(cJSON_bool, IsArray, (const cJSON *item));
	MOCK_METHOD(cJSON_bool, IsNumber, (const cJSON *item));
	MOCK_METHOD(cJSON_bool, IsTrue, (const cJSON *item));
	
	// Creation
	MOCK_METHOD(cJSON*, CreateArray, ());
	MOCK_METHOD(cJSON*, CreateObject, ());
	
	// Adding items
	MOCK_METHOD(cJSON_bool, AddItemToArray, (cJSON *array, cJSON *item));
	MOCK_METHOD(cJSON*, AddBoolToObject, (cJSON *object, const char *name, cJSON_bool boolean));
	MOCK_METHOD(cJSON*, AddNumberToObject, (cJSON *object, const char *name, double number));
	MOCK_METHOD(cJSON*, AddStringToObject, (cJSON *object, const char *name, const char *string));
	MOCK_METHOD(cJSON*, AddObjectToObject, (cJSON *object, const char *name));
	MOCK_METHOD(cJSON*, AddArrayToObject, (cJSON *object, const char *name));
	
	// Printing
	MOCK_METHOD(std::string, Print, (const cJSON *item));
	MOCK_METHOD(std::string, PrintUnformatted, (const cJSON *item));
};

// Global mock instance that fake cJSON functions will delegate to
extern MockCJsonManager* g_mockCJsonManager;

#endif // AAMP_MOCK_CJSON_MANAGER_H