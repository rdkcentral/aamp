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
 * @file AampRialtoPlayer.cpp
 * @brief Implementation of AampRialtoPlayer - all StreamSink calls are
 *        forwarded to the internally owned AAMPGstPlayer instance.
 */

#include "AampRialtoPlayer.h"
#include "AampRialtoMediaPipelineClient.h"
#include "AampRialtoMediaSource.h"
#include "AampDrmBridge.h"
#include "AampLogManager.h"
#include "PrivateInstanceAAMPNotifiable.h"
#include "priv_aamp.h"
#include "IControl.h"
#include "AampRialtoControlBackend.h"
#include "AampRialtoMonitorAV.h"
#include <glib.h>
#include <chrono>
#include <cinttypes>
#include <algorithm>

// Running with real Rialto on Ubuntu locks up intermittently
// due to no TextTrack sink on Rialto Server, this disables
// subtitle support
//#define RIALTO_PLAYER_DISABLE_SUBTITLES

// ---------------------------------------------------------------------------
// Rialto -> AAMP log bridge
// ---------------------------------------------------------------------------

void AampRialtoPlayer::RialtoLogHandler::log(
	Level level,
	const std::string &file,
	int line,
	const std::string &function,
	const std::string &message)
{
	const char *tag = "RialtoClient";
	switch (level)
	{
	case Level::Fatal:
	case Level::Error:
		AAMPLOG_ERR("%s [%s:%d %s] %s",
			tag, file.c_str(), line, function.c_str(), message.c_str());
		break;
	case Level::Warning:
		AAMPLOG_WARN("%s [%s:%d %s] %s",
			tag, file.c_str(), line, function.c_str(), message.c_str());
		break;
	case Level::Milestone:
		AAMPLOG_MIL("%s [%s:%d %s] %s",
			tag, file.c_str(), line, function.c_str(), message.c_str());
		break;
	case Level::Info:
	case Level::External:
		AAMPLOG_INFO("%s [%s:%d %s] %s",
			tag, file.c_str(), line, function.c_str(), message.c_str());
		break;
	case Level::Debug:
	default:
		AAMPLOG_TRACE("%s [%s:%d %s] %s",
			tag, file.c_str(), line, function.c_str(), message.c_str());
		break;
	}
}

// ---------------------------------------------------------------------------
// ProgressTimer implementation
// ---------------------------------------------------------------------------

AampRialtoPlayer::ProgressTimer::~ProgressTimer()
{
	stop();
}

void AampRialtoPlayer::ProgressTimer::start(guint interval_ms, Callback cb)
{
	if (started)
	{
		AAMPLOG_INFO("Progress timer already running");
		return;
	}

	if (interval_ms == 0)
	{
		AAMPLOG_WARN("Invalid interval=%u ms; timer not started", interval_ms);
		return;
	}

	interval = interval_ms;
	callback = std::move(cb);
	started = true;

	// Run immediately first
	runOnce();

	// Then schedule periodic timeout
	source_id = g_timeout_add(interval, &ProgressTimer::timeout_handler, this);

	if (source_id == 0)
	{
		AAMPLOG_WARN("Failed to schedule progress timer");
		started = false;
		callback = nullptr;
	}
	else
	{
		AAMPLOG_INFO("Started progress timer (interval=%u ms)", interval);
	}
}

void AampRialtoPlayer::ProgressTimer::kick()
{
	if (!started)
	{
		return;
	}

	// Remove the existing periodic source first so it cannot fire
	// concurrently with the immediate runOnce() call below.
	if (source_id != 0)
	{
 		if (!g_source_remove(source_id))
 		{
 			AAMPLOG_WARN("Failed to remove progress timer id=%u", source_id);
 		}
	}

	// Dispatch immediately.
	runOnce();

	// Reschedule the periodic timeout, resetting the interval from now.
	source_id = g_timeout_add(interval, &ProgressTimer::timeout_handler, this);
	if (source_id == 0)
	{
		AAMPLOG_WARN("Failed to reschedule progress timer after kick");
		started = false;
	}
	else
	{
		AAMPLOG_INFO("Progress timer kicked (rescheduled)");
	}
}

void AampRialtoPlayer::ProgressTimer::stop()
{
	if (!started)
	{
		return;
	}

	if (source_id != 0)
	{
 		if (!g_source_remove(source_id))
 		{
 			AAMPLOG_WARN("Failed to remove progress timer id=%u", source_id);
 		}
		source_id = 0;
	}

	started = false;
	callback = nullptr;
	AAMPLOG_INFO("Stopped progress timer");
}

gboolean AampRialtoPlayer::ProgressTimer::timeout_handler(gpointer data)
{
	auto *self = static_cast<ProgressTimer *>(data);
	self->runOnce();
	return G_SOURCE_CONTINUE;
}

void AampRialtoPlayer::ProgressTimer::runOnce()
{
	if (callback)
	{
		callback();
	}
}

namespace {
	/// Upper bound for the wait on Rialto's application state transitioning
	/// to RUNNING.  In practice the transition completes in a few milliseconds;
	/// the timeout exists only to avoid a permanent hang if the Rialto server
	/// never reports RUNNING.
	constexpr int kRialtoRunningTimeoutMs = 2000;

	constexpr unsigned int kMsPerSecond = 1000;
}

// ---------------------------------------------------------------------------
// Source lookup helpers
// ---------------------------------------------------------------------------

AampRialtoMediaSource *AampRialtoPlayer::getSource(AampMediaType type)
{
	auto idx = static_cast<size_t>(type);
	if (idx < kMaxSourceTypes)
	{
		return m_sources[idx].get();
	}
	return nullptr;
}

AampRialtoMediaSource *AampRialtoPlayer::findSourceByRialtoId(
	int32_t rialtoSourceId)
{
	for (auto &source : m_sources)
	{
		if (source && source->sourceId() == rialtoSourceId)
		{
			return source.get();
		}
	}
	return nullptr;
}

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

// Defined in AampRialtoSourceCreators.cpp (production) or
// FakeAampRialtoSourceCreators.cpp (tests).
SourceCreator makeDefaultSourceCreator();

AampRialtoPlayer::AampRialtoPlayer(
	PrivateInstanceAAMP *aamp,
	id3_callback_t id3HandlerCallback,
	std::function<void(const unsigned char *, int, int, int)> exportFrames)
	: AampRialtoPlayer(
		aamp,
		/*notifiable=*/nullptr,
		std::make_unique<AampRialtoControlBackend>(),
		id3HandlerCallback,
		std::move(exportFrames),
		makeDefaultSourceCreator())
{
}

AampRialtoPlayer::AampRialtoPlayer(
	PrivateInstanceAAMP *aamp,
	IStreamSinkNotifiable *notifiable,
	std::unique_ptr<IRialtoControlBackend> controlBackend,
	id3_callback_t id3HandlerCallback,
	std::function<void(const unsigned char *, int, int, int)> exportFrames,
	SourceCreator sourceCreator)
	: m_aamp(aamp)
	, m_controlBackend(std::move(controlBackend))
	, m_ID3MetadataHandler{std::move(id3HandlerCallback)}
	, m_sourceCreator(std::move(sourceCreator))
	, m_client(nullptr)
	, m_pipeline(nullptr)
{
	if (notifiable == nullptr)
	{
		m_notifiableAdapter = std::make_unique<PrivateInstanceAAMPNotifiable>(aamp);
		m_notifiable = m_notifiableAdapter.get();
	}
	else
	{
		m_notifiable = notifiable;
	}
	AAMPLOG_INFO("AampRialtoPlayer: constructed, aamp=%p", aamp);
}

AampRialtoPlayer::~AampRialtoPlayer()
{
	StopProgressTimer();
	for (auto &source : m_sources)
	{
		if (source)
		{
			source->invalidateGeneration(m_pipeline.get(), "~AampRialtoPlayer");
		}
	}
	AAMPLOG_INFO("AampRialtoPlayer: destroyed");
}

// ---------------------------------------------------------------------------
// StreamSink overrides
// ---------------------------------------------------------------------------

bool AampRialtoPlayer::ShouldRecreatePipeline(
	StreamOutputFormat videoFormat,
	StreamOutputFormat audioFormat,
	StreamOutputFormat subFormat,
	bool bESChangeStatus,
	bool setReadyAfterPipelineCreation) const
{
	// Explicit override flags always force a full recreation.
	if (m_pipelineStopped.load(std::memory_order_relaxed) ||
	    bESChangeStatus ||
	    setReadyAfterPipelineCreation)
	{
		AAMPLOG_INFO("ShouldRecreatePipeline true ");
		return true;
	}

	const auto *videoSrc = m_sources[eMEDIATYPE_VIDEO].get();
	const auto *audioSrc = m_sources[eMEDIATYPE_AUDIO].get();
	const auto *subtitleSrc = m_sources[eMEDIATYPE_SUBTITLE].get();

	// Video track: any change  add, remove, or codec change - needs rebuild.
	if (videoSrc == nullptr)
	{
		if (videoFormat != FORMAT_INVALID)
		{
			AAMPLOG_INFO("Video: ShouldRecreatePipeline true");
			return true;  // Need to add video source.
		}
	}
	else if (videoFormat == FORMAT_INVALID)
	{
		AAMPLOG_INFO("Video source going away: ShouldRecreatePipeline true");
		return true;  // Video source going away.
	}
	else if (videoSrc->format() != videoFormat)
	{
		AAMPLOG_INFO("Video codec changed (old=%d, new=%d): ShouldRecreatePipeline true",
			videoSrc->format(), videoFormat);
		return true;  // Video codec changed.
	}

	// Audio track: FORMAT_INVALID while a source exists is the trickplay-EOS
	// case.  That is handled separately in Configure() and does NOT require
	// a pipeline rebuild.
	if (audioSrc == nullptr)
	{
		if (audioFormat != FORMAT_INVALID)
		{
			AAMPLOG_INFO("Need to add audio source: ShouldRecreatePipeline true");
			return true;  // Need to add audio source.
		}
	}
	else if (audioFormat != FORMAT_INVALID &&
	         audioSrc->format() != audioFormat)
	{
		AAMPLOG_INFO("Audio codec changed to a different valid format (old=%d, new=%d): ShouldRecreatePipeline true",
			audioSrc->format(), audioFormat);
		return true;  // Audio codec changed to a different valid format.
	}

	const int rate = m_rate.load(std::memory_order_relaxed);
	if(rate == AAMP_NORMAL_PLAY_RATE)
	{
		if ((subtitleSrc == nullptr) || (subtitleSrc->format() != subFormat))
		{
			AAMPLOG_INFO("subFormat=%d, subtitleSrc->format()=%d. ShouldRecreatePipeline true", subFormat, subtitleSrc->format());
			return true;
		}
	}

	AAMPLOG_INFO("EXIT ShouldRecreatePipeline false");
	return false;
}

