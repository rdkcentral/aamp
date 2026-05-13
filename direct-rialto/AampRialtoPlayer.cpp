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
	constexpr int64_t kNsPerSecond = 1'000'000'000LL;
	/// Upper bound for the wait on Rialto's application state transitioning
	/// to RUNNING.  In practice the transition completes in a few milliseconds;
	/// the timeout exists only to avoid a permanent hang if the Rialto server
	/// never reports RUNNING.
	constexpr int kRialtoRunningTimeoutMs = 2000;
}

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

AampRialtoPlayer::AampRialtoPlayer(
	PrivateInstanceAAMP *aamp,
	id3_callback_t id3HandlerCallback,
	std::function<void(const unsigned char *, int, int, int)> exportFrames)
	: AampRialtoPlayer(
		aamp,
		/*notifiable=*/nullptr,
		std::make_unique<AampRialtoControlBackend>(),
		id3HandlerCallback,
		std::move(exportFrames))
{
}

AampRialtoPlayer::AampRialtoPlayer(
	PrivateInstanceAAMP *aamp,
	IStreamSinkNotifiable *notifiable,
	std::unique_ptr<IRialtoControlBackend> controlBackend,
	id3_callback_t id3HandlerCallback,
	std::function<void(const unsigned char *, int, int, int)> exportFrames)
	: m_aamp(aamp)
	, m_drmBridge(std::make_shared<AampDrmBridge>(aamp))
	, m_controlBackend(std::move(controlBackend))
	, m_client(nullptr)
	, m_pipeline(nullptr)
{
	// Build the production adapter when no test-injected notifiable is supplied.
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
	// Wake any in-flight SendTransfer so it abandons the current batch and
	// returns before the player members are torn down.
	{
		std::lock_guard<std::mutex> lock(m_videoSrc.mu);
		++m_videoSrc.generation;
		m_videoSrc.hasPending = false;
		m_videoSrc.cv.notify_all();
	}
	{
		std::lock_guard<std::mutex> lock(m_audioSrc.mu);
		++m_audioSrc.generation;
		m_audioSrc.hasPending = false;
		m_audioSrc.cv.notify_all();
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

	// Reset per-session state so a re-tune starts clean.
	m_videoSourceId = -1;
	m_audioSourceId = -1;
	m_videoMksId = -1;
	m_audioMksId = -1;

	// Reset per-source pacing state for a fresh session.  Bumping the
	// generation invalidates any in-flight SendTransfer batch that may
	// still be holding the previous session's state.
	{
		std::lock_guard<std::mutex> lock(m_videoSrc.mu);
		++m_videoSrc.generation;
		m_videoSrc.hasPending     = false;
		m_videoSrc.addedInPending = 0;
		m_videoSrc.eos            = false;
		m_videoSrc.cv.notify_all();
	}
	{
		std::lock_guard<std::mutex> lock(m_audioSrc.mu);
		++m_audioSrc.generation;
		m_audioSrc.hasPending     = false;
		m_audioSrc.addedInPending = 0;
		m_audioSrc.eos            = false;
		m_audioSrc.cv.notify_all();
	}
	// NOTE: m_videoProt / m_audioProt are intentionally NOT reset here.
	// QueueProtectionEvent() is called by AAMP before Configure() in the
	// normal playback flow; clearing the stored protection params here would
	// discard them before AttachVideoSource / AttachAudioSource can use them
	// to call createSession().  ClearProtectionEvent() handles teardown.
	// NOTE: m_pendingFlushPositionNs is intentionally NOT reset here.
	// Flush() may be called before Configure() to pre-stage the seek position;
	// clearing it here would discard that staged value before sources attach.
	m_playRequested.store(false, std::memory_order_relaxed);
	m_allSourcesAttachedFlag.store(false, std::memory_order_relaxed);

	// Register Rialto → AAMP log bridge once (idempotent: handler is kept alive
	// for the lifetime of this player instance).
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
		// Defensive workaround for a Rialto client-side race: ensure the
		// controller has published application state RUNNING *before*
		// createMediaPipeline().  Otherwise the proxy ctor's internal
		// registerClient() may capture UNKNOWN and stamp the MediaPipeline
		// with UNKNOWN after a concurrent RUNNING broadcast, causing every
		// subsequent NeedMediaData event to be silently dropped.
		if (!m_controlBackend || !m_controlBackend->waitForRunning(kRialtoRunningTimeoutMs))
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

			// load() MUST be called before any attachSource() — it is what
			// causes the Rialto server to create the underlying Gstreamer player.
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

				// Wire callbacks from pipeline client.
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

				// Advance state machine: pipeline is now created and loaded.
				m_stateMachine.onPipelineLoaded();
			}
		}
	}

	if (videoFormat != FORMAT_INVALID && videoFormat != FORMAT_UNKNOWN)
	{
		m_videoDemuxer = std::make_unique<Mp4Demux>();
		m_aamp->ResumeTrackDownloads(eMEDIATYPE_VIDEO);
		AAMPLOG_INFO("Created video Mp4Demux");
	}
	if (audioFormat != FORMAT_INVALID && audioFormat != FORMAT_UNKNOWN)
	{
		m_audioDemuxer = std::make_unique<Mp4Demux>();
		m_aamp->ResumeTrackDownloads(eMEDIATYPE_AUDIO);
		AAMPLOG_INFO("Created audio Mp4Demux");
	}
	if (subFormat != FORMAT_INVALID && subFormat != FORMAT_UNKNOWN)
	{
		m_subtitleDemuxer = std::make_unique<Mp4Demux>();
		m_aamp->ResumeTrackDownloads(eMEDIATYPE_SUBTITLE);
		AAMPLOG_INFO("Created subtitle Mp4Demux");
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

	Mp4Demux *demuxer = nullptr;
	switch (mediaType)
	{
		case eMEDIATYPE_VIDEO:    demuxer = m_videoDemuxer.get();    break;
		case eMEDIATYPE_AUDIO:    demuxer = m_audioDemuxer.get();    break;
		case eMEDIATYPE_SUBTITLE: demuxer = m_subtitleDemuxer.get(); break;
		default: break;
	}

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
		switch (mediaType)
		{
			case eMEDIATYPE_VIDEO: AttachVideoSource(*demuxer); break;
			case eMEDIATYPE_AUDIO: AttachAudioSource(*demuxer); break;
			default: break;
		}
	}
	else
	{
		// Non-init fragment: extract samples and inject them one at a time
		// directly into the Rialto pipeline.  InjectOneSample blocks until
		// a needData request is available, providing natural back-pressure
		// to AAMP's per-track injector thread (no internal queue required).
		auto samples = demuxer->GetSamples();
		if (!samples.empty())
		{
			// Compute total sample bytes before samples are std::move'd
			// (used by the [MemTrace] RialtoQueued log line below).
			size_t totalSampleBytes = 0;
			for (const auto &s : samples)
			{
				totalSampleBytes += s.mDataSize;
			}

			SourceState *st     = nullptr;
			bool         isVideo = false;
			int32_t      sourceId = -1;
			int32_t      width = 0, height = 0;
			int32_t      sampleRate = 0, channels = 0;
			std::shared_ptr<firebolt::rialto::CodecData> pendingCodecData;
			switch (mediaType)
			{
				case eMEDIATYPE_VIDEO:
					st       = &m_videoSrc;
					isVideo  = true;
					sourceId = m_videoSourceId;
					width    = m_videoWidth;
					height   = m_videoHeight;
					pendingCodecData = std::move(m_pendingVideoCodecData);
					m_pendingVideoCodecData = nullptr;
					break;
				case eMEDIATYPE_AUDIO:
					st         = &m_audioSrc;
					isVideo    = false;
					sourceId   = m_audioSourceId;
					sampleRate = m_audioSampleRate;
					channels   = m_audioChannels;
					pendingCodecData = std::move(m_pendingAudioCodecData);
					m_pendingAudioCodecData = nullptr;
					break;
				default:
					break;
			}

			const char *typeName = (mediaType == eMEDIATYPE_VIDEO) ? "video" :
				(mediaType == eMEDIATYPE_AUDIO) ? "audio" : "other";
			AAMPLOG_INFO("[MemTrace][%s] RialtoQueued sampleCount=%zu totalBytes=%zu segDuration=%f",
				typeName, samples.size(), totalSampleBytes, fDuration);

			size_t injectedCount = 0;
			size_t injectedBytes = 0;
			if (st != nullptr && sourceId >= 0)
			{
				// Capture generation at the start of this batch so Flush()
				// can invalidate the in-flight call.
				uint64_t capturedGen;
				{
					std::lock_guard<std::mutex> lock(st->mu);
					capturedGen = st->generation;
				}

				bool firstSample = true;
				for (auto &s : samples)
				{
					std::shared_ptr<firebolt::rialto::CodecData> codecData;
					if (firstSample)
					{
						codecData = pendingCodecData;
					}
					firstSample = false;

					const size_t sampleBytes = s.mDataSize;
					if (!InjectOneSample(
							sourceId, *st, capturedGen,
							std::move(s), isVideo,
							width, height, sampleRate, channels,
							codecData))
					{
						AAMPLOG_INFO(
							"SendTransfer aborted mid-batch mediaType=%d",
							static_cast<int>(mediaType));
						break;
					}
					++injectedCount;
					injectedBytes += sampleBytes;
				}
			}
			AAMPLOG_INFO("[MemTrace][%s] RialtoInjected count=%zu totalBytes=%zu",
				typeName, injectedCount, injectedBytes);
			AAMPLOG_INFO("Processed %zu samples for mediaType=%d", samples.size(),
				static_cast<int>(mediaType));
		}
	}

	AAMPLOG_INFO("EXIT");
	return result;
}

