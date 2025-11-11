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

#ifndef FIREBOLT_RIALTO_SERVER_I_GST_GENERIC_PLAYER_PRIVATE_H_
#define FIREBOLT_RIALTO_SERVER_I_GST_GENERIC_PLAYER_PRIVATE_H_

#include "IMediaPipeline.h"

#include <gst/app/gstappsrc.h>
#include <gst/gst.h>
#include <memory>
#include <string>
#include <vector>

namespace firebolt::rialto::server
{
class IGstGenericPlayerPrivate
{
public:
    IGstGenericPlayerPrivate() = default;
    virtual ~IGstGenericPlayerPrivate() = default;

    IGstGenericPlayerPrivate(const IGstGenericPlayerPrivate &) = delete;
    IGstGenericPlayerPrivate &operator=(const IGstGenericPlayerPrivate &) = delete;
    IGstGenericPlayerPrivate(IGstGenericPlayerPrivate &&) = delete;
    IGstGenericPlayerPrivate &operator=(IGstGenericPlayerPrivate &&) = delete;

    /**
     * @brief Schedules need media data task. Called by the worker thread.
     */
    virtual void scheduleNeedMediaData(GstAppSrc *src) = 0;

    /**
     * @brief Schedules enough data task. Called by the worker thread.
     */
    virtual void scheduleEnoughData(GstAppSrc *src) = 0;

    /**
     * @brief Schedules audio underflow task. Called by the worker thread.
     */
    virtual void scheduleAudioUnderflow() = 0;

    /**
     * @brief Schedules video underflow task. Called by the worker thread.
     */
    virtual void scheduleVideoUnderflow() = 0;

    /**
     * @brief Schedules all sources attached task. Called by the worker thread.
     */
    virtual void scheduleAllSourcesAttached() = 0;

    /**
     * @brief Sets video sink rectangle. Called by the worker thread.
     *
     * @retval true on success.
     */
    virtual bool setVideoSinkRectangle() = 0;

    /**
     * @brief Sets immediate output. Called by the worker thread.
     *
     * @retval true on success.
     */
    virtual bool setImmediateOutput() = 0;

    /**
     * @brief Sets the low latency property. Called by the worker thread.
     *
     * @retval true on success.
     */
    virtual bool setLowLatency() = 0;

    /**
     * @brief Sets the sync property. Called by the worker thread.
     *
     * @retval true on success.
     */
    virtual bool setSync() = 0;

    /**
     * @brief Sets the sync off property. Called by the worker thread.
     *
     * @retval true on success.
     */
    virtual bool setSyncOff() = 0;

    /**
     * @brief Sets the stream sync mode property. Called by the worker thread.
     *
     * @retval true on success.
     */
    virtual bool setStreamSyncMode(const MediaSourceType &type) = 0;

    /**
     * @brief Sets frame rendering. Called by the worker thread.
     *
     * @retval true on success.
     */
    virtual bool setRenderFrame() = 0;

    /**
     * @brief Sets buffering limit. Called by the worker thread.
     *
     * @retval true on success.
     */
    virtual bool setBufferingLimit() = 0;

    /**
     * @brief Sets use buffering. Called by the worker thread.
     *
     * @retval true on success.
     */
    virtual bool setUseBuffering() = 0;

    /**
     * @brief Sets Show Video Window property. Called by the worker thread.
     *
     * @retval true on success.
     */
    virtual bool setShowVideoWindow() = 0;

    /**
     * @brief Sends NeedMediaData notification. Called by the worker thread.
     */
    virtual void notifyNeedMediaData(const MediaSourceType mediaSource) = 0;

    /**
     * @brief Constructs a new buffer with data from media segment. Does not perform decryption.
     *        Called by the worker thread.
     */
    virtual GstBuffer *createBuffer(const IMediaPipeline::MediaSegment &mediaSegment) const = 0;

    virtual void attachData(const firebolt::rialto::MediaSourceType mediaType) = 0;

    /**
     * @brief Checks the new audio mediaSegment metadata and updates the caps accordingly.
     */
    virtual void updateAudioCaps(int32_t rate, int32_t channels, const std::shared_ptr<CodecData> &codecData) = 0;

    /**
     * @brief Checks the new video mediaSegment metadata and updates the caps accordingly.
     */
    virtual void updateVideoCaps(int32_t width, int32_t height, Fraction frameRate,
                                 const std::shared_ptr<CodecData> &codecData) = 0;

    /**
     * @brief Adds clipping meta to the audio buffer.
     *
     * @param buffer the buffer to add the clipping meta to
     * @param clippingStart the start of the clipping
     * @param clippingEnd the end of the clipping
     */
    virtual void addAudioClippingToBuffer(GstBuffer *buffer, uint64_t clippingStart, uint64_t clippingEnd) const = 0;

