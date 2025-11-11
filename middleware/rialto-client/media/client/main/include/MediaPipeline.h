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

#ifndef FIREBOLT_RIALTO_MEDIA_PIPELINE_H_
#define FIREBOLT_RIALTO_MEDIA_PIPELINE_H_

#include "AttachedSources.h"
#include "IClientController.h"
#include "IControlClient.h"
#include "IMediaFrameWriter.h"
#include "IMediaPipeline.h"
#include "IMediaPipelineIpc.h"
#include <atomic>
#include <condition_variable>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace firebolt::rialto
{
/**
 * @brief IMediaPipeline factory class definition.
 */
class MediaPipelineFactory : public IMediaPipelineFactory
{
public:
    MediaPipelineFactory() = default;
    ~MediaPipelineFactory() override = default;

    std::unique_ptr<IMediaPipeline> createMediaPipeline(std::weak_ptr<IMediaPipelineClient> client,
                                                        const VideoRequirements &videoRequirements) const override;

    /**
     * @brief IMediaPipeline factory method with factory parameters for mock injection.
     *
     * @param[in] client                  : The Rialto media player client.
     * @param[in] videoRequirements       : The video decoder requirements for the MediaPipeline session.
     * @param[in] mediaPipelineIpcFactory : This was added for the test environment where a mock object needs to be passed in.
     * @param[in] clientController        : This was added for the test environment where a mock object needs to be passed in.
     *
     * @retval the new backend instance or null on error.
     */
    std::unique_ptr<IMediaPipeline>
    createMediaPipeline(std::weak_ptr<IMediaPipelineClient> client, const VideoRequirements &videoRequirements,
                        std::weak_ptr<client::IMediaPipelineIpcFactory> mediaPipelineIpcFactory,
                        std::weak_ptr<client::IClientController> clientController) const;
};

}; // namespace firebolt::rialto

namespace firebolt::rialto::client
{
class IMediaPipelineAndIControlClient : public IMediaPipeline, public IControlClient
{
};

/**
 * @brief The definition of the MediaPipeline.
 */
class MediaPipeline : public IMediaPipelineAndIControlClient, public IMediaPipelineIpcClient
{
public:
    /**
     * @brief The possible states of the MediaPipeline.
     */
    enum class State
    {
        IDLE,         /**< The MediaPipeline is idle. */
        BUFFERING,    /**< The MediaPipeline is buffering data. */
        PLAYING,      /**< The MediaPipeline is playing. */
        SEEKING,      /**< The MediaPipeline is seeking position. */
        FAILURE,      /**< The MediaPipeline is failed. */
        END_OF_STREAM /**< The MediaPipeline is at the end of stream. */
    };

    /**
     * @brief The constructor.
     *
     * @param[in] client                        : The Rialto media player client.
     * @param[in] videoRequirements             : The video decoder requirements for the MediaPipeline session.
     * @param[in] mediaPipelineIpcFactory         : The media player ipc factory.
     * @param[in] mediaFrameWriterFactory       : The media frame writer factory.
     * @param[in] clientController           : The shared memory manager.
     */
    MediaPipeline(std::weak_ptr<IMediaPipelineClient> client, const VideoRequirements &videoRequirements,
                  const std::shared_ptr<IMediaPipelineIpcFactory> &mediaPipelineIpcFactory,
                  const std::shared_ptr<common::IMediaFrameWriterFactory> &mediaFrameWriterFactory,
                  IClientController &clientController);

    /**
     * @brief Virtual destructor.
     */
    virtual ~MediaPipeline();

    bool load(MediaType type, const std::string &mimeType, const std::string &url) override;

    bool attachSource(const std::unique_ptr<IMediaPipeline::MediaSource> &source) override;

    bool removeSource(int32_t id) override;

    bool allSourcesAttached() override;

    bool play() override;

    bool pause() override;

    bool stop() override;

    bool setPlaybackRate(double rate) override;

    bool setPosition(int64_t position) override;

    bool getPosition(int64_t &position) override;

    bool setImmediateOutput(int32_t sourceId, bool immediateOutput) override;

    bool getImmediateOutput(int32_t sourceId, bool &immediateOutput) override;

    bool getStats(int32_t sourceId, uint64_t &renderedFrames, uint64_t &droppedFrames) override;
    bool setVideoWindow(uint32_t x, uint32_t y, uint32_t width, uint32_t height) override;

