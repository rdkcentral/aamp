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

#ifndef FIREBOLT_RIALTO_SERVER_I_GST_GENERIC_PLAYER_H_
#define FIREBOLT_RIALTO_SERVER_I_GST_GENERIC_PLAYER_H_

#include <MediaCommon.h>
#include <memory>
#include <stdint.h>
#include <string>

#include "IDataReader.h"
#include "IDecryptionService.h"
#include "IGstGenericPlayerClient.h"
#include "IHeartbeatHandler.h"
#include "IMediaPipeline.h"
#include "IRdkGstreamerUtilsWrapper.h"

namespace firebolt::rialto::server
{
class IGstGenericPlayer;

/**
 * @brief IGstGenericPlayer factory class, returns a concrete implementation of IGstGenericPlayer
 */
class IGstGenericPlayerFactory
{
public:
    IGstGenericPlayerFactory() = default;
    virtual ~IGstGenericPlayerFactory() = default;

    /**
     * @brief Gets the IGstGenericPlayerFactory instance.
     *
     * @retval the factory instance or null on error.
     */
    static std::shared_ptr<IGstGenericPlayerFactory> getFactory();

    /**
     * @brief Creates a IGstGenericPlayer object.
     *
     * @param[in] client            : The gstreamer player client.
     * @param[in] decryptionService : The decryption service.
     * @param[in] type              : The media type the gstreamer player shall support.
     * @param[in] videoRequirements : The video requirements for the playback.
     *
     * @retval the new player instance or null on error.
     */
    virtual std::unique_ptr<IGstGenericPlayer>
    createGstGenericPlayer(IGstGenericPlayerClient *client, IDecryptionService &decryptionService, MediaType type,
                           const VideoRequirements &videoRequirements,
                           const std::shared_ptr<firebolt::rialto::wrappers::IRdkGstreamerUtilsWrapperFactory>
                               &rdkGstreamerUtilsWrapperFactory) = 0;
};

class IGstGenericPlayer
{
public:
    IGstGenericPlayer() = default;
    virtual ~IGstGenericPlayer() = default;

    IGstGenericPlayer(const IGstGenericPlayer &) = delete;
    IGstGenericPlayer &operator=(const IGstGenericPlayer &) = delete;
    IGstGenericPlayer(IGstGenericPlayer &&) = delete;
    IGstGenericPlayer &operator=(IGstGenericPlayer &&) = delete;

    /**
     * @brief Attaches a source to gstreamer.
     *
     * @param[in] mediaSource : The media source.
     *
     */
    virtual void attachSource(const std::unique_ptr<IMediaPipeline::MediaSource> &mediaSource) = 0;

    /**
     * @brief Unattaches a source.
     *
     * @param[in] mediaSourceType : The media source type.
     *
     */
    virtual void removeSource(const MediaSourceType &mediaSourceType) = 0;

    /**
     * @brief Handles notification that all sources were attached
     *
     */
    virtual void allSourcesAttached() = 0;

    /**
     * @brief Starts playback of the media.
     *
     * This method is considered to be asynchronous and MUST NOT block
     * but should request playback and then return.
     *
     * Once the backend is successfully playing it should notify the
     * media player client of playback state PlaybackState::PLAYING.
     *
     */
    virtual void play() = 0;

    /**
     * @brief Pauses playback of the media.
     *
     * This method is considered to be asynchronous and MUST NOT block
     * but should request the playback pause and then return.
     *
     * Once the backend is successfully paused it should notify the
     * media player client of playback state PlaybackState::PAUSED.
     *
     */
    virtual void pause() = 0;

    /**
     * @brief Stops playback of the media.
     *
     * This method is considered to be asynchronous and MUST NOT block
     * but should request the playback stop and then return.
     *
     * Once the backend is successfully stopped it should notify the
     * media player client of playback state PlaybackState::STOPPED.
     *
     */
    virtual void stop() = 0;

    /**
     * @brief Sets video geometry
     *
     * @param[in] x      : X position of rectangle on video
     * @param[in] y      : Y position of rectangle on video
     * @param[in] width  : width of rectangle
     * @param[in] height : height of rectangle
     *
     */
    virtual void setVideoGeometry(int x, int y, int width, int height) = 0;

    /**
     * @brief Queues the end of stream notification at the end of the gstreamer buffers.
     *
     * @param[in] type : the media source type to set eos
     *
     */
    virtual void setEos(const firebolt::rialto::MediaSourceType &type) = 0;

    /**
     * @brief Attaches new samples
     *
     * This method is considered to be asynchronous and MUST NOT block
     * but should request to attach new sample and then return.
     */
    virtual void attachSamples(const IMediaPipeline::MediaSegmentVector &mediaSegments) = 0;

