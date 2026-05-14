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
 * @brief Implementation of AampRialtoPlayer — all StreamSink calls are
 *        forwarded to the internally owned AAMPGstPlayer instance.
 */

#include "AampRialtoPlayer.h"
#include "AampRialtoMediaPipelineClient.h"
#include "AampRialtoMediaSource.h"
#include "AampDrmBridge.h"
#include "AampLogManager.h"
#include "PrivateInstanceAAMPNotifiable.h"
#include "priv_aamp.h"
#include "mp4demux/MP4Demux.h"
#include "IControl.h"
#include "AampRialtoControlBackend.h"
#include <chrono>
#include <cinttypes>
#include <algorithm>

// ---------------------------------------------------------------------------
// Rialto → AAMP log bridge
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

namespace {
	/// Upper bound for the wait on Rialto's application state transitioning
	/// to RUNNING.
	constexpr int kRialtoRunningTimeoutMs = 2000;
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
	, m_drmBridge(std::make_shared<AampDrmBridge>(aamp))
	, m_controlBackend(std::move(controlBackend))
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
	for (auto &source : m_sources)
	{
		if (source)
		{
			source->invalidateGeneration();
		}
	}
	AAMPLOG_INFO("AampRialtoPlayer: destroyed");
}

// ---------------------------------------------------------------------------
// StreamSink overrides
// ---------------------------------------------------------------------------

void AampRialtoPlayer::Configure(
	StreamOutputFormat videoFormat,
	StreamOutputFormat audioFormat,
	StreamOutputFormat subFormat,
	bool bESChangeStatus,
	bool setReadyAfterPipelineCreation)
{
	AAMPLOG_INFO("ENTRY videoFormat=%d audioFormat=%d subFormat=%d bESChangeStatus=%d setReadyAfterPipelineCreation=%d", static_cast<int>(videoFormat), static_cast<int>(audioFormat),
		static_cast<int>(subFormat), bESChangeStatus, setReadyAfterPipelineCreation);

	m_stateMachine.onReconfigure();
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

	// NOTE: Protection params are intentionally NOT reset here.
	// NOTE: m_pendingFlushPositionNs is intentionally NOT reset here.
	m_playRequested.store(false, std::memory_order_relaxed);
	m_allSourcesAttachedFlag.store(false, std::memory_order_relaxed);
	for (auto &pa : m_pendingAttach)
	{
		pa.reset();
	}

	// Register Rialto → AAMP log bridge once.
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
			AAMPLOG_WARN("Failed to create IClientLogControlFactory — Rialto logs suppressed");
		}
	}

	if (!m_pipelineFactory)
	{
		m_pipelineFactory = firebolt::rialto::IMediaPipelineFactory::createFactory();
	}
	auto &factory = m_pipelineFactory;
	if (!factory)
	{
		AAMPLOG_ERR("Failed to create IMediaPipelineFactory — is the Rialto server running?");
	}
	else
	{
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
			AAMPLOG_ERR("createMediaPipeline returned nullptr — check Rialto server logs (syslog) for details");
		}
		else
		{
			AAMPLOG_INFO("Created pipeline %p", m_pipeline.get());

			if (!m_pipeline->load(
					firebolt::rialto::MediaType::MSE,
					"video/mp4",
					/*url=*/""))
			{
				AAMPLOG_ERR("load() failed — Rialto will reject attachSource calls");
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

				m_stateMachine.onPipelineLoaded();
			}
		}
	}

	// Create per-source objects and demuxers based on configured formats.
	// FORMAT_ISO_BMFF: AampRialtoPlayer owns demuxing (SendSample path).
	// FORMAT_UNKNOWN:  streamabstraction demuxes externally (SetStreamCaps path).
	if (videoFormat != FORMAT_INVALID)
	{
		auto src = m_sourceCreator(eMEDIATYPE_VIDEO);
		if (src)
		{
			if (videoFormat == FORMAT_ISO_BMFF)
			{
				src->setDemuxer(std::make_unique<Mp4Demux>());
			}
			// Apply any protection queued before this source existed.
			if (m_pendingProtection[eMEDIATYPE_VIDEO].has_value())
			{
				src->setProtection(*m_pendingProtection[eMEDIATYPE_VIDEO]);
			}
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
			if (audioFormat == FORMAT_ISO_BMFF)
			{
				src->setDemuxer(std::make_unique<Mp4Demux>());
			}
			// Apply any protection queued before this source existed.
			if (m_pendingProtection[eMEDIATYPE_AUDIO].has_value())
			{
				src->setProtection(*m_pendingProtection[eMEDIATYPE_AUDIO]);
			}
			m_sources[eMEDIATYPE_AUDIO] = std::move(src);
			m_aamp->ResumeTrackDownloads(eMEDIATYPE_AUDIO);
			AAMPLOG_INFO("Created audio source (format=%d)", static_cast<int>(audioFormat));
		}
	}
	// Subtitle source creation is disabled until AampRialtoSubtitleSource
	// fully implements mapCodecToMime/createRialtoSource.  Until then,
	// creating a source here blocks allSourcesAttached() because the
	// subtitle can never be attached to the Rialto pipeline.
	if (false && subFormat != FORMAT_INVALID)
	{
		auto src = m_sourceCreator(eMEDIATYPE_SUBTITLE);
		if (src)
		{
			if (subFormat == FORMAT_ISO_BMFF)
			{
				src->setDemuxer(std::make_unique<Mp4Demux>());
			}
			m_sources[eMEDIATYPE_SUBTITLE] = std::move(src);
			m_aamp->ResumeTrackDownloads(eMEDIATYPE_SUBTITLE);
			AAMPLOG_INFO("Created subtitle source (format=%d)", static_cast<int>(subFormat));
		}
	}

	AAMPLOG_INFO("EXIT");
}

