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

#ifndef FIREBOLT_RIALTO_SERVER_SERVICE_I_CONTROL_SERVICE_H_
#define FIREBOLT_RIALTO_SERVER_SERVICE_I_CONTROL_SERVICE_H_

#include "ControlCommon.h"
#include "IControlClientServerInternal.h"
#include "IHeartbeatProcedure.h"
#include <cstdint>
#include <memory>

namespace firebolt::rialto::server::service
{
class IControlService
{
public:
    IControlService() = default;
    virtual ~IControlService() = default;

    IControlService(const IControlService &) = delete;
    IControlService(IControlService &&) = delete;
    IControlService &operator=(const IControlService &) = delete;
    IControlService &operator=(IControlService &&) = delete;

    virtual void addControl(int controlId, const std::shared_ptr<IControlClientServerInternal> &client) = 0;
    virtual void removeControl(int controlId) = 0;
    virtual bool ack(int controlId, std::uint32_t id) = 0;
    virtual void setApplicationState(const ApplicationState &state) = 0;
    virtual bool ping(const std::shared_ptr<IHeartbeatProcedure> &heartbeatProcedure) = 0;
};
} // namespace firebolt::rialto::server::service

#endif // FIREBOLT_RIALTO_SERVER_SERVICE_I_CONTROL_SERVICE_H_
