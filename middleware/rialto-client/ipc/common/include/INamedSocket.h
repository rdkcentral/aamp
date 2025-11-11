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

#ifndef FIREBOLT_RIALTO_IPC_I_NAMED_SOCKET_H_
#define FIREBOLT_RIALTO_IPC_I_NAMED_SOCKET_H_

#include <memory>
#include <string>

namespace firebolt::rialto::ipc
{
class INamedSocket;

class INamedSocketFactory
{
public:
    virtual ~INamedSocketFactory() = default;

    static INamedSocketFactory &getFactory();
    virtual std::unique_ptr<INamedSocket> createNamedSocket() const = 0;
    virtual std::unique_ptr<INamedSocket> createNamedSocket(const std::string &socketPath) const = 0;
};

class INamedSocket
{
public:
    virtual ~INamedSocket() = default;
    virtual int getFd() const = 0;
    virtual bool setSocketPermissions(unsigned int socketPermissions) const = 0;
    virtual bool setSocketOwnership(const std::string &socketOwner, const std::string &socketGroup) const = 0;
    virtual bool blockNewConnections() const = 0;
    virtual bool bind(const std::string &socketPath) = 0;
};
} // namespace firebolt::rialto::ipc

#endif // FIREBOLT_RIALTO_IPC_I_NAMED_SOCKET_H_
