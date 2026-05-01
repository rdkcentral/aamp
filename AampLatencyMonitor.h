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
 */

#ifndef AAMP_LATENCY_MONITOR_H
#define AAMP_LATENCY_MONITOR_H

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>
#include <tuple>
#include "AampDefine.h"

class PrivateInstanceAAMP;

/**
 * @brief Configuration bundle for one AampLatencyMonitor session.
 *
 * All latency values and buffer thresholds are in milliseconds.
 * Defaults match the DASH rate-correction defaults.
 */
struct LatencyConfig
{
	// Playback rate applied during normal (in-band) operation.
	double normalPlaybackRate {DEFAULT_NORMAL_RATE_CORRECTION_SPEED};
	// Slow-down correction rate applied when latency is too low.
	double minPlaybackRate {DEFAULT_MIN_RATE_CORRECTION_SPEED};
	// Speed-up correction rate applied when latency is too high.
	double maxPlaybackRate {DEFAULT_MAX_RATE_CORRECTION_SPEED};

	// Lower bound of the latency dead-band; trigger slow-down below this.
	double minLatencyMs {DEFAULT_MIN_LOW_LATENCY * 1000};
	// Desired latency; used as the hysteresis pivot when returning to normal.
	double targetLatencyMs {DEFAULT_TARGET_LOW_LATENCY * 1000};
	// Upper bound of the latency dead-band; trigger speed-up above this.
	double maxLatencyMs {DEFAULT_MAX_LOW_LATENCY * 1000};

	// One-shot startup delay (ms) before the first latency check fires.
	int monitorDelayMs {DEFAULT_LATENCY_MONITOR_DELAY_MS};
	// Polling interval (ms) between successive latency evaluations.
	int monitorIntervalMs {DEFAULT_LATENCY_MONITOR_INTERVAL_MS};

	double correctionActivationThresholdMs {DEFAULT_BUFFER_LEVEL_TO_ENABLE_LATENCY_SEC * 1000};

	/// Latency step (ms) added to all three thresholds on each rebuffering event.
	/// Zero disables the adaptive shift entirely.
	double rebufferingLatencyStepMs {0.0};

	/// Maximum total accumulated increment (ms) applied above the base config
	/// thresholds.  Zero means no cap.
	double rebufferingLatencyMaxIncrementMs {0.0};

	/// Buffer level (ms) that counts as healthy runway for restoration.
	/// If bufferMs >= this value for latencyStableSec consecutive seconds,
	/// one restoration step is applied.  Zero disables dynamic restoration entirely.
	double dangerBufferMs {0.0};

	/// Duration (s) of consecutive polls with buffer >= dangerBufferMs
	/// required before one restoration step is taken.
	/// Zero disables dynamic restoration entirely.
	double latencyStableSec {0.0};

	LatencyConfig() = default;

	// Constructor with params
	LatencyConfig(double normalRate, double minRate, double maxRate,
				  double minLatency, double targetLatency, double maxLatency,
				  int monitorDelay, int monitorInterval, double bufferLevel,
				  double rebufferingStepMs = 0.0, double rebufferingMaxIncrMs = 0.0,
				  double dangerBufferMs = 0.0, double latencyStableSec = 0.0)
		: normalPlaybackRate(normalRate)
		, minPlaybackRate(minRate)
		, maxPlaybackRate(maxRate)
		, minLatencyMs(minLatency)
		, targetLatencyMs(targetLatency)
		, maxLatencyMs(maxLatency)
		, monitorDelayMs(monitorDelay)
		, monitorIntervalMs(monitorInterval)
		, correctionActivationThresholdMs(bufferLevel)
		, rebufferingLatencyStepMs(rebufferingStepMs)
		, rebufferingLatencyMaxIncrementMs(rebufferingMaxIncrMs)
		, dangerBufferMs(dangerBufferMs)
		, latencyStableSec(latencyStableSec)
	{}
};

/**
 * @class AampLatencyMonitor
 * @brief Monitors live-stream latency and applies playback-rate correction.
 *
 * ## Purpose
 * Maintains end-to-end stream latency near a configurable target by adjusting
 * the playback rate.
 *
 * ## State machine
 * @code
 *   kIdle  --Start()--> kStarting --thread launched--> kRunning
 *   kRunning --Stop()--> kStopping --thread joins--> kIdle
 * @endcode
 *
 * ## Rate-correction rules
 * 1. Speed up  (maxRate): latency > maxLatency
 * 2. Slow down (minRate): latency < minLatency
 * 3. Return to normal:    latency back within band.
 *
 * ## Thread safety
 * Start() and Stop() are serialised by mStartStopMutex and may safely be
 * called from any thread, including concurrently.
 * EnableRateCorrection() is also thread-safe.
 */
