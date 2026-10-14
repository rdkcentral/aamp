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
 * @file AampRialtoMediaPipelineClient.cpp
 * @brief IMediaPipelineClient implementation that forwards Rialto events to
 *        the installed callbacks.
 */

#include "AampRialtoMediaPipelineClient.h"
#include "AampLogManager.h"
#include <cinttypes>

using namespace firebolt::rialto;

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

AampRialtoMediaPipelineClient::AampRialtoMediaPipelineClient()
{
	AAMPLOG_INFO("AampRialtoMediaPipelineClient: constructed");
}

AampRialtoMediaPipelineClient::~AampRialtoMediaPipelineClient()
{
	AAMPLOG_INFO("AampRialtoMediaPipelineClient: destroyed");
}

// ---------------------------------------------------------------------------
// IMediaPipelineClient overrides
// ---------------------------------------------------------------------------

void AampRialtoMediaPipelineClient::notifyNetworkState(NetworkState state)
{
	AAMPLOG_INFO("ENTRY state=%d", static_cast<int>(state));
	AAMPLOG_INFO("EXIT");
}

void AampRialtoMediaPipelineClient::notifyPlaybackState(PlaybackState state)
{
	AAMPLOG_INFO("ENTRY state=%d", static_cast<int>(state));
	const auto callback = Snapshot(m_playbackStateCallback);
	if (callback)
	{
		callback(state);
	}
	AAMPLOG_INFO("EXIT");
}

void AampRialtoMediaPipelineClient::notifyPosition(int64_t position)
{
	AAMPLOG_TRACE("ENTRY position=%" PRId64, position);
	const auto callback = Snapshot(m_positionCallback);
	if (callback)
	{
		callback(position);
	}
	AAMPLOG_TRACE("EXIT");
}

void AampRialtoMediaPipelineClient::notifyNeedMediaData(
	int32_t sourceId,
	size_t frameCount,
	uint32_t needDataRequestId,
	const std::shared_ptr<MediaPlayerShmInfo> &mediaPlayerShmInfo)
{
	AAMPLOG_TRACE("ENTRY sourceId=%d frameCount=%zu needDataRequestId=%u",
		sourceId, frameCount, needDataRequestId);
	const auto callback = Snapshot(m_needDataCallback);
	if (callback)
	{
		callback(sourceId, frameCount, needDataRequestId);
	}
	AAMPLOG_TRACE("EXIT");
}

void AampRialtoMediaPipelineClient::notifyQos(
	int32_t sourceId, const QosInfo &qosInfo)
{
	AAMPLOG_TRACE("ENTRY sourceId=%d", sourceId);
	AAMPLOG_TRACE("EXIT");
}

void AampRialtoMediaPipelineClient::notifyBufferUnderflow(int32_t sourceId)
{
	AAMPLOG_INFO("ENTRY sourceId=%d", sourceId);
	const auto callback = Snapshot(m_bufferUnderflowCallback);
	if (callback)
	{
		callback(sourceId);
	}
	AAMPLOG_INFO("EXIT");
}

void AampRialtoMediaPipelineClient::notifyDuration(int64_t duration)
{
	AAMPLOG_INFO("duration=%" PRId64 " ns", duration);
	const auto callback = Snapshot(m_durationCallback);
	if (callback)
	{
		callback(duration);
	}
	AAMPLOG_TRACE("EXIT");
}

void AampRialtoMediaPipelineClient::notifyNativeSize(
	uint32_t width, uint32_t height, double aspect)
{
	AAMPLOG_INFO("ENTRY width=%u height=%u aspect=%f", width, height, aspect);
	AAMPLOG_INFO("EXIT");
}

void AampRialtoMediaPipelineClient::notifyVideoData(bool hasData)
{
	AAMPLOG_INFO("ENTRY hasData=%d", hasData);
	AAMPLOG_INFO("EXIT");
}

void AampRialtoMediaPipelineClient::notifyAudioData(bool hasData)
{
	AAMPLOG_INFO("ENTRY hasData=%d", hasData);
	AAMPLOG_INFO("EXIT");
}

void AampRialtoMediaPipelineClient::notifyCancelNeedMediaData(
	int32_t sourceId)
{
	AAMPLOG_TRACE("ENTRY sourceId=%d", sourceId);
	const auto callback = Snapshot(m_cancelNeedDataCallback);
	if (callback)
	{
		callback(sourceId);
	}
	AAMPLOG_TRACE("EXIT");
}

void AampRialtoMediaPipelineClient::notifyPlaybackError(
	int32_t sourceId, PlaybackError error)
{
	AAMPLOG_WARN("sourceId=%d error=%d", sourceId, static_cast<int>(error));
	const auto callback = Snapshot(m_playbackErrorCallback);
	if (callback)
	{
		callback(sourceId, error);
	}
}

void AampRialtoMediaPipelineClient::notifySourceFlushed(int32_t sourceId)
{
	// Flush() now uses pipeline-level setPosition(); completion is signalled
	// via PlaybackState::SEEK_DONE, not per-source SourceFlushedEvents.
	AAMPLOG_TRACE("Not Implemented - sourceId=%d - no-op", sourceId);
}

void AampRialtoMediaPipelineClient::notifyPlaybackInfo(
	const PlaybackInfo &playbackInfo)
{
	AAMPLOG_TRACE("Not Implemented");
}

void AampRialtoMediaPipelineClient::notifyFirstFrameReceived(int32_t sourceId)
{
	AAMPLOG_TRACE("Not Implemented - sourceId=%d", sourceId);
}
