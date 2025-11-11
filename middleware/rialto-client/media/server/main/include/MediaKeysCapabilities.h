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

#ifndef FIREBOLT_RIALTO_SERVER_MEDIA_KEYS_CAPABILITIES_H_
#define FIREBOLT_RIALTO_SERVER_MEDIA_KEYS_CAPABILITIES_H_

#include "IMediaKeysCapabilities.h"
#include "IOcdm.h"
#include "IOcdmSystem.h"
#include <memory>
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

    std::shared_ptr<IMediaKeysCapabilities> getMediaKeysCapabilities() const override;
};

}; // namespace firebolt::rialto

namespace firebolt::rialto::server
{
/**
 * @brief The definition of the MediaKeys.
 */
class MediaKeysCapabilities : public IMediaKeysCapabilities
{
public:
    /**
     * @brief The constructor.
     *
     * @param[in] ocdmFactory : The ocdm factory.
     * @param[in] ocdmSystemFactory : The ocdmSystem factory.
     */
    MediaKeysCapabilities(std::shared_ptr<firebolt::rialto::wrappers::IOcdmFactory> ocdmFactory,
                          std::shared_ptr<firebolt::rialto::wrappers::IOcdmSystemFactory> ocdmSystemFactory);

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
     * @brief The IOcdm instance.
     */
    std::shared_ptr<firebolt::rialto::wrappers::IOcdm> m_ocdm;

    /**
     * @brief The IOcdmSystem factory.
     */
    std::shared_ptr<firebolt::rialto::wrappers::IOcdmSystemFactory> m_ocdmSystemFactory;
};

}; // namespace firebolt::rialto::server

#endif // FIREBOLT_RIALTO_SERVER_MEDIA_KEYS_CAPABILITIES_H_
