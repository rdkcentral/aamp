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
#include <cinttypes>
#include <utility>

// ---------------------------------------------------------------------------
// mapCodecToMime
// ---------------------------------------------------------------------------

bool AampRialtoSubtitleSource::mapCodecToMime(
	GstStreamOutputFormat codecFormat,
	bool /*naluLengthPrefixed*/,
	std::string &mimeType,
	firebolt::rialto::StreamFormat &streamFormat) const
{
	bool result = false;
	streamFormat = firebolt::rialto::StreamFormat::UNDEFINED;

	switch (codecFormat)
	{
		case GST_FORMAT_SUBTITLE_TTML:
		case GST_FORMAT_SUBTITLE_MP4:
			mimeType = "text/ttml";
			result = true;
			break;
		case GST_FORMAT_SUBTITLE_WEBVTT:
			mimeType = "text/vtt";
			result = true;
			break;
		case GST_FORMAT_UNKNOWN:
			mimeType = "text/cc";
			streamFormat = firebolt::rialto::StreamFormat::RAW;
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
AampRialtoSubtitleSource::createRialtoSource(
	const std::string &mimeType,
	bool /*hasDrm*/,
	const MediaCodecInfo & /*codecInfo*/,
	firebolt::rialto::StreamFormat /*streamFormat*/,
	std::shared_ptr<firebolt::rialto::CodecData> /*codecData*/) const
{
	// For inband CC sources supply the default text-track identifier so the
	// Rialto server starts rendering CC1 immediately.
	const std::string textTrackId = "CC1";
	return std::make_unique<
		firebolt::rialto::IMediaPipeline::MediaSourceSubtitle>(
		mimeType, textTrackId);
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
	// set m_applyTextTransform so that refineDisplayOffset (called from
	// createSegment) can compute the per-buffer displayOffset used by
	// segment->setDisplayOffset().
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
	const int64_t ptsNs      = SecondsToNs(sample.mPts);
	const int64_t durationNs = SecondsToNs(sample.mDuration);
	auto segment = std::make_unique<firebolt::rialto::IMediaPipeline::MediaSegment>(
		sourceId(),
		firebolt::rialto::MediaSourceType::SUBTITLE,
		ptsNs,
		durationNs);

	const int64_t displayOffsetMs =
		refineDisplayOffset(sample);
	if (displayOffsetMs != 0)
	{
		AAMPLOG_INFO("subtitle setDisplayOffset sourceId=%d displayOffsetMs=%" PRId64,
			sourceId(), displayOffsetMs);
		// Rialto's field is unsigned but the server reads it back as signed.
		segment->setDisplayOffset(static_cast<uint64_t>(displayOffsetMs));
	}

	return segment;
}

// ---------------------------------------------------------------------------
// processInitFragment
// ---------------------------------------------------------------------------

std::optional<MediaCodecInfo> AampRialtoSubtitleSource::processInitFragment(
	std::shared_ptr<std::vector<uint8_t>> buffer)
{
	// Reset TTML offset detection state so that content type is
	// re-detected from the new stream (channel changes and seeks).
	m_ttmlOffset.Reset();
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
// processDataFragment
// ---------------------------------------------------------------------------

bool AampRialtoSubtitleSource::processDataFragment(
	firebolt::rialto::IMediaPipeline &pipeline,
	std::shared_ptr<std::vector<uint8_t>> buffer,
	double fpts,
	double fdts,
	double fDuration,
	double fragmentPTSoffset)
{
	if (!hasDemuxer())
	{
		// Raw TTML/WebVTT: wrap the buffer directly into an AampMediaSample
		// and inject via injectSingleSample.  createSegment() will call
		// refineDisplayOffset() to apply TTML offset correction.
		//
		// For TTML (m_applyTextTransform == true): the buffer PTS is already
		// pipeline-clock-relative (ISO-BMFF / DASH timeline).  Pass
		// fragmentPTSoffset as mDisplayOffsetMs for refineDisplayOffset() to
		// correct absolute wall-clock TTML cue timestamps.
		//
		// For WebVTT (m_applyTextTransform == false):
		//   With HlsTsEnablePTSReStamp (fragmentPTSoffset != 0): video is
		//   restamped to start at 0 while subtitle fpts is the raw HLS
		//   position.  Negate fragmentPTSoffset as displayOffset so that
		//   Rialto's renderer can correct via X-TIMESTAMP-MAP MPEGTS:
		//     renderTime = cueTime + MPEGTS_s − displayOffset_s ≈ cueTime
		//   This mirrors the GST_BUFFER_OFFSET = -(ptsOffset×1000) mechanism
		//   in the non-direct-rialto AAMPGstPlayer subtitle sync path.
		//   Without restamping (fragmentPTSoffset == 0): AAMP seeks the
		//   pipeline to the HLS start position, so fpts already matches the
		//   pipeline clock — displayOffset = 0 and no correction is needed.

		const int64_t displayOffsetMs = m_applyTextTransform
			? SecondsToMs(fragmentPTSoffset)
			: -SecondsToMs(fragmentPTSoffset);

		AampMediaSample sample;
		sample.mData     = std::shared_ptr<const uint8_t>(
			buffer, buffer->data());
		sample.mDataSize = buffer->size();
		sample.mPts      = fpts;
		sample.mDts      = fdts;
		sample.mDuration = fDuration;
		sample.mDisplayOffsetMs = displayOffsetMs;
		return injectSingleSample(pipeline, std::move(sample));
	}
	// TTML-in-MP4: use the base-class demuxer path.
	return AampRialtoMediaSource::processDataFragment(
		pipeline, std::move(buffer),
		fpts, fdts, fDuration, fragmentPTSoffset);
}

// ---------------------------------------------------------------------------
// refineDisplayOffset
// ---------------------------------------------------------------------------

int64_t AampRialtoSubtitleSource::refineDisplayOffset(
	const AampMediaSample &sample) const
{
	const int64_t displayOffsetMs = sample.mDisplayOffsetMs;

	AAMPLOG_TRACE("refineDisplayOffset: pre displayOffsetMs=%" PRId64
		" applyTextTransform=%d",
		displayOffsetMs, static_cast<int>(m_applyTextTransform));

	if (!m_applyTextTransform || !sample.mData || sample.mDataSize == 0)
	{
		AAMPLOG_TRACE("refineDisplayOffset: passthrough displayOffsetMs=%" PRId64,
			displayOffsetMs);
		return displayOffsetMs;
	}

	return m_ttmlOffset.Refine(
		reinterpret_cast<const char *>(sample.mData.get()), sample.mDataSize,
		SecondsToMs(sample.mPts), SecondsToMs(sample.mDuration),
		displayOffsetMs);
}

// ---------------------------------------------------------------------------
// isInbandCC
// ---------------------------------------------------------------------------

bool AampRialtoSubtitleSource::isInbandCC() const
{
	return format() == FORMAT_INVALID;
}
