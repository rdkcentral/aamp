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
 * @brief AampLatencyMonitor constuctor
 * @param[in] aamp - Pointer to the AAMP instance.
 */
AampLatencyMonitor::AampLatencyMonitor(PrivateInstanceAAMP* aamp)
	: mAamp{aamp}
	, mConfig{}
	, mThresholdMutex{}
	, mMinLatencyMs{0.0}
	, mTargetLatencyMs{0.0}
	, mMaxLatencyMs{0.0}
	, mAdaptiveShiftMs{0.0}
	, mState{State::kIdle}
	, mCurrentRate{1.0}
	, mCorrectionEnabled{true}
	, mBufferLowCount{0}
	, mBufferLowHitCount{0}
	, mUnderflowMutex{}
	, mUnderflowTimestamps{}
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
	// Validate preconditions and guard against misuse.
	if (mAamp == nullptr)
	{
		AAMPLOG_ERR("[LatencyMonitor] cannot start: null AAMP instance");
		return;
	}

	// Guard against double-start.
	if (mState != State::kIdle)
	{
		AAMPLOG_WARN("[LatencyMonitor] Start() called when already running");
		return;
	}

	mConfig = config;

	// Initialise the dynamic thresholds from the supplied config.
	{
		std::lock_guard<std::mutex> lock(mThresholdMutex);
		mMinLatencyMs    = config.minLatencyMs;
		mTargetLatencyMs = config.targetLatencyMs;
		mMaxLatencyMs    = config.maxLatencyMs;
		mAdaptiveShiftMs = 0.0;
	}

	mBufferLowCount    = 0;
	mBufferLowHitCount = 0;

	// Initial correction rate matches the normal rate.
	mCurrentRate.store(config.normalPlaybackRate);
	mCorrectionEnabled.store(true);

	{
		std::lock_guard<std::mutex> lock(mSleepMutex);
		mWakeupSignalled = false;
	}

	{
		std::lock_guard<std::mutex> lock(mUnderflowMutex);
		mUnderflowTimestamps.clear();
	}

	try
	{
		mState = State::kStarting;
		mThread = std::thread(&AampLatencyMonitor::Run, this);
		AAMPLOG_INFO("[LatencyMonitor] started [%zx]",
			GetPrintableThreadID(mThread));
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
	if (mState == State::kIdle)
	{
		return;
	}

	AAMPLOG_INFO("[LatencyMonitor] stopping");
	mState = State::kStopping;

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
	mState  = State::kIdle;
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

	mCorrectionEnabled.store(enabled);
	// Wake the worker so it can immediately reset the rate if needed.
	{
		std::lock_guard<std::mutex> lock(mSleepMutex);
		mWakeupSignalled = true;
		mSleepCv.notify_all();
	}
}

/**
 * @brief Dynamically update the latency thresholds.
 * @param[in] minMs - New minimum-latency threshold (ms).
 * @param[in] targetMs - New target-latency value (ms).
 * @param[in] maxMs - New maximum-latency threshold (ms).
 */
void AampLatencyMonitor::SetLatencyThresholds(double minMs, double targetMs, double maxMs)
{
	if (minMs <= 0.0 || targetMs < minMs || maxMs < targetMs)
	{
		AAMPLOG_ERR("[LatencyMonitor] invalid thresholds: min=%.0fms "
			"target=%.0fms max=%.0fms", minMs, targetMs, maxMs);
		return;
	}

	{
		std::lock_guard<std::mutex> lock(mThresholdMutex);
		mMinLatencyMs    = minMs;
		mTargetLatencyMs = targetMs;
		mMaxLatencyMs    = maxMs;
	}

	AAMPLOG_INFO("[LatencyMonitor] thresholds updated — min:%.0fms "
		"target:%.0fms max:%.0fms", minMs, targetMs, maxMs);
}

/**
 * @brief Notify the monitor that a video-underflow event occurred.
 */
