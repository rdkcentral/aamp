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

#ifndef FIREBOLT_RIALTO_IPC_I_IPC_CONTROLLER_H_
#define FIREBOLT_RIALTO_IPC_I_IPC_CONTROLLER_H_

#include "IIpcServer.h"
#include <google/protobuf/service.h>

#include <map>
#include <memory>
#include <string>

namespace firebolt::rialto::ipc
{
/**
 * @brief Controller interface for the protobuf RPC service stubs.
 *
 * A pointer to this interface will be supplied as the first argument in every
 * RPC stub call.  It is an extensions of the google::protobuf::RpcController
 * interface and provides an additional method to get the client that made the
 * RPC call.
 */
class IController : public google::protobuf::RpcController
{
public:
    IController() = default;
    virtual ~IController() = default;

    IController(const IController &) = delete;
    IController &operator=(const IController &) = delete;
    IController(IController &&) = delete;
    IController &operator=(IController &&) = delete;

public:
    /**
     * Client-side methods
     * Calls can only be made from the client side. Calling these apis from server side
     * is undefined and could cause crashes.
     */

    /**
     * @brief Reset the RpcController.
     *
     * So that the RpcController can be reused for a new call, sets to the inital state.
     */
    void Reset() override = 0;

    /**
     * @brief Checks id the previosu call has failed.
     *
     * The reason for the failure depends on the implementaion of the RPC. Failed should only
     * be called after after the call has finished. If this call returns true, the response
     * structure returned from the failed call is undefined.
     *
     * @retval true if previous call failed.
     */
    bool Failed() const override = 0;

    /**
     * @brief Returns a description of the error if the previous call has failed.
     *
     * @retval error string.
     */
    std::string ErrorText() const override = 0;

    /**
     * @brief Starts the cancellation of a RPC call
     *
     * RPC may cancel the call immediatly, wait and cancel or not cancel at all. If a call is cancelled
     * failure will be set for the client and "done" will still be called.
     */
    void StartCancel() override = 0;

    /**
     * Server-side methods
     * Calls can only be made from the server side. Calling these apis from client side
     * is undefined and could cause crashes.
     */

    /**
     * @brief To be called during a method call. Returns a ptr to the client that made the call.
     *
     * @retval client that made the call.
     */
    virtual std::shared_ptr<IClient> getClient() const = 0;

    /**
     * @brief Causes failure to be returned to the client.
     *
     * Failed() shall return true. The failure reason can be fetched from the ErrorText().
     * To return machine-readable info on failure, use the method response structure rather than
     * SetFailed().
     *
     * @param[in] reason : The reason for the falure.
     */
    void SetFailed(const std::string &reason) override = 0;

    /**
     * @brief Not supported.
     *
     * @retval false.
     */
    bool IsCanceled() const override = 0;

    /**
     * @brief Not supported.
     *
     * @param[in] callback : Callback on closure.
     */
    void NotifyOnCancel(google::protobuf::Closure *callback) override = 0;
};

} // namespace firebolt::rialto::ipc

#endif // FIREBOLT_RIALTO_IPC_I_IPC_CONTROLLER_H_
