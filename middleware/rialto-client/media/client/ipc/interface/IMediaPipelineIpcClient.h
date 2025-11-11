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

#ifndef FIREBOLT_RIALTO_CLIENT_I_MEDIA_PIPELINE_IPC_CLIENT_H_
#define FIREBOLT_RIALTO_CLIENT_I_MEDIA_PIPELINE_IPC_CLIENT_H_

#include <MediaCommon.h>
#include <memory>
#include <stdint.h>
#include <string>

/**
 * @file IMediaPipelineIpcClient.h
 *
 * The definition of the IMediaPipelineIpcClient interface.
 */

namespace firebolt::rialto::client
{
/**
 * @brief The Rialto media player client ipc interface.
 */
class IMediaPipelineIpcClient
{
public:
    IMediaPipelineIpcClient() = default;
    virtual ~IMediaPipelineIpcClient() = default;

    IMediaPipelineIpcClient(const IMediaPipelineIpcClient &) = delete;
    IMediaPipelineIpcClient &operator=(const IMediaPipelineIpcClient &) = delete;
    IMediaPipelineIpcClient(IMediaPipelineIpcClient &&) = delete;
    IMediaPipelineIpcClient &operator=(IMediaPipelineIpcClient &&) = delete;

    /**
     * @brief Notifies the rialto client of the playback state.
     *
     * @param[in] state : The new playback state.
     */
    virtual void notifyPlaybackState(PlaybackState state) = 0;

    /**
     * @brief Handler for a playback position update from the server.
     *
     * @param[in] event : The playback position changed event structure.
     */
    virtual void notifyPosition(int64_t position) = 0;

    /**
     * @brief Notifies the rialto client of the network state.
     *
     * @param[in] state : The new network state.
     */
    virtual void notifyNetworkState(NetworkState state) = 0;

    /**
     * @brief Notifies the rialto client of a need data request.
     *
     * @param[in] sourceId      : The source to read data from.
     * @param[in] frameCount    : The number of frames to read.
     * @param[in] requestId     : Need data request id.
     * @param[in] shmInfo       : Information for populating the shared memory (null if not applicable to the client).
     */
    virtual void notifyNeedMediaData(int32_t sourceId, size_t frameCount, uint32_t requestId,
                                     const std::shared_ptr<MediaPlayerShmInfo> &shmInfo) = 0;

    /**
     * @brief Notifies the rialto client of a Quality Of Service update.
     *
     * @param[in] sourceId  : The id of the source that produced the Qos.
     * @param[in] qosInfo   : The information provided in the update.
     */
    virtual void notifyQos(int32_t sourceId, const QosInfo &qosInfo) = 0;

    /**
     * @brief Notifies the client that buffer underflow occurred.
     *
     * Notification shall be sent whenever a video/audio buffer underflow occurs
     *
     * @param[in] sourceId  : The id of the source that produced the buffer underflow
     */
    virtual void notifyBufferUnderflow(int32_t sourceId) = 0;

    /**
     * @brief Notifies the client that a non-fatal error has occurred in the player.
     *
     * PlaybackState remains unchanged when an error occurs.
     *
     * @param[in] sourceId  : The id of the source that produced the error.
     * @param[in] error     : The type of error that occurred.
     */
    virtual void notifyPlaybackError(int32_t sourceId, PlaybackError error) = 0;

    /**
     * @brief Notifies the client that the source has been flushed.
     *
     * Notification shall be sent whenever a flush procedure is finished.
     *
     * @param[in] sourceId  : The id of the source that has been flushed.
     */
    virtual void notifySourceFlushed(int32_t sourceId) = 0;
};

}; // namespace firebolt::rialto::client

#endif // FIREBOLT_RIALTO_CLIENT_I_MEDIA_PIPELINE_IPC_CLIENT_H_
