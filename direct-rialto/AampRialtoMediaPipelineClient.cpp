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
 * @brief Stub implementation of AampRialtoMediaPipelineClient.
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
// Initialization
// ---------------------------------------------------------------------------

bool AampRialtoMediaPipelineClient::init()
{
	AAMPLOG_INFO("ENTRY");
	AAMPLOG_INFO("EXIT");
	return false;
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
	if (m_playbackStateCallback)
	{
		m_playbackStateCallback(state);
	}
	AAMPLOG_INFO("EXIT");
}

void AampRialtoMediaPipelineClient::notifyPosition(int64_t position)
{
	AAMPLOG_INFO("ENTRY position=%" PRId64, position);
	if (m_positionCallback)
	{
		m_positionCallback(position);
	}
	AAMPLOG_INFO("EXIT");
}

void AampRialtoMediaPipelineClient::notifyNeedMediaData(
	int32_t sourceId,
	size_t frameCount,
	uint32_t needDataRequestId,
	const std::shared_ptr<MediaPlayerShmInfo> &mediaPlayerShmInfo)
{
	AAMPLOG_INFO("ENTRY sourceId=%d frameCount=%zu needDataRequestId=%u", sourceId, frameCount, needDataRequestId);
	if (m_needDataCallback)
	{
		m_needDataCallback(sourceId, frameCount, needDataRequestId);
	}
	AAMPLOG_INFO("EXIT");
}

void AampRialtoMediaPipelineClient::notifyQos(
	int32_t sourceId, const QosInfo &qosInfo)
{
	AAMPLOG_INFO("ENTRY sourceId=%d", sourceId);
	AAMPLOG_INFO("EXIT");
}

void AampRialtoMediaPipelineClient::notifyBufferUnderflow(int32_t sourceId)
{
	AAMPLOG_INFO("ENTRY sourceId=%d", sourceId);
	if (m_bufferUnderflowCallback)
	{
		m_bufferUnderflowCallback(sourceId);
	}
	AAMPLOG_INFO("EXIT");
}

void AampRialtoMediaPipelineClient::notifyDuration(int64_t duration)
{
	AAMPLOG_INFO("ENTRY duration=%" PRId64, duration);
	if (m_durationCallback)
	{
		m_durationCallback(duration);
	}
	AAMPLOG_INFO("EXIT");
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
	AAMPLOG_INFO("ENTRY sourceId=%d", sourceId);
	if (m_cancelNeedDataCallback)
	{
		m_cancelNeedDataCallback(sourceId);
	}
	AAMPLOG_INFO("EXIT");
}

void AampRialtoMediaPipelineClient::notifyPlaybackError(
	int32_t sourceId, PlaybackError error)
{
	AAMPLOG_WARN("sourceId=%d error=%d — not forwarded to player",
		sourceId, static_cast<int>(error));
}

void AampRialtoMediaPipelineClient::notifySourceFlushed(int32_t sourceId)
{
	// Flush() now uses pipeline-level setPosition(); completion is signalled
	// via PlaybackState::SEEK_DONE, not per-source SourceFlushedEvents.
	AAMPLOG_INFO("sourceId=%d - no-op", sourceId);
}

void AampRialtoMediaPipelineClient::notifyPlaybackInfo(
	const PlaybackInfo &playbackInfo)
{
	AAMPLOG_INFO("ENTRY");
	AAMPLOG_INFO("EXIT");
}

void AampRialtoMediaPipelineClient::notifyFirstFrameReceived(int32_t sourceId)
{
	AAMPLOG_INFO("ENTRY sourceId=%d", sourceId);
	AAMPLOG_INFO("EXIT");
}
