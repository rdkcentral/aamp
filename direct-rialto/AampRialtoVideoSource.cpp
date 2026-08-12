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
 * @file AampRialtoVideoSource.cpp
 * @brief Video-specific Rialto media source implementation.
 */

#include "AampRialtoVideoSource.h"
#include "AampLogManager.h"

// ---------------------------------------------------------------------------
// mapCodecToMime
// ---------------------------------------------------------------------------

bool AampRialtoVideoSource::mapCodecToMime(
	GstStreamOutputFormat codecFormat,
	bool naluLengthPrefixed,
	std::string &mimeType,
	firebolt::rialto::StreamFormat &streamFormat) const
{
	bool result = false;

	switch (codecFormat)
	{
		case GST_FORMAT_VIDEO_ES_H264:
			mimeType = "video/h264";
			// Length-prefixed (AVCC) vs Annex-B (byte-stream) is a property
			// of how the demuxer produced this codecInfo, not of which
			// object instance happens to own an Mp4Demux.
			streamFormat = naluLengthPrefixed
				? firebolt::rialto::StreamFormat::AVC
				: firebolt::rialto::StreamFormat::BYTE_STREAM;
			result = true;
			break;
		case GST_FORMAT_VIDEO_ES_HEVC:
			mimeType     = "video/h265";
			streamFormat = firebolt::rialto::StreamFormat::HVC1;
			result = true;
			break;
		default:
			break;
	}

	return result;
}

// ---------------------------------------------------------------------------
// createRialtoSource
// ---------------------------------------------------------------------------

std::unique_ptr<firebolt::rialto::IMediaPipeline::MediaSource>
AampRialtoVideoSource::createRialtoSource(
	const std::string &mimeType,
	bool hasDrm,
	const MediaCodecInfo &codecInfo,
	firebolt::rialto::StreamFormat streamFormat,
	std::shared_ptr<firebolt::rialto::CodecData> codecData) const
{
	// Hard-coded true regardless of the incoming hasDrm arg: the non-Direct-Rialto
	// path (InterfacePlayerRDK::SetupStream) sets has-drm=false on the video sink
	// only for eGST_MEDIAFORMAT_HLS (TS-based HLS), and true otherwise/always for
	// audio. Forcing true here unconditionally avoided a Rialto Server crash seen
	// during testing. Revisit and mirror that HLS-TS exception if/when we see an
	// actual issue with clear HLS-TS streams under Direct Rialto.
	return std::make_unique<
		firebolt::rialto::IMediaPipeline::MediaSourceVideo>(
		mimeType,
		/* hasDrm */ true,
		static_cast<int32_t>(codecInfo.mInfo.video.mWidth),
		static_cast<int32_t>(codecInfo.mInfo.video.mHeight),
		firebolt::rialto::SegmentAlignment::AU,
		streamFormat,
		codecData);
}

// ---------------------------------------------------------------------------
// updateCachedMetadata
// ---------------------------------------------------------------------------

void AampRialtoVideoSource::updateCachedMetadata(
	const MediaCodecInfo &codecInfo)
{
	m_width  = static_cast<int32_t>(codecInfo.mInfo.video.mWidth);
	m_height = static_cast<int32_t>(codecInfo.mInfo.video.mHeight);
	AAMPLOG_INFO("video metadata updated w=%d h=%d", m_width, m_height);
}

// ---------------------------------------------------------------------------
// createSegment
// ---------------------------------------------------------------------------

std::unique_ptr<firebolt::rialto::IMediaPipeline::MediaSegment>
AampRialtoVideoSource::createSegment(
	const AampMediaSample &sample) const
{
	const int64_t ptsNs      = static_cast<int64_t>(sample.mPts      * kNsPerSecond);
	const int64_t durationNs = static_cast<int64_t>(sample.mDuration * kNsPerSecond);
	return std::make_unique<
		firebolt::rialto::IMediaPipeline::MediaSegmentVideo>(
		m_sourceId, ptsNs, durationNs, m_width, m_height);
}
