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

#include "MediaPipelineModuleService.h"
#include "IMediaPipelineService.h"
#include "MediaPipelineClient.h"
#include "RialtoCommonModule.h"
#include "RialtoServerLogging.h"
#include <IIpcController.h>
#include <algorithm>
#include <cstdint>
#include <unordered_map>

namespace
{
int generateSessionId()
{
    static int sessionId{0};
    return sessionId++;
}

firebolt::rialto::MediaType convertMediaType(const firebolt::rialto::LoadRequest_MediaType &mediaType)
{
    switch (mediaType)
    {
    case firebolt::rialto::LoadRequest_MediaType::LoadRequest_MediaType_UNKNOWN:
    {
        return firebolt::rialto::MediaType::UNKNOWN;
    }
    case firebolt::rialto::LoadRequest_MediaType::LoadRequest_MediaType_MSE:
    {
        return firebolt::rialto::MediaType::MSE;
    }
    }
    return firebolt::rialto::MediaType::UNKNOWN;
}

firebolt::rialto::EaseType convertEaseType(const firebolt::rialto::SetVolumeRequest_EaseType &easeType)
{
    switch (easeType)
    {
    case firebolt::rialto::SetVolumeRequest_EaseType::SetVolumeRequest_EaseType_EASE_LINEAR:
    {
        return firebolt::rialto::EaseType::EASE_LINEAR;
    }
    case firebolt::rialto::SetVolumeRequest_EaseType::SetVolumeRequest_EaseType_EASE_IN_CUBIC:
    {
        return firebolt::rialto::EaseType::EASE_IN_CUBIC;
    }
    case firebolt::rialto::SetVolumeRequest_EaseType::SetVolumeRequest_EaseType_EASE_OUT_CUBIC:
    {
        return firebolt::rialto::EaseType::EASE_OUT_CUBIC;
    }
    }
    return firebolt::rialto::EaseType::EASE_LINEAR;
}

firebolt::rialto::MediaSourceStatus
convertMediaSourceStatus(const firebolt::rialto::HaveDataRequest_MediaSourceStatus &status)
{
    switch (status)
    {
    case firebolt::rialto::HaveDataRequest_MediaSourceStatus_UNKNOWN:
    {
        return firebolt::rialto::MediaSourceStatus::ERROR;
    }
    case firebolt::rialto::HaveDataRequest_MediaSourceStatus_OK:
    {
        return firebolt::rialto::MediaSourceStatus::OK;
    }
    case firebolt::rialto::HaveDataRequest_MediaSourceStatus_EOS:
    {
        return firebolt::rialto::MediaSourceStatus::EOS;
    }
    case firebolt::rialto::HaveDataRequest_MediaSourceStatus_ERROR:
    {
        return firebolt::rialto::MediaSourceStatus::ERROR;
    }
    case firebolt::rialto::HaveDataRequest_MediaSourceStatus_CODEC_CHANGED:
    {
        return firebolt::rialto::MediaSourceStatus::CODEC_CHANGED;
    }
    case firebolt::rialto::HaveDataRequest_MediaSourceStatus_NO_AVAILABLE_SAMPLES:
    {
        return firebolt::rialto::MediaSourceStatus::NO_AVAILABLE_SAMPLES;
    }
    }
    return firebolt::rialto::MediaSourceStatus::ERROR;
}

firebolt::rialto::SourceConfigType convertConfigType(const firebolt::rialto::AttachSourceRequest_ConfigType &configType)
{
    switch (configType)
    {
    case firebolt::rialto::AttachSourceRequest_ConfigType_CONFIG_TYPE_UNKNOWN:
    {
        return firebolt::rialto::SourceConfigType::UNKNOWN;
    }
    case firebolt::rialto::AttachSourceRequest_ConfigType_CONFIG_TYPE_AUDIO:
    {
        return firebolt::rialto::SourceConfigType::AUDIO;
    }
    case firebolt::rialto::AttachSourceRequest_ConfigType_CONFIG_TYPE_VIDEO:
    {
        return firebolt::rialto::SourceConfigType::VIDEO;
    }
    case firebolt::rialto::AttachSourceRequest_ConfigType_CONFIG_TYPE_VIDEO_DOLBY_VISION:
    {
        return firebolt::rialto::SourceConfigType::VIDEO_DOLBY_VISION;
    }
    case firebolt::rialto::AttachSourceRequest_ConfigType_CONFIG_TYPE_SUBTITLE:
    {
        return firebolt::rialto::SourceConfigType::SUBTITLE;
    }
    }
    return firebolt::rialto::SourceConfigType::UNKNOWN;
}

firebolt::rialto::SegmentAlignment
convertSegmentAlignment(const firebolt::rialto::AttachSourceRequest_SegmentAlignment &alignment)
{
    switch (alignment)
    {
    case firebolt::rialto::AttachSourceRequest_SegmentAlignment_ALIGNMENT_UNDEFINED:
    {
        return firebolt::rialto::SegmentAlignment::UNDEFINED;
    }
    case firebolt::rialto::AttachSourceRequest_SegmentAlignment_ALIGNMENT_NAL:
    {
        return firebolt::rialto::SegmentAlignment::NAL;
    }
    case firebolt::rialto::AttachSourceRequest_SegmentAlignment_ALIGNMENT_AU:
    {
        return firebolt::rialto::SegmentAlignment::AU;
    }
    }

    return firebolt::rialto::SegmentAlignment::UNDEFINED;
}

firebolt::rialto::StreamFormat convertStreamFormat(const firebolt::rialto::AttachSourceRequest_StreamFormat &streamFormat)
{
    switch (streamFormat)
    {
    case firebolt::rialto::AttachSourceRequest_StreamFormat_STREAM_FORMAT_RAW:
    {
        return firebolt::rialto::StreamFormat::RAW;
    }
    case firebolt::rialto::AttachSourceRequest_StreamFormat_STREAM_FORMAT_AVC:
    {
        return firebolt::rialto::StreamFormat::AVC;
    }
    case firebolt::rialto::AttachSourceRequest_StreamFormat_STREAM_FORMAT_BYTE_STREAM:
    {
        return firebolt::rialto::StreamFormat::BYTE_STREAM;
    }
    case firebolt::rialto::AttachSourceRequest_StreamFormat_STREAM_FORMAT_HVC1:
    {
        return firebolt::rialto::StreamFormat::HVC1;
    }
    case firebolt::rialto::AttachSourceRequest_StreamFormat_STREAM_FORMAT_HEV1:
    {
        return firebolt::rialto::StreamFormat::HEV1;
    }
    default:
        return firebolt::rialto::StreamFormat::UNDEFINED;
    }
}

firebolt::rialto::CodecDataType convertCodecDataType(const firebolt::rialto::AttachSourceRequest_CodecData_Type &type)
{
    if (firebolt::rialto::AttachSourceRequest_CodecData_Type_STRING == type)
    {
        return firebolt::rialto::CodecDataType::STRING;
    }
    return firebolt::rialto::CodecDataType::BUFFER;
}

firebolt::rialto::Format convertFormat(const firebolt::rialto::AttachSourceRequest_AudioConfig_Format &format)
{
    static const std::unordered_map<firebolt::rialto::AttachSourceRequest_AudioConfig_Format, firebolt::rialto::Format>
        kFormatConversionMap{
            {firebolt::rialto::AttachSourceRequest_AudioConfig_Format_S8, firebolt::rialto::Format::S8},
            {firebolt::rialto::AttachSourceRequest_AudioConfig_Format_U8, firebolt::rialto::Format::U8},
            {firebolt::rialto::AttachSourceRequest_AudioConfig_Format_S16LE, firebolt::rialto::Format::S16LE},
            {firebolt::rialto::AttachSourceRequest_AudioConfig_Format_S16BE, firebolt::rialto::Format::S16BE},
            {firebolt::rialto::AttachSourceRequest_AudioConfig_Format_U16LE, firebolt::rialto::Format::U16LE},
            {firebolt::rialto::AttachSourceRequest_AudioConfig_Format_U16BE, firebolt::rialto::Format::U16BE},
            {firebolt::rialto::AttachSourceRequest_AudioConfig_Format_S24_32LE, firebolt::rialto::Format::S24_32LE},
            {firebolt::rialto::AttachSourceRequest_AudioConfig_Format_S24_32BE, firebolt::rialto::Format::S24_32BE},
            {firebolt::rialto::AttachSourceRequest_AudioConfig_Format_U24_32LE, firebolt::rialto::Format::U24_32LE},
            {firebolt::rialto::AttachSourceRequest_AudioConfig_Format_U24_32BE, firebolt::rialto::Format::U24_32BE},
            {firebolt::rialto::AttachSourceRequest_AudioConfig_Format_S32LE, firebolt::rialto::Format::S32LE},
            {firebolt::rialto::AttachSourceRequest_AudioConfig_Format_S32BE, firebolt::rialto::Format::S32BE},
            {firebolt::rialto::AttachSourceRequest_AudioConfig_Format_U32LE, firebolt::rialto::Format::U32LE},
            {firebolt::rialto::AttachSourceRequest_AudioConfig_Format_U32BE, firebolt::rialto::Format::U32BE},
            {firebolt::rialto::AttachSourceRequest_AudioConfig_Format_S24LE, firebolt::rialto::Format::S24LE},
            {firebolt::rialto::AttachSourceRequest_AudioConfig_Format_S24BE, firebolt::rialto::Format::S24BE},
            {firebolt::rialto::AttachSourceRequest_AudioConfig_Format_U24LE, firebolt::rialto::Format::U24LE},
            {firebolt::rialto::AttachSourceRequest_AudioConfig_Format_U24BE, firebolt::rialto::Format::U24BE},
            {firebolt::rialto::AttachSourceRequest_AudioConfig_Format_S20LE, firebolt::rialto::Format::S20LE},
            {firebolt::rialto::AttachSourceRequest_AudioConfig_Format_S20BE, firebolt::rialto::Format::S20BE},
            {firebolt::rialto::AttachSourceRequest_AudioConfig_Format_U20LE, firebolt::rialto::Format::U20LE},
            {firebolt::rialto::AttachSourceRequest_AudioConfig_Format_U20BE, firebolt::rialto::Format::U20BE},
            {firebolt::rialto::AttachSourceRequest_AudioConfig_Format_S18LE, firebolt::rialto::Format::S18LE},
            {firebolt::rialto::AttachSourceRequest_AudioConfig_Format_S18BE, firebolt::rialto::Format::S18BE},
            {firebolt::rialto::AttachSourceRequest_AudioConfig_Format_U18LE, firebolt::rialto::Format::U18LE},
            {firebolt::rialto::AttachSourceRequest_AudioConfig_Format_U18BE, firebolt::rialto::Format::U18BE},
            {firebolt::rialto::AttachSourceRequest_AudioConfig_Format_F32LE, firebolt::rialto::Format::F32LE},
            {firebolt::rialto::AttachSourceRequest_AudioConfig_Format_F32BE, firebolt::rialto::Format::F32BE},
            {firebolt::rialto::AttachSourceRequest_AudioConfig_Format_F64LE, firebolt::rialto::Format::F64LE},
            {firebolt::rialto::AttachSourceRequest_AudioConfig_Format_F64BE, firebolt::rialto::Format::F64BE}};
    const auto kIt = kFormatConversionMap.find(format);
    if (kFormatConversionMap.end() != kIt)
    {
        return kIt->second;
    }
    return firebolt::rialto::Format::S8;
}

firebolt::rialto::Layout convertLayout(const firebolt::rialto::AttachSourceRequest_AudioConfig_Layout &layout)
{
    static const std::unordered_map<firebolt::rialto::AttachSourceRequest_AudioConfig_Layout, firebolt::rialto::Layout>
        kLayoutConversionMap{{firebolt::rialto::AttachSourceRequest_AudioConfig_Layout_INTERLEAVED,
                              firebolt::rialto::Layout::INTERLEAVED},
                             {firebolt::rialto::AttachSourceRequest_AudioConfig_Layout_NON_INTERLEAVED,
                              firebolt::rialto::Layout::NON_INTERLEAVED}};
    const auto kIt = kLayoutConversionMap.find(layout);
    if (kLayoutConversionMap.end() != kIt)
    {
        return kIt->second;
    }
    return firebolt::rialto::Layout::INTERLEAVED;
}
} // namespace

