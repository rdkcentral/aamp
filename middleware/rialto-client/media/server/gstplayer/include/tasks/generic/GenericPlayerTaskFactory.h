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

#ifndef FIREBOLT_RIALTO_SERVER_GENERIC_PLAYER_TASK_FACTORY_H_
#define FIREBOLT_RIALTO_SERVER_GENERIC_PLAYER_TASK_FACTORY_H_

#include "IGenericPlayerTaskFactory.h"
#include "IGlibWrapper.h"
#include "IGstGenericPlayerClient.h"
#include "IGstTextTrackSinkFactory.h"
#include "IGstWrapper.h"
#include "IMediaPipeline.h"
#include <memory>
#include <string>

namespace firebolt::rialto::server
{
class GenericPlayerTaskFactory : public IGenericPlayerTaskFactory
{
public:
    GenericPlayerTaskFactory(
        IGstGenericPlayerClient *client, const std::shared_ptr<firebolt::rialto::wrappers::IGstWrapper> &gstWrapper,
        const std::shared_ptr<firebolt::rialto::wrappers::IGlibWrapper> &glibWrapper,
        const std::shared_ptr<firebolt::rialto::wrappers::IRdkGstreamerUtilsWrapper> &rdkGstreamerUtilsWrapper,
        const std::shared_ptr<IGstTextTrackSinkFactory> &gstTextTrackSinkFactory);
    ~GenericPlayerTaskFactory() override = default;

