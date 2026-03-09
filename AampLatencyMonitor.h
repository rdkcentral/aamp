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
#include <AampDefine.h>

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

	LatencyConfig() = default;

	// Constructor with params
	LatencyConfig(double normalRate, double minRate, double maxRate,
				  double minLatency, double targetLatency, double maxLatency,
				  int monitorDelay, int monitorInterval, double bufferLevel,
				  double rebufferingStepMs = 0.0, double rebufferingMaxIncrMs = 0.0)
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
 * Start() and Stop() may be called from any thread.
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
	 * @brief Notify the monitor that rebuffering has started.
	 *
	 * Shifts the effective min, target, and max latency thresholds upward by
	 * mConfig.rebufferingLatencyStepMs per call.  The total accumulated shift
	 * is capped at mConfig.rebufferingLatencyMaxIncrementMs above the original
	 * config values (zero cap means uncapped).  Has no effect when
	 * rebufferingLatencyStepMs is zero.
	 *
	 * Thread-safe.
	 */
	void OnRebufferingStart();

	/**
	 * @brief Return the current effective latency thresholds.
	 *
	 * @return Tuple of {minLatencyMs, targetLatencyMs, maxLatencyMs}.
	 * Thread-safe (reads under mThresholdMutex).
	 */
	std::tuple<double, double, double> GetCurrentThresholds() const;

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

	PrivateInstanceAAMP* mAamp   {nullptr}; /**< AAMP instance (telemetry/sink) */

	LatencyConfig mConfig; /**< Immutable copy of the config supplied at Start() */

	// Mutex protecting the three dynamic latency thresholds and the
	// accumulated rebuffering increment.
	mutable std::mutex mThresholdMutex;
	double mMinLatencyMs    {0.0}; /**< Current effective minimum latency (ms) */
	double mTargetLatencyMs {0.0}; /**< Current effective target latency  (ms) */
	double mMaxLatencyMs    {0.0}; /**< Current effective maximum latency (ms) */

	/// Total latency shift (ms) accumulated from OnRebufferingStart() calls.
	/// Reset to zero on Start() and Stop().
	double mLatencyIncrementAccumulatedMs {0.0};

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
