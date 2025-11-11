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

#ifndef FIREBOLT_RIALTO_SERVER_IPC_I_SESSION_MANAGEMENT_SERVER_H_
#define FIREBOLT_RIALTO_SERVER_IPC_I_SESSION_MANAGEMENT_SERVER_H_

#include "RialtoServerLogging.h"
#include <memory>
#include <string>

namespace firebolt::rialto::server::ipc
{
class ISessionManagementServer
{
public:
    ISessionManagementServer() = default;
    virtual ~ISessionManagementServer() = default;

    ISessionManagementServer(const ISessionManagementServer &) = delete;
    ISessionManagementServer(ISessionManagementServer &&) = delete;
    ISessionManagementServer &operator=(const ISessionManagementServer &) = delete;
    ISessionManagementServer &operator=(ISessionManagementServer &&) = delete;

    virtual bool initialize(const std::string &socketName, unsigned int socketPermissions,
                            const std::string &socketOwner, const std::string &socketGroup) = 0;
    virtual bool initialize(int32_t socketFd) = 0;
    virtual void start() = 0;
    virtual void stop() = 0;
    virtual void setLogLevels(RIALTO_DEBUG_LEVEL defaultLogLevels, RIALTO_DEBUG_LEVEL clientLogLevels,
                              RIALTO_DEBUG_LEVEL ipcLogLevels, RIALTO_DEBUG_LEVEL commonLogLevels) = 0;
};
} // namespace firebolt::rialto::server::ipc

#endif // FIREBOLT_RIALTO_SERVER_IPC_I_SESSION_MANAGEMENT_SERVER_H_
