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
 * @file AampRialtoMonitorAV.h
 * @brief Standalone AV health monitor for the direct-Rialto playback path.
 *
 * Replicates the behaviour of the GStreamer MonitorAV() / MonitorAvTimerCallback
 * pair for streams rendered via the Rialto IMediaPipeline interface.
 *
 * Two independent GLib timers are used:
 *   - Sample timer  : queries the pipeline position and classifies AV state.
 *   - Report timer  : reads getStats() and fires SendMonitorAvEvent().
 *
 * Both timers run only while the pipeline is in the PLAYING state.
 * Classification state is preserved across PAUSED periods so that
 * transitions are logged correctly when playback resumes.
 */

#ifndef AAMP_RIALTO_MONITOR_AV_H
#define AAMP_RIALTO_MONITOR_AV_H

#include "IStreamSinkNotifiable.h"
#include "IMediaPipeline.h"

#include <functional>
#include <memory>
#include <string>
#include <cstdint>
#include <glib.h>

/**
 * @class AampRialtoMonitorAV
 * @brief Periodic AV health classifier and event reporter for direct-Rialto.
 *
 * The caller constructs this object once per stream session (in
 * AampRialtoPlayer::Configure()) and calls start() / stop() around the
 * PLAYING interval.
 *
 * Ownership of the Rialto pipeline is shared via std::shared_ptr.  All AAMP
 * notifications go through the injected IStreamSinkNotifiable so that the
 * class can be exercised in unit tests without a live AAMP instance.
 */
class AampRialtoMonitorAV
{
public:
	/**
	 * @brief Configuration values read from AampConfig at construction time.
	 */
	struct Config
	{
		int sampleIntervalMs;        ///< Sample cadence (from eAAMPConfig_ReportProgressInterval * 1000)
		int reportIntervalMs;        ///< Report cadence (from eAAMPConfig_MonitorAVReportingInterval)
		int syncThresholdPositiveMs; ///< AV-sync positive delta threshold (ms)
		int syncThresholdNegativeMs; ///< AV-sync negative delta threshold (ms)
		int jumpThresholdMs;         ///< Unexpected position-jump threshold (ms)
	};

	/**
	 * @brief Construct the monitor.
	 *
	 * @param[in] pipeline            Shared ownership of the Rialto pipeline.
	 *                                Must be non-null when start() is called.
	 * @param[in] notifiable          Non-null.  Receives SendMonitorAvEvent()
	 *                                calls on the report timer tick.  Must
	 *                                outlive this object.
	 * @param[in] videoSourceIdGetter Callable returning the Rialto source-ID
	 *                                for the video track, or -1 if not yet
	 *                                attached.  Invoked from the report timer.
	 * @param[in] rateGetter          Callable returning the current playback
	 *                                rate as an integer (1 = normal).
	 * @param[in] isPlayingGetter     Callable returning true while the
	 *                                pipeline is in the PLAYING state.
	 *                                The sample timer uses this guard to skip
	 *                                ticks that arrive during transient
	 *                                pauses.
	 * @param[in] config              Thresholds and timer intervals.
	 */
	AampRialtoMonitorAV(
		std::shared_ptr<firebolt::rialto::IMediaPipeline> pipeline,
		IStreamSinkNotifiable *notifiable,
		std::function<int32_t()> videoSourceIdGetter,
		std::function<int()> rateGetter,
		std::function<bool()> isPlayingGetter,
		Config config);

	~AampRialtoMonitorAV();

	// Non-copyable, non-movable.
	AampRialtoMonitorAV(const AampRialtoMonitorAV &) = delete;
	AampRialtoMonitorAV &operator=(const AampRialtoMonitorAV &) = delete;

	/**
	 * @brief Start the sample and report timers.
	 *
	 * Has no effect if the timers are already running.
	 */
	void start();

	/**
	 * @brief Stop the sample and report timers.
	 *
	 * Has no effect if the timers are not running.
	 */
	void stop();

private:
	/// Called by the sample timer: queries position and classifies AV state.
	void onSampleTick();

	/// Called by the report timer: reads getStats() and fires SendMonitorAvEvent().
	void onReportTick();

	/// GLib timeout callback for the sample timer.
	static gboolean sampleTimerCb(gpointer data);

	/// GLib timeout callback for the report timer.
	static gboolean reportTimerCb(gpointer data);

	// -----------------------------------------------------------------------
	// Dependencies (injected at construction)
	// -----------------------------------------------------------------------
	std::shared_ptr<firebolt::rialto::IMediaPipeline> m_pipeline;
	IStreamSinkNotifiable *m_notifiable;
	std::function<int32_t()> m_videoSourceIdGetter;
	std::function<int()> m_rateGetter;
	std::function<bool()> m_isPlayingGetter;
	Config m_config;

	// -----------------------------------------------------------------------
	// Classification state (mirrors MonitorAVState from InterfacePlayerRDK.h)
	// -----------------------------------------------------------------------

	/// Wall-clock time (ms) of the last state transition.
	int64_t m_tLastReported{0};

	/// Wall-clock time (ms) of the last successful position sample.
	int64_t m_tLastSampled{0};

	/// Current AV health classification string; nullptr until first sample.
	const char *m_description{nullptr};

	/// Most recently sampled positions: [0]=video, [1]=audio.
	/// In the direct-Rialto path both entries hold the same pipeline position
	/// because getPosition() returns a single shared value.
	int64_t m_avPositionMs[2]{0, 0};

	/// Cached positions forwarded to SendMonitorAvEvent().
	int64_t m_videoPositionMs{0};
	int64_t m_audioPositionMs{0};

	/// True when the previous classification was "ok".
	/// Used to gate unexpected-jump detection, matching GStreamer behaviour.
	bool m_happy{false};

	// -----------------------------------------------------------------------
	// GLib timer IDs (0 = not running)
	// -----------------------------------------------------------------------
	guint m_sampleTimerId{0};
	guint m_reportTimerId{0};
};

#endif // AAMP_RIALTO_MONITOR_AV_H
