/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2023 
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

#ifndef FIREBOLT_RIALTO_SERVER_WEB_AUDIO_PLAYER_CONTEXT_H_
#define FIREBOLT_RIALTO_SERVER_WEB_AUDIO_PLAYER_CONTEXT_H_

#include <gst/gst.h>
#include <gst/pbutils/pbutils.h>

#include <condition_variable>
#include <list>
#include <memory>
#include <mutex>

#include "IGstSrc.h"

namespace firebolt::rialto::server
{
constexpr uint32_t kMaxWebAudioBytes{10 * 1024};

struct WebAudioPlayerContext
{
    /**
     * @brief The rialto src object.
     */
    std::shared_ptr<IGstSrc> gstSrc{nullptr};

    /**
     * @brief The gstreamer pipeline.
     */
    GstElement *pipeline{nullptr};

    /**
     * @brief The gstreamer audio source.
     */
    GstElement *source{nullptr};

    /**
     * @brief Write buffer mutex.
     */
    std::mutex writeBufferMutex;

    /**
     * @brief Write buffer condition variable.
     */
    std::condition_variable writeBufferCond;

    /**
     * @brief The previous number of bytes written to the gstreamer buffers.
     */
    uint32_t lastBytesWritten{};

    /**
     * @brief The number of bytes per sample.
     */
    uint32_t bytesPerSample{};

    /**
     * @brief Pointer to the gstreamer volume element
     */
    GstStreamVolume *gstVolumeElement{nullptr};
};
} // namespace firebolt::rialto::server

#endif // FIREBOLT_RIALTO_SERVER_WEB_AUDIO_PLAYER_CONTEXT_H_
