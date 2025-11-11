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
#include "tasks/generic/GenericPlayerTaskFactory.h"
#include "IMediaPipeline.h"
#include "tasks/generic/AttachSamples.h"
#include "tasks/generic/AttachSource.h"
#include "tasks/generic/CheckAudioUnderflow.h"
#include "tasks/generic/DeepElementAdded.h"
#include "tasks/generic/EnoughData.h"
#include "tasks/generic/Eos.h"
#include "tasks/generic/FinishSetupSource.h"
#include "tasks/generic/Flush.h"
#include "tasks/generic/HandleBusMessage.h"
#include "tasks/generic/NeedData.h"
#include "tasks/generic/Pause.h"
#include "tasks/generic/Ping.h"
#include "tasks/generic/Play.h"
#include "tasks/generic/ProcessAudioGap.h"
#include "tasks/generic/ReadShmDataAndAttachSamples.h"
#include "tasks/generic/RemoveSource.h"
#include "tasks/generic/RenderFrame.h"
#include "tasks/generic/ReportPosition.h"
#include "tasks/generic/SetBufferingLimit.h"
#include "tasks/generic/SetImmediateOutput.h"
#include "tasks/generic/SetLowLatency.h"
#include "tasks/generic/SetMute.h"
#include "tasks/generic/SetPlaybackRate.h"
#include "tasks/generic/SetPosition.h"
#include "tasks/generic/SetSourcePosition.h"
#include "tasks/generic/SetStreamSyncMode.h"
#include "tasks/generic/SetSubtitleOffset.h"
#include "tasks/generic/SetSync.h"
#include "tasks/generic/SetSyncOff.h"
#include "tasks/generic/SetTextTrackIdentifier.h"
#include "tasks/generic/SetUseBuffering.h"
#include "tasks/generic/SetVideoGeometry.h"
#include "tasks/generic/SetVolume.h"
#include "tasks/generic/SetupElement.h"
#include "tasks/generic/SetupSource.h"
#include "tasks/generic/Shutdown.h"
#include "tasks/generic/Stop.h"
#include "tasks/generic/SwitchSource.h"
#include "tasks/generic/SynchroniseSubtitleClock.h"
#include "tasks/generic/Underflow.h"
#include "tasks/generic/UpdatePlaybackGroup.h"

