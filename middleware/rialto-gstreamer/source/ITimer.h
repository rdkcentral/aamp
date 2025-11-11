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

#ifndef I_TIMER_H_
#define I_TIMER_H_

#include <chrono>
#include <functional>
#include <memory>

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

#endif // I_TIMER_H_