class AampLatencyMonitor
{
public:

	// Delete default constructor to require an AAMP instance.
	AampLatencyMonitor() = delete;

	/**
	 * @brief Constructor.
	 * @param aamp Pointer to the AAMP instance for telemetry and sink control.
	 *             The instance must remain valid until Stop() returns.
	 *             (The monitor does not take ownership of the pointer.)
	 */
	AampLatencyMonitor(PrivateInstanceAAMP* aamp);

	/**
	 * @brief Destructor. Calls Stop() to ensure the worker thread exits.
	 */
	~AampLatencyMonitor();

	// Delete copy constructor and assignment operator to prevent copying as it owns a std::thread.
	AampLatencyMonitor(const AampLatencyMonitor&)            = delete;
	AampLatencyMonitor& operator=(const AampLatencyMonitor&) = delete;

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
	 * @brief Returns the playback rate most recently applied by this monitor.
	 *
	 * Thread-safe (atomic load).
	 */
	double GetCurrentRate() const;

	/**
	 * @brief Returns true if the monitor thread is currently running.
	 * Thread-safe (atomic load).
	 * True if the thread is active and monitoring; false if stopped or in transition.
	 */
	bool IsRunning() const
	{
		return mState.load() == State::kRunning;
	}

	/**
	 * @brief Return the current effective latency thresholds.
	 *
	 * @return Tuple of {minLatencyMs, targetLatencyMs, maxLatencyMs}.
	 * Thread-safe (reads under mThresholdMutex).
	 */
	std::tuple<double, double, double> GetCurrentThresholds() const;

	/**
	 * @brief Notify the monitor of the current buffer level.
	 * When @p bufferMs is **below** dangerBufferMs and the episode guard
	 * (mBelowDangerShifted) is clear, the worker thread is signalled to wake
	 * early so it can apply the threshold shift on its next iteration rather
	 * than waiting for the next scheduled poll interval.  Subsequent
	 * notifications within the same episode are no-ops (the episode guard
	 * suppresses redundant wakeups).
	 *
	 * When @p bufferMs is **at or above** dangerBufferMs and a danger episode
	 * is active (mBelowDangerShifted is set), Run() is woken once so it can
	 * clear the episode guard and start the restoration timer from the accurate
	 * moment of recovery. Run() owns all state transitions; mBelowDangerShifted
	 * is never written here.
	 *
	 * A negative @p bufferMs (the sentinel returned by
	 * GetBufferedDurationSecs() on lock-contention) is silently ignored so
	 * that a transient read failure cannot trigger a spurious wakeup.
	 *
	 * Has no effect when dangerBufferMs or rebufferingLatencyStepMs is zero,
	 * or when rate correction is disabled (mCorrectionEnabled == false) —
	 * in that case the worker is sleeping indefinitely and has nothing to act
	 * on, so the wakeup is suppressed to avoid redundant thread scheduling.
	 *
	 * Thread-safe (mBelowDangerShifted is atomic; wakeup uses mSleepMutex).
	 *
	 * @param[in] bufferMs  Current buffered duration in milliseconds,
	 *                      or a negative sentinel if the measurement is unavailable.
	 */
	void OnBufferLevelUpdate(double bufferMs);

private:

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

	/**
	 * @brief Worker thread entry point.
	 */
	void Run();

	/**
	 * @brief Interruptible sleep; wakes early when Stop() or a wakeup signal
	 * is issued.
	 * 
	 * @param ms  Maximum sleep duration in milliseconds.
	 */
	void WaitMs(int ms);

	/**
	 * @brief Sleep indefinitely until a wakeup signal is issued.
	 * Used when rate correction is disabled. The monitor thread wakes when
	 * correction is re-enabled or when Stop() is called.
	 */
	void WaitUntilSignalled();

	/**
	 * @brief Apply a new playback rate via the StreamSink.
	 * Records telemetry (UpdateVideoEndMetrics, SendAnomalyEvent,
	 * profiler.IncrementChangeCount)
	 *
	 * @param newRate         Rate to be applied.
	 */
	void ApplyRate(double newRate);

	/**
	 * @brief Reset the playback rate to normalPlaybackRate via the sink.
	 * Called when rate correction is externally disabled or the stream
	 * stops downloading, so we do not leave an adjusted rate in effect.
	 */
	void ResetToNormalRate();

