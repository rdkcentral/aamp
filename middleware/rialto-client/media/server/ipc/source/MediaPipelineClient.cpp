/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2022 
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

#include "MediaPipelineClient.h"
#include "RialtoServerLogging.h"
#include "mediapipelinemodule.pb.h"
#include <IIpcServer.h>

namespace
{
firebolt::rialto::PlaybackStateChangeEvent_PlaybackState
convertPlaybackState(const firebolt::rialto::PlaybackState &playbackState)
{
    switch (playbackState)
    {
    case firebolt::rialto::PlaybackState::UNKNOWN:
    {
        return firebolt::rialto::PlaybackStateChangeEvent_PlaybackState_UNKNOWN;
    }
    case firebolt::rialto::PlaybackState::IDLE:
    {
        return firebolt::rialto::PlaybackStateChangeEvent_PlaybackState_IDLE;
    }
    case firebolt::rialto::PlaybackState::PLAYING:
    {
        return firebolt::rialto::PlaybackStateChangeEvent_PlaybackState_PLAYING;
    }
    case firebolt::rialto::PlaybackState::PAUSED:
    {
        return firebolt::rialto::PlaybackStateChangeEvent_PlaybackState_PAUSED;
    }
    case firebolt::rialto::PlaybackState::SEEKING:
    {
        return firebolt::rialto::PlaybackStateChangeEvent_PlaybackState_SEEKING;
    }
    case firebolt::rialto::PlaybackState::SEEK_DONE:
    {
        return firebolt::rialto::PlaybackStateChangeEvent_PlaybackState_SEEK_DONE;
    }
    case firebolt::rialto::PlaybackState::STOPPED:
    {
        return firebolt::rialto::PlaybackStateChangeEvent_PlaybackState_STOPPED;
    }
    case firebolt::rialto::PlaybackState::END_OF_STREAM:
    {
        return firebolt::rialto::PlaybackStateChangeEvent_PlaybackState_END_OF_STREAM;
    }
    case firebolt::rialto::PlaybackState::FAILURE:
    {
        return firebolt::rialto::PlaybackStateChangeEvent_PlaybackState_FAILURE;
    }
    }
    return firebolt::rialto::PlaybackStateChangeEvent_PlaybackState_UNKNOWN;
}

firebolt::rialto::NetworkStateChangeEvent_NetworkState
convertNetworkState(const firebolt::rialto::NetworkState &networkState)
{
    switch (networkState)
    {
    case firebolt::rialto::NetworkState::UNKNOWN:
    {
        return firebolt::rialto::NetworkStateChangeEvent_NetworkState_UNKNOWN;
    }
    case firebolt::rialto::NetworkState::IDLE:
    {
        return firebolt::rialto::NetworkStateChangeEvent_NetworkState_IDLE;
    }
    case firebolt::rialto::NetworkState::BUFFERING:
    {
        return firebolt::rialto::NetworkStateChangeEvent_NetworkState_BUFFERING;
    }
    case firebolt::rialto::NetworkState::BUFFERING_PROGRESS:
    {
        return firebolt::rialto::NetworkStateChangeEvent_NetworkState_BUFFERING_PROGRESS;
    }
    case firebolt::rialto::NetworkState::BUFFERED:
    {
        return firebolt::rialto::NetworkStateChangeEvent_NetworkState_BUFFERED;
    }
    case firebolt::rialto::NetworkState::STALLED:
    {
        return firebolt::rialto::NetworkStateChangeEvent_NetworkState_STALLED;
    }
    case firebolt::rialto::NetworkState::FORMAT_ERROR:
    {
        return firebolt::rialto::NetworkStateChangeEvent_NetworkState_FORMAT_ERROR;
    }
    case firebolt::rialto::NetworkState::NETWORK_ERROR:
    {
        return firebolt::rialto::NetworkStateChangeEvent_NetworkState_NETWORK_ERROR;
    }
    case firebolt::rialto::NetworkState::DECODE_ERROR:
    {
        return firebolt::rialto::NetworkStateChangeEvent_NetworkState_DECODE_ERROR;
    }
    }
    return firebolt::rialto::NetworkStateChangeEvent_NetworkState_UNKNOWN;
}

firebolt::rialto::PlaybackErrorEvent_PlaybackError convertPlaybackError(const firebolt::rialto::PlaybackError &playbackError)
{
    switch (playbackError)
    {
    case firebolt::rialto::PlaybackError::UNKNOWN:
    {
        return firebolt::rialto::PlaybackErrorEvent_PlaybackError_UNKNOWN;
    }
    case firebolt::rialto::PlaybackError::DECRYPTION:
    {
        return firebolt::rialto::PlaybackErrorEvent_PlaybackError_DECRYPTION;
    }
    }
    return firebolt::rialto::PlaybackErrorEvent_PlaybackError_UNKNOWN;
}
} // namespace