void AampLatencyMonitor::NotifyUnderflow()
{
	auto now = std::chrono::steady_clock::now();
	{
		std::lock_guard<std::mutex> lock(mUnderflowMutex);
		mUnderflowTimestamps.push_back(now);
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
	mState = State::kRunning;
	AAMPLOG_INFO("[LatencyMonitor] worker running — initial delay %d ms", mConfig.monitorDelayMs);

	// Initial delay to avoid disturbing startup.
	WaitMs(mConfig.monitorDelayMs);

	const double normalRate = mConfig.normalPlaybackRate;
	const double maxRate    = mConfig.maxPlaybackRate;
	const double minRate    = mConfig.minPlaybackRate;

	// Track whether the last rate change moved toward normal (for telemetry).
	bool latencyCorrected      = true;  // true -> within band at start
	bool bufferCorrectionActive = false; // true -> last correction was buffer-driven
	bool reportEvent           = false;

	// Main loop: runs until Stop() sets state to kStopping.
	while (mState == State::kRunning)
	{
		WaitMs(mConfig.monitorIntervalMs);

		// If Stop() was called while we were sleeping, exit now.
		if (mState != State::kRunning)
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
		const long   latencyMs   = mAamp->GetCurrentLatency();
		const double bufferSec   = mAamp->GetBufferedDurationSecs();

		// bufferSec == -1 → transient period-switch; treat as "enough".
		const bool bufferEnough    = (bufferSec < 0.0)
								   || (bufferSec >= mConfig.targetBufferSec);
		const bool bufferCritical  = (bufferSec >= 0.0)
								   && (bufferSec <  mConfig.minBufferSec);

		// Confirm critical-low state across several consecutive polls to
		// avoid reacting to momentary dips (mirrors AAMP_LLD_LOW_BUFF_CHECK_COUNT).
		bool bufferLowHit = false;
		if (bufferCritical)
		{
			++mBufferLowCount;
			if (mBufferLowCount >= kBufferLowHitCount)
			{
				bufferLowHit = true;
				++mBufferLowHitCount;
				mAamp->profiler.SetLLDLowBufferParam(
					static_cast<double>(latencyMs),
					bufferSec,
					mCurrentRate.load(),
					mAamp->mhAbrManager.GetNetworkBandwidth(),
					static_cast<double>(mBufferLowHitCount));
				mBufferLowCount = 0;
			}
		}
		else
		{
			// Buffer recovered — reset streak counters.
			mBufferLowCount    = 0;
			mBufferLowHitCount = 0;
		}

		// Read (potentially adapted) latency thresholds.
		AdaptLatencyThresholds();

		double minLatMs, targetLatMs, maxLatMs;
		{
			std::lock_guard<std::mutex> lock(mThresholdMutex);
			minLatMs    = mMinLatencyMs;
			targetLatMs = mTargetLatencyMs;
			maxLatMs    = mMaxLatencyMs;
		}

		AAMPLOG_INFO("[LatencyMonitor] latency=%ldms buffer=%.2fs "
			"rate=%.2f min=%.0fms target=%.0fms max=%.0fms "
			"bufEnough=%d bufCritical=%d",
			latencyMs, bufferSec, mCurrentRate.load(),
			minLatMs, targetLatMs, maxLatMs,
			bufferEnough, bufferCritical);

		// Check conditions for rate changes, and determine the new desired rate and whether to report a telemetry event.
		double desiredRate = mCurrentRate.load();
		reportEvent        = false;

		if ((latencyMs > static_cast<long>(maxLatMs)) && bufferEnough)
		{
			// Latency above band and buffer is healthy — speed up.
			if (latencyCorrected)
			{
				latencyCorrected = false;
				reportEvent      = true;
			}
			desiredRate = maxRate;
		}
		else if ((latencyMs < static_cast<long>(minLatMs)) ||
				 (bufferLowHit && (mCurrentRate.load() != minRate)))
		{
			// Latency below band OR buffer critically low — slow down.
			if (latencyMs < static_cast<long>(minLatMs) && !latencyCorrected)
			{
				// Latency crossed back inside the band from above; report.
				latencyCorrected = true;
				reportEvent      = true;
			}
			else
			{
				// Buffer-driven only — no event.
				bufferCorrectionActive = true;
			}
			desiredRate = minRate;
		}
		else if ((latencyMs <= static_cast<long>(targetLatMs)) &&
				 (mCurrentRate.load() == maxRate))
		{
			// Latency caught up to target while running fast — return to normal.
			latencyCorrected       = true;
			reportEvent            = true;
			desiredRate            = normalRate;
		}
		else if ((latencyMs >= static_cast<long>(targetLatMs)) &&
				 (mCurrentRate.load() == minRate) &&
				 (bufferSec > mConfig.minBufferSec))
		{
			// Latency rose to target while running slow — return to normal.
			if (bufferCorrectionActive)
			{
				// Buffer-driven correction finished — no event needed.
				bufferCorrectionActive = false;
				reportEvent            = false;
			}
			else
			{
				latencyCorrected = true;
				reportEvent      = true;
			}
			desiredRate = normalRate;
		}
		else if ((mCurrentRate.load() == maxRate) && !bufferEnough)
		{
			// Running fast but buffer is no longer healthy — back off.
			latencyCorrected = false;
			reportEvent      = false;
			desiredRate      = normalRate;
		}
		// else: no change.

		// Apply the new rate if it changed.
		if (desiredRate != mCurrentRate.load())
		{
			ApplyRate(desiredRate, reportEvent);
		}
	}

	// Worker is exiting — reset the sink rate to normal so the stream does
	// not stay at a correction speed after the monitor terminates.
	ResetToNormalRate();

	mState = State::kIdle;
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
			return mWakeupSignalled || (mState == State::kStopping);
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
			return mWakeupSignalled || (mState == State::kStopping);
		});
	mWakeupSignalled = false;
}

