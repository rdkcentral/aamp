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

#include "MediaCommon.h"
#include "MediaKeysCapabilities.h"
#include "MediaKeysCommon.h"
#include "RialtoServerLogging.h"

namespace
{
/**
 * @brief Coverts MediaKeyErrorStatus to string.
 */
const char *toString(const firebolt::rialto::MediaKeyErrorStatus &status)
{
    switch (status)
    {
    case firebolt::rialto::MediaKeyErrorStatus::OK:
        return "OK";
    case firebolt::rialto::MediaKeyErrorStatus::FAIL:
        return "FAIL";
    case firebolt::rialto::MediaKeyErrorStatus::BAD_SESSION_ID:
        return "BAD_SESSION_ID";
    case firebolt::rialto::MediaKeyErrorStatus::INTERFACE_NOT_IMPLEMENTED:
        return "INTERFACE_NOT_IMPLEMENTED";
    case firebolt::rialto::MediaKeyErrorStatus::BUFFER_TOO_SMALL:
        return "BUFFER_TOO_SMALL";
    case firebolt::rialto::MediaKeyErrorStatus::NOT_SUPPORTED:
        return "NOT_SUPPORTED";
    case firebolt::rialto::MediaKeyErrorStatus::INVALID_STATE:
        return "INVALID_STATE";
    }
    return "Unknown";
}
} // namespace

namespace firebolt::rialto
{
std::shared_ptr<IMediaKeysCapabilitiesFactory> IMediaKeysCapabilitiesFactory::createFactory()
{
    std::shared_ptr<IMediaKeysCapabilitiesFactory> factory;

    try
    {
        factory = std::make_shared<MediaKeysCapabilitiesFactory>();
    }
    catch (const std::exception &e)
    {
        RIALTO_SERVER_LOG_ERROR("Failed to create the media keys capabilities factory, reason: %s", e.what());
    }

    return factory;
}

std::shared_ptr<IMediaKeysCapabilities> MediaKeysCapabilitiesFactory::getMediaKeysCapabilities() const
{
    std::shared_ptr<IMediaKeysCapabilities> mediaKeysCapabilities;
    try
    {
        mediaKeysCapabilities =
            std::make_shared<server::MediaKeysCapabilities>(wrappers::IOcdmFactory::createFactory(),
                                                            wrappers::IOcdmSystemFactory::createFactory());
    }
    catch (const std::exception &e)
    {
        RIALTO_SERVER_LOG_ERROR("Failed to create the media keys capabilities, reason: %s", e.what());
    }

    return mediaKeysCapabilities;
}

}; // namespace firebolt::rialto

namespace firebolt::rialto::server
{
MediaKeysCapabilities::MediaKeysCapabilities(std::shared_ptr<firebolt::rialto::wrappers::IOcdmFactory> ocdmFactory,
                                             std::shared_ptr<firebolt::rialto::wrappers::IOcdmSystemFactory> ocdmSystemFactory)
    : m_ocdmSystemFactory{ocdmSystemFactory}
{
    RIALTO_SERVER_LOG_DEBUG("entry:");

    if (!ocdmFactory)
    {
        throw std::runtime_error("ocdmFactory invalid");
    }

    m_ocdm = ocdmFactory->getOcdm();
    if (!m_ocdm)
    {
        throw std::runtime_error("Ocdm could not be fetched");
    }
}

MediaKeysCapabilities::~MediaKeysCapabilities()
{
    RIALTO_SERVER_LOG_DEBUG("entry:");
}

std::vector<std::string> MediaKeysCapabilities::getSupportedKeySystems()
{
    std::vector<std::string> supportedKeySystemVector;
    for (auto it = kSupportedKeySystems.begin(); it != kSupportedKeySystems.end(); it++)
    {
        MediaKeyErrorStatus status = m_ocdm->isTypeSupported(*it);
        if (MediaKeyErrorStatus::OK == status)
        {
            supportedKeySystemVector.push_back(*it);
        }
    }

    return supportedKeySystemVector;
}

bool MediaKeysCapabilities::supportsKeySystem(const std::string &keySystem)
{
    MediaKeyErrorStatus status = m_ocdm->isTypeSupported(keySystem);
    if (MediaKeyErrorStatus::OK != status)
    {
        return false;
    }
    return true;
}

bool MediaKeysCapabilities::getSupportedKeySystemVersion(const std::string &keySystem, std::string &version)
{
    std::shared_ptr<firebolt::rialto::wrappers::IOcdmSystem> ocdmSystem =
        m_ocdmSystemFactory->createOcdmSystem(keySystem);
    if (!ocdmSystem)
    {
        RIALTO_SERVER_LOG_ERROR("Failed to create the ocdm system object");
        version = "";
        return false;
    }

    MediaKeyErrorStatus status = ocdmSystem->getVersion(version);
    if (MediaKeyErrorStatus::OK != status)
    {
        RIALTO_SERVER_LOG_ERROR("Ocdm getVersion failed with status %s", toString(status));
        version = "";
        return false;
    }
    return true;
}

bool MediaKeysCapabilities::isServerCertificateSupported(const std::string &keySystem)
{
    std::shared_ptr<firebolt::rialto::wrappers::IOcdmSystem> ocdmSystem =
        m_ocdmSystemFactory->createOcdmSystem(keySystem);
    if (!ocdmSystem)
    {
        RIALTO_SERVER_LOG_ERROR("Failed to create the ocdm system object");
        return false;
    }
    return ocdmSystem->supportsServerCertificate();
}

}; // namespace firebolt::rialto::server
