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

#include "HeartbeatProcedure.h"

namespace firebolt::rialto::server
{
std::unique_ptr<IHeartbeatProcedureFactory> IHeartbeatProcedureFactory::createFactory()
{
    return std::make_unique<HeartbeatProcedureFactory>();
}

std::shared_ptr<IHeartbeatProcedure>
HeartbeatProcedureFactory::createHeartbeatProcedure(const std::shared_ptr<IAckSender> &ackSender, std::int32_t pingId) const
{
    return std::make_shared<HeartbeatProcedure>(ackSender, pingId);
}

HeartbeatProcedure::HeartbeatHandler::HeartbeatHandler(const std::shared_ptr<HeartbeatProcedure> &procedure,
                                                       std::int32_t pingId)
    : m_procedure{procedure}, m_kPingId{pingId}, m_success{true}
{
}

HeartbeatProcedure::HeartbeatHandler::~HeartbeatHandler()
{
    m_procedure->onFinish(m_success);
}

void HeartbeatProcedure::HeartbeatHandler::error()
{
    m_success = false;
}

std::int32_t HeartbeatProcedure::HeartbeatHandler::id() const
{
    return m_kPingId;
}

HeartbeatProcedure::HeartbeatProcedure(const std::shared_ptr<IAckSender> &ackSender, std::int32_t pingId)
    : m_ackSender{ackSender}, m_kPingId{pingId}, m_success{true}
{
}

HeartbeatProcedure::~HeartbeatProcedure()
{
    m_ackSender->send(m_kPingId, m_success);
}

std::unique_ptr<IHeartbeatHandler> HeartbeatProcedure::createHandler()
{
    return std::make_unique<HeartbeatHandler>(shared_from_this(), m_kPingId);
}

void HeartbeatProcedure::onFinish(bool success)
{
    if (!success)
    {
        m_success = false;
    }
}
} // namespace firebolt::rialto::server
