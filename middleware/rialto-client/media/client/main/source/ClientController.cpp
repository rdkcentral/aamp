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

#include "ClientController.h"
#include "RialtoClientLogging.h"
#include "SharedMemoryHandle.h"
#include <cstring>
#include <stdexcept>
#include <sys/mman.h>
#include <sys/un.h>
#include <unistd.h>

namespace
{
// The following error would be reported if a client is deleted
// before unregisterClient() was called. Calling unregisterClient can
// be automated via a proxy class (like the class MediaPipelineProxy)
const std::string kClientPointerNotLocked{"A client could not be locked"};
}; // namespace

namespace firebolt::rialto::client
{
IClientControllerAccessor &IClientControllerAccessor::instance()
{
    static ClientControllerAccessor factory;
    return factory;
}

IClientController &ClientControllerAccessor::getClientController() const
{
    static ClientController ClientController{IControlIpcFactory::createFactory()};
    return ClientController;
}

ClientController::ClientController(const std::shared_ptr<IControlIpcFactory> &ControlIpcFactory)
    : m_currentState{ApplicationState::UNKNOWN}, m_registrationRequired{true}
{
    RIALTO_CLIENT_LOG_DEBUG("entry:");

    const char kSrcRev[] = SRCREV;
    const char kTags[] = TAGS;

    if (std::strlen(kSrcRev) > 0)
    {
        if (std::strlen(kTags) > 0)
        {
            RIALTO_CLIENT_LOG_MIL("Release Tag(s): %s (Commit ID: %s)", kTags, kSrcRev);
        }
        else
        {
            RIALTO_CLIENT_LOG_MIL("Release Tag(s): No Release Tags! (Commit ID: %s)", kSrcRev);
        }
    }
    else
    {
        RIALTO_CLIENT_LOG_WARN("Failed to get git commit ID!");
    }

    m_controlIpc = ControlIpcFactory->createControlIpc(this);
    if (nullptr == m_controlIpc)
    {
        throw std::runtime_error("Failed to create the ControlIpc object");
    }
}

ClientController::~ClientController()
{
    RIALTO_CLIENT_LOG_DEBUG("entry:");

    termSharedMemory();
}

std::shared_ptr<ISharedMemoryHandle> ClientController::getSharedMemoryHandle()
{
    std::lock_guard<std::mutex> lock{m_mutex};
    return m_shmHandle;
}

bool ClientController::registerClient(std::weak_ptr<IControlClient> client, ApplicationState &appState)
{
    std::shared_ptr<IControlClient> clientLocked = client.lock();
    if (!clientLocked)
    {
        RIALTO_CLIENT_LOG_ERROR("Client ptr is null");
        return false;
    }

    std::lock_guard<std::mutex> lock{m_mutex};
    if (m_registrationRequired)
    {
        if (!m_controlIpc->registerClient())
        {
            RIALTO_CLIENT_LOG_ERROR("Failed to register client");
            return false;
        }
    }
    m_registrationRequired = false;

    bool alreadyRegistered{std::find_if(m_clients.begin(), m_clients.end(),
                                        [&](auto &i)
                                        {
                                            std::shared_ptr<IControlClient> iLocked = i.lock();
                                            return (iLocked == clientLocked);
                                        }) != m_clients.end()};
    if (!alreadyRegistered)
        m_clients.push_back(client);
    appState = m_currentState;

    return true;
}

bool ClientController::unregisterClient(std::weak_ptr<IControlClient> client)
{
    std::shared_ptr<IControlClient> clientLocked = client.lock();
    if (!clientLocked)
    {
        RIALTO_CLIENT_LOG_ERROR("Client ptr is null");
        return false;
    }

    std::lock_guard<std::mutex> lock{m_mutex};

    bool found{false};
    for (auto i = m_clients.begin(); i != m_clients.end();)
    {
        std::shared_ptr<IControlClient> iLocked = i->lock();
        if (!iLocked)
        {
            RIALTO_CLIENT_LOG_ERROR("%s", kClientPointerNotLocked.c_str());
            i = m_clients.erase(i);
        }
        else if (iLocked == clientLocked)
        {
            i = m_clients.erase(i);
            found = true;
            break;
        }
        else
            ++i;
    }

    if (!found)
    {
        RIALTO_CLIENT_LOG_ERROR("Client not found");
        return false;
    }

    return true;
}

bool ClientController::initSharedMemory()
try
{
    std::lock_guard<std::mutex> lock{m_mutex};
    int32_t shmFd{-1};
    uint32_t shmBufferLen{0U};
    if (!m_controlIpc->getSharedMemory(shmFd, shmBufferLen))
    {
        RIALTO_CLIENT_LOG_ERROR("Failed to get the shared memory");
        return false;
    }
    m_shmHandle = std::make_shared<SharedMemoryHandle>(shmFd, shmBufferLen);

    RIALTO_CLIENT_LOG_INFO("Shared buffer was successfully initialised");
    return true;
}
catch (const std::exception &e)
{
    RIALTO_CLIENT_LOG_ERROR("Failed to initialise shared memory: %s", e.what());
    return false;
}

void ClientController::termSharedMemory()
{
    std::lock_guard<std::mutex> lock{m_mutex};
    m_shmHandle.reset();
}

void ClientController::notifyApplicationState(ApplicationState state)
{
    {
        std::lock_guard<std::mutex> lock{m_mutex};
        if (ApplicationState::UNKNOWN == state)
        {
            RIALTO_CLIENT_LOG_DEBUG("Application state changed to unknown. Client will have to register next time");
            m_registrationRequired = true;
        }
        if (m_currentState == state)
        {
            RIALTO_CLIENT_LOG_WARN("Rialto application state already set, %s", stateToString(m_currentState).c_str());
            return;
        }
    }

    switch (state)
    {
    case ApplicationState::RUNNING:
    {
        if (!initSharedMemory())
        {
            RIALTO_CLIENT_LOG_ERROR("Could not initalise the shared memory");
            return;
        }
        // Inform clients after memory initialisation
        changeStateAndNotifyClients(state);
        break;
    }
    case ApplicationState::INACTIVE:
    case ApplicationState::UNKNOWN:
    {
        // Inform clients before memory termination
        changeStateAndNotifyClients(state);
        termSharedMemory();
        break;
    }
    }
}

std::string ClientController::stateToString(ApplicationState state)
{
    switch (state)
    {
    case ApplicationState::RUNNING:
    {
        return "RUNNING";
    }
    case ApplicationState::INACTIVE:
    {
        return "INACTIVE";
    }
    case ApplicationState::UNKNOWN:
    default:
    {
        return "UNKNOWN";
    }
    }
}

void ClientController::changeStateAndNotifyClients(ApplicationState state)
{
    std::vector<std::shared_ptr<IControlClient>> currentClients;
    {
        std::lock_guard<std::mutex> lock{m_mutex};
        RIALTO_CLIENT_LOG_MIL("Rialto application state changed from %s to %s", stateToString(m_currentState).c_str(),
                              stateToString(state).c_str());
        m_currentState = state;
        for (const std::weak_ptr<IControlClient> &client : m_clients)
        {
            std::shared_ptr<IControlClient> clientLocked{client.lock()};
            if (clientLocked)
            {
                currentClients.push_back(clientLocked);
            }
            else
            {
                RIALTO_CLIENT_LOG_ERROR("%s", kClientPointerNotLocked.c_str());
            }
        }
    }
    for (const auto &client : currentClients)
    {
        client->notifyApplicationState(state);
    }
}
} // namespace firebolt::rialto::client
