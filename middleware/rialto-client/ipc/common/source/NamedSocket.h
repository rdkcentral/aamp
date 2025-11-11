/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2025 Sky UK
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

#ifndef FIREBOLT_RIALTO_IPC_NAMED_SOCKET_H_
#define FIREBOLT_RIALTO_IPC_NAMED_SOCKET_H_

#include "INamedSocket.h"
#include <memory>
#include <string>

namespace firebolt::rialto::ipc
{
class NamedSocketFactory : public INamedSocketFactory
{
public:
    std::unique_ptr<INamedSocket> createNamedSocket() const override;
    std::unique_ptr<INamedSocket> createNamedSocket(const std::string &socketPath) const override;
};

class NamedSocket : public INamedSocket
{
public:
    NamedSocket();
    explicit NamedSocket(const std::string &socketPath);
    ~NamedSocket() override;

    int getFd() const override;
    bool setSocketPermissions(unsigned int socketPermissions) const override;
    bool setSocketOwnership(const std::string &socketOwner, const std::string &socketGroup) const override;
    bool blockNewConnections() const override;
    bool bind(const std::string &socketPath) override;

private:
    void closeListeningSocket();
    bool getSocketLock();
    uid_t getSocketOwnerId(const std::string &socketOwner) const;
    gid_t getSocketGroupId(const std::string &socketGroup) const;

private:
    int m_sockFd{-1};
    int m_lockFd{-1};
    std::string m_sockPath{};
    std::string m_lockPath{};
};
} // namespace firebolt::rialto::ipc

#endif // FIREBOLT_RIALTO_IPC_NAMED_SOCKET_H_
