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

/**
 * @file IMediaKeys.h
 * @brief Stub Rialto IMediaKeys interface for unit tests.
 */

#ifndef FIREBOLT_RIALTO_I_MEDIA_KEYS_H_
#define FIREBOLT_RIALTO_I_MEDIA_KEYS_H_

#include "IMediaKeysClient.h"
#include "MediaCommon.h"

#include <memory>
#include <string>
#include <vector>

namespace firebolt::rialto
{

class IMediaKeys;

class IMediaKeysFactory
{
public:
	IMediaKeysFactory() = default;
	virtual ~IMediaKeysFactory() = default;

	static std::shared_ptr<IMediaKeysFactory> createFactory();

	virtual std::unique_ptr<IMediaKeys> createMediaKeys(const std::string& keySystem) const = 0;
};

class IMediaKeys
{
public:
	IMediaKeys() = default;
	virtual ~IMediaKeys() = default;

	IMediaKeys(const IMediaKeys&) = delete;
	IMediaKeys& operator=(const IMediaKeys&) = delete;

	virtual MediaKeyErrorStatus createKeySession(KeySessionType sessionType,
	                                             std::weak_ptr<IMediaKeysClient> client,
	                                             int32_t& keySessionId) = 0;

	virtual MediaKeyErrorStatus generateRequest(int32_t keySessionId,
	                                            InitDataType initDataType,
	                                            const std::vector<uint8_t>& initData,
	                                            const LimitedDurationLicense& ldlState = LimitedDurationLicense::NOT_SPECIFIED) = 0;

	virtual MediaKeyErrorStatus updateSession(int32_t keySessionId,
	                                          const std::vector<uint8_t>& responseData) = 0;

	virtual MediaKeyErrorStatus closeKeySession(int32_t keySessionId) = 0;

	virtual MediaKeyErrorStatus releaseKeySession(int32_t keySessionId) = 0;

	virtual MediaKeyErrorStatus selectKeyId(int32_t keySessionId,
	                                        const std::vector<uint8_t>& keyId) = 0;

	virtual bool containsKey(int32_t keySessionId,
	                         const std::vector<uint8_t>& keyId) = 0;

	virtual MediaKeyErrorStatus loadSession(int32_t keySessionId) = 0;

	virtual MediaKeyErrorStatus setDrmHeader(int32_t keySessionId,
	                                         const std::vector<uint8_t>& requestData) = 0;

	virtual MediaKeyErrorStatus removeKeySession(int32_t keySessionId) = 0;

	virtual MediaKeyErrorStatus deleteDrmStore() = 0;

	virtual MediaKeyErrorStatus deleteKeyStore() = 0;

	virtual MediaKeyErrorStatus getDrmStoreHash(std::vector<unsigned char>& drmStoreHash) = 0;

	virtual MediaKeyErrorStatus getKeyStoreHash(std::vector<unsigned char>& keyStoreHash) = 0;

	virtual MediaKeyErrorStatus getLdlSessionsLimit(uint32_t& ldlLimit) = 0;

	virtual MediaKeyErrorStatus getLastDrmError(int32_t keySessionId, uint32_t& errorCode) = 0;

	virtual MediaKeyErrorStatus getDrmTime(uint64_t& drmTime) = 0;

	virtual MediaKeyErrorStatus getCdmKeySessionId(int32_t keySessionId, std::string& cdmKeySessionId) = 0;
};

} // namespace firebolt::rialto

#endif // FIREBOLT_RIALTO_I_MEDIA_KEYS_H_
