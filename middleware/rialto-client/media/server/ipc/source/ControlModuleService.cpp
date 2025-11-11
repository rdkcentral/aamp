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

#include "ControlModuleService.h"
#include "ControlClientServerInternal.h"
#include "IPlaybackService.h"
#include "RialtoServerLogging.h"
#include "SchemaVersion.h"
#include <IIpcController.h>
#include <algorithm>
#include <cstdint>

namespace
{
int generateControlId()
{
    static int id{0};
    return id++;
}
} // namespace

namespace firebolt::rialto::server::ipc
{
std::shared_ptr<IControlModuleServiceFactory> IControlModuleServiceFactory::createFactory()
{
    std::shared_ptr<IControlModuleServiceFactory> factory;

    try
    {
        factory = std::make_shared<ControlModuleServiceFactory>();
    }
    catch (const std::exception &e)
    {
        RIALTO_SERVER_LOG_ERROR("Failed to create the rialto control module service factory, reason: %s", e.what());
    }

    return factory;
}

std::shared_ptr<IControlModuleService> ControlModuleServiceFactory::create(service::IPlaybackService &playbackService,
                                                                           service::IControlService &controlService) const
{
    std::shared_ptr<IControlModuleService> controlModule;

    try
    {
        controlModule = std::make_shared<ControlModuleService>(playbackService, controlService);
    }
    catch (const std::exception &e)
    {
        RIALTO_SERVER_LOG_ERROR("Failed to create the rialto control module service, reason: %s", e.what());
    }

    return controlModule;
}

ControlModuleService::ControlModuleService(service::IPlaybackService &playbackService,
                                           service::IControlService &controlService)
    : m_playbackService{playbackService}, m_controlService{controlService}
{
}

ControlModuleService::~ControlModuleService()
{
    for (const auto &controlIds : m_controlIds)
    {
        for (int id : controlIds.second)
        {
            m_controlService.removeControl(id);
        }
    }
}

void ControlModuleService::clientConnected(const std::shared_ptr<::firebolt::rialto::ipc::IClient> &ipcClient)
{
    RIALTO_SERVER_LOG_INFO("Client Connected!");
    m_controlIds.emplace(ipcClient, std::set<int>());
    ipcClient->exportService(shared_from_this());
}

void ControlModuleService::clientDisconnected(const std::shared_ptr<::firebolt::rialto::ipc::IClient> &ipcClient)
{
    RIALTO_SERVER_LOG_INFO("Client disconnected!");
    auto controlIdsIter = m_controlIds.find(ipcClient);
    if (m_controlIds.end() != controlIdsIter)
    {
        for (int id : controlIdsIter->second)
        {
            m_controlService.removeControl(id);
        }
        m_controlIds.erase(controlIdsIter);
    }
}

void ControlModuleService::getSharedMemory(::google::protobuf::RpcController *controller,
                                           const ::firebolt::rialto::GetSharedMemoryRequest *request,
                                           ::firebolt::rialto::GetSharedMemoryResponse *response,
                                           ::google::protobuf::Closure *done)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");

    int32_t fd;
    uint32_t size;
    if (!m_playbackService.getSharedMemory(fd, size))
    {
        RIALTO_SERVER_LOG_ERROR("getSharedMemory failed");
        controller->SetFailed("Operation failed");
        done->Run();
        return;
    }
    response->set_fd(fd);
    response->set_size(size);
    done->Run();
}

void ControlModuleService::registerClient(::google::protobuf::RpcController *controller,
                                          const ::firebolt::rialto::RegisterClientRequest *request,
                                          ::firebolt::rialto::RegisterClientResponse *response,
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
    const auto kCurrentSchemaVersion{common::getCurrentSchemaVersion()};
    if (request->has_client_schema_version())
    {
        const firebolt::rialto::common::SchemaVersion kClientSchemaVersion{request->client_schema_version().major(),
                                                                           request->client_schema_version().minor(),
                                                                           request->client_schema_version().patch()};
        RIALTO_SERVER_LOG_DEBUG("Server schema version: %s, client schema version: %s",
                                kCurrentSchemaVersion.str().c_str(), kClientSchemaVersion.str().c_str());
        if (!kCurrentSchemaVersion.isCompatible(kClientSchemaVersion))
        {
            RIALTO_SERVER_LOG_ERROR("Server and client schema versions not compatible");
            controller->SetFailed("Server and client schema versions not compatible");
            done->Run();
            return;
        }
    }
    else
    {
        RIALTO_SERVER_LOG_WARN("Client schema version not present in RegisterClientRequest message");
    }
    const int kControlId{generateControlId()};
    auto ipcClient = ipcController->getClient();
    auto controlClient{std::make_shared<ControlClientServerInternal>(kControlId, ipcClient)};
    m_controlService.addControl(kControlId, controlClient);
    m_controlIds[ipcClient].insert(kControlId);
    response->set_control_handle(kControlId);
    response->mutable_server_schema_version()->set_major(kCurrentSchemaVersion.major());
    response->mutable_server_schema_version()->set_minor(kCurrentSchemaVersion.minor());
    response->mutable_server_schema_version()->set_patch(kCurrentSchemaVersion.patch());
    done->Run();
}

void ControlModuleService::ack(::google::protobuf::RpcController *controller,
                               const ::firebolt::rialto::AckRequest *request, ::firebolt::rialto::AckResponse *response,
                               ::google::protobuf::Closure *done)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");
    if (!m_controlService.ack(request->control_handle(), request->id()))
    {
        RIALTO_SERVER_LOG_ERROR("ack failed");
        controller->SetFailed("Operation failed");
        done->Run();
        return;
    }
    done->Run();
}
} // namespace firebolt::rialto::server::ipc
