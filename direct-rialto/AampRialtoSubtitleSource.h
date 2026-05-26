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
 * @brief Subtitle-specific Rialto media source (skeleton for future use).
 */

#ifndef AAMP_RIALTO_SUBTITLE_SOURCE_H
#define AAMP_RIALTO_SUBTITLE_SOURCE_H

#include "AampRialtoMediaSource.h"

/**
 * @class AampRialtoSubtitleSource
 * @brief Skeleton media source for subtitle tracks.
 *
 * Subtitle injection through the Rialto pipeline is not yet implemented.
 * This class exists so that Configure() can create a source object for
 * subtitle tracks, allowing the demuxer to run, but attachOrUpdate()
 * will return FAILED until codec mapping is implemented.
 */
class AampRialtoSubtitleSource : public AampRialtoMediaSource
{
public:
	AampRialtoSubtitleSource() = default;

	AampMediaType mediaType() const override { return eMEDIATYPE_SUBTITLE; }

	/**
	 * @brief Subtitle-specific init-fragment handling (not yet implemented).
	 *
	 * Overrides the base-class demuxer path.  Returns std::nullopt until
	 * subtitle injection via Rialto is supported.
	 */
	std::optional<MediaCodecInfo> processInitFragment(
		std::shared_ptr<std::vector<uint8_t>> buffer) override;

	/**
	 * @brief Subtitle-specific data-fragment handling (not yet implemented).
	 *
	 * Overrides the base-class demuxer path.  Returns true (no error)
	 * until subtitle injection via Rialto is supported.
	 */
	bool processDataFragment(
		firebolt::rialto::IMediaPipeline &pipeline,
		std::shared_ptr<std::vector<uint8_t>> buffer) override;

	/**
	 * @brief Subtitle-specific sample injection (not yet implemented).
	 *
	 * Overrides the base-class injection path.  Returns false until
	 * subtitle injection via Rialto is supported.
	 */
	bool injectSingleSample(
		firebolt::rialto::IMediaPipeline &pipeline,
		AampMediaSample &&sample,
		bool morePending = false) override;

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

	void updateCachedMetadata(const MediaCodecInfo &codecInfo) override;

	std::unique_ptr<firebolt::rialto::IMediaPipeline::MediaSegment>
		createSegment(const AampMediaSample &sample) const override;
};

#endif /* AAMP_RIALTO_SUBTITLE_SOURCE_H */
