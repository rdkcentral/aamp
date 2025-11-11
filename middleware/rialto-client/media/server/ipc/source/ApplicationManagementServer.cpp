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

#include "ApplicationManagementServer.h"
#include "IServerManagerModuleServiceFactory.h"
#include "RialtoServerLogging.h"
#include <IIpcServerFactory.h>

namespace
{
rialto::SessionServerState convertSessionServerState(const firebolt::rialto::common::SessionServerState &state)
{
    switch (state)
    {
    case firebolt::rialto::common::SessionServerState::UNINITIALIZED:
        return rialto::SessionServerState::UNINITIALIZED;
    case firebolt::rialto::common::SessionServerState::INACTIVE:
        return rialto::SessionServerState::INACTIVE;
    case firebolt::rialto::common::SessionServerState::ACTIVE:
        return rialto::SessionServerState::ACTIVE;
    case firebolt::rialto::common::SessionServerState::NOT_RUNNING:
        return rialto::SessionServerState::NOT_RUNNING;
    case firebolt::rialto::common::SessionServerState::ERROR:
        return rialto::SessionServerState::ERROR;
    }
    return rialto::SessionServerState::ERROR;
}
} // namespace

namespace firebolt::rialto::server::ipc
{
ApplicationManagementServer::ApplicationManagementServer(
    const std::shared_ptr<firebolt::rialto::ipc::IServerFactory> &serverFactory,
    const std::shared_ptr<firebolt::rialto::server::ipc::IServerManagerModuleServiceFactory> &serverManagerModuleFactory,
    service::ISessionServerManager &sessionServerManager)
    : m_ipcServer{serverFactory->create()}, m_service{serverManagerModuleFactory->create(sessionServerManager)}
{
}

ApplicationManagementServer::~ApplicationManagementServer()
{
    stop();
    if (m_ipcServerThread.joinable())
    {
        m_ipcServerThread.join();
    }
}

bool ApplicationManagementServer::initialize(int socket)
{
    RIALTO_SERVER_LOG_INFO("Initializing ApplicationManagementServer, socket: %d", socket);
    if (!m_ipcServer)
    {
        RIALTO_SERVER_LOG_ERROR("Failed to initialize ApplicationManagementServer - Ipc server instance is NULL");
        return false;
    }

    m_ipcClient = m_ipcServer->addClient(socket, std::bind(&ApplicationManagementServer::onClientDisconnected, this,
                                                           std::placeholders::_1));
    if (!m_ipcClient)
    {
        RIALTO_SERVER_LOG_ERROR("Failed to initialize ApplicationManagementServer - Client is NULL");
        return false;
    }
    m_ipcClient->exportService(m_service);
    RIALTO_SERVER_LOG_MIL("ApplicationManagementServer initialized");
    return true;
}

bool ApplicationManagementServer::sendStateChangedEvent(const common::SessionServerState &state)
{
    if (!m_ipcClient->isConnected())
    {
        return false;
    }
    auto stateChangedEvent = std::make_shared<::rialto::StateChangedEvent>();
    stateChangedEvent->set_sessionserverstate(convertSessionServerState(state));
    m_ipcClient->sendEvent(stateChangedEvent);
    return true;
}

void ApplicationManagementServer::start()
{
    m_ipcServerThread = std::thread(
        [this]()
        {
            while (m_ipcServer->process() && m_ipcClient && m_ipcClient->isConnected())
            {
                m_ipcServer->wait(-1);
            }
        });
}

void ApplicationManagementServer::stop()
{
    if (m_ipcClient && m_ipcClient->isConnected())
    {
        m_ipcClient->disconnect();
    }
}

void ApplicationManagementServer::onClientDisconnected(const std::shared_ptr<::firebolt::rialto::ipc::IClient> &client)
{
    RIALTO_SERVER_LOG_WARN("Rialto Server Manager disconnected");
}
} // namespace firebolt::rialto::server::ipc