bool AampRialtoPlayer::SendCopy(
	AampMediaType mediaType,
	std::vector<uint8_t> &&buffer,
	double fpts,
	double fdts,
	double fDuration)
{
	AAMPLOG_INFO("ENTRY mediaType=%d bufferSize=%zu fpts=%f fdts=%f fDuration=%f", static_cast<int>(mediaType), buffer.size(), fpts, fdts, fDuration);
	AAMPLOG_INFO("EXIT");
	return false;
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
		AAMPLOG_INFO("No source for mediaType=%d — ignoring transfer",
			static_cast<int>(mediaType));
		return true;
	}
	Mp4Demux *demuxer = source->demuxer();

	bool result = true;
	if (!demuxer || buffer.empty())
	{
		if (!demuxer)
		{
			AAMPLOG_WARN("No demuxer for mediaType=%d", static_cast<int>(mediaType));
		}
	}
	else if (!demuxer->Parse(std::make_shared<std::vector<uint8_t>>(std::move(buffer))))
	{
		AAMPLOG_ERR("Mp4Demux::Parse failed mediaType=%d err=%d", static_cast<int>(mediaType),
			static_cast<int>(demuxer->GetLastError()));
		result = false;
	}
	else if (initFragment)
	{
		std::lock_guard<std::mutex> lock(m_attachMutex);
		if (m_pipeline)
		{
			MediaCodecInfo codecInfo = demuxer->GetCodecInfo();
			AttachSource(*source, codecInfo);
		}
		else
		{
			AAMPLOG_ERR("pipeline not created");
		}
	}
	else
	{
		// Non-init fragment: extract samples and inject one at a time.
		auto samples = demuxer->GetSamples();
		if (!samples.empty() && source->isAttached() && m_pipeline)
		{
			uint64_t capturedGen = source->captureGeneration();
			auto pendingCodecData = source->takePendingCodecData();

			bool firstSample = true;
			for (auto &s : samples)
			{
				std::shared_ptr<firebolt::rialto::CodecData> codecData;
				if (firstSample)
				{
					codecData = pendingCodecData;
				}
				firstSample = false;

				if (!source->injectOneSample(
						*m_pipeline, capturedGen,
						std::move(s), codecData))
				{
					AAMPLOG_INFO(
						"SendTransfer aborted mid-batch mediaType=%d",
						static_cast<int>(mediaType));
					break;
				}
			}
			AAMPLOG_INFO("Processed %zu samples for mediaType=%d",
				samples.size(), static_cast<int>(mediaType));
		}
	}

	AAMPLOG_INFO("EXIT");
	return result;
}

// ---------------------------------------------------------------------------
// AttachSource — unified attach via polymorphic source
// ---------------------------------------------------------------------------

