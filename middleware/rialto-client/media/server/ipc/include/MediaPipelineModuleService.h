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

#ifndef FIREBOLT_RIALTO_SERVER_IPC_MEDIA_PIPELINE_MODULE_SERVICE_H_
#define FIREBOLT_RIALTO_SERVER_IPC_MEDIA_PIPELINE_MODULE_SERVICE_H_

#include "IMediaPipelineClient.h"
#include "IMediaPipelineModuleService.h"
#include "IMediaPipelineService.h"
#include <map>
#include <memory>
#include <set>

namespace firebolt::rialto::server::ipc
{
class MediaPipelineModuleServiceFactory : public IMediaPipelineModuleServiceFactory
{
public:
    MediaPipelineModuleServiceFactory() = default;
    virtual ~MediaPipelineModuleServiceFactory() = default;

    std::shared_ptr<IMediaPipelineModuleService> create(service::IMediaPipelineService &mediaPipelineService) const override;
};

class MediaPipelineModuleService : public IMediaPipelineModuleService
{
public:
    explicit MediaPipelineModuleService(service::IMediaPipelineService &mediaPipelineService);
    ~MediaPipelineModuleService() override;

    void clientConnected(const std::shared_ptr<::firebolt::rialto::ipc::IClient> &ipcClient) override;
    void clientDisconnected(const std::shared_ptr<::firebolt::rialto::ipc::IClient> &ipcClient) override;