    std::unique_ptr<IPlayerTask> createAttachSamples(GenericPlayerContext &context, IGstGenericPlayerPrivate &player,
                                                     const IMediaPipeline::MediaSegmentVector &mediaSegments) const override;
    std::unique_ptr<IPlayerTask>
    createAttachSource(GenericPlayerContext &context, IGstGenericPlayerPrivate &player,
                       const std::unique_ptr<IMediaPipeline::MediaSource> &source) const override;
    std::unique_ptr<IPlayerTask> createDeepElementAdded(GenericPlayerContext &context, IGstGenericPlayerPrivate &player,
                                                        GstBin *pipeline, GstBin *bin,
                                                        GstElement *element) const override;
    std::unique_ptr<IPlayerTask> createEnoughData(GenericPlayerContext &context, GstAppSrc *src) const override;
    std::unique_ptr<IPlayerTask> createEos(GenericPlayerContext &context, IGstGenericPlayerPrivate &player,
                                           const firebolt::rialto::MediaSourceType &type) const override;
    std::unique_ptr<IPlayerTask> createFinishSetupSource(GenericPlayerContext &context,
                                                         IGstGenericPlayerPrivate &player) const override;
    std::unique_ptr<IPlayerTask> createHandleBusMessage(GenericPlayerContext &context, IGstGenericPlayerPrivate &player,
                                                        GstMessage *message,
                                                        const IFlushWatcher &flushWatcher) const override;
    std::unique_ptr<IPlayerTask> createNeedData(GenericPlayerContext &context, IGstGenericPlayerPrivate &player,
                                                GstAppSrc *src) const override;
    std::unique_ptr<IPlayerTask> createPause(GenericPlayerContext &context,
                                             IGstGenericPlayerPrivate &player) const override;
    std::unique_ptr<IPlayerTask> createPlay(IGstGenericPlayerPrivate &player) const override;
    std::unique_ptr<IPlayerTask>
    createReadShmDataAndAttachSamples(GenericPlayerContext &context, IGstGenericPlayerPrivate &player,
                                      const std::shared_ptr<IDataReader> &dataReader) const override;
    std::unique_ptr<IPlayerTask> createRemoveSource(GenericPlayerContext &context, IGstGenericPlayerPrivate &player,
                                                    const firebolt::rialto::MediaSourceType &type) const override;
    std::unique_ptr<IPlayerTask> createReportPosition(GenericPlayerContext &context,
                                                      IGstGenericPlayerPrivate &player) const override;
    std::unique_ptr<IPlayerTask> createCheckAudioUnderflow(GenericPlayerContext &context,
                                                           IGstGenericPlayerPrivate &player) const override;
    std::unique_ptr<IPlayerTask> createSetPlaybackRate(GenericPlayerContext &context, double rate) const override;
    std::unique_ptr<IPlayerTask> createSetPosition(GenericPlayerContext &context, IGstGenericPlayerPrivate &player,
                                                   std::int64_t position) const override;
    std::unique_ptr<IPlayerTask> createSetupElement(GenericPlayerContext &context, IGstGenericPlayerPrivate &player,
                                                    GstElement *element) const override;
    std::unique_ptr<IPlayerTask> createSetupSource(GenericPlayerContext &context, IGstGenericPlayerPrivate &player,
                                                   GstElement *source) const override;
    std::unique_ptr<IPlayerTask> createSetVideoGeometry(GenericPlayerContext &context, IGstGenericPlayerPrivate &player,
                                                        const Rectangle &rectangle) const override;
    std::unique_ptr<IPlayerTask> createSetVolume(GenericPlayerContext &context, IGstGenericPlayerPrivate &player,
                                                 double targetVolume, uint32_t volumeDuration,
                                                 firebolt::rialto::EaseType easeType) const override;
    std::unique_ptr<IPlayerTask> createSetMute(GenericPlayerContext &context, IGstGenericPlayerPrivate &player,
                                               const MediaSourceType &mediaSourceType, bool mute) const override;
    std::unique_ptr<IPlayerTask> createSetTextTrackIdentifier(GenericPlayerContext &context,
                                                              const std::string &textTrackIdentifier) const override;
    std::unique_ptr<IPlayerTask> createSetLowLatency(GenericPlayerContext &context, IGstGenericPlayerPrivate &player,
                                                     bool lowLatency) const override;
    std::unique_ptr<IPlayerTask> createSetSync(GenericPlayerContext &context, IGstGenericPlayerPrivate &player,
                                               bool sync) const override;
    std::unique_ptr<IPlayerTask> createSetSyncOff(GenericPlayerContext &context, IGstGenericPlayerPrivate &player,
                                                  bool syncOff) const override;
    std::unique_ptr<IPlayerTask> createSetStreamSyncMode(GenericPlayerContext &context, IGstGenericPlayerPrivate &player,
                                                         const firebolt::rialto::MediaSourceType &type,
                                                         int32_t streamSyncMode) const override;
    std::unique_ptr<IPlayerTask> createShutdown(IGstGenericPlayerPrivate &player) const override;
    std::unique_ptr<IPlayerTask> createStop(GenericPlayerContext &context,
                                            IGstGenericPlayerPrivate &player) const override;
    std::unique_ptr<IPlayerTask> createUnderflow(GenericPlayerContext &context, IGstGenericPlayerPrivate &player,
                                                 bool underflowEnable, MediaSourceType sourceType) const override;
    std::unique_ptr<IPlayerTask> createUpdatePlaybackGroup(GenericPlayerContext &context,
                                                           IGstGenericPlayerPrivate &player, GstElement *typefind,
                                                           const GstCaps *caps) const override;
    std::unique_ptr<IPlayerTask> createRenderFrame(GenericPlayerContext &context,
                                                   IGstGenericPlayerPrivate &player) const override;
    std::unique_ptr<IPlayerTask> createPing(std::unique_ptr<IHeartbeatHandler> &&heartbeatHandler) const override;
    std::unique_ptr<IPlayerTask> createFlush(GenericPlayerContext &context, IGstGenericPlayerPrivate &player,
                                             const firebolt::rialto::MediaSourceType &type,
                                             bool resetTime) const override;
    std::unique_ptr<IPlayerTask> createSetSourcePosition(GenericPlayerContext &context, IGstGenericPlayerPrivate &player,
                                                         const firebolt::rialto::MediaSourceType &type,
                                                         std::int64_t position, bool resetTime, double appliedRate,
                                                         uint64_t stopPosition) const override;
    std::unique_ptr<IPlayerTask> createSetSubtitleOffset(GenericPlayerContext &context,
                                                         std::int64_t position) const override;
    std::unique_ptr<IPlayerTask> createProcessAudioGap(GenericPlayerContext &context, std::int64_t position,
                                                       std::uint32_t duration, std::int64_t discontinuityGap,
                                                       bool audioAac) const override;
    std::unique_ptr<IPlayerTask> createSetImmediateOutput(GenericPlayerContext &context, IGstGenericPlayerPrivate &player,
                                                          const firebolt::rialto::MediaSourceType &type,
                                                          bool immediateOutput) const override;
    std::unique_ptr<IPlayerTask> createSetBufferingLimit(GenericPlayerContext &context, IGstGenericPlayerPrivate &player,
                                                         std::uint32_t limit) const override;
    std::unique_ptr<IPlayerTask> createSetUseBuffering(GenericPlayerContext &context, IGstGenericPlayerPrivate &player,
                                                       bool useBuffering) const override;
    std::unique_ptr<IPlayerTask>
    createSwitchSource(IGstGenericPlayerPrivate &player,
                       const std::unique_ptr<IMediaPipeline::MediaSource> &source) const override;

    std::unique_ptr<IPlayerTask> createSynchroniseSubtitleClock(GenericPlayerContext &context,
                                                                IGstGenericPlayerPrivate &player) const override;

private:
    IGstGenericPlayerClient *m_client;
    std::shared_ptr<firebolt::rialto::wrappers::IGstWrapper> m_gstWrapper;
    std::shared_ptr<firebolt::rialto::wrappers::IGlibWrapper> m_glibWrapper;
    std::shared_ptr<firebolt::rialto::wrappers::IRdkGstreamerUtilsWrapper> m_rdkGstreamerUtilsWrapper;
    std::shared_ptr<IGstTextTrackSinkFactory> m_gstTextTrackSinkFactory;
};
} // namespace firebolt::rialto::server

#endif // FIREBOLT_RIALTO_SERVER_GENERIC_PLAYER_TASK_FACTORY_H_
