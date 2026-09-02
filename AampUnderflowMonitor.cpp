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
 * @file AampUnderflowMonitor.cpp
 * @brief Timer-driven video underflow detection for AampUnderflowMonitor.
 *
 * Algorithm overview
 * ==================
 * On each video fragment (or LL-DASH chunk) download completion, the caller
 * invokes NotifyVideoFragment(endPosition, playRate).  The monitor computes:
 *
 *   bufferSec = endPosition - aamp->GetPositionSeconds()   // sampled once here
 *   deadline  = steady_clock::now() + bufferSec / playRate
 *
 * The background thread sleeps until `deadline`.  If it wakes without the
 * deadline having been updated (i.e. no new fragment arrived in time),
 * underflow is declared via SetBufferingState(true).
 *
 * The deadline is re-armed by:
 *   - NotifyVideoFragment()   — each downloaded fragment / chunk
 *   - NotifyPipelineResumed() — after buffering recovery
 *
 * The deadline is suspended (disarmed) by:
 *   - NotifyPipelinePaused()  — while pipeline is paused for buffering
 *   - Stop()                  — shutdown
 *
 * Resume logic
 * ============
 * While underflow is active (mBufUnderFlowStatus == true), NotifyVideoFragment()
 * accumulates buffered content but does NOT resume the pipeline itself.
 * Instead it calls SetBufferingState(false) once bufferSec >= kResumeThresholdSec,
 * which unpauses the pipeline.  The deadline is then re-armed directly inside
 * NotifyVideoFragment() (not via NotifyPipelineResumed()) to avoid re-acquiring
 * mMutex on the same call stack, which would deadlock on some platforms.
 * NotifyPipelineResumed() remains available for callers that resume the pipeline
 * through an independent path (e.g. an external seek or tune).
 *
 * No polling, no GStreamer sinkCacheEmpty, no position-change heuristics.
 */
#include "AampUnderflowMonitor.h"
#include "priv_aamp.h"
#include "AampDefine.h"
#include "AampConfig.h"
#include "AampLogManager.h"
#include "AampMediaType.h"
#include "AampUtils.h"
#include <stdexcept>

// Minimum buffered content remaining (seconds) below which a deadline expiry is
// treated as a genuine underflow.  If more than this much content is still ahead
// of the current playback position when the deadline fires, the expiry is a false
// alarm — e.g. a CDAI period-boundary injection gap, or a brief live-edge stall
// where GStreamer has content queued but the download loop is momentarily idle.
// In those cases the deadline is rearmed for the remaining drain time.
static constexpr double kFalseAlarmGuardSec = 1.0;

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

AampUnderflowMonitor::AampUnderflowMonitor(PrivateInstanceAAMP* aamp)
    : mAamp(aamp)
{
    if (mAamp == nullptr)
    {
        throw std::invalid_argument("AampUnderflowMonitor: aamp cannot be null");
    }
}