    void createSession(::google::protobuf::RpcController *controller,
                       const ::firebolt::rialto::CreateSessionRequest *request,
                       ::firebolt::rialto::CreateSessionResponse *response, ::google::protobuf::Closure *done) override;
    void destroySession(::google::protobuf::RpcController *controller,
                        const ::firebolt::rialto::DestroySessionRequest *request,
                        ::firebolt::rialto::DestroySessionResponse *response, ::google::protobuf::Closure *done) override;
    void load(::google::protobuf::RpcController *controller, const ::firebolt::rialto::LoadRequest *request,
              ::firebolt::rialto::LoadResponse *response, ::google::protobuf::Closure *done) override;
    void setVideoWindow(::google::protobuf::RpcController *controller,
                        const ::firebolt::rialto::SetVideoWindowRequest *request,
                        ::firebolt::rialto::SetVideoWindowResponse *response, ::google::protobuf::Closure *done) override;
    void attachSource(::google::protobuf::RpcController *controller,
                      const ::firebolt::rialto::AttachSourceRequest *request,
                      ::firebolt::rialto::AttachSourceResponse *response, ::google::protobuf::Closure *done) override;
    void removeSource(::google::protobuf::RpcController *controller,
                      const ::firebolt::rialto::RemoveSourceRequest *request,
                      ::firebolt::rialto::RemoveSourceResponse *response, ::google::protobuf::Closure *done) override;
    void allSourcesAttached(::google::protobuf::RpcController *controller,
                            const ::firebolt::rialto::AllSourcesAttachedRequest *request,
                            ::firebolt::rialto::AllSourcesAttachedResponse *response,
                            ::google::protobuf::Closure *done) override;
    void play(::google::protobuf::RpcController *controller, const ::firebolt::rialto::PlayRequest *request,
              ::firebolt::rialto::PlayResponse *response, ::google::protobuf::Closure *done) override;
    void pause(::google::protobuf::RpcController *controller, const ::firebolt::rialto::PauseRequest *request,
               ::firebolt::rialto::PauseResponse *response, ::google::protobuf::Closure *done) override;
    void stop(::google::protobuf::RpcController *controller, const ::firebolt::rialto::StopRequest *request,
              ::firebolt::rialto::StopResponse *response, ::google::protobuf::Closure *done) override;
    void setPosition(::google::protobuf::RpcController *controller, const ::firebolt::rialto::SetPositionRequest *request,
                     ::firebolt::rialto::SetPositionResponse *response, ::google::protobuf::Closure *done) override;
    void haveData(::google::protobuf::RpcController *controller, const ::firebolt::rialto::HaveDataRequest *request,
                  ::firebolt::rialto::HaveDataResponse *response, ::google::protobuf::Closure *done) override;
    void setPlaybackRate(::google::protobuf::RpcController *controller,
                         const ::firebolt::rialto::SetPlaybackRateRequest *request,
                         ::firebolt::rialto::SetPlaybackRateResponse *response,
                         ::google::protobuf::Closure *done) override;
    void getPosition(::google::protobuf::RpcController *controller, const ::firebolt::rialto::GetPositionRequest *request,
                     ::firebolt::rialto::GetPositionResponse *response, ::google::protobuf::Closure *done) override;
    void setImmediateOutput(::google::protobuf::RpcController *controller,
                            const ::firebolt::rialto::SetImmediateOutputRequest *request,
                            ::firebolt::rialto::SetImmediateOutputResponse *response,
                            ::google::protobuf::Closure *done) override;
    void getImmediateOutput(::google::protobuf::RpcController *controller,
                            const ::firebolt::rialto::GetImmediateOutputRequest *request,
                            ::firebolt::rialto::GetImmediateOutputResponse *response,
                            ::google::protobuf::Closure *done) override;
    void getStats(::google::protobuf::RpcController *controller, const ::firebolt::rialto::GetStatsRequest *request,
                  ::firebolt::rialto::GetStatsResponse *response, ::google::protobuf::Closure *done) override;
    void renderFrame(::google::protobuf::RpcController *controller, const ::firebolt::rialto::RenderFrameRequest *request,
                     ::firebolt::rialto::RenderFrameResponse *response, ::google::protobuf::Closure *done) override;
    void setVolume(::google::protobuf::RpcController *controller, const ::firebolt::rialto::SetVolumeRequest *request,
                   ::firebolt::rialto::SetVolumeResponse *response, ::google::protobuf::Closure *done) override;
    void getVolume(::google::protobuf::RpcController *controller, const ::firebolt::rialto::GetVolumeRequest *request,
                   ::firebolt::rialto::GetVolumeResponse *response, ::google::protobuf::Closure *done) override;
    void setMute(::google::protobuf::RpcController *controller, const ::firebolt::rialto::SetMuteRequest *request,
                 ::firebolt::rialto::SetMuteResponse *response, ::google::protobuf::Closure *done) override;
    void getMute(::google::protobuf::RpcController *controller, const ::firebolt::rialto::GetMuteRequest *request,
                 ::firebolt::rialto::GetMuteResponse *response, ::google::protobuf::Closure *done) override;
    void setTextTrackIdentifier(::google::protobuf::RpcController *controller,
                                const ::firebolt::rialto::SetTextTrackIdentifierRequest *request,
                                ::firebolt::rialto::SetTextTrackIdentifierResponse *response,
                                ::google::protobuf::Closure *done) override;
    void getTextTrackIdentifier(::google::protobuf::RpcController *controller,
                                const ::firebolt::rialto::GetTextTrackIdentifierRequest *request,
                                ::firebolt::rialto::GetTextTrackIdentifierResponse *response,
                                ::google::protobuf::Closure *done) override;
    void setLowLatency(::google::protobuf::RpcController *controller,
                       const ::firebolt::rialto::SetLowLatencyRequest *request,
                       ::firebolt::rialto::SetLowLatencyResponse *response, ::google::protobuf::Closure *done) override;
    void setSync(::google::protobuf::RpcController *controller, const ::firebolt::rialto::SetSyncRequest *request,
                 ::firebolt::rialto::SetSyncResponse *response, ::google::protobuf::Closure *done) override;
    void getSync(::google::protobuf::RpcController *controller, const ::firebolt::rialto::GetSyncRequest *request,
                 ::firebolt::rialto::GetSyncResponse *response, ::google::protobuf::Closure *done) override;
    void setSyncOff(::google::protobuf::RpcController *controller, const ::firebolt::rialto::SetSyncOffRequest *request,
                    ::firebolt::rialto::SetSyncOffResponse *response, ::google::protobuf::Closure *done) override;
    void setStreamSyncMode(::google::protobuf::RpcController *controller,
                           const ::firebolt::rialto::SetStreamSyncModeRequest *request,
                           ::firebolt::rialto::SetStreamSyncModeResponse *response,
                           ::google::protobuf::Closure *done) override;
    void getStreamSyncMode(::google::protobuf::RpcController *controller,
                           const ::firebolt::rialto::GetStreamSyncModeRequest *request,
                           ::firebolt::rialto::GetStreamSyncModeResponse *response,
                           ::google::protobuf::Closure *done) override;
    void flush(::google::protobuf::RpcController *controller, const ::firebolt::rialto::FlushRequest *request,
               ::firebolt::rialto::FlushResponse *response, ::google::protobuf::Closure *done) override;
    void setSourcePosition(::google::protobuf::RpcController *controller,
                           const ::firebolt::rialto::SetSourcePositionRequest *request,
                           ::firebolt::rialto::SetSourcePositionResponse *response,
                           ::google::protobuf::Closure *done) override;
    void setSubtitleOffset(::google::protobuf::RpcController *controller,
                           const ::firebolt::rialto::SetSubtitleOffsetRequest *request,
                           ::firebolt::rialto::SetSubtitleOffsetResponse *response,
                           ::google::protobuf::Closure *done) override;
    void processAudioGap(::google::protobuf::RpcController *controller,
                         const ::firebolt::rialto::ProcessAudioGapRequest *request,
                         ::firebolt::rialto::ProcessAudioGapResponse *response,
                         ::google::protobuf::Closure *done) override;
    void setBufferingLimit(::google::protobuf::RpcController *controller,
                           const ::firebolt::rialto::SetBufferingLimitRequest *request,
                           ::firebolt::rialto::SetBufferingLimitResponse *response,
                           ::google::protobuf::Closure *done) override;
    void getBufferingLimit(::google::protobuf::RpcController *controller,
                           const ::firebolt::rialto::GetBufferingLimitRequest *request,
                           ::firebolt::rialto::GetBufferingLimitResponse *response,
                           ::google::protobuf::Closure *done) override;
    void setUseBuffering(::google::protobuf::RpcController *controller,
                         const ::firebolt::rialto::SetUseBufferingRequest *request,
                         ::firebolt::rialto::SetUseBufferingResponse *response,
                         ::google::protobuf::Closure *done) override;
    void getUseBuffering(::google::protobuf::RpcController *controller,
                         const ::firebolt::rialto::GetUseBufferingRequest *request,
                         ::firebolt::rialto::GetUseBufferingResponse *response,
                         ::google::protobuf::Closure *done) override;

private:
    service::IMediaPipelineService &m_mediaPipelineService;
    std::map<std::shared_ptr<::firebolt::rialto::ipc::IClient>, std::set<int>> m_clientSessions;
};
} // namespace firebolt::rialto::server::ipc

#endif // FIREBOLT_RIALTO_SERVER_IPC_MEDIA_PIPELINE_MODULE_SERVICE_H_
