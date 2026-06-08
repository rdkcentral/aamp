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
 * @file FakeAampRialtoMediaPipelineClient.cpp
 * @brief Fake implementation of AampRialtoMediaPipelineClient for L1 tests.
 *
 * This fake faithfully dispatches callbacks just like the real implementation.
 * AampRialtoPlayerTests relies on the callback dispatch to exercise the
 * needData/cancelNeedData/playbackState/position/duration event paths.
 *
 * Test targets that include the real AampRialtoPlayer.cpp will link against
 * this fake instead of the real AampRialtoMediaPipelineClient.cpp.
 */

#include "AampRialtoMediaPipelineClient.h"

AampRialtoMediaPipelineClient::AampRialtoMediaPipelineClient() {}
AampRialtoMediaPipelineClient::~AampRialtoMediaPipelineClient() {}

bool AampRialtoMediaPipelineClient::init() { return true; }

void AampRialtoMediaPipelineClient::notifyNetworkState(
	firebolt::rialto::NetworkState /*state*/) {}

void AampRialtoMediaPipelineClient::notifyPlaybackState(
	firebolt::rialto::PlaybackState state)
{
	if (m_playbackStateCallback)
	{
		m_playbackStateCallback(state);
	}
}

void AampRialtoMediaPipelineClient::notifyPosition(int64_t position)
{
	if (m_positionCallback)
	{
		m_positionCallback(position);
	}
}

void AampRialtoMediaPipelineClient::notifyNeedMediaData(
	int32_t sourceId,
	size_t frameCount,
	uint32_t needDataRequestId,
	const std::shared_ptr<firebolt::rialto::MediaPlayerShmInfo> & /*shmInfo*/)
{
	if (m_needDataCallback)
	{
		m_needDataCallback(sourceId, frameCount, needDataRequestId);
	}
}

void AampRialtoMediaPipelineClient::notifyQos(
	int32_t /*sourceId*/,
	const firebolt::rialto::QosInfo & /*qosInfo*/) {}

void AampRialtoMediaPipelineClient::notifyBufferUnderflow(int32_t sourceId)
{
        if (m_bufferUnderflowCallback)
        {
                m_bufferUnderflowCallback(sourceId);
        }
}

void AampRialtoMediaPipelineClient::notifyDuration(int64_t duration)
{
	if (m_durationCallback)
	{
		m_durationCallback(duration);
	}
}

void AampRialtoMediaPipelineClient::notifyNativeSize(
	uint32_t /*width*/, uint32_t /*height*/, double /*aspect*/) {}

void AampRialtoMediaPipelineClient::notifyVideoData(bool /*hasData*/) {}

void AampRialtoMediaPipelineClient::notifyAudioData(bool /*hasData*/) {}

void AampRialtoMediaPipelineClient::notifyCancelNeedMediaData(
	int32_t sourceId)
{
	if (m_cancelNeedDataCallback)
	{
		m_cancelNeedDataCallback(sourceId);
	}
}

void AampRialtoMediaPipelineClient::notifyPlaybackError(
	int32_t /*sourceId*/,
	firebolt::rialto::PlaybackError /*error*/) {}

void AampRialtoMediaPipelineClient::notifySourceFlushed(
	int32_t sourceId)
{
	if (m_sourceFlushedCallback)
	{
		m_sourceFlushedCallback(sourceId);
	}
}
void AampRialtoMediaPipelineClient::notifyPlaybackInfo(
	const firebolt::rialto::PlaybackInfo & /*playbackInfo*/) {}
