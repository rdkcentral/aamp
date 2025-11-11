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

#ifndef FIREBOLT_RIALTO_CLIENT_MEDIA_PIPELINE_CAPABILITIES_IPC_H_
#define FIREBOLT_RIALTO_CLIENT_MEDIA_PIPELINE_CAPABILITIES_IPC_H_

#include "IMediaPipelineCapabilitiesIpcFactory.h"
#include "IpcModule.h"
#include <memory>
#include <string>
#include <vector>

#include "mediapipelinecapabilitiesmodule.pb.h"

namespace firebolt::rialto::client
{
/**
 * @brief IMediaPipelineCapabilitiesIpc factory class definition.
 */
class MediaPipelineCapabilitiesIpcFactory : public IMediaPipelineCapabilitiesIpcFactory
{
public:
    MediaPipelineCapabilitiesIpcFactory() = default;
    ~MediaPipelineCapabilitiesIpcFactory() override = default;

    std::unique_ptr<IMediaPipelineCapabilities> createMediaPipelineCapabilitiesIpc() const override;
};

/**
 * @brief The definition of the MediaPipelineCapabilitiesIpc.
 */
class MediaPipelineCapabilitiesIpc : public IMediaPipelineCapabilities, public IpcModule
{
public:
    /**
     * @brief The constructor.
     *
     * @param[in] ipcClient      : The ipc client
     */
    explicit MediaPipelineCapabilitiesIpc(IIpcClient &ipcClient);

    /**
     * @brief Virtual destructor.
     */
    virtual ~MediaPipelineCapabilitiesIpc();

    /**
     * @brief Returns the MSE mime types supported by Rialto for \a sourceType
     *
     * @param[in] sourceType : source type
     *
     * @retval The supported mime types.
     */
    std::vector<std::string> getSupportedMimeTypes(MediaSourceType sourceType) override;

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
    bool isMimeTypeSupported(const std::string &mimeType) override;

    /**
     * @brief  Check sinks and decoders for supported properties
     *
     * @param[in] mediaType     : The media type to search. If set to UNKNOWN then both AUDIO and VIDEO are searched
     * @param[in] propertyNames : A vector of property names to look for
     *
     * @retval Returns the subset of propertyNames that are supported by the mediaType
     */
    std::vector<std::string> getSupportedProperties(MediaSourceType mediaType,
                                                    const std::vector<std::string> &propertyNames) override;

    /**
     * @brief Checks if the platform is video master.
     *
     * @param[out] isVideoMaster : The output value. True if video is master otherwise false.
     *
     * @retval true on success false otherwise
     */
    bool isVideoMaster(bool &isVideoMaster) override;

private:
    /**
     * @brief The ipc protobuf media Pipeline capabilities stub.
     */
    std::unique_ptr<::firebolt::rialto::MediaPipelineCapabilitiesModule_Stub> m_mediaPipelineCapabilitiesStub;

    bool createRpcStubs(const std::shared_ptr<ipc::IChannel> &ipcChannel) override;

    bool subscribeToEvents(const std::shared_ptr<ipc::IChannel> &ipcChannel) override { return true; }
};

}; // namespace firebolt::rialto::client

#endif // FIREBOLT_RIALTO_CLIENT_MEDIA_PIPELINE_CAPABILITIES_IPC_H_