namespace firebolt::rialto::server::ipc
{
std::shared_ptr<IMediaPipelineModuleServiceFactory> IMediaPipelineModuleServiceFactory::createFactory()
{
    std::shared_ptr<IMediaPipelineModuleServiceFactory> factory;

    try
    {
        factory = std::make_shared<MediaPipelineModuleServiceFactory>();
    }
    catch (const std::exception &e)
    {
        RIALTO_SERVER_LOG_ERROR("Failed to create the media player module service factory, reason: %s", e.what());
    }

    return factory;
}

std::shared_ptr<IMediaPipelineModuleService>
MediaPipelineModuleServiceFactory::create(service::IMediaPipelineService &mediaPipelineService) const
{
    std::shared_ptr<IMediaPipelineModuleService> mediaPipelineModule;

    try
    {
        mediaPipelineModule = std::make_shared<MediaPipelineModuleService>(mediaPipelineService);
    }
    catch (const std::exception &e)
    {
        RIALTO_SERVER_LOG_ERROR("Failed to create the media player module service, reason: %s", e.what());
    }

    return mediaPipelineModule;
}

MediaPipelineModuleService::MediaPipelineModuleService(service::IMediaPipelineService &mediaPipelineService)
    : m_mediaPipelineService{mediaPipelineService}
{
}

MediaPipelineModuleService::~MediaPipelineModuleService() {}

void MediaPipelineModuleService::clientConnected(const std::shared_ptr<::firebolt::rialto::ipc::IClient> &ipcClient)
{
    RIALTO_SERVER_LOG_INFO("Client Connected!");
    {
        m_clientSessions.emplace(ipcClient, std::set<int>());
    }
    ipcClient->exportService(shared_from_this());
}

void MediaPipelineModuleService::clientDisconnected(const std::shared_ptr<::firebolt::rialto::ipc::IClient> &ipcClient)
{
    RIALTO_SERVER_LOG_INFO("Client disconnected!");
    std::set<int> sessionIds;
    {
        auto sessionIter = m_clientSessions.find(ipcClient);
        if (sessionIter == m_clientSessions.end())
        {
            RIALTO_SERVER_LOG_ERROR("unknown client disconnected");
            return;
        }
        sessionIds = sessionIter->second; // copy to avoid deadlock
        m_clientSessions.erase(sessionIter);
    }
    for (const auto &sessionId : sessionIds)
    {
        m_mediaPipelineService.destroySession(sessionId);
    }
}

void MediaPipelineModuleService::createSession(::google::protobuf::RpcController *controller,
                                               const ::firebolt::rialto::CreateSessionRequest *request,
                                               ::firebolt::rialto::CreateSessionResponse *response,
                                               ::google::protobuf::Closure *done)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");
    auto ipcController = dynamic_cast<firebolt::rialto::ipc::IController *>(controller);
    if (!ipcController)
    {
        RIALTO_SERVER_LOG_ERROR("ipc library provided incompatible controller object");
        controller->SetFailed("ipc library provided incompatible controller object");
        done->Run();
        return;
    }

