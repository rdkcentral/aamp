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
 * @file AampLatencyMonitor.h
 * @brief Unified live-stream latency monitor for HLS and DASH.
 *
 * Replaces:
 *   - PrivateInstanceAAMP::RateCorrectionWorkerThread (standard-latency HLS)
 *   - StreamAbstractionAAMP_MPD::MonitorLatency (LL-DASH)
 *
 * Usage:
 * @code
 *   AampLatencyMonitor monitor;
 *   monitor.Start(config, &myLatencySource, aamp);
 *   // ... playback ...
 *   monitor.Stop();
 * @endcode
 */

#ifndef AAMP_LATENCY_MONITOR_H
#define AAMP_LATENCY_MONITOR_H

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>
#include <AampDefine.h>

class PrivateInstanceAAMP;

// ---------------------------------------------------------------------------
// LatencyConfig
// ---------------------------------------------------------------------------

/**
 * @brief Configuration bundle for one AampLatencyMonitor session.
 *
 * All latency values are in milliseconds; buffer thresholds are in seconds.
 * Defaults match the legacy HLS rate-correction defaults.
 */
struct LatencyConfig
{
	/// Playback rate applied during normal (in-band) operation.
	double normalPlaybackRate {DEFAULT_NORMAL_RATE_CORRECTION_SPEED};
	/// Slow-down correction rate applied when latency is too low.
	double minPlaybackRate {DEFAULT_MIN_RATE_CORRECTION_SPEED};
	/// Speed-up correction rate applied when latency is too high.
	double maxPlaybackRate {DEFAULT_MAX_RATE_CORRECTION_SPEED};

	/// Lower bound of the latency dead-band; trigger slow-down below this.
	double minLatencyMs {DEFAULT_MIN_LOW_LATENCY * 1000};
	/// Desired latency; used as the hysteresis pivot when returning to normal.
	double targetLatencyMs {DEFAULT_TARGET_LOW_LATENCY * 1000};
	/// Upper bound of the latency dead-band; trigger speed-up above this.
	double maxLatencyMs {DEFAULT_MAX_LOW_LATENCY * 1000};

	/// Buffer level below which a speed-up is suppressed (not enough data).
	double targetBufferSec {DEFAULT_TARGET_BUFFER_LOW_LATENCY};
	/**
	 * Buffer level below which the rate is immediately reduced to allow
	 * buffer recovery, regardless of current latency.
	 */
	double minBufferSec {DEFAULT_MIN_BUFFER_LOW_LATENCY};

	/// One-shot startup delay (ms) before the first latency check fires.
	int monitorDelayMs {DEFAULT_LATENCY_MONITOR_DELAY * 1000};
	/// Polling interval (ms) between successive latency evaluations.
	int monitorIntervalMs {DEFAULT_LATENCY_MONITOR_INTERVAL * 1000};

	LatencyConfig() = default;

	// Constructor with params
	LatencyConfig(double normalRate, double minRate, double maxRate,
				  double minLatency, double targetLatency, double maxLatency,
				  double targetBuffer, double minBuffer,
				  int monitorDelay, int monitorInterval)
		: normalPlaybackRate(normalRate)
		, minPlaybackRate(minRate)
		, maxPlaybackRate(maxRate)
		, minLatencyMs(minLatency)
		, targetLatencyMs(targetLatency)
		, maxLatencyMs(maxLatency)
		, targetBufferSec(targetBuffer)
		, minBufferSec(minBuffer)
		, monitorDelayMs(monitorDelay)
		, monitorIntervalMs(monitorInterval)
	{}
};

// ---------------------------------------------------------------------------
// AampLatencyMonitor
// ---------------------------------------------------------------------------

/**
 * @class AampLatencyMonitor
 * @brief Monitors live-stream latency and applies playback-rate correction.
 *
 * ## Purpose
 * Maintains end-to-end stream latency near a configurable target by adjusting
 * the GStreamer playback rate.
 *
 * ## State machine
 * @code
 *   kIdle  --Start()--> kStarting --thread launched--> kRunning
 *   kRunning --Stop()--> kStopping --thread joins--> kIdle
 * @endcode
 *
 * ## Rate-correction rules
 * 1. Speed up  (maxRate): latency > maxLatency AND buffer >= targetBuffer.
 * 2. Slow down (minRate): latency < minLatency
 *                         OR buffer critically low (< minBuffer).
 * 3. Return to normal:    latency back within band,
 *                         OR buffer low while running at max rate.
 *
 * ## Dynamic latency adaptation
 * If NotifyUnderflow() is called >= kUnderflowThreshold times within
 * kUnderflowWindowSec seconds, the latency band (min/target/max) is shifted
 * upward by kUnderflowShiftMs to give the pipeline more headroom.  The total
 * upward shift is capped at kMaxAdaptiveShiftMs.
 *
 * ## Thread safety
 * Start() and Stop() may be called from any thread.
 * EnableRateCorrection(), SetLatencyThresholds() and NotifyUnderflow() are
 * also thread-safe.
 */
