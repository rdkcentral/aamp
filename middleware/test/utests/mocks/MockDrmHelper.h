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
*/

#ifndef AAMP_MOCK_AAMP_DRM_HELPER_H
#define AAMP_MOCK_AAMP_DRM_HELPER_H

#include <gmock/gmock.h>
#include "DrmHelper.h"

class MockDrmHelper : public DrmHelper
{
public:

    MockDrmHelper() : DrmHelper(DrmInfo{})
    {

	}

	MOCK_METHOD(const std::string&, ocdmSystemId, (), (const, override));

	MOCK_METHOD(void, createInitData, (std::vector<uint8_t>& initData), (const, override));

	MOCK_METHOD(bool, parsePssh, (const uint8_t* initData, uint32_t initDataLen), (override));

	MOCK_METHOD(bool, isClearDecrypt, (), (const));

	MOCK_METHOD(bool, isHdcp22Required, (), (const, override));

	MOCK_METHOD(const std::string&, getDrmMetaData, (), (const, override));

	MOCK_METHOD(void, setDrmMetaData, (const std::string& metaData), (override));

	MOCK_METHOD(void, setDefaultKeyID, (const std::string& cencData), (override));

	MOCK_METHOD(int, getDrmCodecType, (), (const, override));

	MOCK_METHOD(uint32_t, licenseGenerateTimeout, (), (const, override));

	MOCK_METHOD(uint32_t, keyProcessTimeout, (), (const, override));

	MOCK_METHOD(void, getKey, (std::vector<uint8_t>& keyID), (const, override));

	MOCK_METHOD(void, getKeys, ((std::map<int, std::vector<uint8_t>>)& keyIDs), (const, override));

	MOCK_METHOD(const std::string&, getUuid, (), (const, override));

	MOCK_METHOD(bool, isExternalLicense, (), (const, override));

	MOCK_METHOD(void, generateLicenseRequest, (const ChallengeInfo& challengeInfo, LicenseRequest& licenseRequest), (const, override));

	MOCK_METHOD(void, transformLicenseResponse, (std::shared_ptr<DrmData> licenseResponse), (const, override));

	MOCK_METHOD(DRMMemorySystem*, getMemorySystem, (), (override));

	MOCK_METHOD(void, cancelDrmSession, (), (override));

	MOCK_METHOD(bool, canCancelDrmSession, (), (override));

	MOCK_METHOD(const std::string&, friendlyName, (), (const, override));

	MOCK_METHOD(void, setOutputProtectionFlag, (bool  bValue));

	MOCK_METHOD(bool, isDecryptClearSamplesRequired,());


};


#endif /* AAMP_MOCK_AAMP_CONFIG_H */
