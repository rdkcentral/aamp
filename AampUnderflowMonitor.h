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

class StreamAbstractionAAMP;
class PrivateInstanceAAMP;

class AampUnderflowMonitor {
public:
    /**
    * @fn AampUnderflowMonitor
    * @brief Construct an `AampUnderflowMonitor`.
    * @param[in] stream Stream abstraction used to query buffered video duration
    *                   and playback state relevant to underflow detection.
    * @param[in] aamp   AAMP instance used for coordination (downloads, buffering
    *                   state) and event emission.
    * @note Ownership: The caller retains ownership of both `stream` and `aamp`.
    *       All pointer accesses are protected by an internal mutex to prevent
    *       TOCTOU races during shutdown.
    * @note Thread Safety: `Stop()` safely terminates the monitoring thread and
    *       prevents further pointer access. Call `Stop()` before destroying the
    *       `StreamAbstractionAAMP` or `PrivateInstanceAAMP` instances.
     */
    AampUnderflowMonitor(StreamAbstractionAAMP* stream, PrivateInstanceAAMP* aamp);

    /**
    * @fn ~AampUnderflowMonitor
     * @brief Destructor. Ensures monitoring has been stopped.
     */
    ~AampUnderflowMonitor();

     /**
      * @fn Start
      * @brief Start the monitoring thread. If already running, returns immediately.
      * @return void
      */
    void Start();

    /**
      * @fn Stop
      * @brief Request the monitoring thread to stop and join it if joinable.
      *        Safe to call multiple times. Nullifies internal pointers after
      *        thread termination to prevent use-after-free.
      * @return void
      * @note After `Stop()` returns, the monitoring thread has fully terminated
      *       and will not access `StreamAbstractionAAMP` or `PrivateInstanceAAMP`.
     */
    void Stop();

    /**
     * @fn isRunning
     * @brief Check whether the monitoring thread is currently active.
     * @return true if running, false otherwise.
     */
    bool IsRunning() const { return mRunning.load(); }

private:
    /**
     * @fn run
     * @brief Thread entry routine that polls/awaits underflow conditions
     *        and triggers coordinated handling.
     */
    void Run();

    StreamAbstractionAAMP* mStream; /** Stream abstraction used to query buffered duration and playback state. */
    PrivateInstanceAAMP* mAamp; /** AAMP instance used to emit events, control downloads, and query state. */
    std::thread mThread;    /** Background thread that performs underflow monitoring. */
    std::atomic<bool> mRunning{false};   /** Atomic running flag indicating thread active state. */
    std::mutex mMutex; /** Protects pointer access in Run() and serializes Start/Stop. */
};

#endif // AAMP_UNDERFLOW_MONITOR_H