/**
 * @brief Apply the specified playback rate to the stream sink.
 * @param[in] newRate - The new playback rate to apply.
 * @param[in] reportTelemetry - Whether to report this change as a telemetry event.
 */
void AampLatencyMonitor::ApplyRate(double newRate, bool reportTelemetry)
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
	if (reportTelemetry)
	{
		mAamp->UpdateVideoEndMetrics(newRate);
		mAamp->SendAnomalyEvent(ANOMALY_WARNING,
			"[LatencyMonitor] rate changed to:%.2f", newRate);
	}

	mAamp->profiler.IncrementChangeCount(Count_RateCorrection);

	AAMPLOG_WARN("[LatencyMonitor] rate -> %.2f (telemetry=%d)",
		newRate, reportTelemetry);
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
 * @brief Adjust the latency thresholds dynamically based on recent underflow events.
 */
void AampLatencyMonitor::AdaptLatencyThresholds()
{
	// If >= kUnderflowThreshold underflows occurred within the last
	// kUnderflowWindowSec seconds, shift the band upward by
	// kUnderflowShiftMs (capped at kMaxAdaptiveShiftMs total).
	auto now = std::chrono::steady_clock::now();
	auto windowStart = now - std::chrono::seconds(kUnderflowWindowSec);

	int recentCount = 0;
	{
		std::lock_guard<std::mutex> lock(mUnderflowMutex);

		// Prune stale entries.
		while (!mUnderflowTimestamps.empty()
			&& mUnderflowTimestamps.front() < windowStart)
		{
			mUnderflowTimestamps.pop_front();
		}

		recentCount = static_cast<int>(mUnderflowTimestamps.size());
	}

	if (recentCount < kUnderflowThreshold)
	{
		return;
	}

	std::lock_guard<std::mutex> lock(mThresholdMutex);

	if (mAdaptiveShiftMs >= kMaxAdaptiveShiftMs)
	{
		// Already at the maximum adaptive shift; nothing more to do.
		return;
	}

	const double remaining = kMaxAdaptiveShiftMs - mAdaptiveShiftMs;
	const double shift     = std::min(kUnderflowShiftMs, remaining);

	mMinLatencyMs    += shift;
	mTargetLatencyMs += shift;
	mMaxLatencyMs    += shift;
	mAdaptiveShiftMs += shift;

	AAMPLOG_WARN("[LatencyMonitor] adaptive shift +%.0fms applied "
		"(total=%.0fms) — min:%.0fms target:%.0fms max:%.0fms",
		shift, mAdaptiveShiftMs,
		mMinLatencyMs, mTargetLatencyMs, mMaxLatencyMs);

	// Clear the window so the same batch of underflows doesn't trigger
	// repeated shifts.
	{
		std::lock_guard<std::mutex> ufLock(mUnderflowMutex);
		mUnderflowTimestamps.clear();
	}
}