void AampRialtoPlayer::AttachSource(
	AampRialtoMediaSource &source, MediaCodecInfo &codecInfo)
{
	const auto type = source.mediaType();

	// THEORY (unproven — revert this block if disproved):
	// In the failing first-tune log, audio attached first (id=1) and video
	// second (id=2); the Rialto server then reported:
	//   "audsrc: not-linked (-1)"
	// and transitioned SOURCES_ATTACHED → ERROR.  In the passing second-tune
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
		m_pendingAttach[type] = std::move(codecInfo);
		return;
	}

	if (!source.isAttached())
	{
		m_stateMachine.onSourceAttaching();
	}

	auto result = source.attachOrUpdate(
		*m_pipeline, codecInfo, m_drmBridge.get(),
		m_pendingFlushPositionNs.load(std::memory_order_relaxed));

	if (result == AampRialtoMediaSource::AttachResult::NEWLY_ATTACHED ||
	    result == AampRialtoMediaSource::AttachResult::UPDATED)
	{
		// Clear the paused flag that Flush() sets.  Flush's purpose is to
		// abort in-flight injection threads from the previous generation;
		// once the source is (re-)attached the injection path must be
		// allowed to block normally waiting for needData rather than
		// immediately returning false.
		{
			auto &st = source.state();
			std::lock_guard<std::mutex> lock(st.mu);
			st.paused = false;
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
		CheckAllSourcesAttached();
	}
}

void AampRialtoPlayer::CheckAllSourcesAttached()
{
	if (!m_pipeline)
	{
		return;
	}

	if (m_allSourcesAttachedFlag.load(std::memory_order_relaxed))
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

	AAMPLOG_INFO("All sources attached — calling allSourcesAttached()");

	if (!m_pipeline->allSourcesAttached())
	{
		AAMPLOG_ERR("allSourcesAttached() failed");
	}
	else
	{
		m_stateMachine.onAllSourcesAttached();
		m_allSourcesAttachedFlag.store(true, std::memory_order_seq_cst);

		if (m_playRequested.load(std::memory_order_seq_cst))
		{
			AAMPLOG_INFO("play() deferred by Stream() — issuing now");
			bool async = false;
			if (!m_pipeline->play(async))
			{
				AAMPLOG_ERR("play() failed");
			}
		}
	}
}

bool AampRialtoPlayer::SendSample(AampMediaType mediaType, AampMediaSample &&sample)
{
	AAMPLOG_INFO("ENTRY mediaType=%d pts=%f dur=%f",
		static_cast<int>(mediaType), sample.mPts, sample.mDuration);

	bool result = false;

	auto *source = getSource(mediaType);
	if (source && source->isAttached() && m_pipeline)
	{
		uint64_t capturedGen = source->captureGeneration();
		auto pendingCodecData = source->takePendingCodecData();

		result = source->injectOneSample(
			*m_pipeline, capturedGen,
			std::move(sample), pendingCodecData);
	}
	else
	{
		if (!source)
		{
			AAMPLOG_WARN("unsupported mediaType=%d",
				static_cast<int>(mediaType));
		}
		else
		{
			AAMPLOG_WARN("source not attached for mediaType=%d",
				static_cast<int>(mediaType));
		}
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
	AAMPLOG_INFO("EXIT");
}

void AampRialtoPlayer::Stream()
{
	AAMPLOG_INFO("ENTRY");
	if (m_pipeline)
	{
		m_playRequested.store(true, std::memory_order_seq_cst);

		if (m_allSourcesAttachedFlag.load(std::memory_order_seq_cst))
		{
			bool async = false;
			if (!m_pipeline->play(async))
			{
				AAMPLOG_ERR("play() failed");
			}
		}
		else
		{
			AAMPLOG_INFO("deferring play() until allSourcesAttached()");
		}
	}
	AAMPLOG_INFO("EXIT");
}

void AampRialtoPlayer::Stop(bool keepLastFrame)
{
	AAMPLOG_INFO("ENTRY keepLastFrame=%d", keepLastFrame);
	for (auto &source : m_sources)
	{
		if (source)
		{
			source->invalidateGeneration();
			auto &st = source->state();
			std::lock_guard<std::mutex> lock(st.mu);
			st.eos = false;
		}
	}

	if (m_pipeline)
	{
		m_pipeline->stop();
	}
	m_stateMachine.onStop();
	AAMPLOG_INFO("EXIT");
}

void AampRialtoPlayer::Flush(double position, int rate, bool shouldTearDown)
{
	AAMPLOG_INFO("ENTRY position=%f rate=%d shouldTearDown=%d", position, rate, shouldTearDown);

	for (auto &source : m_sources)
	{
		if (source)
		{
			source->invalidateGeneration();
			auto &st = source->state();
			std::lock_guard<std::mutex> lock(st.mu);
			st.eos = false;
		}
	}

	const int64_t posNs = static_cast<int64_t>(position * kNsPerSecond);
	m_pendingFlushPositionNs.store(posNs, std::memory_order_relaxed);

	if (m_pipeline)
	{
		for (auto &source : m_sources)
		{
			if (source && source->isAttached())
			{
				source->flushSource(*m_pipeline, posNs);
			}
		}
	}

	m_stateMachine.onFlush();

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
	long long result = m_positionMs.load(std::memory_order_relaxed);
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
	AAMPLOG_INFO("EXIT");
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
		AAMPLOG_INFO("EXIT — no init data");
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

		// Also apply to the source immediately if one exists, so that
		// late-arriving protection (after Configure) takes effect.
		auto *source = getSource(type);
		if (source)
		{
			source->setProtection(std::move(prot));
		}
		else
		{
			AAMPLOG_INFO("No source yet for type=%d — protection buffered at player level",
				static_cast<int>(type));
		}

		AAMPLOG_INFO("EXIT — params stored for type=%d", static_cast<int>(type));
	}
}

