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

#ifndef FIREBOLT_RIALTO_COMMON_I_EVENT_THREAD_H_
#define FIREBOLT_RIALTO_COMMON_I_EVENT_THREAD_H_

#include <atomic>
#include <condition_variable>
#include <functional>
#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace firebolt::rialto::common
{
class IEventThread;

/**
 * @brief IEventThreadFactory factory class, returns a concrete implementation of IEventThread
 */
class IEventThreadFactory
{
public:
    IEventThreadFactory() = default;
    virtual ~IEventThreadFactory() = default;

    /**
     * @brief Creates a IEventThreadFactory instance.
     *
     * @retval the factory instance or null on error.
     */
    static std::shared_ptr<IEventThreadFactory> createFactory();

    /**
     * @brief Creates an IEventThread object.
     *
     * @param[in] threadName    : The name of the thread
     *
     * @retval the new event thread instance or null on error.
     */
    virtual std::unique_ptr<IEventThread> createEventThread(std::string threadName = std::string()) const = 0;
};

class IEventThread
{
public:
    IEventThread() = default;
    virtual ~IEventThread() = default;

    IEventThread(const IEventThread &) = delete;
    IEventThread &operator=(const IEventThread &) = delete;
    IEventThread(IEventThread &&) = delete;
    IEventThread &operator=(IEventThread &&) = delete;

    /**
     * @brief Flush any waiting events.
     */
    virtual void flush() = 0;

    /**
     * @brief Add the event handler function.
     *
     * @param[in] func  : Function to call on event.
     */
    template <class Function> inline void add(Function &&func) { this->addImpl(std::forward<Function>(func)); }

    /**
     * @brief Add the event handler function with arguments.
     *
     * @param[in] func  : Function to call on event.
     * @param[in] args  : Arguments to pass into the function.
     */
    template <class Function, class... Args> inline void add(Function &&func, Args &&...args)
    {
        this->addImpl(std::bind(std::forward<Function>(func), std::forward<Args>(args)...));
    }

private:
    virtual void addImpl(std::function<void()> &&func) = 0;
};

}; // namespace firebolt::rialto::common

#endif // FIREBOLT_RIALTO_COMMON_I_EVENT_THREAD_H_
