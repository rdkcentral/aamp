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

#ifndef FIREBOLT_RIALTO_CLIENT_MEDIA_KEYS_CAPABILITIES_H_
#define FIREBOLT_RIALTO_CLIENT_MEDIA_KEYS_CAPABILITIES_H_

#include "IMediaKeysCapabilities.h"
#include "IMediaKeysCapabilitiesIpcFactory.h"
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace firebolt::rialto
{
/**
 * @brief IMediaKeysCapabilities factory class definition.
 */
class MediaKeysCapabilitiesFactory : public IMediaKeysCapabilitiesFactory
{
public:
    MediaKeysCapabilitiesFactory() = default;
    ~MediaKeysCapabilitiesFactory() override = default;

    /**
     * @brief Weak pointer to the singleton object.
     */
    static std::weak_ptr<IMediaKeysCapabilities> m_mediaKeysCapabilities;

    /**
     * @brief Mutex protection for creation of the MediaKeysCapabilities object.
     */
    static std::mutex m_creationMutex;

    std::shared_ptr<IMediaKeysCapabilities> getMediaKeysCapabilities() const override;
};

}; // namespace firebolt::rialto

namespace firebolt::rialto::client
{
/**
 * @brief The definition of the MediaKeysCapabilities.
 */
class MediaKeysCapabilities : public IMediaKeysCapabilities
{
public:
    /**
     * @brief The constructor.
     *
     * @param[in] mediaKeysCapabilitiesIpcFactory : The media keys capabilities ipc factory.
     */
    explicit MediaKeysCapabilities(const std::shared_ptr<IMediaKeysCapabilitiesIpcFactory> &MediaKeysCapabilitiesIpcFactory);

    /**
     * @brief Virtual destructor.
     */
    virtual ~MediaKeysCapabilities();

    std::vector<std::string> getSupportedKeySystems() override;

    bool supportsKeySystem(const std::string &keySystem) override;

    bool getSupportedKeySystemVersion(const std::string &keySystem, std::string &version) override;

    bool isServerCertificateSupported(const std::string &keySystem) override;

private:
    /**
     * @brief The media keys capabilities ipc object.
     */
    std::shared_ptr<IMediaKeysCapabilities> m_mediaKeysCapabilitiesIpc;
};

}; // namespace firebolt::rialto::client

#endif // FIREBOLT_RIALTO_CLIENT_MEDIA_KEYS_CAPABILITIES_H_
