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

#ifndef MOCK_DRM_HELPER_H
#define MOCK_DRM_HELPER_H

#include <gmock/gmock.h>
#include <memory>
#include "DrmHelper.h"

class MockDrmHelper : public DrmHelper
{
public:
	MockDrmHelper() : DrmHelper(DrmInfo()) {}
	
	MOCK_METHOD(const std::string&, ocdmSystemId, (), (const, override));
	MOCK_METHOD(void, createInitData, (std::vector<uint8_t>& initData), (const, override));
	MOCK_METHOD(bool, parsePssh, (const uint8_t* initData, uint32_t initDataLen), (override));
	MOCK_METHOD(bool, isClearDecrypt, (), (const, override));
	MOCK_METHOD(void, getKey, (std::vector<uint8_t>& keyID), (const, override));
	MOCK_METHOD(void, generateLicenseRequest, (const ChallengeInfo& challengeInfo, LicenseRequest& licenseRequest), (const, override));
	MOCK_METHOD(uint32_t, keyProcessTimeout, (), (const, override));
	MOCK_METHOD(uint32_t, licenseGenerateTimeout, (), (const, override));
};

#endif // MOCK_DRM_HELPER_H
