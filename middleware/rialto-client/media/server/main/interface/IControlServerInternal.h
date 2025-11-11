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

#ifndef FIREBOLT_RIALTO_SERVER_I_CONTROL_SERVER_INTERNAL_H_
#define FIREBOLT_RIALTO_SERVER_I_CONTROL_SERVER_INTERNAL_H_

/**
 * @file IControlServerInternal.h
 *
 * The definition of the IControlServerInternal interface.
 *
 * This interface defines the server internal API of Rialto for controlling rialto clients.
 */

#include "IControl.h"
#include "IControlClientServerInternal.h"
#include "IHeartbeatHandler.h"
#include <memory>

namespace firebolt::rialto::server
{
class IControlServerInternal;
class IControlServerInternalFactory : public IControlFactory
{
public:
    IControlServerInternalFactory() = default;
    ~IControlServerInternalFactory() override = default;

    /**
     * @brief Create a IControlServerInternalFactory instance.
     *
     * @retval the factory instance or null on error.
     */
    static std::shared_ptr<IControlServerInternalFactory> createFactory();

    /**
     * @brief IControlServerInternal factory method, returns a concrete implementation of IControlServerInternal
     *
     * @param[in]  id       : Control id
     * @param[in]  client   : Client object for callbacks
     *
     * @retval the new IControlServerInternal instance or null on error.
     */
    virtual std::shared_ptr<IControlServerInternal>
    createControlServerInternal(int id, const std::shared_ptr<IControlClientServerInternal> &client) const = 0;
};

class IControlServerInternal : public IControl
{
public:
    IControlServerInternal() = default;
    virtual ~IControlServerInternal() = default;

    /**
     * @brief Informs connected rialto client about rialto server state change
     *
     * @param[in] state: The new application state.
     */
    virtual void setApplicationState(const ApplicationState &state) = 0;

    /**
     * @brief Acknowledgement of a received ping request
     *
     * @param[in] id  : id received in ping notification
     */
    virtual void ack(int32_t id) = 0;

    /**
     * @brief Ping notification for checking system health
     * The client should perform any health checks then respond with
     * a call to ack(id) if system healthy
     *
     * @param[in] heartbeatHandler  : Handler of current heartbeat request
     */
    virtual void ping(std::unique_ptr<IHeartbeatHandler> &&heartbeatHandler) = 0;
};
} // namespace firebolt::rialto::server

#endif // FIREBOLT_RIALTO_SERVER_I_CONTROL_SERVER_INTERNAL_H_
