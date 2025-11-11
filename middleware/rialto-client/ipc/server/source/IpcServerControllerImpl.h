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

#ifndef FIREBOLT_RIALTO_IPC_IPC_SERVER_CONTROLLER_IMPL_H_
#define FIREBOLT_RIALTO_IPC_IPC_SERVER_CONTROLLER_IMPL_H_

#include "IIpcController.h"
#include "IpcClientImpl.h"
#include <memory>
#include <string>

namespace firebolt::rialto::ipc
{
/**
 * @brief Implementation of the RialtoIpcServerController interface, a pointer to an
 * object of this type is supplied in each RPC call handler.
 *
 * This object is inherited from RialtoIpcServerController interface which in turn
 * is inherited from google::protobuf::RpcController, and therefore meets the
 * requirements of the protobuf RPC service handler.
 *
 * It implements the google::protobuf::RpcController::SetFailed(...) method
 * which allows the server to return a failure on an RPC call.
 *
 * It also implements the RialtoIpcController::getClient(...) method which
 * allows for returning a pointer to the RialtoIpc client that made the request.
 * This is an extension to the google::protobuf::RpcController interface.
 * If clients need this API then they are expected to perform a dynamic_cast
 * on the controller pointer to a RialtoIpcController pointer.
 */
class ServerControllerImpl final : public IController
{
public:
    ~ServerControllerImpl() final = default;

public:
    /**
     * Ignore Client-side methods
     */
    void Reset() final {}
    bool Failed() const final { return false; }
    std::string ErrorText() const final { return std::string(); }
    void StartCancel() final {}

public:
    /**
     * Server-side methods
     */
    void SetFailed(const std::string &reason) final;
    bool IsCanceled() const final;
    void NotifyOnCancel(google::protobuf::Closure *callback) final;
    std::shared_ptr<IClient> getClient() const final;

protected:
    friend class ServerImpl;
    ServerControllerImpl(std::shared_ptr<ClientImpl> client, uint64_t serialId);

    const std::shared_ptr<ClientImpl> m_kClient;
    const uint64_t m_serialId{};
    const uint64_t m_kSerialId;

    bool m_failed = false;
    std::string m_failureReason;
};

} // namespace firebolt::rialto::ipc

#endif // FIREBOLT_RIALTO_IPC_IPC_SERVER_CONTROLLER_IMPL_H_
