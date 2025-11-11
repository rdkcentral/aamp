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

#ifndef FIREBOLT_RIALTO_SERVER_I_GENERIC_PLAYER_TASK_FACTORY_H_
#define FIREBOLT_RIALTO_SERVER_I_GENERIC_PLAYER_TASK_FACTORY_H_

#include "GenericPlayerContext.h"
#include "IDataReader.h"
#include "IFlushWatcher.h"
#include "IGstGenericPlayerPrivate.h"
#include "IHeartbeatHandler.h"
#include "IMediaPipeline.h"
#include "IPlayerTask.h"
#include "MediaCommon.h"
#include <cstdint>
#include <gst/app/gstappsrc.h>
#include <memory>
#include <string>

namespace firebolt::rialto::server
{
/**
 * @brief IGenericPlayerTaskFactory factory class, returns a concrete implementation of IPlayerTask
 */
class IGenericPlayerTaskFactory
{
public:
    IGenericPlayerTaskFactory() = default;
    virtual ~IGenericPlayerTaskFactory() = default;

    /**
     * @brief Creates a AttachSamples task.
     *
     * @param[in] context       : The GstGenericPlayer context
     * @param[in] player        : The GstGenericPlayer instance
     * @param[in] mediaSegments : The media segments to attach
     *
     * @retval the new AttachSamples task instance.
     */
    virtual std::unique_ptr<IPlayerTask>
    createAttachSamples(GenericPlayerContext &context, IGstGenericPlayerPrivate &player,
                        const IMediaPipeline::MediaSegmentVector &mediaSegments) const = 0;

    /**
     * @brief Creates a AttachSource task.
     *
     * @param[in] context   : The GstGenericPlayer context
     * @param[in] player    : The GstGenericPlayer instance
     * @param[in] source    : The source to attach.
     *
     * @retval the new AttachSource task instance.
     */
    virtual std::unique_ptr<IPlayerTask>
    createAttachSource(GenericPlayerContext &context, IGstGenericPlayerPrivate &player,
                       const std::unique_ptr<IMediaPipeline::MediaSource> &source) const = 0;

    /**
     * @brief Creates a DeepElementAdded task.
     *
     * @param[in] context  : The GstPlayer context
     * @param[in] player        : The GstGenericPlayer instance
     * @param[in] pipeline : The pipeline the signal was fired from.
     * @param[in] bin      : the GstBin the element was added to
     * @param[in] element  : an element that was added to the playbin hierarchy
     *
     * @retval the new DeepElementAdded task instance.
     */
    virtual std::unique_ptr<IPlayerTask> createDeepElementAdded(GenericPlayerContext &context,
                                                                IGstGenericPlayerPrivate &player, GstBin *pipeline,
                                                                GstBin *bin, GstElement *element) const = 0;

    /**
     * @brief Creates a EnoughData task.
     *
     * @param[in] context : The GstGenericPlayer context
     * @param[in] src     : The source, which reports enough data.
     *
     * @retval the new EnoughData task instance.
     */
    virtual std::unique_ptr<IPlayerTask> createEnoughData(GenericPlayerContext &context, GstAppSrc *src) const = 0;

    /**
     * @brief Creates a Eos task.
     *
     * @param[in] context : The GstGenericPlayer context
     * @param[in] type    : The media source type, which reports eos.
     *
     * @retval the new Eos task instance.
     */
    virtual std::unique_ptr<IPlayerTask> createEos(GenericPlayerContext &context, IGstGenericPlayerPrivate &player,
                                                   const firebolt::rialto::MediaSourceType &type) const = 0;

    /**
     * @brief Creates a FinishSetupSource task.
     *
     * @param[in] context       : The GstGenericPlayer context
     * @param[in] player        : The GstGenericPlayer instance
     *
     * @retval the new FinishSetupSource task instance.
     */
    virtual std::unique_ptr<IPlayerTask> createFinishSetupSource(GenericPlayerContext &context,
                                                                 IGstGenericPlayerPrivate &player) const = 0;

