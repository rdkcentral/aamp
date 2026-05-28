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
#include "AampTextTransform.h"
#include "StreamOutputFormat.h"

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
 *    Data fragments are injected directly by AampRialtoPlayer::SendTransfer
 *    via injectSingleSample() (base-class implementation).
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
	 * @brief Parse or synthesise the init segment for this subtitle track.
	 *
	 * For raw TTML/WebVTT (FORMAT_SUBTITLE_TTML / FORMAT_SUBTITLE_WEBVTT)
	 * there is no MP4 container, so a synthetic MediaCodecInfo is returned
	 * without demuxing.  For TTML-in-MP4 (FORMAT_SUBTITLE_MP4) the base
	 * class demuxer path is used.
	 *
	 * Also resets the AampTextTransform state so that the content type is
	 * re-detected from the new stream, and sets m_applyTextTransform based
	 * on the resolved codec format.
	 */
	std::optional<MediaCodecInfo> processInitFragment(
		std::shared_ptr<std::vector<uint8_t>> buffer) override;

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

	std::unique_ptr<firebolt::rialto::IMediaPipeline::MediaSegment>
		createSegment(const AampMediaSample &sample) const override;

	/**
	 * @brief Refine the display offset using AampTextTransform for TTML.
	 *
	 * Called by the base class once per data fragment (first sample in
	 * processDataFragment) and once per injectSingleSample.  For TTML
	 * content (raw or stpp-in-MP4) the text transform inspects the cue
	 * timestamps and detects any LINEAR_OFFSET mismatch vs the container
	 * PTS.  For WebVTT content (m_applyTextTransform == false) the
	 * supplied @p displayOffsetNs is returned unchanged (PASSTHROUGH).
	 */
	int64_t refineDisplayOffset(
		const AampMediaSample &sample, int64_t displayOffsetMs) override;


private:
	StreamOutputFormat m_subtitleFormat{FORMAT_INVALID};
	/// Stateful TTML display-offset corrector.  Mirrors vipertransform.
	AampTextTransform  m_textTransform;
	/// true when the resolved codec is TTML (raw or stpp-in-MP4);
	/// false for WebVTT (raw or wvtt-in-MP4) which needs no correction.
	bool               m_applyTextTransform{false};
};

#endif /* AAMP_RIALTO_SUBTITLE_SOURCE_H */
