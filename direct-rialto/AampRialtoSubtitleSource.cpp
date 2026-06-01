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
#include <cmath>
#include <cstdio>
#include <cstring>

// ---------------------------------------------------------------------------
// parseTimeToMs  (TTML clock-value H:MM:SS[.fff] → milliseconds)
// ---------------------------------------------------------------------------

int64_t AampRialtoSubtitleSource::parseTimeToMs(const std::string &timeStr)
{
	int   hours = 0;
	int   mins  = 0;
	float secs  = 0.f;

	if (std::sscanf(timeStr.c_str(), "%d:%d:%f", &hours, &mins, &secs) != 3)
	{
		return 0;
	}

	return (static_cast<int64_t>(hours) * 3600LL +
	        static_cast<int64_t>(mins)  * 60LL)
	       * 1000LL
	       + static_cast<int64_t>(secs * 1000.f);
}

// ---------------------------------------------------------------------------
// findFirstBeginMs  (locate first begin="…" attribute value in TTML)
// ---------------------------------------------------------------------------

bool AampRialtoSubtitleSource::findFirstBeginMs(
	const std::string &ttml, int64_t &outMs)
{
	const std::string tag{"begin=\""};
	const auto pos = ttml.find(tag);
	if (pos == std::string::npos)
	{
		return false;
	}

	const auto valStart = pos + tag.size();
	const auto valEnd   = ttml.find('"', valStart);
	if (valEnd == std::string::npos)
	{
		return false;
	}

	outMs = parseTimeToMs(ttml.substr(valStart, valEnd - valStart));
	AAMPLOG_DEBUG("findFirstBeginMs: found begin=\"%s\" → %" PRId64 " ms",
		ttml.substr(valStart, valEnd - valStart).c_str(), outMs);
	return true;
}

// ---------------------------------------------------------------------------
// mapCodecToMime
// ---------------------------------------------------------------------------

