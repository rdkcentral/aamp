/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2022 Sky UK
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

#include "IpcServerControllerImpl.h"
#include "IpcClientImpl.h"
#include <memory>
#include <string>
#include <utility>

namespace firebolt::rialto::ipc
{
ServerControllerImpl::ServerControllerImpl(std::shared_ptr<ClientImpl> client, uint64_t serialId)
    : m_kClient(std::move(client)), m_kSerialId(serialId)
{
}

void ServerControllerImpl::SetFailed(const std::string &reason) // NOLINT(build/function_format)
{
    m_failed = true;
    m_failureReason = reason;
}

bool ServerControllerImpl::IsCanceled() const // NOLINT(build/function_format)
{
    // neither client nor server support cancel
    return false;
}

void ServerControllerImpl::NotifyOnCancel(google::protobuf::Closure *callback) // NOLINT(build/function_format)
{
    // neither client nor server support cancel
    (void)callback;
}

std::shared_ptr<IClient> ServerControllerImpl::getClient() const
{
    return m_kClient;
}

} // namespace firebolt::rialto::ipc