    int sessionId = generateSessionId();
    bool sessionCreated =
        m_mediaPipelineService.createSession(sessionId,
                                             std::make_shared<MediaPipelineClient>(sessionId, ipcController->getClient()),
                                             request->max_width(), request->max_height());
    if (sessionCreated)
    {
        // Assume that IPC library works well and client is present
        m_clientSessions[ipcController->getClient()].insert(sessionId);
        response->set_session_id(sessionId);
    }
    else
    {
        RIALTO_SERVER_LOG_ERROR("Create session failed");
        controller->SetFailed("Operation failed");
    }
    done->Run();
}

void MediaPipelineModuleService::destroySession(::google::protobuf::RpcController *controller,
                                                const ::firebolt::rialto::DestroySessionRequest *request,
                                                ::firebolt::rialto::DestroySessionResponse *response,
                                                ::google::protobuf::Closure *done)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");
    auto ipcController = dynamic_cast<firebolt::rialto::ipc::IController *>(controller);
    if (!ipcController)
    {
        RIALTO_SERVER_LOG_ERROR("ipc library provided incompatible controller object");
        controller->SetFailed("ipc library provided incompatible controller object");
        done->Run();
        return;
    }

    if (!m_mediaPipelineService.destroySession(request->session_id()))
    {
        RIALTO_SERVER_LOG_ERROR("Destroy session failed");
        controller->SetFailed("Operation failed");
        done->Run();
        return;
    }
    auto sessionIter = m_clientSessions.find(ipcController->getClient());
    if (sessionIter != m_clientSessions.end())
    {
        sessionIter->second.erase(request->session_id());
    }
    done->Run();
}

