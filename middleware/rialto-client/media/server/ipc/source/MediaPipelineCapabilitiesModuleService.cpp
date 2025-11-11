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

#include "MediaPipelineCapabilitiesModuleService.h"
#include "RialtoCommonModule.h"
#include "RialtoServerLogging.h"
#include <IIpcController.h>

namespace firebolt::rialto::server::ipc
{
std::shared_ptr<IMediaPipelineCapabilitiesModuleServiceFactory>
IMediaPipelineCapabilitiesModuleServiceFactory::createFactory()
{
    std::shared_ptr<IMediaPipelineCapabilitiesModuleServiceFactory> factory;

    try
    {
        factory = std::make_shared<MediaPipelineCapabilitiesModuleServiceFactory>();
    }
    catch (const std::exception &e)
    {
        RIALTO_SERVER_LOG_ERROR("Failed to create the media player capabilities module service factory, reason: %s",
                                e.what());
    }

    return factory;
}

std::shared_ptr<IMediaPipelineCapabilitiesModuleService>
MediaPipelineCapabilitiesModuleServiceFactory::create(service::IMediaPipelineService &mediaPipelineService) const
{
    std::shared_ptr<IMediaPipelineCapabilitiesModuleService> mediaPipelineCapabilitiesModule;

    try
    {
        mediaPipelineCapabilitiesModule = std::make_shared<MediaPipelineCapabilitiesModuleService>(mediaPipelineService);
    }
    catch (const std::exception &e)
    {
        RIALTO_SERVER_LOG_ERROR("Failed to create the media player module service, reason: %s", e.what());
    }

    return mediaPipelineCapabilitiesModule;
}

MediaPipelineCapabilitiesModuleService::MediaPipelineCapabilitiesModuleService(
    service::IMediaPipelineService &mediaPipelineService)
    : m_mediaPipelineService{mediaPipelineService}
{
}

MediaPipelineCapabilitiesModuleService::~MediaPipelineCapabilitiesModuleService() {}

void MediaPipelineCapabilitiesModuleService::clientConnected(const std::shared_ptr<::firebolt::rialto::ipc::IClient> &ipcClient)
{
    RIALTO_SERVER_LOG_INFO("Client Connected!");
    ipcClient->exportService(shared_from_this());
}

void MediaPipelineCapabilitiesModuleService::clientDisconnected(
    const std::shared_ptr<::firebolt::rialto::ipc::IClient> &ipcClient)
{
    RIALTO_SERVER_LOG_INFO("Client disconnected!");
}

void MediaPipelineCapabilitiesModuleService::getSupportedMimeTypes(
    ::google::protobuf::RpcController *controller, const ::firebolt::rialto::GetSupportedMimeTypesRequest *request,
    ::firebolt::rialto::GetSupportedMimeTypesResponse *response, ::google::protobuf::Closure *done)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");
    auto ipcController = dynamic_cast<firebolt::rialto::ipc::IController *>(controller);
    if (!ipcController)
    {
        RIALTO_SERVER_LOG_ERROR("ipc library provided incompatible controller object");
        controller->SetFailed("ipc library provided incompatible controller object");
        done->Run();
        return;
    }

    firebolt::rialto::MediaSourceType sourceType = convertMediaSourceType(request->media_type());
    std::vector<std::string> supportedMimeTypes = m_mediaPipelineService.getSupportedMimeTypes(sourceType);

    for (std::string &mimeType : supportedMimeTypes)
    {
        response->add_mime_types(mimeType);
    }

    done->Run();
}

void MediaPipelineCapabilitiesModuleService::isMimeTypeSupported(
    ::google::protobuf::RpcController *controller, const ::firebolt::rialto::IsMimeTypeSupportedRequest *request,
    ::firebolt::rialto::IsMimeTypeSupportedResponse *response, ::google::protobuf::Closure *done)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");
    auto ipcController = dynamic_cast<firebolt::rialto::ipc::IController *>(controller);
    if (!ipcController)
    {
        RIALTO_SERVER_LOG_ERROR("ipc library provided incompatible controller object");
        controller->SetFailed("ipc library provided incompatible controller object");
        done->Run();
        return;
    }

    bool isSupported = m_mediaPipelineService.isMimeTypeSupported(request->mime_type());
    response->set_is_supported(isSupported);

    done->Run();
}

void MediaPipelineCapabilitiesModuleService::getSupportedProperties(
    ::google::protobuf::RpcController *controller, const ::firebolt::rialto::GetSupportedPropertiesRequest *request,
    ::firebolt::rialto::GetSupportedPropertiesResponse *response, ::google::protobuf::Closure *done)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");
    auto ipcController = dynamic_cast<firebolt::rialto::ipc::IController *>(controller);
    if (!ipcController)
    {
        RIALTO_SERVER_LOG_ERROR("ipc library provided incompatible controller object");
        controller->SetFailed("ipc library provided incompatible controller object");
        done->Run();
        return;
    }

    firebolt::rialto::MediaSourceType mediaType = convertMediaSourceType(request->media_type());
    std::vector<std::string> propertiesToSearch{request->property_names().begin(), request->property_names().end()};

    std::vector<std::string> supportedProperties{
        m_mediaPipelineService.getSupportedProperties(mediaType, propertiesToSearch)};

    for (const std::string &property : supportedProperties)
    {
        response->add_supported_properties(property.c_str());
    }

    done->Run();
}

void MediaPipelineCapabilitiesModuleService::isVideoMaster(::google::protobuf::RpcController *controller,
                                                           const ::firebolt::rialto::IsVideoMasterRequest *request,
                                                           ::firebolt::rialto::IsVideoMasterResponse *response,
                                                           ::google::protobuf::Closure *done)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");
    auto ipcController = dynamic_cast<firebolt::rialto::ipc::IController *>(controller);
    if (!ipcController)
    {
        RIALTO_SERVER_LOG_ERROR("ipc library provided incompatible controller object");
        controller->SetFailed("ipc library provided incompatible controller object");
        done->Run();
        return;
    }

    bool isMaster{false};
    if (!m_mediaPipelineService.isVideoMaster(isMaster))
    {
        RIALTO_SERVER_LOG_ERROR("isVideoMaster check failed");
        controller->SetFailed("isVideoMaster check failed");
        done->Run();
        return;
    }

    response->set_is_video_master(isMaster);

    done->Run();
}
} // namespace firebolt::rialto::server::ipc
