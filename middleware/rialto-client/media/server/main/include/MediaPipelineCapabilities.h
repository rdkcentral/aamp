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

#ifndef FIREBOLT_RIALTO_SERVER_MEDIA_PIPELINE_CAPABILITIES_H_
#define FIREBOLT_RIALTO_SERVER_MEDIA_PIPELINE_CAPABILITIES_H_

#include "IGstCapabilities.h"
#include "IMediaPipelineCapabilities.h"
#include <memory>
#include <string>
#include <vector>

namespace firebolt::rialto
{
/**
 * @brief IMediaPipelineCapabilities factory class definition.
 */
class MediaPipelineCapabilitiesFactory : public IMediaPipelineCapabilitiesFactory
{
public:
    MediaPipelineCapabilitiesFactory() = default;
    ~MediaPipelineCapabilitiesFactory() override = default;

    std::unique_ptr<IMediaPipelineCapabilities> createMediaPipelineCapabilities() const override;
};

}; // namespace firebolt::rialto

namespace firebolt::rialto::server
{
/**
 * @brief The definition of the MediaPipeline.
 */
class MediaPipelineCapabilities : public IMediaPipelineCapabilities
{
public:
    /**
     * @brief The constructor.
     *
     * @param[in] gstCapabilitiesFactory : The gstreamer capabilities factory.
     */
    explicit MediaPipelineCapabilities(std::shared_ptr<IGstCapabilitiesFactory> gstCapabilitiesFactory);

    /**
     * @brief Virtual destructor.
     */
    virtual ~MediaPipelineCapabilities();

    std::vector<std::string> getSupportedMimeTypes(MediaSourceType sourceType) override;
    bool isMimeTypeSupported(const std::string &mimeType) override;
    std::vector<std::string> getSupportedProperties(MediaSourceType mediaType,
                                                    const std::vector<std::string> &propertyNames) override;
    bool isVideoMaster(bool &isVideoMaster) override;

private:
    /**
     * @brief The gstreamer capabilities.
     */
    std::unique_ptr<IGstCapabilities> m_gstCapabilities;

    /**
     * @brief The gstreamer capabilities factory object.
     */
    const std::shared_ptr<IGstCapabilitiesFactory> m_kGstCapabilitiesFactory;
};

}; // namespace firebolt::rialto::server

#endif // FIREBOLT_RIALTO_SERVER_MEDIA_PIPELINE_CAPABILITIES_H_
