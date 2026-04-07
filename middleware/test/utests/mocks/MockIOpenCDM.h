/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2026 RDK Management
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

#ifndef MOCK_I_OPEN_CDM_H
#define MOCK_I_OPEN_CDM_H

#include "IOpenCDM.h"
#include <gmock/gmock.h>
#include <memory>

/**
 * @class MockIOpenCDMSession
 * @brief GMock implementation of IOpenCDMSession for unit tests.
 */
class MockIOpenCDMSession : public IOpenCDMSession
{
public:
	MOCK_METHOD(KeyStatus, getStatus,
	            (const uint8_t* keyId, uint8_t keyIdSize), (override));

	MOCK_METHOD(OpenCDMError, update,
	            (const uint8_t* keyMessage, uint16_t keyMessageLength), (override));

	MOCK_METHOD(OpenCDMError, close, (), (override));

	MOCK_METHOD(OpenCDMError, destruct, (), (override));

	MOCK_METHOD(bool, hasGstDecryptBuffer, (), (override));

	MOCK_METHOD(int32_t, getMediaKeySessionId, (), (override));

	MOCK_METHOD(OpenCDMError, decrypt,
	            (uint8_t* data, uint32_t dataSize,
	             EncryptionScheme encScheme, EncryptionPattern pattern,
	             const uint8_t* iv, uint16_t ivSize,
	             const uint8_t* keyId, uint16_t keyIdSize,
	             uint32_t initWithLast15),
	            (override));

	MOCK_METHOD(OpenCDMError, decryptGst,
	            (GstBuffer* buffer, GstCaps* caps), (override));

	MOCK_METHOD(OpenCDMError, decryptGstLegacy,
	            (GstBuffer* buffer, GstBuffer* subSamples,
	             uint32_t subSampleCount, GstBuffer* iv,
	             GstBuffer* keyId, uint32_t initWithLast15),
	            (override));
};

/**
 * @class MockIOpenCDM
 * @brief GMock implementation of IOpenCDM for unit tests.
 *
 * Usage — inject into adapter constructor, then configure constructSession()
 * to return a MockIOpenCDMSession:
 *
 * @code
 *   auto* rawSession = new NiceMock<MockIOpenCDMSession>();
 *   auto  mockOcdm   = std::make_unique<NiceMock<MockIOpenCDM>>();
 *   ON_CALL(*mockOcdm, constructSession(_, _, _, _, _, _, _, _))
 *       .WillByDefault(testing::Invoke([rawSession](auto&&...) {
 *           return std::unique_ptr<IOpenCDMSession>(rawSession);
 *       }));
 *   auto* adapter = new OCDMBasicSessionAdapter(helper, std::move(mockOcdm), nullptr);
 * @endcode
 */
class MockIOpenCDM : public IOpenCDM
{
public:
	MOCK_METHOD(std::unique_ptr<IOpenCDMSession>, constructSession,
	            (const std::string& keySystem,
	             LicenseType        licenseType,
	             const std::string& initDataType,
	             const uint8_t*     initData,
	             uint32_t           initDataSize,
	             const uint8_t*     customData,
	             uint16_t           customDataSize,
	             const OpenCDMSessionCallbackSet& callbacks),
	            (override));
};

#endif // MOCK_I_OPEN_CDM_H
