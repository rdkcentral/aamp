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
 * @brief Implements the AampUnderflowMonitor class for monitoring underflow conditions.
 */
#include "AampUnderflowMonitor.h"
#include "StreamAbstractionAAMP.h"
#include "AampEvent.h"
#include "AampDefine.h"
#include "AampConfig.h"
#include "AampLogManager.h"
#include "AampUtils.h"
#include <stdexcept>

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

AampUnderflowMonitor::AampUnderflowMonitor(PrivateInstanceAAMP* aamp)
    : mAamp(aamp)
{
    if (mAamp == nullptr)
    {
        throw std::invalid_argument("Aamp cannot be null");
    }
    if (mStream == nullptr)
    {
        throw std::invalid_argument("StreamAbstractionAAMP cannot be null");
    }
}

AampUnderflowMonitor::~AampUnderflowMonitor() {
    Stop();
}

void AampUnderflowMonitor::Start() {
    // Use unique_lock to allow unlock around join operations
    std::unique_lock<std::mutex> lock(mMutex);

    // If a previous thread exists but is not running, join it before starting a new
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

    try
    {
        mRunning.store(true);
        mThread = std::thread(&AampUnderflowMonitor::Run, this);
        AAMPLOG_INFO("AampUnderflowMonitor thread created [%zx]", GetPrintableThreadID(mThread));
    }
    catch(const std::exception& e)
    {
        mRunning.store(false);
        AAMPLOG_WARN("Failed to create AampUnderflowMonitor thread : %s", e.what());
        return;
    }

    // If the thread exited immediately (e.g., due to player state), ensure we join it to avoid a dangling joinable thread
    if (!mRunning.load() && mThread.joinable())
    {
        lock.unlock();
        mThread.join();
        AAMPLOG_WARN("AampUnderflowMonitor thread exited immediately after start; joined");
    }
}

void AampUnderflowMonitor::Stop()
{
    // Signal thread to stop
    mRunning.store(false);
    
    // Wait for thread to terminate
    if (mThread.joinable())
    {
        mThread.join();
        AAMPLOG_INFO("AampUnderflowMonitor thread joined");
    }
    
    // Nullify pointers under mutex to prevent any race with thread cleanup
    std::lock_guard<std::mutex> lock(mMutex);
    mAamp = nullptr;
    mStream = nullptr;
}

void AampUnderflowMonitor::Run()
{
    // Resolve configurable thresholds and polling intervals once
    const double kUnderflowDetectThresholdSec = mAamp->mConfig->GetConfigValue(eAAMPConfig_UnderflowDetectThresholdSec);
    const double kUnderflowResumeThresholdSec = mAamp->mConfig->GetConfigValue(eAAMPConfig_UnderflowResumeThresholdSec);
    const double kLowBufferSec = mAamp->mConfig->GetConfigValue(eAAMPConfig_UnderflowLowBufferSec);
    const double kHighBufferSec = mAamp->mConfig->GetConfigValue(eAAMPConfig_UnderflowHighBufferSec);
    const int kLowBufferPollMs = mAamp->mConfig->GetConfigValue(eAAMPConfig_UnderflowLowBufferPollMs);
    const int kMediumBufferPollMs = mAamp->mConfig->GetConfigValue(eAAMPConfig_UnderflowMediumBufferPollMs);
    const int kHighBufferPollMs = mAamp->mConfig->GetConfigValue(eAAMPConfig_UnderflowHighBufferPollMs);

    // Wait until playback enters PLAYING state or underflow becomes active; exit if playback stops
    while (mRunning.load()) {
        AAMPPlayerState state;
        bool shouldBreak = false;
        bool underflowStatus = false;
        
        {
            std::lock_guard<std::mutex> lock(mMutex);
            if (!mAamp) return; // Stop() was called
            
            state = mAamp->GetState();
            if (state == eSTATE_STOPPED || state == eSTATE_RELEASED || state == eSTATE_ERROR) {
                mRunning.store(false);
                return;
            }
            underflowStatus = mAamp->GetBufUnderFlowStatus();
            shouldBreak = (state == eSTATE_PLAYING || underflowStatus);
        }
        
        if (shouldBreak) {
            break;
        }
        
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

    while (mRunning.load()) {
        // Check player state and underflow status under mutex
        bool underflowActive;
        AAMPPlayerState playerState;
        float currentRate;
        double bufferedTimeSec;
        
        {
            std::lock_guard<std::mutex> lock(mMutex);
            if (!mAamp || !mStream) return; // Stop() was called
            
            underflowActive = mAamp->GetBufUnderFlowStatus();
            playerState = mAamp->GetState();
            // Exit when playback transitions to stopped/released/error/idle
            if (playerState == eSTATE_STOPPED || playerState == eSTATE_RELEASED || playerState == eSTATE_ERROR || playerState == eSTATE_IDLE) {
                break;
            }
            
            // Skip buffer-based underflow checks during trickplay or seeking
            currentRate = mAamp->rate;
            
            // Query buffered duration once and reuse for detection and sleep cadence
            bufferedTimeSec = mStream->GetBufferedVideoDurationSec();
            if (bufferedTimeSec < 0.0) bufferedTimeSec = 0.0;
        }
        
        const bool inPlayOrUnderflow = (playerState == eSTATE_PLAYING) || underflowActive;
        const bool isTrickplay = (currentRate != AAMP_NORMAL_PLAY_RATE && currentRate != AAMP_SLOWMOTION_RATE && currentRate != AAMP_RATE_PAUSE);
        const bool isSeekingState = (playerState == eSTATE_SEEKING);

        if (inPlayOrUnderflow) {
            // Video underflow detection/resume (query under mutex)
            bool trackDownloadsEnabled;
            bool sinkCacheEmpty;
            
            {
                std::lock_guard<std::mutex> lock(mMutex);
                if (!mAamp) return;
                trackDownloadsEnabled = mAamp->TrackDownloadsAreEnabled(eMEDIATYPE_VIDEO);
                sinkCacheEmpty = mAamp->IsSinkCacheEmpty(eMEDIATYPE_VIDEO);
            }

        if (!mAamp->GetBufUnderFlowStatus())
        {
            std::lock_guard<std::mutex> lock(mMutex);
            if (!mAamp) return;
            mAamp->interruptibleMsSleep(sleepMs);
        }
    }
    mRunning.store(false);
}

