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
 * @brief Declares `AampUnderflowMonitor`, a timer-driven video underflow detector.
 *
 * Instead of polling playback position, the monitor maintains a wall-clock
 * deadline equal to:
 *
 *   now + (lastDownloadedPosition - currentPosition) / playRate
 *
 * This deadline is re-armed every time a new video fragment (or LL-DASH chunk)
 * is queued, and whenever the play rate or pipeline pause-state changes.
 * If the deadline expires before a new fragment arrives, underflow is declared.
 *
 * Benefits over the previous polling approach:
 *  - No dependency on GStreamer position polling in steady state.
 *  - No platform-specific IsSinkCacheEmpty / PTS-change heuristics.
 *  - Works identically across device types.
 *  - Thread sleeps precisely to the expected drain point; no arbitrary cadence.
 */
#ifndef AAMP_UNDERFLOW_MONITOR_H
#define AAMP_UNDERFLOW_MONITOR_H

#include <thread>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>

class PrivateInstanceAAMP;

class AampUnderflowMonitor {
public:
    /**
     * @fn AampUnderflowMonitor
     * @brief Construct the monitor.  Does not start the background thread.
     * @param[in] aamp AAMP instance — must outlive this object (or Stop() must
     *                 be called before it is destroyed).
     */
    explicit AampUnderflowMonitor(PrivateInstanceAAMP* aamp);

    /**
     * @fn ~AampUnderflowMonitor
     * @brief Destructor. Calls Stop() to ensure the thread is joined first.
     */
    ~AampUnderflowMonitor();

    // Non-copyable, non-movable.
    AampUnderflowMonitor(const AampUnderflowMonitor&) = delete;
    AampUnderflowMonitor& operator=(const AampUnderflowMonitor&) = delete;

    /**
     * @fn Start
     * @brief Start the background timer thread.  Safe to call once.
     *        If already running, returns immediately.
     */
    void Start();

    /**
     * @fn Stop
     * @brief Signal the thread to exit and block until it has joined.
     *        Safe to call multiple times.  After return, no further callbacks
     *        into `aamp` will be made.
     */
    void Stop();

    /**
     * @fn IsRunning
     * @return true while the background thread is alive.
     */
    bool IsRunning() const { return mRunning.load(); }

    /**
     * @fn NotifyVideoFragment
     * @brief Called by the video track whenever a fragment (or LL-DASH chunk)
     *        has been successfully downloaded and queued for injection.
     *
     * Re-arms the underflow deadline:
     *   deadline = now + bufferSec / playRate
     * where bufferSec = endPosition - currentPlaybackPositionSec.
     *
     * @param[in] endPosition   Absolute stream position (seconds) of the end of
     *                          the newly queued content — i.e.
     *                          absolutePosition + fragmentDuration.
     * @param[in] playRate      Current play rate (1.0, 1.03, 0.97, …).
     *                          Must be > 0.
     */
    void NotifyVideoFragment(double endPosition, float playRate);

    /**
     * @fn NotifyPipelinePaused
     * @brief Suspend deadline tracking while the pipeline is paused for
     *        buffering.  The underflow condition is already active; no further
     *        timer firings are needed until playback resumes.
     */
    void NotifyPipelinePaused();

    /**
     * @fn NotifyPipelineResumed
     * @brief Re-arm the deadline after buffering has ended.
     *        Call this after the pipeline transitions back to PLAYING.
     * @param[in] endPosition  Latest known end-of-buffer position (seconds).
     * @param[in] playRate     Current play rate.
     */
    void NotifyPipelineResumed(double endPosition, float playRate);

    /**
     * @fn NotifyRateChange
     * @brief Called immediately after the playback rate changes (SetRate / trickplay).
     *
     * Updates the cached play rate so that the background thread's trickplay-
     * suppression check uses the new rate before any fragment at the new rate
     * has been received.  If the new rate is a trickplay rate, the current
     * deadline is also disarmed — the first fragment at the new rate will rearm
     * it via NotifyVideoFragment.
     *
     * @param[in] rate  New play rate (may be negative for rewind).
     */
    void NotifyRateChange(float rate);

private:
    /**
     * @fn Run
     * @brief Background thread entry point.
     * Sleeps until the underflow deadline, then detects/resumes underflow and
     * waits for the next deadline notification.
     */
    void Run();

    /**
     * @brief Re-arm the deadline.  Must be called under mMutex.
     * @param bufferSec Remaining buffer in seconds (must be ≥ 0).
     * @param playRate  Current play rate (must be > 0).
     */
    void RearmDeadline(double bufferSec, float playRate);

    using Clock     = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    PrivateInstanceAAMP*    mAamp;              ///< AAMP instance — not owned.
    std::thread             mThread;            ///< Background timer thread.
    std::atomic<bool>       mRunning{false};    ///< True while thread is alive.

    std::mutex              mMutex;             ///< Guards all state below.
    std::condition_variable mCV;                ///< Woken on deadline change or stop.

    TimePoint               mDeadline;          ///< Wall-clock time of expected underflow.
    bool                    mDeadlineArmed{false}; ///< False while pipeline is paused for buffering.
    double                  mCurrentPlayRate{1.0}; ///< Cached for resume re-arm.
    double                  mCurrentEndPosition{0.0}; ///< Cached for resume re-arm.
};

#endif // AAMP_UNDERFLOW_MONITOR_H
