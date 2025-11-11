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

#include "EventThread.h"
#include "RialtoCommonLogging.h"

#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>

namespace firebolt::rialto::common
{
std::shared_ptr<IEventThreadFactory> IEventThreadFactory::createFactory()
{
    std::shared_ptr<IEventThreadFactory> factory;

    try
    {
        factory = std::make_shared<EventThreadFactory>();
    }
    catch (const std::exception &e)
    {
        RIALTO_COMMON_LOG_ERROR("Failed to create the event thread factory, reason: %s", e.what());
    }

    return factory;
}

std::unique_ptr<IEventThread> EventThreadFactory::createEventThread(std::string threadName) const
{
    return std::make_unique<EventThread>(threadName);
}

EventThread::EventThread(std::string threadName) : m_kThreadName(std::move(threadName)), m_shutdown(false)
{
    m_thread = std::thread(&EventThread::threadExecutor, this);
}

EventThread::~EventThread()
{
    std::unique_lock<std::mutex> locker(m_lock);

    m_shutdown = true;

    m_cond.notify_all();

    locker.unlock();

    if (m_thread.joinable())
        m_thread.join();
}

void EventThread::threadExecutor()
{
    if (!m_kThreadName.empty())
    {
        pthread_setname_np(pthread_self(), m_kThreadName.c_str());
    }

    std::unique_lock<std::mutex> locker(m_lock);

    while (true)
    {
        while (!m_shutdown && m_funcs.empty())
            m_cond.wait(locker);

        if (m_shutdown)
            break;

        std::function<void()> func = std::move(m_funcs.front());
        m_funcs.pop_front();

        m_lock.unlock();

        if (func)
            func();

        m_lock.lock();
    }
}

void EventThread::flush()
{
    sem_t semaphore;
    sem_init(&semaphore, 0, 0);

    // add a simple function to release the semaphore in the context of the event thread
    addImpl(
        [sem = &semaphore]()
        {
            if (sem_post(sem) != 0)
                RIALTO_COMMON_LOG_SYS_ERROR(errno, "failed to signal semaphore");
        });

    // wait for the above call to unblock the semaphore
    TEMP_FAILURE_RETRY(sem_wait(&semaphore));
}

void EventThread::addImpl(std::function<void()> &&func)
{
    std::lock_guard<std::mutex> locker(m_lock);
    m_funcs.emplace_back(std::move(func));
    m_cond.notify_all();
}

}; // namespace firebolt::rialto::common
