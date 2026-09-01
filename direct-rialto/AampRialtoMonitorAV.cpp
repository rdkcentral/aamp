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
 * @file AampRialtoMonitorAV.cpp
 * @brief Implementation of AampRialtoMonitorAV.
 *
 * Classification logic mirrors the GStreamer MonitorAV() free function
 * (middleware/InterfacePlayerRDK.cpp) and the MonitorAvTimerCallback in
 * aampgstplayer.cpp.
 *
 * Key behavioural differences from the GStreamer path:
 *   - IMediaPipeline::getPosition() returns the same pipeline position for
 *     every source.  As a result both m_avPositionMs[VIDEO] and
 *     m_avPositionMs[AUDIO] always hold the same value and the "avsync"
 *     classification will never trigger in practice.
 *   - Dropped-frame counts are obtained directly from
 *     IMediaPipeline::getStats() on the report timer tick, rather than from
 *     GStreamer's GstPlaybackQualityStruct.
 */

#include "AampRialtoMonitorAV.h"
#include "AampUtils.h"
#include "AampLogManager.h"
#include "AampDefine.h"

#include <cinttypes>
#include <algorithm>

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

/// Video track index (matches eMEDIATYPE_VIDEO = 0).
static constexpr int kVideoIdx = 0;

/// Audio track index (matches eMEDIATYPE_AUDIO = 1).
static constexpr int kAudioIdx = 1;

/// Nanoseconds per millisecond.
static constexpr int64_t kNsPerMs = 1'000'000LL;

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

AampRialtoMonitorAV::AampRialtoMonitorAV(
	std::shared_ptr<firebolt::rialto::IMediaPipeline> pipeline,
	IStreamSinkNotifiable *notifiable,
	std::function<int32_t()> videoSourceIdGetter,
	std::function<int()> rateGetter,
	std::function<bool()> isPlayingGetter,
	Config config)
	: m_pipeline(std::move(pipeline))
	, m_notifiable(notifiable)
	, m_videoSourceIdGetter(std::move(videoSourceIdGetter))
	, m_rateGetter(std::move(rateGetter))
	, m_isPlayingGetter(std::move(isPlayingGetter))
	, m_config(config)
{
}

AampRialtoMonitorAV::~AampRialtoMonitorAV()
{
	stop();
}

// ---------------------------------------------------------------------------
// Timer control
// ---------------------------------------------------------------------------

void AampRialtoMonitorAV::start()
{
	bool started = false;
	if (m_sampleTimerId == 0)
	{
		m_sampleTimerId = g_timeout_add(
			static_cast<guint>(m_config.sampleIntervalMs),
			sampleTimerCb,
			this);
		if (m_sampleTimerId == 0)
		{
			AAMPLOG_WARN("MonitorAvTimer failed to start sample timer");
		}
		else
		{
			started = true;
		}
	}
	if (m_reportTimerId == 0)
	{
		m_reportTimerId = g_timeout_add(
			static_cast<guint>(m_config.reportIntervalMs),
			reportTimerCb,
			this);
		if (m_reportTimerId == 0)
		{
			AAMPLOG_WARN("MonitorAvTimer failed to start report timer");
		}
		else
		{
			started = true;
		}
	}
	if (started)
	{
		AAMPLOG_MIL("MonitorAvTimer started with interval %d ms", 
					m_config.reportIntervalMs);
	}
}

void AampRialtoMonitorAV::stop()
{
	if (m_sampleTimerId != 0)
	{
		g_source_remove(m_sampleTimerId);
		m_sampleTimerId = 0;
	}
	if (m_reportTimerId != 0)
	{
		g_source_remove(m_reportTimerId);
		m_reportTimerId = 0;
	}
	AAMPLOG_MIL("MonitorAvTimer stopped");
}

// ---------------------------------------------------------------------------
// GLib timer callbacks
// ---------------------------------------------------------------------------

gboolean AampRialtoMonitorAV::sampleTimerCb(gpointer data)
{
	auto *self = static_cast<AampRialtoMonitorAV *>(data);
	self->onSampleTick();
	return G_SOURCE_CONTINUE;
}

gboolean AampRialtoMonitorAV::reportTimerCb(gpointer data)
{
	auto *self = static_cast<AampRialtoMonitorAV *>(data);
	self->onReportTick();
	return G_SOURCE_CONTINUE;
}

// ---------------------------------------------------------------------------
// Sample tick — classify AV health
// ---------------------------------------------------------------------------

