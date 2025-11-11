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

#ifndef FIREBOLT_RIALTO_SERVER_I_CONTROL_CLIENT_SERVER_INTERNAL_H_
#define FIREBOLT_RIALTO_SERVER_I_CONTROL_CLIENT_SERVER_INTERNAL_H_

/**
 * @file IControlClientServerInternal.h
 *
 * The definition of the IControlClientServerInternal interface.
 *
 * This file comprises the definition of the IControlClientServerInternal abstract
 * class. This is the API by which a IControl implementation will
 * pass notifications to its client.
 */

#include "IControlClient.h"

namespace firebolt::rialto::server
{
/**
 * @brief The Rialto control client server internal interface.
 *
 * This is server internal Rialto control client abstract base class. It should be
 * implemented by any object that sends notification about rialto server state to its client
 *
 */

class IControlClientServerInternal : public IControlClient
{
public:
    /**
     * @brief Virtual destructor
     */
    virtual ~IControlClientServerInternal() = default;

    /**
     * @brief Ping notification for checking system health
     * The client should perform any health checks then respond with
     * a call to ack(id) if system healthy
     *
     * @param[in] id  : Unique id, should be passed to corresponding ack call
     */
    virtual void ping(uint32_t id) = 0;
};
} // namespace firebolt::rialto::server

#endif // FIREBOLT_RIALTO_SERVER_I_CONTROL_CLIENT_SERVER_INTERNAL_H_
