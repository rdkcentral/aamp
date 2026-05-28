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
 * @file AampRialtoSubtitleSource.cpp
 * @brief Subtitle-specific Rialto media source implementation.
 */

#include "AampRialtoSubtitleSource.h"
#include "AampLogManager.h"
#include "middleware/GstUtils.h"
#include <cinttypes>

// ---------------------------------------------------------------------------
// mapCodecToMime
// ---------------------------------------------------------------------------

bool AampRialtoSubtitleSource::mapCodecToMime(
	GstStreamOutputFormat codecFormat,
	std::string &mimeType,
	firebolt::rialto::StreamFormat &streamFormat) const
{
	streamFormat = firebolt::rialto::StreamFormat::UNDEFINED;

	switch (codecFormat)
	{
		case GST_FORMAT_SUBTITLE_TTML:
		case GST_FORMAT_SUBTITLE_MP4:
			mimeType = "text/ttml";
			return true;
		case GST_FORMAT_SUBTITLE_WEBVTT:
			mimeType = "text/vtt";
			return true;
		default:
			break;
	}

	// Fallback: use the format set at Configure() time via setSubtitleFormat().
	switch (m_subtitleFormat)
	{
		case FORMAT_SUBTITLE_MP4:
		case FORMAT_SUBTITLE_TTML:
			mimeType = "text/ttml";
			return true;
		case FORMAT_SUBTITLE_WEBVTT:
			mimeType = "text/vtt";
			return true;
		default:
			AAMPLOG_WARN("Subtitle: unrecognised codecFormat=%d subtitleFormat=%d",
				static_cast<int>(codecFormat),
				static_cast<int>(m_subtitleFormat));
			return false;
	}
}

// ---------------------------------------------------------------------------
// createRialtoSource
// ---------------------------------------------------------------------------

std::unique_ptr<firebolt::rialto::IMediaPipeline::MediaSource>
AampRialtoSubtitleSource::createRialtoSource(
	const std::string &mimeType,
	bool /*hasDrm*/,
	const MediaCodecInfo & /*codecInfo*/,
	firebolt::rialto::StreamFormat /*streamFormat*/,
	std::shared_ptr<firebolt::rialto::CodecData> /*codecData*/) const
{
	return std::make_unique<
		firebolt::rialto::IMediaPipeline::MediaSourceSubtitle>(
		mimeType, /*textTrackIdentifier=*/"");
}

// ---------------------------------------------------------------------------
// updateCachedMetadata
// ---------------------------------------------------------------------------

void AampRialtoSubtitleSource::updateCachedMetadata(
	const MediaCodecInfo &codecInfo)
{
	// For FORMAT_SUBTITLE_MP4 streams the codec format is discovered by
	// the external AampMp4Demuxer (not processInitFragment, which is only
	// exercised via the SendTransfer path that subtitle never uses).
	// This is the only reliable point at which we know the inner codec;
	// set m_applyTextTransform so that refineDisplayOffset can compute
	// the per-buffer displayOffset used by setDisplayOffset() in injectOneSample.
	const auto fmt = codecInfo.mCodecFormat;
	const bool apply = (fmt == GST_FORMAT_SUBTITLE_TTML ||
	                    fmt == GST_FORMAT_SUBTITLE_MP4);
	if (apply != m_applyTextTransform)
	{
		m_applyTextTransform = apply;
		AAMPLOG_INFO("Subtitle text transform %s for codecFormat=%d",
			apply ? "enabled" : "disabled",
			static_cast<int>(fmt));
	}
}

// ---------------------------------------------------------------------------
// createSegment
// ---------------------------------------------------------------------------

std::unique_ptr<firebolt::rialto::IMediaPipeline::MediaSegment>
AampRialtoSubtitleSource::createSegment(
	const AampMediaSample &sample) const
{
	const int64_t ptsNs =
		static_cast<int64_t>(sample.mPts * kNsPerSecond);
	const int64_t durationNs =
		static_cast<int64_t>(sample.mDuration * kNsPerSecond);
	return std::make_unique<firebolt::rialto::IMediaPipeline::MediaSegment>(
		m_sourceId,
		firebolt::rialto::MediaSourceType::SUBTITLE,
		ptsNs,
		durationNs);
}

// ---------------------------------------------------------------------------
// processInitFragment
// ---------------------------------------------------------------------------

std::optional<MediaCodecInfo> AampRialtoSubtitleSource::processInitFragment(
	std::shared_ptr<std::vector<uint8_t>> buffer)
{
	// Reset AampTextTransform so that content type is re-detected from
	// the new stream (channel changes and seeks).
	m_textTransform.reset();
	m_applyTextTransform = false;

	std::optional<MediaCodecInfo> result;

	if (m_subtitleFormat == FORMAT_SUBTITLE_TTML ||
	    m_subtitleFormat == FORMAT_SUBTITLE_WEBVTT)
	{
		// Raw subtitle: synthesise codec info so attachOrUpdate() can
		// call mapCodecToMime() and attach the subtitle source.
		MediaCodecInfo ci{};
		ci.mCodecFormat = (m_subtitleFormat == FORMAT_SUBTITLE_TTML)
		                  ? GST_FORMAT_SUBTITLE_TTML
		                  : GST_FORMAT_SUBTITLE_WEBVTT;
		AAMPLOG_INFO("Raw subtitle processInitFragment: synthesised "
			"codecFormat=%d for subtitleFormat=%d",
			static_cast<int>(ci.mCodecFormat),
			static_cast<int>(m_subtitleFormat));
		result = std::optional<MediaCodecInfo>(std::move(ci));
	}
	else
	{
		// FORMAT_SUBTITLE_MP4: demux the init segment via the base class.
		result = AampRialtoMediaSource::processInitFragment(std::move(buffer));
	}

	// Enable text transform for TTML codecs (raw TTML or stpp-in-MP4).
	// WebVTT — whether raw or wvtt-in-MP4 — does not need correction
	// because cue timestamps are relative to segment start and
	// AAMP\'s fragmentPTSoffset is sufficient.
	if (result.has_value())
	{
		const auto fmt = result->mCodecFormat;
		m_applyTextTransform = (fmt == GST_FORMAT_SUBTITLE_TTML ||
		                        fmt == GST_FORMAT_SUBTITLE_MP4);
		AAMPLOG_INFO("Subtitle text transform %s for codecFormat=%d",
			m_applyTextTransform ? "enabled" : "disabled",
			static_cast<int>(fmt));
	}

	return result;
}

// ---------------------------------------------------------------------------
// refineDisplayOffset
// ---------------------------------------------------------------------------

int64_t AampRialtoSubtitleSource::refineDisplayOffset(
	const AampMediaSample &sample, int64_t displayOffsetMs)
{
	if (!m_applyTextTransform || !sample.mData || sample.mDataSize == 0)
	{
		return displayOffsetMs;
	}

	const int64_t ptsMs      = static_cast<int64_t>(sample.mPts      * 1000.0);
	const int64_t durationMs = static_cast<int64_t>(sample.mDuration * 1000.0);

	return m_textTransform.compute(
		sample.mData.get(), sample.mDataSize,
		ptsMs, displayOffsetMs, durationMs);
}

