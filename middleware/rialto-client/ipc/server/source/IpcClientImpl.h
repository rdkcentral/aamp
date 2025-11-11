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

#ifndef FIREBOLT_RIALTO_IPC_IPC_CLIENT_IMPL_H_
#define FIREBOLT_RIALTO_IPC_IPC_CLIENT_IMPL_H_

#include "IIpcServer.h"

#include <sys/socket.h>

#include <map>
#include <memory>
#include <string>

namespace firebolt::rialto::ipc
{
class ServerImpl;

class ClientImpl final : public IClient
{
public:
    ClientImpl(const std::shared_ptr<ServerImpl> &server, uint64_t clientId, const struct ucred &creds);
    ~ClientImpl() final = default;

public:
    pid_t getClientPid() const override;
    uid_t getClientUserId() const override;
    gid_t getClientGroupId() const override;

    void disconnect() override;

    void exportService(const std::shared_ptr<google::protobuf::Service> &service) override;

    bool sendEvent(const std::shared_ptr<google::protobuf::Message> &message) override;

    bool isConnected() const override;

protected:
    friend class ServerImpl;
    inline uint64_t id() const { return m_kClientId; }

private:
    const std::weak_ptr<ServerImpl> m_kServer;
    const uint64_t m_kClientId{};
    const struct ucred m_kCredentials;

    std::map<std::string, std::shared_ptr<google::protobuf::Service>> m_services;
};

} // namespace firebolt::rialto::ipc

#endif // FIREBOLT_RIALTO_IPC_IPC_CLIENT_IMPL_H_