    /**
     * @brief Changes pipeline state.
     *
     * @param[in] newState    : The desired state.
     *
     * @retval true on success.
     */
    virtual bool changePipelineState(GstState newState) = 0;

    /**
     * @brief Gets the current position of the element
     *
     * @param[in] element : The GstElement to check.
     *
     * @retval position of the element; -1 in case of failure
     */
    virtual int64_t getPosition(GstElement *element) = 0;

    /**
     * @brief Starts position reporting and check audio underflow. Called by the worker thread.
     */
    virtual void startPositionReportingAndCheckAudioUnderflowTimer() = 0;

    /**
     * @brief Stops position reporting and check audio underflow. Called by the worker thread.
     */
    virtual void stopPositionReportingAndCheckAudioUnderflowTimer() = 0;

    /**
     * @brief Starts subtitle clock resync. Called by the worker thread.
     */
    virtual void startSubtitleClockResyncTimer() = 0;

    /**
     * @brief Stops subtitle clock resync. Called by the worker thread.
     */
    virtual void stopSubtitleClockResyncTimer() = 0;

    /**
     * @brief Stops worker thread. Called by the worker thread.
     */
    virtual void stopWorkerThread() = 0;

    /**
     * @brief Restores playback after underflow. Called by the worker thread.
     *
     * @param[in] underflowFlag    : The audio or video underflow flag to be cleared.
     */
    virtual void cancelUnderflow(firebolt::rialto::MediaSourceType mediaSource) = 0;

    /**
     * @brief Sets pending playback rate after reaching PLAYING state
     *
     */
    virtual void setPendingPlaybackRate() = 0;

    /**
     * @brief Updates Playback Group in PlayerContext.
     */
    virtual void updatePlaybackGroup(GstElement *typefind, const GstCaps *caps) = 0;

    /**
     * @brief Notification that a new child element has been added to the autovideosink.
     *        Stores the child video sink in the player context.
     *
     * @param[in] object    : Element added to the autovideosink.
     */
    virtual void addAutoVideoSinkChild(GObject *object) = 0;

    /**
     * @brief Notification that a new child element has been added to the autoaudiosink.
     *        Stores the child audio sink in the player context.
     *
     * @param[in] object    : Element added to the autoaudiosink.
     */
    virtual void addAutoAudioSinkChild(GObject *object) = 0;

    /**
     * @brief Notification that a child element has been removed from the autovideosink.
     *        Removes the child video sink in the player context if it has been stored.
     *
     * @param[in] object    : Element removed from the autovideosink.
     */
    virtual void removeAutoVideoSinkChild(GObject *object) = 0;

    /**
     * @brief Notification that a child element has been removed from the autoaudiosink.
     *        Removes the child audio sink in the player context if it has been stored.
     *
     * @param[in] object    : Element removed from the autoaudiosink.
     */
    virtual void removeAutoAudioSinkChild(GObject *object) = 0;

    /**
     * @brief Gets the sink element for source type.
     *
     * @param[in] mediaSourceType : the source type to obtain the sink for
     *
     * @retval The sink, NULL if not found. Please call getObjectUnref() if it's non-null
     */
    virtual GstElement *getSink(const MediaSourceType &mediaSourceType) const = 0;

    /**
     * @brief Sets the audio and video flags on the pipeline based on the input.
     *
     * @param[in] enableAudio : Whether to enable audio flags.
     */
    virtual void setPlaybinFlags(bool enableAudio) = 0;

    /**
     * @brief Pushes GstSample if playback position has changed or new segment needs to be sent.
     *
     * @param[in] source          : The Gst Source element, that should receive new sample
     * @param[in] typeStr         : The media source type string
     */
    virtual void pushSampleIfRequired(GstElement *source, const std::string &typeStr) = 0;

    /**
     * @brief Reattaches source (or switches it)
     *
     * @param[in] source          : The new media source
     *
     * @retval True on success
     */
    virtual bool reattachSource(const std::unique_ptr<IMediaPipeline::MediaSource> &source) = 0;

    /**
     * @brief Checks if the player has a source of the given type.
     *
     * @param[in] mediaSourceType : The source type to check
     *
     * @retval True if the player has a source of the given type, false otherwise
     */
    virtual bool hasSourceType(const MediaSourceType &mediaSourceType) const = 0;

    /**
     * @brief Sets source state flushed
     *
     * @param[in] mediaSourceType : the source type that has been flushed
     */
    virtual void setSourceFlushed(const MediaSourceType &mediaSourceType) = 0;

    /**
     * @brief Clears need data scheduled flag
     *
     * @param[in] src : the app source
     */
    virtual void clearNeedDataScheduled(GstAppSrc *src) = 0;
};
} // namespace firebolt::rialto::server

#endif // FIREBOLT_RIALTO_SERVER_I_GST_GENERIC_PLAYER_PRIVATE_H_
