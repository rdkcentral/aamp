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
#include "AampLogManager.h"
#include "priv_aamp.h"
#include "mp4demux/MP4Demux.h"
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
}

// #define USE_AAMP_GST_PLAYER

#ifndef USE_AAMP_GST_PLAYER

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

AampRialtoPlayer::AampRialtoPlayer(
	PrivateInstanceAAMP *aamp,
	id3_callback_t id3HandlerCallback,
	std::function<void(const unsigned char *, int, int, int)> exportFrames)
	: m_aamp(aamp)
	, m_client(nullptr)
	, m_pipeline(nullptr)
{
	AAMPLOG_INFO("AampRialtoPlayer: constructed, aamp=%p", aamp);
}

AampRialtoPlayer::~AampRialtoPlayer()
{
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
	AAMPLOG_INFO("AampRialtoPlayer::%s ENTRY videoFormat=%d audioFormat=%d subFormat=%d"
		" bESChangeStatus=%d setReadyAfterPipelineCreation=%d",
		__FUNCTION__, static_cast<int>(videoFormat), static_cast<int>(audioFormat),
		static_cast<int>(subFormat), bESChangeStatus, setReadyAfterPipelineCreation);


	if (!m_client)
	{
		m_client = std::make_shared<AampRialtoMediaPipelineClient>();
	}

	// Reset per-session state so a re-tune starts clean.
	m_videoSourceId = -1;
	m_audioSourceId = -1;
	m_pendingFlushPositionNs.store(-1, std::memory_order_relaxed);
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
			AAMPLOG_WARN("AampRialtoPlayer::%s Failed to create"
				" IClientLogControlFactory — Rialto logs suppressed",
				__FUNCTION__);
		}
	}

	if (!m_pipelineFactory)
	{
		m_pipelineFactory = firebolt::rialto::IMediaPipelineFactory::createFactory();
	}
	auto &factory = m_pipelineFactory;
	if (!factory)
	{
		AAMPLOG_ERR("AampRialtoPlayer::%s Failed to create IMediaPipelineFactory"
			" — is the Rialto server running?", __FUNCTION__);
	}
	else
	{
		constexpr std::uint32_t kWidth{3840};
		constexpr std::uint32_t kHeight{2160};
		firebolt::rialto::VideoRequirements kRequirements{kWidth, kHeight};
		m_pipeline = factory->createMediaPipeline(
			std::weak_ptr<firebolt::rialto::IMediaPipelineClient>(m_client),
			kRequirements);
		if (!m_pipeline)
		{
			AAMPLOG_ERR("AampRialtoPlayer::%s createMediaPipeline returned nullptr"
				" — check Rialto server logs (syslog) for details", __FUNCTION__);
		}
		else
		{
			AAMPLOG_INFO("AampRialtoPlayer::%s Created pipeline %p",
				__FUNCTION__, m_pipeline.get());

			// load() MUST be called before any attachSource() — it is what
			// causes the Rialto server to create the underlying Gstreamer player.
			if (!m_pipeline->load(
					firebolt::rialto::MediaType::MSE,
					"video/mp4",
					/*url=*/""))
			{
				AAMPLOG_ERR("AampRialtoPlayer::%s load() failed"
					" — Rialto will reject attachSource calls",
					__FUNCTION__);
			}
			else
			{
				AAMPLOG_INFO("AampRialtoPlayer::%s load() succeeded",
					__FUNCTION__);

				// Wire need-data callbacks so the injection thread
				// receives notifications from the pipeline client.
				m_client->SetNeedDataCallback(
					[this](int32_t sid, size_t fc, uint32_t rid) {
						OnNeedMediaData(sid, fc, rid);
					});
				m_client->SetCancelNeedDataCallback(
					[this](int32_t sid) {
						OnCancelNeedMediaData(sid);
					});

				StartInjectionThread();
			}
		}
	}

	if (videoFormat != FORMAT_INVALID && videoFormat != FORMAT_UNKNOWN)
	{
		m_videoDemuxer = std::make_unique<Mp4Demux>();
		m_aamp->ResumeTrackDownloads(eMEDIATYPE_VIDEO);
		AAMPLOG_INFO("AampRialtoPlayer::%s Created video Mp4Demux", __FUNCTION__);
	}
	if (audioFormat != FORMAT_INVALID && audioFormat != FORMAT_UNKNOWN)
	{
		m_audioDemuxer = std::make_unique<Mp4Demux>();
		m_aamp->ResumeTrackDownloads(eMEDIATYPE_AUDIO);
		AAMPLOG_INFO("AampRialtoPlayer::%s Created audio Mp4Demux", __FUNCTION__);
	}
	if (subFormat != FORMAT_INVALID && subFormat != FORMAT_UNKNOWN)
	{
		m_subtitleDemuxer = std::make_unique<Mp4Demux>();
		m_aamp->ResumeTrackDownloads(eMEDIATYPE_SUBTITLE);
		AAMPLOG_INFO("AampRialtoPlayer::%s Created subtitle Mp4Demux", __FUNCTION__);
	}


	AAMPLOG_INFO("AampRialtoPlayer::%s EXIT", __FUNCTION__);
}

bool AampRialtoPlayer::SendCopy(
	AampMediaType mediaType,
	std::vector<uint8_t> &&buffer,
	double fpts,
	double fdts,
	double fDuration)
{
	AAMPLOG_INFO("AampRialtoPlayer::%s ENTRY mediaType=%d bufferSize=%zu fpts=%f fdts=%f fDuration=%f",
		__FUNCTION__, static_cast<int>(mediaType), buffer.size(), fpts, fdts, fDuration);
	AAMPLOG_INFO("AampRialtoPlayer::%s EXIT", __FUNCTION__);
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
	AAMPLOG_INFO("AampRialtoPlayer::%s ENTRY mediaType=%d bufferSize=%zu"
		" fpts=%f fdts=%f fDuration=%f fragmentPTSoffset=%f"
		" initFragment=%d discontinuity=%d",
		__FUNCTION__, static_cast<int>(mediaType), buffer.size(),
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
			AAMPLOG_WARN("AampRialtoPlayer::%s No demuxer for"
				" mediaType=%d", __FUNCTION__,
				static_cast<int>(mediaType));
		}
	}
	else if (!demuxer->Parse(buffer.data(), buffer.size()))
	{
		AAMPLOG_ERR("AampRialtoPlayer::%s Mp4Demux::Parse failed"
			" mediaType=%d err=%d",
			__FUNCTION__, static_cast<int>(mediaType),
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
		// Non-init fragment: extract samples and push to the
		// injection buffer so the injection thread can forward
		// them to the Rialto pipeline on the next needData event.
		auto samples = demuxer->GetSamples();
		if (!samples.empty())
		{
			std::lock_guard<std::mutex> lock(m_injectorMutex);
			switch (mediaType)
			{
				case eMEDIATYPE_VIDEO:
					for (auto &s : samples)
					{
						m_videoSampleQueue.push_back(
							std::move(s));
					}
					break;
				case eMEDIATYPE_AUDIO:
					for (auto &s : samples)
					{
						m_audioSampleQueue.push_back(
							std::move(s));
					}
					break;
				default:
					break;
			}
			m_injectorCv.notify_one();
			AAMPLOG_INFO("AampRialtoPlayer::%s Queued %zu samples"
				" for mediaType=%d",
				__FUNCTION__, samples.size(),
				static_cast<int>(mediaType));
		}
	}

	AAMPLOG_INFO("AampRialtoPlayer::%s EXIT", __FUNCTION__);
	return result;
}

