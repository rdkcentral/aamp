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

#include "Timer.h"
#include "RialtoCommonLogging.h"

namespace firebolt::rialto::common
{
std::weak_ptr<ITimerFactory> TimerFactory::m_factory;

std::shared_ptr<ITimerFactory> ITimerFactory::getFactory()
{
    std::shared_ptr<ITimerFactory> factory = TimerFactory::m_factory.lock();

    if (!factory)
    {
        try
        {
            factory = std::make_shared<TimerFactory>();
        }
        catch (const std::exception &e)
        {
            RIALTO_COMMON_LOG_ERROR("Failed to create the timer factory, reason: %s", e.what());
        }

        TimerFactory::m_factory = factory;
    }

    return factory;
}

std::unique_ptr<ITimer> TimerFactory::createTimer(const std::chrono::milliseconds &timeout,
                                                  const std::function<void()> &callback, TimerType timerType) const
{
    return std::make_unique<Timer>(timeout, callback, timerType);
}

Timer::Timer(const std::chrono::milliseconds &timeout, const std::function<void()> &callback, TimerType timerType)
    : m_active{true}, m_timeout{timeout}, m_callback{callback}
{
    m_thread = std::thread(
        [this, timerType]()
        {
            do
            {
                std::unique_lock<std::mutex> lock{m_mutex};
                if (!m_cv.wait_for(lock, m_timeout, [this]() { return !m_active; }))
                {
                    if (m_active && m_callback)
                    {
                        lock.unlock();
                        m_callback();
                    }
                }
            } while (timerType == TimerType::PERIODIC && m_active);
            m_active = false;
        });
}

Timer::~Timer()
{
    cancel();
}

void Timer::cancel()
{
    m_active = false;

    if (std::this_thread::get_id() != m_thread.get_id() && m_thread.joinable())
    {
        m_cv.notify_one();
        m_thread.join();
    }
}

bool Timer::isActive() const
{
    return m_active;
}
} // namespace firebolt::rialto::common
