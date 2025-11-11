/*
 * Copyright (C) 2023   
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "Timer.h"
#include <gst/gst.h>

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
            GST_ERROR("Failed to create the timer factory, reason: %s", e.what());
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
    doCancel();
}

void Timer::cancel()
{
    doCancel();
}

bool Timer::isActive() const
{
    return m_active;
}

void Timer::doCancel()
{
    m_active = false;

    if (std::this_thread::get_id() != m_thread.get_id() && m_thread.joinable())
    {
        m_cv.notify_one();
        m_thread.join();
    }
}