// ---------------------------------------------------------------------------
// WaitForFlushToComplete
// ---------------------------------------------------------------------------

void AampRialtoPlayer::WaitForFlushToComplete()
{
	std::unique_lock<std::mutex> lock(m_flushMutex);
	// Block until the state machine leaves FLUSHING.  Holds m_flushMutex
	// while the predicate is evaluated so that the optional claim below is
	// atomic with the check: no concurrent caller can pass the predicate
	// and also claim FLUSHING before us.
	m_flushCv.wait(lock, [this]()
	{
		return m_stateMachine.currentState() != PlayerStateId::FLUSHING;
	});
}

void AampRialtoPlayer::Configure(
	StreamOutputFormat videoFormat,
	StreamOutputFormat audioFormat,
	StreamOutputFormat subFormat,
	bool bESChangeStatus,
	bool setReadyAfterPipelineCreation)
{
	AAMPLOG_INFO("ENTRY videoFormat=%d audioFormat=%d subFormat=%d bESChangeStatus=%d setReadyAfterPipelineCreation=%d", static_cast<int>(videoFormat), static_cast<int>(audioFormat),
		static_cast<int>(subFormat), bESChangeStatus, setReadyAfterPipelineCreation);

	// If a flush cycle is in progress, block until all sources finish
	// flushing so m_rate is committed before ShouldRecreatePipeline reads it.
	WaitForFlushToComplete();

	StopProgressTimer();

	// Guard: skip teardown and recreation when the pipeline can be reused.
	// Rialto does not support dynamic source management, so any change to
	// the source set requires a full rebuild.  The exception is audio going
	// FORMAT_INVALID (trickplay entry): that is handled by signalling EOS on
	// the audio source so video continues without interruption.
	{
		const bool audioGoingInvalid =
			m_sources[eMEDIATYPE_AUDIO] != nullptr &&
			audioFormat == FORMAT_INVALID;

		if (!ShouldRecreatePipeline(videoFormat, audioFormat, subFormat,
		        bESChangeStatus, setReadyAfterPipelineCreation))
		{
			if (audioGoingInvalid)
			{
				AAMPLOG_INFO("Audio going FORMAT_INVALID (trickplay) - "
					"signalling EOS on audio source, no pipeline recreation");
				EndOfStreamReached(eMEDIATYPE_AUDIO);
				EndOfStreamReached(eMEDIATYPE_SUBTITLE);
			}
			else if (m_sources[eMEDIATYPE_AUDIO] &&
			         audioFormat != FORMAT_INVALID)
			{
				// Trickplay exit: if audio was EOS'd (trickplay), clear it
				// so the injection path can resume.
				m_sources[eMEDIATYPE_AUDIO]->setEos(false,
					"Configure trickplay-exit");
			}

			// Resume downloads for all existing sources so AAMP's track
			// worker threads are unblocked even when the pipeline is reused.
			for (size_t i = 0; i < kMaxSourceTypes; ++i)
			{
				if (m_sources[i])
				{
					m_aamp->ResumeTrackDownloads(
						static_cast<AampMediaType>(i));
				}
			}

			AAMPLOG_INFO("EXIT - source set unchanged, skipping pipeline recreation");
			return;
		}
	}

	// Clear the stopped flag now that we are about to rebuild the pipeline.
	m_pipelineStopped.store(false, std::memory_order_relaxed);

	// Signal the state machine that Configure() is starting a new session
	// (re-tune or first tune).  This resets to IDLE from whatever previous
	// state the player was in.
	m_stateMachine.onReconfigure();

	// Reset first-frame flag so the new tune session forwards the initial
	// PLAYING notification correctly.
	m_firstFrameNotified.store(false, std::memory_order_relaxed);

	if (!m_client)
	{
		m_client = std::make_shared<AampRialtoMediaPipelineClient>();
	}

	// Reset per-source state for a fresh session.
	for (auto &source : m_sources)
	{
		if (source)
		{
			source->reset();
		}
	}

	// NOTE: m_videoProt / m_audioProt are intentionally NOT reset here.
	// QueueProtectionEvent() is called by AAMP before Configure() in the
	// normal playback flow; clearing the stored protection params here would
	// discard them before AttachVideoSource / AttachAudioSource can use them
	// to call createSession().  ClearProtectionEvent() handles teardown.
	// NOTE: m_pendingPositionNs is intentionally NOT reset here.
	// Flush() may be called before Configure() to pre-stage the seek position;
	// clearing it here would discard that staged value before sources attach.
	m_playRequested.store(false, std::memory_order_relaxed);
	m_allSourcesAttachedFlag.store(false, std::memory_order_relaxed);
	for (auto &pa : m_pendingAttach)
	{
		pa.reset();
	}

	// Register Rialto -> AAMP log bridge once.
	if (!m_rialtoLogHandler)
	{
		m_rialtoLogHandler = std::make_shared<RialtoLogHandler>();
		auto logControlFactory =
			firebolt::rialto::IClientLogControlFactory::createFactory();
		if (logControlFactory)
		{
			logControlFactory->createClientLogControl()
				.registerLogHandler(m_rialtoLogHandler,
					/*ignoreLogLevels=*/true);
		}
		else
		{
			AAMPLOG_WARN("Failed to create IClientLogControlFactory - Rialto logs suppressed");
		}
	}

	if (!m_pipelineFactory)
	{
		m_pipelineFactory = firebolt::rialto::IMediaPipelineFactory::createFactory();
	}
	if (!m_pipelineCapabilities)
	{
		auto capFactory =
			firebolt::rialto::IMediaPipelineCapabilitiesFactory::createFactory();
		if (capFactory)
		{
			m_pipelineCapabilities = capFactory->createMediaPipelineCapabilities();
		}
		else
		{
			AAMPLOG_WARN("Failed to create IMediaPipelineCapabilitiesFactory");
		}
	}
	auto &factory = m_pipelineFactory;
	if (!factory)
	{
		AAMPLOG_ERR("Failed to create IMediaPipelineFactory - is the Rialto server running?");
	}
	else
	{
		// Wait for the Rialto server to report ApplicationState::RUNNING before
		// creating the media pipeline - ensures the proxy ctor sees RUNNING from
		// its internal registerClient() call, preventing NeedMediaData events
		// from being silently dropped.
		if (m_controlBackend && !m_controlBackend->waitForRunning(kRialtoRunningTimeoutMs))
		{
			AAMPLOG_WARN(
				"Proceeding to createMediaPipeline despite Rialto state not RUNNING");
		}

		constexpr std::uint32_t kWidth{3840};
		constexpr std::uint32_t kHeight{2160};
		firebolt::rialto::VideoRequirements kRequirements{kWidth, kHeight};
		m_pipeline = factory->createMediaPipeline(
			std::weak_ptr<firebolt::rialto::IMediaPipelineClient>(m_client),
			kRequirements);
		if (!m_pipeline)
		{
			AAMPLOG_ERR("createMediaPipeline returned nullptr - check Rialto server logs (syslog) for details");
		}
		else
		{
			AAMPLOG_INFO("Created pipeline %p", m_pipeline.get());

			// TODO: How do we determine value for isLive
			if (!m_pipeline->load(
					firebolt::rialto::MediaType::MSE,
					"video/mp4",
					/*url=*/"",
					/*isLive*/false))
			{
				AAMPLOG_ERR("load() failed - Rialto will reject attachSource calls");
			}
			else
			{
				AAMPLOG_INFO("load() succeeded");

				m_client->SetNeedDataCallback(
					[this](int32_t sid, size_t fc, uint32_t rid) {
						OnNeedMediaData(sid, fc, rid);
					});
				m_client->SetCancelNeedDataCallback(
					[this](int32_t sid) {
						OnCancelNeedMediaData(sid);
					});
				m_client->SetPlaybackStateCallback(
					[this](firebolt::rialto::PlaybackState state) {
						OnPlaybackState(state);
					});
				m_client->SetPositionCallback(
					[this](int64_t posNs) {
						OnPosition(posNs);
					});
				m_client->SetDurationCallback(
					[this](int64_t durNs) {
						OnDuration(durNs);
					});
				m_client->SetBufferUnderflowCallback(
					[this](int32_t sid) {
						OnBufferUnderflow(sid);
					});

				// Advance state machine: pipeline is now created and loaded.
				m_stateMachine.onPipelineLoaded();

				// Create the AV health monitor when the feature is enabled.
				if (m_aamp->mConfig->IsConfigSet(eAAMPConfig_MonitorAV))
				{
					const double progressSec =
						m_aamp->mConfig->GetConfigValue(
							eAAMPConfig_ReportProgressInterval);
					AampRialtoMonitorAV::Config monCfg{};
					monCfg.sampleIntervalMs =
						static_cast<int>(progressSec * 1000.0);
					monCfg.reportIntervalMs =
						m_aamp->mConfig->GetConfigValue(
							eAAMPConfig_MonitorAVReportingInterval);
					monCfg.syncThresholdPositiveMs =
						m_aamp->mConfig->GetConfigValue(
							eAAMPConfig_MonitorAVSyncThresholdPositive);
					monCfg.syncThresholdNegativeMs =
						m_aamp->mConfig->GetConfigValue(
							eAAMPConfig_MonitorAVSyncThresholdNegative);
					monCfg.jumpThresholdMs =
						m_aamp->mConfig->GetConfigValue(
							eAAMPConfig_MonitorAVJumpThreshold);

					m_monitorAV = std::make_unique<AampRialtoMonitorAV>(
						m_pipeline,
						m_notifiable,
						[this]() -> int32_t
						{
							auto &src = m_sources[eMEDIATYPE_VIDEO];
							return src ? src->sourceId() : -1;
						},
						[this]() -> int { return m_rate.load(); },
						[this]() -> bool
						{
							return m_stateMachine.currentState()
								== PlayerStateId::PLAYING;
						},
						monCfg);

					AAMPLOG_MIL("AampRialtoMonitorAV created");
				}
			}
		}
	}

	// Create per-source objects based on configured formats.
	// FORMAT_ISO_BMFF: AampRialtoPlayer demuxes via SendTransfer; the demuxer
	//                  is created lazily on the first SendTransfer call.
	// FORMAT_UNKNOWN:  streamabstraction demuxes externally (SetStreamCaps path).

	if (videoFormat != FORMAT_INVALID)
	{
		auto src = m_sourceCreator(eMEDIATYPE_VIDEO);
		if (src)
		{
			src->setFormat(videoFormat);
			m_sources[eMEDIATYPE_VIDEO] = std::move(src);
			m_aamp->ResumeTrackDownloads(eMEDIATYPE_VIDEO);
			AAMPLOG_INFO("Created video source (format=%d)", static_cast<int>(videoFormat));
		}
	}
	if (audioFormat != FORMAT_INVALID)
	{
		auto src = m_sourceCreator(eMEDIATYPE_AUDIO);
		if (src)
		{
			src->setFormat(audioFormat);
			m_sources[eMEDIATYPE_AUDIO] = std::move(src);
			m_aamp->ResumeTrackDownloads(eMEDIATYPE_AUDIO);
			AAMPLOG_INFO("Created audio source (format=%d)", static_cast<int>(audioFormat));
		}
	}
#if !defined(RIALTO_PLAYER_DISABLE_SUBTITLES)
	if (subFormat == FORMAT_SUBTITLE_TTML || subFormat == FORMAT_SUBTITLE_MP4 || subFormat == FORMAT_SUBTITLE_WEBVTT)
	{
		auto src = m_sourceCreator(eMEDIATYPE_SUBTITLE);
		if (src)
		{
			src->setFormat(subFormat);
			m_sources[eMEDIATYPE_SUBTITLE] = std::move(src);
			m_aamp->ResumeTrackDownloads(eMEDIATYPE_SUBTITLE);
			AAMPLOG_INFO("Created subtitle source (format=%d)", static_cast<int>(subFormat));

			// For raw subtitle formats (TTML/WebVTT) there is no MP4 init
			// segment, so no AampMp4Demuxer is created and SetStreamCaps
			// is never called for subtitle.  Queue the source attachment
			// here so it fires once video attaches (same deferred path as
			// audio).  FORMAT_SUBTITLE_MP4 is also handled via the demuxer
			// updateCachedMetadata → SetStreamCaps path.
			MediaCodecInfo ci{};
			ci.mCodecFormat = (subFormat == FORMAT_SUBTITLE_WEBVTT)
						? GST_FORMAT_SUBTITLE_WEBVTT
						: GST_FORMAT_SUBTITLE_TTML;
			AAMPLOG_INFO("Queueing subtitle attachment for format=%d",
				static_cast<int>(subFormat));
			AttachSource(*m_sources[eMEDIATYPE_SUBTITLE], ci);
		}
	}
	else if (videoFormat != FORMAT_INVALID)
	{
		// No sidecar subtitle track — create an inband CC source that uses
		// the "application/x-subtitle-cc" MIME type so the Rialto pipeline
		// can deliver closed-caption data from the video stream.
		auto src = m_sourceCreator(eMEDIATYPE_SUBTITLE);
		if (src)
		{
			src->setFormat(FORMAT_INVALID);
			m_sources[eMEDIATYPE_SUBTITLE] = std::move(src);
			MediaCodecInfo ci{};
			ci.mCodecFormat = GST_FORMAT_UNKNOWN;
			AttachSource(*m_sources[eMEDIATYPE_SUBTITLE], ci);
			AAMPLOG_INFO("Created inband CC subtitle source");
		}
	}
#endif
	AAMPLOG_INFO("EXIT");
}

