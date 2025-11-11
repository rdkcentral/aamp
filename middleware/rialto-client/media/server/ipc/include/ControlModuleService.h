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

#ifndef FIREBOLT_RIALTO_SERVER_IPC_CONTROL_MODULE_SERVICE_H_
#define FIREBOLT_RIALTO_SERVER_IPC_CONTROL_MODULE_SERVICE_H_

#include "IControlModuleService.h"
#include "IControlService.h"
#include "IPlaybackService.h"
#include <IIpcServer.h>
#include <map>
#include <memory>
#include <set>

namespace firebolt::rialto::server::ipc
{
class ControlModuleServiceFactory : public IControlModuleServiceFactory
{
public:
    ControlModuleServiceFactory() = default;
    virtual ~ControlModuleServiceFactory() = default;

    std::shared_ptr<IControlModuleService> create(service::IPlaybackService &playbackService,
                                                  service::IControlService &controlService) const override;
};

class ControlModuleService : public IControlModuleService
{
public:
    explicit ControlModuleService(service::IPlaybackService &playbackService, service::IControlService &controlService);
    ~ControlModuleService() override;

    void clientConnected(const std::shared_ptr<::firebolt::rialto::ipc::IClient> &ipcClient) override;
    void clientDisconnected(const std::shared_ptr<::firebolt::rialto::ipc::IClient> &ipcClient) override;

    void getSharedMemory(::google::protobuf::RpcController *controller,
                         const ::firebolt::rialto::GetSharedMemoryRequest *request,
                         ::firebolt::rialto::GetSharedMemoryResponse *response,
                         ::google::protobuf::Closure *done) override;
    void registerClient(::google::protobuf::RpcController *controller,
                        const ::firebolt::rialto::RegisterClientRequest *request,
                        ::firebolt::rialto::RegisterClientResponse *response, ::google::protobuf::Closure *done) override;
    void ack(::google::protobuf::RpcController *controller, const ::firebolt::rialto::AckRequest *request,
             ::firebolt::rialto::AckResponse *response, ::google::protobuf::Closure *done) override;

private:
    service::IPlaybackService &m_playbackService;
    service::IControlService &m_controlService;
    std::map<std::shared_ptr<::firebolt::rialto::ipc::IClient>, std::set<int>> m_controlIds;
};
} // namespace firebolt::rialto::server::ipc

#endif // FIREBOLT_RIALTO_SERVER_IPC_CONTROL_MODULE_SERVICE_H_
