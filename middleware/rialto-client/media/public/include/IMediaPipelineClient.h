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

#ifndef FIREBOLT_RIALTO_I_MEDIA_PIPELINE_CLIENT_H_
#define FIREBOLT_RIALTO_I_MEDIA_PIPELINE_CLIENT_H_

#include <MediaCommon.h>
#include <memory>
#include <stdint.h>
#include <string>

/**
 * @file IMediaPipelineClient.h
 *
 * The definition of the IMediaPipelineClient interface.
 *
 * This file comprises the definition of the IMediaPipelineClient abstract
 * class. This is the API by which a IMediaPipeline implementation will
 * pass notifications to its client.
 */

namespace firebolt::rialto
{
/* Values representing custom duration values, used by IMediaPipelineClient::notifyDuration() method*/
/**
 * @brief Stream duration is unknown or undefined
 *
 */
constexpr int64_t kDurationUnknown{-1};
/**
 * @brief Stream duration is unending or live
 */
constexpr int64_t kDurationUnending{-2};

/**
 * @brief The Rialto media player client interface.
 *
 * This is The Rialto media player client abstract base class. It should be
 * implemented by any object that wishes to be notified by changes in the
 * state of the player or that provides data for media playback.
 */
class IMediaPipelineClient
{
public:
    /**
     * @brief Virtual destructor
     */
    virtual ~IMediaPipelineClient() {}

    /**
     * @brief Notifies the client of the total duration of the media.
     *
     * This method notifies the client of the total duration of the
     * media in nanoseconds. If this is unknown or undefined then a value
     * of kDurationUnknown. If the stream is unending, for example
     * live, then a value of kDurationUnending should be used.
     *
     * @param[in] duration : The duration of the media in seconds.
     */
    virtual void notifyDuration(int64_t duration) = 0;

    /**
     * @brief Notifies the client of the current playback position.
     *
     * This method notifies the client of the current playback position
     * in nanoseconds.
     *
     * When playing this should be called often enough to provide
     * sufficient granularity of position reporting. Typically this will
     * be every 0.25s.
     *
     * @param[in] position : The playback position in nanoseconds.
     */
    virtual void notifyPosition(int64_t position) = 0;

    /**
     * @brief Notifies the client of the native size of the video and
              the pixel aspect ratio.
     *
     * This method should be called one or more times to reporting the
     * native video size. If the video size changes the method should be
     * called with the updated size or aspect ratio.
     *
     * @param[in] width  : The width in pixels.
     * @param[in] height : The height in pixels.
     * @param[in] aspect : The pixel aspect ratio.
     */
    virtual void notifyNativeSize(uint32_t width, uint32_t height, double aspect = 1.0) = 0;

    /**
     * @brief Notifies the client of the network state.
     *
     * The network state reflects the state of the network. For backend
     * streaming, this is important as the backend uses the network to obtain the media data directly.
     *
     * For streaming that uses the browser to obtain data, say Media Source
     * Extensions playback, only the states NetworkState::IDLE,
     * NetworkState::BUFFERED and NetworkState::DECODE_ERROR should be
     * indicated by the backend.
     *
     * @param[in] state : The new network state.
     */
    virtual void notifyNetworkState(NetworkState state) = 0;

    /**
     * @brief Notifies the client of the playback state.
     *
     * The player will start IDLE. Once play() has been called the player
     * will be PLAYING, or once pause() has been called the player will be
     * PAUSED. A seek() request will result in SEEKING and once the seek
     * is complete SEEK_DONE will be issued followed by PLAYING. The STOPPED
     * state will be issued after a stop() request.
     *
     * @param[in] state : The new playback state.
     */
    virtual void notifyPlaybackState(PlaybackState state) = 0;

    /**
     * @brief Notifies the client that video data is available
     *
     * @param[in] hasData: true if video data is available.
     */
    virtual void notifyVideoData(bool hasData) = 0;

    /**
     * @brief Notifies the client that audio data is available
     *
     * @param[in] hasData: true if audio data is available.
     */
    virtual void notifyAudioData(bool hasData) = 0;

    /**
     * @brief Notifies the client that we need media data.
     *
     * This method notifies the client that we need media data from the
     * client. This is only used when Media Source Extensions are used.
     * In that case media is read by JavaScript and buffered by the
     * browser before being passed to this API for decoding.
     *
     * You cannot request data if a data request is currently pending.
     *
     * The frames the client sends should meet the criteria:
     *     numFramesSent <= frameCount
     *     numBytesSent <= maxMediaBytes
     *
     * @param[in] sourceId          : The source to read data from.
     * @param[in] frameCount        : The number of frames to read.
     * @param[in] needDataRequestId : Need data request id.
     * @param[in] shmInfo           : Information for populating the shared memory (null if not applicable to the client).
     */
    virtual void notifyNeedMediaData(int32_t sourceId, size_t frameCount, uint32_t needDataRequestId,
                                     const std::shared_ptr<MediaPlayerShmInfo> &shmInfo) = 0;

    /**
     * @brief Notifies the client to cancel any outstand need request.
     *
     * This method notifies the client to cancel any data request made using
     * notifyNeedMediaData(). It is not an error to cancel a request for data
     * when one is not pending.
     *
     * @param[in] sourceId : The source id to cancel the request for.
     */
    virtual void notifyCancelNeedMediaData(int32_t sourceId) = 0;

    /**
     * @brief Notifies the client of a Quality Of Service update from the Player.
     *
     * Notification shall be sent whenever a video/audio buffer drops a frame/sample.
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
     * @param[in] error     : The type of error that occured.
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

}; // namespace firebolt::rialto

#endif // FIREBOLT_RIALTO_I_MEDIA_PIPELINE_CLIENT_H_