void AampRialtoPlayer::AttachVideoSource(Mp4Demux &demuxer)
{
	std::lock_guard<std::mutex> lock(m_attachMutex);
	if (!m_pipeline)
	{
		AAMPLOG_ERR("pipeline not created");
	}
	else
	{
		MediaCodecInfo codecInfo = demuxer.GetCodecInfo();

		AAMPLOG_INFO("codecFormat=%d codecDataSize=%zu w=%u h=%u", static_cast<int>(codecInfo.mCodecFormat),
			codecInfo.mCodecData.size(),
			codecInfo.mInfo.video.mWidth,
			codecInfo.mInfo.video.mHeight);

		std::string mimeType;
		firebolt::rialto::StreamFormat streamFormat = firebolt::rialto::StreamFormat::UNDEFINED;
		bool validCodec = true;
		switch (codecInfo.mCodecFormat)
		{
			case GST_FORMAT_VIDEO_ES_H264:
				mimeType = "video/h264";
				streamFormat = firebolt::rialto::StreamFormat::AVC;
				break;
			case GST_FORMAT_VIDEO_ES_HEVC:
				mimeType = "video/h265";
				streamFormat = firebolt::rialto::StreamFormat::HVC1;
				break;
			default:
				AAMPLOG_ERR("Unknown video codec format=%d", static_cast<int>(codecInfo.mCodecFormat));
				validCodec = false;
				break;
		}

		if (validCodec)
		{
			std::shared_ptr<firebolt::rialto::CodecData> codecData;
			if (!codecInfo.mCodecData.empty())
			{
				codecData = std::make_shared<firebolt::rialto::CodecData>();
				codecData->data = codecInfo.mCodecData;
			}

			// Update cached dimensions (stamped onto every enqueued sample).
			m_videoWidth  = static_cast<int32_t>(codecInfo.mInfo.video.mWidth);
			m_videoHeight = static_cast<int32_t>(codecInfo.mInfo.video.mHeight);
			// Stage codec data so SendTransfer stamps it onto the first sample of
			// the next non-init fragment at the correct queue position.
			m_pendingVideoCodecData = codecData;

			if (m_videoSourceId >= 0)
			{
				AAMPLOG_INFO("video source already attached (id=%d), staged new codec data w=%d h=%d",
					m_videoSourceId, m_videoWidth, m_videoHeight);
			}
			else
			{
				// Advance state machine: we are about to call attachSource() for the
				// first time on this source.
				m_stateMachine.onSourceAttaching();

				// createSession() was deferred from QueueProtectionEvent; call it now so
				// the license pre-fetcher has had time to acquire the license.
				if (m_videoProt.has_value() && m_drmBridge)
				{
					const auto &prot = *m_videoProt;
					m_videoMksId = m_drmBridge->createSession(
						prot.systemId.c_str(),
						prot.initData.data(),
						prot.initData.size(),
						prot.type);
					if (m_videoMksId < 0)
					{
						AAMPLOG_WARN("createSession failed for video");
					}
					else
					{
						AAMPLOG_INFO("createSession returned mksId=%d for video", m_videoMksId);
					}
				}

				auto source = std::make_unique<firebolt::rialto::IMediaPipeline::MediaSourceVideo>(
					mimeType,
					m_videoMksId >= 0,
					static_cast<int32_t>(codecInfo.mInfo.video.mWidth),
					static_cast<int32_t>(codecInfo.mInfo.video.mHeight),
					firebolt::rialto::SegmentAlignment::AU,
					streamFormat,
					codecData);

				std::unique_ptr<firebolt::rialto::IMediaPipeline::MediaSource> sourceBase = std::move(source);
				if (!m_pipeline->attachSource(sourceBase))
				{
					AAMPLOG_ERR("attachSource (video) failed");
				}
				else
				{
					m_videoSourceId = sourceBase->getId();
					AAMPLOG_INFO("Attached video source id=%d mime=%s w=%d h=%d", m_videoSourceId, mimeType.c_str(),
						codecInfo.mInfo.video.mWidth,
						codecInfo.mInfo.video.mHeight);

					// Set the initial segment position on the server so that
					// pushSampleIfRequired() creates a GStreamer segment before the first
					// buffer is pushed.  Without this, frames at large live-stream PTS
					// values are never rendered.
					const int64_t posNs =
						m_pendingFlushPositionNs.load(std::memory_order_relaxed);
					if (posNs >= 0)
					{
						if (!m_pipeline->setSourcePosition(
								m_videoSourceId, posNs, /*resetTime=*/true))
						{
							AAMPLOG_WARN("setSourcePosition(video, %" PRId64 ") failed", posNs);
						}
						else
						{
							AAMPLOG_INFO("setSourcePosition(video, %" PRId64 ") ok", posNs);
						}
					}

					CheckAllSourcesAttached();
				}
			} // end else: m_videoSourceId < 0
		} // end if (validCodec)
	} // end else: m_pipeline valid
}

