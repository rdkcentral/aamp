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

#ifndef FIREBOLT_RIALTO_SERVER_I_MAIN_THREAD_H_
#define FIREBOLT_RIALTO_SERVER_I_MAIN_THREAD_H_

#include "IMainThread.h"
#include <functional>
#include <memory>
#include <utility>

namespace firebolt::rialto::server
{
class IMainThread;

/**
 * @brief IMainThread factory class, gets the concrete implementation of IMainThread
 */
class IMainThreadFactory
{
public:
    IMainThreadFactory() = default;
    virtual ~IMainThreadFactory() = default;

    /**
     * @brief Create a IMainThreadFactory instance.
     *
     * @retval the factory instance or null on error.
     */
    static std::shared_ptr<IMainThreadFactory> createFactory();

    /**
     * @brief IMainThread factory method, gets a concrete implementation of IMainThread
     *
     * @retval the main thread instance or null on error.
     */
    virtual std::shared_ptr<IMainThread> getMainThread() const = 0;
};

/**
 * @brief The definition of the IMainThread interface.
 */
class IMainThread
{
public:
    using Task = std::function<void()>;

    IMainThread() = default;
    virtual ~IMainThread() = default;

    IMainThread(const IMainThread &) = delete;
    IMainThread(IMainThread &&) = delete;
    IMainThread &operator=(const IMainThread &) = delete;
    IMainThread &operator=(IMainThread &&) = delete;

    /**
     * @brief Register a client on the main thread.
     *
     * Required by clients who want to enqueue tasks on the main thread.
     *
     * @retval The registered client id.
     */
    virtual int32_t registerClient() = 0;

    /**
     * @brief Unregister a client on the main thread.
     *
     * Should be called on the main thread.
     * After been called the client will no longer be able to enqueue tasks.
     *
     * @param[in]  clientId : The id of the registered client.
     */
    virtual void unregisterClient(uint32_t clientId) = 0;

    /**
     * @brief Enqueue a task on the main thread and return.
     *
     * @param[in]  clientId : The id of the registered client.
     * @param[in]  task     : Task to queue.
     */
    virtual void enqueueTask(uint32_t clientId, Task task) = 0;

    /**
     * @brief Enqueue a task on the main thread and wait for it to finish before returning.
     *
     * @param[in]  clientId : The id of the registered client.
     * @param[in]  task     : Task to queue.
     */
    virtual void enqueueTaskAndWait(uint32_t clientId, Task task) = 0;

    /**
     * @brief Enqueue a priority task on the main thread and wait for it to finish before returning.
     *
     * @param[in]  clientId : The id of the registered client.
     * @param[in]  task     : Task to queue.
     */
    virtual void enqueuePriorityTaskAndWait(uint32_t clientId, Task task) = 0;
};
} // namespace firebolt::rialto::server

#endif // FIREBOLT_RIALTO_SERVER_I_MAIN_THREAD_H_
