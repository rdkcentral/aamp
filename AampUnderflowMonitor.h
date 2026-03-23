/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2026 RDK Management
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

/**
 * @file AampUnderflowMonitor.h
 * @brief Declares `AampUnderflowMonitor`, a helper that monitors
 *        underflow conditions in a dedicated background thread
 *        and assists with coordinated resume behavior.
 */
#ifndef AAMP_UNDERFLOW_MONITOR_H
#define AAMP_UNDERFLOW_MONITOR_H

#include <thread>
#include <atomic>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <chrono>

class StreamAbstractionAAMP;
class PrivateInstanceAAMP;

/**
 * @class AampUnderflowMonitor
 * @brief Monitors video buffer underflow and drives coordinated pipeline resume.
 *
 * ## Purpose
 * Polls the buffered video duration at a configurable interval and sets/clears
 * the buffering state on the AAMP instance when underflow thresholds are crossed.
 *
 * ## State machine
 * @code
 *   stopped --Start()--> running --Stop()--> stopped
 * @endcode
 *
 * ## Thread safety
 * Start() and Stop() are serialised by mStartStopMutex and may safely be
 * called from any thread, including concurrently.
 * Stop() blocks until the monitor thread has exited.
 */
class AampUnderflowMonitor {
public:
    /**
     * @brief Constructor.
     * @param[in] stream Stream abstraction used to query buffered video duration.
     *                   Must remain valid until Stop() returns.
     *                   (The monitor does not take ownership of the pointer.)
     * @param[in] aamp   AAMP instance used to query state and control buffering.
     *                   Must remain valid until Stop() returns.
     *                   (The monitor does not take ownership of the pointer.)
     */
    AampUnderflowMonitor(StreamAbstractionAAMP* stream, PrivateInstanceAAMP* aamp);

    /**
     * @brief Destructor. Calls Stop() to ensure the monitor thread exits.
     */
    ~AampUnderflowMonitor();

    /**
     * @brief Start the monitoring thread. If already running, returns immediately.
     * @return void
     */
    void Start();

    /**
     * @brief Stop and join the monitoring thread.
     * @return void
     */
    void Stop();

    /**
     * @brief Check whether the monitoring thread is currently active.
     * @return true if running, false otherwise.
     */
    bool IsRunning() const { return mRunning.load(); }

private:
    /**
     * @brief Thread entry routine that polls/awaits underflow conditions
     *        and triggers coordinated handling.
     */
    void Run();

    /**
     * @brief Sleep for up to @p ms milliseconds, returning early if Stop() signals.
     * @param[in] ms Maximum wait duration in milliseconds.
     */
    void WaitMs(int ms);

    StreamAbstractionAAMP* mStream; /** Stream abstraction used to query buffered duration and playback state. */
    PrivateInstanceAAMP* mAamp; /** AAMP instance used to emit events, control downloads, and query state. */
    std::thread mUnderflowMonitorThread;    /** Background thread that performs underflow monitoring. */
    std::atomic<bool> mRunning{false};   /** Atomic running flag indicating thread active state. */
    std::mutex mStartStopMutex; /** Serializes concurrent Start/Stop calls. */
    std::mutex mSleepMutex;              /** Guards mSleepCv waits inside Run(). */
    std::condition_variable mSleepCv;    /** Signalled by Stop() to unblock WaitMs() immediately. */
    bool mWakeupSignalled{false};        /** Explicit wakeup flag, set under mSleepMutex before notify. */
};

#endif // AAMP_UNDERFLOW_MONITOR_H