static GstStreamOutputFormat toGstStreamOutputFormat(StreamOutputFormat fmt)
{
	struct FormatMapEntry
	{
		StreamOutputFormat source;
		GstStreamOutputFormat target;
	};

	GstStreamOutputFormat gstFmt = GST_FORMAT_UNKNOWN;
	static const FormatMapEntry kFormatMap[] = {
		{ FORMAT_INVALID, GST_FORMAT_INVALID },
		{ FORMAT_MPEGTS, GST_FORMAT_MPEGTS },
		{ FORMAT_ISO_BMFF, GST_FORMAT_ISO_BMFF },
		{ FORMAT_AUDIO_ES_MP3, GST_FORMAT_AUDIO_ES_MP3 },
		{ FORMAT_AUDIO_ES_AAC, GST_FORMAT_AUDIO_ES_AAC },
		{ FORMAT_AUDIO_ES_AAC_RAW, GST_FORMAT_AUDIO_ES_AAC_RAW },
		{ FORMAT_AUDIO_ES_AC3, GST_FORMAT_AUDIO_ES_AC3 },
		{ FORMAT_AUDIO_ES_EC3, GST_FORMAT_AUDIO_ES_EC3 },
		{ FORMAT_AUDIO_ES_ATMOS, GST_FORMAT_AUDIO_ES_ATMOS },
		{ FORMAT_AUDIO_ES_AC4, GST_FORMAT_AUDIO_ES_AC4 },
		{ FORMAT_VIDEO_ES_H264, GST_FORMAT_VIDEO_ES_H264 },
		{ FORMAT_VIDEO_ES_HEVC, GST_FORMAT_VIDEO_ES_HEVC },
		{ FORMAT_VIDEO_ES_MPEG2, GST_FORMAT_VIDEO_ES_MPEG2 },
		{ FORMAT_SUBTITLE_WEBVTT, GST_FORMAT_SUBTITLE_WEBVTT },
		{ FORMAT_SUBTITLE_TTML, GST_FORMAT_SUBTITLE_TTML },
		{ FORMAT_SUBTITLE_MP4, GST_FORMAT_SUBTITLE_MP4 },
		{ FORMAT_UNKNOWN, GST_FORMAT_UNKNOWN }
	};

	for (size_t i = 0; i < (sizeof(kFormatMap) / sizeof(kFormatMap[0])); ++i)
	{
		if (kFormatMap[i].source == fmt)
		{
			gstFmt = kFormatMap[i].target;
			break;
		}
	}
	return gstFmt;
}

bool AampRialtoPlayer::SendCopy(
	AampMediaType mediaType,
	std::vector<uint8_t> &&buffer,
	double fpts,
	double fdts,
	double fDuration)
{
	AAMPLOG_INFO("ENTRY mediaType=%d bufferSize=%zu fpts=%f fdts=%f fDuration=%f", static_cast<int>(mediaType), buffer.size(), fpts, fdts, fDuration);
	bool success = false;

	auto *source = getSource(mediaType);
	if (!source)
	{
		// No source for this track (e.g. subtitle not yet supported).
		AAMPLOG_WARN("No source for mediaType=%d",
			static_cast<int>(mediaType));
	}
	else if (buffer.empty())
	{
		AAMPLOG_WARN("Buffer is empty for mediaType=%d",
			static_cast<int>(mediaType));
	}
	else if (!m_pipeline)
	{
		AAMPLOG_WARN("No pipeline - cannot process data fragment");
	}
	else
	{
		if (!source->isAttached())
		{
			// Attaching all sources to avoid deadlock with muxed HLS-TS which uses only one injection thread
			// For HLS-TS, the codec format is all that Rialto requires to set the stream caps.
			for (const auto& source2: m_sources)
			{
				if (source2 && !source2->isAttached())
				{
					AAMPLOG_INFO("Setting stream caps for mediaType=%d", static_cast<int>(source2->mediaType()));
					MediaCodecInfo codecInfo{};
					codecInfo.mCodecFormat = toGstStreamOutputFormat(source2->format());
					SetStreamCaps(source2->mediaType(), std::move(codecInfo));
				}
			}
		}

		auto sharedBuffer =
			std::make_shared<std::vector<uint8_t>>(std::move(buffer));
		if (!source->processDataFragment(
					*m_pipeline, std::move(sharedBuffer),
					fpts, fdts, fDuration, 0.0))
		{
			AAMPLOG_WARN("processDataFragment failed for mediaType=%d",
				static_cast<int>(mediaType));
		}
		else
		{
			success = true;
		}
	}

	AAMPLOG_INFO("EXIT, success=%d", success);
	return success;
}