void MediaPipelineModuleService::load(::google::protobuf::RpcController *controller,
                                      const ::firebolt::rialto::LoadRequest *request,
                                      ::firebolt::rialto::LoadResponse *response, ::google::protobuf::Closure *done)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");
    if (!m_mediaPipelineService.load(request->session_id(), convertMediaType(request->type()), request->mime_type(),
                                     request->url()))
    {
        RIALTO_SERVER_LOG_ERROR("Load failed");
        controller->SetFailed("Operation failed");
    }
    done->Run();
}

void MediaPipelineModuleService::setVideoWindow(::google::protobuf::RpcController *controller,
                                                const ::firebolt::rialto::SetVideoWindowRequest *request,
                                                ::firebolt::rialto::SetVideoWindowResponse *response,
                                                ::google::protobuf::Closure *done)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");
    if (!m_mediaPipelineService.setVideoWindow(request->session_id(), request->x(), request->y(), request->width(),
                                               request->height()))
    {
        RIALTO_SERVER_LOG_ERROR("Set Video Window failed");
        controller->SetFailed("Operation failed");
    }
    done->Run();
}

void MediaPipelineModuleService::attachSource(::google::protobuf::RpcController *controller,
                                              const ::firebolt::rialto::AttachSourceRequest *request,
                                              ::firebolt::rialto::AttachSourceResponse *response,
                                              ::google::protobuf::Closure *done)
{
    RIALTO_SERVER_LOG_DEBUG("mime_type: %s", request->mime_type().c_str());

    std::shared_ptr<CodecData> codecData{};
    if (request->has_codec_data())
    {
        auto codecDataProto = request->codec_data();
        codecData = std::make_shared<CodecData>();
        codecData->data = std::vector<std::uint8_t>(codecDataProto.data().begin(), codecDataProto.data().end());
        codecData->type = convertCodecDataType(codecDataProto.type());
    }
    std::unique_ptr<IMediaPipeline::MediaSource> mediaSource;
    firebolt::rialto::SourceConfigType configType = convertConfigType(request->config_type());
    bool hasDrm = request->has_drm();

    if (configType == firebolt::rialto::SourceConfigType::AUDIO)
    {
        const auto &kConfig = request->audio_config();
        uint32_t numberofchannels = kConfig.number_of_channels();
        uint32_t sampleRate = kConfig.sample_rate();

        std::vector<uint8_t> codecSpecificConfig;
        if (kConfig.has_codec_specific_config())
        {
            auto codecSpecificConfigStr = kConfig.codec_specific_config();
            codecSpecificConfig.assign(codecSpecificConfigStr.begin(), codecSpecificConfigStr.end());
        }
        std::optional<firebolt::rialto::Format> format{std::nullopt};
        if (kConfig.has_format())
        {
            format = convertFormat(kConfig.format());
        }
        std::optional<firebolt::rialto::Layout> layout{std::nullopt};
        if (kConfig.has_layout())
        {
            layout = convertLayout(kConfig.layout());
        }
        std::optional<uint64_t> channelMask{std::nullopt};
        if (kConfig.has_channel_mask())
        {
            channelMask = kConfig.channel_mask();
        }
        std::vector<std::vector<uint8_t>> streamHeaders;
        for (int i = 0; i < kConfig.stream_header_size(); ++i)
        {
            auto streamHeaderStr = kConfig.stream_header(i);
            streamHeaders.push_back(std::vector<uint8_t>{streamHeaderStr.begin(), streamHeaderStr.end()});
        }
        std::optional<bool> framed;
        if (kConfig.has_framed())
        {
            framed = kConfig.framed();
        }
        AudioConfig audioConfig{numberofchannels, sampleRate,  codecSpecificConfig, format,
                                layout,           channelMask, streamHeaders,       framed};

        mediaSource =
            std::make_unique<IMediaPipeline::MediaSourceAudio>(request->mime_type(), hasDrm, audioConfig,
                                                               convertSegmentAlignment(request->segment_alignment()),
                                                               convertStreamFormat(request->stream_format()), codecData);
    }
    else if (configType == firebolt::rialto::SourceConfigType::VIDEO)
    {
        mediaSource =
            std::make_unique<IMediaPipeline::MediaSourceVideo>(request->mime_type().c_str(), hasDrm, request->width(),
                                                               request->height(),
                                                               convertSegmentAlignment(request->segment_alignment()),
                                                               convertStreamFormat(request->stream_format()), codecData);
    }
    else if (configType == firebolt::rialto::SourceConfigType::VIDEO_DOLBY_VISION)
    {
        mediaSource =
            std::make_unique<IMediaPipeline::MediaSourceVideoDolbyVision>(request->mime_type().c_str(),
                                                                          request->dolby_vision_profile(), hasDrm,
                                                                          request->width(), request->height(),
                                                                          convertSegmentAlignment(
                                                                              request->segment_alignment()),
                                                                          convertStreamFormat(request->stream_format()),
                                                                          codecData);
    }
    else if (configType == firebolt::rialto::SourceConfigType::SUBTITLE)
    {
        mediaSource = std::make_unique<IMediaPipeline::MediaSourceSubtitle>(request->mime_type().c_str(),
                                                                            request->text_track_identifier());
    }
    else
    {
        RIALTO_SERVER_LOG_ERROR("Unknown source type");
        controller->SetFailed("Operation failed");
        done->Run();
        return;
    }

    if (!request->has_switch_source() || !request->switch_source())
    {
        RIALTO_SERVER_LOG_DEBUG("Attaching source");
        if (!m_mediaPipelineService.attachSource(request->session_id(), mediaSource))
        {
            RIALTO_SERVER_LOG_ERROR("Attach source failed");
            controller->SetFailed("Operation failed");
        }
    }
    else
    {
        RIALTO_SERVER_LOG_DEBUG("Switching source");
        if (!m_mediaPipelineService.switchSource(request->session_id(), mediaSource))
        {
            RIALTO_SERVER_LOG_ERROR("Switch source failed");
            controller->SetFailed("Operation failed");
        }
    }
    response->set_source_id(mediaSource->getId());
    done->Run();
}

