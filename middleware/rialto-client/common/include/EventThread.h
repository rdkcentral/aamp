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

#ifndef FIREBOLT_RIALTO_COMMON_EVENT_THREAD_H_
#define FIREBOLT_RIALTO_COMMON_EVENT_THREAD_H_

#include "IEventThread.h"
#include <atomic>
#include <condition_variable>
#include <functional>
#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace firebolt::rialto::common
{
class EventThreadFactory : public IEventThreadFactory
{
public:
    std::unique_ptr<IEventThread> createEventThread(std::string threadName) const override;
};

class EventThread : public IEventThread
{
public:
    explicit EventThread(std::string threadName = std::string());
    ~EventThread();

    void flush() override;

private:
    void addImpl(std::function<void()> &&func) override;

    void threadExecutor();

private:
    const std::string m_kThreadName;

    std::list<std::function<void()>> m_funcs;
    std::mutex m_lock;
    std::condition_variable m_cond;

    std::atomic<bool> m_shutdown;
    std::thread m_thread;
};

}; // namespace firebolt::rialto::common

#endif // FIREBOLT_RIALTO_COMMON_EVENT_THREAD_H_