void AampRialtoPlayer::AttachAudioSource(Mp4Demux &demuxer)
{
	std::lock_guard<std::mutex> lock(m_attachMutex);
	if (!m_pipeline)
	{
		AAMPLOG_ERR("pipeline not created");
	}
	else
	{
		MediaCodecInfo codecInfo = demuxer.GetCodecInfo();

		std::string mimeType;
		firebolt::rialto::StreamFormat streamFormat = firebolt::rialto::StreamFormat::UNDEFINED;
		bool validCodec = true;
		switch (codecInfo.mCodecFormat)
		{
			case GST_FORMAT_AUDIO_ES_AAC_RAW:
				mimeType = "audio/aac";
				streamFormat = firebolt::rialto::StreamFormat::RAW;
				break;
			case GST_FORMAT_AUDIO_ES_EC3:
				mimeType = "audio/x-eac3";
				streamFormat = firebolt::rialto::StreamFormat::UNDEFINED;
				break;
			case GST_FORMAT_AUDIO_ES_AC4:
				mimeType = "audio/x-ac4";
				streamFormat = firebolt::rialto::StreamFormat::UNDEFINED;
				break;
			default:
				AAMPLOG_ERR("Unknown audio codec format=%d", static_cast<int>(codecInfo.mCodecFormat));
				validCodec = false;
				break;
		}

		if (validCodec)
		{
			std::shared_ptr<firebolt::rialto::CodecData> audioCodecData;
			if (!codecInfo.mCodecData.empty())
			{
				audioCodecData = std::make_shared<firebolt::rialto::CodecData>();
				audioCodecData->data = codecInfo.mCodecData;
				AAMPLOG_INFO("audio codecData size=%zu", audioCodecData->data.size());
			}
			else
			{
				AAMPLOG_WARN("audio codecData is empty — Rialto may produce empty caps");
			}

			// Update cached audio parameters (stamped onto every enqueued sample).
			m_audioSampleRate = static_cast<int32_t>(codecInfo.mInfo.audio.mSampleRate);
			m_audioChannels   = static_cast<int32_t>(codecInfo.mInfo.audio.mChannelCount);
			// Stage codec data so SendTransfer stamps it onto the first sample of
			// the next non-init fragment at the correct queue position.
			m_pendingAudioCodecData = audioCodecData;

			if (m_audioSourceId >= 0)
			{
				AAMPLOG_INFO("audio source already attached (id=%d), staged new codec data rate=%d ch=%d",
					m_audioSourceId, m_audioSampleRate, m_audioChannels);
			}
			else
			{
				// Advance state machine: we are about to call attachSource() for the
				// first time on this audio source.
				m_stateMachine.onSourceAttaching();

				// createSession() was deferred from QueueProtectionEvent; call it now so
				// the license pre-fetcher has had time to acquire the license.
				if (m_audioProt.has_value() && m_drmBridge)
				{
					const auto &prot = *m_audioProt;
					m_audioMksId = m_drmBridge->createSession(
						prot.systemId.c_str(),
						prot.initData.data(),
						prot.initData.size(),
						prot.type);
					if (m_audioMksId < 0)
					{
						AAMPLOG_WARN("createSession failed for audio");
					}
					else
					{
						AAMPLOG_INFO("createSession returned mksId=%d for audio", m_audioMksId);
					}
				}

				firebolt::rialto::AudioConfig audioConfig;
				audioConfig.numberOfChannels = codecInfo.mInfo.audio.mChannelCount;
				audioConfig.sampleRate = codecInfo.mInfo.audio.mSampleRate;
				if (!codecInfo.mCodecData.empty())
				{
					audioConfig.codecSpecificConfig = codecInfo.mCodecData;
				}

				auto source = std::make_unique<firebolt::rialto::IMediaPipeline::MediaSourceAudio>(
					mimeType,
					m_audioMksId >= 0,
					audioConfig,
					firebolt::rialto::SegmentAlignment::UNDEFINED,
					streamFormat,
					audioCodecData);

				std::unique_ptr<firebolt::rialto::IMediaPipeline::MediaSource> sourceBase = std::move(source);
				if (!m_pipeline->attachSource(sourceBase))
				{
					AAMPLOG_ERR("attachSource (audio) failed");
				}
				else
				{
					m_audioSourceId = sourceBase->getId();
					AAMPLOG_INFO("Attached audio source id=%d mime=%s channels=%u rate=%u", m_audioSourceId, mimeType.c_str(),
						audioConfig.numberOfChannels, audioConfig.sampleRate);

					// Set the initial segment position on the server (mirrors the video path).
					const int64_t posNs =
						m_pendingFlushPositionNs.load(std::memory_order_relaxed);
					if (posNs >= 0)
					{
						if (!m_pipeline->setSourcePosition(
								m_audioSourceId, posNs, /*resetTime=*/true))
						{
							AAMPLOG_WARN("setSourcePosition(audio, %" PRId64 ") failed", posNs);
						}
						else
						{
							AAMPLOG_INFO("setSourcePosition(audio, %" PRId64 ") ok", posNs);
						}
					}

					CheckAllSourcesAttached();
				}
			} // end else: m_audioSourceId < 0
		} // end if (validCodec)
	} // end else: m_pipeline valid
}

