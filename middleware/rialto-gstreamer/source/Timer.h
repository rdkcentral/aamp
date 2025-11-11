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

#ifndef TIMER_H_
#define TIMER_H_

#include "ITimer.h"

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>

/**
 * @brief ITimerFactory factory class definition.
 */
class TimerFactory : public ITimerFactory
{
public:
    /**
     * @brief Weak pointer to the singleton factory object.
     */
    static std::weak_ptr<ITimerFactory> m_factory;

    std::unique_ptr<ITimer> createTimer(const std::chrono::milliseconds &timeout, const std::function<void()> &callback,
                                        TimerType timerType = TimerType::ONE_SHOT) const override;
};

class Timer : public ITimer
{
public:
    Timer(const std::chrono::milliseconds &timeout, const std::function<void()> &callback,
          TimerType timerType = TimerType::ONE_SHOT);
    ~Timer();
    Timer(const Timer &) = delete;
    Timer(Timer &&) = delete;
    Timer &operator=(const Timer &) = delete;
    Timer &operator=(Timer &&) = delete;

    void cancel() override;
    bool isActive() const override;

private:
    void doCancel();

    std::atomic<bool> m_active;
    std::chrono::milliseconds m_timeout;
    std::function<void()> m_callback;
    mutable std::mutex m_mutex;
    std::thread m_thread;
    std::condition_variable m_cv;
};

#endif // TIMER_H_
