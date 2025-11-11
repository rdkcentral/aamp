/*
 * Copyright (C) 2025   
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

#include "IPullModePlaybackDelegate.h"
#include <gst/gst.h>

#include <string>

#include "ControlBackendInterface.h"
#include "MediaPlayerManager.h"
#include <atomic>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>

class PullModePlaybackDelegate : public IPullModePlaybackDelegate
{
protected:
    explicit PullModePlaybackDelegate(GstElement *sink);
    ~PullModePlaybackDelegate() override;

public:
    void setSourceId(int32_t sourceId) override;

    void handleEos() override;
    void handleFlushCompleted() override;
    void handleStateChanged(firebolt::rialto::PlaybackState state) override;
    void handleError(const std::string &message, gint code = 0) override;
    GstStateChangeReturn changeState(GstStateChange transition) override;
    void postAsyncStart() override;
    void setProperty(const Property &type, const GValue *value) override;
    void getProperty(const Property &type, GValue *value) override;
    std::optional<gboolean> handleQuery(GstQuery *query) const override;
    gboolean handleSendEvent(GstEvent *event) override;
    gboolean handleEvent(GstPad *pad, GstObject *parent, GstEvent *event) override;
    GstFlowReturn handleBuffer(GstBuffer *buffer) override;
    GstRefSample getFrontSample() const override;
    void popSample() override;
    bool isEos() const override;
    void lostState() override;
    bool isReadyToSendData() const override;

protected:
    bool attachToMediaClientAndSetStreamsNumber(const uint32_t maxVideoWidth = 0, const uint32_t maxVideoHeight = 0);

private:
    void clearBuffersUnlocked();
    void postAsyncDone();
    void copySegment(GstEvent *event);
    void setSegment();
    void changePlaybackRate(GstEvent *event);
    void startFlushing();
    void stopFlushing(bool resetTime);
    void flushServer(bool resetTime);
    bool setStreamsNumber(GstObject *parentObject);

protected:
    GstElement *m_sink{nullptr};
    GstPad *m_sinkPad{nullptr};
    GstSegment m_lastSegment{};
    GstCaps *m_caps{nullptr};

    std::atomic<int32_t> m_sourceId{-1};
    std::queue<GstSample *> m_samples{};
    bool m_isEos{false};
    std::atomic<bool> m_segmentSet{false};
    std::atomic<bool> m_isFlushOngoing{false};
    std::atomic<bool> m_isStateCommitNeeded{false};
    mutable std::mutex m_sinkMutex{};

    std::condition_variable m_needDataCondVariable{};
    std::condition_variable m_flushCondVariable{};
    std::mutex m_flushMutex{};

    MediaPlayerManager m_mediaPlayerManager{};
    std::unique_ptr<firebolt::rialto::client::ControlBackendInterface> m_rialtoControlClient{};
    std::atomic<bool> m_sourceAttached{false};
    bool m_isSinglePathStream{false};
    int32_t m_numOfStreams{1};
    std::atomic<bool> m_hasDrm{true};
    std::atomic<bool> m_isAsync{false};
    firebolt::rialto::PlaybackState m_serverPlaybackState{firebolt::rialto::PlaybackState::UNKNOWN};
    firebolt::rialto::MediaSourceType m_mediaSourceType{firebolt::rialto::MediaSourceType::UNKNOWN};
    guint32 m_lastInstantRateChangeSeqnum{GST_SEQNUM_INVALID};
    std::atomic<guint32> m_currentInstantRateChangeSeqnum{GST_SEQNUM_INVALID};
};