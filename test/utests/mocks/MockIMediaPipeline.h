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

#ifndef MOCK_IMEDIA_PIPELINE_H
#define MOCK_IMEDIA_PIPELINE_H

#include "IMediaPipeline.h"
#include <gmock/gmock.h>
#include <memory>
#include <string>

/**
 * @class MockIMediaPipeline
 * @brief Google Mock for firebolt::rialto::IMediaPipeline.
 *
 * Used by AampRialtoPlayer L1 tests to verify that the player calls the
 * correct pipeline API methods without requiring a live Rialto server.
 */
class MockIMediaPipeline : public firebolt::rialto::IMediaPipeline
{
public:
	// clang-format off
	MOCK_METHOD(std::weak_ptr<firebolt::rialto::IMediaPipelineClient>,
		getClient, (), (override));

	MOCK_METHOD(bool, load,
		(firebolt::rialto::MediaType type,
		 const std::string &mimeType,
		 const std::string &url,
		bool isLive),
		(override));

	MOCK_METHOD(bool, attachSource,
		(const std::unique_ptr<firebolt::rialto::IMediaPipeline::MediaSource> &source),
		(override));

	MOCK_METHOD(bool, removeSource,
		(int32_t id),
		(override));

	MOCK_METHOD(bool, allSourcesAttached, (), (override));

	MOCK_METHOD(bool, play, (bool &async), (override));

	MOCK_METHOD(bool, pause, (), (override));

	MOCK_METHOD(bool, stop, (), (override));

	MOCK_METHOD(bool, setPlaybackRate, (double rate), (override));

	MOCK_METHOD(bool, setPosition, (int64_t position), (override));

	MOCK_METHOD(bool, getPosition, (int64_t &position), (override));

	MOCK_METHOD(bool, getStats,
		(int32_t sourceId, uint64_t &renderedFrames, uint64_t &droppedFrames),
		(override));

	MOCK_METHOD(bool, setImmediateOutput,
		(int32_t sourceId, bool immediateOutput),
		(override));

	MOCK_METHOD(bool, getImmediateOutput,
		(int32_t sourceId, bool &immediateOutput),
		(override));

	MOCK_METHOD(bool, setVideoWindow,
		(uint32_t x, uint32_t y, uint32_t width, uint32_t height),
		(override));

	MOCK_METHOD(bool, haveData,
		(firebolt::rialto::MediaSourceStatus status,
		 uint32_t needDataRequestId),
		(override));

	MOCK_METHOD(firebolt::rialto::AddSegmentStatus, addSegment,
		(uint32_t needDataRequestId,
		 const std::unique_ptr<firebolt::rialto::IMediaPipeline::MediaSegment> &mediaSegment),
		(override));

	MOCK_METHOD(bool, renderFrame, (), (override));

	MOCK_METHOD(bool, setVolume,
		(double targetVolume, uint32_t volumeDuration, firebolt::rialto::EaseType easeType),
		(override));

	MOCK_METHOD(bool, getVolume, (double &currentVolume), (override));

	MOCK_METHOD(bool, setMute, (int32_t sourceId, bool mute), (override));

	MOCK_METHOD(bool, getMute, (int32_t sourceId, bool &mute), (override));

	MOCK_METHOD(bool, setTextTrackIdentifier,
		(const std::string &textTrackIdentifier),
		(override));

	MOCK_METHOD(bool, getTextTrackIdentifier,
		(std::string &textTrackIdentifier),
		(override));

	MOCK_METHOD(bool, setLowLatency, (bool lowLatency), (override));

	MOCK_METHOD(bool, setSync, (bool sync), (override));

	MOCK_METHOD(bool, getSync, (bool &sync), (override));

	MOCK_METHOD(bool, setSyncOff, (bool syncOff), (override));

	MOCK_METHOD(bool, setStreamSyncMode,
		(int32_t sourceId, int32_t streamSyncMode),
		(override));

	MOCK_METHOD(bool, getStreamSyncMode,
		(int32_t &streamSyncMode),
		(override));

	MOCK_METHOD(bool, flush,
		(int32_t sourceId, bool resetTime, bool &async),
		(override));

	MOCK_METHOD(bool, setSourcePosition,
		(int32_t sourceId, int64_t position, bool resetTime,
		 double appliedRate, uint64_t stopPosition),
		(override));

	MOCK_METHOD(bool, setSubtitleOffset,
		(int32_t sourceId, int64_t position),
		(override));

	MOCK_METHOD(bool, processAudioGap,
		(int64_t position, uint32_t duration, int64_t discontinuityGap,
		 bool audioAac),
		(override));

	MOCK_METHOD(bool, setBufferingLimit, (uint32_t limitBufferingMs), (override));

	MOCK_METHOD(bool, getBufferingLimit, (uint32_t &limitBufferingMs), (override));

	MOCK_METHOD(bool, setUseBuffering, (bool useBuffering), (override));

	MOCK_METHOD(bool, getUseBuffering, (bool &useBuffering), (override));

	MOCK_METHOD(bool, switchSource,
		(const std::unique_ptr<firebolt::rialto::IMediaPipeline::MediaSource> &source),
		(override));
	// clang-format on

	MOCK_METHOD(bool, getDuration, (int64_t &duration), (override));

};

#endif // MOCK_IMEDIA_PIPELINE_H