bool AampRialtoPlayer::SendTransfer(
	AampMediaType mediaType,
	std::vector<uint8_t> &&buffer,
	double fpts,
	double fdts,
	double fDuration,
	double fragmentPTSoffset,
	bool initFragment,
	bool discontinuity)
{
	AAMPLOG_INFO("ENTRY mediaType=%d bufferSize=%zu fpts=%f fdts=%f fDuration=%f fragmentPTSoffset=%f initFragment=%d discontinuity=%d",
				static_cast<int>(mediaType), buffer.size(),
				fpts, fdts, fDuration, fragmentPTSoffset,
				initFragment, discontinuity);

	auto *source = getSource(mediaType);
	if (!source)
	{
		// No source for this track (e.g. subtitle not yet supported).
		AAMPLOG_INFO("No source for mediaType=%d - ignoring transfer",
			static_cast<int>(mediaType));
		AAMPLOG_INFO("EXIT");
		return true;
	}

	if (buffer.empty())
	{
		AAMPLOG_INFO("EXIT");
		return true;
	}

	auto sharedBuffer =
		std::make_shared<std::vector<uint8_t>>(std::move(buffer));
	bool result = true;

	if (initFragment)
	{
		auto codecInfo = source->processInitFragment(std::move(sharedBuffer));
		if (!codecInfo)
		{
			AAMPLOG_ERR("processInitFragment failed mediaType=%d",
				static_cast<int>(mediaType));
			result = false;
		}
		else
		{
			std::lock_guard<std::mutex> lock(m_attachMutex);
			if (m_pipeline)
			{
				AttachSource(*source, *codecInfo);
			}
			else
			{
				AAMPLOG_ERR("pipeline not created");
			}
		}
	}
	else if (m_pipeline)
	{
		if (!source->processDataFragment(
				*m_pipeline, std::move(sharedBuffer),
				fpts, fdts, fDuration, fragmentPTSoffset))
		{
			result = false;
		}
	}

	AAMPLOG_INFO("EXIT");
	return result;
}

// ---------------------------------------------------------------------------
// AttachSource - unified attach via polymorphic source
// ---------------------------------------------------------------------------

void AampRialtoPlayer::AttachSource(
	AampRialtoMediaSource &source, MediaCodecInfo &codecInfo)
{
	const auto type = source.mediaType();

	// THEORY (unproven - revert this block if disproved):
	// In the failing first-tune log, audio attached first (id=1) and video
	// second (id=2); the Rialto server then reported:
	//   "audsrc: not-linked (-1)"
	// and transitioned SOURCES_ATTACHED -> ERROR.  In the passing second-tune
	// log, video happened to attach first and no error occurred.  The
	// hypothesis is that GStreamer's playbin/uridecodebin autoplugging requires
	// video to be present before audio is added.  This has NOT been confirmed
	// via Rialto documentation or a controlled experiment (e.g. forcing
	// audio-first on the second tune to reproduce the failure).
	// Alternative explanations: cold-start pipeline state, different DRM
	// latency (mksId=0 vs mksId=1), or a first-pipeline-after-boot race.
	if (type != eMEDIATYPE_VIDEO &&
	    m_sources[eMEDIATYPE_VIDEO] &&
	    !m_sources[eMEDIATYPE_VIDEO]->isAttached())
	{
		AAMPLOG_INFO("Deferring attachment of mediaType=%d until video is attached",
			static_cast<int>(type));
		// Signal inject threads to block rather than discard frames.
		// Without this, media fragments are silently dropped while the
		// source is unattached, leaving an audio gap that causes
		// GStreamer's A/V sync to skip video forward at startup.
		{
			auto &st = source.state();
			std::lock_guard<std::mutex> lock(st.mu);
			st.attachPending = true;
		}
		m_pendingAttach[type] = std::move(codecInfo);
		return;
	}

	if (!source.isAttached())
	{
		m_stateMachine.onSourceAttaching();
	}

	// Ensure m_drmBridge is initialized before attaching source.
	// Lazy initialization supports pre-roll ads: if SetEncryptedAamp was
	// called, use that player's DRM session manager; otherwise, use m_aamp.
	if (!m_drmBridge)
	{
		m_drmBridge = std::make_shared<AampDrmBridge>(m_aamp);
		AAMPLOG_INFO("Creating m_drmBridge with m_aamp=%p", m_aamp);
	}

	auto result = source.attachOrUpdate(
		*m_pipeline, codecInfo, m_drmBridge.get(),
		m_pendingPositionNs.load(std::memory_order_relaxed),
		m_pendingProtection[static_cast<size_t>(type)],
		computeAppliedRate(
			m_pendingFlushRate.load(std::memory_order_relaxed)));

	if (result == AampRialtoMediaSource::AttachResult::NEWLY_ATTACHED ||
	    result == AampRialtoMediaSource::AttachResult::UPDATED)
	{
		// Clear attachPending and wake any inject thread that was blocking
		// because the source's attachment was deferred.
		//
		// injectionGated is deliberately NOT cleared here.  AttachSource()
		// can run mid-Configure(), before a subsequent Flush() completes a
		// multi-step trickplay sequence (Flush(pos=0) -> Configure() ->
		// Flush(correctPos) -> Stream()); clearing the gate here would let
		// an early needData slip through with stale-position data. The gate
		// is cleared only via UngateAllSources(), called at the points that
		// actually issue play().
		{
			auto &st = source.state();
			std::lock_guard<std::mutex> lock(st.mu);
			st.attachPending = false;
			st.cv.notify_all();
		}
	}

	if (result == AampRialtoMediaSource::AttachResult::NEWLY_ATTACHED)
	{
		// After video attaches, drain any non-video sources that were deferred
		// by the video-before-audio ordering theory above.
		if (type == eMEDIATYPE_VIDEO)
		{
			for (size_t i = 0; i < kMaxSourceTypes; ++i)
			{
				if (i != eMEDIATYPE_VIDEO &&
				    m_pendingAttach[i].has_value() &&
				    m_sources[i])
				{
					AAMPLOG_INFO("Processing deferred attachment for mediaType=%zu", i);
					AttachSource(*m_sources[i], *m_pendingAttach[i]);
					m_pendingAttach[i].reset();
				}
			}
		}

		// Re-apply cached subtitle mute state when the subtitle source first
		// attaches.  SetSubtitleMute() may have been called before the source
		// was ready (e.g. user muted before playback started, or between period
		// transitions when the source is detached and re-attached).
		if (type == eMEDIATYPE_SUBTITLE && m_subtitleMuted)
		{
			AAMPLOG_INFO("Applying cached subtitle mute on attach sourceId=%d",
				source.sourceId());
			m_pipeline->setMute(source.sourceId(), true);
		}

		CheckAllSourcesAttached();
	}
}

void AampRialtoPlayer::UngateAllSources(const char *reason)
{
	for (auto &source : m_sources)
	{
		if (source)
		{
			source->clearInjectionGate(m_pipeline.get(), reason);
		}
	}
}

void AampRialtoPlayer::CheckAllSourcesAttached()
{
	if (!m_pipeline)
	{
		return;
	}

	// Guard: allSourcesAttached() must be called exactly once per pipeline
	// session.  The machine is in SOURCES_ATTACHING only while waiting for
	// all sources to register; once it advances past that state the call
	// has already been issued.
	if (m_stateMachine.currentState() != PlayerStateId::SOURCES_ATTACHING)
	{
		return;
	}

	for (auto &source : m_sources)
	{
		if (source && !source->isAttached())
		{
			return;
		}
	}

	AAMPLOG_INFO("All sources attached - calling allSourcesAttached()");

	if (!m_pipeline->allSourcesAttached())
	{
		AAMPLOG_ERR("allSourcesAttached() failed");
	}
	else
	{
		m_stateMachine.onAllSourcesAttached();
		// seq_cst store: pairs with the seq_cst load in Stream() so that
		// either Stream() sees the flag and calls play() itself, or this
		// function sees m_playRequested=true and calls it here.  The state
		// machine transition above uses a mutex (not seq_cst) so it cannot
		// substitute for this atomic rendezvous.
		m_allSourcesAttachedFlag.store(true, std::memory_order_seq_cst);

		if (m_playRequested.load(std::memory_order_seq_cst))
		{
			AAMPLOG_INFO("play() deferred by Stream() - issuing now");
			m_playRequested.store(false, std::memory_order_relaxed);
			UngateAllSources("CheckAllSourcesAttached");
			bool async = false;
			if (!m_pipeline->play(async))
			{
				AAMPLOG_ERR("play() failed");
			}
		}
	}
}

bool AampRialtoPlayer::SendSample(AampMediaType mediaType, AampMediaSample &&sample, bool morePending)
{
	AAMPLOG_INFO("ENTRY mediaType=%d pts=%f dur=%f morePending=%d",
		static_cast<int>(mediaType), sample.mPts, sample.mDuration, morePending);

	bool result = false;

	auto *source = getSource(mediaType);
	if (!source)
	{
		AAMPLOG_WARN("unsupported mediaType=%d", static_cast<int>(mediaType));
		AAMPLOG_INFO("EXIT result=%d", result);
		return result;
	}

	if (m_pipeline)
	{
		result = source->injectSingleSample(*m_pipeline, std::move(sample), morePending);
	}

	AAMPLOG_INFO("EXIT result=%d", result);
	return result;
}

bool AampRialtoPlayer::PipelineConfiguredForMedia(AampMediaType type)
{
	AAMPLOG_INFO("ENTRY type=%d", static_cast<int>(type));
	auto *source = getSource(type);
	bool result = source && source->isAttached();
	AAMPLOG_INFO("EXIT result=%d", result);
	return result;
}