class AampLatencyMonitor
{
public:
	// -----------------------------------------------------------------------
	// Construction / destruction
	// -----------------------------------------------------------------------

	// Delete default constructor to require an AAMP instance.
	AampLatencyMonitor() = delete;

	/// @brief Constructs a stopped (kIdle) monitor.
	AampLatencyMonitor(PrivateInstanceAAMP* aamp);

	/**
	 * @brief Destructor.  Calls Stop() to ensure the worker thread exits.
	 */
	~AampLatencyMonitor();

	/// Non-copyable — owns a std::thread.
	AampLatencyMonitor(const AampLatencyMonitor&)            = delete;
	/// Non-copyable — owns a std::thread.
	AampLatencyMonitor& operator=(const AampLatencyMonitor&) = delete;

	// -----------------------------------------------------------------------
	// Lifecycle
	// -----------------------------------------------------------------------

	/**
	 * @brief Start the latency-monitor worker thread.
	 *
	 * No-op if the monitor is already running.  The @p aamp instance
	 * must remain valid until Stop() returns.
	 *
	 * @param config  Rate and latency thresholds for this session.
	 */
	void Start(const LatencyConfig& config);

	/**
	 * @brief Stop the worker thread and block until it exits.
	 *
	 * Safe to call when the monitor is already stopped.
	 * After Stop() returns, @p aamp may be destroyed.
	 */
	void Stop();

	// -----------------------------------------------------------------------
	// Runtime control
	// -----------------------------------------------------------------------

	/**
	 * @brief Enable or disable rate correction at runtime.
	 *
	 * Call with @c false during track switches or CDAI ad insertion to
	 * prevent disruptive rate changes; call with @c true afterward to
	 * resume normal monitoring.  While disabled the monitor resets the
	 * playback rate to normalPlaybackRate if it is not already there.
	 *
	 * @param enabled  @c true to allow corrections, @c false to suppress them.
	 */
	void EnableRateCorrection(bool enabled);

	/**
	 * @brief Dynamically update the latency target band.
	 *
	 * May be called from any thread at any time, for example after the
	 * caller picks up a new ServiceDescription element from the manifest.
	 * The change takes effect at the next monitor poll.
	 *
	 * @param minMs    New minimum-latency threshold (ms).
	 * @param targetMs New target-latency value     (ms).
	 * @param maxMs    New maximum-latency threshold (ms).
	 */
	void SetLatencyThresholds(double minMs, double targetMs, double maxMs);

	/**
	 * @brief Notify the monitor that a video-underflow event occurred.
	 *
	 * If kUnderflowThreshold notifications arrive within kUnderflowWindowSec
	 * seconds, the latency band is shifted upward by kUnderflowShiftMs (up to
	 * kMaxAdaptiveShiftMs cumulative) to prevent further starvation.
	 */
	void NotifyUnderflow();

	// -----------------------------------------------------------------------
	// Observers
	// -----------------------------------------------------------------------

	/**
	 * @brief Returns the playback rate most recently applied by this monitor.
	 *
	 * Thread-safe (atomic load).
	 */
	double GetCurrentRate() const;

private:
	// -----------------------------------------------------------------------
	// State machine
	// -----------------------------------------------------------------------

	/**
	 * @brief Internal monitor state.
	 *
	 * Transitions:
	 *   kIdle → kStarting → kRunning → kStopping → kIdle
	 */
	enum class State
	{
		kIdle,      ///< Thread not running.
		kStarting,  ///< Start() called; thread launching.
		kRunning,   ///< Thread actively monitoring.
		kStopping   ///< Stop() called; thread draining.
	};

	// -----------------------------------------------------------------------
	// Adaptive-latency constants
	// -----------------------------------------------------------------------

	/// Rolling window (seconds) in which underflows are counted.
	static constexpr int    kUnderflowWindowSec  = 20;
	/// Number of underflows within the window that triggers a threshold shift.
	static constexpr int    kUnderflowThreshold  = 4;
	/// Latency-band upward shift applied each time the threshold is exceeded.
	static constexpr double kUnderflowShiftMs    = 1000.0;
	/// Maximum cumulative adaptive shift that may be applied.
	static constexpr double kMaxAdaptiveShiftMs  = 3000.0;

