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

#ifndef FIREBOLT_RIALTO_SERVER_SERVICE_CONTROL_SERVICE_H_
#define FIREBOLT_RIALTO_SERVER_SERVICE_CONTROL_SERVICE_H_

#include "IControlServerInternal.h"
#include "IControlService.h"
#include <map>
#include <memory>
#include <mutex>

namespace firebolt::rialto::server::service
{
class ControlService : public IControlService
{
public:
    explicit ControlService(const std::shared_ptr<IControlServerInternalFactory> &controlServerInternalFactory);
    ~ControlService() override = default;

    void addControl(int controlId, const std::shared_ptr<IControlClientServerInternal> &client) override;
    void removeControl(int controlId) override;
    bool ack(int controlId, std::uint32_t id) override;
    void setApplicationState(const ApplicationState &state) override;
    bool ping(const std::shared_ptr<IHeartbeatProcedure> &heartbeatProcedure) override;

private:
    std::mutex m_mutex;
    ApplicationState m_currentState;
    std::shared_ptr<IControlServerInternalFactory> m_controlServerInternalFactory;
    std::map<int, std::shared_ptr<IControlServerInternal>> m_controls;
};
} // namespace firebolt::rialto::server::service

#endif // FIREBOLT_RIALTO_SERVER_SERVICE_CONTROL_SERVICE_H_