void AampRialtoPlayer::AttachVideoSource(Mp4Demux &demuxer)
{
	if (!m_pipeline)
	{
		AAMPLOG_ERR("AampRialtoPlayer::%s pipeline not created", __FUNCTION__);
	}
	else if (m_videoSourceId >= 0)
	{
		AAMPLOG_INFO("AampRialtoPlayer::%s video source already attached"
			" (id=%d), skipping", __FUNCTION__, m_videoSourceId);
	}
	else
	{
		MediaCodecInfo codecInfo = demuxer.GetCodecInfo();

		AAMPLOG_INFO("AampRialtoPlayer::%s codecFormat=%d codecDataSize=%zu"
			" w=%u h=%u",
			__FUNCTION__,
			static_cast<int>(codecInfo.mCodecFormat),
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
				AAMPLOG_ERR("AampRialtoPlayer::%s Unknown video codec format=%d",
					__FUNCTION__, static_cast<int>(codecInfo.mCodecFormat));
				validCodec = false;
				break;
		}

		if (validCodec)
		{
			std::shared_ptr<firebolt::rialto::CodecData> codecData;
			if (!codecInfo.mCodecData.empty())
			{
				codecData = std::make_shared<firebolt::rialto::CodecData>();
				codecData->data = std::move(codecInfo.mCodecData);
			}

			auto source = std::make_unique<firebolt::rialto::IMediaPipeline::MediaSourceVideo>(
				mimeType,
				/*hasDrm=*/false,
				static_cast<int32_t>(codecInfo.mInfo.video.mWidth),
				static_cast<int32_t>(codecInfo.mInfo.video.mHeight),
				firebolt::rialto::SegmentAlignment::AU,
				streamFormat,
				codecData);

			std::unique_ptr<firebolt::rialto::IMediaPipeline::MediaSource> sourceBase = std::move(source);
			if (!m_pipeline->attachSource(sourceBase))
			{
				AAMPLOG_ERR("AampRialtoPlayer::%s attachSource (video) failed", __FUNCTION__);
			}
			else
			{
				m_videoSourceId = sourceBase->getId();
				AAMPLOG_INFO("AampRialtoPlayer::%s Attached video source id=%d"
					" mime=%s w=%d h=%d",
					__FUNCTION__, m_videoSourceId, mimeType.c_str(),
					codecInfo.mInfo.video.mWidth,
					codecInfo.mInfo.video.mHeight);

				m_videoWidth  = static_cast<int32_t>(codecInfo.mInfo.video.mWidth);
				m_videoHeight = static_cast<int32_t>(codecInfo.mInfo.video.mHeight);

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
						AAMPLOG_WARN("AampRialtoPlayer::%s"
							" setSourcePosition(video, %" PRId64 ") failed",
							__FUNCTION__, posNs);
					}
					else
					{
						AAMPLOG_INFO("AampRialtoPlayer::%s"
							" setSourcePosition(video, %" PRId64 ") ok",
							__FUNCTION__, posNs);
					}
				}

				CheckAllSourcesAttached();
			}
		}
	}
}

void AampRialtoPlayer::AttachAudioSource(Mp4Demux &demuxer)
{
	if (!m_pipeline)
	{
		AAMPLOG_ERR("AampRialtoPlayer::%s pipeline not created", __FUNCTION__);
	}
	else if (m_audioSourceId >= 0)
	{
		AAMPLOG_INFO("AampRialtoPlayer::%s audio source already attached"
			" (id=%d), skipping", __FUNCTION__, m_audioSourceId);
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
				AAMPLOG_ERR("AampRialtoPlayer::%s Unknown audio codec format=%d",
					__FUNCTION__, static_cast<int>(codecInfo.mCodecFormat));
				validCodec = false;
				break;
		}

		if (validCodec)
		{
			firebolt::rialto::AudioConfig audioConfig;
			audioConfig.numberOfChannels = codecInfo.mInfo.audio.mChannelCount;
			audioConfig.sampleRate = codecInfo.mInfo.audio.mSampleRate;
			if (!codecInfo.mCodecData.empty())
			{
				audioConfig.codecSpecificConfig = codecInfo.mCodecData;
			}

			// Pass codec data via the shared_ptr parameter so the Rialto server
			// can build the GStreamer caps codec_data buffer (same mechanism as video).
			std::shared_ptr<firebolt::rialto::CodecData> audioCodecData;
			if (!codecInfo.mCodecData.empty())
			{
				audioCodecData = std::make_shared<firebolt::rialto::CodecData>();
				audioCodecData->data = codecInfo.mCodecData;
				AAMPLOG_INFO("AampRialtoPlayer::%s audio codecData size=%zu",
					__FUNCTION__, audioCodecData->data.size());
			}
			else
			{
				AAMPLOG_WARN("AampRialtoPlayer::%s audio codecData is empty"
					" — Rialto may produce empty caps", __FUNCTION__);
			}

			auto source = std::make_unique<firebolt::rialto::IMediaPipeline::MediaSourceAudio>(
				mimeType,
				/*hasDrm=*/false,
				audioConfig,
				firebolt::rialto::SegmentAlignment::UNDEFINED,
				streamFormat,
				audioCodecData);

			std::unique_ptr<firebolt::rialto::IMediaPipeline::MediaSource> sourceBase = std::move(source);
			if (!m_pipeline->attachSource(sourceBase))
			{
				AAMPLOG_ERR("AampRialtoPlayer::%s attachSource (audio) failed", __FUNCTION__);
			}
			else
			{
				m_audioSourceId = sourceBase->getId();
				AAMPLOG_INFO("AampRialtoPlayer::%s Attached audio source id=%d"
					" mime=%s channels=%u rate=%u",
					__FUNCTION__, m_audioSourceId, mimeType.c_str(),
					audioConfig.numberOfChannels, audioConfig.sampleRate);

				m_audioSampleRate = static_cast<int32_t>(audioConfig.sampleRate);
				m_audioChannels   = static_cast<int32_t>(audioConfig.numberOfChannels);

				// Set the initial segment position on the server (mirrors the video path).
				const int64_t posNs =
					m_pendingFlushPositionNs.load(std::memory_order_relaxed);
				if (posNs >= 0)
				{
					if (!m_pipeline->setSourcePosition(
							m_audioSourceId, posNs, /*resetTime=*/true))
					{
						AAMPLOG_WARN("AampRialtoPlayer::%s"
							" setSourcePosition(audio, %" PRId64 ") failed",
							__FUNCTION__, posNs);
					}
					else
					{
						AAMPLOG_INFO("AampRialtoPlayer::%s"
							" setSourcePosition(audio, %" PRId64 ") ok",
							__FUNCTION__, posNs);
					}
				}

				CheckAllSourcesAttached();
			}
		}
	}
}

void AampRialtoPlayer::CheckAllSourcesAttached()
{
	// A source is "expected" if its demuxer was created in Configure().
	// It is "ready" once attachSource() has assigned it a valid ID (>= 0).
	if (m_pipeline &&
		!(m_videoDemuxer && m_videoSourceId < 0) &&
		!(m_audioDemuxer && m_audioSourceId < 0))
	{
		AAMPLOG_INFO("AampRialtoPlayer::%s All sources attached — calling allSourcesAttached()",
			__FUNCTION__);

		if (!m_pipeline->allSourcesAttached())
		{
			AAMPLOG_ERR("AampRialtoPlayer::%s allSourcesAttached() failed", __FUNCTION__);
		}
		else
		{
			// Mark that allSourcesAttached() completed so Stream() can act on it
			// even if it is called after this point.
			m_allSourcesAttachedFlag.store(true, std::memory_order_seq_cst);

			// If Stream() has already been called, issue play() now — this keeps
			// the Rialto protocol order: allSourcesAttached() → play().
			if (m_playRequested.load(std::memory_order_seq_cst))
			{
				AAMPLOG_INFO("AampRialtoPlayer::%s play() deferred by Stream() —"
					" issuing now", __FUNCTION__);
				bool async = false;
				if (!m_pipeline->play(async))
				{
					AAMPLOG_ERR("AampRialtoPlayer::%s play() failed", __FUNCTION__);
				}
			}
		}
	}
}

