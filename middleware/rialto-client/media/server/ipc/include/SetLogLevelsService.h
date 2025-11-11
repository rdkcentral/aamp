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

#ifndef FIREBOLT_RIALTO_SERVER_IPC_SET_LOG_LEVELS_SERVICE_H_
#define FIREBOLT_RIALTO_SERVER_IPC_SET_LOG_LEVELS_SERVICE_H_

#include <memory>
#include <mutex>
#include <set>

#include "IIpcServer.h"
#include "RialtoLogging.h"

namespace firebolt::rialto::server::ipc
{
class SetLogLevelsService
{
public:
    SetLogLevelsService() = default;
    ~SetLogLevelsService() = default;
    SetLogLevelsService(const SetLogLevelsService &) = delete;
    SetLogLevelsService(SetLogLevelsService &&) = delete;
    SetLogLevelsService &operator=(const SetLogLevelsService &) = delete;
    SetLogLevelsService &operator=(SetLogLevelsService &&) = delete;

    void clientConnected(const std::shared_ptr<::firebolt::rialto::ipc::IClient> &ipcClient);
    void clientDisconnected(const std::shared_ptr<::firebolt::rialto::ipc::IClient> &ipcClient);
    void setLogLevels(RIALTO_DEBUG_LEVEL defaultLogLevels, RIALTO_DEBUG_LEVEL clientLogLevels,
                      RIALTO_DEBUG_LEVEL ipcLogLevels, RIALTO_DEBUG_LEVEL commonLogLevels);

private:
    std::mutex m_mutex;
    std::set<std::shared_ptr<::firebolt::rialto::ipc::IClient>> m_connectedClients;
};
} // namespace firebolt::rialto::server::ipc

#endif // FIREBOLT_RIALTO_SERVER_IPC_SET_LOG_LEVELS_SERVICE_H_
