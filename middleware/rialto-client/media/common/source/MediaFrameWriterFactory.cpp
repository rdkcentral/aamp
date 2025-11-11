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

#include "MediaFrameWriterFactory.h"
#include "MediaFrameWriterV1.h"
#include "MediaFrameWriterV2.h"
#include "RialtoCommonLogging.h"
#include <algorithm>
#include <string>
#include <unistd.h>

namespace
{
constexpr int kLatestMetadataVersion{2};
const char *kMetadataEnvVariableName{"RIALTO_METADATA_VERSION"};
} // namespace

namespace firebolt::rialto::common
{
std::shared_ptr<IMediaFrameWriterFactory> IMediaFrameWriterFactory::getFactory()
try
{
    return std::make_shared<MediaFrameWriterFactory>();
}
catch (const std::exception &e)
{
    RIALTO_COMMON_LOG_ERROR("Failed to create the media frame writer factory, reason: %s", e.what());
    return nullptr;
}

MediaFrameWriterFactory::MediaFrameWriterFactory() : m_metadataVersion{kLatestMetadataVersion}
{
    const char *kEnvVar = getenv(kMetadataEnvVariableName);
    if (!kEnvVar)
    {
        return;
    }
    std::string envVarStr{kEnvVar};
    try
    {
        m_metadataVersion = std::stoi(envVarStr);
    }
    catch (const std::exception &e)
    {
    }
    if (m_metadataVersion > kLatestMetadataVersion)
    {
        m_metadataVersion = kLatestMetadataVersion;
    }
}

std::unique_ptr<IMediaFrameWriter>
MediaFrameWriterFactory::createFrameWriter(uint8_t *shmBuffer, const std::shared_ptr<MediaPlayerShmInfo> &shmInfo)
try
{
    if (1 == m_metadataVersion)
    {
        return std::make_unique<MediaFrameWriterV1>(shmBuffer, shmInfo);
    }
    return std::make_unique<MediaFrameWriterV2>(shmBuffer, shmInfo);
}
catch (const std::exception &e)
{
    RIALTO_COMMON_LOG_ERROR("Failed to create the frame writer, reason: %s", e.what());
    return nullptr;
}
} // namespace firebolt::rialto::common