bool AampRialtoPlayer::SendSample(AampMediaType mediaType, AampMediaSample &sample)
{
	AAMPLOG_INFO("AampRialtoPlayer::%s ENTRY mediaType=%d", __FUNCTION__, static_cast<int>(mediaType));
	AAMPLOG_INFO("AampRialtoPlayer::%s EXIT", __FUNCTION__);
	return false;
}

bool AampRialtoPlayer::PipelineConfiguredForMedia(AampMediaType type)
{
	AAMPLOG_INFO("AampRialtoPlayer::%s ENTRY type=%d", __FUNCTION__, static_cast<int>(type));
	AAMPLOG_INFO("AampRialtoPlayer::%s EXIT", __FUNCTION__);
	return false;
}

void AampRialtoPlayer::EndOfStreamReached(AampMediaType type)
{
	AAMPLOG_INFO("AampRialtoPlayer::%s ENTRY type=%d",
		__FUNCTION__, static_cast<int>(type));
	{
		std::lock_guard<std::mutex> lock(m_injectorMutex);
		switch (type)
		{
			case eMEDIATYPE_VIDEO: m_videoEos = true; break;
			case eMEDIATYPE_AUDIO: m_audioEos = true; break;
			default: break;
		}
	}
	m_injectorCv.notify_one();
	AAMPLOG_INFO("AampRialtoPlayer::%s EXIT", __FUNCTION__);
}

void AampRialtoPlayer::Stream()
{
	AAMPLOG_INFO("AampRialtoPlayer::%s ENTRY", __FUNCTION__);
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
				AAMPLOG_ERR("AampRialtoPlayer::%s play() failed",
					__FUNCTION__);
			}
		}
		else
		{
			// Sources are not yet attached; play() will be issued by
			// CheckAllSourcesAttached() once all sources are registered.
			AAMPLOG_INFO("AampRialtoPlayer::%s deferring play() until"
				" allSourcesAttached()", __FUNCTION__);
		}
	}
	AAMPLOG_INFO("AampRialtoPlayer::%s EXIT", __FUNCTION__);
}

void AampRialtoPlayer::Stop(bool keepLastFrame)
{
	AAMPLOG_INFO("AampRialtoPlayer::%s ENTRY keepLastFrame=%d",
		__FUNCTION__, keepLastFrame);
	StopInjectionThread();
	if (m_pipeline)
	{
		m_pipeline->stop();
	}
	AAMPLOG_INFO("AampRialtoPlayer::%s EXIT", __FUNCTION__);
}

void AampRialtoPlayer::Flush(double position, int rate, bool shouldTearDown)
{
	AAMPLOG_INFO("AampRialtoPlayer::%s ENTRY position=%f rate=%d"
		" shouldTearDown=%d",
		__FUNCTION__, position, rate, shouldTearDown);
	{
		std::lock_guard<std::mutex> lock(m_injectorMutex);
		m_videoSampleQueue.clear();
		m_audioSampleQueue.clear();
		m_videoPendingReqs.clear();
		m_audioPendingReqs.clear();
		m_videoEos = false;
		m_audioEos = false;
	}

	// Store the segment start position so it can be forwarded to the Rialto
	// server via setSourcePosition() for every attached source.  This is
	// required to make the Rialto server emit a GStreamer segment event
	// before the first buffer; without it pushSampleIfRequired() is a no-op
	// and frames at large live-stream PTS values (e.g. 12542 s) are never
	// rendered by the downstream decoder.
	const int64_t posNs = static_cast<int64_t>(position * kNsPerSecond);
	m_pendingFlushPositionNs.store(posNs, std::memory_order_relaxed);

	// If sources were already attached (e.g. mid-stream seek) apply the
	// position immediately; otherwise AttachVideoSource/AttachAudioSource
	// will apply it once each source is registered.
	if (m_pipeline)
	{
		if (m_videoSourceId >= 0)
		{
			if (!m_pipeline->setSourcePosition(
					m_videoSourceId, posNs, /*resetTime=*/true))
			{
				AAMPLOG_WARN("AampRialtoPlayer::%s"
					" setSourcePosition(video) failed",
					__FUNCTION__);
			}
		}
		if (m_audioSourceId >= 0)
		{
			if (!m_pipeline->setSourcePosition(
					m_audioSourceId, posNs, /*resetTime=*/true))
			{
				AAMPLOG_WARN("AampRialtoPlayer::%s"
					" setSourcePosition(audio) failed",
					__FUNCTION__);
			}
		}
	}

	AAMPLOG_INFO("AampRialtoPlayer::%s EXIT", __FUNCTION__);
}

void AampRialtoPlayer::FlushTrack(AampMediaType mediaType, double position)
{
	AAMPLOG_INFO("AampRialtoPlayer::%s ENTRY mediaType=%d position=%f",
		__FUNCTION__, static_cast<int>(mediaType), position);
	AAMPLOG_INFO("AampRialtoPlayer::%s EXIT", __FUNCTION__);
}

bool AampRialtoPlayer::SetPlayBackRate(double rate)
{
	AAMPLOG_INFO("AampRialtoPlayer::%s ENTRY rate=%f", __FUNCTION__, rate);
	AAMPLOG_INFO("AampRialtoPlayer::%s EXIT", __FUNCTION__);
	return false;
}

bool AampRialtoPlayer::Pause(bool pause, bool forceStopGstreamerPreBuffering)
{
	AAMPLOG_INFO("AampRialtoPlayer::%s ENTRY pause=%d forceStopGstreamerPreBuffering=%d",
		__FUNCTION__, pause, forceStopGstreamerPreBuffering);
	AAMPLOG_INFO("AampRialtoPlayer::%s EXIT", __FUNCTION__);
	return false;
}

long AampRialtoPlayer::GetDurationMilliseconds()
{
	AAMPLOG_INFO("AampRialtoPlayer::%s ENTRY", __FUNCTION__);
	AAMPLOG_INFO("AampRialtoPlayer::%s EXIT", __FUNCTION__);
	return 0;
}

long long AampRialtoPlayer::GetPositionMilliseconds()
{
	AAMPLOG_INFO("AampRialtoPlayer::%s ENTRY", __FUNCTION__);
	AAMPLOG_INFO("AampRialtoPlayer::%s EXIT", __FUNCTION__);
	return 0;
}

long long AampRialtoPlayer::GetVideoPTS()
{
	AAMPLOG_INFO("AampRialtoPlayer::%s ENTRY", __FUNCTION__);
	AAMPLOG_INFO("AampRialtoPlayer::%s EXIT", __FUNCTION__);
	return 0;
}

void AampRialtoPlayer::SetVideoRectangle(int x, int y, int w, int h)
{
	AAMPLOG_INFO("AampRialtoPlayer::%s ENTRY x=%d y=%d w=%d h=%d", __FUNCTION__, x, y, w, h);
	AAMPLOG_INFO("AampRialtoPlayer::%s EXIT", __FUNCTION__);
}