void AampRialtoPlayer::ClearProtectionEvent()
{
	AAMPLOG_INFO("ENTRY");
	if (m_drmBridge)
	{
		m_drmBridge->clearSessions();
	}
	for (auto &prot : m_pendingProtection)
	{
		prot.reset();
	}
	for (auto &source : m_sources)
	{
		if (source)
		{
			source->clearProtection();
		}
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
	//   true  — resume playback unconditionally, regardless of buffer level.
	//   false — resume only if enough decoded frames are queued in the decoder.
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
	AAMPLOG_INFO("EXIT");
	return false;
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
			source->invalidateGeneration();
		}
	}
	AAMPLOG_INFO("EXIT");
}

void AampRialtoPlayer::SetStreamCaps(AampMediaType type, MediaCodecInfo &&codecInfo)
{
	AAMPLOG_INFO("ENTRY type=%d codecFormat=%d", static_cast<int>(type),
		static_cast<int>(codecInfo.mCodecFormat));

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
	AAMPLOG_INFO("ENTRY aampInstance=%p", aampInstance);
	AAMPLOG_INFO("EXIT");
	return false;
}

void AampRialtoPlayer::ChangeAamp(PrivateInstanceAAMP *newAamp, id3_callback_t id3HandlerCallback)
{
	AAMPLOG_INFO("ENTRY newAamp=%p", newAamp);
	AAMPLOG_INFO("EXIT");
}

void AampRialtoPlayer::SetEncryptedAamp(PrivateInstanceAAMP *aamp)
{
	AAMPLOG_INFO("ENTRY aamp=%p", aamp);
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

void AampRialtoPlayer::OnPlaybackState(firebolt::rialto::PlaybackState state)
{
	AAMPLOG_INFO("state=%d", static_cast<int>(state));

	switch (state)
	{
		case firebolt::rialto::PlaybackState::PLAYING:
		{
			m_stateMachine.onPlaybackStarted();

			const bool firstFrame =
				!m_firstFrameNotified.exchange(true, std::memory_order_acq_rel);

			if (firstFrame)
			{
				m_notifiable->LogFirstFrame();
				m_notifiable->LogTuneComplete();
				m_notifiable->NotifyFirstBufferProcessed(GetVideoRectangle());
				m_notifiable->NotifyFirstFrameReceived(/*ccDecoderHandle=*/0);
			}
			else if (m_notifiable->GetState() == eSTATE_SEEKING)
			{
				m_notifiable->NotifyFirstBufferProcessed(GetVideoRectangle());
				m_notifiable->NotifyFirstFrameReceived(/*ccDecoderHandle=*/0);
			}
			else
			{
				m_notifiable->NotifyFirstBufferProcessed(GetVideoRectangle());
				m_notifiable->NotifySpeedChanged(
					AAMP_NORMAL_PLAY_RATE, /*changeState=*/true);
			}
			break;
		}
		case firebolt::rialto::PlaybackState::PAUSED:
			m_stateMachine.onPlaybackPaused();
			break;
		case firebolt::rialto::PlaybackState::END_OF_STREAM:
			m_notifiable->NotifyEOSReached();
			break;
		case firebolt::rialto::PlaybackState::FAILURE:
			m_stateMachine.onError();
			break;
		default:
			break;
	}
}

void AampRialtoPlayer::OnPosition(int64_t positionNs)
{
	constexpr int64_t kNsPerMs = 1'000'000LL;
	m_positionMs.store(positionNs / kNsPerMs, std::memory_order_relaxed);
	m_notifiable->MonitorProgress();
}

void AampRialtoPlayer::OnDuration(int64_t durationNs)
{
	constexpr int64_t kNsPerMs = 1'000'000LL;
	m_durationMs.store(durationNs / kNsPerMs, std::memory_order_relaxed);
	AAMPLOG_INFO("duration updated: %" PRId64 " ms", m_durationMs.load());
}
