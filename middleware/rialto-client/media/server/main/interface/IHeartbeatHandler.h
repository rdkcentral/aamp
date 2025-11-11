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

#ifndef FIREBOLT_RIALTO_SERVER_I_HEARTBEAT_HANDLER_H_
#define FIREBOLT_RIALTO_SERVER_I_HEARTBEAT_HANDLER_H_

#include <cstdint>

/**
 * @file IHeartbeatHandler.h
 *
 * The definition of the IHeartbeatHandler interface.
 *
 * This interface defines the handler of single ping/ack action.
 *
 */

namespace firebolt::rialto::server
{
class IHeartbeatHandler
{
public:
    IHeartbeatHandler() = default;
    IHeartbeatHandler(const IHeartbeatHandler &) = delete;
    IHeartbeatHandler(IHeartbeatHandler &&) = delete;
    IHeartbeatHandler &operator=(const IHeartbeatHandler &) = delete;
    IHeartbeatHandler &operator=(IHeartbeatHandler &&) = delete;
    virtual ~IHeartbeatHandler() = default;

    virtual void error() = 0;
    virtual std::int32_t id() const = 0;
};
} // namespace firebolt::rialto::server

#endif // FIREBOLT_RIALTO_SERVER_I_HEARTBEAT_HANDLER_H_