void AampRialtoPlayer::EndOfStreamReached(AampMediaType type)
{
	AAMPLOG_INFO("ENTRY type=%d", static_cast<int>(type));
	auto *source = getSource(type);
	if (source)
	{
		source->signalEos(m_pipeline.get());
	}

	// In direct-Rialto mode every attached source must receive haveData(EOS)
	// before the server emits END_OF_STREAM. Here we compensate in two scenarios:
	//
	// Audio EOS triggers subtitle EOS.  Covers:
	//    a. Content with no subtitle tracks (e.g. an ad): subtitle source was
	//       created from a saved header but will receive no data.

	if (type == eMEDIATYPE_VIDEO)
	{
		auto *subtitleSrc = m_sources[eMEDIATYPE_SUBTITLE].get();

		if (subtitleSrc
		    && subtitleSrc->isAttached()
		    && !subtitleSrc->state().eos)
		{
			AAMPLOG_INFO("auto-signaling subtitle EOS: video at EOS");
			subtitleSrc->signalEos(m_pipeline.get());
		}
	}

	AAMPLOG_INFO("EXIT");
}

void AampRialtoPlayer::Stream()
{
	AAMPLOG_INFO("ENTRY");
	if (m_pipeline)
	{
		// Signal that play() should be issued.  We use seq_cst ordering so
		// that CheckAllSourcesAttached() on the injection thread sees this
		// store before it reads m_playRequested (and vice-versa).
		m_playRequested.store(true, std::memory_order_seq_cst);

		// If a flush is in progress, play() will be issued by the SEEK_DONE
		// handler in OnPlaybackState() once onFlushComplete() restores state.
		// FLUSHING is set inside m_flushMutex in Flush() and cleared by
		// onFlushComplete() (both protected by the state machine mutex).
		if (m_stateMachine.currentState() == PlayerStateId::FLUSHING)
		{
			AAMPLOG_INFO("deferring play() - flush in progress");
			AAMPLOG_INFO("EXIT");
			return;
		}

		// seq_cst load: pairs with the seq_cst store in CheckAllSourcesAttached()
		// to guarantee one side always calls play().
		if (m_allSourcesAttachedFlag.load(std::memory_order_seq_cst))
		{
			m_playRequested.store(false, std::memory_order_relaxed);
			// Ungate unconditionally - Stream() is the point at which
			// playback is genuinely about to (re)start, regardless of
			// whether the state machine already happens to report
			// PLAYING.  play() itself is also issued unconditionally
			// below, rather than relying on the state machine's view of
			// Rialto's state to decide whether a call is "redundant".
			UngateAllSources("Stream");

			// allSourcesAttached() already completed before this call -
			// promote to PLAYING immediately.
			bool async = false;
			if (!m_pipeline->play(async))
			{
				AAMPLOG_ERR("play() failed");
			}
		}
		else
		{
			// Sources are not yet attached; play() will be issued by
			// CheckAllSourcesAttached() once all sources are registered.
			AAMPLOG_INFO("deferring play() until allSourcesAttached()");
		}
	}
	AAMPLOG_INFO("EXIT");
}

void AampRialtoPlayer::Stop(bool keepLastFrame)
{
	AAMPLOG_INFO("ENTRY keepLastFrame=%d", keepLastFrame);

	// Wait for any in-progress flush cycle to complete before stopping.
	// This ensures the state machine has already exited FLUSHING so that
	// onStop() transitions from a well-defined state (PLAYING, PAUSED,
	// SOURCES_ATTACHED, etc.) and the FLUSHING → STOPPED arc is never
	// reached.
	WaitForFlushToComplete();

	StopProgressTimer();
	if (m_monitorAV)
	{
		m_monitorAV->stop();
		m_monitorAV.reset();
	}
	
	// Wake any in-flight data so it abandons the current batch.
	for (auto &source : m_sources)
	{
		if (source)
		{
			source->invalidateGeneration(m_pipeline.get(), "Stop",
				/*newEosState=*/false);
		}
	}

	if (m_pipeline)
	{
		m_pipeline->stop();
	}
	// Mark the pipeline as stopped so the next Configure() always triggers
	// a full pipeline recreation, even when stream formats are unchanged.
	m_pipelineStopped.store(true, std::memory_order_relaxed);
	m_stateMachine.onStop();
	AAMPLOG_INFO("EXIT");
}

void AampRialtoPlayer::Flush(double position, int rate, bool shouldTearDown)
{
	AAMPLOG_INFO("ENTRY position=%f rate=%d shouldTearDown=%d", position, rate, shouldTearDown);

	// Step 1: Wait for any previous flush cycle to complete (no claim yet).
	// The teardown check below reads the current state AFTER any in-flight
	// flush has restored it (e.g. a seek from PLAYING restores to PLAYING).
	WaitForFlushToComplete();

	// Step 2: Decide whether to tear down or flush based on current state.
	// This check happens BEFORE claiming FLUSHING so that Stop() — which
	// also calls WaitForFlushToComplete() at its start — does not deadlock
	// on a FLUSHING state we have already claimed.
	const PlayerStateId preFlushState = m_stateMachine.currentState();
	const bool isPlayingPausedOrAttached =
		(preFlushState == PlayerStateId::PLAYING ||
		 preFlushState == PlayerStateId::SOURCES_ATTACHED ||
		 preFlushState == PlayerStateId::PAUSED);

	if (!isPlayingPausedOrAttached && shouldTearDown)
	{
		// Player is not in a flushable state; recover by stopping.
		// Stop() will call WaitForFlushToComplete() which returns immediately
		// since we have not claimed FLUSHING.
		AAMPLOG_WARN("Player was not in PLAYING/PAUSED/SOURCES_ATTACHED "
			"(pre-flush state=%d) and shouldTearDown=true - calling Stop(true)",
			static_cast<int>(preFlushState));
		Stop(true);
		AAMPLOG_INFO("EXIT - teardown requested");
		return;
	}

	if (!isPlayingPausedOrAttached)
	{
		// Not in a flushable state and no teardown requested.
		// Stage the parameters (covers the pre-Configure() seek-position
		// pre-staging path) and commit the rate, but do not attempt a full
		// flush cycle — onFlush() would be a no-op on the state machine.
		//
		// Still wake any blocked inject threads (sources may exist but not
		// yet be Rialto-attached in the deferred-attachment path) and, on
		// trickplay exit (rate == 1), clear any audio EOS set during trickplay
		// entry so injection can resume once the pipeline is ready.
		const int64_t posNs = static_cast<int64_t>(position * kNsPerSecond);
		m_pendingPositionNs.store(posNs, std::memory_order_relaxed);
		m_pendingFlushRate.store(rate, std::memory_order_relaxed);
		m_rate.store(rate, std::memory_order_relaxed);

		for (auto &source : m_sources)
		{
			if (source)
			{
				// Mirror the flushable-path semantics below: video/subtitle
				// are always cleared, audio keeps EOS during trickplay so
				// the pipeline clock is not stalled waiting for audio data
				// that will never arrive.
				const bool newEos = rate != AAMP_NORMAL_PLAY_RATE &&
				          ((source.get() == m_sources[eMEDIATYPE_AUDIO].get()) ||
						   (source.get() == m_sources[eMEDIATYPE_SUBTITLE].get()));
				source->invalidateGeneration(m_pipeline.get(),
					"Flush(non-flushable)", newEos);
			}
		}

		AAMPLOG_INFO("Flush() in non-flushable state %d (shouldTearDown=false): "
			"parameters staged, rate=%d committed, no flush cycle started",
			static_cast<int>(preFlushState), rate);
		AAMPLOG_INFO("EXIT");
		return;
	}

	// Step 3: Claim FLUSHING atomically while holding m_flushMutex so that
	// any concurrent WaitForFlushToComplete() caller (e.g. Configure()) sees
	// FLUSHING and blocks until onFlushComplete() signals completion.
	// Flush() and Stop()/Configure() are always on the AAMP control thread,
	// so no TOCTOU risk exists between step 2 and step 3.
	{
		std::lock_guard<std::mutex> lock(m_flushMutex);
		m_stateMachine.onFlush();
	}

	// Stage flush parameters now that FLUSHING is claimed.
	const int64_t posNs = static_cast<int64_t>(position * kNsPerSecond);
	m_pendingPositionNs.store(posNs, std::memory_order_relaxed);
	m_pendingFlushRate.store(rate, std::memory_order_relaxed);

	// Wake any in-flight data so it abandons the current batch.
	for (auto &source : m_sources)
	{
		if (source)
		{
			// During trickplay (rate != 1), keep audio EOS'd so the
			// Rialto/GStreamer pipeline clock is not stalled waiting
			// for audio data that will never arrive.
			const bool newEos = rate != AAMP_NORMAL_PLAY_RATE &&
					          ((source.get() == m_sources[eMEDIATYPE_AUDIO].get()) ||
							   (source.get() == m_sources[eMEDIATYPE_SUBTITLE].get()));
			source->invalidateGeneration(m_pipeline.get(), "Flush", newEos);
		}
	}

	if (m_pipeline)
	{
		// Perform a pipeline-level flushing seek.  Rialto will emit
		// PlaybackState::SEEK_DONE when the seek completes; OnPlaybackState()
		// handles rate commit, per-source segment position (for trickplay
		// applied_rate), state machine restoration, and flush-CV signalling.
		AAMPLOG_INFO("Issuing pipeline setPosition posNs=%" PRId64, posNs);
		if (!m_pipeline->setPosition(posNs))
		{
			AAMPLOG_WARN("setPosition failed for posNs=%" PRId64
				" - committing rate immediately and exiting FLUSHING", posNs);
			m_rate.store(
				m_pendingFlushRate.load(std::memory_order_relaxed),
				std::memory_order_relaxed);
			m_stateMachine.onFlushComplete();
			m_flushCv.notify_all();
		}
	}
	else
	{
		// With no pipeline there can be no SEEK_DONE callback, so commit
		// the pending rate immediately and exit FLUSHING.
		m_rate.store(
			m_pendingFlushRate.load(std::memory_order_relaxed),
			std::memory_order_relaxed);
		AAMPLOG_INFO("No pipeline during flush - committed playback rate=%d",
			m_rate.load(std::memory_order_relaxed));
		m_stateMachine.onFlushComplete();
		m_flushCv.notify_all();
	}

	// m_firstPtsMs is reset automatically on each source by
	// invalidateGeneration() (called above), so the next injection into
	// the video source establishes the fresh segment-start baseline.

	AAMPLOG_INFO("EXIT");
}

