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

#include "ControlServerInternal.h"
#include "RialtoServerLogging.h"
#include <stdexcept>

namespace
{
const char *convertApplicationState(const firebolt::rialto::ApplicationState &appState)
{
    switch (appState)
    {
    case firebolt::rialto::ApplicationState::UNKNOWN:
        return "UNKNOWN";
    case firebolt::rialto::ApplicationState::RUNNING:
        return "RUNNING";
    case firebolt::rialto::ApplicationState::INACTIVE:
        return "INACTIVE";
    }
    return "UNKNOWN";
}
} // namespace

namespace firebolt::rialto
{
std::shared_ptr<IControlFactory> IControlFactory::createFactory()
{
    return server::IControlServerInternalFactory::createFactory();
}
} // namespace firebolt::rialto

namespace firebolt::rialto::server
{
std::shared_ptr<IControlServerInternalFactory> IControlServerInternalFactory::createFactory()
try
{
    return std::make_shared<ControlServerInternalFactory>();
}
catch (std::exception &e)
{
    RIALTO_SERVER_LOG_ERROR("ControlServerInternalFactory creation failed");
    return nullptr;
}

std::shared_ptr<IControl> ControlServerInternalFactory::createControl() const
{
    RIALTO_SERVER_LOG_ERROR("This function can't be used by rialto server. Please use createControlServerInternal");
    return nullptr;
}

std::shared_ptr<IControlServerInternal> ControlServerInternalFactory::createControlServerInternal(
    int id, const std::shared_ptr<IControlClientServerInternal> &client) const
try
{
    return std::make_shared<ControlServerInternal>(server::IMainThreadFactory::createFactory(), id, client);
}
catch (const std::exception &e)
{
    RIALTO_SERVER_LOG_ERROR("ControlServerInternal creation failed");
    return nullptr;
}

ControlServerInternal::ControlServerInternal(const std::shared_ptr<IMainThreadFactory> &mainThreadFactory, int id,
                                             const std::shared_ptr<IControlClientServerInternal> &client)
    : m_controlId{id}, m_client{client}, m_currentState{ApplicationState::UNKNOWN}
{
    RIALTO_SERVER_LOG_DEBUG("entry:");
    m_mainThread = mainThreadFactory->getMainThread();
    if (!m_mainThread)
    {
        throw std::runtime_error("Failed to get the main thread");
    }
    m_mainThreadClientId = m_mainThread->registerClient();
}

ControlServerInternal::~ControlServerInternal()
{
    RIALTO_SERVER_LOG_DEBUG("entry:");
    m_mainThread->unregisterClient(m_mainThreadClientId);
}

void ControlServerInternal::ack(int32_t ackId)
{
    RIALTO_SERVER_LOG_DEBUG("Control with id: %d received ack for ping: %d", m_controlId, ackId);
    auto task = [&]()
    {
        if (!m_heartbeatHandler)
        {
            RIALTO_SERVER_LOG_WARN("No heartbeatHandler present for control with id: %d", m_controlId);
            return;
        }
        if (m_heartbeatHandler->id() != ackId)
        {
            RIALTO_SERVER_LOG_WARN("Control with id: %d received ack with wrong id: %d", m_controlId, ackId);
            return;
        }
        m_heartbeatHandler.reset();
    };
    m_mainThread->enqueueTaskAndWait(m_mainThreadClientId, task);
}

void ControlServerInternal::setApplicationState(const ApplicationState &state)
{
    RIALTO_SERVER_LOG_MIL("Notify rialto client about state changed to: %s", convertApplicationState(state));
    auto task = [&]()
    {
        m_currentState = state;
        if (m_client)
            m_client->notifyApplicationState(state);
    };
    m_mainThread->enqueueTaskAndWait(m_mainThreadClientId, task);
}

void ControlServerInternal::ping(std::unique_ptr<IHeartbeatHandler> &&heartbeatHandler)
{
    RIALTO_SERVER_LOG_DEBUG("Control with id: %d will send ping with id: %d", m_controlId, heartbeatHandler->id());
    auto task = [&]()
    {
        if (m_currentState != ApplicationState::RUNNING)
        {
            RIALTO_SERVER_LOG_DEBUG("Control with id: %d in INACTIVE state. Ack immediately for ping with id: %d",
                                    m_controlId, heartbeatHandler->id());
            m_heartbeatHandler.reset();
            heartbeatHandler.reset();
            return;
        }
        if (m_heartbeatHandler)
        {
            m_heartbeatHandler->error();
            m_heartbeatHandler.reset();
        }
        if (m_client)
        {
            m_client->ping(heartbeatHandler->id());
            m_heartbeatHandler = std::move(heartbeatHandler);
        }
    };
    m_mainThread->enqueueTaskAndWait(m_mainThreadClientId, task);
}

bool ControlServerInternal::registerClient(std::weak_ptr<IControlClient> client, ApplicationState &appState)
{
    RIALTO_SERVER_LOG_ERROR("Method not implemented");
    return false;
}
} // namespace firebolt::rialto::server
