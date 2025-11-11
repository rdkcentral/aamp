/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2024 
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

#ifndef FIREBOLT_RIALTO_CLIENT_I_CLIENT_CONTROLLER_H_
#define FIREBOLT_RIALTO_CLIENT_I_CLIENT_CONTROLLER_H_

#include <functional>
#include <memory>
#include <stdint.h>

#include "IClientLogHandler.h"
#include "IControlClient.h"
#include "ISharedMemoryHandle.h"

namespace firebolt::rialto::client
{
class IClientController;

/**
 * @brief IClientController accessor class definition.
 */
class IClientControllerAccessor
{
public:
    virtual ~IClientControllerAccessor() = default;
    IClientControllerAccessor(const IClientControllerAccessor &) = delete;
    IClientControllerAccessor &operator=(const IClientControllerAccessor &) = delete;
    IClientControllerAccessor(IClientControllerAccessor &&) = delete;
    IClientControllerAccessor &operator=(IClientControllerAccessor &&) = delete;

    /**
     * @brief Get a IClientControllerAccessor instance.
     *
     * @retval the accessor instance
     */
    static IClientControllerAccessor &instance();

    /**
     * @brief Get ClientController object.
     *
     * @retval the reference to ClientController singleton object
     */
    virtual IClientController &getClientController() const = 0;

protected:
    IClientControllerAccessor() = default;
};

/**
 * @brief The definition of the IClientController interface.
 *
 * This interface defines the internal API querying shared memory.
 */
class IClientController
{
public:
    IClientController() = default;
    virtual ~IClientController() = default;

    IClientController(const IClientController &) = delete;
    IClientController &operator=(const IClientController &) = delete;
    IClientController(IClientController &&) = delete;
    IClientController &operator=(IClientController &&) = delete;

    /**
     * @brief Gets the handle to the mapped shared memory.
     *
     * @retval shared pointer to shm handle.
     */
    virtual std::shared_ptr<ISharedMemoryHandle> getSharedMemoryHandle() = 0;

    /**
     * @brief Register a client notify when the shared buffer changes.
     *
     * @param[in]  client   : Client to register.
     * @param[out] appState : Current application state
     *
     * @retval true on success, false otherwise.
     */
    virtual bool registerClient(std::weak_ptr<IControlClient> client, ApplicationState &appState) = 0;

    /**
     * @brief Unregister a client.
     *
     * @param[in] client    : Client to unregister.
     *
     * @retval true on success, false otherwise.
     */
    virtual bool unregisterClient(std::weak_ptr<IControlClient> client) = 0;
};

}; // namespace firebolt::rialto::client

#endif // FIREBOLT_RIALTO_CLIENT_I_CLIENT_CONTROLLER_H_