void AampRialtoPlayer::SetVideoZoom(VideoZoomMode zoom)
{
	AAMPLOG_INFO("AampRialtoPlayer::%s ENTRY zoom=%d", __FUNCTION__, static_cast<int>(zoom));
	AAMPLOG_INFO("AampRialtoPlayer::%s EXIT", __FUNCTION__);
}

void AampRialtoPlayer::SetVideoMute(bool muted)
{
	AAMPLOG_INFO("AampRialtoPlayer::%s ENTRY muted=%d", __FUNCTION__, muted);
	AAMPLOG_INFO("AampRialtoPlayer::%s EXIT", __FUNCTION__);
}

void AampRialtoPlayer::SetSubtitleMute(bool muted)
{
	AAMPLOG_INFO("AampRialtoPlayer::%s ENTRY muted=%d", __FUNCTION__, muted);
	AAMPLOG_INFO("AampRialtoPlayer::%s EXIT", __FUNCTION__);
}

void AampRialtoPlayer::SetSubtitlePtsOffset(std::uint64_t pts_offset)
{
	AAMPLOG_INFO("AampRialtoPlayer::%s ENTRY pts_offset=%" PRIu64, __FUNCTION__, pts_offset);
	AAMPLOG_INFO("AampRialtoPlayer::%s EXIT", __FUNCTION__);
}

void AampRialtoPlayer::SetAudioVolume(int volume)
{
	AAMPLOG_INFO("AampRialtoPlayer::%s ENTRY volume=%d", __FUNCTION__, volume);
	AAMPLOG_INFO("AampRialtoPlayer::%s EXIT", __FUNCTION__);
}

bool AampRialtoPlayer::Discontinuity(AampMediaType mediaType)
{
	AAMPLOG_INFO("AampRialtoPlayer::%s ENTRY mediaType=%d", __FUNCTION__, static_cast<int>(mediaType));
	AAMPLOG_INFO("AampRialtoPlayer::%s EXIT", __FUNCTION__);
	return false;
}

bool AampRialtoPlayer::CheckForPTSChangeWithTimeout(long timeout)
{
	AAMPLOG_INFO("AampRialtoPlayer::%s ENTRY timeout=%ld", __FUNCTION__, timeout);
	AAMPLOG_INFO("AampRialtoPlayer::%s EXIT", __FUNCTION__);
	return false;
}

bool AampRialtoPlayer::IsCacheEmpty(AampMediaType mediaType)
{
	AAMPLOG_INFO("AampRialtoPlayer::%s ENTRY mediaType=%d", __FUNCTION__, static_cast<int>(mediaType));
	AAMPLOG_INFO("AampRialtoPlayer::%s EXIT", __FUNCTION__);
	return false;
}

void AampRialtoPlayer::ResetEOSSignalledFlag()
{
	AAMPLOG_INFO("AampRialtoPlayer::%s ENTRY", __FUNCTION__);
	AAMPLOG_INFO("AampRialtoPlayer::%s EXIT", __FUNCTION__);
}

void AampRialtoPlayer::NotifyFragmentCachingComplete()
{
	AAMPLOG_INFO("AampRialtoPlayer::%s ENTRY", __FUNCTION__);
	AAMPLOG_INFO("AampRialtoPlayer::%s EXIT", __FUNCTION__);
}

void AampRialtoPlayer::NotifyFragmentCachingOngoing()
{
	AAMPLOG_INFO("AampRialtoPlayer::%s ENTRY", __FUNCTION__);
	AAMPLOG_INFO("AampRialtoPlayer::%s EXIT", __FUNCTION__);
}

void AampRialtoPlayer::GetVideoSize(int &w, int &h)
{
	AAMPLOG_INFO("AampRialtoPlayer::%s ENTRY", __FUNCTION__);
	AAMPLOG_INFO("AampRialtoPlayer::%s EXIT w=%d h=%d", __FUNCTION__, w, h);
}

void AampRialtoPlayer::QueueProtectionEvent(
	const char *protSystemId,
	const void *ptr,
	size_t len,
	AampMediaType type)
{
	AAMPLOG_INFO("AampRialtoPlayer::%s ENTRY protSystemId=%s len=%zu type=%d",
		__FUNCTION__, protSystemId ? protSystemId : "(null)", len, static_cast<int>(type));
	AAMPLOG_INFO("AampRialtoPlayer::%s EXIT", __FUNCTION__);
}

void AampRialtoPlayer::ClearProtectionEvent()
{
	AAMPLOG_INFO("AampRialtoPlayer::%s ENTRY", __FUNCTION__);
	AAMPLOG_INFO("AampRialtoPlayer::%s EXIT", __FUNCTION__);
}

void AampRialtoPlayer::SignalTrickModeDiscontinuity()
{
	AAMPLOG_INFO("AampRialtoPlayer::%s ENTRY", __FUNCTION__);
	AAMPLOG_INFO("AampRialtoPlayer::%s EXIT", __FUNCTION__);
}

void AampRialtoPlayer::SeekStreamSink(double position, double rate)
{
	AAMPLOG_INFO("AampRialtoPlayer::%s ENTRY position=%f rate=%f", __FUNCTION__, position, rate);
	AAMPLOG_INFO("AampRialtoPlayer::%s EXIT", __FUNCTION__);
}

std::string AampRialtoPlayer::GetVideoRectangle()
{
	AAMPLOG_INFO("AampRialtoPlayer::%s ENTRY", __FUNCTION__);
	AAMPLOG_INFO("AampRialtoPlayer::%s EXIT", __FUNCTION__);
	return {};
}

void AampRialtoPlayer::StopBuffering(bool forceStop)
{
	AAMPLOG_INFO("AampRialtoPlayer::%s ENTRY forceStop=%d", __FUNCTION__, forceStop);
	AAMPLOG_INFO("AampRialtoPlayer::%s EXIT", __FUNCTION__);
}

bool AampRialtoPlayer::SetTextStyle(const std::string &options)
{
	AAMPLOG_INFO("AampRialtoPlayer::%s ENTRY options=%s", __FUNCTION__, options.c_str());
	AAMPLOG_INFO("AampRialtoPlayer::%s EXIT", __FUNCTION__);
	return false;
}

PlaybackQualityStruct *AampRialtoPlayer::GetVideoPlaybackQuality()
{
	AAMPLOG_INFO("AampRialtoPlayer::%s ENTRY", __FUNCTION__);
	AAMPLOG_INFO("AampRialtoPlayer::%s EXIT", __FUNCTION__);
	return nullptr;
}

bool AampRialtoPlayer::SignalSubtitleClock()
{
	AAMPLOG_INFO("AampRialtoPlayer::%s ENTRY", __FUNCTION__);
	AAMPLOG_INFO("AampRialtoPlayer::%s EXIT", __FUNCTION__);
	return false;
}

void AampRialtoPlayer::SetPauseOnStartPlayback(bool enable)
{
	AAMPLOG_INFO("AampRialtoPlayer::%s ENTRY enable=%d", __FUNCTION__, enable);
	AAMPLOG_INFO("AampRialtoPlayer::%s EXIT", __FUNCTION__);
}

void AampRialtoPlayer::NotifyInjectorToResume()
{
	AAMPLOG_INFO("AampRialtoPlayer::%s ENTRY", __FUNCTION__);
	AAMPLOG_INFO("AampRialtoPlayer::%s EXIT", __FUNCTION__);
}

void AampRialtoPlayer::NotifyInjectorToPause()
{
	AAMPLOG_INFO("AampRialtoPlayer::%s ENTRY", __FUNCTION__);
	AAMPLOG_INFO("AampRialtoPlayer::%s EXIT", __FUNCTION__);
}

