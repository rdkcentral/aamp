/*
 * Copyright (C) 2022   
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <gst/gst.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

#include "BufferParser.h"
#include "Constants.h"
#include "IMediaPipeline.h"
#include "IMessageQueue.h"
#include "IPullModePlaybackDelegate.h"
#include "MediaCommon.h"
#include "MediaPlayerClientBackendInterface.h"
#include "RialtoGStreamerMSEBaseSink.h"

#define DEFAULT_MAX_VIDEO_WIDTH 3840
#define DEFAULT_MAX_VIDEO_HEIGHT 2160

class GStreamerMSEMediaPlayerClient;

enum class ClientState
{
    IDLE,
    READY,
    AWAITING_PAUSED,
    PAUSED,
    AWAITING_PLAYING,
    PLAYING
};

class BufferPuller
{
public:
    BufferPuller(const std::shared_ptr<IMessageQueueFactory> &messageQueueFactory, GstElement *rialtoSink,
                 const std::shared_ptr<BufferParser> &bufferParser,
                 const std::shared_ptr<IPullModePlaybackDelegate> &delegate);

    void start();
    void stop();
    bool requestPullBuffer(int sourceId, size_t frameCount, unsigned int needDataRequestId,
                           GStreamerMSEMediaPlayerClient *player);

private:
    std::unique_ptr<IMessageQueue> m_queue;
    GstElement *m_rialtoSink;
    std::shared_ptr<BufferParser> m_bufferParser;
    std::shared_ptr<IPullModePlaybackDelegate> m_delegate;
};

class AttachedSource
{
    friend class GStreamerMSEMediaPlayerClient;

public:
    AttachedSource(RialtoMSEBaseSink *rialtoSink, std::shared_ptr<BufferPuller> puller,
                   const std::shared_ptr<IPullModePlaybackDelegate> &delegate, firebolt::rialto::MediaSourceType type,
                   ClientState state = ClientState::READY)
        : m_rialtoSink(rialtoSink), m_bufferPuller(puller), m_delegate{delegate}, m_type(type), m_state(state)
    {
    }

    firebolt::rialto::MediaSourceType getType() const { return m_type; }
    void setPosition(int64_t position) { m_position = position; }

private:
    RialtoMSEBaseSink *m_rialtoSink;
    std::shared_ptr<BufferPuller> m_bufferPuller;
    std::shared_ptr<IPullModePlaybackDelegate> m_delegate;
    std::unordered_set<uint32_t> m_ongoingNeedDataRequests;
    firebolt::rialto::MediaSourceType m_type = firebolt::rialto::MediaSourceType::UNKNOWN;
    int64_t m_position = 0;
    bool m_isFlushing = false;
    ClientState m_state = ClientState::READY;
};

class HaveDataMessage : public Message
{
public:
    HaveDataMessage(firebolt::rialto::MediaSourceStatus status, int sourceId, unsigned int needDataRequestId,
                    GStreamerMSEMediaPlayerClient *player);
    void handle() override;

private:
    firebolt::rialto::MediaSourceStatus m_status;
    int m_sourceId;
    unsigned int m_needDataRequestId;
    GStreamerMSEMediaPlayerClient *m_player;
};

class PullBufferMessage : public Message
{
public:
    PullBufferMessage(int sourceId, size_t frameCount, unsigned int needDataRequestId, GstElement *rialtoSink,
                      const std::shared_ptr<BufferParser> &bufferParser, IMessageQueue &pullerQueue,
                      GStreamerMSEMediaPlayerClient *player, const std::shared_ptr<IPullModePlaybackDelegate> &delegate);
    void handle() override;

private:
    int m_sourceId;
    size_t m_frameCount;
    unsigned int m_needDataRequestId;
    GstElement *m_rialtoSink;
    std::shared_ptr<BufferParser> m_bufferParser;
    IMessageQueue &m_pullerQueue;
    GStreamerMSEMediaPlayerClient *m_player;
    std::shared_ptr<IPullModePlaybackDelegate> m_delegate;
};

class NeedDataMessage : public Message
{
public:
    NeedDataMessage(int sourceId, size_t frameCount, unsigned int needDataRequestId,
                    GStreamerMSEMediaPlayerClient *player);
    void handle() override;

private:
    int m_sourceId;
    size_t m_frameCount;
    unsigned int m_needDataRequestId;
    GStreamerMSEMediaPlayerClient *m_player;
};

class PlaybackStateMessage : public Message
{
public:
    PlaybackStateMessage(firebolt::rialto::PlaybackState state, GStreamerMSEMediaPlayerClient *player);
    void handle() override;

private:
    firebolt::rialto::PlaybackState m_state;
    GStreamerMSEMediaPlayerClient *m_player;
};

class QosMessage : public Message
{
public:
    QosMessage(int sourceId, firebolt::rialto::QosInfo qosInfo, GStreamerMSEMediaPlayerClient *player);
    void handle() override;

private:
    int m_sourceId;
    firebolt::rialto::QosInfo m_qosInfo;
    GStreamerMSEMediaPlayerClient *m_player;
};

class BufferUnderflowMessage : public Message
{
public:
    BufferUnderflowMessage(int sourceId, GStreamerMSEMediaPlayerClient *player);
    void handle() override;

private:
    int m_sourceId;
    GStreamerMSEMediaPlayerClient *m_player;
};

class PlaybackErrorMessage : public Message
{
public:
    PlaybackErrorMessage(int sourceId, firebolt::rialto::PlaybackError error, GStreamerMSEMediaPlayerClient *player);
    void handle() override;

private:
    int m_sourceId;
    firebolt::rialto::PlaybackError m_error;
    GStreamerMSEMediaPlayerClient *m_player;
};

class SetPositionMessage : public Message
{
public:
    SetPositionMessage(int64_t newPosition, std::unordered_map<int32_t, AttachedSource> &attachedSources);
    void handle() override;

private:
    int64_t m_newPosition;
    std::unordered_map<int32_t, AttachedSource> &m_attachedSources;
};

class SetDurationMessage : public Message
{
public:
    SetDurationMessage(int64_t newDuration, int64_t &targetDuration);
    void handle() override;

private:
    int64_t m_newDuration;
    int64_t &m_targetDuration;
};

class SourceFlushedMessage : public Message
{
public:
    SourceFlushedMessage(int32_t sourceId, GStreamerMSEMediaPlayerClient *player);
    void handle() override;

private:
    int32_t m_sourceId;
    GStreamerMSEMediaPlayerClient *m_player;
};

enum class StateChangeResult
{
    SUCCESS_ASYNC,
    SUCCESS_SYNC,
    NOT_ATTACHED
};

class GStreamerMSEMediaPlayerClient : public firebolt::rialto::IMediaPipelineClient,
                                      public std::enable_shared_from_this<GStreamerMSEMediaPlayerClient>
{
    friend class NeedDataMessage;
    friend class PullBufferMessage;
    friend class HaveDataMessage;
    friend class QosMessage;

public:
    GStreamerMSEMediaPlayerClient(
        const std::shared_ptr<IMessageQueueFactory> &messageQueueFactory,
        const std::shared_ptr<firebolt::rialto::client::MediaPlayerClientBackendInterface> &MediaPlayerClientBackend,
        const uint32_t maxVideoWidth, const uint32_t maxVideoHeight);
    virtual ~GStreamerMSEMediaPlayerClient();

    void notifyDuration(int64_t duration) override;
    void notifyPosition(int64_t position) override;
    void notifyNativeSize(uint32_t width, uint32_t height, double aspect) override;
    void notifyNetworkState(firebolt::rialto::NetworkState state) override;
    void notifyPlaybackState(firebolt::rialto::PlaybackState state) override;
    void notifyVideoData(bool hasData) override;
    void notifyAudioData(bool hasData) override;
    void notifyNeedMediaData(int32_t sourceId, size_t frameCount, uint32_t needDataRequestId,
                             const std::shared_ptr<firebolt::rialto::MediaPlayerShmInfo> &shmInfo) override;
    void notifyCancelNeedMediaData(int32_t sourceId) override;
    void notifyQos(int32_t sourceId, const firebolt::rialto::QosInfo &qosInfo) override;
    void notifyBufferUnderflow(int32_t sourceId) override;
    void notifyPlaybackError(int32_t sourceId, firebolt::rialto::PlaybackError error) override;
    void notifySourceFlushed(int32_t sourceId) override;

    void getPositionDo(int64_t *position, int32_t sourceId);
    int64_t getPosition(int32_t sourceId);
    bool setImmediateOutput(int32_t sourceId, bool immediateOutput);
    bool getImmediateOutput(int32_t sourceId, bool &immediateOutput);
    bool getStats(int32_t sourceId, uint64_t &renderedFrames, uint64_t &droppedFrames);

    firebolt::rialto::AddSegmentStatus
    addSegment(unsigned int needDataRequestId,
               const std::unique_ptr<firebolt::rialto::IMediaPipeline::MediaSegment> &mediaSegment);

    bool createBackend();
    StateChangeResult play(int32_t sourceId);
    StateChangeResult pause(int32_t sourceId);
    void stop();
    void setPlaybackRate(double rate);
    void flush(int32_t sourceId, bool resetTime);
    void setSourcePosition(int32_t sourceId, int64_t position, bool resetTime, double appliedRate = 1.0,
                           uint64_t stopPosition = GST_CLOCK_TIME_NONE);
    void setSubtitleOffset(int32_t sourceId, int64_t position);
    void processAudioGap(int64_t position, uint32_t duration, int64_t discontinuityGap, bool audioAac);

    bool attachSource(std::unique_ptr<firebolt::rialto::IMediaPipeline::MediaSource> &source,
                      RialtoMSEBaseSink *rialtoSink, const std::shared_ptr<IPullModePlaybackDelegate> &delegate);
    void removeSource(int32_t sourceId);
    void handlePlaybackStateChange(firebolt::rialto::PlaybackState state);
    void handleSourceFlushed(int32_t sourceId);
    void sendAllSourcesAttachedIfPossible();

    void setVideoRectangle(const std::string &rectangleString);
    std::string getVideoRectangle();

    bool requestPullBuffer(int streamId, size_t frameCount, unsigned int needDataRequestId);
    bool handleQos(int sourceId, firebolt::rialto::QosInfo qosInfo);
    bool handleBufferUnderflow(int sourceId);
    bool handlePlaybackError(int sourceId, firebolt::rialto::PlaybackError error);
    void stopStreaming();
    void destroyClientBackend();
    bool renderFrame(int32_t sourceId);
    void setVolume(double targetVolume, uint32_t volumeDuration, firebolt::rialto::EaseType easeType);
    bool getVolume(double &volume);
    void setMute(bool mute, int32_t sourceId);
    bool getMute(int sourceId);
    void setTextTrackIdentifier(const std::string &textTrackIdentifier);
    std::string getTextTrackIdentifier();
    bool setLowLatency(bool lowLatency);
    bool setSync(bool sync);
    bool getSync(bool &sync);
    bool setSyncOff(bool syncOff);
    bool setStreamSyncMode(int32_t sourceId, int32_t streamSyncMode);
    bool getStreamSyncMode(int32_t &streamSyncMode);
    ClientState getClientState();
    void handleStreamCollection(int32_t audioStreams, int32_t videoStreams, int32_t subtitleStreams);
    void setBufferingLimit(uint32_t limitBufferingMs);
    uint32_t getBufferingLimit();
    void setUseBuffering(bool useBuffering);
    bool getUseBuffering();
    bool switchSource(const std::unique_ptr<firebolt::rialto::IMediaPipeline::MediaSource> &source);

private:
    bool areAllStreamsAttached();
    void sendAllSourcesAttachedIfPossibleInternal();
    bool checkIfAllAttachedSourcesInStates(const std::vector<ClientState> &states);

    std::unique_ptr<IMessageQueue> m_backendQueue;
    std::shared_ptr<IMessageQueueFactory> m_messageQueueFactory;
    std::shared_ptr<firebolt::rialto::client::MediaPlayerClientBackendInterface> m_clientBackend;
    int64_t m_position;
    int64_t m_duration;
    std::mutex m_playerMutex;
    std::unordered_map<int32_t, AttachedSource> m_attachedSources;
    bool m_wasAllSourcesAttachedSent = false;
    int32_t m_audioStreams;
    int32_t m_videoStreams;
    int32_t m_subtitleStreams;

    struct Rectangle
    {
        unsigned int x, y, width, height;
    } m_videoRectangle;

    firebolt::rialto::PlaybackState m_serverPlaybackState = firebolt::rialto::PlaybackState::IDLE;
    ClientState m_clientState = ClientState::IDLE;
    // To check if the backend message queue and pulling of data to serve backend is stopped or not
    bool m_streamingStopped;

    const uint32_t m_maxWidth;
    const uint32_t m_maxHeight;
};
