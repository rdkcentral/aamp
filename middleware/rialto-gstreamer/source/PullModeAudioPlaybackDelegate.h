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

#include "PullModePlaybackDelegate.h"

struct AudioFadeConfig
{
    double volume;
    uint32_t duration;
    firebolt::rialto::EaseType easeType;
};

class PullModeAudioPlaybackDelegate : public PullModePlaybackDelegate,
                                      public std::enable_shared_from_this<PullModeAudioPlaybackDelegate>
{
public:
    explicit PullModeAudioPlaybackDelegate(GstElement *sink);
    ~PullModeAudioPlaybackDelegate() override = default;

    GstStateChangeReturn changeState(GstStateChange transition) override;
    gboolean handleEvent(GstPad *pad, GstObject *parent, GstEvent *event) override;
    void getProperty(const Property &type, GValue *value) override;
    void setProperty(const Property &type, const GValue *value) override;
    void handleQos(uint64_t processed, uint64_t dropped) const override;

private:
    std::unique_ptr<firebolt::rialto::IMediaPipeline::MediaSource> createMediaSource(GstCaps *caps) const;

private:
    std::atomic<double> m_targetVolume{kDefaultVolume};
    std::atomic_bool m_mute{kDefaultMute};
    std::atomic_bool m_isVolumeQueued{false};
    std::atomic_bool m_isMuteQueued{false};
    std::atomic_bool m_lowLatency{kDefaultLowLatency};
    std::atomic_bool m_isLowLatencyQueued{false};
    std::atomic_bool m_sync{kDefaultSync};
    std::atomic_bool m_isSyncQueued{false};
    std::atomic_bool m_syncOff{kDefaultSyncOff};
    std::atomic_bool m_isSyncOffQueued{false};
    std::atomic<int32_t> m_streamSyncMode{kDefaultStreamSyncMode};
    std::atomic_bool m_isStreamSyncModeQueued{false};
    AudioFadeConfig m_audioFadeConfig{};
    std::mutex m_audioFadeConfigMutex{};
    std::atomic_bool m_isAudioFadeQueued{false};
    std::atomic<uint32_t> m_bufferingLimit{kDefaultBufferingLimit};
    std::atomic_bool m_isBufferingLimitQueued{false};
    std::atomic_bool m_useBuffering{kDefaultUseBuffering};
    std::atomic_bool m_isUseBufferingQueued{false};
};