    /**
     * @brief Attaches new samples
     *
     * This method is considered to be asynchronous and MUST NOT block
     * but should request to attach new sample and then return.
     */
    virtual void attachSamples(const std::shared_ptr<IDataReader> &dataReader) = 0;

    /**
     * @brief Set the playback position in nanoseconds.
     *
     * If playback has not started this method sets the start position
     * for playback. If playback has started this method performs a seek.
     *
     * @param[in] position : The playback position in nanoseconds.
     *
     */
    virtual void setPosition(std::int64_t position) = 0;

    /**
     * @brief Get the playback position in nanoseconds.
     *
     * @param[out] position : The playback position in nanoseconds.
     *
     * @retval True on success
     */
    virtual bool getPosition(std::int64_t &position) = 0;

    /**
     * @brief Sets the "Immediate Output" property for this source.
     *
     * @param[in] mediaSourceType : The media source type
     * @param[in] immediateOutput : Set immediate output mode on the sink
     *
     * @retval true on success.
     */
    virtual bool setImmediateOutput(const MediaSourceType &mediaSourceType, bool immediateOutput) = 0;

    /**
     * @brief Gets the "Immediate Output" property for this source.
     *
     * @param[in] mediaSourceType : The media source type
     * @param[out] immediateOutput : Get immediate output mode on the sink
     *
     * @retval true on success.
     */
    virtual bool getImmediateOutput(const MediaSourceType &mediaSourceType, bool &immediateOutput) = 0;

    /**
     * @brief Get stats for this source.
     *
     * @param[in] mediaSourceType : The media source type to get stats for
     * @param[out] renderedFrames : The number of rendered frames
     * @param[out] droppedFrames : The number of dropped frames
     *
     * @retval true on success.
     */
    virtual bool getStats(const MediaSourceType &mediaSourceType, uint64_t &renderedFrames, uint64_t &droppedFrames) = 0;

    /**
     * @brief Set the playback rate.
     *
     * @param[in] rate : The playback rate.
     *
     */
    virtual void setPlaybackRate(double rate) = 0;

    /**
     * @brief Requests to render a prerolled frame
     *
     */
    virtual void renderFrame() = 0;

    /**
     * @brief Set level and transition of audio attenuation.
     *        Sets the current volume for the pipeline (0.0 silent -> 1.0 full volume)
     *
     * @param[in] volume : Target volume level (0.0 - 1.0)
     */
    virtual void setVolume(double targetVolume, uint32_t volumeDuration, firebolt::rialto::EaseType easeType) = 0;

    /**
     * @brief Get current audio level. Fetches the current volume level for the pipeline.
     *
     * @param[out] volume : Current volume level (range 0.0 - 1.0)
     *
     * @retval True on success
     */
    virtual bool getVolume(double &volume) = 0;

    /**
     * @brief Set mute status of pipeline
     *
     * Muting does not change the underlying volyme setting so when
     * unmuted the user will hear audio at the same volume as previously
     * set.
     *
     * @param[in] mute : Desired mute state, true=muted, false=not muted
     */
    virtual void setMute(const MediaSourceType &mediaSourceType, bool mute) = 0;

    /**
     * @brief Get current mute status of the pipeline
     *
     * @param[out] mute : Current mute state
     *
     * @retval True in success, false otherwise
     */
    virtual bool getMute(const MediaSourceType &mediaSourceType, bool &mute) = 0;

    /**
     * @brief Change Text Track Identifier
     *
     * @param[in] textTrackIdentifier Text track identifier of subtitle stream
     *
     * @retval true on success false otherwise
     */
    virtual void setTextTrackIdentifier(const std::string &textTrackIdentifier) = 0;

    /**
     * @brief Get Text Track Identifier
     *
     * @param[in] textTrackIdentifier Text track identifier of subtitle stream
     *
     * @retval true on success false otherwise
     */
    virtual bool getTextTrackIdentifier(std::string &textTrackIdentifier) = 0;

    /**
     * @brief Set low latency property on the pipeline. Default false.
     *
     * @param[in] lowLatency : The low latency value to set.
     *
     * @retval true on success false otherwise
     */
    virtual bool setLowLatency(bool lowLatency) = 0;

    /**
     * @brief Set sync property on the pipeline. Default false.
     *
     * @param[in] sync : The sync value to set.
     *
     * @retval true on success false otherwise
     */
    virtual bool setSync(bool sync) = 0;

    /**
     * @brief Get sync property on the pipeline.
     *
     * @param[out] sync : Current sync value.
     *
     * @retval true on success false otherwise
     */
    virtual bool getSync(bool &sync) = 0;

