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
 * @file AampBufferMetrics.h
 * @brief Smoothed buffer-level measurement and state classification for AAMP.
 *
 * ## Design overview
 *
 * The progress timer fires every ~250 ms and calls AddSample() with the raw
 * buffer level (seconds) for a given media track.  AampBufferMetrics applies
 * an Exponential Moving Average (EMA) to reduce short-term jitter and maps the
 * smoothed value to one of three states:
 *
 *   | State    | Meaning                                      |
 *   |----------|----------------------------------------------|
 *   | CRITICAL | Buffer near-empty; imminent underflow risk.  |
 *   | LOW      | Buffer below target; ABR / latency may react.|
 *   | GOOD     | Buffer healthy; no action required.          |
 *
 * ### EMA smoothing
 * The smoothing coefficient alpha (α) is derived from a desired time-constant
 * tau (τ, seconds) and the sample interval T:
 *
 *   α = 1 - exp(-T / τ)
 *
 * Larger τ → smoother output, slower tracking.
 * Default: τ = 2.0 s, T = 0.25 s → α ≈ 0.118.
 *
 * ### Hysteresis
 * State transitions include a "guard band" (hysteresisSec) to prevent
 * flapping around threshold edges:
 *
 *   Downward (GOOD→LOW):     triggers when ema < lowThresholdSec
 *   Upward   (LOW→GOOD):     triggers when ema > lowThresholdSec + hysteresisSec
 *
 * The same pattern applies between LOW and CRITICAL.
 *
 * ### Underflow
 * NotifyUnderflow() immediately drops the EMA to zero and forces the state
 * to CRITICAL, modelling a true underflow event.
 *
 * ### Thread safety
 * All public methods are protected by a single internal mutex.  Listener
 * callbacks are invoked while the mutex is held; listeners MUST NOT call
 * back into AampBufferMetrics from the callback or deadlock will result.
 *
 * ## Usage
 * @code
 *   BufferMetricsConfig cfg;
 *   cfg.criticalThresholdSec = 0.5;
 *   cfg.lowThresholdSec      = 2.0;
 *   cfg.hysteresisSec        = 0.3;
 *   cfg.sampleIntervalSec    = 0.25;
 *   cfg.tauSec               = 2.0;
 *
 *   AampBufferMetrics bm(cfg);
 *   bm.AddListener(&myConsumer);
 *
 *   // Called by progress timer (250 ms cadence):
 *   bm.AddSample(eMEDIATYPE_VIDEO, currentVideoBufferSec);
 *
 *   // Query at any time:
 *   double smoothed = bm.GetSmoothedLevel(eMEDIATYPE_VIDEO);
 *   BufferState state = bm.GetState(eMEDIATYPE_VIDEO);
 * @endcode
 */

#ifndef AAMP_BUFFER_METRICS_H
#define AAMP_BUFFER_METRICS_H

#include "AampMediaType.h"

#include <functional>
#include <mutex>
#include <vector>

// ---------------------------------------------------------------------------
// BufferState
// ---------------------------------------------------------------------------

/**
 * @enum BufferState
 * @brief Classified health state of a media buffer.
 */
enum class BufferState : uint8_t
{
	CRITICAL = 0, ///< Buffer critically low; underflow imminent.
	LOW      = 1, ///< Buffer below healthy target.
	GOOD     = 2  ///< Buffer healthy.
};

// ---------------------------------------------------------------------------
// BufferMetricsConfig
// ---------------------------------------------------------------------------

/**
 * @brief Construction parameters for AampBufferMetrics.
 *
 * Thresholds are in seconds.  Two canonical presets are provided as static
 * factory methods:
 *   - BufferMetricsConfig::StandardDASH()
 *   - BufferMetricsConfig::LLDASH()
 */
struct BufferMetricsConfig
{
	/// Buffer level (s) that triggers the CRITICAL state.
	double criticalThresholdSec {0.5};

	/// Buffer level (s) that triggers the LOW state.
	double lowThresholdSec {2.0};

	/**
	 * @brief Guard band applied to upward transitions.
	 *
	 * Prevents flapping: the EMA must exceed (threshold + hysteresisSec)
	 * before the state advances upward.  Always >= 0.
	 */
	double hysteresisSec {0.3};

	/// Expected sample cadence (s); used to compute the EMA alpha.
	double sampleIntervalSec {0.25};

	/**
	 * @brief EMA time-constant (s).
	 *
	 * Controls the envelope of the filter.  Larger value = slower response.
	 * alpha = 1 - exp(-sampleIntervalSec / tauSec).
	 */
	double tauSec {2.0};

