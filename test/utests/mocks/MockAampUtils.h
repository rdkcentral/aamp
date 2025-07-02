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

#ifndef AAMP_MOCK_AAMP_UTILS_H
#define AAMP_MOCK_AAMP_UTILS_H

#include <gmock/gmock.h>
#include "main_aamp.h"
#include "middleware/InterfacePlayerRDK.h"
class MockAampUtils
{
public:

	MOCK_METHOD(long long, aamp_GetCurrentTimeMS, ());
	MOCK_METHOD(long long, GetCurrentTimeMS, ());

	MOCK_METHOD(std::string, aamp_GetConfigPath, (std::string));

	MOCK_METHOD(bool, parseAndValidateSCTE35, (const std::string &scte35Data));

	MOCK_METHOD(double, GetNetworkTime, (const std::string& remoteUrl, int *http_error , std::string NetworkProxy));

	MOCK_METHOD(std::string, Getiso639map_NormalizeLanguageCode, (std::string, LangCodePreference));

	MOCK_METHOD(double, RecalculatePTS, (AampMediaType mediaType, const void *ptr, size_t len, PrivateInstanceAAMP *aamp));
};

extern MockAampUtils *g_mockAampUtils;

#endif /* AAMP_MOCK_AAMP_UTILS_H */
