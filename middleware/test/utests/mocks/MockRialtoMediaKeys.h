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

#ifndef MOCK_RIALTO_MEDIA_KEYS_H
#define MOCK_RIALTO_MEDIA_KEYS_H

#include <gmock/gmock.h>
#include "IMediaKeys.h"

class MockMediaKeys : public firebolt::rialto::IMediaKeys
{
public:
	MOCK_METHOD(firebolt::rialto::MediaKeyErrorStatus, createKeySession,
	            (firebolt::rialto::KeySessionType sessionType,
	             std::weak_ptr<firebolt::rialto::IMediaKeysClient> client,
	             int32_t& keySessionId),
	            (override));

	MOCK_METHOD(firebolt::rialto::MediaKeyErrorStatus, generateRequest,
	            (int32_t keySessionId,
	             firebolt::rialto::InitDataType initDataType,
	             const std::vector<uint8_t>& initData,
	             const firebolt::rialto::LimitedDurationLicense& ldlState),
	            (override));

	MOCK_METHOD(firebolt::rialto::MediaKeyErrorStatus, updateSession,
	            (int32_t keySessionId, const std::vector<uint8_t>& responseData),
	            (override));

	MOCK_METHOD(firebolt::rialto::MediaKeyErrorStatus, closeKeySession,
	            (int32_t keySessionId),
	            (override));

	MOCK_METHOD(firebolt::rialto::MediaKeyErrorStatus, releaseKeySession,
	            (int32_t keySessionId),
	            (override));

	MOCK_METHOD(firebolt::rialto::MediaKeyErrorStatus, selectKeyId,
	            (int32_t keySessionId, const std::vector<uint8_t>& keyId),
	            (override));

	MOCK_METHOD(bool, containsKey,
	            (int32_t keySessionId, const std::vector<uint8_t>& keyId),
	            (override));

	MOCK_METHOD(firebolt::rialto::MediaKeyErrorStatus, loadSession,
	            (int32_t keySessionId),
	            (override));

	MOCK_METHOD(firebolt::rialto::MediaKeyErrorStatus, setDrmHeader,
	            (int32_t keySessionId, const std::vector<uint8_t>& requestData),
	            (override));

	MOCK_METHOD(firebolt::rialto::MediaKeyErrorStatus, removeKeySession,
	            (int32_t keySessionId),
	            (override));

	MOCK_METHOD(firebolt::rialto::MediaKeyErrorStatus, deleteDrmStore, (), (override));

	MOCK_METHOD(firebolt::rialto::MediaKeyErrorStatus, deleteKeyStore, (), (override));

	MOCK_METHOD(firebolt::rialto::MediaKeyErrorStatus, getDrmStoreHash,
	            (std::vector<unsigned char>& drmStoreHash),
	            (override));

	MOCK_METHOD(firebolt::rialto::MediaKeyErrorStatus, getKeyStoreHash,
	            (std::vector<unsigned char>& keyStoreHash),
	            (override));

	MOCK_METHOD(firebolt::rialto::MediaKeyErrorStatus, getLdlSessionsLimit,
	            (uint32_t& ldlLimit),
	            (override));

	MOCK_METHOD(firebolt::rialto::MediaKeyErrorStatus, getLastDrmError,
	            (int32_t keySessionId, uint32_t& errorCode),
	            (override));

	MOCK_METHOD(firebolt::rialto::MediaKeyErrorStatus, getDrmTime,
	            (uint64_t& drmTime),
	            (override));

	MOCK_METHOD(firebolt::rialto::MediaKeyErrorStatus, getCdmKeySessionId,
	            (int32_t keySessionId, std::string& cdmKeySessionId),
	            (override));
};

class MockMediaKeysFactory : public firebolt::rialto::IMediaKeysFactory
{
public:
	MOCK_METHOD(std::unique_ptr<firebolt::rialto::IMediaKeys>, createMediaKeys,
	            (const std::string& keySystem),
	            (const, override));
};

#endif // MOCK_RIALTO_MEDIA_KEYS_H
