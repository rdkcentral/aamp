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

#include <stdexcept>

#include "IMediaKeysCapabilitiesIpcFactory.h"
#include "MediaKeysCapabilities.h"
#include "RialtoClientLogging.h"

namespace firebolt::rialto
{
std::weak_ptr<IMediaKeysCapabilities> MediaKeysCapabilitiesFactory::m_mediaKeysCapabilities;
std::mutex MediaKeysCapabilitiesFactory::m_creationMutex;

std::shared_ptr<IMediaKeysCapabilitiesFactory> IMediaKeysCapabilitiesFactory::createFactory()
{
    std::shared_ptr<IMediaKeysCapabilitiesFactory> factory;

    try
    {
        factory = std::make_shared<MediaKeysCapabilitiesFactory>();
    }
    catch (const std::exception &e)
    {
        RIALTO_CLIENT_LOG_ERROR("Failed to create the media keys capabilities factory, reason: %s", e.what());
    }

    return factory;
}

std::shared_ptr<IMediaKeysCapabilities> MediaKeysCapabilitiesFactory::getMediaKeysCapabilities() const
{
    std::lock_guard<std::mutex> lock{m_creationMutex};

    std::shared_ptr<IMediaKeysCapabilities> mediaKeysCapabilities =
        MediaKeysCapabilitiesFactory::m_mediaKeysCapabilities.lock();

    if (!mediaKeysCapabilities)
    {
        try
        {
            mediaKeysCapabilities = std::make_shared<client::MediaKeysCapabilities>(
                client::IMediaKeysCapabilitiesIpcFactory::createFactory());
        }
        catch (const std::exception &e)
        {
            RIALTO_CLIENT_LOG_ERROR("Failed to create the media keys capabilities, reason: %s", e.what());
        }

        MediaKeysCapabilitiesFactory::m_mediaKeysCapabilities = mediaKeysCapabilities;
    }

    return mediaKeysCapabilities;
}

}; // namespace firebolt::rialto

namespace firebolt::rialto::client
{
MediaKeysCapabilities::MediaKeysCapabilities(
    const std::shared_ptr<IMediaKeysCapabilitiesIpcFactory> &MediaKeysCapabilitiesIpcFactory)
{
    RIALTO_CLIENT_LOG_DEBUG("entry:");

    m_mediaKeysCapabilitiesIpc = MediaKeysCapabilitiesIpcFactory->getMediaKeysCapabilitiesIpc();
    if (!m_mediaKeysCapabilitiesIpc)
    {
        throw std::runtime_error("Media keys capabilities ipc could not be created");
    }
}

MediaKeysCapabilities::~MediaKeysCapabilities()
{
    RIALTO_CLIENT_LOG_DEBUG("entry:");

    m_mediaKeysCapabilitiesIpc.reset();
}

std::vector<std::string> MediaKeysCapabilities::getSupportedKeySystems()
{
    RIALTO_CLIENT_LOG_DEBUG("entry:");

    return m_mediaKeysCapabilitiesIpc->getSupportedKeySystems();
}

bool MediaKeysCapabilities::supportsKeySystem(const std::string &keySystem)
{
    RIALTO_CLIENT_LOG_DEBUG("entry:");

    return m_mediaKeysCapabilitiesIpc->supportsKeySystem(keySystem);
}

bool MediaKeysCapabilities::getSupportedKeySystemVersion(const std::string &keySystem, std::string &version)
{
    RIALTO_CLIENT_LOG_DEBUG("entry:");

    return m_mediaKeysCapabilitiesIpc->getSupportedKeySystemVersion(keySystem, version);
}

bool MediaKeysCapabilities::isServerCertificateSupported(const std::string &keySystem)
{
    RIALTO_CLIENT_LOG_DEBUG("entry:");

    return m_mediaKeysCapabilitiesIpc->isServerCertificateSupported(keySystem);
}

}; // namespace firebolt::rialto::client
