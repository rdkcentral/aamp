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

#ifndef FIREBOLT_RIALTO_CLIENT_MEDIA_PIPELINE_IPC_H_
#define FIREBOLT_RIALTO_CLIENT_MEDIA_PIPELINE_IPC_H_

#include "IEventThread.h"
#include "IMediaPipelineIpc.h"
#include "IpcModule.h"
#include <IMediaPipeline.h>
#include <memory>
#include <string>

#include "mediapipelinemodule.pb.h"

namespace firebolt::rialto::client
{
/**
 * @brief IMediaPipelineIpc factory class definition.
 */
class MediaPipelineIpcFactory : public IMediaPipelineIpcFactory
{
public:
    /**
     * @brief Weak pointer to the singleton factory object.
     */
    static std::weak_ptr<IMediaPipelineIpcFactory> m_factory;

    std::unique_ptr<IMediaPipelineIpc> createMediaPipelineIpc(IMediaPipelineIpcClient *client,
                                                              const VideoRequirements &videoRequirements,
                                                              std::weak_ptr<IIpcClient> ipcClient) override;
};

/**
 * @brief The definition of the MediaPipelineIpc.
 */
class MediaPipelineIpc : public IMediaPipelineIpc, public IpcModule
{
public:
    /**
     * @brief The constructor.
     *
     * @param[in] client                : The Rialto ipc media player client.
     * @param[in] videoRequirements     : The video decoder requirements for the MediaPipeline session.
     * @param[in] ipcClient             : The ipc client
     * @param[in] eventThreadFactory    : The event thread factory
     */
    MediaPipelineIpc(IMediaPipelineIpcClient *client, const VideoRequirements &videoRequirements, IIpcClient &ipcClient,
                     const std::shared_ptr<common::IEventThreadFactory> &eventThreadFactory);

    /**
     * @brief Virtual destructor.
     */
    virtual ~MediaPipelineIpc();

    bool attachSource(const std::unique_ptr<IMediaPipeline::MediaSource> &source, int32_t &sourceId) override;

    bool removeSource(int32_t sourceId) override;

    bool allSourcesAttached() override;

    bool load(MediaType type, const std::string &mimeType, const std::string &url) override;

    bool setVideoWindow(uint32_t x, uint32_t y, uint32_t width, uint32_t height) override;

    bool play() override;

    bool pause() override;

    bool stop() override;

    bool haveData(MediaSourceStatus status, uint32_t numFrames, uint32_t requestId) override;

    bool setPosition(int64_t position) override;

    bool getPosition(int64_t &position) override;

    bool setImmediateOutput(int32_t sourceId, bool immediateOutput) override;

    bool getImmediateOutput(int32_t sourceId, bool &immediateOutput) override;

    bool getStats(int32_t sourceId, uint64_t &renderedFrames, uint64_t &droppedFrames) override;

    bool setPlaybackRate(double rate) override;

    bool renderFrame() override;

    bool setVolume(double targetVolume, uint32_t volumeDuration, EaseType easeType) override;

    bool getVolume(double &volume) override;

    bool setMute(int32_t sourceId, bool mute) override;

    bool getMute(int32_t sourceId, bool &mute) override;

    bool setTextTrackIdentifier(const std::string &textTrackIdentifier) override;

    bool getTextTrackIdentifier(std::string &textTrackIdentifier) override;

    bool setLowLatency(bool lowLatency) override;

    bool setSync(bool sync) override;

    bool getSync(bool &sync) override;

    bool setSyncOff(bool syncOff) override;

    bool setStreamSyncMode(int32_t sourceId, int32_t streamSyncMode) override;

    bool getStreamSyncMode(int32_t &streamSyncMode) override;

    bool flush(int32_t sourceId, bool resetTime, bool &async) override;

    bool setSourcePosition(int32_t sourceId, int64_t position, bool resetTime, double appliedRate,
                           uint64_t stopPosition) override;

    bool setSubtitleOffset(int32_t sourceId, int64_t position) override;

    bool processAudioGap(int64_t position, uint32_t duration, int64_t discontinuityGap, bool audioAac) override;

    bool setBufferingLimit(uint32_t limitBufferingMs) override;

    bool getBufferingLimit(uint32_t &limitBufferingMs) override;

    bool setUseBuffering(bool useBuffering) override;

    bool getUseBuffering(bool &useBuffering) override;

    bool switchSource(const std::unique_ptr<IMediaPipeline::MediaSource> &source) override;

private:
    /**
     * @brief The media player client ipc.
     */
    IMediaPipelineIpcClient *m_mediaPipelineIpcClient;

    /**
     * @brief The ipc protobuf media player stub.
     */
    std::unique_ptr<::firebolt::rialto::MediaPipelineModule_Stub> m_mediaPipelineStub;

    /**
     * @brief The session id of the current session.
     */
    // onPlaybackStateUpdated() can be called before createSession() therefore
    // initialise m_sessionId to -1 which is an invalid session_id
    std::atomic<int> m_sessionId{-1};

    /**
     * @brief Thread for handling media player events from the server.
     */
    std::unique_ptr<common::IEventThread> m_eventThread;