	/**
	 * @brief Reset the three dynamic latency thresholds to the values stored
	 * in mConfig and clear the accumulated rebuffering increment.
	 *
	 * @pre mThresholdMutex must be held by the caller.
	 */
	void ResetLatencyThresholdsLocked();

	/**
	 * @brief Update danger-buffer state and the threshold restoration timer.
	 *
	 * Called from Run() on every poll where dangerBufferMs and
	 * rebufferingLatencyStepMs are both configured. Acquires mThresholdMutex
	 * internally.
	 *
	 * @param[in] bufferMs - Current buffer level in milliseconds.
	 */
	void UpdateDangerBufferState(double bufferMs);

	/**
	 * @brief Apply one upward threshold shift (rebufferingLatencyStepMs).
	 *
	 * Increments mLatencyIncrementAccumulatedMs by rebufferingLatencyStepMs,
	 * capped at rebufferingLatencyMaxIncrementMs when non-zero, then
	 * recomputes the three dynamic thresholds.
	 *
	 * @pre mThresholdMutex must be held by the caller.
	 */
	void IncreaseThresholdsLocked();

	/**
	 * @brief Attempt one restoration step toward the config-default thresholds.
	 *
	 * Called from Run() once the buffer has remained at or above dangerBufferMs
	 * for latencyStableSec consecutive seconds (measured across polling intervals).
	 * Decrements mLatencyIncrementAccumulatedMs by rebufferingLatencyStepMs (floored
	 * at zero) and recomputes the three dynamic thresholds.  Has no effect
	 * when rebufferingLatencyStepMs is not configured or accumulated increment is already
	 * zero.
	 *
	 * @pre mThresholdMutex must be held by the caller.
	 */
	void TryRestoreThresholdsLocked();

	PrivateInstanceAAMP* mAamp   {nullptr}; /**< AAMP instance (telemetry/sink) */

	LatencyConfig mConfig; /**< Immutable copy of the config supplied at Start() */

	// Mutex protecting the three dynamic latency thresholds, the
	// accumulated rebuffering increment, and the restoration window timer.
	mutable std::mutex mThresholdMutex;
	double mMinLatencyMs    {0.0}; /**< Current effective minimum latency (ms) */
	double mTargetLatencyMs {0.0}; /**< Current effective target latency  (ms) */
	double mMaxLatencyMs    {0.0}; /**< Current effective maximum latency (ms) */

	/// Total latency shift (ms) accumulated from low-buffer events via OnBufferLevelUpdate().
	/// Reset to zero on Start() and Stop().
	double mLatencyIncrementAccumulatedMs {0.0};

	/// Time point when the buffer first reached dangerBufferMs, marking the
	/// start of the current restoration window.  Reset to epoch (default-constructed) on
	/// Start(), Stop(), and whenever a rebuffering event interrupts the healthy streak.
	std::chrono::steady_clock::time_point mRestorationWindowStartTime {};

	/// Episode guard for the low-buffer shift.
	///
	/// Set by Run() when it shifts thresholds for a below-dangerBufferMs episode.
	/// Cleared by OnBufferLevelUpdate() as soon as buffer recovers to >= dangerBufferMs,
	/// so the next distinct dip triggers a fresh shift.
	///
	/// OnBufferLevelUpdate() also reads this atomically (without holding mThresholdMutex)
	/// to suppress redundant wakeups: once Run() has already shifted for the current
	/// episode, further low-buffer fragment notifications do not re-signal the thread.
	///
	/// Atomic so that OnBufferLevelUpdate() can load it cheaply from the downloader
	/// thread without acquiring mThresholdMutex on every fragment download.
	std::atomic<bool> mBelowDangerShifted {false};

	/// Serializes concurrent calls to Start() and Stop().
	std::mutex mStartStopMutex;

	std::atomic<State> mState {State::kIdle}; /**< Current state of the monitor thread */

	// Playback rate currently applied; updated atomically on each change.
	std::atomic<double> mCurrentRate {1.0};

	// When false, rate correction is suppressed and the rate is held at
	// normalPlaybackRate.  Updated by EnableRateCorrection().
	std::atomic<bool> mCorrectionEnabled {true};

	std::mutex              mSleepMutex; /**< Mutex for condition variable and wakeup signalling */
	std::condition_variable mSleepCv; /**< Condition variable for wakeup signalling */
	bool                    mWakeupSignalled {false}; /**< Flag indicating a wakeup signal has been issued */

	std::thread mThread; /**< Worker thread for latency monitoring and rate correction. */
};

#endif // AAMP_LATENCY_MONITOR_H