AampUnderflowMonitor::~AampUnderflowMonitor()
{
    Stop();
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void AampUnderflowMonitor::Start()
{
    std::unique_lock<std::mutex> lock(mMutex);

    if (mThread.joinable())
    {
        if (mRunning.load())
        {
            AAMPLOG_INFO("AampUnderflowMonitor already running; skipping start");
            return;
        }
        lock.unlock();
        mThread.join();
        AAMPLOG_INFO("AampUnderflowMonitor previous thread joined before restart");
        lock.lock();
    }

    mDeadlineArmed = false;
    mRunning.store(true);

    try
    {
        mThread = std::thread(&AampUnderflowMonitor::Run, this);
        AAMPLOG_INFO("AampUnderflowMonitor thread created [%zx]",
                     GetPrintableThreadID(mThread));
    }
    catch (const std::exception& e)
    {
        mRunning.store(false);
        AAMPLOG_WARN("Failed to create AampUnderflowMonitor thread: %s", e.what());
    }
}

void AampUnderflowMonitor::Stop()
{
    {
        std::lock_guard<std::mutex> lock(mMutex);
        mRunning.store(false);
        mDeadlineArmed = false;
    }
    mCV.notify_all();

    if (mThread.joinable())
    {
        mThread.join();
        AAMPLOG_INFO("AampUnderflowMonitor thread joined");
    }
}

// ---------------------------------------------------------------------------
// Deadline management (called from downloader / pipeline threads)
// ---------------------------------------------------------------------------

void AampUnderflowMonitor::RearmDeadline(double bufferSec, float playRate)
{
    // Caller holds mMutex.
    if (playRate <= 0.0f)
    {
        mDeadlineArmed = false;
        return;
    }
    const double sleepSec = bufferSec / static_cast<double>(playRate);
    using Dur = Clock::duration;
    mDeadline      = Clock::now() + std::chrono::duration_cast<Dur>(std::chrono::duration<double>(sleepSec));
    mDeadlineArmed = true;
}

void AampUnderflowMonitor::NotifyVideoFragment(double endPosition, float playRate)
{
    if (!mRunning.load()) return;

    double bufferSec       = 0.0;
    bool   shouldResume    = false;
    double resumeThreshold = 0.0;

    {
        std::lock_guard<std::mutex> lock(mMutex);
        if (!mAamp) return;

        const double positionSec = mAamp->GetPositionMs() / 1000.0;
        bufferSec = endPosition - positionSec;
        if (bufferSec < 0.0) bufferSec = 0.0;

        mCurrentEndPosition = endPosition;
        mCurrentPlayRate    = playRate;

        const bool underflowActive = mAamp->GetBufUnderFlowStatus();

        if (underflowActive)
        {
            // Pipeline is paused for buffering.  Check whether we now have enough
            // content to resume.  Don't rearm the timer here — NotifyPipelineResumed()
            // does that once the pipeline is confirmed live.
            resumeThreshold = mAamp->mConfig->GetConfigValue(eAAMPConfig_UnderflowResumeThresholdSec);
            shouldResume    = (bufferSec >= resumeThreshold);
        }
        else
        {
            // Normal playback: rearm the deadline.
            RearmDeadline(bufferSec, playRate);
        }
    }

    if (shouldResume)
    {
        AAMPLOG_INFO("[video] underflow ended. buffered=%.3f (>= resume threshold %.3f)",
                     bufferSec, resumeThreshold);
        mAamp->SetBufferingState(false);
        // Directly rearm the deadline here rather than through SetBufferingState →
        // NotifyPipelineResumedToUnderflowMonitor, which would try to re-acquire
        // mUnderflowMonitorMutex on the same thread (deadlock on macOS).
        {
            std::lock_guard<std::mutex> lock(mMutex);
            RearmDeadline(bufferSec, playRate);
        }
    }
    else if (mAamp->GetBufUnderFlowStatus())
    {
        AAMPLOG_INFO("[video] waiting to end underflow. buffered=%.3f", bufferSec);
    }

    mCV.notify_one();
}

void AampUnderflowMonitor::NotifyRateChange(float rate)
{
    {
        std::lock_guard<std::mutex> lock(mMutex);
        mCurrentPlayRate = rate;
        // If transitioning to trickplay, disarm immediately so the stale deadline
        // (armed at the old rate) cannot fire before the first trickplay fragment
        // arrives and rearmed the timer via NotifyVideoFragment.
        const bool isTrickplay = (rate != AAMP_NORMAL_PLAY_RATE &&
                                  rate != AAMP_SLOWMOTION_RATE  &&
                                  rate != AAMP_RATE_PAUSE);
        if (isTrickplay)
        {
            AAMPLOG_INFO("[video] AampUnderflowMonitor: rate changed to %.2f (trickplay); disarming deadline",
                         rate);
            mDeadlineArmed = false;
        }
    }
    mCV.notify_one();
}

void AampUnderflowMonitor::NotifyPipelinePaused()
{
    {
        std::lock_guard<std::mutex> lock(mMutex);
        mDeadlineArmed = false;
    }
    mCV.notify_one();
}

void AampUnderflowMonitor::NotifyPipelineResumed(double endPosition, float playRate)
{
    {
        std::lock_guard<std::mutex> lock(mMutex);
        if (!mAamp) return;

        const double positionSec = mAamp->GetPositionMs() / 1000.0;
        double bufferSec = endPosition - positionSec;
        if (bufferSec < 0.0) bufferSec = 0.0;

        mCurrentEndPosition = endPosition;
        mCurrentPlayRate    = playRate;
        RearmDeadline(bufferSec, playRate);
    }
    mCV.notify_one();
}

// ---------------------------------------------------------------------------
// Background thread
// ---------------------------------------------------------------------------

void AampUnderflowMonitor::Run()
{
    AAMPLOG_INFO("Started AampUnderflowMonitor for video");

    std::unique_lock<std::mutex> lock(mMutex);

    while (mRunning.load())
    {
        if (!mDeadlineArmed)
        {
            // No active deadline — wait for a fragment notification or Stop().
            mCV.wait(lock, [this]{ return !mRunning.load() || mDeadlineArmed; });
            continue;
        }

        // Sleep until the deadline, or until woken by a rearm / stop.
        const TimePoint deadline = mDeadline;
        const bool timedOut = !mCV.wait_until(lock, deadline,
            [this, &deadline]{ return !mRunning.load() || !mDeadlineArmed || mDeadline != deadline; });

        if (!mRunning.load()) break;

        // Woken early (deadline changed or disarmed) — loop back.
        if (!timedOut) continue;

        // Deadline expired — declare underflow.
        if (!mAamp) break;

        const AAMPPlayerState state = mAamp->GetState();
        if (state == eSTATE_STOPPED || state == eSTATE_RELEASED ||
            state == eSTATE_ERROR   || state == eSTATE_IDLE ||
            state == eSTATE_COMPLETE)
        {
            AAMPLOG_INFO("[video] AampUnderflowMonitor: exiting (state=%d)", static_cast<int>(state));
            break;
        }

        // Use the cached play rate (updated under mMutex in NotifyVideoFragment) rather
        // than reading mAamp->rate directly — the latter is a plain non-atomic float
        // that would constitute a C++ data race with the writer thread.
        const float rate       = mCurrentPlayRate;
        const bool  isTrickplay = (rate != AAMP_NORMAL_PLAY_RATE &&
                                   rate != AAMP_SLOWMOTION_RATE  &&
                                   rate != AAMP_RATE_PAUSE);
        const bool  isSeeking   = (state == eSTATE_SEEKING);

        if (isTrickplay || isSeeking)
        {
            AAMPLOG_TRACE("[video] underflow deadline expired but suppressed "
                          "(rate=%.2f trickplay=%d seeking=%d); disarming",
                          rate, (int)isTrickplay, (int)isSeeking);
            mDeadlineArmed = false;
            continue;
        }

        // Re-read the actual buffer depth before declaring underflow.  If GStreamer
        // still has content queued (e.g. CDAI period-boundary injection gap or a
        // brief live-edge stall), the deadline expiry is premature — rearm the
        // timer for the remaining drain time and wait rather than pausing the pipeline.
        // GetPositionMs() is safe to call under mMutex (same pattern used in
        // NotifyVideoFragment); mCurrentEndPosition is guarded by the same mutex.
        {
            const double posNow  = mAamp->GetPositionMs() / 1000.0;
            const double bufLeft = mCurrentEndPosition - posNow;
            if (bufLeft > kFalseAlarmGuardSec)
            {
                AAMPLOG_INFO("[video] deadline expired but %.3fs still buffered; "
                             "rearming (period gap / live-edge stall, not underflow)",
                             bufLeft);
                RearmDeadline(bufLeft, rate);
                continue;
            }

        }

        if (!mAamp->GetBufUnderFlowStatus())
        {
            AAMPLOG_INFO("[video] underflow detected (deadline expired, rate=%.2f)", rate);
            mDeadlineArmed = false;  // Disarm — resume path will rearm.

            // Release the lock while calling into aamp to avoid priority inversion.
            lock.unlock();
            mAamp->SetBufferingState(true);
            PlaybackErrorType errorType = eGST_ERROR_UNDERFLOW;
            mAamp->SendAnomalyEvent(ANOMALY_WARNING, "%s %s",
                                    GetMediaTypeName(eMEDIATYPE_VIDEO),
                                    mAamp->getStringForPlaybackError(errorType));
            lock.lock();
        }
        else
        {
            // Already in underflow (NotifyPipelinePaused should have disarmed,
            // but guard against racing calls).
            mDeadlineArmed = false;
        }
    }

    mRunning.store(false);
}