void MediaPipelineModuleService::removeSource(::google::protobuf::RpcController *controller,
                                              const ::firebolt::rialto::RemoveSourceRequest *request,
                                              ::firebolt::rialto::RemoveSourceResponse *response,
                                              ::google::protobuf::Closure *done)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");
    if (!m_mediaPipelineService.removeSource(request->session_id(), request->source_id()))
    {
        RIALTO_SERVER_LOG_ERROR("Remove source failed");
        controller->SetFailed("Operation failed");
    }
    done->Run();
}

void MediaPipelineModuleService::allSourcesAttached(::google::protobuf::RpcController *controller,
                                                    const ::firebolt::rialto::AllSourcesAttachedRequest *request,
                                                    ::firebolt::rialto::AllSourcesAttachedResponse *response,
                                                    ::google::protobuf::Closure *done)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");
    if (!m_mediaPipelineService.allSourcesAttached(request->session_id()))
    {
        RIALTO_SERVER_LOG_ERROR("All sources attached failed");
        controller->SetFailed("Operation failed");
    }
    done->Run();
}

void MediaPipelineModuleService::play(::google::protobuf::RpcController *controller,
                                      const ::firebolt::rialto::PlayRequest *request,
                                      ::firebolt::rialto::PlayResponse *response, ::google::protobuf::Closure *done)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");
    if (!m_mediaPipelineService.play(request->session_id()))
    {
        RIALTO_SERVER_LOG_ERROR("Play failed");
        controller->SetFailed("Operation failed");
    }
    done->Run();
}

void MediaPipelineModuleService::pause(::google::protobuf::RpcController *controller,
                                       const ::firebolt::rialto::PauseRequest *request,
                                       ::firebolt::rialto::PauseResponse *response, ::google::protobuf::Closure *done)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");
    if (!m_mediaPipelineService.pause(request->session_id()))
    {
        RIALTO_SERVER_LOG_ERROR("pause failed");
        controller->SetFailed("Operation failed");
    }
    done->Run();
}

void MediaPipelineModuleService::stop(::google::protobuf::RpcController *controller,
                                      const ::firebolt::rialto::StopRequest *request,
                                      ::firebolt::rialto::StopResponse *response, ::google::protobuf::Closure *done)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");
    if (!m_mediaPipelineService.stop(request->session_id()))
    {
        RIALTO_SERVER_LOG_ERROR("Stop failed");
        controller->SetFailed("Operation failed");
    }
    done->Run();
}

void MediaPipelineModuleService::setPosition(::google::protobuf::RpcController *controller,
                                             const ::firebolt::rialto::SetPositionRequest *request,
                                             ::firebolt::rialto::SetPositionResponse *response,
                                             ::google::protobuf::Closure *done)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");
    if (!m_mediaPipelineService.setPosition(request->session_id(), request->position()))
    {
        RIALTO_SERVER_LOG_ERROR("Set Position failed");
        controller->SetFailed("Operation failed");
    }
    done->Run();
}

void MediaPipelineModuleService::haveData(::google::protobuf::RpcController *controller,
                                          const ::firebolt::rialto::HaveDataRequest *request,
                                          ::firebolt::rialto::HaveDataResponse *response,
                                          ::google::protobuf::Closure *done)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");
    firebolt::rialto::MediaSourceStatus status{convertMediaSourceStatus(request->status())};
    if (!m_mediaPipelineService.haveData(request->session_id(), status, request->num_frames(), request->request_id()))
    {
        RIALTO_SERVER_LOG_ERROR("Have data failed");
        controller->SetFailed("Operation failed");
    }
    done->Run();
}

