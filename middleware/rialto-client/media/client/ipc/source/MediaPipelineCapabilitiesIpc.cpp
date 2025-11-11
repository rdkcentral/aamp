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

#include "MediaPipelineCapabilitiesIpc.h"
#include "RialtoClientLogging.h"
#include "RialtoCommonIpc.h"

namespace firebolt::rialto::client
{
std::shared_ptr<IMediaPipelineCapabilitiesIpcFactory> IMediaPipelineCapabilitiesIpcFactory::createFactory()
{
    std::shared_ptr<IMediaPipelineCapabilitiesIpcFactory> factory;

    try
    {
        factory = std::make_shared<MediaPipelineCapabilitiesIpcFactory>();
    }
    catch (const std::exception &e)
    {
        RIALTO_CLIENT_LOG_ERROR("Failed to create the media pipeline capabilities ipc factory, reason: %s", e.what());
    }

    return factory;
}

std::unique_ptr<IMediaPipelineCapabilities> MediaPipelineCapabilitiesIpcFactory::createMediaPipelineCapabilitiesIpc() const
{
    std::unique_ptr<IMediaPipelineCapabilities> mediaPipelineCapabilitiesIpc;

    try
    {
        mediaPipelineCapabilitiesIpc =
            std::make_unique<client::MediaPipelineCapabilitiesIpc>(IIpcClientAccessor::instance().getIpcClient());
    }
    catch (const std::exception &e)
    {
        RIALTO_CLIENT_LOG_ERROR("Failed to create the media pipeline capabilities ipc, reason: %s", e.what());
    }

    return mediaPipelineCapabilitiesIpc;
}

MediaPipelineCapabilitiesIpc::MediaPipelineCapabilitiesIpc(IIpcClient &ipcClient) : IpcModule(ipcClient)
{
    RIALTO_CLIENT_LOG_DEBUG("entry:");

    if (!attachChannel())
    {
        throw std::runtime_error("Failed attach to the ipc channel");
    }
}

MediaPipelineCapabilitiesIpc::~MediaPipelineCapabilitiesIpc()
{
    RIALTO_CLIENT_LOG_DEBUG("entry:");

    detachChannel();
}

bool MediaPipelineCapabilitiesIpc::createRpcStubs(const std::shared_ptr<ipc::IChannel> &ipcChannel)
{
    m_mediaPipelineCapabilitiesStub =
        std::make_unique<::firebolt::rialto::MediaPipelineCapabilitiesModule_Stub>(ipcChannel.get());
    if (!m_mediaPipelineCapabilitiesStub)
    {
        return false;
    }
    return true;
}

std::vector<std::string> MediaPipelineCapabilitiesIpc::getSupportedMimeTypes(MediaSourceType sourceType)
{
    if (!reattachChannelIfRequired())
    {
        RIALTO_CLIENT_LOG_ERROR("Reattachment of the ipc channel failed, ipc disconnected");
        return {};
    }

    firebolt::rialto::GetSupportedMimeTypesRequest request;
    request.set_media_type(convertProtoMediaSourceType(sourceType));

    firebolt::rialto::GetSupportedMimeTypesResponse response;
    auto ipcController = m_ipc.createRpcController();
    auto blockingClosure = m_ipc.createBlockingClosure();
    m_mediaPipelineCapabilitiesStub->getSupportedMimeTypes(ipcController.get(), &request, &response,
                                                           blockingClosure.get());

    // wait for the call to complete
    blockingClosure->wait();

    // check the result
    if (ipcController->Failed())
    {
        RIALTO_CLIENT_LOG_ERROR("failed to get supported mime types due to '%s'", ipcController->ErrorText().c_str());
        return {};
    }

    return std::vector<std::string>{response.mime_types().begin(), response.mime_types().end()};
}

bool MediaPipelineCapabilitiesIpc::isMimeTypeSupported(const std::string &mimeType)
{
    if (!reattachChannelIfRequired())
    {
        RIALTO_CLIENT_LOG_ERROR("Reattachment of the ipc channel failed, ipc disconnected");
        return false;
    }

    firebolt::rialto::IsMimeTypeSupportedRequest request;
    request.set_mime_type(mimeType);

    firebolt::rialto::IsMimeTypeSupportedResponse response;
    auto ipcController = m_ipc.createRpcController();
    auto blockingClosure = m_ipc.createBlockingClosure();
    m_mediaPipelineCapabilitiesStub->isMimeTypeSupported(ipcController.get(), &request, &response, blockingClosure.get());

    // wait for the call to complete
    blockingClosure->wait();

    // check the result
    if (ipcController->Failed())
    {
        RIALTO_CLIENT_LOG_ERROR("failed to check if mime type '%s' is supported due to '%s'", mimeType.c_str(),
                                ipcController->ErrorText().c_str());
        return false;
    }

    return response.is_supported();
}

std::vector<std::string> MediaPipelineCapabilitiesIpc::getSupportedProperties(MediaSourceType mediaType,
                                                                              const std::vector<std::string> &propertyNames)
{
    if (!reattachChannelIfRequired())
    {
        RIALTO_CLIENT_LOG_ERROR("Reattachment of the ipc channel failed, ipc disconnected");
        return {};
    }

    firebolt::rialto::GetSupportedPropertiesRequest request;
    request.set_media_type(convertProtoMediaSourceType(mediaType));
    for (const std::string &property : propertyNames)
        request.add_property_names(property);

    firebolt::rialto::GetSupportedPropertiesResponse response;
    auto ipcController = m_ipc.createRpcController();
    auto blockingClosure = m_ipc.createBlockingClosure();
    m_mediaPipelineCapabilitiesStub->getSupportedProperties(ipcController.get(), &request, &response,
                                                            blockingClosure.get());

    // wait for the call to complete
    blockingClosure->wait();

    // check the result
    if (ipcController->Failed())
    {
        RIALTO_CLIENT_LOG_ERROR("failed due to '%s'", ipcController->ErrorText().c_str());
        return std::vector<std::string>{};
    }

    return std::vector<std::string>{response.supported_properties().begin(), response.supported_properties().end()};
}

bool MediaPipelineCapabilitiesIpc::isVideoMaster(bool &isVideoMaster)
{
    if (!reattachChannelIfRequired())
    {
        RIALTO_CLIENT_LOG_ERROR("Reattachment of the ipc channel failed, ipc disconnected");
        return {};
    }

    firebolt::rialto::IsVideoMasterRequest request;
    firebolt::rialto::IsVideoMasterResponse response;
    auto ipcController = m_ipc.createRpcController();
    auto blockingClosure = m_ipc.createBlockingClosure();
    m_mediaPipelineCapabilitiesStub->isVideoMaster(ipcController.get(), &request, &response, blockingClosure.get());

    // wait for the call to complete
    blockingClosure->wait();

    // check the result
    if (ipcController->Failed())
    {
        RIALTO_CLIENT_LOG_ERROR("failed due to '%s'", ipcController->ErrorText().c_str());
        return false;
    }

    isVideoMaster = response.is_video_master();

    return true;
}
}; // namespace firebolt::rialto::client
