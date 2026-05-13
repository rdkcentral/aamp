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
 * @file AampRialtoAudioSource.h
 * @brief Audio-specific Rialto media source.
 */

#ifndef AAMP_RIALTO_AUDIO_SOURCE_H
#define AAMP_RIALTO_AUDIO_SOURCE_H

#include "AampRialtoMediaSource.h"

/**
 * @class AampRialtoAudioSource
 * @brief Concrete media source for audio tracks.
 *
 * Handles AAC/EAC3/AC4 codec mapping, MediaSegmentAudio construction,
 * and MediaSourceAudio creation for the Rialto pipeline.
 */
class AampRialtoAudioSource : public AampRialtoMediaSource
{
public:
	AampRialtoAudioSource() = default;

	AampMediaType mediaType() const override { return eMEDIATYPE_AUDIO; }

	int32_t sampleRate() const { return m_sampleRate; }
	int32_t channels() const { return m_channels; }

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
		createSegment(int64_t ptsNs, int64_t durationNs) const override;

private:
	int32_t m_sampleRate{0};
	int32_t m_channels{0};
};

#endif /* AAMP_RIALTO_AUDIO_SOURCE_H */
