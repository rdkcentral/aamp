/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2023 Sky UK
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

#include "MediaKeysCapabilitiesBackend.h"

MediaKeysCapabilitiesBackend &MediaKeysCapabilitiesBackend::instance()
{
    static MediaKeysCapabilitiesBackend mkcBackend;
    return mkcBackend;
}

std::vector<std::string> MediaKeysCapabilitiesBackend::getSupportedKeySystems()
{
    std::vector<std::string> result{};
    if (m_mediaKeysCapabilities)
    {
        result = m_mediaKeysCapabilities->getSupportedKeySystems();
    }
    return result;
}

OpenCDMError MediaKeysCapabilitiesBackend::supportsKeySystem(const std::string &keySystem)
{
    OpenCDMError result{ERROR_FAIL};
    if (m_mediaKeysCapabilities)
    {
        result = ERROR_KEYSYSTEM_NOT_SUPPORTED;
        if (m_mediaKeysCapabilities->supportsKeySystem(keySystem))
        {
            result = ERROR_NONE;
        }
    }
    return result;
}

bool MediaKeysCapabilitiesBackend::getSupportedKeySystemVersion(const std::string &keySystem, std::string &version)
{
    bool result{false};
    if (m_mediaKeysCapabilities)
    {
        result = m_mediaKeysCapabilities->getSupportedKeySystemVersion(keySystem, version);
    }
    return result;
}

bool MediaKeysCapabilitiesBackend::isServerCertificateSupported(const std::string &keySystem)
{
    bool result{false};
    if (m_mediaKeysCapabilities)
    {
        result = m_mediaKeysCapabilities->isServerCertificateSupported(keySystem);
    }
    return result;
}

MediaKeysCapabilitiesBackend::MediaKeysCapabilitiesBackend()
{
    std::shared_ptr<firebolt::rialto::IMediaKeysCapabilitiesFactory> factory =
        firebolt::rialto::IMediaKeysCapabilitiesFactory::createFactory();
    if (factory)
    {
        m_mediaKeysCapabilities = factory->getMediaKeysCapabilities();
    }
}

MediaKeysCapabilitiesBackend::~MediaKeysCapabilitiesBackend()
{
    m_mediaKeysCapabilities.reset();
}