    /**
     * @brief Creates a HandleBusMessage task.
     *
     * @param[in] context      : The GstGenericPlayer context
     * @param[in] player       : The GstGenericPlayer instance
     * @param[in] message      : The message to be handled.
     * @param[in] flushWatcher : Flush watcher instance
     *
     * @retval the new HandleBusMessage task instance.
     */
    virtual std::unique_ptr<IPlayerTask> createHandleBusMessage(GenericPlayerContext &context,
                                                                IGstGenericPlayerPrivate &player, GstMessage *message,
                                                                const IFlushWatcher &flushWatcher) const = 0;

    /**
     * @brief Creates a NeedData task.
     *
     * @param[in] context : The GstGenericPlayer context
     * @param[in] player  : The GstGenericPlayer instance
     * @param[in] src     : The source, which reports need data.
     *
     * @retval the new NeedData task instance.
     */
    virtual std::unique_ptr<IPlayerTask> createNeedData(GenericPlayerContext &context, IGstGenericPlayerPrivate &player,
                                                        GstAppSrc *src) const = 0;

    /**
     * @brief Creates a Pause task.
     *
     * @param[in] context : The GstGenericPlayer context
     * @param[in] player  : The GstGenericPlayer instance
     *
     * @retval the new Pause task instance.
     */
    virtual std::unique_ptr<IPlayerTask> createPause(GenericPlayerContext &context,
                                                     IGstGenericPlayerPrivate &player) const = 0;

    /**
     * @brief Creates a Play task.
     *
     * @param[in] player        : The GstGenericPlayer instance
     *
     * @retval the new Play task instance.
     */
    virtual std::unique_ptr<IPlayerTask> createPlay(IGstGenericPlayerPrivate &player) const = 0;

    /**
     * @brief Creates a ReadShmDataAndAttachSamples task.
     *
     * @param[in] context       : The GstGenericPlayer context
     * @param[in] player        : The GstGenericPlayer instance
     * @param[in] dataReader    : The shared memory data reader
     *
     * @retval the new ReadShmDataAndAttachSamples task instance.
     */
    virtual std::unique_ptr<IPlayerTask>
    createReadShmDataAndAttachSamples(GenericPlayerContext &context, IGstGenericPlayerPrivate &player,
                                      const std::shared_ptr<IDataReader> &dataReader) const = 0;

    /**
     * @brief Creates a Remove Source task.
     *
     * @param[in] context : The GstPlayer context
     * @param[in] player  : The GstGenericPlayer instance
     * @param[in] type    : The media source type to remove
     *
     * @retval the new Remove Source task instance.
     */
    virtual std::unique_ptr<IPlayerTask> createRemoveSource(GenericPlayerContext &context,
                                                            IGstGenericPlayerPrivate &player,
                                                            const firebolt::rialto::MediaSourceType &type) const = 0;

    /**
     * @brief Creates a ReportPosition task.
     *
     * @param[in] context       : The GstGenericPlayer context
     * @param[in] player        : The GstGenericPlayer instance
     *
     * @retval the new ReportPosition task instance.
     */
    virtual std::unique_ptr<IPlayerTask> createReportPosition(GenericPlayerContext &context,
                                                              IGstGenericPlayerPrivate &player) const = 0;

    /**
     * @brief Creates a CheckAudioUnderflow task.
     *
     * @param[in] context       : The GstGenericPlayer context
     * @param[in] player        : The GstGenericPlayer instance
     *
     * @retval the new CheckAudioUnderflow task instance.
     */
    virtual std::unique_ptr<IPlayerTask> createCheckAudioUnderflow(GenericPlayerContext &context,
                                                                   IGstGenericPlayerPrivate &player) const = 0;

    /**
     * @brief Creates a SetPlaybackRate task.
     *
     * @param[in] context   : The GstGenericPlayer context
     * @param[in] rate      : The new playback rate.
     *
     * @retval the new SetPlaybackRate task instance.
     */
    virtual std::unique_ptr<IPlayerTask> createSetPlaybackRate(GenericPlayerContext &context, double rate) const = 0;

    /**
     * @brief Creates a SetPosition task.
     *
     * @param[in] context     : The GstGenericPlayer context
     * @param[in] player      : The GstGenericPlayer instance
     * @param[in] position    : The position to be set
     *
     * @retval the new SetPosition task instance.
     */
    virtual std::unique_ptr<IPlayerTask>
    createSetPosition(GenericPlayerContext &context, IGstGenericPlayerPrivate &player, std::int64_t position) const = 0;