    bool createRpcStubs(const std::shared_ptr<ipc::IChannel> &ipcChannel) override;

    bool subscribeToEvents(const std::shared_ptr<ipc::IChannel> &ipcChannel) override;

    /**
     * @brief Handler for a playback state update from the server.
     *
     * @param[in] event : The player state changed event structure.
     */
    void onPlaybackStateUpdated(const std::shared_ptr<firebolt::rialto::PlaybackStateChangeEvent> &event);

    /**
     * @brief Handler for a playback position update from the server.
     *
     * @param[in] event : The playback position changed event structure.
     */
    void onPositionUpdated(const std::shared_ptr<firebolt::rialto::PositionChangeEvent> &event);

    /**
     * @brief Handler for a network state update from the server.
     *
     * @param[in] event : The player state changed event structure.
     */
    void onNetworkStateUpdated(const std::shared_ptr<firebolt::rialto::NetworkStateChangeEvent> &event);

    /**
     * @brief Handler for a need data request from the server.
     *
     * @param[in] event : The need data event structure.
     */
    void onNeedMediaData(const std::shared_ptr<firebolt::rialto::NeedMediaDataEvent> &event);

    /**
     * @brief Handler for a QOS update from the server.
     *
     * @param[in] event : The qos event structure.
     */
    void onQos(const std::shared_ptr<firebolt::rialto::QosEvent> &event);

    /**
     * @brief Handler for a buffer underflow notification from the server.
     *
     * @param[in] event : The buffer underflow event structure.
     */
    void onBufferUnderflow(const std::shared_ptr<firebolt::rialto::BufferUnderflowEvent> &event);

    /**
     * @brief Handler for a playback error notification from the server.
     *
     * @param[in] event : The playback error event structure.
     */
    void onPlaybackError(const std::shared_ptr<firebolt::rialto::PlaybackErrorEvent> &event);

    /**
     * @brief Handler for a source flushed notification from the server.
     *
     * @param[in] event : The source flushed event structure.
     */
    void onSourceFlushed(const std::shared_ptr<firebolt::rialto::SourceFlushedEvent> &event);

    /**
     * @brief Create a new player session.
     *
     * @param[in] videoRequirements : The video decoder requirements for the MediaPipeline session.
     *
     * @retval true on success, false otherwise.
     */
    bool createSession(const VideoRequirements &videoRequirements);

    /**
     * @brief Destroy the current player session.
     */
    void destroySession();

    /**
     * @brief Converts the MediaType enum to protobuf LoadRequest MediaType.
     */
    firebolt::rialto::LoadRequest_MediaType convertLoadRequestMediaType(MediaType mediaType) const;

    /**
     * @brief Converts the MediaSourceStatus enum to protobuf HaveDataRequest MediaSourceStatus.
     */
    firebolt::rialto::HaveDataRequest_MediaSourceStatus
    convertHaveDataRequestMediaSourceStatus(MediaSourceStatus status) const;

    /**
     * @brief Converts the SegmentAlignment enum to protobuf AttachSourceRequest SegmentAlignment.
     */
    firebolt::rialto::AttachSourceRequest_SegmentAlignment
    convertSegmentAlignment(const firebolt::rialto::SegmentAlignment &alignment) const;

    /**
     * @brief Converts the StreamFormat enum to protobuf AttachSourceRequest StreamFormat.
     */
    firebolt::rialto::AttachSourceRequest_StreamFormat
    convertStreamFormat(const firebolt::rialto::StreamFormat &streamFormat) const;

    firebolt::rialto::AttachSourceRequest_ConfigType
    convertConfigType(const firebolt::rialto::SourceConfigType &configType) const;

    /**
     * @brief Converts the CodecDataType enum to protobuf AttachSourceRequest CodecDataType.
     */
    firebolt::rialto::AttachSourceRequest_CodecData_Type
    convertCodecDataType(const firebolt::rialto::CodecDataType &codecDataType) const;

    /**
     * @brief Converts the Format enum to protobuf AttachSourceRequest Format.
     */
    firebolt::rialto::AttachSourceRequest_AudioConfig_Format convertFormat(const firebolt::rialto::Format &format) const;

    /**
     * @brief Converts the Layout enum to protobuf AttachSourceRequest Layout.
     */
    firebolt::rialto::AttachSourceRequest_AudioConfig_Layout convertLayout(const firebolt::rialto::Layout &layout) const;

    /**
     * @brief Converts the EaseType enum to protobuf SetVolumeRequest EaseType.
     */
    firebolt::rialto::SetVolumeRequest_EaseType convertEaseType(const firebolt::rialto::EaseType &easeType) const;

    /**
     * @brief Sets AttachSourceRequest parameters based on given MediaSource
     */
    bool buildAttachSourceRequest(firebolt::rialto::AttachSourceRequest &request,
                                  const std::unique_ptr<IMediaPipeline::MediaSource> &source) const;
};

}; // namespace firebolt::rialto::client

#endif // FIREBOLT_RIALTO_CLIENT_MEDIA_PIPELINE_IPC_H_
