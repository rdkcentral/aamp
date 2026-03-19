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
 * @file AampLatencyMonitor.cpp
 * @brief Implementation of AampLatencyMonitor
 */

#include "AampLatencyMonitor.h"
#include "priv_aamp.h"
#include "AampStreamSinkManager.h"
#include "AampDefine.h"
#include "AampLogManager.h"
#include "AAMPAnomalyMessageType.h"
#include "AampProfiler.h"

#include <algorithm>
#include <cassert>

/**
 * @brief AampLatencyMonitor constructor
 * @param[in] aamp - Pointer to the AAMP instance.
 */
AampLatencyMonitor::AampLatencyMonitor(PrivateInstanceAAMP* aamp)
	: mAamp{aamp}
	, mConfig{}
	, mThresholdMutex{}
	, mMinLatencyMs{0.0}
	, mTargetLatencyMs{0.0}
	, mMaxLatencyMs{0.0}
	, mLatencyIncrementAccumulatedMs{0.0}
	, mStartStopMutex{}
	, mState{State::kIdle}
	, mCurrentRate{1.0}
	, mCorrectionEnabled{true}
	, mSleepMutex{}
	, mSleepCv{}
	, mWakeupSignalled{false}
	, mThread{}
{
}

/**
 * @brief AampLatencyMonitor destructor.
 */
AampLatencyMonitor::~AampLatencyMonitor()
{
	Stop();
}

/**
 * @brief Start the latency monitor worker thread.
 * @param[in] config - LatencyConfig struct containing configuration parameters.
 */
void AampLatencyMonitor::Start(const LatencyConfig& config)
{
	std::lock_guard<std::mutex> startStopLock(mStartStopMutex);

	// Validate preconditions and guard against misuse.
	if (mAamp == nullptr)
	{
		AAMPLOG_ERR("[LatencyMonitor] cannot start: null AAMP instance");
		return;
	}

	// Guard against double-start.  The mutex guarantees no concurrent
	// Start()/Stop() can change mState between this check and the assignment
	// below, so a plain load is sufficient here.
	if (mState.load() != State::kIdle)
	{
		AAMPLOG_WARN("[LatencyMonitor] Start() called when already running (state=%d)",
			static_cast<int>(mState.load()));
		return;
	}

	mConfig = config;

	// Initialise the dynamic thresholds from the supplied config.
	{
		std::lock_guard<std::mutex> lock(mThresholdMutex);
		mMinLatencyMs    = config.minLatencyMs;
		mTargetLatencyMs = config.targetLatencyMs;
		mMaxLatencyMs    = config.maxLatencyMs;
		mLatencyIncrementAccumulatedMs = 0.0;
	}

	// Initial correction rate matches the normal rate.
	mCurrentRate.store(config.normalPlaybackRate);
	mCorrectionEnabled.store(true);

	{
		std::lock_guard<std::mutex> lock(mSleepMutex);
		mWakeupSignalled = false;
	}

	try
	{
		mState = State::kStarting;
		mThread = std::thread(&AampLatencyMonitor::Run, this);
		AAMPLOG_INFO("[LatencyMonitor] started [%zx]", GetPrintableThreadID(mThread));
	}
	catch (const std::exception& ex)
	{
		AAMPLOG_ERR("[LatencyMonitor] failed to start: %s", ex.what());
		mState = State::kIdle;
	}
}

/**
 * @brief Stop the latency monitor worker thread.
 */