    /**
     * @brief Creates a SetupElement task.
     *
     * @param[in] context    : The GstGenericPlayer context
     * @param[in] player     : The GstGenericPlayer instance
     * @param[in] element    : The element to be setup.
     *
     * @retval the new SetupElement task instance.
     */
    virtual std::unique_ptr<IPlayerTask>
    createSetupElement(GenericPlayerContext &context, IGstGenericPlayerPrivate &player, GstElement *element) const = 0;

    /**
     * @brief Creates a SetupSource task.
     *
     * @param[in] context   : The GstGenericPlayer context
     * @param[in] player    : The GstGenericPlayer instance
     * @param[in] source    : The source to be setup.
     *
     * @retval the new SetupSource task instance.
     */
    virtual std::unique_ptr<IPlayerTask>
    createSetupSource(GenericPlayerContext &context, IGstGenericPlayerPrivate &player, GstElement *source) const = 0;

    /**
     * @brief Creates a SetVideoGeometry task.
     *
     * @param[in] context      : The GstGenericPlayer context
     * @param[in] player       : The GstGenericPlayer instance
     * @param[in] rectangle    : The video geometry data.
     *
     * @retval the new SetVideoGeometry task instance.
     */
    virtual std::unique_ptr<IPlayerTask> createSetVideoGeometry(GenericPlayerContext &context,
                                                                IGstGenericPlayerPrivate &player,
                                                                const Rectangle &rectangle) const = 0;

    /**
     * @brief Creates a SetVolume task.
     *
     * @param[in] context       : The GstGenericPlayer context
     * @param[in] volume        : The volume to be set
     *
     * @retval the new SetVolume task instance.
     */
    virtual std::unique_ptr<IPlayerTask> createSetVolume(GenericPlayerContext &context, IGstGenericPlayerPrivate &player,
                                                         double targetVolume, uint32_t volumeDuration,
                                                         firebolt::rialto::EaseType easeType) const = 0;

    /**
     * @brief Creates a SetMute task.
     *
     * @param[in] context         : The GstGenericPlayer context
     * @param[in] player          : The GstGenericPlayer instance
     * @param[in] mediaSourceType : The media source type to set mute
     * @param[in] mute            : The mute state to be set
     *
     * @retval the new SetMute task instance.
     */
    virtual std::unique_ptr<IPlayerTask> createSetMute(GenericPlayerContext &context, IGstGenericPlayerPrivate &player,
                                                       const MediaSourceType &mediaSourceType, bool mute) const = 0;

    /**
     * @brief Creates a SetTextTrackIdentifier task.
     *
     * @param[in] context             : The GstGenericPlayer context
     * @param[in] textTrackIdentifier : The text track identifier to be set
     *
     * @retval the new SetTextTrackIdentifier task instance.
     */
    virtual std::unique_ptr<IPlayerTask> createSetTextTrackIdentifier(GenericPlayerContext &context,
                                                                      const std::string &textTrackIdentifier) const = 0;

    /**
     * @brief Creates a SetLowLatency task.
     *
     * @param[in] context       : The GstGenericPlayer context
     * @param[in] player        : The GstGenericPlayer instance
     * @param[in] lowLatency    : The low latency value to set
     *
     * @retval the new SetLowLatency task instance.
     */
    virtual std::unique_ptr<IPlayerTask> createSetLowLatency(GenericPlayerContext &context,
                                                             IGstGenericPlayerPrivate &player, bool lowLatency) const = 0;

    /**
     * @brief Creates a SetSync task.
     *
     * @param[in] context       : The GstGenericPlayer context
     * @param[in] player        : The GstGenericPlayer instance
     * @param[in] sync          : The sync value to set
     *
     * @retval the new SetSync task instance.
     */
    virtual std::unique_ptr<IPlayerTask> createSetSync(GenericPlayerContext &context, IGstGenericPlayerPrivate &player,
                                                       bool sync) const = 0;

    /**
     * @brief Creates a SetSyncOff task.
     *
     * @param[in] context       : The GstGenericPlayer context
     * @param[in] player        : The GstGenericPlayer instance
     * @param[in] syncOff       : The syncOff value to set
     *
     * @retval the new SetSyncOff task instance.
     */
    virtual std::unique_ptr<IPlayerTask> createSetSyncOff(GenericPlayerContext &context,
                                                          IGstGenericPlayerPrivate &player, bool syncOff) const = 0;

