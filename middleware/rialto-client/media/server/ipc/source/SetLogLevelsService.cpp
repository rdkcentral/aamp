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

#include "SetLogLevelsService.h"
#include "mediapipelinemodule.pb.h"

namespace firebolt::rialto::server::ipc
{
void SetLogLevelsService::clientConnected(const std::shared_ptr<::firebolt::rialto::ipc::IClient> &ipcClient)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_connectedClients.insert(ipcClient);
}

void SetLogLevelsService::clientDisconnected(const std::shared_ptr<::firebolt::rialto::ipc::IClient> &ipcClient)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_connectedClients.erase(ipcClient);
}

void SetLogLevelsService::setLogLevels(RIALTO_DEBUG_LEVEL defaultLogLevels, RIALTO_DEBUG_LEVEL clientLogLevels,
                                       RIALTO_DEBUG_LEVEL ipcLogLevels, RIALTO_DEBUG_LEVEL commonLogLevels)
{
    auto event = std::make_shared<firebolt::rialto::SetLogLevelsEvent>();
    event->set_defaultloglevels(static_cast<std::uint32_t>(defaultLogLevels));
    event->set_clientloglevels(static_cast<std::uint32_t>(clientLogLevels));
    event->set_ipcloglevels(static_cast<std::uint32_t>(ipcLogLevels));
    event->set_commonloglevels(static_cast<std::uint32_t>(commonLogLevels));

    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto &client : m_connectedClients)
    {
        client->sendEvent(event);
    }
}
} // namespace firebolt::rialto::server::ipc