void AampRialtoPlayer::SetStreamCaps(AampMediaType type, MediaCodecInfo &&codecInfo)
{
	AAMPLOG_INFO("AampRialtoPlayer::%s ENTRY type=%d", __FUNCTION__, static_cast<int>(type));
	AAMPLOG_INFO("AampRialtoPlayer::%s EXIT", __FUNCTION__);
}

bool AampRialtoPlayer::IsAssociatedAamp(PrivateInstanceAAMP *aampInstance)
{
	AAMPLOG_INFO("AampRialtoPlayer::%s ENTRY aampInstance=%p", __FUNCTION__, aampInstance);
	AAMPLOG_INFO("AampRialtoPlayer::%s EXIT", __FUNCTION__);
	return false;
}

void AampRialtoPlayer::ChangeAamp(PrivateInstanceAAMP *newAamp, id3_callback_t id3HandlerCallback)
{
	AAMPLOG_INFO("AampRialtoPlayer::%s ENTRY newAamp=%p", __FUNCTION__, newAamp);
	AAMPLOG_INFO("AampRialtoPlayer::%s EXIT", __FUNCTION__);
}

void AampRialtoPlayer::SetEncryptedAamp(PrivateInstanceAAMP *aamp)
{
	AAMPLOG_INFO("AampRialtoPlayer::%s ENTRY aamp=%p", __FUNCTION__, aamp);
	AAMPLOG_INFO("AampRialtoPlayer::%s EXIT", __FUNCTION__);
}

void AampRialtoPlayer::ResetFirstFrame()
{
	AAMPLOG_INFO("AampRialtoPlayer::%s ENTRY", __FUNCTION__);
	AAMPLOG_INFO("AampRialtoPlayer::%s EXIT", __FUNCTION__);
}

// ---------------------------------------------------------------------------
// Injection thread — lifecycle
// ---------------------------------------------------------------------------

void AampRialtoPlayer::StartInjectionThread()
{
	if (m_injectionThread.joinable())
	{
		AAMPLOG_WARN("AampRialtoPlayer::%s injection thread already"
			" running", __FUNCTION__);
	}
	else
	{
		m_stopInjection = false;
		m_injectionThread =
			std::thread(&AampRialtoPlayer::RunInjectionThread, this);
		AAMPLOG_INFO("AampRialtoPlayer::%s injection thread started",
			__FUNCTION__);
	}
}

void AampRialtoPlayer::StopInjectionThread()
{
	if (m_injectionThread.joinable())
	{
		{
			std::lock_guard<std::mutex> lock(m_injectorMutex);
			m_stopInjection = true;
		}
		m_injectorCv.notify_all();
		m_injectionThread.join();
		AAMPLOG_INFO("AampRialtoPlayer::%s injection thread stopped",
			__FUNCTION__);
	}
}

// ---------------------------------------------------------------------------
// Injection thread — main loop
// ---------------------------------------------------------------------------

void AampRialtoPlayer::RunInjectionThread()
{
	AAMPLOG_INFO("AampRialtoPlayer: injection thread running");

	while (true)
	{
		std::unique_lock<std::mutex> lock(m_injectorMutex);

		// Block until work is available or we are asked to stop.
		m_injectorCv.wait(lock, [this] {
			bool result = m_stopInjection;
			if (!result)
			{
				bool videoReady =
					!m_videoPendingReqs.empty() &&
					(!m_videoSampleQueue.empty() || m_videoEos);
				bool audioReady =
					!m_audioPendingReqs.empty() &&
					(!m_audioSampleQueue.empty() || m_audioEos);
				result = videoReady || audioReady;
			}
			return result;
		});

		if (m_stopInjection)
		{
			break;
		}

		// Drain all video requests that have data (or EOS) available.
		while (!m_stopInjection &&
		       !m_videoPendingReqs.empty() &&
		       (!m_videoSampleQueue.empty() || m_videoEos))
		{
			PendingNeedData req = m_videoPendingReqs.front();
			m_videoPendingReqs.pop_front();

			size_t toSend =
				std::min(req.frameCount,
				         m_videoSampleQueue.size());
			std::vector<AampMediaSample> toInject;
			toInject.reserve(toSend);
			for (size_t i = 0; i < toSend; ++i)
			{
				toInject.push_back(
					std::move(m_videoSampleQueue.front()));
				m_videoSampleQueue.pop_front();
			}
			bool eos = m_videoEos && m_videoSampleQueue.empty();

			lock.unlock();
			InjectSamples(m_videoSourceId, req.requestId,
				std::move(toInject), eos);
			lock.lock();
		}

		// Drain all audio requests that have data (or EOS) available.
		while (!m_stopInjection &&
		       !m_audioPendingReqs.empty() &&
		       (!m_audioSampleQueue.empty() || m_audioEos))
		{
			PendingNeedData req = m_audioPendingReqs.front();
			m_audioPendingReqs.pop_front();

			size_t toSend =
				std::min(req.frameCount,
				         m_audioSampleQueue.size());
			std::vector<AampMediaSample> toInject;
			toInject.reserve(toSend);
			for (size_t i = 0; i < toSend; ++i)
			{
				toInject.push_back(
					std::move(m_audioSampleQueue.front()));
				m_audioSampleQueue.pop_front();
			}
			bool eos = m_audioEos && m_audioSampleQueue.empty();

			lock.unlock();
			InjectSamples(m_audioSourceId, req.requestId,
				std::move(toInject), eos);
			lock.lock();
		}
	}

	AAMPLOG_INFO("AampRialtoPlayer: injection thread exiting");
}

// ---------------------------------------------------------------------------
// Injection thread — segment injection
// ---------------------------------------------------------------------------

void AampRialtoPlayer::InjectSamples(
	int32_t sourceId,
	uint32_t requestId,
	std::vector<AampMediaSample> &&samples,
	bool eos)
{
	if (!m_pipeline)
	{
		AAMPLOG_WARN("AampRialtoPlayer::%s pipeline is null,"
			" dropping %zu samples for sourceId=%d",
			__FUNCTION__, samples.size(), sourceId);
	}
	else
	{
		bool isVideo = (sourceId == m_videoSourceId);

		// samples must stay alive until haveData() returns because
		// MediaSegment::setData() stores a raw pointer into each sample's
		// AampGrowableBuffer (see Rialto data-lifetime contract).
		for (auto &sample : samples)
		{
			std::unique_ptr<firebolt::rialto::IMediaPipeline::MediaSegment>
				segment;

			if (isVideo)
			{
				segment = std::make_unique<
					firebolt::rialto::IMediaPipeline::MediaSegmentVideo>(
					sourceId,
					static_cast<int64_t>(sample.mPts * kNsPerSecond),
					static_cast<int64_t>(
						sample.mDuration * kNsPerSecond),
					m_videoWidth,
					m_videoHeight);
			}
			else
			{
				segment = std::make_unique<
					firebolt::rialto::IMediaPipeline::MediaSegmentAudio>(
					sourceId,
					static_cast<int64_t>(sample.mPts * kNsPerSecond),
					static_cast<int64_t>(
						sample.mDuration * kNsPerSecond),
					m_audioSampleRate,
					m_audioChannels);
			}

			segment->setData(
				static_cast<uint32_t>(sample.mData.size()),
				reinterpret_cast<const uint8_t *>(
					sample.mData.GetPtr()));

			auto addStatus =
				m_pipeline->addSegment(requestId, segment);
			if (addStatus != firebolt::rialto::AddSegmentStatus::OK)
			{
				AAMPLOG_WARN("AampRialtoPlayer::%s addSegment failed"
					" sourceId=%d requestId=%u status=%d",
					__FUNCTION__, sourceId, requestId,
					static_cast<int>(addStatus));
			}
		}

		// Signal haveData — data pointers inside samples remain valid here.
		firebolt::rialto::MediaSourceStatus haveDataStatus;
		if (samples.empty() && eos)
		{
			haveDataStatus = firebolt::rialto::MediaSourceStatus::EOS;
		}
		else if (samples.empty())
		{
			haveDataStatus =
				firebolt::rialto::MediaSourceStatus::NO_AVAILABLE_SAMPLES;
		}
		else
		{
			haveDataStatus = firebolt::rialto::MediaSourceStatus::OK;
		}

		if (!m_pipeline->haveData(haveDataStatus, requestId))
		{
			AAMPLOG_WARN("AampRialtoPlayer::%s haveData failed"
				" requestId=%u", __FUNCTION__, requestId);
		}

		AAMPLOG_INFO("AampRialtoPlayer::%s Injected %zu segments"
			" sourceId=%d requestId=%u eos=%d status=%d",
			__FUNCTION__, samples.size(), sourceId, requestId,
			eos, static_cast<int>(haveDataStatus));
	}
}

