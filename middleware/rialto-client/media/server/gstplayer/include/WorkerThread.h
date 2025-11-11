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

#ifndef FIREBOLT_RIALTO_SERVER_WORKER_THREAD_H_
#define FIREBOLT_RIALTO_SERVER_WORKER_THREAD_H_

#include "IWorkerThread.h"
#include "tasks/IPlayerTask.h"
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>

namespace firebolt::rialto::server
{
class WorkerThreadFactory : public IWorkerThreadFactory
{
public:
    std::unique_ptr<IWorkerThread> createWorkerThread() const override;
};

class WorkerThread : public IWorkerThread
{
public:
    WorkerThread();
    ~WorkerThread() override;

    void stop() override;
    void join() override;
    void enqueueTask(std::unique_ptr<IPlayerTask> &&task) override;
    void enqueuePriorityTask(std::unique_ptr<IPlayerTask> &&task) override;

private:
    /**
     * @brief For handling new tasks in the worker thread.
     */
    void taskHandler();

    /**
     * @brief Gets the next task or wait for a task on the queue.
     *
     * @retval Next task to process.
     */
    std::unique_ptr<IPlayerTask> waitForTask();

private:
    /**
     * @brief Flag used to check, if task thread is active
     */
    bool m_isTaskThreadActive{true};

    /**
     * @brief Thread for handling player tasks.
     */
    std::thread m_taskThread{};

    /**
     * @brief Mutex to protect the task handling.
     */
    std::mutex m_taskMutex{};

    /**
     * @brief New task condition varaible.
     */
    std::condition_variable m_taskCV{};

    /**
     * @brief Queue to store new tasks.
     */
    std::deque<std::unique_ptr<IPlayerTask>> m_taskQueue{};
};
} // namespace firebolt::rialto::server

#endif // FIREBOLT_RIALTO_SERVER_WORKER_THREAD_H_
