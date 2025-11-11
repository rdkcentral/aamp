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

#ifndef FIREBOLT_RIALTO_SERVER_IPC_APPLICATION_MANAGEMENT_SERVER_H_
#define FIREBOLT_RIALTO_SERVER_IPC_APPLICATION_MANAGEMENT_SERVER_H_

#include "IApplicationManagementServer.h"
#include "IServerManagerModuleServiceFactory.h"
#include "ISessionServerManager.h"
#include <IIpcServer.h>
#include <IIpcServerFactory.h>
#include <memory>
#include <thread>

namespace firebolt::rialto::server::ipc
{
class ApplicationManagementServer : public IApplicationManagementServer
{
public:
    ApplicationManagementServer(const std::shared_ptr<firebolt::rialto::ipc::IServerFactory> &serverFactory,
                                const std::shared_ptr<firebolt::rialto::server::ipc::IServerManagerModuleServiceFactory>
                                    &serverManagerModuleFactory,
                                service::ISessionServerManager &sessionServerManager);
    ~ApplicationManagementServer() override;
    ApplicationManagementServer(const ApplicationManagementServer &) = delete;
    ApplicationManagementServer(ApplicationManagementServer &&) = delete;
    ApplicationManagementServer &operator=(const ApplicationManagementServer &) = delete;
    ApplicationManagementServer &operator=(ApplicationManagementServer &&) = delete;

    bool initialize(int socket) override;
    bool sendStateChangedEvent(const common::SessionServerState &state) override;
    void start() override;
    void stop() override;

private:
    void onClientDisconnected(const std::shared_ptr<::firebolt::rialto::ipc::IClient> &client);

private:
    std::thread m_ipcServerThread;
    std::shared_ptr<::firebolt::rialto::ipc::IServer> m_ipcServer;
    std::shared_ptr<::firebolt::rialto::ipc::IClient> m_ipcClient;
    std::shared_ptr<::rialto::ServerManagerModule> m_service;
};
} // namespace firebolt::rialto::server::ipc

#endif // FIREBOLT_RIALTO_SERVER_IPC_APPLICATION_MANAGEMENT_SERVER_H_
