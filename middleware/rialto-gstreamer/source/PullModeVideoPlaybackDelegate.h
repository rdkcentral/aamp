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

class PullModeVideoPlaybackDelegate : public PullModePlaybackDelegate,
                                      public std::enable_shared_from_this<PullModeVideoPlaybackDelegate>
{
public:
    explicit PullModeVideoPlaybackDelegate(GstElement *sink);
    ~PullModeVideoPlaybackDelegate() override = default;

    GstStateChangeReturn changeState(GstStateChange transition) override;
    gboolean handleEvent(GstPad *pad, GstObject *parent, GstEvent *event) override;
    void getProperty(const Property &type, GValue *value) override;
    void setProperty(const Property &type, const GValue *value) override;
    void handleQos(uint64_t processed, uint64_t dropped) const override;

private:
    std::unique_ptr<firebolt::rialto::IMediaPipeline::MediaSource> createMediaSource(GstCaps *caps) const;

private:
    uint32_t m_maxWidth{0};
    uint32_t m_maxHeight{0};
    bool m_stepOnPrerollEnabled{false};

    std::mutex m_propertyMutex{};
    // START of variables locked by propertyMutex
    // rectangle properties
    std::string m_videoRectangle{"0,0,1920,1080"};
    bool m_rectangleSettingQueued{false};
    // immediate output properties
    bool m_immediateOutput{false};
    bool m_immediateOutputQueued{false};
    bool m_syncmodeStreaming{false};
    bool m_syncmodeStreamingQueued{false};
    bool m_videoMute{false};
    bool m_videoMuteQueued{false};
    // END of variables locked by propertyMutex
};