bool AampRialtoSubtitleSource::mapCodecToMime(
	GstStreamOutputFormat codecFormat,
	std::string &mimeType,
	firebolt::rialto::StreamFormat &streamFormat) const
{
	// Inband CC: the Rialto server reads CC from the video stream; use a
	// dedicated MIME type and bypass the normal codec mapping.
	if (m_inbandCC)
	{
		mimeType    = "application/x-subtitle-cc";
		streamFormat = firebolt::rialto::StreamFormat::RAW;
		return true;
	}

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
	// For inband CC sources supply the default text-track identifier so the
	// Rialto server starts rendering CC1 immediately.
	const std::string textTrackId = m_inbandCC ? "CC1" : "";
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
	const int64_t ptsNs =
		static_cast<int64_t>(sample.mPts * kNsPerSecond);
	const int64_t durationNs =
		static_cast<int64_t>(sample.mDuration * kNsPerSecond);
	auto segment = std::make_unique<firebolt::rialto::IMediaPipeline::MediaSegment>(
		m_sourceId,
		firebolt::rialto::MediaSourceType::SUBTITLE,
		ptsNs,
		durationNs);

	const int64_t displayOffsetMs =
		refineDisplayOffset(sample);
	if (displayOffsetMs > 0)
	{
		AAMPLOG_INFO("subtitle setDisplayOffset sourceId=%d displayOffsetMs=%" PRId64,
			m_sourceId, displayOffsetMs);
		segment->setDisplayOffset(displayOffsetMs);
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
	m_transformContentType  = ContentType::UNKNOWN;
	m_linearBeginOffsetMs   = 0;
	m_applyTextTransform    = false;

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
		AampMediaSample sample;
		sample.mData     = std::shared_ptr<const uint8_t>(
			buffer, buffer->data());
		sample.mDataSize = buffer->size();
		sample.mPts      = fpts;
		sample.mDts      = fdts;
		sample.mDuration = fDuration;
		sample.mDisplayOffsetMs =
			static_cast<int64_t>(fragmentPTSoffset * 1000.0);
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
		" applyTextTransform=%d contentType=%d",
		displayOffsetMs,
		static_cast<int>(m_applyTextTransform),
		static_cast<int>(m_transformContentType));

	if (!m_applyTextTransform || !sample.mData || sample.mDataSize == 0)
	{
		AAMPLOG_TRACE("refineDisplayOffset: passthrough displayOffsetMs=%" PRId64,
			displayOffsetMs);
		return displayOffsetMs;
	}

	const int64_t ptsMs      = static_cast<int64_t>(sample.mPts      * 1000.0);
	const int64_t durationMs = static_cast<int64_t>(sample.mDuration * 1000.0);

	// --- inlined AampTextTransform::compute() ---

	if (m_transformContentType == ContentType::PASSTHROUGH)
	{
		AAMPLOG_TRACE("refineDisplayOffset: PASSTHROUGH → 0");
		return 0LL;
	}

	// Build a string from the raw payload.
	// For Harmonic UHD buffers that concatenate multiple XML documents,
	// truncate to the first document, matching gstvipertransform behaviour.
	std::string ttml(
		reinterpret_cast<const char *>(sample.mData.get()), sample.mDataSize);
	const auto secondXml = ttml.find("<?xml", 5);
	if (secondXml != std::string::npos)
	{
		AAMPLOG_DEBUG("refineDisplayOffset: truncating Harmonic UHD "
			"multi-doc buffer at offset %zu", secondXml);
		ttml.resize(secondXml);
	}

	int64_t firstBeginMs = 0;
	if (!findFirstBeginMs(ttml, firstBeginMs))
	{
		// Empty / header-only segment — reuse last known offset.
		const int64_t result =
			(m_transformContentType == ContentType::LINEAR_OFFSET)
			? m_linearBeginOffsetMs
			: 0LL;
		AAMPLOG_DEBUG("refineDisplayOffset: no begin= found, "
			"reusing stored offset %" PRId64 " ms", result);
		return result;
	}

	const int64_t offsetFromPtsMs = firstBeginMs - ptsMs;
	AAMPLOG_DEBUG("refineDisplayOffset: firstBeginMs=%" PRId64
		" ptsMs=%" PRId64 " offsetFromPtsMs=%" PRId64
		" aampOffsetMs=%" PRId64 " durationMs=%" PRId64,
		firstBeginMs, ptsMs, offsetFromPtsMs,
		displayOffsetMs, durationMs);

	if (m_transformContentType == ContentType::UNKNOWN)
	{
		if (std::abs(offsetFromPtsMs - displayOffsetMs) > durationMs)
		{
			// Large mismatch: TTML timestamps are absolute wall-clock.
			m_transformContentType = ContentType::LINEAR_OFFSET;
			m_linearBeginOffsetMs  = offsetFromPtsMs;
			AAMPLOG_INFO("refineDisplayOffset: detected LINEAR_OFFSET "
				"from TTML, offset=%" PRId64 " ms",
				m_linearBeginOffsetMs);
		}
		else if (displayOffsetMs != 0)
		{
			// AAMP already knows the period-start correction.
			m_transformContentType = ContentType::LINEAR_OFFSET;
			m_linearBeginOffsetMs  = displayOffsetMs;
			AAMPLOG_INFO("refineDisplayOffset: detected LINEAR_OFFSET "
				"from AAMP offset, offset=%" PRId64 " ms",
				m_linearBeginOffsetMs);
		}
		else
		{
			// TTML cue times align with container PTS.
			m_transformContentType = ContentType::PASSTHROUGH;
			AAMPLOG_INFO("refineDisplayOffset: detected PASSTHROUGH");
			return 0LL;
		}
	}
	else if (m_transformContentType == ContentType::LINEAR_OFFSET)
	{
		// Refine the stored offset each fragment, matching vipertransform.
		const int64_t prev = m_linearBeginOffsetMs;
		m_linearBeginOffsetMs =
			(std::abs(offsetFromPtsMs - displayOffsetMs) > durationMs)
			? offsetFromPtsMs
			: displayOffsetMs;
		if (m_linearBeginOffsetMs != prev)
		{
			AAMPLOG_DEBUG("refineDisplayOffset: refined offset "
				"%" PRId64 " → %" PRId64 " ms",
				prev, m_linearBeginOffsetMs);
		}
	}

	const int64_t result = m_linearBeginOffsetMs;
	AAMPLOG_TRACE("refineDisplayOffset: post displayOffsetMs=%" PRId64,
		result);
	return result;
}

