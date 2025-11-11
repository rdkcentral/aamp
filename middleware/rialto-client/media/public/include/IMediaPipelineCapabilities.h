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

#ifndef FIREBOLT_RIALTO_I_MEDIA_PIPELINE_CAPABILITIES_H_
#define FIREBOLT_RIALTO_I_MEDIA_PIPELINE_CAPABILITIES_H_

/**
 * @file IMediaPipelineCapabilities.h
 *
 * The definition of the IMediaPipelineCapabilities interface.
 */

#include "MediaCommon.h"
#include <memory>
#include <string>
#include <vector>

namespace firebolt::rialto
{
class IMediaPipelineCapabilities;

/**
 * @brief IMediaPipelineCapabilities factory class, for getting the IMediaPipelineCapabilities object.
 */
class IMediaPipelineCapabilitiesFactory
{
public:
    IMediaPipelineCapabilitiesFactory() = default;
    virtual ~IMediaPipelineCapabilitiesFactory() = default;

    /**
     * @brief Gets the IMediaPipelineCapabilitiesFactory instance.
     *
     * @retval the factory instance or null on error.
     */
    static std::shared_ptr<IMediaPipelineCapabilitiesFactory> createFactory();

    /**
     * @brief Creates the IMediaPipelineCapabilities object.
     *
     * @retval the MediaPipelineCapabilities instance or null on error.
     */
    virtual std::unique_ptr<IMediaPipelineCapabilities> createMediaPipelineCapabilities() const = 0;
};

/**
 * @brief The definition of the IMediaPipelineCapabilities interface.
 *
 * This interface defines the public API of Rialto for querying EME decryption capabilities.
 * It should be implemented by both Rialto Client & Rialto Server.
 */
class IMediaPipelineCapabilities
{
public:
    IMediaPipelineCapabilities() = default;
    virtual ~IMediaPipelineCapabilities() = default;

    IMediaPipelineCapabilities(const IMediaPipelineCapabilities &) = delete;
    IMediaPipelineCapabilities &operator=(const IMediaPipelineCapabilities &) = delete;
    IMediaPipelineCapabilities(IMediaPipelineCapabilities &&) = delete;
    IMediaPipelineCapabilities &operator=(IMediaPipelineCapabilities &&) = delete;

    /**
     * @brief Returns the MSE mime types supported by Rialto for \a sourceType
     *
     * @param[in] sourceType : source type
     *
     * @retval The supported mime types.
     */
    virtual std::vector<std::string> getSupportedMimeTypes(MediaSourceType sourceType) = 0;

    /**
     * @brief Indicates if the specified mime type is supported.
     *
     * This method should be called to ensure that the specified mime
     * type is supported by Rialto.
     *
     * @param[in] mimeType : The mime type to check.
     *
     * @retval true if supported.
     */
    virtual bool isMimeTypeSupported(const std::string &mimeType) = 0;

    /**
     * @brief  Check sinks and decoders for supported properties
     *
     * @param[in] mediaType     : The media type to search. If set to UNKNOWN then both AUDIO and VIDEO are searched
     * @param[in] propertyNames : A vector of property names to look for
     *
     * @retval Returns the subset of propertyNames that are supported by the mediaType
     */
    virtual std::vector<std::string> getSupportedProperties(MediaSourceType mediaType,
                                                            const std::vector<std::string> &propertyNames) = 0;

    /**
     * @brief Checks if the platform is video master.
     *
     * @param[out] isVideoMaster : The output value. True if video is master otherwise false.
     *
     * @retval true on success false otherwise
     */
    virtual bool isVideoMaster(bool &isVideoMaster) = 0;
};

}; // namespace firebolt::rialto

#endif // FIREBOLT_RIALTO_I_MEDIA_PIPELINE_CAPABILITIES_H_
