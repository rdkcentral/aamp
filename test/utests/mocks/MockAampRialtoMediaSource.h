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
 * @file MockAampRialtoMediaSource.h
 * @brief Google Mock for AampRialtoMediaSource.
 *
 * Used by AampRialtoPlayerTests to verify the player's interactions
 * with its per-track source objects without compiling the real
 * Video/Audio/Subtitle implementations.
 */

#pragma once

#include <gmock/gmock.h>
#include "AampRialtoMediaSource.h"

class MockAampRialtoMediaSource : public AampRialtoMediaSource
{
public:
	MOCK_METHOD(AampMediaType, mediaType, (), (const, override));

	MOCK_METHOD(bool, mapCodecToMime,
		(GstStreamOutputFormat codecFormat,
		 std::string &mimeType,
		 firebolt::rialto::StreamFormat &streamFormat),
		(const, override));

	MOCK_METHOD(std::unique_ptr<firebolt::rialto::IMediaPipeline::MediaSource>,
		createRialtoSource,
		(const std::string &mimeType,
		 bool hasDrm,
		 const MediaCodecInfo &codecInfo,
		 firebolt::rialto::StreamFormat streamFormat,
		 std::shared_ptr<firebolt::rialto::CodecData> codecData),
		(const, override));

	MOCK_METHOD(void, updateCachedMetadata,
		(const MediaCodecInfo &codecInfo),
		(override));

	MOCK_METHOD(std::unique_ptr<firebolt::rialto::IMediaPipeline::MediaSegment>,
		createSegment,
		(const AampMediaSample &sample),
		(const, override));

	MOCK_METHOD(bool, injectSingleSampleProxy,
		(firebolt::rialto::IMediaPipeline &pipeline),
		());

	MOCK_METHOD(bool, processDataFragmentProxy,
		(firebolt::rialto::IMediaPipeline &pipeline,
		 double fpts, double fdts, double fDuration,
		 double fragmentPTSoffset),
		());

	/**
	 * For SUBTITLE sources, routes through injectSingleSampleProxy so
	 * tests can verify routing without a Rialto NeedData handshake.
	 * For other media types, the base class implementation is used so
	 * existing video/audio SendSample tests continue to work correctly.
	 */
	bool injectSingleSample(
		firebolt::rialto::IMediaPipeline &pipeline,
		AampMediaSample &&sample,
		bool morePending) override
	{
		if (mediaType() == eMEDIATYPE_SUBTITLE)
			return injectSingleSampleProxy(pipeline);
		return AampRialtoMediaSource::injectSingleSample(
			pipeline, std::move(sample), morePending);
	}

	/**
	 * For SUBTITLE sources, routes through processDataFragmentProxy so
	 * tests can verify routing and parameters without demuxer setup.
	 * For other media types, the base class implementation is used.
	 */
	bool processDataFragment(
		firebolt::rialto::IMediaPipeline &pipeline,
		std::shared_ptr<std::vector<uint8_t>> buffer,
		double fpts, double fdts, double fDuration,
		double fragmentPTSoffset) override
	{
		if (mediaType() == eMEDIATYPE_SUBTITLE)
			return processDataFragmentProxy(
				pipeline, fpts, fdts, fDuration, fragmentPTSoffset);
		return AampRialtoMediaSource::processDataFragment(
			pipeline, std::move(buffer),
			fpts, fdts, fDuration, fragmentPTSoffset);
	}

	MOCK_METHOD(int64_t, firstPtsMs, (), (const, override));
};
