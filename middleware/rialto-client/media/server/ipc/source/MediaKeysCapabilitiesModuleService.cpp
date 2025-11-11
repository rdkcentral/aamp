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

#include <IIpcController.h>
#include <algorithm>
#include <cstdint>

#include "ICdmService.h"
#include "MediaKeysCapabilitiesModuleService.h"
#include "RialtoServerLogging.h"

namespace firebolt::rialto::server::ipc
{
std::shared_ptr<IMediaKeysCapabilitiesModuleServiceFactory> IMediaKeysCapabilitiesModuleServiceFactory::createFactory()
{
    std::shared_ptr<IMediaKeysCapabilitiesModuleServiceFactory> factory;

    try
    {
        factory = std::make_shared<MediaKeysCapabilitiesModuleServiceFactory>();
    }
    catch (const std::exception &e)
    {
        RIALTO_SERVER_LOG_ERROR("Failed to create the media keys capabilities module service factory, reason: %s",
                                e.what());
    }

    return factory;
}

std::shared_ptr<IMediaKeysCapabilitiesModuleService>
MediaKeysCapabilitiesModuleServiceFactory::create(service::ICdmService &cdmService) const
{
    std::shared_ptr<IMediaKeysCapabilitiesModuleService> mediaKeysCapabilitiesModule;

    try
    {
        mediaKeysCapabilitiesModule = std::make_shared<MediaKeysCapabilitiesModuleService>(cdmService);
    }
    catch (const std::exception &e)
    {
        RIALTO_SERVER_LOG_ERROR("Failed to create the media keys capabilities module service, reason: %s", e.what());
    }

    return mediaKeysCapabilitiesModule;
}

MediaKeysCapabilitiesModuleService::MediaKeysCapabilitiesModuleService(service::ICdmService &cdmService)
    : m_cdmService{cdmService}
{
}

MediaKeysCapabilitiesModuleService::~MediaKeysCapabilitiesModuleService() {}

void MediaKeysCapabilitiesModuleService::clientConnected(const std::shared_ptr<::firebolt::rialto::ipc::IClient> &ipcClient)
{
    RIALTO_SERVER_LOG_INFO("Client Connected!");
    ipcClient->exportService(shared_from_this());
}

void MediaKeysCapabilitiesModuleService::clientDisconnected(const std::shared_ptr<::firebolt::rialto::ipc::IClient> &ipcClient)
{
    RIALTO_SERVER_LOG_INFO("Client disconnected!");
}

void MediaKeysCapabilitiesModuleService::getSupportedKeySystems(
    ::google::protobuf::RpcController *controller, const ::firebolt::rialto::GetSupportedKeySystemsRequest *request,
    ::firebolt::rialto::GetSupportedKeySystemsResponse *response, ::google::protobuf::Closure *done)
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

    std::vector<std::string> keySystems = m_cdmService.getSupportedKeySystems();

    for (auto it = keySystems.begin(); it != keySystems.end(); it++)
    {
        response->add_key_systems(*it);
    }
    done->Run();
}

void MediaKeysCapabilitiesModuleService::supportsKeySystem(::google::protobuf::RpcController *controller,
                                                           const ::firebolt::rialto::SupportsKeySystemRequest *request,
                                                           ::firebolt::rialto::SupportsKeySystemResponse *response,
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

    response->set_is_supported(m_cdmService.supportsKeySystem(request->key_system()));
    done->Run();
}

void MediaKeysCapabilitiesModuleService::getSupportedKeySystemVersion(
    ::google::protobuf::RpcController *controller, const ::firebolt::rialto::GetSupportedKeySystemVersionRequest *request,
    ::firebolt::rialto::GetSupportedKeySystemVersionResponse *response, ::google::protobuf::Closure *done)
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

    std::string version;
    bool status = m_cdmService.getSupportedKeySystemVersion(request->key_system(), version);
    if (status)
    {
        response->set_version(version);
    }
    else
    {
        RIALTO_SERVER_LOG_ERROR("Failed to get the key system version");
        controller->SetFailed("Operation failed");
    }

    done->Run();
}

void MediaKeysCapabilitiesModuleService::isServerCertificateSupported(
    ::google::protobuf::RpcController *controller, const ::firebolt::rialto::IsServerCertificateSupportedRequest *request,
    ::firebolt::rialto::IsServerCertificateSupportedResponse *response, ::google::protobuf::Closure *done)
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

    response->set_is_supported(m_cdmService.isServerCertificateSupported(request->key_system()));
    done->Run();
}

} // namespace firebolt::rialto::server::ipc
