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

#ifndef PLAYER_MOCK_DRM_SESSION_H
#define PLAYER_MOCK_DRM_SESSION_H

#include <gmock/gmock.h>
#include "DrmSession.h"

/**
 * @class MockDrmSession
 * @brief Mock implementation of DrmSession for unit testing
 * 
 * This mock allows testing DRM session behavior without requiring
 * actual OCDM system interactions.
 */
class MockDrmSession : public DrmSession
{
public:
	MockDrmSession() : DrmSession("mock-key-system") {}
	virtual ~MockDrmSession() = default;

	MOCK_METHOD(void, generateDRMSession, (const uint8_t *f_pbInitData, uint32_t f_cbInitData, std::string &customData), (override));
	MOCK_METHOD(DrmData*, generateKeyRequest, (string& destinationURL, uint32_t timeout), (override));
	MOCK_METHOD(int, processDRMKey, (DrmData* key, uint32_t timeout), (override));
	MOCK_METHOD(KeyState, getState, (), (override));
	MOCK_METHOD(void, clearDecryptContext, (), (override));
	MOCK_METHOD(const std::vector<std::vector<uint8_t>>&, getUsableKeys, (), (const, override));
};

#endif /* PLAYER_MOCK_DRM_SESSION_H */