void AampLatencyMonitor::Stop()
{
	std::lock_guard<std::mutex> startStopLock(mStartStopMutex);

	if (mState.load() == State::kIdle)
	{
		return;
	}

	AAMPLOG_INFO("[LatencyMonitor] stopping");
	mState.store(State::kStopping);

	// Wake the sleeping worker so it exits promptly.
	{
		std::lock_guard<std::mutex> lock(mSleepMutex);
		mWakeupSignalled = true;
		mSleepCv.notify_all();
	}

	if (mThread.joinable())
	{
		try
		{
			mThread.join();
		}
		catch (const std::exception& ex)
		{
			AAMPLOG_ERR("[LatencyMonitor] join failed: %s", ex.what());
		}
	}

	// The worker resets the rate to normal before exiting, but if it died
	// before doing so, ensure a clean state here too.
	mCurrentRate.store(mConfig.normalPlaybackRate);

	// Reset dynamic thresholds back to config defaults so that if the monitor
	// is reused the next Start() begins from a known baseline.
	{
		std::lock_guard<std::mutex> lock(mThresholdMutex);
		ResetLatencyThresholdsLocked();
	}

	mState.store(State::kIdle);
	AAMPLOG_INFO("[LatencyMonitor] stopped");
}

/**
 * @brief Enable or disable rate correction at runtime.
 * @param[in] enabled - true to allow corrections, false to suppress them.
 */
void AampLatencyMonitor::EnableRateCorrection(bool enabled)
{
	AAMPLOG_INFO("[LatencyMonitor] state[%d] rate correction %s",
		static_cast<int>(mState.load()), enabled ? "enabled" : "disabled");

	// If the desired state matches the current state, no action is needed.
	if (mCorrectionEnabled.load() == enabled)
	{
		return;
	}

	// Reset dynamic thresholds back to config defaults so that if the monitor
	// resumes when seeked to live, the thresholds are used from a known baseline.
	{
		std::lock_guard<std::mutex> lock(mThresholdMutex);
		ResetLatencyThresholdsLocked();
	}

	mCorrectionEnabled.store(enabled);
	// Wake the worker so it can immediately reset the rate if needed.
	{
		std::lock_guard<std::mutex> lock(mSleepMutex);
		mWakeupSignalled = true;
		mSleepCv.notify_all();
	}
}

/**
 * @brief Get the current playback rate applied by the monitor.
 * @return Current playback rate.
 */
double AampLatencyMonitor::GetCurrentRate() const
{
	return mCurrentRate.load();
}

/**
 * @brief Worker thread function that monitors latency and applies rate corrections.
 */