void MediaPipelineModuleService::setPlaybackRate(::google::protobuf::RpcController *controller,
                                                 const ::firebolt::rialto::SetPlaybackRateRequest *request,
                                                 ::firebolt::rialto::SetPlaybackRateResponse *response,
                                                 ::google::protobuf::Closure *done)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");
    if (!m_mediaPipelineService.setPlaybackRate(request->session_id(), request->rate()))
    {
        RIALTO_SERVER_LOG_ERROR("Set playback rate failed");
        controller->SetFailed("Operation failed");
    }
    done->Run();
}

void MediaPipelineModuleService::getPosition(::google::protobuf::RpcController *controller,
                                             const ::firebolt::rialto::GetPositionRequest *request,
                                             ::firebolt::rialto::GetPositionResponse *response,
                                             ::google::protobuf::Closure *done)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");
    int64_t position{};
    if (!m_mediaPipelineService.getPosition(request->session_id(), position))
    {
        RIALTO_SERVER_LOG_ERROR("Get position failed");
        controller->SetFailed("Operation failed");
    }
    else
    {
        response->set_position(position);
    }
    done->Run();
}

void MediaPipelineModuleService::setImmediateOutput(::google::protobuf::RpcController *controller,
                                                    const ::firebolt::rialto::SetImmediateOutputRequest *request,
                                                    ::firebolt::rialto::SetImmediateOutputResponse *response,
                                                    ::google::protobuf::Closure *done)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");
    if (!m_mediaPipelineService.setImmediateOutput(request->session_id(), request->source_id(),
                                                   request->immediate_output()))
    {
        RIALTO_SERVER_LOG_ERROR("Set Immediate Output failed");
        controller->SetFailed("Operation failed");
    }
    done->Run();
}

void MediaPipelineModuleService::getImmediateOutput(::google::protobuf::RpcController *controller,
                                                    const ::firebolt::rialto::GetImmediateOutputRequest *request,
                                                    ::firebolt::rialto::GetImmediateOutputResponse *response,
                                                    ::google::protobuf::Closure *done)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");
    bool immediateOutputState;
    if (!m_mediaPipelineService.getImmediateOutput(request->session_id(), request->source_id(), immediateOutputState))
    {
        RIALTO_SERVER_LOG_ERROR("Get Immediate Output failed");
        controller->SetFailed("Operation failed");
    }
    else
    {
        response->set_immediate_output(immediateOutputState);
    }
    done->Run();
}

void MediaPipelineModuleService::getStats(::google::protobuf::RpcController *controller,
                                          const ::firebolt::rialto::GetStatsRequest *request,
                                          ::firebolt::rialto::GetStatsResponse *response,
                                          ::google::protobuf::Closure *done)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");
    uint64_t renderedFrames;
    uint64_t droppedFrames;
    if (!m_mediaPipelineService.getStats(request->session_id(), request->source_id(), renderedFrames, droppedFrames))
    {
        RIALTO_SERVER_LOG_ERROR("Get stats failed");
        controller->SetFailed("Operation failed");
    }
    else
    {
        response->set_rendered_frames(renderedFrames);
        response->set_dropped_frames(droppedFrames);
    }
    done->Run();
}

void MediaPipelineModuleService::renderFrame(::google::protobuf::RpcController *controller,
                                             const ::firebolt::rialto::RenderFrameRequest *request,
                                             ::firebolt::rialto::RenderFrameResponse *response,
                                             ::google::protobuf::Closure *done)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");

    if (!m_mediaPipelineService.renderFrame(request->session_id()))
    {
        RIALTO_SERVER_LOG_ERROR("Render frame");
        controller->SetFailed("Operation failed");
    }

    done->Run();
}

void MediaPipelineModuleService::setVolume(::google::protobuf::RpcController *controller,
                                           const ::firebolt::rialto::SetVolumeRequest *request,
                                           ::firebolt::rialto::SetVolumeResponse *response,
                                           ::google::protobuf::Closure *done)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");

    if (!m_mediaPipelineService.setVolume(request->session_id(), request->volume(), request->volume_duration(),
                                          convertEaseType(request->ease_type())))
    {
        RIALTO_SERVER_LOG_ERROR("Set volume failed.");
        controller->SetFailed("Operation failed");
    }

    done->Run();
}

void MediaPipelineModuleService::getVolume(::google::protobuf::RpcController *controller,
                                           const ::firebolt::rialto::GetVolumeRequest *request,
                                           ::firebolt::rialto::GetVolumeResponse *response,
                                           ::google::protobuf::Closure *done)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");
    double volume{};

    if (!m_mediaPipelineService.getVolume(request->session_id(), volume))
    {
        RIALTO_SERVER_LOG_ERROR("Get volume failed.");
        controller->SetFailed("Operation failed");
    }
    else
    {
        response->set_volume(volume);
    }

    done->Run();
}

void MediaPipelineModuleService::setMute(::google::protobuf::RpcController *controller,
                                         const ::firebolt::rialto::SetMuteRequest *request,
                                         ::firebolt::rialto::SetMuteResponse *response, ::google::protobuf::Closure *done)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");

    if (!m_mediaPipelineService.setMute(request->session_id(), request->source_id(), request->mute()))
    {
        RIALTO_SERVER_LOG_ERROR("Set mute failed.");
        controller->SetFailed("Operation failed");
    }

    done->Run();
}

