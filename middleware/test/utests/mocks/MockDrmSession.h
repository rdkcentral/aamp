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

#ifndef MOCK_DRMSession_H
#define MOCK_DRMSession_H

#include "DrmSession.h"
#include <gmock/gmock.h>

class MockDrmSession : public DrmSession {
public:
	MockDrmSession(const std::string& keySystem = PLAYREADY_KEY_SYSTEM_STRING)
		: DrmSession(keySystem) {}

	MOCK_METHOD(void, generateDRMSession, (const uint8_t *f_pbInitData, uint32_t f_cbInitData, std::string &customData), (override));
	MOCK_METHOD(DrmData*, generateKeyRequest, (std::string& destinationURL, uint32_t timeout), (override));
	MOCK_METHOD(int, processDRMKey, (DrmData* key, uint32_t timeout), (override));
	MOCK_METHOD(int, decrypt, (GstBuffer* keyIDBuffer, GstBuffer* ivBuffer, GstBuffer* buffer, unsigned subSampleCount, GstBuffer* subSamplesBuffer, GstCaps* caps), (override));
	MOCK_METHOD(int, decrypt, (const uint8_t *f_pbIV, uint32_t f_cbIV, const uint8_t *payloadData, uint32_t payloadDataSize, uint8_t **ppOpaqueData), (override));
	MOCK_METHOD(KeyState, getState, (), (override));
	MOCK_METHOD(void, clearDecryptContext, (), (override));
#if defined(USE_OPENCDM_ADAPTER)
	MOCK_METHOD(void, setKeyId, (const std::vector<uint8_t>& keyId), (override));
#endif
};
extern MockDrmSession *g_mockDrmSession;

#endif // MOCK_DRMSession_H
