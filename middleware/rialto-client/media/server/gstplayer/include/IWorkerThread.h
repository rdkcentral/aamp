/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2022 
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

#ifndef FIREBOLT_RIALTO_SERVER_I_WORKER_THREAD_H_
#define FIREBOLT_RIALTO_SERVER_I_WORKER_THREAD_H_

#include "IPlayerTask.h"
#include <memory>

namespace firebolt::rialto::server
{
class IWorkerThread;
class IWorkerThreadFactory
{
public:
    IWorkerThreadFactory() = default;
    virtual ~IWorkerThreadFactory() = default;

    virtual std::unique_ptr<IWorkerThread> createWorkerThread() const = 0;
};

class IWorkerThread
{
public:
    IWorkerThread() = default;
    virtual ~IWorkerThread() = default;
    IWorkerThread(const IWorkerThread &) = delete;
    IWorkerThread(IWorkerThread &&) = delete;
    IWorkerThread &operator=(const IWorkerThread &) = delete;
    IWorkerThread &operator=(IWorkerThread &&) = delete;

    /**
     * @brief Stops the task thread
     */
    virtual void stop() = 0;

    /**
     * @brief Joins the task thread
     */
    virtual void join() = 0;

    /**
     * @brief Queues a task in the task queue.
     */
    virtual void enqueueTask(std::unique_ptr<IPlayerTask> &&task) = 0;
    virtual void enqueuePriorityTask(std::unique_ptr<IPlayerTask> &&task) = 0;
};
} // namespace firebolt::rialto::server

#endif // FIREBOLT_RIALTO_SERVER_I_WORKER_THREAD_H_
