/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2022 Sky UK
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

#include "IpcClientImpl.h"
#include "IpcServerImpl.h"
#include <memory>

namespace firebolt::rialto::ipc
{
ClientImpl::ClientImpl(const std::shared_ptr<ServerImpl> &server, uint64_t clientId, const struct ucred &creds)
    : m_kServer(server), m_kClientId(clientId), m_kCredentials(creds)
{
}

pid_t ClientImpl::getClientPid() const
{
    return m_kCredentials.pid;
}

uid_t ClientImpl::getClientUserId() const
{
    return m_kCredentials.uid;
}

gid_t ClientImpl::getClientGroupId() const
{
    return m_kCredentials.gid;
}

void ClientImpl::disconnect()
{
    auto server = m_kServer.lock();
    if (server)
    {
        server->disconnectClient(m_kClientId);
    }
}

void ClientImpl::exportService(const std::shared_ptr<google::protobuf::Service> &service)
{
    auto descriptor = service->GetDescriptor();
    m_services.emplace(descriptor->full_name(), service);
}

bool ClientImpl::sendEvent(const std::shared_ptr<google::protobuf::Message> &message)
{
    auto server = m_kServer.lock();
    if (server)
        return server->sendEvent(m_kClientId, message);
    else
        return false;
}

bool ClientImpl::isConnected() const
{
    auto server = m_kServer.lock();
    if (server)
        return server->isClientConnected(m_kClientId);
    else
        return false;
}

} // namespace firebolt::rialto::ipc
