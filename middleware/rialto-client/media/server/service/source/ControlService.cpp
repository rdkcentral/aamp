/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2023 
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

#include "ControlService.h"
#include "IHeartbeatProcedure.h"
#include "RialtoServerLogging.h"
#include <utility>

namespace firebolt::rialto::server::service
{
ControlService::ControlService(const std::shared_ptr<IControlServerInternalFactory> &controlServerInternalFactory)
    : m_currentState{ApplicationState::UNKNOWN}, m_controlServerInternalFactory{controlServerInternalFactory}
{
    RIALTO_SERVER_LOG_DEBUG("entry:");
}

void ControlService::addControl(int controlId, const std::shared_ptr<IControlClientServerInternal> &client)
{
    RIALTO_SERVER_LOG_INFO("Creating new Control with id: %d", controlId);
    auto controlServerInternal{m_controlServerInternalFactory->createControlServerInternal(controlId, client)};
    std::unique_lock<std::mutex> lock{m_mutex};
    controlServerInternal->setApplicationState(m_currentState);
    m_controls.emplace(controlId, controlServerInternal);
}

void ControlService::removeControl(int controlId)
{
    RIALTO_SERVER_LOG_INFO("Removing Control with id: %d", controlId);
    std::unique_lock<std::mutex> lock{m_mutex};
    m_controls.erase(controlId);
}

bool ControlService::ack(int controlId, std::uint32_t id)
{
    std::unique_lock<std::mutex> lock{m_mutex};
    auto controlIter = m_controls.find(controlId);
    if (m_controls.end() == controlIter)
    {
        RIALTO_SERVER_LOG_ERROR("Control with id: %d not found", controlId);
        return false;
    }
    controlIter->second->ack(id);
    return true;
}

void ControlService::setApplicationState(const ApplicationState &state)
{
    std::unique_lock<std::mutex> lock{m_mutex};
    m_currentState = state;
    for (const auto &control : m_controls)
    {
        control.second->setApplicationState(state);
    }
}

bool ControlService::ping(const std::shared_ptr<IHeartbeatProcedure> &heartbeatProcedure)
{
    std::unique_lock<std::mutex> lock{m_mutex};
    for (const auto &control : m_controls)
    {
        control.second->ping(heartbeatProcedure->createHandler());
    }
    return true;
}
} // namespace firebolt::rialto::server::service
