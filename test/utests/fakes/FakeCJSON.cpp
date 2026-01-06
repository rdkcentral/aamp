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
*
* Fake implementations of APIs from cJSON which is:
* Copyright (c) 2009-2017 Dave Gamble and cJSON contributors
* Licensed under the MIT License
*/

#include "cjson/cJSON.h"
#include "../mocks/MockCJsonManager.h"
#include <cstdlib>
#include <cstring>

MockCJsonManager *g_mockCJsonManager = nullptr;

CJSON_PUBLIC(cJSON *) cJSON_Parse(const char *value)
{
	return g_mockCJsonManager ? g_mockCJsonManager->Parse(value) : nullptr;
}

CJSON_PUBLIC(void) cJSON_free(void *object)
{
	if (g_mockCJsonManager) {
		g_mockCJsonManager->Free(object);
	}
}

CJSON_PUBLIC(cJSON *) cJSON_GetObjectItem(const cJSON * const object, const char * const string)
{
	return g_mockCJsonManager ? g_mockCJsonManager->GetObjectItem(object, string) : nullptr;
}

CJSON_PUBLIC(void) cJSON_Delete(cJSON *item)
{
	if (g_mockCJsonManager) {
		g_mockCJsonManager->Delete(item);
	}
}

CJSON_PUBLIC(int) cJSON_GetArraySize(const cJSON *array)
{
	return g_mockCJsonManager ? g_mockCJsonManager->GetArraySize(array) : 0;
}

CJSON_PUBLIC(cJSON *) cJSON_GetArrayItem(const cJSON *array, int index)
{
	return g_mockCJsonManager ? g_mockCJsonManager->GetArrayItem(array, index) : nullptr;
}

CJSON_PUBLIC(cJSON_bool) cJSON_IsArray(const cJSON * const item)
{
	return g_mockCJsonManager ? g_mockCJsonManager->IsArray(item) : cJSON_False;
}

CJSON_PUBLIC(cJSON_bool) cJSON_IsNumber(const cJSON * const item)
{
	return g_mockCJsonManager ? g_mockCJsonManager->IsNumber(item) : cJSON_False;
}

CJSON_PUBLIC(cJSON_bool) cJSON_IsTrue(const cJSON * const item)
{
	return g_mockCJsonManager ? g_mockCJsonManager->IsTrue(item) : cJSON_False;
}

CJSON_PUBLIC(const char *) cJSON_GetErrorPtr(void)
{
    return (const char *)"";
}

CJSON_PUBLIC(cJSON *) cJSON_CreateArray(void)
{
	return g_mockCJsonManager ? g_mockCJsonManager->CreateArray() : nullptr;
}

CJSON_PUBLIC(cJSON *) cJSON_CreateObject(void)
{
	return g_mockCJsonManager ? g_mockCJsonManager->CreateObject() : nullptr;
}

CJSON_PUBLIC(cJSON_bool) cJSON_AddItemToArray(cJSON *array, cJSON *item)
{
	return g_mockCJsonManager ? g_mockCJsonManager->AddItemToArray(array, item) : cJSON_False;
}

CJSON_PUBLIC(cJSON*) cJSON_AddBoolToObject(cJSON * const object, const char * const name, const cJSON_bool boolean)
{
	return g_mockCJsonManager ? g_mockCJsonManager->AddBoolToObject(object, name, boolean) : nullptr;
}

CJSON_PUBLIC(cJSON*) cJSON_AddNumberToObject(cJSON * const object, const char * const name, const double number)
{
	return g_mockCJsonManager ? g_mockCJsonManager->AddNumberToObject(object, name, number) : nullptr;
}

CJSON_PUBLIC(cJSON*) cJSON_AddStringToObject(cJSON * const object, const char * const name, const char * const string)
{
	return g_mockCJsonManager ? g_mockCJsonManager->AddStringToObject(object, name, string) : nullptr;
}

CJSON_PUBLIC(cJSON*) cJSON_AddObjectToObject(cJSON * const object, const char * const name)
{
	return g_mockCJsonManager ? g_mockCJsonManager->AddObjectToObject(object, name) : nullptr;
}

CJSON_PUBLIC(cJSON*) cJSON_AddArrayToObject(cJSON * const object, const char * const name)
{
	return g_mockCJsonManager ? g_mockCJsonManager->AddArrayToObject(object, name) : nullptr;
}

CJSON_PUBLIC(char *) cJSON_Print(const cJSON *item)
{
	if (g_mockCJsonManager) {
		std::string result = g_mockCJsonManager->Print(item);
		// Allocate memory with malloc to match real cJSON behavior
		char* mallocResult = static_cast<char*>(malloc(result.length() + 1));
		if (mallocResult) {
			strcpy(mallocResult, result.c_str());
		}
		return mallocResult;
	}
	return strdup(""); // Return malloc'd empty string as fallback
}

CJSON_PUBLIC(char *) cJSON_PrintUnformatted(const cJSON *item)
{
	if (g_mockCJsonManager) {
		std::string result = g_mockCJsonManager->PrintUnformatted(item);
		// Allocate memory with malloc to match real cJSON behavior
		char* mallocResult = static_cast<char*>(malloc(result.length() + 1));
		if (mallocResult) {
			strcpy(mallocResult, result.c_str());
		}
		return mallocResult;
	}
	return strdup(""); // Return malloc'd empty string as fallback
}

