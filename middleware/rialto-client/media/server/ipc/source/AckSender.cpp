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

#include "AckSender.h"
#include "RialtoServerLogging.h"
#include "servermanagermodule.pb.h"

namespace firebolt::rialto::server::ipc
{
AckSender::AckSender(const std::shared_ptr<::firebolt::rialto::ipc::IClient> &ipcClient) : m_ipcClient{ipcClient} {}

void AckSender::send(int id, bool success) const
{
    RIALTO_SERVER_LOG_DEBUG("Sending AckEvent with id %d", id);

    auto event = std::make_shared<::rialto::AckEvent>();
    event->set_id(id);
    event->set_success(success);

    m_ipcClient->sendEvent(event);
}
} // namespace firebolt::rialto::server::ipc
