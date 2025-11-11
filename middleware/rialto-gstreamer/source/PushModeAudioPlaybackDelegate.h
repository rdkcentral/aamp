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

#include "Constants.h"
#include "ControlBackendInterface.h"
#include "GStreamerWebAudioPlayerClient.h"
#include "IPlaybackDelegate.h"
#include <atomic>
#include <gst/gst.h>
#include <memory>

class PushModeAudioPlaybackDelegate : public IPlaybackDelegate
{
public:
    explicit PushModeAudioPlaybackDelegate(GstElement *sink);
    ~PushModeAudioPlaybackDelegate() override;

    void handleEos() override;
    void handleStateChanged(firebolt::rialto::PlaybackState state) override;
    void handleError(const std::string &message, gint code = 0) override;
    void handleQos(uint64_t processed, uint64_t dropped) const override;
    GstStateChangeReturn changeState(GstStateChange transition) override;
    void postAsyncStart() override;
    void setProperty(const Property &type, const GValue *value) override;
    void getProperty(const Property &type, GValue *value) override;
    std::optional<gboolean> handleQuery(GstQuery *query) const override;
    gboolean handleSendEvent(GstEvent *event) override;
    gboolean handleEvent(GstPad *pad, GstObject *parent, GstEvent *event) override;
    GstFlowReturn handleBuffer(GstBuffer *buffer) override;

private:
    void postAsyncDone();

private:
    GstElement *m_sink;
    std::unique_ptr<firebolt::rialto::client::ControlBackendInterface> m_rialtoControlClient;
    std::shared_ptr<GStreamerWebAudioPlayerClient> m_webAudioClient;
    bool m_isPlayingDelayed{false};
    std::atomic<bool> m_isStateCommitNeeded{false};
    std::atomic<double> m_volume{kDefaultVolume};
    std::atomic<bool> m_isVolumeQueued{false};
};