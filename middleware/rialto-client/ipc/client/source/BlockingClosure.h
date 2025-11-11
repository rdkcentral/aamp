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

#ifndef FIREBOLT_RIALTO_IPC_BLOCKING_CLOSURE_H_
#define FIREBOLT_RIALTO_IPC_BLOCKING_CLOSURE_H_

#include "IBlockingClosure.h"
#include <memory>
#include <semaphore.h>

namespace firebolt::rialto::ipc
{
class BlockingClosureFactory : public IBlockingClosureFactory
{
public:
    BlockingClosureFactory() = default;
    ~BlockingClosureFactory() override = default;

    std::shared_ptr<IBlockingClosure>
    createBlockingClosurePoll(std::shared_ptr<::firebolt::rialto::ipc::IChannel> ipcChannel) override;
    std::shared_ptr<IBlockingClosure> createBlockingClosureSemaphore() override;
};

class BlockingClosurePoll final : public IBlockingClosure
{
public:
    explicit BlockingClosurePoll(std::shared_ptr<::firebolt::rialto::ipc::IChannel> ipcChannel);
    ~BlockingClosurePoll() final = default;

    void Run() final;
    void wait() final;

private:
    const std::shared_ptr<::firebolt::rialto::ipc::IChannel> m_kChannel;
    bool m_done;
};

class BlockingClosureSemaphore final : public IBlockingClosure
{
public:
    BlockingClosureSemaphore();
    ~BlockingClosureSemaphore() final;

    void Run() final;
    void wait() final;

private:
    sem_t m_sem;
};
}; // namespace firebolt::rialto::ipc

#endif // FIREBOLT_RIALTO_IPC_BLOCKING_CLOSURE_H_
