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

#include "BlockingClosure.h"
#include "IpcLogging.h"

#include <memory>
#include <unistd.h>
#include <utility>

namespace firebolt::rialto::ipc
{
std::shared_ptr<IBlockingClosureFactory> IBlockingClosureFactory::createFactory()
{
    std::shared_ptr<IBlockingClosureFactory> factory;
    try
    {
        factory = std::make_shared<BlockingClosureFactory>();
    }
    catch (const std::exception &e)
    {
        RIALTO_IPC_LOG_ERROR("Failed to create the blocking closure factory, reason: %s", e.what());
    }

    return factory;
}

std::shared_ptr<IBlockingClosure>
BlockingClosureFactory::createBlockingClosurePoll(std::shared_ptr<::firebolt::rialto::ipc::IChannel> ipcChannel)
{
    std::shared_ptr<IBlockingClosure> blockingClosure;
    try
    {
        blockingClosure = std::make_shared<BlockingClosurePoll>(ipcChannel);
    }
    catch (const std::exception &e)
    {
        RIALTO_IPC_LOG_ERROR("Failed to create the blocking closure, reason: %s", e.what());
    }

    return blockingClosure;
}

std::shared_ptr<IBlockingClosure> BlockingClosureFactory::createBlockingClosureSemaphore()
{
    std::shared_ptr<IBlockingClosure> blockingClosure;
    try
    {
        blockingClosure = std::make_shared<BlockingClosureSemaphore>();
    }
    catch (const std::exception &e)
    {
        RIALTO_IPC_LOG_ERROR("Failed to create the blocking closure, reason: %s", e.what());
    }

    return blockingClosure;
}

BlockingClosurePoll::BlockingClosurePoll(std::shared_ptr<::firebolt::rialto::ipc::IChannel> ipcChannel)
    : m_kChannel(std::move(ipcChannel)), m_done(false)
{
}

void BlockingClosurePoll::Run() // NOLINT(build/function_format)
{
    m_done = true;
}

void BlockingClosurePoll::wait()
{
    while (m_kChannel->process() && !m_done)
    {
        m_kChannel->wait(-1);
    }
}

BlockingClosureSemaphore::BlockingClosureSemaphore() : m_sem{{0}}
{
    sem_init(&m_sem, 0, 0);
}

BlockingClosureSemaphore::~BlockingClosureSemaphore()
{
    sem_destroy(&m_sem);
}

void BlockingClosureSemaphore::Run() // NOLINT(build/function_format)
{
    if (sem_post(&m_sem) != 0)
        RIALTO_IPC_LOG_SYS_ERROR(errno, "failed to signal semaphore");
}

void BlockingClosureSemaphore::wait()
{
    TEMP_FAILURE_RETRY(sem_wait(&m_sem));
}
}; // namespace firebolt::rialto::ipc