void AampLatencyMonitor::Run()
{
	// Transition kStarting → kRunning.  If Stop() was called between Start()
	// spawning the thread and here, state is already kStopping — exit
	// immediately so Stop()'s join() returns and it can finalize cleanup.
	State expected = State::kStarting;
	if (!mState.compare_exchange_strong(expected, State::kRunning))
	{
		AAMPLOG_WARN("[LatencyMonitor] worker aborted early (state=%d)",
			static_cast<int>(expected));
		return;
	}
	AAMPLOG_INFO("[LatencyMonitor] worker running — initial delay %d ms", mConfig.monitorDelayMs);

	// Initial delay to avoid disturbing startup.
	WaitMs(mConfig.monitorDelayMs);

	const double normalRate = mConfig.normalPlaybackRate;
	const double maxRate    = mConfig.maxPlaybackRate;
	const double minRate    = mConfig.minPlaybackRate;

	// Main loop: runs until Stop() sets state to kStopping.
	while (mState.load() == State::kRunning)
	{
		WaitMs(mConfig.monitorIntervalMs);

		// If Stop() was called while we were sleeping, exit now.
		if (mState.load() != State::kRunning)
		{
			break;
		}

		// If rate correction is disabled, hold the rate at normal and skip monitoring.
		if (!mCorrectionEnabled.load())
		{
			AAMPLOG_DEBUG("[LatencyMonitor] correction suppressed ");
			ResetToNormalRate();
			// Once rate correction is disabled, the monitor thread sleeps indefinitely until either
			// correction is re-enabled or Stop() is called. This prevents any rate changes during
			// track switches or ad insertions, which could cause playback issues.
			WaitUntilSignalled();
			continue;
		}

		// Skip monitoring if not currently playing, to avoid reacting to transient startup conditions
		AAMPPlayerState playerState = mAamp->GetState();
		if (playerState != eSTATE_PLAYING)
		{
			AAMPLOG_DEBUG("[LatencyMonitor] state=%d, skipping poll", playerState);
			continue;
		}

		// Skip latency correction when CDAI ad is playing to avoid disrupting the ad experience.
		// Latency monitor will be disabled for trickplay, so avoiding that check here.
		if (mAamp->IsAdPlaying())
		{
			AAMPLOG_DEBUG("[LatencyMonitor] CDAI ad playing, skipping poll");
			ResetToNormalRate();
			continue;
		}

		// Collect measurements.
		const long   latencyMs   = mAamp->GetCurrentLatencyMs();
		const double bufferMs   = mAamp->GetBufferedDurationSecs() * 1000.0;

		if (bufferMs  < mConfig.correctionActivationThresholdMs)
		{
			AAMPLOG_DEBUG("[LatencyMonitor] Buffer level[%.3f] is too low for latency correction, skipping poll",
						 bufferMs);
			ResetToNormalRate();
			continue;
		}

		double minLatMs, targetLatMs, maxLatMs;
		{
			std::lock_guard<std::mutex> lock(mThresholdMutex);
			minLatMs    = mMinLatencyMs;
			targetLatMs = mTargetLatencyMs;
			maxLatMs    = mMaxLatencyMs;
		}

		AAMPLOG_INFO("[LatencyMonitor] latency=%ldms buffer=%.3fms "
			"rate=%.2f min=%.0fms target=%.0fms max=%.0fms ",
			latencyMs, bufferMs, mCurrentRate.load(),
			minLatMs, targetLatMs, maxLatMs);

		// Determine the desired rate based on current latency and buffer state.
		double desiredRate = mCurrentRate.load();

		if (latencyMs > static_cast<long>(maxLatMs))
		{
			// Latency above band — speed up.
			desiredRate = maxRate;
		}
		else if (latencyMs < static_cast<long>(minLatMs))
		{
			// Latency below band — slow down.
			desiredRate = minRate;
		}
		else if ((latencyMs <= static_cast<long>(targetLatMs)) &&
				 (mCurrentRate.load() == maxRate))
		{
			// Latency caught up to target while running fast — return to normal.
			desiredRate = normalRate;
		}
		else if ((latencyMs >= static_cast<long>(targetLatMs)) &&
				 (mCurrentRate.load() == minRate))
		{
			// Latency rose to target while running slow — return to normal.
			desiredRate = normalRate;
		}
		// else: no change.

		// Apply the new rate if it changed.
		if (desiredRate != mCurrentRate.load())
		{
			ApplyRate(desiredRate);
		}
	}

	// Worker is exiting — reset the sink rate to normal so the stream does
	// not stay at a correction speed after the monitor terminates.
	ResetToNormalRate();

	// Do NOT write mState here.  Stop() owns the kStopping → kIdle
	// transition after join() returns, ensuring a clean hand-off.
	AAMPLOG_INFO("[LatencyMonitor] worker exited");
}

/**
 * @brief Sleep for the specified duration or until signalled to wake or stop.
 * @param[in] ms - Duration to sleep in milliseconds.
 */
void AampLatencyMonitor::WaitMs(int ms)
{
	std::unique_lock<std::mutex> lock(mSleepMutex);
	mSleepCv.wait_for(lock,
		std::chrono::milliseconds(ms),
		[this]() {
			return mWakeupSignalled || (mState.load() == State::kStopping);
		});
	mWakeupSignalled = false;
}

/**
 * @brief Sleep indefinitely until signalled to wake or stop.
 */
void AampLatencyMonitor::WaitUntilSignalled()
{
	std::unique_lock<std::mutex> lock(mSleepMutex);
	mSleepCv.wait(lock,
		[this]() {
			return mWakeupSignalled || (mState.load() == State::kStopping);
		});
	mWakeupSignalled = false;
}

/**
 * @brief Apply the specified playback rate to the stream sink.
 * @param[in] newRate - The new playback rate to apply.
 */
