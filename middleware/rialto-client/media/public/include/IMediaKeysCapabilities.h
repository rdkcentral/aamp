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

#ifndef FIREBOLT_RIALTO_I_MEDIA_KEYS_CAPABILITIES_H_
#define FIREBOLT_RIALTO_I_MEDIA_KEYS_CAPABILITIES_H_

/**
 * @file IMediaKeysCapabilities.h
 *
 * The definition of the IMediaKeysCapabilities interface.
 */

#include <memory>
#include <string>
#include <vector>

namespace firebolt::rialto
{
class IMediaKeysCapabilities;

/**
 * @brief IMediaKeysCapabilities factory class, for getting the IMediaKeysCapabilities singleton object.
 */
class IMediaKeysCapabilitiesFactory
{
public:
    IMediaKeysCapabilitiesFactory() = default;
    virtual ~IMediaKeysCapabilitiesFactory() = default;

    /**
     * @brief Gets the IMediaKeysCapabilitiesFactory instance.
     *
     * @retval the factory instance or null on error.
     */
    static std::shared_ptr<IMediaKeysCapabilitiesFactory> createFactory();

    /**
     * @brief Gets the IMediaKeysCapabilities singleton object.
     *
     * @retval the MediaKeysCapabilities instance or null on error.
     */
    virtual std::shared_ptr<IMediaKeysCapabilities> getMediaKeysCapabilities() const = 0;
};

/**
 * @brief The definition of the IMediaKeysCapabilities interface.
 *
 * This interface defines the public API of Rialto for querying EME decryption capabilities.
 * It should be implemented by both Rialto Client & Rialto Server.
 */
class IMediaKeysCapabilities
{
public:
    IMediaKeysCapabilities() = default;
    virtual ~IMediaKeysCapabilities() = default;

    IMediaKeysCapabilities(const IMediaKeysCapabilities &) = delete;
    IMediaKeysCapabilities &operator=(const IMediaKeysCapabilities &) = delete;
    IMediaKeysCapabilities(IMediaKeysCapabilities &&) = delete;
    IMediaKeysCapabilities &operator=(IMediaKeysCapabilities &&) = delete;

    /**
     * @brief Returns the EME key systems supported by Rialto
     *
     * @retval The supported key systems.
     */
    virtual std::vector<std::string> getSupportedKeySystems() = 0;

    /**
     * @brief Indicates if the specified key system is supported.
     *
     * This method should be called to ensure that the specified key
     * system is supported by Rialto.
     *
     * @param[in] keySystem : The key system.
     *
     * @retval true if supported.
     */
    virtual bool supportsKeySystem(const std::string &keySystem) = 0;

    /**
     * @brief Returns version of supported key system
     *
     * @param[in] keySystem    : The key system.
     * @param[out] version     : The supported version of the key system
     *
     * @retval true if operation was successful
     */
    virtual bool getSupportedKeySystemVersion(const std::string &keySystem, std::string &version) = 0;

    /**
     * @brief Gets support server certificate.
     *
     * Some DRMs (e.g. WideVine) use a system-wide server certificate. This method
     * gets if system has support for that certificate.
     *
     * @param[in] keySystem    : The key system.
     *
     * @retval true if server certificate is supported
     */
    virtual bool isServerCertificateSupported(const std::string &keySystem) = 0;
};

}; // namespace firebolt::rialto

#endif // FIREBOLT_RIALTO_I_MEDIA_KEYS_CAPABILITIES_H_