// ---------------------------------------------------------------------------
// Callbacks invoked from AampRialtoMediaPipelineClient
// ---------------------------------------------------------------------------

void AampRialtoPlayer::OnNeedMediaData(
	int32_t sourceId, size_t frameCount, uint32_t requestId)
{
	AAMPLOG_INFO("AampRialtoPlayer::%s sourceId=%d frameCount=%zu"
		" requestId=%u",
		__FUNCTION__, sourceId, frameCount, requestId);
	{
		std::lock_guard<std::mutex> lock(m_injectorMutex);
		PendingNeedData req{requestId, frameCount};
		if (sourceId == m_videoSourceId)
		{
			m_videoPendingReqs.push_back(req);
		}
		else if (sourceId == m_audioSourceId)
		{
			m_audioPendingReqs.push_back(req);
		}
		else
		{
			AAMPLOG_WARN("AampRialtoPlayer::%s unknown sourceId=%d",
				__FUNCTION__, sourceId);
		}
	}
	m_injectorCv.notify_one();
}

void AampRialtoPlayer::OnCancelNeedMediaData(int32_t sourceId)
{
	AAMPLOG_INFO("AampRialtoPlayer::%s sourceId=%d",
		__FUNCTION__, sourceId);
	std::lock_guard<std::mutex> lock(m_injectorMutex);
	if (sourceId == m_videoSourceId)
	{
		m_videoPendingReqs.clear();
	}
	else if (sourceId == m_audioSourceId)
	{
		m_audioPendingReqs.clear();
	}
}

#else
#include "aampgstplayer.h"

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

AampRialtoPlayer::AampRialtoPlayer(
	PrivateInstanceAAMP *aamp,
	id3_callback_t id3HandlerCallback,
	std::function<void(const unsigned char *, int, int, int)> exportFrames)
	: m_aamp(aamp)
	, mGstPlayer{std::make_unique<AAMPGstPlayer>(
		aamp,
		std::move(id3HandlerCallback),
		std::move(exportFrames))}
{
	AAMPLOG_INFO("AampRialtoPlayer: constructed, aamp=%p", aamp);
}

AampRialtoPlayer::~AampRialtoPlayer()
{
	AAMPLOG_INFO("AampRialtoPlayer: destroyed");
}

// ---------------------------------------------------------------------------
// StreamSink overrides
// ---------------------------------------------------------------------------

void AampRialtoPlayer::Configure(
	StreamOutputFormat format,
	StreamOutputFormat audioFormat,
	StreamOutputFormat subFormat,
	bool bESChangeStatus,
	bool setReadyAfterPipelineCreation)
{
	AAMPLOG_INFO("AampRialtoPlayer::%s ENTRY format=%d audioFormat=%d subFormat=%d"
		" bESChangeStatus=%d setReadyAfterPipelineCreation=%d",
		__FUNCTION__, static_cast<int>(format), static_cast<int>(audioFormat),
		static_cast<int>(subFormat), bESChangeStatus, setReadyAfterPipelineCreation);
	mGstPlayer->Configure(
		format, audioFormat, subFormat, bESChangeStatus, setReadyAfterPipelineCreation);
	AAMPLOG_INFO("AampRialtoPlayer::%s EXIT", __FUNCTION__);
}

bool AampRialtoPlayer::SendCopy(
	AampMediaType mediaType,
	std::vector<uint8_t> &&buffer,
	double fpts,
	double fdts,
	double fDuration)
{
	AAMPLOG_INFO("AampRialtoPlayer::%s ENTRY mediaType=%d bufferSize=%zu fpts=%f fdts=%f fDuration=%f",
		__FUNCTION__, static_cast<int>(mediaType), buffer.size(), fpts, fdts, fDuration);
	bool result = mGstPlayer->SendCopy(mediaType, std::move(buffer), fpts, fdts, fDuration);
	AAMPLOG_INFO("AampRialtoPlayer::%s EXIT result=%d", __FUNCTION__, result);
	return result;
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
	AAMPLOG_INFO("AampRialtoPlayer::%s ENTRY mediaType=%d bufferSize=%zu fpts=%f fdts=%f"
		" fDuration=%f fragmentPTSoffset=%f initFragment=%d discontinuity=%d",
		__FUNCTION__, static_cast<int>(mediaType), buffer.size(), fpts, fdts,
		fDuration, fragmentPTSoffset, initFragment, discontinuity);
	bool result = mGstPlayer->SendTransfer(
		mediaType, std::move(buffer), fpts, fdts, fDuration,
		fragmentPTSoffset, initFragment, discontinuity);
	AAMPLOG_INFO("AampRialtoPlayer::%s EXIT result=%d", __FUNCTION__, result);
	return result;
}

bool AampRialtoPlayer::SendSample(AampMediaType mediaType, AampMediaSample &sample)
{
	AAMPLOG_INFO("AampRialtoPlayer::%s ENTRY mediaType=%d", __FUNCTION__, static_cast<int>(mediaType));
	bool result = mGstPlayer->SendSample(mediaType, sample);
	AAMPLOG_INFO("AampRialtoPlayer::%s EXIT result=%d", __FUNCTION__, result);
	return result;
}

bool AampRialtoPlayer::PipelineConfiguredForMedia(AampMediaType type)
{
	AAMPLOG_INFO("AampRialtoPlayer::%s ENTRY type=%d", __FUNCTION__, static_cast<int>(type));
	bool result = mGstPlayer->PipelineConfiguredForMedia(type);
	AAMPLOG_INFO("AampRialtoPlayer::%s EXIT result=%d", __FUNCTION__, result);
	return result;
}

void AampRialtoPlayer::EndOfStreamReached(AampMediaType type)
{
	AAMPLOG_INFO("AampRialtoPlayer::%s ENTRY type=%d", __FUNCTION__, static_cast<int>(type));
	mGstPlayer->EndOfStreamReached(type);
	AAMPLOG_INFO("AampRialtoPlayer::%s EXIT", __FUNCTION__);
}

void AampRialtoPlayer::Stream()
{
	AAMPLOG_INFO("AampRialtoPlayer::%s ENTRY", __FUNCTION__);
	mGstPlayer->Stream();
	AAMPLOG_INFO("AampRialtoPlayer::%s EXIT", __FUNCTION__);
}

