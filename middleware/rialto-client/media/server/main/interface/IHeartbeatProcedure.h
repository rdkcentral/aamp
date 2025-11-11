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

#ifndef FIREBOLT_RIALTO_SERVER_I_HEARTBEAT_PROCEDURE_H_
#define FIREBOLT_RIALTO_SERVER_I_HEARTBEAT_PROCEDURE_H_

/**
 * @file IHeartbeatHandler.h
 *
 * The definition of the IHeartbeatProcedure interface.
 *
 * This interface defines the ping/ack procedure sent for each client.
 *
 */

#include "IAckSender.h"
#include "IHeartbeatHandler.h"
#include <memory>

namespace firebolt::rialto::server
{
class IHeartbeatProcedure;
class IHeartbeatProcedureFactory
{
public:
    virtual ~IHeartbeatProcedureFactory() = default;

    static std::unique_ptr<IHeartbeatProcedureFactory> createFactory();
    virtual std::shared_ptr<IHeartbeatProcedure> createHeartbeatProcedure(const std::shared_ptr<IAckSender> &ackSender,
                                                                          std::int32_t pingId) const = 0;
};

class IHeartbeatProcedure
{
public:
    virtual ~IHeartbeatProcedure() = default;
    virtual std::unique_ptr<IHeartbeatHandler> createHandler() = 0;
};
} // namespace firebolt::rialto::server

#endif // FIREBOLT_RIALTO_SERVER_I_HEARTBEAT_PROCEDURE_H_
