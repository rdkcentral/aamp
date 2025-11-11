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

#ifndef FIREBOLT_RIALTO_IPC_I_BLOCKING_CLOSURE_H_
#define FIREBOLT_RIALTO_IPC_I_BLOCKING_CLOSURE_H_

#include "IIpcChannel.h"
#include <memory>

namespace firebolt::rialto::ipc
{
class IBlockingClosure;

class IBlockingClosureFactory
{
public:
    IBlockingClosureFactory() = default;
    virtual ~IBlockingClosureFactory() = default;

    /**
     * @brief Create a IBlockingClosureFactory instance.
     *
     * @retval the factory instance or null on error.
     */
    static std::shared_ptr<IBlockingClosureFactory> createFactory();

    /**
     * @brief Creates a polling IBlockingClosure object.
     * Pump the event loop from within the wait() method.
     *
     * @param[in] ipcChannel  : The ipc channel to wait on.
     *
     * @retval the protobuf controller instance or null on error.
     */
    virtual std::shared_ptr<IBlockingClosure>
    createBlockingClosurePoll(std::shared_ptr<::firebolt::rialto::ipc::IChannel> ipcChannel) = 0;

    /**
     * @brief Creates a semaphore IBlockingClosure object.
     * Do not pump the event loop from within the wait() method.
     *
     * @retval the protobuf controller instance or null on error.
     */
    virtual std::shared_ptr<IBlockingClosure> createBlockingClosureSemaphore() = 0;
};

class IBlockingClosure : public google::protobuf::Closure
{
public:
    IBlockingClosure() = default;
    virtual ~IBlockingClosure() = default;

    IBlockingClosure(const IBlockingClosure &) = delete;
    IBlockingClosure &operator=(const IBlockingClosure &) = delete;
    IBlockingClosure(IBlockingClosure &&) = delete;
    IBlockingClosure &operator=(IBlockingClosure &&) = delete;

    /**
     * @brief Wait for the response.
     */
    virtual void wait() = 0;
};

}; // namespace firebolt::rialto::ipc

#endif // FIREBOLT_RIALTO_IPC_I_BLOCKING_CLOSURE_H_
