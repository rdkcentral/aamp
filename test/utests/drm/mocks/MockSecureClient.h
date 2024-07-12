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

#ifndef AAMP_MOCK_SECURE_CLIENT_H
#define AAMP_MOCK_SECURE_CLIENT_H

#include "sec_client.h"
#include <gmock/gmock.h>

class MockSecureClient
{
public:
	// This method has 16 parameters, but googlemock only supports 10.
	// Therefore, only including the parameters that tests verify here.
	MOCK_METHOD(int32_t, SecClient_AcquireLicense,
				(const char *serviceHostUrl, char **licenseResponse,
				 size_t *licenseResponseLength));
};

extern MockSecureClient *g_mocksecclient;

#endif /* AAMP_MOCK_SECURE_CLIENT_H */
