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


AampUnderflowMonitor::AampUnderflowMonitor(StreamAbstractionAAMP* stream, PrivateInstanceAAMP* aamp)
: mStream(stream), mAamp(aamp)
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

void AampUnderflowMonitor::Start()
{
    std::lock_guard<std::mutex> lock(mStartStopMutex);

    if (mUnderflowMonitorThread.joinable())
    {
        if (mRunning.load())
        {
            AAMPLOG_INFO("AampUnderflowMonitor already running; skipping start");
            return;
        }
        mUnderflowMonitorThread.join();
        AAMPLOG_INFO("AampUnderflowMonitor previous thread joined before restart");
    }

    try
    {
        {
            std::lock_guard<std::mutex> sleepLock(mSleepMutex);
            mWakeupSignalled = false;
        }
        mRunning.store(true);
        mUnderflowMonitorThread = std::thread(&AampUnderflowMonitor::Run, this);
        AAMPLOG_INFO("AampUnderflowMonitor thread created [%zx]", GetPrintableThreadID(mUnderflowMonitorThread));
    }
    catch(const std::exception& e)
    {
        mRunning.store(false);
        AAMPLOG_WARN("Failed to create AampUnderflowMonitor thread : %s", e.what());
    }
}

void AampUnderflowMonitor::Stop()
{
    std::lock_guard<std::mutex> lock(mStartStopMutex);
    mRunning.store(false);
    {
        std::lock_guard<std::mutex> sleepLock(mSleepMutex);
        mWakeupSignalled = true;
        mSleepCv.notify_all();
    }
    if (mUnderflowMonitorThread.joinable())
    {
        mUnderflowMonitorThread.join();
        AAMPLOG_INFO("AampUnderflowMonitor thread joined");
    }
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

    while (mRunning.load())
    {
        AAMPPlayerState playerState = mAamp->GetState();
        bool underflowActive = mAamp->GetBufUnderFlowStatus();
        double bufferedTimeSec = 0.0;

        const bool isPlayingOrUnderflow = (playerState == eSTATE_PLAYING) || underflowActive;
        if (isPlayingOrUnderflow)
        {
            bufferedTimeSec = mStream->GetBufferedVideoDurationSec();
            if (bufferedTimeSec < 0.0) bufferedTimeSec = 0.0;

            const float currentRate = mAamp->rate;
            const bool isTrickplay = (currentRate != AAMP_NORMAL_PLAY_RATE && currentRate != AAMP_SLOWMOTION_RATE && currentRate != AAMP_RATE_PAUSE);
            const bool isSeekingState = (playerState == eSTATE_SEEKING);

            const bool sinkCacheEmpty = mAamp->IsSinkCacheEmpty(eMEDIATYPE_VIDEO);
            const bool allowBufferCheck = (!isTrickplay && !isSeekingState);

            // Detection: only when not in trickplay/seeking
            if (allowBufferCheck && (bufferedTimeSec <= kUnderflowDetectThresholdSec || sinkCacheEmpty))
            {
                if (!underflowActive)
                {
                    AAMPLOG_INFO("[video] underflow detected. buffered=%.3f cacheEmpty=%d (rate=%.2f)", bufferedTimeSec, (int)sinkCacheEmpty, currentRate);
                    mAamp->SetBufferingState(true);
                }
            }
            else if (!allowBufferCheck)
            {
                AAMPLOG_TRACE("[video] skipping underflow detection (rate=%.2f, trickplay=%d, seeking=%d). buffered=%.3f",
                    currentRate, (int)isTrickplay, (int)isSeekingState, bufferedTimeSec);
            }

            // Resume: always evaluate when underflow is active so it can clear during trickplay/seeking
            if (underflowActive)
            {
                const bool pipelinePaused = mAamp->mSinkPaused.load();
                if (pipelinePaused)
                {
                    if (bufferedTimeSec >= kUnderflowResumeThresholdSec && !sinkCacheEmpty)
                    {
                        AAMPLOG_INFO("[video] underflow ended. buffered=%.3f cacheEmpty=%d", bufferedTimeSec, (int)sinkCacheEmpty);
                        mAamp->SetBufferingState(false);
                    }
                    else
                    {
                        AAMPLOG_INFO("[video] waiting to end underflow. buffered=%.3f cacheEmpty=%d", bufferedTimeSec, (int)sinkCacheEmpty);
                    }
                }
            }
            // Audio underflow is not handled currently as we are aligning with the existing behavior
        }

        const int sleepMs = (bufferedTimeSec < kLowBufferSec)   ? kLowBufferPollMs
                           : (bufferedTimeSec >= kHighBufferSec) ? kHighBufferPollMs
                           : kMediumBufferPollMs;
        WaitMs(sleepMs);
    }
    mRunning.store(false);
}

void AampUnderflowMonitor::WaitMs(int ms)
{
    std::unique_lock<std::mutex> lk(mSleepMutex);
    mSleepCv.wait_for(lk,
        std::chrono::milliseconds(ms),
        [this]() {
            return mWakeupSignalled || !mRunning.load();
        });
    mWakeupSignalled = false;
}

