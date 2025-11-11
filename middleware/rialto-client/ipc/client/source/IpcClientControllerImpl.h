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

#ifndef FIREBOLT_RIALTO_IPC_IPC_CLIENT_CONTROLLER_IMPL_H_
#define FIREBOLT_RIALTO_IPC_IPC_CLIENT_CONTROLLER_IMPL_H_

#include <google/protobuf/service.h>

#include <cstdarg>
#include <mutex>
#include <string>

namespace firebolt::rialto::ipc
{
class ClientControllerImpl final : public google::protobuf::RpcController
{
public:
    ClientControllerImpl() = default;
    ~ClientControllerImpl() final = default;

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
    void Reset() override;

    /**
     * @brief Checks id the previosu call has failed.
     *
     * The reason for the failure depends on the implementaion of the RPC. Failed should only
     * be called after after the call has finished. If this call returns true, the response
     * structure returned from the failed call is undefined.
     *
     * @retval true if previous call failed.
     */
    bool Failed() const override;

    /**
     * @brief Returns a description of the error if the previous call has failed.
     *
     * @retval error string.
     */
    std::string ErrorText() const override;

    /**
     * @brief Starts the cancellation of a RPC call
     *
     * RPC may cancel the call immediatly, wait and cancel or not cancel at all. If a call is cancelled
     * failure will be set for the client and "done" will still be called.
     */
    void StartCancel() override;

    /**
     * Server-side methods
     * Calls can only be made from the server side. Calling these apis from client side
     * is undefined and could cause crashes.
     */

    /**
     * @brief Causes failure to be returned to the client.
     *
     * Failed() shall return true. The failure reason can be fetched from the ErrorText().
     * To return machine-readable info on failure, use the method response structure rather than
     * SetFailed().
     *
     * @param[in] reason : The reason for the falure.
     */
    void SetFailed(const std::string &reason) override { (void)reason; }

    /**
     * @brief Check if the client had cancelled the RPC.
     *
     * If true, indicates that the client canceled the RPC, so the server may
     * as well give up on replying to it.  The server should still call the
     * final "done" callback.
     *
     * @retval true of cancelled, false otherwise.
     */
    bool IsCanceled() const override { return false; }

    /**
     * @brief Request a notification when RPC is cancelled.
     *
     * The callback will always be called exactly once.  If the RPC completes without
     * being canceled, the callback will be called after completion.  If the RPC
     * has already been canceled when NotifyOnCancel() is called, the callback
     * will be called immediately.
     *
     * @param[in] callback  : Callback method on cancelled.
     */
    void NotifyOnCancel(google::protobuf::Closure *callback) override { (void)callback; }

private:
    friend class ChannelImpl;

    void setMethodCallFailed(std::string reason);
    void setMethodCallFailed(const char *format, va_list ap) __attribute__((format(printf, 2, 0)));

private:
    mutable std::mutex m_lock;
    bool m_failed = false;
    std::string m_reason;
};

} // namespace firebolt::rialto::ipc

#endif // FIREBOLT_RIALTO_IPC_IPC_CLIENT_CONTROLLER_IMPL_H_
