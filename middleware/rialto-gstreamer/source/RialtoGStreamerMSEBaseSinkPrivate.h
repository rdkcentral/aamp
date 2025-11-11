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

#include <gst/gst.h>

#include <string>

#include "ControlBackendInterface.h"
#include "IPlaybackDelegate.h"
#include "MediaPlayerManager.h"
#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>

enum class PlaybackMode
{
    Pull,
    Push
};

G_BEGIN_DECLS

struct _RialtoMSEBaseSinkPrivate
{
    _RialtoMSEBaseSinkPrivate() = default;
    ~_RialtoMSEBaseSinkPrivate() = default;

    GstPad *m_sinkPad{nullptr};
    std::mutex m_sinkMutex;
    std::shared_ptr<IPlaybackDelegate> m_delegate{nullptr};
    std::map<IPlaybackDelegate::Property, GValue> m_queuedProperties{};
    std::atomic<PlaybackMode> m_playbackMode{PlaybackMode::Pull};
};
G_END_DECLS
