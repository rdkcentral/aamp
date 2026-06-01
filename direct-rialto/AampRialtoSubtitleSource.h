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
 * @file AampRialtoSubtitleSource.h
 * @brief Subtitle-specific Rialto media source.
 */

#ifndef AAMP_RIALTO_SUBTITLE_SOURCE_H
#define AAMP_RIALTO_SUBTITLE_SOURCE_H

#include "AampRialtoMediaSource.h"
#include "StreamOutputFormat.h"

#include <cmath>
#include <string>

/**
 * @class AampRialtoSubtitleSource
 * @brief Concrete media source for subtitle tracks.
 *
 * Supports two injection paths:
 *
 *  - **Raw TTML / WebVTT** (FORMAT_SUBTITLE_TTML / FORMAT_SUBTITLE_WEBVTT):
 *    No MP4 container, so no demuxer is used.  processInitFragment()
 *    synthesises a MediaCodecInfo from the cached m_subtitleFormat so that
 *    attachOrUpdate() can call mapCodecToMime() and attachSource().
 *    Data fragments are injected via processDataFragment() which wraps the
 *    buffer into an AampMediaSample and calls injectSingleSample().
 *
 *  - **TTML-in-MP4** (FORMAT_SUBTITLE_MP4 / stpp or wvtt stsd box):
 *    Mp4Demux parses the init and data segments via the base-class
 *    processInitFragment() / processDataFragment() path.
 */
class AampRialtoSubtitleSource : public AampRialtoMediaSource
{
public:
	AampRialtoSubtitleSource() = default;

	AampMediaType mediaType() const override { return eMEDIATYPE_SUBTITLE; }

	/**
	 * @brief Record the stream format determined at Configure() time.
	 *
	 * Must be called before the first processInitFragment() so that the
	 * raw subtitle path can synthesise the correct codec format.
	 */
	void setSubtitleFormat(StreamOutputFormat fmt) { m_subtitleFormat = fmt; }

	/**
	 * @brief Enable inband closed-caption mode.
	 *
	 * When active, mapCodecToMime() returns "application/x-subtitle-cc"
	 * regardless of the codec format, and createRialtoSource() passes the
	 * initial text-track identifier to the Rialto server.  No data injection
	 * is performed for inband CC — the server extracts CC from the video
	 * bitstream internally.
	 */
	void enableInbandCC() override { m_inbandCC = true; }

	/**
	 * @brief Parse or synthesise the init segment for this subtitle track.
	 *
	 * For raw TTML/WebVTT (FORMAT_SUBTITLE_TTML / FORMAT_SUBTITLE_WEBVTT)
	 * there is no MP4 container, so a synthetic MediaCodecInfo is returned
	 * without demuxing.  For TTML-in-MP4 (FORMAT_SUBTITLE_MP4) the base
	 * class demuxer path is used.
	 *
	 * Also resets the TTML offset detection state so that the content type
	 * is re-detected from the new stream, and sets m_applyTextTransform based
	 * on the resolved codec format.
	 */
	std::optional<MediaCodecInfo> processInitFragment(
		std::shared_ptr<std::vector<uint8_t>> buffer) override;

	/**
	 * @brief Parse or inject one subtitle data fragment.
	 *
	 * For raw TTML/WebVTT (no demuxer): wraps the buffer directly into an
	 * AampMediaSample — filling mPts, mDts, mDuration and mDisplayOffsetMs
	 * from the supplied parameters — then calls injectSingleSample().
	 *
	 * For TTML-in-MP4 (demuxer present): delegates to the base-class
	 * processDataFragment() which demuxes via Mp4Demux.
	 */
	bool processDataFragment(
		firebolt::rialto::IMediaPipeline &pipeline,
		std::shared_ptr<std::vector<uint8_t>> buffer,
		double fpts,
		double fdts,
		double fDuration,
		double fragmentPTSoffset) override;

protected:
	bool mapCodecToMime(
		GstStreamOutputFormat codecFormat,
		std::string &mimeType,
		firebolt::rialto::StreamFormat &streamFormat) const override;

	std::unique_ptr<firebolt::rialto::IMediaPipeline::MediaSource>
		createRialtoSource(
			const std::string &mimeType,
			bool hasDrm,
			const MediaCodecInfo &codecInfo,
			firebolt::rialto::StreamFormat streamFormat,
			std::shared_ptr<firebolt::rialto::CodecData> codecData) const override;

	size_t needDataBatchSize() const override { return 1; }

	void updateCachedMetadata(const MediaCodecInfo &codecInfo) override;

	/**
	 * @brief Create a Rialto MediaSegment for injection.
	 *
	 * Extracts @c mDisplayOffsetMs from the sample, passes it through
	 * refineDisplayOffset() to apply TTML offset correction, and calls
	 * segment->setDisplayOffset() so GstTextTrackSink can shift absolute
	 * TTML cue timestamps to programme-relative time.
	 */
	std::unique_ptr<firebolt::rialto::IMediaPipeline::MediaSegment>
		createSegment(const AampMediaSample &sample) const override;

private:
	/**
	 * @brief Compute the display offset (ms) for one TTML sample.
	 *
	 * Detects LINEAR_OFFSET vs PASSTHROUGH on the first call by comparing
	 * the TTML begin= timestamp against the container PTS.  Refines the
	 * stored offset on subsequent calls, mirroring gstvipertransform.
	 * Returns 0 for PASSTHROUGH content or when m_applyTextTransform is
	 * false (WebVTT).  The AAMP offset is read from sample.mDisplayOffsetMs.
	 *
	 * @param sample  The subtitle sample whose payload and timing is inspected.
	 * @return Refined display offset in milliseconds.
	 */
	int64_t refineDisplayOffset(
		const AampMediaSample &sample) const;

	/**
	 * @brief Convert a TTML clock-value string (H:MM:SS[.fff]) to ms.
	 */
	static int64_t parseTimeToMs(const std::string &timeStr);

	/**
	 * @brief Locate the first begin="…" attribute value (ms) in @p ttml.
	 * @return true if found; false for empty/header-only documents.
	 */
	static bool findFirstBeginMs(const std::string &ttml, int64_t &outMs);

	StreamOutputFormat m_subtitleFormat{FORMAT_INVALID};

	/// When true, this source uses the "application/x-subtitle-cc" MIME type
	/// and the Rialto server extracts CC from the video bitstream internally.
	bool m_inbandCC{false};

	/// Detection state for TTML cue-timestamp mode.
	enum class ContentType
	{
		UNKNOWN,       ///< Not yet determined.
		PASSTHROUGH,   ///< TTML timestamps align with container PTS.
		LINEAR_OFFSET  ///< Absolute TTML timestamps require correction.
	};
	/// mutable so that refineDisplayOffset() can update state from
	/// the const createSegment() override.
	mutable ContentType m_transformContentType{ContentType::UNKNOWN};
	mutable int64_t     m_linearBeginOffsetMs{0};

	/// true when the resolved codec is TTML (raw or stpp-in-MP4);
	/// false for WebVTT (raw or wvtt-in-MP4) which needs no correction.
	bool m_applyTextTransform{false};
};

#endif /* AAMP_RIALTO_SUBTITLE_SOURCE_H */