void AampRialtoMonitorAV::onSampleTick()
{
	if (!m_isPlayingGetter())
	{
		return;
	}

	const int64_t tNow = aamp_GetCurrentTimeMS();

	// Initialise tLastReported on the first tick.
	if (m_tLastReported == 0)
	{
		m_tLastReported = tNow;
	}

	// Query the pipeline position once; both tracks share the same value in
	// the Rialto path (IMediaPipeline::getPosition returns the presentation
	// position regardless of which source is asking).
	int64_t posNs = 0;
	const bool posOk = m_pipeline->getPosition(posNs);
	const int64_t posMs = posOk ? (posNs / kNsPerMs) : -1LL;

	const int rate = m_rateGetter();
	// Skip the audio track during trickplay to match GStreamer behaviour.
	const int maxTracks = (rate == AAMP_NORMAL_PLAY_RATE) ? 2 : 1;

	const char *description = nullptr;
	int numTracks = 0;
	bool bigJump = false;

	// Shadow array updated during the loop; written to m_avPositionMs after.
	int64_t newPos[2] = { m_avPositionMs[kVideoIdx], m_avPositionMs[kAudioIdx] };

	for (int i = 0; i < maxTracks; i++)
	{
		if (!posOk)
		{
			// Cannot determine position for this track.
			continue;
		}

		if (posMs == m_avPositionMs[i])
		{
			// Position unchanged since last sample — track has stalled.
			if (description)
			{
				// Both tracks stalled.
				description = "stall";
			}
			else
			{
				description = (i == kVideoIdx) ? "video freeze" : "audio drop";
			}
		}
		else if (i == kVideoIdx && m_happy)
		{
			// Check for an unexpectedly large forward jump on the video track.
			const int64_t actualDelta = posMs - m_avPositionMs[kVideoIdx];
			const int64_t expectedDelta = tNow - m_tLastSampled;
			if (actualDelta > expectedDelta + m_config.jumpThresholdMs)
			{
				bigJump = true;
			}
		}

		newPos[i] = posMs;
		numTracks++;
	}

	m_tLastSampled = tNow;
	m_avPositionMs[kVideoIdx] = newPos[kVideoIdx];
	m_avPositionMs[kAudioIdx] = newPos[kAudioIdx];
	m_videoPositionMs = m_avPositionMs[kVideoIdx];
	m_audioPositionMs = m_avPositionMs[kAudioIdx];

	switch (numTracks)
	{
		case 0:
			description = "eos";
			break;
		case 1:
			description = "trickplay";
			break;
		case 2:
		{
			const int delta = static_cast<int>(
				m_avPositionMs[kVideoIdx] - m_avPositionMs[kAudioIdx]);
			if (delta > m_config.syncThresholdPositiveMs
				|| delta < m_config.syncThresholdNegativeMs)
			{
				if (!description)
				{
					// Both tracks moving, but diverged.
					description = "avsync";
				}
			}
			else if (bigJump)
			{
				description = "jump";
			}
		}
			break;
		default:
			break;
	}

	if (!description)
	{
		description = "ok";
	}

	if (m_description != description)
	{
		// Log only when the classification changes.
		if (m_description)
		{
			// Log the outgoing state with its duration.
			AAMPLOG_MIL(
				"MonitorAV_%s: %" PRId64 ",%" PRId64 ",%d, %" PRId64,
				m_description,
				m_avPositionMs[kVideoIdx],
				m_avPositionMs[kAudioIdx],
				static_cast<int>(m_avPositionMs[kVideoIdx] - m_avPositionMs[kAudioIdx]),
				m_tLastSampled - m_tLastReported);
		}
		// Log the incoming state.
		AAMPLOG_MIL(
			"MonitorAV_%s: %" PRId64 ",%" PRId64 ",%d,0",
			description,
			m_avPositionMs[kVideoIdx],
			m_avPositionMs[kAudioIdx],
			static_cast<int>(m_avPositionMs[kVideoIdx] - m_avPositionMs[kAudioIdx]));

		m_tLastReported = m_tLastSampled;
		m_description = description;
	}

	// Track whether we are in the healthy "ok" state; used to gate jump
	// detection on the next tick (mirrors MonitorAVState::happy behaviour).
	m_happy = (description != nullptr && std::string(description) == "ok");
}

// ---------------------------------------------------------------------------
// Report tick — send telemetry event
// ---------------------------------------------------------------------------

void AampRialtoMonitorAV::onReportTick()
{
	if (m_tLastSampled == 0 || m_description == nullptr)
	{
		// No sample has been taken yet; nothing useful to report.
		AAMPLOG_TRACE("AampRialtoMonitorAV: report tick skipped - no sample yet");
		return;
	}

	// Obtain cumulative dropped-frame count from the pipeline.
	uint64_t rendered = 0;
	uint64_t dropped = 0;
	const int32_t srcId = m_videoSourceIdGetter();
	if (srcId >= 0)
	{
		if (!m_pipeline->getStats(srcId, rendered, dropped))
		{
			AAMPLOG_TRACE(
				"AampRialtoMonitorAV: getStats(srcId=%d) failed, "
				"reporting dropped=0", srcId);
			dropped = 0;
		}
	}

	// Time in current state, capped to one reporting interval.
	int64_t timeInState = m_tLastSampled - m_tLastReported;
	if (timeInState < 0)
	{
		timeInState = 0;
	}
	else if (timeInState > static_cast<int64_t>(m_config.reportIntervalMs))
	{
		timeInState = static_cast<int64_t>(m_config.reportIntervalMs);
	}

	m_notifiable->SendMonitorAvEvent(
		m_description,
		m_videoPositionMs,
		m_audioPositionMs,
		static_cast<uint64_t>(timeInState),
		dropped);
}