void AampRialtoPlayer::FlushTrack(AampMediaType mediaType, double position)
{
	AAMPLOG_INFO("ENTRY mediaType=%d position=%f", static_cast<int>(mediaType), position);
	AAMPLOG_INFO("EXIT");
}

bool AampRialtoPlayer::SetPlayBackRate(double rate)
{
	AAMPLOG_INFO("ENTRY rate=%f", rate);
	bool result = false;
	if (!m_pipeline)
	{
		AAMPLOG_WARN("pipeline is null");
	}
	else
	{
		result = m_pipeline->setPlaybackRate(rate);
	}
	AAMPLOG_INFO("EXIT result=%d", result);
	return result;
}

bool AampRialtoPlayer::Pause(bool pause, bool forceStopGstreamerPreBuffering)
{
	AAMPLOG_INFO("ENTRY pause=%d forceStopGstreamerPreBuffering=%d", pause, forceStopGstreamerPreBuffering);
	bool result = false;
	if (!m_pipeline)
	{
		AAMPLOG_WARN("pipeline is null");
	}
	else
	{
		if (pause)
		{
			result = m_pipeline->pause();
		}
		else
		{
			UngateAllSources("Pause(false)");
			bool async = false;
			result = m_pipeline->play(async);
		}
	}
	AAMPLOG_INFO("EXIT result=%d", result);
	return result;
}

long AampRialtoPlayer::GetDurationMilliseconds()
{
	AAMPLOG_INFO("ENTRY");
	long result = static_cast<long>(m_durationMs.load(std::memory_order_relaxed));
	AAMPLOG_INFO("EXIT result=%ld", result);
	return result;
}

long long AampRialtoPlayer::GetPositionMilliseconds()
{
	AAMPLOG_INFO("ENTRY");

	// Segment-start: PTS (ms) of the first video sample injected since the
	// last Configure/Flush.  Mirrors GStreamer's segmentStart, which is
	// derived from the segment event pushed before the first buffer.
	// kFirstPtsNotSet (-1) means no sample has arrived yet -> return 0.
	const auto *videoSource = m_sources[eMEDIATYPE_VIDEO].get();
	const int64_t startMs = videoSource
		? videoSource->firstPtsMs()
		: AampRialtoMediaSource::kFirstPtsNotSet;

	// Initialise rawMs to startMs: if the pipeline is unavailable or its
	// query fails, the subtraction (rawMs - startMs) correctly yields 0
	// rather than surfacing a potentially stale cached value.
	int64_t rawMs = startMs;
	long long result = 0LL;

	if (m_pipeline)
	{
		constexpr int64_t kNsPerMs = 1'000'000LL;
		int64_t queriedNs = 0;
		if (m_pipeline->getPosition(queriedNs))
		{
			rawMs = queriedNs / kNsPerMs;
			if (startMs >= 0)
			{
				const int rate = m_rate.load(std::memory_order_relaxed);
				const int64_t elapsed = rawMs - startMs;
				// For forward play (rate > 0) clamp to zero to avoid a
				// negative blip caused by clock jitter at startup.  For
				// reverse trickplay (rate < 0) allow negative so the caller
				// observes a decrementing position - mirroring GStreamer's
				//   rc = (pos - segmentStart) * rate.
				result = (rate > 0)
					? std::max(int64_t{0}, elapsed) * rate
					: elapsed * rate;
			}
			AAMPLOG_INFO("queried=%" PRId64 " ms  segmentStart=%" PRId64
				" ms  rate=%d  position=%lld ms", rawMs, startMs,
				m_rate.load(std::memory_order_relaxed), result);
		}
		else
		{
			AAMPLOG_WARN("getPosition() failed");
		}
	}
	else
	{
		AAMPLOG_WARN("pipeline is null");
	}

	AAMPLOG_INFO("EXIT result=%lld", result);
	return result;
}

long long AampRialtoPlayer::GetVideoPTS()
{
	AAMPLOG_INFO("ENTRY");
	AAMPLOG_INFO("EXIT");
	return 0;
}

void AampRialtoPlayer::SetVideoRectangle(int x, int y, int w, int h)
{
	AAMPLOG_INFO("ENTRY x=%d y=%d w=%d h=%d", x, y, w, h);
	m_videoRectangle = std::to_string(x) + "," + std::to_string(y)
	                 + "," + std::to_string(w) + "," + std::to_string(h);
	if (m_pipeline)
	{
		m_pipeline->setVideoWindow(
			static_cast<uint32_t>(x), static_cast<uint32_t>(y),
			static_cast<uint32_t>(w), static_cast<uint32_t>(h));
	}
	AAMPLOG_INFO("EXIT");
}

void AampRialtoPlayer::SetVideoZoom(VideoZoomMode zoom)
{
	AAMPLOG_INFO("ENTRY zoom=%d", static_cast<int>(zoom));
	AAMPLOG_INFO("EXIT");
}

void AampRialtoPlayer::SetVideoMute(bool muted)
{
	AAMPLOG_INFO("ENTRY muted=%d", muted);
	AAMPLOG_INFO("EXIT");
}

void AampRialtoPlayer::SetSubtitleMute(bool muted)
{
	AAMPLOG_INFO("ENTRY muted=%d", muted);
	std::lock_guard<std::mutex> lock(m_attachMutex);
	m_subtitleMuted = muted;
	auto *subtitleSource = m_sources[eMEDIATYPE_SUBTITLE].get();
	if (m_pipeline && subtitleSource && subtitleSource->isAttached())
	{
		m_pipeline->setMute(subtitleSource->sourceId(), muted);
	}
	AAMPLOG_INFO("EXIT");
}

// ---------------------------------------------------------------------------
// IDirectRialtoCC
// ---------------------------------------------------------------------------

bool AampRialtoPlayer::setTextTrackIdentifier(const std::string &id)
{
	AAMPLOG_INFO("ENTRY id=%s", id.c_str());
	bool result = false;
	if (m_pipeline)
	{
		result = m_pipeline->setTextTrackIdentifier(id);
	}
	AAMPLOG_INFO("EXIT result=%d", result);
	return result;
}

bool AampRialtoPlayer::setCCMute(bool muted)
{
	AAMPLOG_INFO("ENTRY muted=%d", muted);
	bool result = false;
	std::lock_guard<std::mutex> lock(m_attachMutex);
	auto *sub = m_sources[eMEDIATYPE_SUBTITLE].get();
	if (m_pipeline && sub && sub->isAttached())
	{
		result = m_pipeline->setMute(sub->sourceId(), muted);
	}
	AAMPLOG_INFO("EXIT result=%d", result);
	return result;
}

void AampRialtoPlayer::SetSubtitlePtsOffset(std::uint64_t pts_offset)
{
	AAMPLOG_INFO("ENTRY pts_offset=%" PRIu64, pts_offset);
	AAMPLOG_INFO("EXIT");
}

void AampRialtoPlayer::SetAudioVolume(int volume)
{
	AAMPLOG_INFO("ENTRY volume=%d", volume);
	AAMPLOG_INFO("EXIT");
}

bool AampRialtoPlayer::Discontinuity(AampMediaType mediaType)
{
	AAMPLOG_INFO("ENTRY mediaType=%d", static_cast<int>(mediaType));
	AAMPLOG_INFO("EXIT");
	return false;
}

bool AampRialtoPlayer::CheckForPTSChangeWithTimeout(long timeout)
{
	AAMPLOG_INFO("ENTRY timeout=%ld", timeout);
	AAMPLOG_INFO("EXIT");
	return false;
}

bool AampRialtoPlayer::IsCacheEmpty(AampMediaType mediaType)
{
	AAMPLOG_INFO("ENTRY mediaType=%d", static_cast<int>(mediaType));
	AAMPLOG_INFO("EXIT");
	return false;
}

void AampRialtoPlayer::ResetEOSSignalledFlag()
{
	AAMPLOG_INFO("ENTRY");
	AAMPLOG_INFO("EXIT");
}

void AampRialtoPlayer::NotifyFragmentCachingComplete()
{
	AAMPLOG_INFO("ENTRY");
	AAMPLOG_INFO("EXIT");
}

void AampRialtoPlayer::NotifyFragmentCachingOngoing()
{
	AAMPLOG_INFO("ENTRY");
	AAMPLOG_INFO("EXIT");
}

void AampRialtoPlayer::GetVideoSize(int &w, int &h)
{
	AAMPLOG_INFO("ENTRY");
	AAMPLOG_INFO("EXIT w=%d h=%d", w, h);
}