    /**
     * @brief Set sync off property on the pipeline. Default false.
     *
     * @param[in] syncOff : The sync off value to set.
     *
     * @retval true on success false otherwise
     */
    virtual bool setSyncOff(bool syncOff) = 0;

    /**
     * @brief Set stream sync mode property on the pipeline. Default 0.
     *
     * @param[in] mediaSourceType : The media source type to set stream sync mode.
     * @param[in] streamSyncMode : The stream sync mode value to set.
     *
     * @retval true on success false otherwise
     */
    virtual bool setStreamSyncMode(const MediaSourceType &mediaSourceType, int32_t streamSyncMode) = 0;

    /**
     * @brief Get stream sync mode property on the pipeline.
     *
     * @param[out] streamSyncMode : Current stream sync mode value.
     *
     * @retval true on success false otherwise
     */
    virtual bool getStreamSyncMode(int32_t &streamSyncMode) = 0;

    /**
     * @brief Checks if worker thread is not deadlocked
     *
     * @param[out] heartbeatHandler : The heartbeat handler instance
     *
     */
    virtual void ping(std::unique_ptr<IHeartbeatHandler> &&heartbeatHandler) = 0;

    /**
     * @brief Flushes a source.
     *
     * @param[in] mediaSourceType : The media source type to flush.
     * @param[in] resetTime : True if time should be reset
     * @param[out] async     : True if flushed source is asynchronous (will preroll after flush)
     *
     */
    virtual void flush(const MediaSourceType &mediaSourceType, bool resetTime, bool &async) = 0;

    /**
     * @brief Set the source position in nanoseconds.
     *
     * This method sets the start position for a source.
     *
     * @param[in] mediaSourceType : The media source type to flush.
     * @param[in] position : The position in nanoseconds.
     * @param[in] resetTime : True if time should be reset
     * @param[in] appliedRate : The applied rate after seek
     * @param[in] stopPosition : The position of last pushed buffer
     */
    virtual void setSourcePosition(const MediaSourceType &mediaSourceType, int64_t position, bool resetTime,
                                   double appliedRate, uint64_t stopPosition) = 0;

    /**
     * @brief Sets the subtitle offset.
     *
     * This method sets the subtitle offset to synchronize subtitle timing.
     *
     * @param[in] position : The subtitle offset position in nanoseconds.
     */
    virtual void setSubtitleOffset(int64_t position) = 0;

    /**
     * @brief Process audio gap
     *
     * This method handles audio gap in order to avoid audio pops during transitions.
     *
     * @param[in] position         : Audio pts fade position value
     * @param[in] duration         : Audio pts fade duration
     * @param[in] discontinuityGap : Audio discontinuity gap
     * @param[in] audioAac         : True if audio codec is AAC
     */
    virtual void processAudioGap(int64_t position, uint32_t duration, int64_t discontinuityGap, bool audioAac) = 0;

    /**
     * @brief Set buffering limit
     *
     * This method enables/disables limit buffering and sets millisecond threshold used.
     * Use kInvalidLimitBuffering to disable limit buffering
     *
     * @param[in] limitBufferingMs         : buffering limit in ms
     *
     */
    virtual void setBufferingLimit(uint32_t limitBufferingMs) = 0;

    /**
     * @brief Get buffering limit
     *
     * This method returns current value of buffering limit in milliseconds
     * Method will return kInvalidLimitBuffering limit buffering is disabled
     *
     * @param[out] limitBufferingMs         : buffering limit in ms
     *
     * @retval true on success.
     */
    virtual bool getBufferingLimit(uint32_t &limitBufferingMs) = 0;

    /**
     * @brief Enables/disables the buffering option
     *
     * This method enables the buffering option so that BUFFERING messages are
     * emitted based on low-/high-percent thresholds.
     *
     * @param[in] useBuffering         : true if buffering option enabled.
     *
     */
    virtual void setUseBuffering(bool useBuffering) = 0;

    /**
     * @brief Checks, if buffering is enabled
     *
     * This method returns true, if buffering is enabled
     *
     * @param[out] useBuffering         : true if buffering option is enabled.
     *
     * @retval true on success.
     */
    virtual bool getUseBuffering(bool &useBuffering) = 0;

    /**
     * @brief Switches a source.
     *
     * @param[in] mediaSource : The media source.
     *
     */
    virtual void switchSource(const std::unique_ptr<IMediaPipeline::MediaSource> &mediaSource) = 0;
};

}; // namespace firebolt::rialto::server

#endif // FIREBOLT_RIALTO_SERVER_I_GST_GENERIC_PLAYER_H_
