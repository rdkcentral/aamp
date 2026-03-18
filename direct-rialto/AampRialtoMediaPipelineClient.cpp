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
	AAMPLOG_INFO("AampRialtoMediaPipelineClient::%s ENTRY", __FUNCTION__);
	AAMPLOG_INFO("AampRialtoMediaPipelineClient::%s EXIT", __FUNCTION__);
	return false;
}

// ---------------------------------------------------------------------------
// IMediaPipelineClient overrides
// ---------------------------------------------------------------------------

void AampRialtoMediaPipelineClient::notifyNetworkState(NetworkState state)
{
	AAMPLOG_INFO("AampRialtoMediaPipelineClient::%s ENTRY state=%d",
		__FUNCTION__, static_cast<int>(state));
	AAMPLOG_INFO("AampRialtoMediaPipelineClient::%s EXIT", __FUNCTION__);
}

void AampRialtoMediaPipelineClient::notifyPlaybackState(PlaybackState state)
{
	AAMPLOG_INFO("AampRialtoMediaPipelineClient::%s ENTRY state=%d",
		__FUNCTION__, static_cast<int>(state));
	AAMPLOG_INFO("AampRialtoMediaPipelineClient::%s EXIT", __FUNCTION__);
}

void AampRialtoMediaPipelineClient::notifyPosition(int64_t position)
{
	AAMPLOG_INFO("AampRialtoMediaPipelineClient::%s ENTRY position=%" PRId64,
		__FUNCTION__, position);
	AAMPLOG_INFO("AampRialtoMediaPipelineClient::%s EXIT", __FUNCTION__);
}

void AampRialtoMediaPipelineClient::notifyNeedMediaData(
	int32_t sourceId,
	size_t frameCount,
	uint32_t needDataRequestId,
	const std::shared_ptr<MediaPlayerShmInfo> &mediaPlayerShmInfo)
{
	AAMPLOG_INFO("AampRialtoMediaPipelineClient::%s ENTRY"
		" sourceId=%d frameCount=%zu needDataRequestId=%u",
		__FUNCTION__, sourceId, frameCount, needDataRequestId);
	if (m_needDataCallback)
		m_needDataCallback(sourceId, frameCount, needDataRequestId);
	AAMPLOG_INFO("AampRialtoMediaPipelineClient::%s EXIT", __FUNCTION__);
}

void AampRialtoMediaPipelineClient::notifyQos(
	int32_t sourceId, const QosInfo &qosInfo)
{
	AAMPLOG_INFO("AampRialtoMediaPipelineClient::%s ENTRY sourceId=%d",
		__FUNCTION__, sourceId);
	AAMPLOG_INFO("AampRialtoMediaPipelineClient::%s EXIT", __FUNCTION__);
}

void AampRialtoMediaPipelineClient::notifyBufferUnderflow(int32_t sourceId)
{
	AAMPLOG_INFO("AampRialtoMediaPipelineClient::%s ENTRY sourceId=%d",
		__FUNCTION__, sourceId);
	AAMPLOG_INFO("AampRialtoMediaPipelineClient::%s EXIT", __FUNCTION__);
}

void AampRialtoMediaPipelineClient::notifyDuration(int64_t duration)
{
	AAMPLOG_INFO("AampRialtoMediaPipelineClient::%s ENTRY duration=%" PRId64,
		__FUNCTION__, duration);
	AAMPLOG_INFO("AampRialtoMediaPipelineClient::%s EXIT", __FUNCTION__);
}

void AampRialtoMediaPipelineClient::notifyNativeSize(
	uint32_t width, uint32_t height, double aspect)
{
	AAMPLOG_INFO("AampRialtoMediaPipelineClient::%s ENTRY"
		" width=%u height=%u aspect=%f",
		__FUNCTION__, width, height, aspect);
	AAMPLOG_INFO("AampRialtoMediaPipelineClient::%s EXIT", __FUNCTION__);
}

void AampRialtoMediaPipelineClient::notifyVideoData(bool hasData)
{
	AAMPLOG_INFO("AampRialtoMediaPipelineClient::%s ENTRY hasData=%d",
		__FUNCTION__, hasData);
	AAMPLOG_INFO("AampRialtoMediaPipelineClient::%s EXIT", __FUNCTION__);
}

void AampRialtoMediaPipelineClient::notifyAudioData(bool hasData)
{
	AAMPLOG_INFO("AampRialtoMediaPipelineClient::%s ENTRY hasData=%d",
		__FUNCTION__, hasData);
	AAMPLOG_INFO("AampRialtoMediaPipelineClient::%s EXIT", __FUNCTION__);
}

void AampRialtoMediaPipelineClient::notifyCancelNeedMediaData(
	int32_t sourceId)
{
	AAMPLOG_INFO("AampRialtoMediaPipelineClient::%s ENTRY sourceId=%d",
		__FUNCTION__, sourceId);
	if (m_cancelNeedDataCallback)
		m_cancelNeedDataCallback(sourceId);
	AAMPLOG_INFO("AampRialtoMediaPipelineClient::%s EXIT", __FUNCTION__);
}

void AampRialtoMediaPipelineClient::notifyPlaybackError(
	int32_t sourceId, PlaybackError error)
{
	AAMPLOG_INFO("AampRialtoMediaPipelineClient::%s ENTRY"
		" sourceId=%d error=%d",
		__FUNCTION__, sourceId, static_cast<int>(error));
	AAMPLOG_INFO("AampRialtoMediaPipelineClient::%s EXIT", __FUNCTION__);
}

void AampRialtoMediaPipelineClient::notifySourceFlushed(int32_t sourceId)
{
	AAMPLOG_INFO("AampRialtoMediaPipelineClient::%s ENTRY sourceId=%d",
		__FUNCTION__, sourceId);
	AAMPLOG_INFO("AampRialtoMediaPipelineClient::%s EXIT", __FUNCTION__);
}

void AampRialtoMediaPipelineClient::notifyPlaybackInfo(
	const PlaybackInfo &playbackInfo)
{
	AAMPLOG_INFO("AampRialtoMediaPipelineClient::%s ENTRY",
		__FUNCTION__);
	AAMPLOG_INFO("AampRialtoMediaPipelineClient::%s EXIT", __FUNCTION__);
}
