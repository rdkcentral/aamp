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
 * @file AampRialtoAudioSource.cpp
 * @brief Audio-specific Rialto media source implementation.
 */

#include "AampRialtoAudioSource.h"
#include "AampLogManager.h"

// ---------------------------------------------------------------------------
// mapCodecToMime
// ---------------------------------------------------------------------------

bool AampRialtoAudioSource::mapCodecToMime(
	GstStreamOutputFormat codecFormat,
	std::string &mimeType,
	firebolt::rialto::StreamFormat &streamFormat) const
{
	switch (codecFormat)
	{
		case GST_FORMAT_AUDIO_ES_AAC_RAW:
			mimeType     = "audio/aac";
			streamFormat = firebolt::rialto::StreamFormat::RAW;
			return true;
		case GST_FORMAT_AUDIO_ES_EC3:
			mimeType     = "audio/x-eac3";
			streamFormat = firebolt::rialto::StreamFormat::UNDEFINED;
			return true;
		case GST_FORMAT_AUDIO_ES_AC4:
			mimeType     = "audio/x-ac4";
			streamFormat = firebolt::rialto::StreamFormat::UNDEFINED;
			return true;
		case GST_FORMAT_AUDIO_ES_AAC:
			// HLS-TS ES path: ADTS AAC (audio/mpeg mpegversion=2
			// stream-format=adts).
			mimeType     = "audio/mp4";
			streamFormat = firebolt::rialto::StreamFormat::UNDEFINED;
			return true;
		default:
			return false;
	}
}

// ---------------------------------------------------------------------------
// createRialtoSource
// ---------------------------------------------------------------------------

std::unique_ptr<firebolt::rialto::IMediaPipeline::MediaSource>
AampRialtoAudioSource::createRialtoSource(
	const std::string &mimeType,
	bool hasDrm,
	const MediaCodecInfo &codecInfo,
	firebolt::rialto::StreamFormat streamFormat,
	std::shared_ptr<firebolt::rialto::CodecData> codecData) const
{
	firebolt::rialto::AudioConfig audioConfig;
	audioConfig.numberOfChannels = codecInfo.mInfo.audio.mChannelCount;
	audioConfig.sampleRate       = codecInfo.mInfo.audio.mSampleRate;
	if (!codecInfo.mCodecData.empty())
	{
		audioConfig.codecSpecificConfig = codecInfo.mCodecData;
	}

	return std::make_unique<
		firebolt::rialto::IMediaPipeline::MediaSourceAudio>(
		mimeType,
		hasDrm,
		audioConfig,
		firebolt::rialto::SegmentAlignment::UNDEFINED,
		streamFormat,
		codecData);
}

// ---------------------------------------------------------------------------
// updateCachedMetadata
// ---------------------------------------------------------------------------

void AampRialtoAudioSource::updateCachedMetadata(
	const MediaCodecInfo &codecInfo)
{
	m_sampleRate = static_cast<int32_t>(codecInfo.mInfo.audio.mSampleRate);
	m_channels   = static_cast<int32_t>(codecInfo.mInfo.audio.mChannelCount);

	if (codecInfo.mCodecData.empty())
	{
		AAMPLOG_WARN("audio codecData is empty — Rialto may produce "
			"empty caps");
	}
	else
	{
		AAMPLOG_INFO("audio metadata updated rate=%d ch=%d codecData=%zu",
			m_sampleRate, m_channels, codecInfo.mCodecData.size());
	}
}

// ---------------------------------------------------------------------------
// createSegment
// ---------------------------------------------------------------------------

std::unique_ptr<firebolt::rialto::IMediaPipeline::MediaSegment>
AampRialtoAudioSource::createSegment(
	const AampMediaSample &sample) const
{
	const int64_t ptsNs      = static_cast<int64_t>(sample.mPts      * kNsPerSecond);
	const int64_t durationNs = static_cast<int64_t>(sample.mDuration * kNsPerSecond);
	return std::make_unique<
		firebolt::rialto::IMediaPipeline::MediaSegmentAudio>(
		m_sourceId, ptsNs, durationNs, m_sampleRate, m_channels);
}
