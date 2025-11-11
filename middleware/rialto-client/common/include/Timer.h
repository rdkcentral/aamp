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

#ifndef FIREBOLT_RIALTO_COMMON_TIMER_H_
#define FIREBOLT_RIALTO_COMMON_TIMER_H_

#include "ITimer.h"

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>

namespace firebolt::rialto::common
{
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
    std::atomic<bool> m_active;
    std::chrono::milliseconds m_timeout;
    std::function<void()> m_callback;
    mutable std::mutex m_mutex;
    std::thread m_thread;
    std::condition_variable m_cv;
};
} // namespace firebolt::rialto::common

#endif // FIREBOLT_RIALTO_COMMON_TIMER_H_
