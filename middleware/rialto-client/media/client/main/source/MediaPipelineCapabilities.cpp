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

#include "IMediaPipelineCapabilitiesIpcFactory.h"
#include "MediaPipelineCapabilities.h"
#include "RialtoClientLogging.h"

namespace firebolt::rialto
{
std::shared_ptr<IMediaPipelineCapabilitiesFactory> IMediaPipelineCapabilitiesFactory::createFactory()
{
    std::shared_ptr<IMediaPipelineCapabilitiesFactory> factory;

    try
    {
        factory = std::make_shared<MediaPipelineCapabilitiesFactory>();
    }
    catch (const std::exception &e)
    {
        RIALTO_CLIENT_LOG_ERROR("Failed to create the media pipeline capabilities factory, reason: %s", e.what());
    }

    return factory;
}

std::unique_ptr<IMediaPipelineCapabilities> MediaPipelineCapabilitiesFactory::createMediaPipelineCapabilities() const
{
    std::unique_ptr<IMediaPipelineCapabilities> mediaPipelineCapabilities;
    try
    {
        mediaPipelineCapabilities = std::make_unique<client::MediaPipelineCapabilities>(
            client::IMediaPipelineCapabilitiesIpcFactory::createFactory());
    }
    catch (const std::exception &e)
    {
        RIALTO_CLIENT_LOG_ERROR("Failed to create the media pipeline capabilities, reason: %s", e.what());
    }

    return mediaPipelineCapabilities;
}

}; // namespace firebolt::rialto

namespace firebolt::rialto::client
{
MediaPipelineCapabilities::MediaPipelineCapabilities(
    const std::shared_ptr<IMediaPipelineCapabilitiesIpcFactory> &MediaPipelineCapabilitiesIpcFactory)
{
    RIALTO_CLIENT_LOG_DEBUG("entry:");

    m_mediaPipelineCapabilitiesIpc = MediaPipelineCapabilitiesIpcFactory->createMediaPipelineCapabilitiesIpc();
    if (!m_mediaPipelineCapabilitiesIpc)
    {
        throw std::runtime_error("Media pipeline capabilities ipc could not be created");
    }
}

MediaPipelineCapabilities::~MediaPipelineCapabilities()
{
    RIALTO_CLIENT_LOG_DEBUG("entry:");

    m_mediaPipelineCapabilitiesIpc.reset();
}

std::vector<std::string> MediaPipelineCapabilities::getSupportedMimeTypes(MediaSourceType sourceType)
{
    RIALTO_CLIENT_LOG_DEBUG("entry:");

    return m_mediaPipelineCapabilitiesIpc->getSupportedMimeTypes(sourceType);
}

bool MediaPipelineCapabilities::isMimeTypeSupported(const std::string &mimeType)
{
    RIALTO_CLIENT_LOG_DEBUG("entry:");

    return m_mediaPipelineCapabilitiesIpc->isMimeTypeSupported(mimeType);
}

std::vector<std::string> MediaPipelineCapabilities::getSupportedProperties(MediaSourceType mediaType,
                                                                           const std::vector<std::string> &propertyNames)
{
    RIALTO_CLIENT_LOG_DEBUG("entry:");

    return m_mediaPipelineCapabilitiesIpc->getSupportedProperties(mediaType, propertyNames);
}

bool MediaPipelineCapabilities::isVideoMaster(bool &isVideoMaster)
{
    RIALTO_CLIENT_LOG_DEBUG("entry:");

    return m_mediaPipelineCapabilitiesIpc->isVideoMaster(isVideoMaster);
}
}; // namespace firebolt::rialto::client
