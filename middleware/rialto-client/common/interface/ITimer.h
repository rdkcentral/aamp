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

#ifndef FIREBOLT_RIALTO_COMMON_I_TIMER_H_
#define FIREBOLT_RIALTO_COMMON_I_TIMER_H_

#include <chrono>
#include <functional>
#include <memory>

namespace firebolt::rialto::common
{
class ITimer;

enum class TimerType
{
    ONE_SHOT,
    PERIODIC
};

/**
 * @brief ITimerFactory factory class, returns a concrete implementation of ITimer
 */
class ITimerFactory
{
public:
    ITimerFactory() = default;
    virtual ~ITimerFactory() = default;

    /**
     * @brief Gets the ITimerFactory instance.
     *
     * @retval the factory instance or null on error.
     */
    static std::shared_ptr<ITimerFactory> getFactory();

    /**
     * @brief Creates an ITimer object.
     *
     * @param[in] timeout   : Timeout after which callback will be called
     * @param[in] callback  : Function which is called after timeout
     * @param[in] timerType : Type of timer
     *
     * @retval the new timer instance or null on error.
     */
    virtual std::unique_ptr<ITimer> createTimer(const std::chrono::milliseconds &timeout,
                                                const std::function<void()> &callback,
                                                TimerType timerType = TimerType::ONE_SHOT) const = 0;
};

class ITimer
{
public:
    ITimer() = default;
    virtual ~ITimer() = default;

    ITimer(const ITimer &) = delete;
    ITimer &operator=(const ITimer &) = delete;
    ITimer(ITimer &&) = delete;
    ITimer &operator=(ITimer &&) = delete;

    /**
     * @brief Cancels the timer
     */
    virtual void cancel() = 0;

    /**
     * @brief Checks if timer is active
     *
     * @retval true if timer is active, false otherwise
     */
    virtual bool isActive() const = 0;
};

} // namespace firebolt::rialto::common

#endif // FIREBOLT_RIALTO_COMMON_I_TIMER_H_