void MediaPipelineModuleService::getMute(::google::protobuf::RpcController *controller,
                                         const ::firebolt::rialto::GetMuteRequest *request,
                                         ::firebolt::rialto::GetMuteResponse *response, ::google::protobuf::Closure *done)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");
    bool mute{};

    if (!m_mediaPipelineService.getMute(request->session_id(), request->source_id(), mute))
    {
        RIALTO_SERVER_LOG_ERROR("Get mute failed.");
        controller->SetFailed("Operation failed");
    }
    else
    {
        response->set_mute(mute);
    }

    done->Run();
}

void MediaPipelineModuleService::setTextTrackIdentifier(::google::protobuf::RpcController *controller,
                                                        const ::firebolt::rialto::SetTextTrackIdentifierRequest *request,
                                                        ::firebolt::rialto::SetTextTrackIdentifierResponse *response,
                                                        ::google::protobuf::Closure *done)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");

    if (!m_mediaPipelineService.setTextTrackIdentifier(request->session_id(), request->text_track_identifier()))
    {
        RIALTO_SERVER_LOG_ERROR("Set text track identifier failed.");
        controller->SetFailed("Operation failed");
    }

    done->Run();
}

void MediaPipelineModuleService::getTextTrackIdentifier(::google::protobuf::RpcController *controller,
                                                        const ::firebolt::rialto::GetTextTrackIdentifierRequest *request,
                                                        ::firebolt::rialto::GetTextTrackIdentifierResponse *response,
                                                        ::google::protobuf::Closure *done)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");
    std::string textTrackIdentifier{};

    if (!m_mediaPipelineService.getTextTrackIdentifier(request->session_id(), textTrackIdentifier))
    {
        RIALTO_SERVER_LOG_ERROR("Get TextTrackIdentifier failed.");
        controller->SetFailed("Operation failed");
    }
    else
    {
        response->set_text_track_identifier(textTrackIdentifier);
    }

    done->Run();
}

void MediaPipelineModuleService::setLowLatency(::google::protobuf::RpcController *controller,
                                               const ::firebolt::rialto::SetLowLatencyRequest *request,
                                               ::firebolt::rialto::SetLowLatencyResponse *response,
                                               ::google::protobuf::Closure *done)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");

    if (!m_mediaPipelineService.setLowLatency(request->session_id(), request->low_latency()))
    {
        RIALTO_SERVER_LOG_ERROR("Set low latency failed.");
        controller->SetFailed("Operation failed");
    }

    done->Run();
}

void MediaPipelineModuleService::setSync(::google::protobuf::RpcController *controller,
                                         const ::firebolt::rialto::SetSyncRequest *request,
                                         ::firebolt::rialto::SetSyncResponse *response, ::google::protobuf::Closure *done)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");

    if (!m_mediaPipelineService.setSync(request->session_id(), request->sync()))
    {
        RIALTO_SERVER_LOG_ERROR("Set sync failed.");
        controller->SetFailed("Operation failed");
    }

    done->Run();
}

void MediaPipelineModuleService::getSync(::google::protobuf::RpcController *controller,
                                         const ::firebolt::rialto::GetSyncRequest *request,
                                         ::firebolt::rialto::GetSyncResponse *response, ::google::protobuf::Closure *done)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");
    bool sync{};

    if (!m_mediaPipelineService.getSync(request->session_id(), sync))
    {
        RIALTO_SERVER_LOG_ERROR("Get sync failed.");
        controller->SetFailed("Operation failed");
    }
    else
    {
        response->set_sync(sync);
    }

    done->Run();
}

void MediaPipelineModuleService::setSyncOff(::google::protobuf::RpcController *controller,
                                            const ::firebolt::rialto::SetSyncOffRequest *request,
                                            ::firebolt::rialto::SetSyncOffResponse *response,
                                            ::google::protobuf::Closure *done)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");

    if (!m_mediaPipelineService.setSyncOff(request->session_id(), request->sync_off()))
    {
        RIALTO_SERVER_LOG_ERROR("Set sync off failed.");
        controller->SetFailed("Operation failed");
    }

    done->Run();
}

void MediaPipelineModuleService::setStreamSyncMode(::google::protobuf::RpcController *controller,
                                                   const ::firebolt::rialto::SetStreamSyncModeRequest *request,
                                                   ::firebolt::rialto::SetStreamSyncModeResponse *response,
                                                   ::google::protobuf::Closure *done)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");

    if (!m_mediaPipelineService.setStreamSyncMode(request->session_id(), request->source_id(),
                                                  request->stream_sync_mode()))
    {
        RIALTO_SERVER_LOG_ERROR("Set stream sync mode failed.");
        controller->SetFailed("Operation failed");
    }

    done->Run();
}

void MediaPipelineModuleService::getStreamSyncMode(::google::protobuf::RpcController *controller,
                                                   const ::firebolt::rialto::GetStreamSyncModeRequest *request,
                                                   ::firebolt::rialto::GetStreamSyncModeResponse *response,
                                                   ::google::protobuf::Closure *done)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");
    int32_t streamSyncMode{};

    if (!m_mediaPipelineService.getStreamSyncMode(request->session_id(), streamSyncMode))
    {
        RIALTO_SERVER_LOG_ERROR("Get stream sync mode failed.");
        controller->SetFailed("Operation failed");
    }
    else
    {
        response->set_stream_sync_mode(streamSyncMode);
    }

    done->Run();
}