namespace firebolt::rialto::server
{
GenericPlayerTaskFactory::GenericPlayerTaskFactory(
    IGstGenericPlayerClient *client, const std::shared_ptr<firebolt::rialto::wrappers::IGstWrapper> &gstWrapper,
    const std::shared_ptr<firebolt::rialto::wrappers::IGlibWrapper> &glibWrapper,
    const std::shared_ptr<firebolt::rialto::wrappers::IRdkGstreamerUtilsWrapper> &rdkGstreamerUtilsWrapper,
    const std::shared_ptr<IGstTextTrackSinkFactory> &gstTextTrackSinkFactory)
    : m_client{client}, m_gstWrapper{gstWrapper}, m_glibWrapper{glibWrapper},
      m_rdkGstreamerUtilsWrapper{rdkGstreamerUtilsWrapper}, m_gstTextTrackSinkFactory{gstTextTrackSinkFactory}
{
}

std::unique_ptr<IPlayerTask>
GenericPlayerTaskFactory::createAttachSamples(GenericPlayerContext &context, IGstGenericPlayerPrivate &player,
                                              const IMediaPipeline::MediaSegmentVector &mediaSegments) const
{
    return std::make_unique<tasks::generic::AttachSamples>(context, m_gstWrapper, player, mediaSegments);
}

std::unique_ptr<IPlayerTask>
GenericPlayerTaskFactory::createAttachSource(GenericPlayerContext &context, IGstGenericPlayerPrivate &player,
                                             const std::unique_ptr<IMediaPipeline::MediaSource> &source) const
{
    return std::make_unique<tasks::generic::AttachSource>(context, m_gstWrapper, m_glibWrapper,
                                                          m_gstTextTrackSinkFactory, player, source);
}

std::unique_ptr<IPlayerTask> GenericPlayerTaskFactory::createDeepElementAdded(GenericPlayerContext &context,
                                                                              IGstGenericPlayerPrivate &player,
                                                                              GstBin *pipeline, GstBin *bin,
                                                                              GstElement *element) const
{
    return std::make_unique<tasks::generic::DeepElementAdded>(context, player, m_gstWrapper, m_glibWrapper, pipeline,
                                                              bin, element);
}

std::unique_ptr<IPlayerTask> GenericPlayerTaskFactory::createEnoughData(GenericPlayerContext &context, GstAppSrc *src) const
{
    return std::make_unique<tasks::generic::EnoughData>(context, src);
}

std::unique_ptr<IPlayerTask> GenericPlayerTaskFactory::createEos(GenericPlayerContext &context,
                                                                 IGstGenericPlayerPrivate &player,
                                                                 const firebolt::rialto::MediaSourceType &type) const
{
    return std::make_unique<tasks::generic::Eos>(context, player, m_gstWrapper, type);
}

std::unique_ptr<IPlayerTask> GenericPlayerTaskFactory::createFinishSetupSource(GenericPlayerContext &context,
                                                                               IGstGenericPlayerPrivate &player) const
{
    return std::make_unique<tasks::generic::FinishSetupSource>(context, player, m_client);
}

std::unique_ptr<IPlayerTask> GenericPlayerTaskFactory::createHandleBusMessage(GenericPlayerContext &context,
                                                                              IGstGenericPlayerPrivate &player,
                                                                              GstMessage *message,
                                                                              const IFlushWatcher &flushWatcher) const
{
    return std::make_unique<tasks::generic::HandleBusMessage>(context, player, m_client, m_gstWrapper, m_glibWrapper,
                                                              message, flushWatcher);
}

std::unique_ptr<IPlayerTask> GenericPlayerTaskFactory::createNeedData(GenericPlayerContext &context,
                                                                      IGstGenericPlayerPrivate &player,
                                                                      GstAppSrc *src) const
{
    return std::make_unique<tasks::generic::NeedData>(context, player, m_client, src);
}

std::unique_ptr<IPlayerTask> GenericPlayerTaskFactory::createPause(GenericPlayerContext &context,
                                                                   IGstGenericPlayerPrivate &player) const
{
    return std::make_unique<tasks::generic::Pause>(context, player);
}

std::unique_ptr<IPlayerTask> GenericPlayerTaskFactory::createPlay(IGstGenericPlayerPrivate &player) const
{
    return std::make_unique<tasks::generic::Play>(player);
}

std::unique_ptr<IPlayerTask> GenericPlayerTaskFactory::createReadShmDataAndAttachSamples(
    GenericPlayerContext &context, IGstGenericPlayerPrivate &player, const std::shared_ptr<IDataReader> &dataReader) const
{
    return std::make_unique<tasks::generic::ReadShmDataAndAttachSamples>(context, m_gstWrapper, player, dataReader);
}

std::unique_ptr<IPlayerTask>
GenericPlayerTaskFactory::createRemoveSource(GenericPlayerContext &context, IGstGenericPlayerPrivate &player,
                                             const firebolt::rialto::MediaSourceType &type) const
{
    return std::make_unique<tasks::generic::RemoveSource>(context, player, m_client, m_gstWrapper, type);
}

std::unique_ptr<IPlayerTask> GenericPlayerTaskFactory::createReportPosition(GenericPlayerContext &context,
                                                                            IGstGenericPlayerPrivate &player) const
{
    return std::make_unique<tasks::generic::ReportPosition>(context, m_client, m_gstWrapper, player);
}

std::unique_ptr<IPlayerTask> GenericPlayerTaskFactory::createCheckAudioUnderflow(GenericPlayerContext &context,
                                                                                 IGstGenericPlayerPrivate &player) const
{
    return std::make_unique<tasks::generic::CheckAudioUnderflow>(context, player, m_client, m_gstWrapper);
}

std::unique_ptr<IPlayerTask> GenericPlayerTaskFactory::createSetPlaybackRate(GenericPlayerContext &context,
                                                                             double rate) const
{
    return std::make_unique<tasks::generic::SetPlaybackRate>(context, m_gstWrapper, m_glibWrapper, rate);
}

std::unique_ptr<IPlayerTask> GenericPlayerTaskFactory::createSetPosition(GenericPlayerContext &context,
                                                                         IGstGenericPlayerPrivate &player,
                                                                         std::int64_t position) const
{
    return std::make_unique<tasks::generic::SetPosition>(context, player, m_client, m_gstWrapper, position);
}

std::unique_ptr<IPlayerTask> GenericPlayerTaskFactory::createSetupElement(GenericPlayerContext &context,
                                                                          IGstGenericPlayerPrivate &player,
                                                                          GstElement *element) const
{
    return std::make_unique<tasks::generic::SetupElement>(context, m_gstWrapper, m_glibWrapper, player, element);
}

std::unique_ptr<IPlayerTask> GenericPlayerTaskFactory::createSetupSource(GenericPlayerContext &context,
                                                                         IGstGenericPlayerPrivate &player,
                                                                         GstElement *source) const
{
    return std::make_unique<tasks::generic::SetupSource>(context, player, source);
}

std::unique_ptr<IPlayerTask> GenericPlayerTaskFactory::createSetVideoGeometry(GenericPlayerContext &context,
                                                                              IGstGenericPlayerPrivate &player,
                                                                              const Rectangle &rectangle) const
{
    return std::make_unique<tasks::generic::SetVideoGeometry>(context, player, rectangle);
}

std::unique_ptr<IPlayerTask> GenericPlayerTaskFactory::createSetVolume(GenericPlayerContext &context,
                                                                       IGstGenericPlayerPrivate &player,
                                                                       double targetVolume, uint32_t volumeDuration,
                                                                       firebolt::rialto::EaseType easeType) const
{
    return std::make_unique<tasks::generic::SetVolume>(context, player, m_gstWrapper, m_glibWrapper,
                                                       m_rdkGstreamerUtilsWrapper, targetVolume, volumeDuration,
                                                       easeType);
}

std::unique_ptr<IPlayerTask> GenericPlayerTaskFactory::createSetMute(GenericPlayerContext &context,
                                                                     IGstGenericPlayerPrivate &player,
                                                                     const MediaSourceType &mediaSourceType,
                                                                     bool mute) const
{
    return std::make_unique<tasks::generic::SetMute>(context, player, m_gstWrapper, m_glibWrapper, mediaSourceType, mute);
}

std::unique_ptr<IPlayerTask>
GenericPlayerTaskFactory::createSetTextTrackIdentifier(GenericPlayerContext &context,
                                                       const std::string &textTrackIdentifier) const
{
    return std::make_unique<tasks::generic::SetTextTrackIdentifier>(context, m_glibWrapper, textTrackIdentifier);
}

std::unique_ptr<IPlayerTask> GenericPlayerTaskFactory::createSetLowLatency(GenericPlayerContext &context,
                                                                           IGstGenericPlayerPrivate &player,
                                                                           bool lowLatency) const
{
    return std::make_unique<tasks::generic::SetLowLatency>(context, player, lowLatency);
}

std::unique_ptr<IPlayerTask> GenericPlayerTaskFactory::createSetSync(GenericPlayerContext &context,
                                                                     IGstGenericPlayerPrivate &player, bool sync) const
{
    return std::make_unique<tasks::generic::SetSync>(context, player, sync);
}

std::unique_ptr<IPlayerTask> GenericPlayerTaskFactory::createSetSyncOff(GenericPlayerContext &context,
                                                                        IGstGenericPlayerPrivate &player,
                                                                        bool syncOff) const
{
    return std::make_unique<tasks::generic::SetSyncOff>(context, player, syncOff);
}

std::unique_ptr<IPlayerTask>
GenericPlayerTaskFactory::createSetStreamSyncMode(GenericPlayerContext &context, IGstGenericPlayerPrivate &player,
                                                  const firebolt::rialto::MediaSourceType &type,
                                                  int32_t streamSyncMode) const
{
    return std::make_unique<tasks::generic::SetStreamSyncMode>(context, player, type, streamSyncMode);
}

std::unique_ptr<IPlayerTask> GenericPlayerTaskFactory::createShutdown(IGstGenericPlayerPrivate &player) const
{
    return std::make_unique<tasks::generic::Shutdown>(player);
}

std::unique_ptr<IPlayerTask> GenericPlayerTaskFactory::createStop(GenericPlayerContext &context,
                                                                  IGstGenericPlayerPrivate &player) const
{
    return std::make_unique<tasks::generic::Stop>(context, player);
}

std::unique_ptr<IPlayerTask> GenericPlayerTaskFactory::createUnderflow(GenericPlayerContext &context,
                                                                       IGstGenericPlayerPrivate &player,
                                                                       bool underflowEnabled,
                                                                       MediaSourceType sourceType) const
{
    return std::make_unique<tasks::generic::Underflow>(context, player, m_client, underflowEnabled, sourceType);
}

std::unique_ptr<IPlayerTask> GenericPlayerTaskFactory::createUpdatePlaybackGroup(GenericPlayerContext &context,
                                                                                 IGstGenericPlayerPrivate &player,
                                                                                 GstElement *typefind,
                                                                                 const GstCaps *caps) const
{
    return std::make_unique<tasks::generic::UpdatePlaybackGroup>(context, player, m_gstWrapper, m_glibWrapper, typefind,
                                                                 caps);
}

std::unique_ptr<IPlayerTask> GenericPlayerTaskFactory::createRenderFrame(GenericPlayerContext &context,
                                                                         IGstGenericPlayerPrivate &player) const
{
    return std::make_unique<tasks::generic::RenderFrame>(context, player);
}

std::unique_ptr<IPlayerTask> GenericPlayerTaskFactory::createPing(std::unique_ptr<IHeartbeatHandler> &&heartbeatHandler) const
{
    return std::make_unique<tasks::generic::Ping>(std::move(heartbeatHandler));
}

std::unique_ptr<IPlayerTask> GenericPlayerTaskFactory::createFlush(GenericPlayerContext &context,
                                                                   IGstGenericPlayerPrivate &player,
                                                                   const firebolt::rialto::MediaSourceType &type,
                                                                   bool resetTime) const
{
    return std::make_unique<tasks::generic::Flush>(context, player, m_client, m_gstWrapper, type, resetTime);
}

std::unique_ptr<IPlayerTask>
GenericPlayerTaskFactory::createSetSourcePosition(GenericPlayerContext &context, IGstGenericPlayerPrivate &player,
                                                  const firebolt::rialto::MediaSourceType &type, std::int64_t position,
                                                  bool resetTime, double appliedRate, uint64_t stopPosition) const
{
    return std::make_unique<tasks::generic::SetSourcePosition>(context, player, m_client, m_glibWrapper, type, position,
                                                               resetTime, appliedRate, stopPosition);
}

std::unique_ptr<IPlayerTask> GenericPlayerTaskFactory::createSetSubtitleOffset(GenericPlayerContext &context,
                                                                               std::int64_t position) const
{
    return std::make_unique<tasks::generic::SetSubtitleOffset>(context, m_glibWrapper, position);
}

std::unique_ptr<IPlayerTask> GenericPlayerTaskFactory::createProcessAudioGap(GenericPlayerContext &context,
                                                                             std::int64_t position,
                                                                             std::uint32_t duration,
                                                                             std::int64_t discontinuityGap,
                                                                             bool audioAac) const
{
    return std::make_unique<tasks::generic::ProcessAudioGap>(context, m_gstWrapper, m_glibWrapper,
                                                             m_rdkGstreamerUtilsWrapper, position, duration,
                                                             discontinuityGap, audioAac);
}

std::unique_ptr<IPlayerTask>
GenericPlayerTaskFactory::createSetImmediateOutput(GenericPlayerContext &context, IGstGenericPlayerPrivate &player,
                                                   const firebolt::rialto::MediaSourceType &type,
                                                   bool immediateOutput) const
{
    return std::make_unique<tasks::generic::SetImmediateOutput>(context, player, type, immediateOutput);
}

std::unique_ptr<IPlayerTask> GenericPlayerTaskFactory::createSetBufferingLimit(GenericPlayerContext &context,
                                                                               IGstGenericPlayerPrivate &player,
                                                                               std::uint32_t limit) const
{
    return std::make_unique<tasks::generic::SetBufferingLimit>(context, player, limit);
}

std::unique_ptr<IPlayerTask> GenericPlayerTaskFactory::createSetUseBuffering(GenericPlayerContext &context,
                                                                             IGstGenericPlayerPrivate &player,
                                                                             bool useBuffering) const
{
    return std::make_unique<tasks::generic::SetUseBuffering>(context, player, useBuffering);
}

std::unique_ptr<IPlayerTask>
GenericPlayerTaskFactory::createSwitchSource(IGstGenericPlayerPrivate &player,
                                             const std::unique_ptr<IMediaPipeline::MediaSource> &source) const
{
    return std::make_unique<tasks::generic::SwitchSource>(player, source);
}

std::unique_ptr<IPlayerTask> GenericPlayerTaskFactory::createSynchroniseSubtitleClock(GenericPlayerContext &context,
                                                                                      IGstGenericPlayerPrivate &player) const
{
    return std::make_unique<tasks::generic::SynchroniseSubtitleClock>(context, player, m_gstWrapper, m_glibWrapper);
}
} // namespace firebolt::rialto::server