    bool haveData(MediaSourceStatus status, uint32_t needDataRequestId) override;

    AddSegmentStatus addSegment(uint32_t needDataRequestId, const std::unique_ptr<MediaSegment> &mediaSegment) override;

    std::weak_ptr<IMediaPipelineClient> getClient() override;

    void notifyPlaybackState(PlaybackState state) override;

    void notifyPosition(int64_t position) override;

    void notifyNetworkState(NetworkState state) override;

    void notifyNeedMediaData(int32_t sourceId, size_t frameCount, uint32_t requestId,
                             const std::shared_ptr<MediaPlayerShmInfo> &shmInfo) override;

    void notifyQos(int32_t sourceId, const QosInfo &qosInfo) override;

    void notifyBufferUnderflow(int32_t sourceId) override;

    void notifyPlaybackError(int32_t sourceId, PlaybackError error) override;

    void notifySourceFlushed(int32_t sourceId) override;

    bool renderFrame() override;

    bool setVolume(double targetVolume, uint32_t volumeDuration, EaseType easeType) override;

    bool getVolume(double &currentVolume) override;

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

    bool switchSource(const std::unique_ptr<MediaSource> &source) override;

    void notifyApplicationState(ApplicationState state) override;

protected:
    /**
     * @brief The need data request data.
     */
    struct NeedDataRequest
    {
        int32_t sourceId;                                       /**< The source id. */
        std::shared_ptr<MediaPlayerShmInfo> shmInfo;            /**< The shared memory information. */
        std::unique_ptr<common::IMediaFrameWriter> frameWriter; /**< The frame writer used to add segments. */
    };

    /**
     * @brief The media player client.
     */
    std::weak_ptr<IMediaPipelineClient> m_mediaPipelineClient;

    /**
     * @brief The media player ipc object.
     */
    std::unique_ptr<IMediaPipelineIpc> m_mediaPipelineIpc;

    /**
     * @brief The rialto shared memory manager object.
     */
    IClientController &m_clientController;

    /**
     * @brief The Need data request map.
     * Key: requestId
     * Value: NeedDataRequest
     */
    std::map<uint32_t, std::shared_ptr<NeedDataRequest>> m_needDataRequestMap;

    /**
     * @brief The current application state. Protected by m_needDataRequestMapMutex
     */
    ApplicationState m_currentAppState;

    /**
     * @brief The need data request map mutex.
     */
    std::mutex m_needDataRequestMapMutex;

    /**
     * @brief The media frame writer factory.
     */
    std::shared_ptr<common::IMediaFrameWriterFactory> m_mediaFrameWriterFactory;

    /**
     * @brief The shared memory mutex.
     */
    std::mutex m_shmMutex;

    /**
     * @brief The current state of the MediaPipeline.
     */
    std::atomic<State> m_currentState;

    /**
     * @brief The flush request mutex.
     */
    std::mutex m_flushMutex;

    /**
     * @brief The attach source mutex.
     */
    std::mutex m_attachSourceMutex;

    /**
     * @brief The attach source condition variable.
     */
    std::condition_variable m_attachSourceCond;

    /**
     * @brief Whether attachSource is currently in progress.
     */
    bool m_attachingSource;

    /**
     * @brief The container with attached source id <-> MediaSourceType mapping
     */
    AttachedSources m_attachedSources;

    /**
     * @brief Sets the new internal MediaPipeline state based on the NetworkState.
     *
     * @param[in] state : The new NeworkState.
     */
    void updateState(NetworkState state);

    /**
     * @brief Sets the new internal MediaPipeline state based on the PlaybackState.
     *
     * @param[in] state : The new PlaybackState.
     */
    void updateState(PlaybackState state);

    /**
     * @brief Handles a have data request.
     *
     * @param[in] status  : The status
     * @param[in] needDataRequestId : Need data request id
     *
     * @retval true on success.
     */
    bool handleHaveData(MediaSourceStatus status, uint32_t needDataRequestId);

    /**
     * @brief Handles a set position request.
     *
     * @param[in] position : The playback position in nanoseconds.
     *
     * @retval true on success.
     */
    bool handleSetPosition(int64_t position);

    /**
     * @brief Discards the need data request with id.
     *
     * @param[in] needDataRequestId : Need data request id
     */
    void discardNeedDataRequest(uint32_t needDataRequestId);
};

}; // namespace firebolt::rialto::client

#endif // FIREBOLT_RIALTO_MEDIA_PIPELINE_H_