void AampRialtoPlayer::Stop(bool keepLastFrame)
{
	AAMPLOG_INFO("AampRialtoPlayer::%s ENTRY keepLastFrame=%d", __FUNCTION__, keepLastFrame);
	mGstPlayer->Stop(keepLastFrame);
	AAMPLOG_INFO("AampRialtoPlayer::%s EXIT", __FUNCTION__);
}

void AampRialtoPlayer::Flush(double position, int rate, bool shouldTearDown)
{
	AAMPLOG_INFO("AampRialtoPlayer::%s ENTRY position=%f rate=%d shouldTearDown=%d",
		__FUNCTION__, position, rate, shouldTearDown);
	mGstPlayer->Flush(position, rate, shouldTearDown);
	AAMPLOG_INFO("AampRialtoPlayer::%s EXIT", __FUNCTION__);
}

void AampRialtoPlayer::FlushTrack(AampMediaType mediaType, double position)
{
	AAMPLOG_INFO("AampRialtoPlayer::%s ENTRY mediaType=%d position=%f",
		__FUNCTION__, static_cast<int>(mediaType), position);
	mGstPlayer->FlushTrack(mediaType, position);
	AAMPLOG_INFO("AampRialtoPlayer::%s EXIT", __FUNCTION__);
}

bool AampRialtoPlayer::SetPlayBackRate(double rate)
{
	AAMPLOG_INFO("AampRialtoPlayer::%s ENTRY rate=%f", __FUNCTION__, rate);
	bool result = mGstPlayer->SetPlayBackRate(rate);
	AAMPLOG_INFO("AampRialtoPlayer::%s EXIT result=%d", __FUNCTION__, result);
	return result;
}

bool AampRialtoPlayer::Pause(bool pause, bool forceStopGstreamerPreBuffering)
{
	AAMPLOG_INFO("AampRialtoPlayer::%s ENTRY pause=%d forceStopGstreamerPreBuffering=%d",
		__FUNCTION__, pause, forceStopGstreamerPreBuffering);
	bool result = mGstPlayer->Pause(pause, forceStopGstreamerPreBuffering);
	AAMPLOG_INFO("AampRialtoPlayer::%s EXIT result=%d", __FUNCTION__, result);
	return result;
}

long AampRialtoPlayer::GetDurationMilliseconds()
{
	AAMPLOG_INFO("AampRialtoPlayer::%s ENTRY", __FUNCTION__);
	long result = mGstPlayer->GetDurationMilliseconds();
	AAMPLOG_INFO("AampRialtoPlayer::%s EXIT result=%ld", __FUNCTION__, result);
	return result;
}

long long AampRialtoPlayer::GetPositionMilliseconds()
{
	AAMPLOG_INFO("AampRialtoPlayer::%s ENTRY", __FUNCTION__);
	long long result = mGstPlayer->GetPositionMilliseconds();
	AAMPLOG_INFO("AampRialtoPlayer::%s EXIT result=%lld", __FUNCTION__, result);
	return result;
}

long long AampRialtoPlayer::GetVideoPTS()
{
	AAMPLOG_INFO("AampRialtoPlayer::%s ENTRY", __FUNCTION__);
	long long result = mGstPlayer->GetVideoPTS();
	AAMPLOG_INFO("AampRialtoPlayer::%s EXIT result=%lld", __FUNCTION__, result);
	return result;
}

void AampRialtoPlayer::SetVideoRectangle(int x, int y, int w, int h)
{
	AAMPLOG_INFO("AampRialtoPlayer::%s ENTRY x=%d y=%d w=%d h=%d", __FUNCTION__, x, y, w, h);
	mGstPlayer->SetVideoRectangle(x, y, w, h);
	AAMPLOG_INFO("AampRialtoPlayer::%s EXIT", __FUNCTION__);
}

void AampRialtoPlayer::SetVideoZoom(VideoZoomMode zoom)
{
	AAMPLOG_INFO("AampRialtoPlayer::%s ENTRY zoom=%d", __FUNCTION__, static_cast<int>(zoom));
	mGstPlayer->SetVideoZoom(zoom);
	AAMPLOG_INFO("AampRialtoPlayer::%s EXIT", __FUNCTION__);
}

void AampRialtoPlayer::SetVideoMute(bool muted)
{
	AAMPLOG_INFO("AampRialtoPlayer::%s ENTRY muted=%d", __FUNCTION__, muted);
	mGstPlayer->SetVideoMute(muted);
	AAMPLOG_INFO("AampRialtoPlayer::%s EXIT", __FUNCTION__);
}

void AampRialtoPlayer::SetSubtitleMute(bool muted)
{
	AAMPLOG_INFO("AampRialtoPlayer::%s ENTRY muted=%d", __FUNCTION__, muted);
	mGstPlayer->SetSubtitleMute(muted);
	AAMPLOG_INFO("AampRialtoPlayer::%s EXIT", __FUNCTION__);
}

void AampRialtoPlayer::SetSubtitlePtsOffset(std::uint64_t pts_offset)
{
	AAMPLOG_INFO("AampRialtoPlayer::%s ENTRY pts_offset=%" PRIu64, __FUNCTION__, pts_offset);
	mGstPlayer->SetSubtitlePtsOffset(pts_offset);
	AAMPLOG_INFO("AampRialtoPlayer::%s EXIT", __FUNCTION__);
}

void AampRialtoPlayer::SetAudioVolume(int volume)
{
	AAMPLOG_INFO("AampRialtoPlayer::%s ENTRY volume=%d", __FUNCTION__, volume);
	mGstPlayer->SetAudioVolume(volume);
	AAMPLOG_INFO("AampRialtoPlayer::%s EXIT", __FUNCTION__);
}

bool AampRialtoPlayer::Discontinuity(AampMediaType mediaType)
{
	AAMPLOG_INFO("AampRialtoPlayer::%s ENTRY mediaType=%d", __FUNCTION__, static_cast<int>(mediaType));
	bool result = mGstPlayer->Discontinuity(mediaType);
	AAMPLOG_INFO("AampRialtoPlayer::%s EXIT result=%d", __FUNCTION__, result);
	return result;
}

bool AampRialtoPlayer::CheckForPTSChangeWithTimeout(long timeout)
{
	AAMPLOG_INFO("AampRialtoPlayer::%s ENTRY timeout=%ld", __FUNCTION__, timeout);
	bool result = mGstPlayer->CheckForPTSChangeWithTimeout(timeout);
	AAMPLOG_INFO("AampRialtoPlayer::%s EXIT result=%d", __FUNCTION__, result);
	return result;
}

bool AampRialtoPlayer::IsCacheEmpty(AampMediaType mediaType)
{
	AAMPLOG_INFO("AampRialtoPlayer::%s ENTRY mediaType=%d", __FUNCTION__, static_cast<int>(mediaType));
	bool result = mGstPlayer->IsCacheEmpty(mediaType);
	AAMPLOG_INFO("AampRialtoPlayer::%s EXIT result=%d", __FUNCTION__, result);
	return result;
}

void AampRialtoPlayer::ResetEOSSignalledFlag()
{
	AAMPLOG_INFO("AampRialtoPlayer::%s ENTRY", __FUNCTION__);
	mGstPlayer->ResetEOSSignalledFlag();
	AAMPLOG_INFO("AampRialtoPlayer::%s EXIT", __FUNCTION__);
}

void AampRialtoPlayer::NotifyFragmentCachingComplete()
{
	AAMPLOG_INFO("AampRialtoPlayer::%s ENTRY", __FUNCTION__);
	mGstPlayer->NotifyFragmentCachingComplete();
	AAMPLOG_INFO("AampRialtoPlayer::%s EXIT", __FUNCTION__);
}

