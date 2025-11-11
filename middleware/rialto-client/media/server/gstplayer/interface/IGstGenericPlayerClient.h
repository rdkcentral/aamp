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

#ifndef FIREBOLT_RIALTO_SERVER_I_GST_GENERIC_PLAYER_CLIENT_H_
#define FIREBOLT_RIALTO_SERVER_I_GST_GENERIC_PLAYER_CLIENT_H_

#include <MediaCommon.h>
#include <stdint.h>
#include <string>

/**
 * @file IGstGenericPlayerClient.h
 *
 * The definition of the IGstGenericPlayerClient interface.
 *
 * This file comprises the definition of the IGstGenericPlayerClient abstract
 * class. This is the API by which a IGstGenericPlayer implementation will
 * pass notifications to its client.
 */

namespace firebolt::rialto::server
{
/**
 * @brief The Rialto gstreamer player client interface.
 *
 * This is The Rialto gstreamer player client abstract base class. It should be
 * implemented by any object that wishes to be notified by changes in the
 * state of the gstreamer player or that provides data for playback.
 */
class IGstGenericPlayerClient
{
public:
    IGstGenericPlayerClient() = default;
    virtual ~IGstGenericPlayerClient() = default;

    IGstGenericPlayerClient(const IGstGenericPlayerClient &) = delete;
    IGstGenericPlayerClient &operator=(const IGstGenericPlayerClient &) = delete;
    IGstGenericPlayerClient(IGstGenericPlayerClient &&) = delete;
    IGstGenericPlayerClient &operator=(IGstGenericPlayerClient &&) = delete;

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
     *     numBytesSent <= maxBytes
     *
     * @param[in] mediaSourceType      : The media type of source to read data from.
     *
     * @retval True on success.
     */
    virtual bool notifyNeedMediaData(MediaSourceType mediaSourceType) = 0;

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
     *
     */
    virtual void notifyPosition(std::int64_t position) = 0;

    /**
     * @brief Notifies the client of the network state.
     *
     * The network state reflects the state of the network. For backend
     * streaming, say using MediaPipelineURLDelegate, this is important
     * as the backend uses the network to obtain the media data directly.
     *
     * For streaming that uses the browser to obtain data, say Media Source
     * Extensions playback, only the states NetworkState::IDLE,
     * NetworkState::BUFFERED and NetworkState::DECODE_ERROR should be
     * indicated by the backend.
     *
     * @param[in] state : The new network state.
     *
     */
    virtual void notifyNetworkState(NetworkState state) = 0;

    /**
     * @brief Clears all active NeedMediaDataRequests cache for session
     *
     */
    virtual void clearActiveRequestsCache() = 0;

    /**
     * @brief Clears all active NeedMediaDataRequests cache for given Media Source Type
     *
     * @param[in] type : Media source type.
     *
     */
    virtual void invalidateActiveRequests(const MediaSourceType &type) = 0;

    /**
     * @brief Notifies the client of a Quality Of Service update from the Player.
     *
     * @param[in] qosInfo       : The Qos infomation extracted from the message.
     * @param[in] sourceType    : The type of source that sent the message.
     */
    virtual void notifyQos(MediaSourceType mediaSourceType, const QosInfo &qosInfo) = 0;

    /**
     * @brief Notifies the client that buffer underflow occurred.
     *
     * Notification shall be sent whenever a video/audio buffer underflow occurs
     *
     * @param[in] mediaSourceType  : The type of the source that produced the buffer underflow
     */
    virtual void notifyBufferUnderflow(MediaSourceType mediaSourceType) = 0;

    /**
     * @brief Notifies the client that a non-fatal error has occurred in the player.
     *
     * PlaybackState remains unchanged when an error occurs.
     *
     * @param[in] mediaSourceType  : The type of the source that produced the error.
     * @param[in] error            : The type of error that occurred.
     */
    virtual void notifyPlaybackError(MediaSourceType mediaSourceType, PlaybackError error) = 0;

    /**
     * @brief Notifies the client that the source has been flushed.
     *
     * Notification shall be sent whenever a flush procedure is finished.
     *
     * @param[in] mediaSourceType  : The type of the source that has been flushed.
     */
    virtual void notifySourceFlushed(MediaSourceType mediaSourceType) = 0;
};

}; // namespace firebolt::rialto::server

#endif // FIREBOLT_RIALTO_SERVER_I_GST_GENERIC_PLAYER_CLIENT_H_
