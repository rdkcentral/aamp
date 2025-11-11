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

#ifndef FIREBOLT_RIALTO_CLIENT_CLIENT_CONTROLLER_H_
#define FIREBOLT_RIALTO_CLIENT_CLIENT_CONTROLLER_H_

#include <list>
#include <memory>
#include <mutex>
#include <string>

#include "IClientController.h"
#include "IControlClient.h"
#include "IControlIpc.h"

namespace firebolt::rialto::client
{
class ClientControllerAccessor : public IClientControllerAccessor
{
public:
    ~ClientControllerAccessor() override = default;
    IClientController &getClientController() const override;
};

class ClientController : public IClientController, public IControlClient
{
public:
    explicit ClientController(const std::shared_ptr<IControlIpcFactory> &ControlIpcFactory);
    ~ClientController() override;

    std::shared_ptr<ISharedMemoryHandle> getSharedMemoryHandle() override;
    bool registerClient(std::weak_ptr<IControlClient> client, ApplicationState &appState) override;
    bool unregisterClient(std::weak_ptr<IControlClient> client) override;

private:
    void notifyApplicationState(ApplicationState state) override;

    /**
     * @brief Initalised the shared memory for media playback.
     *
     * @retval true on success, false otherwise.
     */
    bool initSharedMemory();

    /**
     * @brief Terminates the shared memory.
     */
    void termSharedMemory();

    /**
     * @brief Coverts a ApplicationState to string.
     *
     * @param[in] state     : The application state.
     *
     * @retval the application state string, or "" on error.
     */
    std::string stateToString(ApplicationState state);

    /**
     * @brief Forwards new ApplicationState to subscribed clients
     *
     * @param[in] state     : The new application state.
     *
     */
    void changeStateAndNotifyClients(ApplicationState state);

private:
    /**
     * @brief Mutex protection for class attributes.
     */
    std::mutex m_mutex;

    /**
     * @brief The current application state
     */
    ApplicationState m_currentState;

    /**
     * @brief Flag indicating if registerRequest has to be sent to Rialto Server
     */
    bool m_registrationRequired;

    /**
     * @brief The shared memory buffer handle.
     */
    std::shared_ptr<ISharedMemoryHandle> m_shmHandle;

    /**
     * @brief The rialto control ipc factory.
     */
    std::shared_ptr<IControlIpc> m_controlIpc;

    /**
     * @brief List of clients to notify.
     */
    std::list<std::weak_ptr<IControlClient>> m_clients;
};
} // namespace firebolt::rialto::client

#endif // FIREBOLT_RIALTO_CLIENT_CLIENT_CONTROLLER_H_