	// -----------------------------------------------------------------------
	// Preset factories
	// -----------------------------------------------------------------------

	/**
	 * @brief Preset for standard DASH / HLS (larger buffers, 250-ms progress timer).
	 * @return Populated config suitable for standard DASH streams.
	 */
	static BufferMetricsConfig StandardDASH()
	{
		BufferMetricsConfig cfg;
		cfg.criticalThresholdSec = 0.5;
		cfg.lowThresholdSec      = 2.0;
		cfg.hysteresisSec        = 0.3;
		cfg.sampleIntervalSec    = 0.25;
		cfg.tauSec               = 2.0;
		return cfg;
	}

	/**
	 * @brief Preset for LL-DASH (tighter buffers, 250-ms progress timer).
	 * @return Populated config suitable for LL-DASH streams.
	 */
	static BufferMetricsConfig LLDASH()
	{
		BufferMetricsConfig cfg;
		cfg.criticalThresholdSec = 0.2;
		cfg.lowThresholdSec      = 0.8;
		cfg.hysteresisSec        = 0.1;
		cfg.sampleIntervalSec    = 0.25;
		cfg.tauSec               = 0.8;
		return cfg;
	}
};

// ---------------------------------------------------------------------------
// IBufferMetricsListener
// ---------------------------------------------------------------------------

/**
 * @class IBufferMetricsListener
 * @brief Interface for consumers that want state-change notifications.
 *
 * Implement this interface and register with AampBufferMetrics::AddListener().
 */
class IBufferMetricsListener
{
public:
	virtual ~IBufferMetricsListener() = default;

	/**
	 * @brief Invoked when a track's buffer state changes.
	 *
	 * Called from the thread that invoked AampBufferMetrics::AddSample() or
	 * AampBufferMetrics::NotifyUnderflow().
	 *
	 * @warning Do NOT call back into AampBufferMetrics from this callback.
	 *
	 * @param mediaType  Track that changed state (eMEDIATYPE_VIDEO or AUDIO).
	 * @param oldState   Previous state.
	 * @param newState   New state.
	 * @param smoothedSec Current smoothed buffer level at transition time.
	 */
	virtual void OnBufferStateChanged(AampMediaType mediaType,
	                                  BufferState    oldState,
	                                  BufferState    newState,
	                                  double         smoothedSec) = 0;
};

// ---------------------------------------------------------------------------
// AampBufferMetrics
// ---------------------------------------------------------------------------

/**
 * @class AampBufferMetrics
 * @brief Smoothed buffer-level tracker and state classifier.
 *
 * Tracks video and audio independently via the AampMediaType parameter.
 * Only eMEDIATYPE_VIDEO and eMEDIATYPE_AUDIO are fully handled; other types
 * are silently ignored.
 */
class AampBufferMetrics
{
public:
	// -----------------------------------------------------------------------
	// Construction / destruction
	// -----------------------------------------------------------------------

	/// @brief Constructs metrics with the given configuration.
	/// @param config  Thresholds, smoothing and cadence parameters.
	explicit AampBufferMetrics(const BufferMetricsConfig& config);

	~AampBufferMetrics() = default;

	/// Non-copyable.
	AampBufferMetrics(const AampBufferMetrics&)            = delete;
	AampBufferMetrics& operator=(const AampBufferMetrics&) = delete;

	// -----------------------------------------------------------------------
	// Listener management
	// -----------------------------------------------------------------------

	/**
	 * @brief Register a listener for buffer-state-change notifications.
	 *
	 * Listeners are held by raw pointer; the caller is responsible for
	 * ensuring the listener outlives this object or for calling
	 * RemoveListener() before the listener is destroyed.
	 *
	 * @param listener  Non-null pointer to a listener implementation.
	 */
	void AddListener(IBufferMetricsListener* listener);

	/**
	 * @brief Deregister a previously registered listener.
	 *
	 * No-op if the listener is not currently registered.
	 *
	 * @param listener  Pointer previously passed to AddListener().
	 */
	void RemoveListener(IBufferMetricsListener* listener);

	// -----------------------------------------------------------------------
	// Sample ingestion
	// -----------------------------------------------------------------------

	/**
	 * @brief Ingest a new raw buffer measurement.
	 *
	 * Called by the player's progress timer (default every 250 ms).
	 * Updates the EMA, evaluates state transitions, and notifies listeners.
	 *
	 * Only eMEDIATYPE_VIDEO and eMEDIATYPE_AUDIO are processed.
	 *
	 * @param mediaType  The track this measurement belongs to.
	 * @param rawSec     Raw buffer level in seconds (>= 0).
	 */
	void AddSample(AampMediaType mediaType, double rawSec);