	// -----------------------------------------------------------------------
	// Buffer-low consecutive-hit tracking
	// -----------------------------------------------------------------------

	/// Number of consecutive below-minBuffer readings needed to confirm the
	/// low-buffer state (mirrors AAMP_LLD_LOW_BUFF_CHECK_COUNT).
	static constexpr int    kBufferLowHitCount   = 4;

	// -----------------------------------------------------------------------
	// Worker thread
	// -----------------------------------------------------------------------

	/// Worker thread entry point.
	void Run();

	// -----------------------------------------------------------------------
	// Internal helpers
	// -----------------------------------------------------------------------

	/**
	 * @brief Interruptible sleep; wakes early when Stop() or a wakeup signal
	 *        is issued.
	 * @param ms  Maximum sleep duration in milliseconds.
	 */
	void WaitMs(int ms);

	/**
	 * @brief Apply a new playback rate via the StreamSink.
	 *
	 * Records telemetry (UpdateVideoEndMetrics, SendAnomalyEvent,
	 * profiler.IncrementChangeCount) when @p reportTelemetry is true.
	 *
	 * @param newRate         Rate to be applied.
	 * @param reportTelemetry Whether to emit telemetry for this change.
	 */
	void ApplyRate(double newRate, bool reportTelemetry);

	/**
	 * @brief Reset the playback rate to normalPlaybackRate via the sink.
	 *
	 * Called when rate correction is externally disabled or the stream
	 * stops downloading, so we do not leave an adjusted rate in effect.
	 */
	void ResetToNormalRate();

	/**
	 * @brief Examine the recent underflow timestamps and, if the adaptive
	 *        threshold is exceeded, shift mMinLatencyMs / mTargetLatencyMs /
	 *        mMaxLatencyMs upward and prune the window.
	 */
	void AdaptLatencyThresholds();

	// -----------------------------------------------------------------------
	// Dependencies (set by Start(), cleared by Stop())
	// -----------------------------------------------------------------------

	PrivateInstanceAAMP* mAamp   {nullptr}; ///< AAMP instance (telemetry/sink)

	// -----------------------------------------------------------------------
	// Configuration and dynamic thresholds
	// -----------------------------------------------------------------------

	LatencyConfig mConfig; ///< Immutable copy of the config supplied at Start()

	/// Mutex protecting the three dynamic latency thresholds below.
	mutable std::mutex mThresholdMutex;
	double mMinLatencyMs    {0.0}; ///< Current effective minimum latency (ms)
	double mTargetLatencyMs {0.0}; ///< Current effective target latency  (ms)
	double mMaxLatencyMs    {0.0}; ///< Current effective maximum latency (ms)

	/// Cumulative adaptive upward shift applied so far (protected by
	/// mThresholdMutex).
	double mAdaptiveShiftMs {0.0};

	// -----------------------------------------------------------------------
	// State and rate
	// -----------------------------------------------------------------------

	std::atomic<State>  mState        {State::kIdle};

	/// Playback rate currently applied; updated atomically on each change.
	std::atomic<double> mCurrentRate  {1.0};

	/// When false, rate correction is suppressed and the rate is held at
	/// normalPlaybackRate.  Updated by EnableRateCorrection().
	std::atomic<bool>   mCorrectionEnabled {true};

	// -----------------------------------------------------------------------
	// Buffer-low streak tracking (worker-thread-only; no lock needed)
	// -----------------------------------------------------------------------

	int mBufferLowCount    {0}; ///< Consecutive below-minBuffer readings
	int mBufferLowHitCount {0}; ///< Total confirmed low-buffer events

	// -----------------------------------------------------------------------
	// Underflow event tracking
	// -----------------------------------------------------------------------

	/// Mutex protecting mUnderflowTimestamps.
	std::mutex mUnderflowMutex;
	/// Timestamps (steady_clock) of recent underflow notifications.
	std::deque<std::chrono::steady_clock::time_point> mUnderflowTimestamps;

	// -----------------------------------------------------------------------
	// Interruptible sleep primitives
	// -----------------------------------------------------------------------

	std::mutex              mSleepMutex;
	std::condition_variable mSleepCv;
	bool                    mWakeupSignalled {false};

	// -----------------------------------------------------------------------
	// Worker thread handle
	// -----------------------------------------------------------------------

	std::thread mThread;
};

#endif // AAMP_LATENCY_MONITOR_H