void AampRialtoPlayer::CheckAllSourcesAttached()
{
	// A source is "expected" if its demuxer was created in Configure().
	// It is "ready" once attachSource() has assigned it a valid ID (>= 0).
	if (m_pipeline &&
		!(m_videoDemuxer && m_videoSourceId < 0) &&
		!(m_audioDemuxer && m_audioSourceId < 0))
	{
		AAMPLOG_INFO("All sources attached — calling allSourcesAttached()");

		if (!m_pipeline->allSourcesAttached())
		{
			AAMPLOG_ERR("allSourcesAttached() failed");
		}
		else
		{
			// Advance state machine: all sources are now fully attached.
			m_stateMachine.onAllSourcesAttached();

			// Mark that allSourcesAttached() completed so Stream() can act on it
			// even if it is called after this point.
			m_allSourcesAttachedFlag.store(true, std::memory_order_seq_cst);

			// If Stream() has already been called, issue play() now — this keeps
			// the Rialto protocol order: allSourcesAttached() → play().
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
}

bool AampRialtoPlayer::SendSample(AampMediaType mediaType, AampMediaSample &&sample)
{
	AAMPLOG_INFO("ENTRY mediaType=%d", static_cast<int>(mediaType));
	AAMPLOG_INFO("EXIT");
	return false;
}

bool AampRialtoPlayer::PipelineConfiguredForMedia(AampMediaType type)
{
	AAMPLOG_INFO("ENTRY type=%d", static_cast<int>(type));
	AAMPLOG_INFO("EXIT");
	return false;
}

void AampRialtoPlayer::EndOfStreamReached(AampMediaType type)
{
	AAMPLOG_INFO("ENTRY type=%d", static_cast<int>(type));
	SourceState *st = nullptr;
	switch (type)
	{
		case eMEDIATYPE_VIDEO:
			st = &m_videoSrc;
			break;
		case eMEDIATYPE_AUDIO:
			st = &m_audioSrc;
			break;
		default:
			break;
	}
	if (st != nullptr)
	{
		bool     fireEos   = false;
		uint32_t reqId     = 0;
		{
			std::lock_guard<std::mutex> lock(st->mu);
			st->eos = true;
			if (st->hasPending)
			{
				reqId = st->pendingRequestId;
				st->hasPending     = false;
				st->addedInPending = 0;
				fireEos = true;
			}
		}
		if (fireEos && m_pipeline)
		{
			if (!m_pipeline->haveData(
					firebolt::rialto::MediaSourceStatus::EOS, reqId))
			{
				AAMPLOG_WARN("haveData(EOS) failed requestId=%u", reqId);
			}
		}
		st->cv.notify_all();
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

		if (m_allSourcesAttachedFlag.load(std::memory_order_seq_cst))
		{
			// allSourcesAttached() already completed before this call —
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
	// Wake any in-flight SendTransfer so it abandons the current batch.
	for (SourceState *st : {&m_videoSrc, &m_audioSrc})
	{
		std::lock_guard<std::mutex> lock(st->mu);
		++st->generation;
		st->hasPending     = false;
		st->addedInPending = 0;
		st->eos            = false;
		st->cv.notify_all();
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

	// Invalidate any in-flight SendTransfer batch and clear all pacing state
	// so stale samples are not injected after the seek.
	for (SourceState *st : {&m_videoSrc, &m_audioSrc})
	{
		std::lock_guard<std::mutex> lock(st->mu);
		++st->generation;
		st->hasPending     = false;
		st->addedInPending = 0;
		st->eos            = false;
		st->cv.notify_all();
	}

	// Store the segment start position so it can be forwarded to the Rialto
	// server via setSourcePosition() for every attached source.  This is
	// required to make the Rialto server emit a GStreamer segment event
	// before the first buffer; without it pushSampleIfRequired() is a no-op
	// and frames at large live-stream PTS values (e.g. 12542 s) are never
	// rendered by the downstream decoder.
	const int64_t posNs = static_cast<int64_t>(position * kNsPerSecond);
	m_pendingFlushPositionNs.store(posNs, std::memory_order_relaxed);

	// Inform RialtoServer about the flush and update source positions for
	// any already-attached sources.
	if (m_pipeline)
	{
		if (m_videoSourceId >= 0)
		{
			bool async = false;
			if (!m_pipeline->flush(m_videoSourceId, /*resetTime=*/true, async))
			{
				AAMPLOG_WARN("flush(video) failed");
			}
			if (!m_pipeline->setSourcePosition(
					m_videoSourceId, posNs, /*resetTime=*/true))
			{
				AAMPLOG_WARN("setSourcePosition(video) failed");
			}
		}
		if (m_audioSourceId >= 0)
		{
			bool async = false;
			if (!m_pipeline->flush(m_audioSourceId, /*resetTime=*/true, async))
			{
				AAMPLOG_WARN("flush(audio) failed");
			}
			if (!m_pipeline->setSourcePosition(
					m_audioSourceId, posNs, /*resetTime=*/true))
			{
				AAMPLOG_WARN("setSourcePosition(audio) failed");
			}
		}
	}

	// Advance the state machine — new sources will re-attach on next init fragment.
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
		// Store the protection parameters now; createSession() is deferred until
		// the init fragment arrives (AttachVideoSource / AttachAudioSource) so
		// that the license pre-fetcher has had time to acquire the license first,
		// making DrmSessionManager::createDrmSession() non-blocking in the
		// common case.
		ProtectionParams prot;
		prot.systemId = protSystemId;
		prot.initData.assign(
			static_cast<const uint8_t *>(ptr),
			static_cast<const uint8_t *>(ptr) + len);
		prot.type = type;

		if (type == eMEDIATYPE_VIDEO)
		{
			m_videoProt = std::move(prot);
		}
		else if (type == eMEDIATYPE_AUDIO)
		{
			m_audioProt = std::move(prot);
		}

		AAMPLOG_INFO("EXIT — params stored for type=%d", static_cast<int>(type));
	} // end else: ptr/len/protSystemId valid
}

void AampRialtoPlayer::ClearProtectionEvent()
{
	AAMPLOG_INFO("ENTRY");
	if (m_drmBridge)
	{
		m_drmBridge->clearSessions();
	}
	m_videoMksId = -1;
	m_audioMksId = -1;
	m_videoProt = std::nullopt;
	m_audioProt = std::nullopt;
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
	AAMPLOG_INFO("ENTRY forceStop=%d", forceStop);
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
	// Wake any in-flight SendTransfer so it abandons the current batch.
	// This is called during TeardownStream before StopInjectLoop() joins
	// the injection threads.  Without this generation bump the injection
	// thread may remain blocked in InjectOneSample::cv.wait() and the
	// join would deadlock.
	for (SourceState *st : {&m_videoSrc, &m_audioSrc})
	{
		std::lock_guard<std::mutex> lock(st->mu);
		++st->generation;
		st->hasPending     = false;
		st->addedInPending = 0;
		st->cv.notify_all();
	}
	AAMPLOG_INFO("EXIT");
}

void AampRialtoPlayer::SetStreamCaps(AampMediaType type, MediaCodecInfo &&codecInfo)
{
	AAMPLOG_INFO("ENTRY type=%d", static_cast<int>(type));
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
// Segment injection — synchronous, paced by Rialto needData requests
// ---------------------------------------------------------------------------

namespace {

/// Map AAMP cipher type to the Rialto CipherMode enum.
firebolt::rialto::CipherMode cipherTypeToRialto(CipherType cipher)
{
	switch (cipher)
	{
		case CIPHER_TYPE_CENC: return firebolt::rialto::CipherMode::CENC;
		case CIPHER_TYPE_CBCS: return firebolt::rialto::CipherMode::CBCS;
		case CIPHER_TYPE_CBC1: return firebolt::rialto::CipherMode::CBC1;
		case CIPHER_TYPE_CENS: return firebolt::rialto::CipherMode::CENS;
		default:               return firebolt::rialto::CipherMode::UNKNOWN;
	}
}

} // namespace

bool AampRialtoPlayer::InjectOneSample(
	int32_t sourceId,
	SourceState &st,
	uint64_t capturedGen,
	AampMediaSample &&sample,
	bool isVideo,
	int32_t width,
	int32_t height,
	int32_t sampleRate,
	int32_t channels,
	std::shared_ptr<firebolt::rialto::CodecData> codecData)
{
	bool injected = false;
	if (!m_pipeline)
	{
		AAMPLOG_WARN("pipeline is null, dropping sample for sourceId=%d", sourceId);
	}
	else
	{
		// Loop until either:
		//   - a needData request is satisfied with this sample (success), or
		//   - the batch is invalidated by Flush()/Stop() (caller aborts), or
		//   - addSegment NO_SPACE: close current request and wait for the next.
		bool done = false;
		while (!done)
		{
			uint32_t reqId = 0;
			{
				std::unique_lock<std::mutex> lock(st.mu);
				st.cv.wait(lock, [&]{
					return st.generation != capturedGen ||
					       st.hasPending;
				});
				if (st.generation != capturedGen)
				{
					// Flush()/Stop()/destructor invalidated this batch.
					done = true;
					continue;
				}
				reqId = st.pendingRequestId;
			}

			// Build the segment outside the lock.
			std::unique_ptr<firebolt::rialto::IMediaPipeline::MediaSegment>
				segment;
			if (isVideo)
			{
				segment = std::make_unique<
					firebolt::rialto::IMediaPipeline::MediaSegmentVideo>(
					sourceId,
					static_cast<int64_t>(sample.mPts * kNsPerSecond),
					static_cast<int64_t>(sample.mDuration * kNsPerSecond),
					width,
					height);
			}
			else
			{
				segment = std::make_unique<
					firebolt::rialto::IMediaPipeline::MediaSegmentAudio>(
					sourceId,
					static_cast<int64_t>(sample.mPts * kNsPerSecond),
					static_cast<int64_t>(sample.mDuration * kNsPerSecond),
					sampleRate,
					channels);
			}
			if (codecData)
			{
				segment->setCodecData(codecData);
			}

			// Annotate the segment with DRM encryption metadata when the
			// sample is encrypted and a valid mks_id is available.
			const int32_t mksId = isVideo ? m_videoMksId : m_audioMksId;
			if (sample.mDrmMetadata.mIsEncrypted && mksId >= 0)
			{
				segment->setEncrypted(true);
				segment->setMediaKeySessionId(mksId);
				segment->setKeyId(sample.mDrmMetadata.mKeyId);
				segment->setInitVector(sample.mDrmMetadata.mIV);
				segment->setCipherMode(
					cipherTypeToRialto(sample.mDrmMetadata.mCipher));
				if (sample.mDrmMetadata.mCipher == CIPHER_TYPE_CBCS)
				{
					segment->setEncryptionPattern(
						sample.mDrmMetadata.mCryptByteBlock,
						sample.mDrmMetadata.mSkipByteBlock);
				}

				if (sample.mDrmMetadata.mNumSubSamples > 0)
				{
					// Each subsample entry is packed big-endian:
					//   uint16_t clearBytes + uint32_t encryptedBytes
					const size_t kEntrySize = 6;
					const auto  &raw        = sample.mDrmMetadata.mSubSamples;
					for (uint32_t s = 0;
					     s < sample.mDrmMetadata.mNumSubSamples; ++s)
					{
						const size_t offset = s * kEntrySize;
						if (offset + kEntrySize > raw.size())
						{
							break;
						}
						const uint16_t clearBytes =
							(static_cast<uint16_t>(raw[offset])     << 8) |
							 static_cast<uint16_t>(raw[offset + 1]);
						const uint32_t encBytes =
							(static_cast<uint32_t>(raw[offset + 2]) << 24) |
							(static_cast<uint32_t>(raw[offset + 3]) << 16) |
							(static_cast<uint32_t>(raw[offset + 4]) <<  8) |
							 static_cast<uint32_t>(raw[offset + 5]);
						segment->addSubSample(clearBytes, encBytes);
					}
				}
				else
				{
					// No subsample map — treat the whole sample as encrypted.
					segment->addSubSample(
						/*numClearBytes=*/0,
						static_cast<uint32_t>(sample.mDataSize));
				}
			}

			segment->setData(
				static_cast<uint32_t>(sample.mDataSize),
				sample.mData.get());

			auto addStatus = m_pipeline->addSegment(reqId, segment);
			if (addStatus == firebolt::rialto::AddSegmentStatus::NO_SPACE)
			{
				// Close out the current needData with whatever has already
				// been accepted, then loop and wait for the next request to
				// retry this sample.
				size_t addedSoFar = 0;
				bool   sendHaveData = false;
				{
					std::lock_guard<std::mutex> lock(st.mu);
					if (st.generation == capturedGen && st.hasPending &&
					    st.pendingRequestId == reqId)
					{
						addedSoFar         = st.addedInPending;
						st.hasPending      = false;
						st.addedInPending  = 0;
						sendHaveData       = true;
					}
				}
				if (sendHaveData)
				{
					const auto status = addedSoFar > 0
						? firebolt::rialto::MediaSourceStatus::OK
						: firebolt::rialto::MediaSourceStatus::NO_AVAILABLE_SAMPLES;
					if (!m_pipeline->haveData(status, reqId))
					{
						AAMPLOG_WARN("haveData failed requestId=%u", reqId);
					}
				}
				AAMPLOG_INFO("addSegment NO_SPACE sourceId=%d requestId=%u — waiting for next needData",
					sourceId, reqId);
				// continue: loop and wait for the next pending request
			}
			else
			{
				if (addStatus != firebolt::rialto::AddSegmentStatus::OK)
				{
					AAMPLOG_WARN("addSegment failed sourceId=%d requestId=%u status=%d",
						sourceId, reqId, static_cast<int>(addStatus));
				}

				bool   sendHaveData = false;
				{
					std::lock_guard<std::mutex> lock(st.mu);
					if (st.generation == capturedGen && st.hasPending &&
					    st.pendingRequestId == reqId)
					{
						++st.addedInPending;
						if (st.addedInPending >= st.pendingFrameCount)
						{
							st.hasPending     = false;
							st.addedInPending = 0;
							sendHaveData      = true;
						}
					}
				}
				if (sendHaveData)
				{
					if (!m_pipeline->haveData(
							firebolt::rialto::MediaSourceStatus::OK, reqId))
					{
						AAMPLOG_WARN("haveData failed requestId=%u", reqId);
					}
				}
				injected = true;
				done     = true;
			}
		}
	}
	return injected;
}

// ---------------------------------------------------------------------------
// Callbacks invoked from AampRialtoMediaPipelineClient
// ---------------------------------------------------------------------------

void AampRialtoPlayer::OnNeedMediaData(
	int32_t sourceId, size_t frameCount, uint32_t requestId)
{
	AAMPLOG_INFO("sourceId=%d frameCount=%zu requestId=%u", sourceId, frameCount, requestId);
	SourceState *st = nullptr;
	if (sourceId == m_videoSourceId)
	{
		st = &m_videoSrc;
	}
	else if (sourceId == m_audioSourceId)
	{
		st = &m_audioSrc;
	}
	else
	{
		AAMPLOG_WARN("unknown sourceId=%d (video=%d audio=%d)",
			sourceId, m_videoSourceId, m_audioSourceId);
	}
	if (st != nullptr)
	{
		bool fireEos = false;
		{
			std::lock_guard<std::mutex> lock(st->mu);
			if (st->eos)
			{
				fireEos = true;
			}
			else
			{
				st->hasPending        = true;
				st->pendingRequestId  = requestId;
				st->pendingFrameCount = std::max<size_t>(frameCount, 1);
				st->addedInPending    = 0;
			}
		}
		if (fireEos)
		{
			if (m_pipeline &&
			    !m_pipeline->haveData(
				    firebolt::rialto::MediaSourceStatus::EOS, requestId))
			{
				AAMPLOG_WARN("haveData(EOS) failed requestId=%u", requestId);
			}
		}
		else
		{
			st->cv.notify_all();
		}
	}
}

void AampRialtoPlayer::OnCancelNeedMediaData(int32_t sourceId)
{
	AAMPLOG_INFO("sourceId=%d", sourceId);
	SourceState *st = nullptr;
	if (sourceId == m_videoSourceId)
	{
		st = &m_videoSrc;
	}
	else if (sourceId == m_audioSourceId)
	{
		st = &m_audioSrc;
	}
	if (st != nullptr)
	{
		{
			std::lock_guard<std::mutex> lock(st->mu);
			st->hasPending     = false;
			st->addedInPending = 0;
		}
		st->cv.notify_all();
	}
}

void AampRialtoPlayer::OnPlaybackState(firebolt::rialto::PlaybackState state)
{
	AAMPLOG_INFO("state=%d", static_cast<int>(state));

	// Drive the state machine based on the Rialto server notification.
	switch (state)
	{
		case firebolt::rialto::PlaybackState::PLAYING:
		{
			m_stateMachine.onPlaybackStarted();

			const bool firstFrame =
				!m_firstFrameNotified.exchange(true, std::memory_order_acq_rel);

			if (firstFrame)
			{
				// ── Initial tune: first transition to PLAYING ──────────────
				// Log profiler timestamps then drive AAMP to eSTATE_PLAYING
				// via NotifyFirstFrameReceived (which also signals
				// waitforplaystart, fires the tuned event, and inits CC).
				m_notifiable->LogFirstFrame();
				m_notifiable->LogTuneComplete();
				m_notifiable->NotifyFirstBufferProcessed(GetVideoRectangle());
				m_notifiable->NotifyFirstFrameReceived(/*ccDecoderHandle=*/0);
			}
			else if (m_notifiable->GetState() == eSTATE_SEEKING)
			{
				// ── Post-seek recovery ─────────────────────────────────────
				// NotifyFirstBufferProcessed checks for eSTATE_SEEKING and
				// transitions to eSTATE_PLAYING when it is the current state.
				m_notifiable->NotifyFirstBufferProcessed(GetVideoRectangle());
				m_notifiable->NotifyFirstFrameReceived(/*ccDecoderHandle=*/0);
			}
			else
			{
				// ── Resume from pause (PAUSED → PLAYING) ──────────────────
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
