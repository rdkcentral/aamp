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

#ifndef FIREBOLT_RIALTO_SERVER_MAIN_THREAD_H_
#define FIREBOLT_RIALTO_SERVER_MAIN_THREAD_H_

#include "IMainThread.h"
#include <atomic>
#include <condition_variable>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>

namespace firebolt::rialto::server
{
/**
 * @brief IMainThread factory class definition.
 */
class MainThreadFactory : public IMainThreadFactory
{
public:
    MainThreadFactory() = default;
    ~MainThreadFactory() override = default;

    std::shared_ptr<IMainThread> getMainThread() const override;

protected:
    /**
     * @brief Weak pointer to the singleton main thread object.
     */
    static std::weak_ptr<IMainThread> m_mainThread;

    /**
     * @brief Mutex protection for creation of the MainThread object.
     */
    static std::mutex m_creationMutex;
};

/**
 * @brief The definition of the MediaKeys.
 */
class MainThread : public IMainThread
{
public:
    MainThread();
    virtual ~MainThread();

    int32_t registerClient() override;
    void unregisterClient(uint32_t clientId) override;

    void enqueueTask(uint32_t clientId, Task task) override;
    void enqueueTaskAndWait(uint32_t clientId, Task task) override;
    void enqueuePriorityTaskAndWait(uint32_t clientId, Task task) override;

private:
    /**
     * @brief Information of a task.
     */
    struct TaskInfo
    {
        uint32_t clientId;                           /**< The id of the client creating the task. */
        Task task;                                   /**< The task to execute. */
        std::unique_ptr<std::mutex> mutex;           /**< Mutex for the task condition variable. */
        std::unique_ptr<std::condition_variable> cv; /**< The condition variable of the task. */
    };

    /**
     * @brief Starts a loop that listens for enqueued tasks.
     */
    void mainThreadLoop();

    /**
     * @brief Waits for tasks to enter the queue and returns the next task.
     *
     * @retval The next task in the queue.
     */
    const std::shared_ptr<TaskInfo> waitForTask();

    /**
     * @brief Whether the main thread is running.
     */
    bool m_isMainThreadRunning;

    /**
     * @brief The main thread.
     */
    std::thread m_thread;

    /**
     * @brief A mutex protecting access to the task queue.
     */
    std::mutex m_taskQueueMutex;

    /**
     * @brief A condition variable used to notify of tasks entering the task queue.
     */
    std::condition_variable m_taskQueueCv;

    /**
     * @brief The queue of tasks and there infomation.
     */
    std::deque<std::shared_ptr<TaskInfo>> m_taskQueue;

    /**
     * @brief The main thread objects client id, for registering new clients.
     */
    const uint32_t m_mainThreadClientId;

    /**
     * @brief The next client id to be given to a registering client.
     */
    std::atomic<uint32_t> m_nextClientId;

    /**
     * @brief Clients registered on this thread.
     */
    std::set<uint32_t> m_registeredClients;
};
} // namespace firebolt::rialto::server

#endif // FIREBOLT_RIALTO_SERVER_MAIN_THREAD_H_