void MediaPipelineModuleService::flush(::google::protobuf::RpcController *controller,
                                       const ::firebolt::rialto::FlushRequest *request,
                                       ::firebolt::rialto::FlushResponse *response, ::google::protobuf::Closure *done)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");

    bool isAsync{false};
    if (!m_mediaPipelineService.flush(request->session_id(), request->source_id(), request->reset_time(), isAsync))
    {
        RIALTO_SERVER_LOG_ERROR("Flush failed.");
        controller->SetFailed("Operation failed");
    }
    response->set_async(isAsync);

    done->Run();
}

void MediaPipelineModuleService::setSourcePosition(::google::protobuf::RpcController *controller,
                                                   const ::firebolt::rialto::SetSourcePositionRequest *request,
                                                   ::firebolt::rialto::SetSourcePositionResponse *response,
                                                   ::google::protobuf::Closure *done)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");
    if (!m_mediaPipelineService.setSourcePosition(request->session_id(), request->source_id(), request->position(),
                                                  request->reset_time(), request->applied_rate(),
                                                  request->stop_position()))
    {
        RIALTO_SERVER_LOG_ERROR("Set Source Position failed.");
        controller->SetFailed("Operation failed");
    }
    done->Run();
}

void MediaPipelineModuleService::setSubtitleOffset(::google::protobuf::RpcController *controller,
                                                   const ::firebolt::rialto::SetSubtitleOffsetRequest *request,
                                                   ::firebolt::rialto::SetSubtitleOffsetResponse *response,
                                                   ::google::protobuf::Closure *done)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");
    if (!m_mediaPipelineService.setSubtitleOffset(request->session_id(), request->source_id(), request->position()))
    {
        RIALTO_SERVER_LOG_ERROR("Set Subtitle Offset failed.");
        controller->SetFailed("Operation failed");
    }
    done->Run();
}

void MediaPipelineModuleService::processAudioGap(::google::protobuf::RpcController *controller,
                                                 const ::firebolt::rialto::ProcessAudioGapRequest *request,
                                                 ::firebolt::rialto::ProcessAudioGapResponse *response,
                                                 ::google::protobuf::Closure *done)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");
    if (!m_mediaPipelineService.processAudioGap(request->session_id(), request->position(), request->duration(),
                                                request->discontinuity_gap(), request->audio_aac()))
    {
        RIALTO_SERVER_LOG_ERROR("Process audio gap failed.");
        controller->SetFailed("Operation failed");
    }
    done->Run();
}

void MediaPipelineModuleService::setBufferingLimit(::google::protobuf::RpcController *controller,
                                                   const ::firebolt::rialto::SetBufferingLimitRequest *request,
                                                   ::firebolt::rialto::SetBufferingLimitResponse *response,
                                                   ::google::protobuf::Closure *done)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");
    if (!m_mediaPipelineService.setBufferingLimit(request->session_id(), request->limit_buffering_ms()))
    {
        RIALTO_SERVER_LOG_ERROR("Set buffering limit failed.");
        controller->SetFailed("Operation failed");
    }
    done->Run();
}

void MediaPipelineModuleService::getBufferingLimit(::google::protobuf::RpcController *controller,
                                                   const ::firebolt::rialto::GetBufferingLimitRequest *request,
                                                   ::firebolt::rialto::GetBufferingLimitResponse *response,
                                                   ::google::protobuf::Closure *done)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");
    uint32_t bufferingLimit{};

    if (!m_mediaPipelineService.getBufferingLimit(request->session_id(), bufferingLimit))
    {
        RIALTO_SERVER_LOG_ERROR("Get buffering limit failed.");
        controller->SetFailed("Operation failed");
    }
    else
    {
        response->set_limit_buffering_ms(bufferingLimit);
    }

    done->Run();
}

void MediaPipelineModuleService::setUseBuffering(::google::protobuf::RpcController *controller,
                                                 const ::firebolt::rialto::SetUseBufferingRequest *request,
                                                 ::firebolt::rialto::SetUseBufferingResponse *response,
                                                 ::google::protobuf::Closure *done)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");
    if (!m_mediaPipelineService.setUseBuffering(request->session_id(), request->use_buffering()))
    {
        RIALTO_SERVER_LOG_ERROR("Set use buffering failed.");
        controller->SetFailed("Operation failed");
    }
    done->Run();
}

void MediaPipelineModuleService::getUseBuffering(::google::protobuf::RpcController *controller,
                                                 const ::firebolt::rialto::GetUseBufferingRequest *request,
                                                 ::firebolt::rialto::GetUseBufferingResponse *response,
                                                 ::google::protobuf::Closure *done)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");
    bool useBuffering{};

    if (!m_mediaPipelineService.getUseBuffering(request->session_id(), useBuffering))
    {
        RIALTO_SERVER_LOG_ERROR("Get use buffering failed.");
        controller->SetFailed("Operation failed");
    }
    else
    {
        response->set_use_buffering(useBuffering);
    }

    done->Run();
}
} // namespace firebolt::rialto::server::ipc