    /**
     * @brief Creates a SetStreamSyncMode task.
     *
     * @param[in] context           : The GstGenericPlayer context
     * @param[in] player            : The GstGenericPlayer instance
     * @param[in] type              : The media source type to set stream sync mode
     * @param[in] streamSyncMode    : The streamSyncMode value to set
     *
     * @retval the new SetStreamSyncMode task instance.
     */
    virtual std::unique_ptr<IPlayerTask> createSetStreamSyncMode(GenericPlayerContext &context,
                                                                 IGstGenericPlayerPrivate &player,
                                                                 const firebolt::rialto::MediaSourceType &type,
                                                                 int32_t streamSyncMode) const = 0;

    /**
     * @brief Creates a Shutdown task.
     *
     * @param[in] context       : The GstGenericPlayer context
     *
     * @retval the new Shutdown task instance.
     */
    virtual std::unique_ptr<IPlayerTask> createShutdown(IGstGenericPlayerPrivate &player) const = 0;

    /**
     * @brief Creates a Stop task.
     *
     * @param[in] context    : The GstGenericPlayer context
     * @param[in] player     : The GstGenericPlayer instance
     *
     * @retval the new Stop  task instance.
     */
    virtual std::unique_ptr<IPlayerTask> createStop(GenericPlayerContext &context,
                                                    IGstGenericPlayerPrivate &player) const = 0;

    /**
     * @brief Creates an Underflow task.
     *
     * @param[in] context          : The GstGenericPlayer context
     * @param[in] player           : The GstPlayer instance
     * @param[in] underflowEnabled : The underflow enabled flag (audio or video).
     *
     * @retval the new Underflow task instance.
     */
    virtual std::unique_ptr<IPlayerTask> createUnderflow(GenericPlayerContext &context, IGstGenericPlayerPrivate &player,
                                                         bool underflowEnabled, MediaSourceType sourceType) const = 0;

    /**
     * @brief Creates an UpdatePlaybackGroup task.
     *
     * @param[in] context       : The GstGenericPlayer context
     * @param[in] player        : The GstGenericPlayer instance
     * @param[in] typefind      : The typefind element.
     * @param[in] caps          : The GstCaps of added element
     *
     * @retval the new UpdatePlaybackGroup task instance.
     */
    virtual std::unique_ptr<IPlayerTask> createUpdatePlaybackGroup(GenericPlayerContext &context,
                                                                   IGstGenericPlayerPrivate &player,
                                                                   GstElement *typefind, const GstCaps *caps) const = 0;

    /**
     * @brief Creates a RenderFrame task.
     *
     * @param[in] context       : The GstGenericPlayer context
     * @param[in] player        : The GstPlayer instance
     *
     * @retval the new RenderFrame task instance.
     */
    virtual std::unique_ptr<IPlayerTask> createRenderFrame(GenericPlayerContext &context,
                                                           IGstGenericPlayerPrivate &player) const = 0;

    /**
     * @brief Creates a Ping task.
     *
     * @param[in] heartbeatHandler       : The HeartbeatHandler instance
     *
     * @retval the new Ping task instance.
     */
    virtual std::unique_ptr<IPlayerTask> createPing(std::unique_ptr<IHeartbeatHandler> &&heartbeatHandler) const = 0;

    /**
     * @brief Creates a Flush task.
     *
     * @param[in] context   : The GstPlayer context
     * @param[in] type      : The media source type to flush
     * @param[in] resetTime : True if time should be reset
     *
     * @retval the new Flush task instance.
     */
    virtual std::unique_ptr<IPlayerTask> createFlush(GenericPlayerContext &context, IGstGenericPlayerPrivate &player,
                                                     const firebolt::rialto::MediaSourceType &type,
                                                     bool resetTime) const = 0;