void AampLatencyMonitor::ApplyRate(double newRate)
{
	StreamSink* sink = AampStreamSinkManager::GetInstance().GetStreamSink(mAamp);
	if (sink == nullptr)
	{
		AAMPLOG_WARN("[LatencyMonitor] no StreamSink — cannot apply rate %.2f",
			newRate);
		return;
	}

	if (!sink->SetPlayBackRate(newRate))
	{
		AAMPLOG_WARN("[LatencyMonitor] SetPlayBackRate(%.2f) failed", newRate);
		return;
	}

	mCurrentRate.store(newRate);

	// Telemetry on rate-correction events.
	mAamp->UpdateVideoEndMetrics(newRate);
	mAamp->SendAnomalyEvent(ANOMALY_WARNING, "[LatencyMonitor] rate changed to:%.2f", newRate);

	mAamp->profiler.IncrementChangeCount(Count_RateCorrection);

	AAMPLOG_WARN("[LatencyMonitor] rate -> %.2f", newRate);
}

/**
 * @brief Reset the playback rate to the normal rate defined in the config.
 */
void AampLatencyMonitor::ResetToNormalRate()
{
	const double normalRate = mConfig.normalPlaybackRate;
	if (mCurrentRate.load() == normalRate)
	{
		return;
	}

	StreamSink* sink = AampStreamSinkManager::GetInstance().GetStreamSink(mAamp);
	if (sink == nullptr)
	{
		return;
	}

	if (sink->SetPlayBackRate(normalRate))
	{
		mCurrentRate.store(normalRate);
		AAMPLOG_INFO("[LatencyMonitor] rate reset to normal (%.2f)", normalRate);
	}
	else
	{
		AAMPLOG_WARN("[LatencyMonitor] failed to reset rate to normal");
	}
}

/**
 * @brief Notify the monitor that rebuffering has started.
 */
void AampLatencyMonitor::OnRebufferingStart()
{
	if (mConfig.rebufferingLatencyStepMs <= 0.0)
	{
		return;
	}

	std::lock_guard<std::mutex> lock(mThresholdMutex);

	double newAccumulated = mLatencyIncrementAccumulatedMs
		+ mConfig.rebufferingLatencyStepMs;

	if (mConfig.rebufferingLatencyMaxIncrementMs > 0.0)
	{
		newAccumulated = std::min(newAccumulated,
			mConfig.rebufferingLatencyMaxIncrementMs);
	}

	mLatencyIncrementAccumulatedMs = newAccumulated;
	mMinLatencyMs    = mConfig.minLatencyMs    + mLatencyIncrementAccumulatedMs;
	mTargetLatencyMs = mConfig.targetLatencyMs + mLatencyIncrementAccumulatedMs;
	mMaxLatencyMs    = mConfig.maxLatencyMs    + mLatencyIncrementAccumulatedMs;

	AAMPLOG_INFO("[LatencyMonitor] rebuffering: thresholds shifted +%.0fms "
		"(accumulated=%.0fms) -> min=%.0fms target=%.0fms max=%.0fms",
		mConfig.rebufferingLatencyStepMs, mLatencyIncrementAccumulatedMs,
		mMinLatencyMs, mTargetLatencyMs, mMaxLatencyMs);
}

/**
 * @brief Return the current effective latency thresholds.
 */
std::tuple<double, double, double> AampLatencyMonitor::GetCurrentThresholds() const
{
	std::lock_guard<std::mutex> lock(mThresholdMutex);
	return {mMinLatencyMs, mTargetLatencyMs, mMaxLatencyMs};
}

/**
 * @brief Reset the three dynamic thresholds to config defaults.
 * @pre mThresholdMutex must be held by the caller.
 */
void AampLatencyMonitor::ResetLatencyThresholdsLocked()
{
	mMinLatencyMs    = mConfig.minLatencyMs;
	mTargetLatencyMs = mConfig.targetLatencyMs;
	mMaxLatencyMs    = mConfig.maxLatencyMs;
	mLatencyIncrementAccumulatedMs = 0.0;
}
