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

#ifndef FIREBOLT_RIALTO_SERVER_IPC_SERVER_MANAGER_MODULE_SERVICE_H_
#define FIREBOLT_RIALTO_SERVER_IPC_SERVER_MANAGER_MODULE_SERVICE_H_

#include "IServerManagerModuleServiceFactory.h"
#include "ISessionServerManager.h"
#include <IIpcServer.h>
#include <memory>

namespace firebolt::rialto::server::ipc
{
class ServerManagerModuleServiceFactory : public IServerManagerModuleServiceFactory
{
public:
    ServerManagerModuleServiceFactory() = default;
    virtual ~ServerManagerModuleServiceFactory() = default;

    std::shared_ptr<::rialto::ServerManagerModule>
    create(service::ISessionServerManager &sessionServerManager) const override;
};

class ServerManagerModuleService : public ::rialto::ServerManagerModule
{
public:
    explicit ServerManagerModuleService(service::ISessionServerManager &sessionServerManager);
    ~ServerManagerModuleService() override;

    void setConfiguration(::google::protobuf::RpcController *controller, const ::rialto::SetConfigurationRequest *request,
                          ::rialto::SetConfigurationResponse *response, ::google::protobuf::Closure *done) override;
    void setState(::google::protobuf::RpcController *controller, const ::rialto::SetStateRequest *request,
                  ::rialto::SetStateResponse *response, ::google::protobuf::Closure *done) override;
    void setLogLevels(::google::protobuf::RpcController *controller, const ::rialto::SetLogLevelsRequest *request,
                      ::rialto::SetLogLevelsResponse *response, ::google::protobuf::Closure *done) override;
    void ping(::google::protobuf::RpcController *controller, const ::rialto::PingRequest *request,
              ::rialto::PingResponse *response, ::google::protobuf::Closure *done) override;

private:
    service::ISessionServerManager &m_sessionServerManager;
};
} // namespace firebolt::rialto::server::ipc

#endif // FIREBOLT_RIALTO_SERVER_IPC_SERVER_MANAGER_MODULE_SERVICE_H_