namespace firebolt::rialto::server::ipc
{
MediaPipelineClient::MediaPipelineClient(int sessionId, const std::shared_ptr<::firebolt::rialto::ipc::IClient> &ipcClient)
    : m_sessionId{sessionId}, m_ipcClient{ipcClient}
{
}

MediaPipelineClient::~MediaPipelineClient() {}

void MediaPipelineClient::notifyDuration(int64_t duration)
{
    RIALTO_SERVER_LOG_WARN("Notify Duration not supported");
}

void MediaPipelineClient::notifyPosition(int64_t position)
{
    RIALTO_SERVER_LOG_DEBUG("Sending PositionChangeEvent");

    auto event = std::make_shared<firebolt::rialto::PositionChangeEvent>();
    event->set_session_id(m_sessionId);
    event->set_position(position);

    m_ipcClient->sendEvent(event);
}

void MediaPipelineClient::notifyNativeSize(uint32_t width, uint32_t height, double aspect)
{
    RIALTO_SERVER_LOG_WARN("Notify Native Size not supported");
}

void MediaPipelineClient::notifyNetworkState(NetworkState state)
{
    RIALTO_SERVER_LOG_DEBUG("Sending NetworkStateChangeEvent");

    auto event = std::make_shared<firebolt::rialto::NetworkStateChangeEvent>();
    event->set_session_id(m_sessionId);
    event->set_state(convertNetworkState(state));

    m_ipcClient->sendEvent(event);
}

void MediaPipelineClient::notifyPlaybackState(PlaybackState state)
{
    RIALTO_SERVER_LOG_DEBUG("Sending PlaybackStateChangeEvent...");

    auto event = std::make_shared<firebolt::rialto::PlaybackStateChangeEvent>();
    event->set_session_id(m_sessionId);
    event->set_state(convertPlaybackState(state));

    m_ipcClient->sendEvent(event);
}

void MediaPipelineClient::notifyVideoData(bool hasData)
{
    RIALTO_SERVER_LOG_WARN("Notify Video Data not supported");
}

void MediaPipelineClient::notifyAudioData(bool hasData)
{
    RIALTO_SERVER_LOG_WARN("Notify Audio Data not supported");
}

void MediaPipelineClient::notifyNeedMediaData(int32_t sourceId, size_t frameCount, uint32_t needDataRequestId,
                                              const std::shared_ptr<MediaPlayerShmInfo> &shmInfo)
{
    RIALTO_SERVER_LOG_DEBUG("Sending NeedMediaDataEvent...");

    auto event = std::make_shared<firebolt::rialto::NeedMediaDataEvent>();
    event->set_session_id(m_sessionId);
    event->set_source_id(sourceId);
    event->set_request_id(needDataRequestId);
    event->set_frame_count(frameCount);
    event->mutable_shm_info()->set_max_metadata_bytes(shmInfo->maxMetadataBytes);
    event->mutable_shm_info()->set_metadata_offset(shmInfo->metadataOffset);
    event->mutable_shm_info()->set_media_data_offset(shmInfo->mediaDataOffset);
    event->mutable_shm_info()->set_max_media_bytes(shmInfo->maxMediaBytes);

    m_ipcClient->sendEvent(event);
}

void MediaPipelineClient::notifyCancelNeedMediaData(int32_t sourceId)
{
    RIALTO_SERVER_LOG_WARN("Notify Cancel Need Media Data not supported");
}

void MediaPipelineClient::notifyQos(int32_t sourceId, const QosInfo &qosInfo)
{
    RIALTO_SERVER_LOG_DEBUG("Sending QosEvent...");

    auto event = std::make_shared<firebolt::rialto::QosEvent>();
    event->set_session_id(m_sessionId);
    event->set_source_id(sourceId);
    event->mutable_qos_info()->set_processed(qosInfo.processed);
    event->mutable_qos_info()->set_dropped(qosInfo.dropped);

    m_ipcClient->sendEvent(event);
}

void MediaPipelineClient::notifyBufferUnderflow(int32_t sourceId)
{
    RIALTO_SERVER_LOG_DEBUG("Sending BufferUnderflowEvent...");

    auto event = std::make_shared<firebolt::rialto::BufferUnderflowEvent>();
    event->set_session_id(m_sessionId);
    event->set_source_id(sourceId);

    m_ipcClient->sendEvent(event);
}

void MediaPipelineClient::notifyPlaybackError(int32_t sourceId, PlaybackError error)
{
    RIALTO_SERVER_LOG_DEBUG("Sending notifyPlaybackError...");

    auto event = std::make_shared<firebolt::rialto::PlaybackErrorEvent>();
    event->set_session_id(m_sessionId);
    event->set_source_id(sourceId);
    event->set_error(convertPlaybackError(error));

    m_ipcClient->sendEvent(event);
}

void MediaPipelineClient::notifySourceFlushed(int32_t sourceId)
{
    RIALTO_SERVER_LOG_DEBUG("Sending SourceFlushedEvent...");

    auto event = std::make_shared<firebolt::rialto::SourceFlushedEvent>();
    event->set_session_id(m_sessionId);
    event->set_source_id(sourceId);

    m_ipcClient->sendEvent(event);
}
} // namespace firebolt::rialto::server::ipc