void AampRialtoPlayer::QueueProtectionEvent(
	const char *protSystemId,
	const void *ptr,
	size_t len,
	AampMediaType type)
{
	AAMPLOG_INFO("ENTRY protSystemId=%s len=%zu type=%d", protSystemId ? protSystemId : "(null)", len, static_cast<int>(type));
	if (!ptr || len == 0 || !protSystemId)
	{
		AAMPLOG_INFO("EXIT - no init data");
	}
	else
	{
		AampRialtoMediaSource::ProtectionParams prot;
		prot.systemId = protSystemId;
		prot.initData.assign(
			static_cast<const uint8_t *>(ptr),
			static_cast<const uint8_t *>(ptr) + len);
		prot.type = type;

		// Always store at the player level so protection survives
		// regardless of whether sources exist yet.
		auto idx = static_cast<size_t>(type);
		if (idx < kMaxSourceTypes)
		{
			m_pendingProtection[idx] = prot;
		}

		AAMPLOG_INFO("EXIT - params stored for type=%d", static_cast<int>(type));
	}
}

void AampRialtoPlayer::ClearProtectionEvent()
{
	AAMPLOG_INFO("ENTRY");
	for (auto &prot : m_pendingProtection)
	{
		prot.reset();
	}
	AAMPLOG_INFO("EXIT");
}

void AampRialtoPlayer::SignalTrickModeDiscontinuity()
{
	AAMPLOG_INFO("ENTRY");
	AAMPLOG_INFO("EXIT");
}