void AampRialtoPlayer::NotifyFragmentCachingOngoing()
{
	AAMPLOG_INFO("AampRialtoPlayer::%s ENTRY", __FUNCTION__);
	mGstPlayer->NotifyFragmentCachingOngoing();
	AAMPLOG_INFO("AampRialtoPlayer::%s EXIT", __FUNCTION__);
}

void AampRialtoPlayer::GetVideoSize(int &w, int &h)
{
	AAMPLOG_INFO("AampRialtoPlayer::%s ENTRY", __FUNCTION__);
	mGstPlayer->GetVideoSize(w, h);
	AAMPLOG_INFO("AampRialtoPlayer::%s EXIT w=%d h=%d", __FUNCTION__, w, h);
}

void AampRialtoPlayer::QueueProtectionEvent(
	const char *protSystemId,
	const void *ptr,
	size_t len,
	AampMediaType type)
{
	AAMPLOG_INFO("AampRialtoPlayer::%s ENTRY protSystemId=%s len=%zu type=%d",
		__FUNCTION__, protSystemId ? protSystemId : "(null)", len, static_cast<int>(type));
	mGstPlayer->QueueProtectionEvent(protSystemId, ptr, len, type);
	AAMPLOG_INFO("AampRialtoPlayer::%s EXIT", __FUNCTION__);
}

void AampRialtoPlayer::ClearProtectionEvent()
{
	AAMPLOG_INFO("AampRialtoPlayer::%s ENTRY", __FUNCTION__);
	mGstPlayer->ClearProtectionEvent();
	AAMPLOG_INFO("AampRialtoPlayer::%s EXIT", __FUNCTION__);
}

void AampRialtoPlayer::SignalTrickModeDiscontinuity()
{
	AAMPLOG_INFO("AampRialtoPlayer::%s ENTRY", __FUNCTION__);
	mGstPlayer->SignalTrickModeDiscontinuity();
	AAMPLOG_INFO("AampRialtoPlayer::%s EXIT", __FUNCTION__);
}

void AampRialtoPlayer::SeekStreamSink(double position, double rate)
{
	AAMPLOG_INFO("AampRialtoPlayer::%s ENTRY position=%f rate=%f", __FUNCTION__, position, rate);
	mGstPlayer->SeekStreamSink(position, rate);
	AAMPLOG_INFO("AampRialtoPlayer::%s EXIT", __FUNCTION__);
}

std::string AampRialtoPlayer::GetVideoRectangle()
{
	AAMPLOG_INFO("AampRialtoPlayer::%s ENTRY", __FUNCTION__);
	std::string result = mGstPlayer->GetVideoRectangle();
	AAMPLOG_INFO("AampRialtoPlayer::%s EXIT result=%s", __FUNCTION__, result.c_str());
	return result;
}

void AampRialtoPlayer::StopBuffering(bool forceStop)
{
	AAMPLOG_INFO("AampRialtoPlayer::%s ENTRY forceStop=%d", __FUNCTION__, forceStop);
	mGstPlayer->StopBuffering(forceStop);
	AAMPLOG_INFO("AampRialtoPlayer::%s EXIT", __FUNCTION__);
}

bool AampRialtoPlayer::SetTextStyle(const std::string &options)
{
	AAMPLOG_INFO("AampRialtoPlayer::%s ENTRY options=%s", __FUNCTION__, options.c_str());
	bool result = mGstPlayer->SetTextStyle(options);
	AAMPLOG_INFO("AampRialtoPlayer::%s EXIT result=%d", __FUNCTION__, result);
	return result;
}

PlaybackQualityStruct *AampRialtoPlayer::GetVideoPlaybackQuality()
{
	AAMPLOG_INFO("AampRialtoPlayer::%s ENTRY", __FUNCTION__);
	PlaybackQualityStruct *result = mGstPlayer->GetVideoPlaybackQuality();
	AAMPLOG_INFO("AampRialtoPlayer::%s EXIT result=%p", __FUNCTION__, static_cast<void*>(result));
	return result;
}

bool AampRialtoPlayer::SignalSubtitleClock()
{
	AAMPLOG_INFO("AampRialtoPlayer::%s ENTRY", __FUNCTION__);
	bool result = mGstPlayer->SignalSubtitleClock();
	AAMPLOG_INFO("AampRialtoPlayer::%s EXIT result=%d", __FUNCTION__, result);
	return result;
}

void AampRialtoPlayer::SetPauseOnStartPlayback(bool enable)
{
	AAMPLOG_INFO("AampRialtoPlayer::%s ENTRY enable=%d", __FUNCTION__, enable);
	mGstPlayer->SetPauseOnStartPlayback(enable);
	AAMPLOG_INFO("AampRialtoPlayer::%s EXIT", __FUNCTION__);
}

void AampRialtoPlayer::NotifyInjectorToResume()
{
	AAMPLOG_INFO("AampRialtoPlayer::%s ENTRY", __FUNCTION__);
	mGstPlayer->NotifyInjectorToResume();
	AAMPLOG_INFO("AampRialtoPlayer::%s EXIT", __FUNCTION__);
}

void AampRialtoPlayer::NotifyInjectorToPause()
{
	AAMPLOG_INFO("AampRialtoPlayer::%s ENTRY", __FUNCTION__);
	mGstPlayer->NotifyInjectorToPause();
	AAMPLOG_INFO("AampRialtoPlayer::%s EXIT", __FUNCTION__);
}

void AampRialtoPlayer::SetStreamCaps(AampMediaType type, MediaCodecInfo &&codecInfo)
{
	AAMPLOG_INFO("AampRialtoPlayer::%s ENTRY type=%d", __FUNCTION__, static_cast<int>(type));
	mGstPlayer->SetStreamCaps(type, std::move(codecInfo));
	AAMPLOG_INFO("AampRialtoPlayer::%s EXIT", __FUNCTION__);
}

bool AampRialtoPlayer::IsAssociatedAamp(PrivateInstanceAAMP *aampInstance)
{
	AAMPLOG_INFO("AampRialtoPlayer::%s ENTRY aampInstance=%p", __FUNCTION__, aampInstance);
	bool result = mGstPlayer->IsAssociatedAamp(aampInstance);
	AAMPLOG_INFO("AampRialtoPlayer::%s EXIT result=%d", __FUNCTION__, result);
	return result;
}

void AampRialtoPlayer::ChangeAamp(PrivateInstanceAAMP *newAamp, id3_callback_t id3HandlerCallback)
{
	AAMPLOG_INFO("AampRialtoPlayer::%s ENTRY newAamp=%p", __FUNCTION__, newAamp);
	mGstPlayer->ChangeAamp(newAamp, std::move(id3HandlerCallback));
	AAMPLOG_INFO("AampRialtoPlayer::%s EXIT", __FUNCTION__);
}

void AampRialtoPlayer::SetEncryptedAamp(PrivateInstanceAAMP *aamp)
{
	AAMPLOG_INFO("AampRialtoPlayer::%s ENTRY aamp=%p", __FUNCTION__, aamp);
	mGstPlayer->SetEncryptedAamp(aamp);
	AAMPLOG_INFO("AampRialtoPlayer::%s EXIT", __FUNCTION__);
}

void AampRialtoPlayer::ResetFirstFrame()
{
	AAMPLOG_INFO("AampRialtoPlayer::%s ENTRY", __FUNCTION__);
	mGstPlayer->ResetFirstFrame();
	AAMPLOG_INFO("AampRialtoPlayer::%s EXIT", __FUNCTION__);
}

#endif