    /**
     * @brief Creates a SetSourcePosition task.
     *
     * @param[in] context   : The GstPlayer context
     * @param[in] player     : The GstGenericPlayer instance
     * @param[in] type      : The media source type to set position
     * @param[in] position  : The new source position
     * @param[in] resetTime : True if time should be reset
     * @param[in] appliedRate : The applied rate after seek
     * @param[in] stopPosition : The position of last pushed buffer
     *
     * @retval the new SetSourcePosition task instance.
     */
    virtual std::unique_ptr<IPlayerTask> createSetSourcePosition(GenericPlayerContext &context,
                                                                 IGstGenericPlayerPrivate &player,
                                                                 const firebolt::rialto::MediaSourceType &type,
                                                                 std::int64_t position, bool resetTime,
                                                                 double appliedRate, uint64_t stopPosition) const = 0;

    /**
     * @brief Creates a SetSubtitleOffset task.
     *
     * @param[in] context  : The GstPlayer context
     * @param[in] position : The subtitle offset position in nanoseconds
     *
     * @retval the new SetSubtitleOffset task instance.
     */
    virtual std::unique_ptr<IPlayerTask> createSetSubtitleOffset(GenericPlayerContext &context,
                                                                 std::int64_t position) const = 0;

    /**
     * @brief Creates a ProcessAudioGap task.
     *
     * @param[in] context          : The GstPlayer context
     * @param[in] position         : Audio pts fade position
     * @param[in] duration         : Audio pts fade duration
     * @param[in] discontinuityGap : Audio discontinuity gap
     * @param[in] audioAac         : True if audio codec is AAC
     *
     * @retval the new ProcessAudioGap task instance.
     */
    virtual std::unique_ptr<IPlayerTask> createProcessAudioGap(GenericPlayerContext &context, std::int64_t position,
                                                               std::uint32_t duration, std::int64_t discontinuityGap,
                                                               bool audioAac) const = 0;

    /**
     * @brief Creates a SetImmediateOutput task.
     *
     * @param[in] context         : The GstPlayer context
     * @param[in] player          : The GstPlayer instance
     * @param[in] type            : The media source type
     * @param[in] immediateOutput : the value to set for immediate-output
     *
     * @retval the new ProcessAudioGap task instance.
     */
    virtual std::unique_ptr<IPlayerTask> createSetImmediateOutput(GenericPlayerContext &context,
                                                                  IGstGenericPlayerPrivate &player,
                                                                  const firebolt::rialto::MediaSourceType &type,
                                                                  bool immediateOutput) const = 0;

    /**
     * @brief Creates a SetBufferingLimit task.
     *
     * @param[in] context         : The GstPlayer context
     * @param[in] player          : The GstPlayer instance
     * @param[in] limit           : the value to set for buffering limit
     *
     * @retval the new ProcessAudioGap task instance.
     */
    virtual std::unique_ptr<IPlayerTask> createSetBufferingLimit(GenericPlayerContext &context,
                                                                 IGstGenericPlayerPrivate &player,
                                                                 std::uint32_t limit) const = 0;

    /**
     * @brief Creates a SetUseBuffering task.
     *
     * @param[in] context         : The GstPlayer context
     * @param[in] player          : The GstPlayer instance
     * @param[in] useBuffering    : the value to set for use buffering
     *
     * @retval the new ProcessAudioGap task instance.
     */
    virtual std::unique_ptr<IPlayerTask>
    createSetUseBuffering(GenericPlayerContext &context, IGstGenericPlayerPrivate &player, bool useBuffering) const = 0;

    /**
     * @brief Creates a SwitchSource task.
     *
     * @param[in] player    : The GstGenericPlayer instance
     * @param[in] source    : The source to switch.
     *
     * @retval the new SwitchSource task instance.
     */
    virtual std::unique_ptr<IPlayerTask>
    createSwitchSource(IGstGenericPlayerPrivate &player,
                       const std::unique_ptr<IMediaPipeline::MediaSource> &source) const = 0;

    /**
     * @brief Creates a SynchroniseSubtitleClock task.
     *
     * @param[in] context   : The GstGenericPlayer context
     * @param[in] player    : The GstGenericPlayer instance
     *
     * @retval the new SynchroniseSubtitleClock task instance.
     */
    virtual std::unique_ptr<IPlayerTask> createSynchroniseSubtitleClock(GenericPlayerContext &context,
                                                                        IGstGenericPlayerPrivate &player) const = 0;
};

} // namespace firebolt::rialto::server

#endif // FIREBOLT_RIALTO_SERVER_I_GENERIC_PLAYER_TASK_FACTORY_H_
