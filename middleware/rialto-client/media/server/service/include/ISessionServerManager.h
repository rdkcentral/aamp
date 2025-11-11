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

#ifndef FIREBOLT_RIALTO_SERVER_SERVICE_I_SESSION_SERVER_MANAGER_H_
#define FIREBOLT_RIALTO_SERVER_SERVICE_I_SESSION_SERVER_MANAGER_H_

#include "IAckSender.h"
#include "RialtoLogging.h"
#include "SessionServerCommon.h"
#include <memory>
#include <string>

namespace firebolt::rialto::server::service
{
class ISessionServerManager
{
public:
    ISessionServerManager() = default;
    virtual ~ISessionServerManager() = default;

    ISessionServerManager(const ISessionServerManager &) = delete;
    ISessionServerManager(ISessionServerManager &&) = delete;
    ISessionServerManager &operator=(const ISessionServerManager &) = delete;
    ISessionServerManager &operator=(ISessionServerManager &&) = delete;

    virtual bool initialize(int argc, char *argv[]) = 0;
    virtual void startService() = 0;
    virtual bool configureIpc(const std::string &socketName, unsigned int socketPermissions,
                              const std::string &socketOwner, const std::string &socketGroup) = 0;
    virtual bool configureIpc(int32_t socketFd) = 0;
    virtual bool configureServices(const common::SessionServerState &state,
                                   const common::MaxResourceCapabilitites &maxResource,
                                   const std::string &clientDisplayName, const std::string &appName) = 0;
    virtual bool setState(const common::SessionServerState &state) = 0;
    virtual void setLogLevels(RIALTO_DEBUG_LEVEL defaultLogLevels, RIALTO_DEBUG_LEVEL clientLogLevels,
                              RIALTO_DEBUG_LEVEL sessionServerLogLevels, RIALTO_DEBUG_LEVEL ipcLogLevels,
                              RIALTO_DEBUG_LEVEL serverManagerLogLevels, RIALTO_DEBUG_LEVEL commonLogLevels) = 0;
    virtual bool ping(std::int32_t id, const std::shared_ptr<IAckSender> &ackSender) = 0;
};
} // namespace firebolt::rialto::server::service

#endif // FIREBOLT_RIALTO_SERVER_SERVICE_I_SESSION_SERVER_MANAGER_H_