	/**
	 * @brief Notify that a pipeline underflow has occurred.
	 *
	 * Immediately sets the EMA to 0 and transitions the state to CRITICAL
	 * for the specified track, then fires listeners.
	 *
	 * @param mediaType  Track that underflowed (typically eMEDIATYPE_VIDEO).
	 */
	void NotifyUnderflow(AampMediaType mediaType);

	// -----------------------------------------------------------------------
	// Queries
	// -----------------------------------------------------------------------

	/**
	 * @brief Returns the current EMA-smoothed buffer level.
	 *
	 * @param mediaType  Track to query.
	 * @return Smoothed level in seconds, or 0.0 for unsupported types.
	 */
	double GetSmoothedLevel(AampMediaType mediaType) const;

	/**
	 * @brief Returns the current classified buffer state.
	 *
	 * @param mediaType  Track to query.
	 * @return Current BufferState, or CRITICAL for unsupported types.
	 */
	BufferState GetState(AampMediaType mediaType) const;

	// -----------------------------------------------------------------------
	// Reset hooks
	// -----------------------------------------------------------------------

	/**
	 * @brief Reset state and EMA for all tracks on a seek.
	 *
	 * Clears the EMA and resets the state to GOOD.  The next AddSample()
	 * call will prime the EMA with that raw value.
	 */
	void ResetOnSeek();

	/**
	 * @brief Reset state and EMA on trickplay activation.
	 *
	 * During trickplay, buffer semantics change; resetting avoids spurious
	 * CRITICAL transitions when re-entering normal playback.
	 */
	void ResetOnTrickplay();

	/**
	 * @brief Full reset on playback stop.
	 *
	 * Clears listeners and resets all internal state.
	 */
	void ResetOnStop();

	// -----------------------------------------------------------------------
	// Configuration update
	// -----------------------------------------------------------------------

	/**
	 * @brief Replace the active configuration at runtime.
	 *
	 * The EMA coefficient is recomputed from the new parameters.
	 * Track state and smoothed values are preserved.
	 *
	 * @param config  New configuration to apply.
	 */
	void UpdateConfig(const BufferMetricsConfig& config);

private:
	// -----------------------------------------------------------------------
	// Per-track state
	// -----------------------------------------------------------------------

	/**
	 * @brief Holds smoothing and state data for a single media track.
	 */
	struct TrackMetrics
	{
		double      ema        {0.0};           ///< Current EMA value (seconds).
		bool        primed     {false};         ///< False until first AddSample.
		BufferState state      {BufferState::GOOD}; ///< Current classified state.
	};

	// -----------------------------------------------------------------------
	// Private helpers
	// -----------------------------------------------------------------------

	/**
	 * @brief Classify a smoothed level under the current config.
	 *
	 * Applies hysteresis relative to the current state.
	 *
	 * @param ema          Smoothed buffer level (s).
	 * @param currentState Existing state (determines hysteresis direction).
	 * @return             New state after applying threshold rules.
	 */
	BufferState Classify(double ema, BufferState currentState) const;

	/**
	 * @brief Notify all registered listeners of a state transition.
	 *
	 * @pre mMutex is already held by the caller.
	 */
	void NotifyListeners(AampMediaType mediaType,
	                     BufferState   oldState,
	                     BufferState   newState,
	                     double        smoothedSec) const;

	/**
	 * @brief Return a pointer to the TrackMetrics for the given type.
	 *
	 * @return nullptr for unsupported media types.
	 */
	TrackMetrics* GetTrack(AampMediaType mediaType);

	/**
	 * @brief Const overload of GetTrack.
	 */
	const TrackMetrics* GetTrack(AampMediaType mediaType) const;

	/**
	 * @brief Reset a single track's EMA and state without touching listeners.
	 *
	 * @pre mMutex is already held by the caller.
	 */
	void ResetTrack(TrackMetrics& track);

	// -----------------------------------------------------------------------
	// Data members
	// -----------------------------------------------------------------------

	mutable std::mutex              mMutex;        ///< Guards all mutable state.
	BufferMetricsConfig             mConfig;       ///< Active configuration.
	double                          mAlpha;        ///< EMA coefficient [0,1].

	TrackMetrics                    mVideo;        ///< Video track metrics.
	TrackMetrics                    mAudio;        ///< Audio track metrics.

	std::vector<IBufferMetricsListener*> mListeners; ///< Registered listeners.
};

#endif // AAMP_BUFFER_METRICS_H
