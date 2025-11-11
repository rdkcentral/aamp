/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2022 
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

#ifndef FIREBOLT_RIALTO_CLIENT_MEDIA_KEYS_H_
#define FIREBOLT_RIALTO_CLIENT_MEDIA_KEYS_H_

#include "IMediaKeys.h"
#include "IMediaKeysIpcFactory.h"
#include <memory>
#include <string>
#include <vector>

namespace firebolt::rialto
{
/**
 * @brief IMediaKeys factory class definition.
 */
class MediaKeysFactory : public IMediaKeysFactory
{
public:
    MediaKeysFactory() = default;
    ~MediaKeysFactory() override = default;

    std::unique_ptr<IMediaKeys> createMediaKeys(const std::string &keySystem) const override;

    /**
     * @brief IMediaKeys factory method with factory parameters for mock injection.
     *
     * @param[in] keySystem           : The key system for which to create a Media Keys instance
     * @param[in] mediaKeysIpcFactory : This was added for the test environment where a mock object needs to be passed in.
     *
     * @retval the new media keys instance or null on error.
     */
    std::unique_ptr<IMediaKeys>
    createMediaKeys(const std::string &keySystem,
                    std::weak_ptr<firebolt::rialto::client::IMediaKeysIpcFactory> mediaKeysIpcFactory) const;
};

}; // namespace firebolt::rialto

namespace firebolt::rialto::client
{
/**
 * @brief The definition of the MediaKeys.
 */
class MediaKeys : public IMediaKeys
{
public:
    /**
     * @brief The constructor.
     *
     * @param[in] keySystem           : The key system for which to create a Media Keys instance.
     * @param[in] mediaKeysIpcFactory : The media keys ipc factory.
     */
    explicit MediaKeys(const std::string &keySystem, const std::shared_ptr<IMediaKeysIpcFactory> &mediaKeysIpcFactory);

    /**
     * @brief Virtual destructor.
     */
    virtual ~MediaKeys();

    MediaKeyErrorStatus selectKeyId(int32_t keySessionId, const std::vector<uint8_t> &keyId) override;

    bool containsKey(int32_t keySessionId, const std::vector<uint8_t> &keyId) override;

    MediaKeyErrorStatus createKeySession(KeySessionType sessionType, std::weak_ptr<IMediaKeysClient> client, bool isLDL,
                                         int32_t &keySessionId) override;

    MediaKeyErrorStatus generateRequest(int32_t keySessionId, InitDataType initDataType,
                                        const std::vector<uint8_t> &initData) override;

    MediaKeyErrorStatus loadSession(int32_t keySessionId) override;

    MediaKeyErrorStatus updateSession(int32_t keySessionId, const std::vector<uint8_t> &responseData) override;

    MediaKeyErrorStatus setDrmHeader(int32_t keySessionId, const std::vector<uint8_t> &requestData) override;

    MediaKeyErrorStatus closeKeySession(int32_t keySessionId) override;

    MediaKeyErrorStatus removeKeySession(int32_t keySessionId) override;

    MediaKeyErrorStatus deleteDrmStore() override;

    MediaKeyErrorStatus deleteKeyStore() override;

    MediaKeyErrorStatus getDrmStoreHash(std::vector<unsigned char> &drmStoreHash) override;

    MediaKeyErrorStatus getKeyStoreHash(std::vector<unsigned char> &keyStoreHash) override;

    MediaKeyErrorStatus getLdlSessionsLimit(uint32_t &ldlLimit) override;

    MediaKeyErrorStatus getLastDrmError(int32_t keySessionId, uint32_t &errorCode) override;

    MediaKeyErrorStatus getDrmTime(uint64_t &drmTime) override;

    MediaKeyErrorStatus getCdmKeySessionId(int32_t keySessionId, std::string &cdmKeySessionId) override;

    MediaKeyErrorStatus releaseKeySession(int32_t keySessionId) override;

    MediaKeyErrorStatus getMetricSystemData(std::vector<uint8_t> &buffer) override;

private:
    /**
     * @brief The media keys ipc object.
     */
    std::unique_ptr<IMediaKeys> m_mediaKeysIpc;

    /**
     * @brief The key system.
     */
    std::string m_keySystem;
};

}; // namespace firebolt::rialto::client

#endif // FIREBOLT_RIALTO_CLIENT_MEDIA_KEYS_H_