void AampRialtoPlayer::SeekStreamSink(double position, double rate)
{
	AAMPLOG_INFO("ENTRY position=%f rate=%f", position, rate);

	if (m_pipeline)
	{
		// Convert position from seconds to nanoseconds for Rialto API
		const int64_t positionNs = static_cast<int64_t>(position * 1'000'000'000LL);
		if (!m_pipeline->setPosition(positionNs))
		{
			AAMPLOG_WARN("setPosition failed for position=%.3f s (%" PRId64 " ns)",
			             position, positionNs);
		}
	}

	// Store rate for GetPositionMilliseconds() calculations
	m_rate.store(static_cast<int>(rate), std::memory_order_relaxed);

	AAMPLOG_INFO("EXIT");
}

std::string AampRialtoPlayer::GetVideoRectangle()
{
	AAMPLOG_INFO("ENTRY");
	AAMPLOG_INFO("EXIT");
	return m_videoRectangle;
}

void AampRialtoPlayer::StopBuffering(bool forceStop)
{
	// forceStop semantics (GStreamer reference: InterfacePlayerRDK::StopBuffering):
	//   true  - resume playback unconditionally, regardless of buffer level.
	//   false - resume only if enough decoded frames are queued in the decoder.
	//
	// The Rialto client API does not expose the server-side decoder's queued
	// frame count, so there is no condition to gate the non-forced path on.
	// Both cases therefore resume unconditionally via play().
	AAMPLOG_INFO("ENTRY forceStop=%d", forceStop);
	if (!m_pipeline)
	{
		AAMPLOG_WARN("pipeline is null");
	}
	else
	{
		UngateAllSources("StopBuffering");
		bool async = false;
		if (!m_pipeline->play(async))
		{
			AAMPLOG_ERR("play() failed while stopping buffering");
		}
	}
	AAMPLOG_INFO("EXIT");
}

bool AampRialtoPlayer::SetTextStyle(const std::string &options)
{
	AAMPLOG_INFO("ENTRY options=%s", options.c_str());
	AAMPLOG_INFO("EXIT");
	return false;
}

PlaybackQualityStruct *AampRialtoPlayer::GetVideoPlaybackQuality()
{
	AAMPLOG_INFO("ENTRY");
	AAMPLOG_INFO("EXIT");
	return nullptr;
}

bool AampRialtoPlayer::SignalSubtitleClock()
{
	AAMPLOG_INFO("ENTRY");
	bool result = false;
	auto *subtitleSource = m_sources[eMEDIATYPE_SUBTITLE].get();
	if (m_pipeline && subtitleSource && subtitleSource->isAttached())
	{
		int64_t position = 0;
		if (m_pipeline->getPosition(position))
		{
			result = m_pipeline->setSourcePosition(
				subtitleSource->sourceId(),
				position,
				/*resetTime=*/false);
		}
	}
	AAMPLOG_INFO("EXIT result=%d", result);
	return result;
}

void AampRialtoPlayer::SetPauseOnStartPlayback(bool enable)
{
	AAMPLOG_INFO("ENTRY enable=%d", enable);
	AAMPLOG_INFO("EXIT");
}

void AampRialtoPlayer::NotifyInjectorToResume()
{
	AAMPLOG_INFO("ENTRY");
	AAMPLOG_INFO("EXIT");
}

void AampRialtoPlayer::NotifyInjectorToPause()
{
	AAMPLOG_INFO("ENTRY");
	for (auto &source : m_sources)
	{
		if (source)
		{
			source->invalidateGeneration(m_pipeline.get(), "NotifyInjectorToPause");
		}
	}
	AAMPLOG_INFO("EXIT");
}

void AampRialtoPlayer::SetStreamCaps(AampMediaType type, MediaCodecInfo &&codecInfo)
{
	AAMPLOG_INFO("ENTRY type=%d codecFormat=%d codecDataLen=%zu",
		static_cast<int>(type),
		static_cast<int>(codecInfo.mCodecFormat),
		codecInfo.mCodecData.size());
	if (AampLogManager::isLogLevelAllowed(eLOGLEVEL_TRACE))
	{
		DumpBlob(codecInfo.mCodecData.data(), codecInfo.mCodecData.size());
	}

	std::lock_guard<std::mutex> lock(m_attachMutex);
	if (!m_pipeline)
	{
		AAMPLOG_ERR("pipeline not created");
	}
	else
	{
		auto *source = getSource(type);
		if (source)
		{
			AttachSource(*source, codecInfo);

			// SelectSubtitleTrack() calls StopTrackDownloads(SUBTITLE) during
			// period transitions, setting mbTrackDownloadsBlocked[SUBTITLE]=true.
			// InjectFragment() calls BlockUntilGstreamerWantsData() which spins
			// while that flag is true.  For Rialto, subtitle backpressure is
			// managed via injectionGated (not via NeedData/EnoughData), so
			// ResumeTrackDownloads is never triggered by a Rialto NeedData
			// callback after a caps update.  Clear the flag here so that the
			// inject loop is unblocked once the new period init has been accepted.
			if (type == eMEDIATYPE_SUBTITLE && m_aamp)
			{
				m_aamp->ResumeTrackDownloads(eMEDIATYPE_SUBTITLE);
			}
		}
		else
		{
			AAMPLOG_WARN("unsupported media type=%d", static_cast<int>(type));
		}
	}

	AAMPLOG_INFO("EXIT");
}

bool AampRialtoPlayer::IsAssociatedAamp(PrivateInstanceAAMP *aampInstance)
{
	bool ret = false;
	AAMPLOG_INFO("ENTRY aampInstance=%p", aampInstance);
	ret = (m_aamp == aampInstance);
	AAMPLOG_INFO("EXIT ret=%d", ret);
	return ret;
}

void AampRialtoPlayer::ChangeAamp(PrivateInstanceAAMP *newAamp, id3_callback_t id3HandlerCallback)
{
	AAMPLOG_INFO("ENTRY newAamp=%p", newAamp);
	m_ID3MetadataHandler = std::move(id3HandlerCallback);
	if (newAamp == nullptr)
	{
		AAMPLOG_WARN("newAamp is null in ChangeAamp; keeping current association");
	}
	else
	{
		m_aamp = newAamp;
		if (m_notifiableAdapter)
		{
			static_cast<PrivateInstanceAAMPNotifiable *>(
				m_notifiableAdapter.get())->ChangeAamp(newAamp);
		}
	}
	AAMPLOG_INFO("EXIT");
}

void AampRialtoPlayer::SetEncryptedAamp(PrivateInstanceAAMP *aamp)
{
	AAMPLOG_INFO("ENTRY aamp=%p", aamp);
	// Create DRM bridge with the encrypted player's aamp to share its DRM
	// session manager.  Supports pre-roll ads: clear ad player calls this
	// with the encrypted VOD player so the pipeline is configured for DRM
	// even though the first asset is unencrypted.
	m_drmBridge = std::make_shared<AampDrmBridge>(aamp);
	AAMPLOG_INFO("EXIT");
}

void AampRialtoPlayer::ResetFirstFrame()
{
	AAMPLOG_INFO("ENTRY");
	AAMPLOG_INFO("EXIT");
}

// ---------------------------------------------------------------------------
// Callbacks invoked from AampRialtoMediaPipelineClient
// ---------------------------------------------------------------------------

void AampRialtoPlayer::OnNeedMediaData(
	int32_t sourceId, size_t frameCount, uint32_t requestId)
{
	AAMPLOG_INFO("sourceId=%d frameCount=%zu requestId=%u", sourceId, frameCount, requestId);
	auto *source = findSourceByRialtoId(sourceId);
	if (source)
	{
		source->handleNeedData(frameCount, requestId, m_pipeline.get());
	}
	else
	{
		AAMPLOG_WARN("unknown sourceId=%d", sourceId);
	}
}

void AampRialtoPlayer::OnCancelNeedMediaData(int32_t sourceId)
{
	AAMPLOG_INFO("sourceId=%d", sourceId);
	auto *source = findSourceByRialtoId(sourceId);
	if (source)
	{
		source->handleCancelNeedData();
	}
}

unsigned long AampRialtoPlayer::GetCCHandle() const
{
	auto *mediaSource = m_sources[eMEDIATYPE_SUBTITLE].get();
	// Build the CC decoder handle. In inband-CC mode pass the
	// IDirectRialtoCC pointer (as unsigned long) so that
	// priv_aamp::InitializeCC() initialises the CC manager with the
	// correct control interface.
	const unsigned long ccHandle = (mediaSource && mediaSource->isInbandCC())
		? reinterpret_cast<unsigned long>(
			static_cast<const IDirectRialtoCC *>(this))
		: 0UL;

	return ccHandle;
}

void AampRialtoPlayer::OnPlaybackState(firebolt::rialto::PlaybackState state)
{
	AAMPLOG_INFO("state=%d", static_cast<int>(state));

	switch (state)
	{
		case firebolt::rialto::PlaybackState::PLAYING:
		{
			m_stateMachine.onPlaybackStarted();
			// Edge-case race: if PLAYING arrives before onFlushComplete() fires
			// (a delayed ack for a play() that was in-flight when Flush() was
			// called), the state machine has just transitioned out of FLUSHING.
			// Wake any thread blocked in WaitForFlushToComplete() so it can
			// re-evaluate its predicate.
			m_flushCv.notify_all();

			// Clear injectionGated so inject threads resume blocking
			// normally on needData rather than aborting immediately.
			UngateAllSources("OnPlaybackState(PLAYING)");

			const bool firstFrame =
				!m_firstFrameNotified.exchange(true, std::memory_order_acq_rel);
			const unsigned long ccHandle = GetCCHandle();

			if (firstFrame)
			{
				m_notifiable->LogFirstFrame();
				m_notifiable->LogTuneComplete();
				m_notifiable->NotifyFirstBufferProcessed(GetVideoRectangle());
				m_notifiable->NotifyFirstFrameReceived(ccHandle);
				m_notifiable->NotifyFirstVideoFrameDisplayed();
			}
			else if (m_notifiable->GetState() == eSTATE_SEEKING)
			{
				m_notifiable->NotifyFirstBufferProcessed(GetVideoRectangle());
				m_notifiable->NotifyFirstFrameReceived(ccHandle);
				m_notifiable->NotifyFirstVideoFrameDisplayed();
			}
			else
			{
				m_notifiable->NotifyFirstBufferProcessed(GetVideoRectangle());
				m_notifiable->NotifySpeedChanged(
					static_cast<float>(m_rate.load(std::memory_order_relaxed)), // actual rate
					/*changeState=*/true);
			}
			StartProgressTimer();

			if (m_monitorAV)
			{
				m_monitorAV->start();
			}

			break;
		}
		case firebolt::rialto::PlaybackState::PAUSED:
			m_stateMachine.onPlaybackPaused();
			// Do NOT set paused=true on sources here.  PAUSED arrives for
			// buffering-pause (e.g. AampUnderflowMonitor) as well as for
			// user-initiated pause.  In both cases Rialto continues to
			// accept data (needData events keep arriving), so injection
			// threads should remain blocked on needData rather than
			// aborting and discarding the current batch of samples.
			// Injection is only aborted by invalidateGeneration() which is
			// called on flush / stop / seek - not on every pipeline pause.
			break;
		case firebolt::rialto::PlaybackState::END_OF_STREAM:
			m_notifiable->NotifyEOSReached();
			break;
		case firebolt::rialto::PlaybackState::FAILURE:
			m_stateMachine.onError();
			break;
		case firebolt::rialto::PlaybackState::SEEKING:
			AAMPLOG_INFO("SEEKING notification received (state=%s)",
				m_stateMachine.currentStateName());
			break;
		case firebolt::rialto::PlaybackState::SEEK_DONE:
		{
			// Ignore SEEK_DONE not originating from a Flush()-initiated seek.
			// setPosition() is also called by SeekStreamSink() without
			// entering FLUSHING; that path needs no flush-completion work.
			if (m_stateMachine.currentState() != PlayerStateId::FLUSHING)
			{
				AAMPLOG_INFO("SEEK_DONE received outside FLUSHING (state=%s) - ignored",
					m_stateMachine.currentStateName());
				break;
			}

			const int64_t posNs =
				m_pendingPositionNs.load(std::memory_order_relaxed);
			const int pendingRate =
				m_pendingFlushRate.load(std::memory_order_relaxed);

			// Apply video-source segment position so the GStreamer
			// segment event carries the correct applied_rate for trickplay.
			// resetTime=false: the pipeline-level setPosition() already
			// flushed source buffers; we only update the segment rate.
			auto appliedRate = computeAppliedRate(pendingRate);
			if (appliedRate != AAMP_NORMAL_PLAY_RATE)
			{
				auto *videoSource = m_sources[eMEDIATYPE_VIDEO].get();
				if (!m_pipeline->setSourcePosition(
							videoSource->sourceId(), posNs,
							/*resetTime=*/false,
							computeAppliedRate(pendingRate)))
				{
					AAMPLOG_WARN("setSourcePosition failed for "
						"sourceId=%d after SEEK_DONE",
						videoSource->sourceId());
				}
			}

			m_rate.store(pendingRate, std::memory_order_relaxed);
			AAMPLOG_INFO("SEEK_DONE: committed playback rate=%d", pendingRate);

			// Restore the pre-flush state before notifying
			// WaitForFlushToComplete().
			m_stateMachine.onFlushComplete();
			m_flushCv.notify_all();

			// Ungate if Stream() was called while the flush was in progress,
			// regardless of what onFlushComplete() just restored.  For
			// seek-while-playing, the pre-flush (and thus restored) state is
			// already PLAYING, so this is the only guaranteed point that
			// ungates sources — waiting for a subsequent "redundant" Rialto
			// PLAYING notification is an assumption, not a guarantee, and
			// must not be relied on to avoid leaving sources gated forever.
			// seek-while-paused is handled correctly: Stream() is not called
			// in that path so m_playRequested stays false and the gate is
			// left for the later Pause(false)/StopBuffering() to clear.
			const bool playRequested =
				m_playRequested.load(std::memory_order_seq_cst);
			m_playRequested.store(false, std::memory_order_relaxed);

			if (playRequested)
			{
				UngateAllSources("OnPlaybackState(SEEK_DONE)");
				AAMPLOG_INFO("SEEK_DONE: issuing play() (state=%s)",
					m_stateMachine.currentStateName());
				bool async = false;
				if (!m_pipeline->play(async))
				{
					AAMPLOG_ERR("play() failed after SEEK_DONE");
				}
			}
			else
			{
				AAMPLOG_INFO("SEEK_DONE: not issuing play() (state=%s)",
					m_stateMachine.currentStateName());
			}

			break;
		}
		default:
			break;
	}
}

void AampRialtoPlayer::OnPosition(int64_t positionNs)
{
	// Not used
}

void AampRialtoPlayer::OnDuration(int64_t durationNs)
{
	constexpr int64_t kNsPerMs = 1'000'000LL;
	m_durationMs.store(durationNs / kNsPerMs, std::memory_order_relaxed);
	AAMPLOG_INFO("duration updated: %" PRId64 " ms", m_durationMs.load());
}

void AampRialtoPlayer::OnBufferUnderflow(int32_t sourceId)
{
	AAMPLOG_INFO("sourceId=%d", sourceId);
	auto *source = findSourceByRialtoId(sourceId);
	if (!source)
	{
		AAMPLOG_WARN("unknown sourceId=%d - ignoring underflow notification",
			sourceId);
		return;
	}
	m_notifiable->NotifyBufferUnderflow(source->mediaType());
}

double AampRialtoPlayer::computeAppliedRate(int candidateRate) const
{
	if (m_pipelineCapabilities)
	{
		bool videoMaster = false;
		if (m_pipelineCapabilities->isVideoMaster(videoMaster) && !videoMaster)
		{
			return static_cast<double>(candidateRate);
		}
	}
	return 1.0;
}

void AampRialtoPlayer::OnSourceFlushed(int32_t sourceId)
{
	// Flush() now uses pipeline-level setPosition(); flush completion is
	// driven by PlaybackState::SEEK_DONE, not per-source SourceFlushedEvents.
	// This callback should not be reached.
	AAMPLOG_WARN("OnSourceFlushed called unexpectedly for sourceId=%d "
		"- flush is driven by setPosition/SEEK_DONE", sourceId);
}

void AampRialtoPlayer::StartProgressTimer()
{
	if (!m_progressTimer)
	{
		m_progressTimer = std::make_unique<ProgressTimer>();
	}

	if (m_progressTimer->isRunning())
	{
		AAMPLOG_INFO("Progress timer already running - kicking for immediate dispatch");
		m_progressTimer->kick();
		return;
	}

	double intervalSeconds = 0.0;
	if (m_notifiable == nullptr)
	{
		AAMPLOG_WARN("notifiable is null, progress timer not started");
		return;
	}

	intervalSeconds = m_notifiable->GetProgressReportIntervalSeconds();

	unsigned int intervalMs = 0;
	if (intervalSeconds > 0.0)
	{
		intervalMs = static_cast<unsigned int>(intervalSeconds * kMsPerSecond);
	}

	if (intervalMs == 0)
	{
		AAMPLOG_WARN("Invalid progress interval=%f seconds; timer disabled",
			intervalSeconds);
		return;
	}

	AAMPLOG_INFO("Starting progress timer %dms", intervalMs);
	m_progressTimer->start(intervalMs, [this]() {
		this->OnProgressTimerTick();
	});
}

void AampRialtoPlayer::StopProgressTimer()
{
	if (m_progressTimer)
	{
		AAMPLOG_INFO("Stopping progress timer");
		m_progressTimer->stop();
	}
}

void AampRialtoPlayer::OnProgressTimerTick()
{
	if (m_notifiable == nullptr)
	{
		AAMPLOG_WARN("notifiable is null on progress timer tick");
	}
	else
	{
		m_notifiable->MonitorProgress(/*sync=*/false,
			/*beginningOfStream=*/false);
	}
}
