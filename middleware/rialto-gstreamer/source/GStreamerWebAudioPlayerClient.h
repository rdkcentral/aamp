/*
 * Copyright (C) 2023   
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
#include "IMessageQueue.h"
#include "IPlaybackDelegate.h"
#include "ITimer.h"
#include "MediaCommon.h"
#include "WebAudioClientBackendInterface.h"

#include <gst/app/gstappsink.h>
#include <gst/base/gstbasesink.h>
#include <gst/gst.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

class GStreamerWebAudioPlayerClient : public firebolt::rialto::IWebAudioPlayerClient,
                                      public std::enable_shared_from_this<GStreamerWebAudioPlayerClient>
{
public:
    /**
     * @brief Constructor.
     *
     * @param[in] callbacks : The callbacks for the sink.
     */
    GStreamerWebAudioPlayerClient(
        std::unique_ptr<firebolt::rialto::client::WebAudioClientBackendInterface> &&webAudioClientBackend,
        std::unique_ptr<IMessageQueue> &&backendQueue, IPlaybackDelegate &delegate,
        std::shared_ptr<ITimerFactory> timerFactory);

    /**
     * @brief Destructor.
     */
    virtual ~GStreamerWebAudioPlayerClient();

    /**
     * @brief Extracts the sample info from the gstreamer capabilities
     *        and opens the web audio.
     *
     * @param[in] caps : Sample gstreamer capabilities.
     *
     * @retval true on success.
     */
    bool open(GstCaps *caps);

    /**
     * @brief Closes the web audio player.
     *
     * @retval true on success.
     */
    bool close();

    /**
     * @brief Play the web audio.
     *
     * @retval true on success.
     */
    bool play();

    /**
     * @brief Pause the web audio.
     *
     * @retval true on success.
     */
    bool pause();

    /**
     * @brief Notify EOS.
     *
     * @retval true on success.
     */
    bool setEos();

    /**
     * @brief Get the volume.
     *
     * @param[out] volume : The volume level
     *
     * @retval true on success.
     */
    bool getVolume(double &volume);

    /**
     * @brief Set the volume.
     *
     * @param[in] volume : The volume level
     *
     * @retval true on success.
     */
    bool setVolume(double volume);

    /**
     * @brief Whether the backend has been opened or not.
     *
     * @retval true if open.
     */
    bool isOpen();

    /**
     * @brief Notifies that there is a new sample in gstreamer.
     *
     * @param[in] buf : The new sample buffer.
     *
     * @retval true on success.
     */
    bool notifyNewSample(GstBuffer *buf);

    /**
     * @brief Notify push sample timer expiry.
     */
    void notifyPushSamplesTimerExpired();

    /**
     * @brief Implements the player state change notification.
     *
     * @param[in] state : The new playback state.
     */
    void notifyState(firebolt::rialto::WebAudioPlayerState state) override;

private:
    /**
     * @brief Perform the next push operation.
     *
     * The samples are pushed only when there is the available buffer
     * in RialtoServer.
     */
    void pushSamples();

    /**
     * @brief Checks the config against that previously stored in the object.
     *
     * @retval true if this is a new config.
     */
    bool isNewConfig(const std::string &audioMimeType, std::weak_ptr<const firebolt::rialto::WebAudioConfig> config);

    /**
     * @brief Backend message queue.
     */
    std::unique_ptr<IMessageQueue> m_backendQueue;

    /**
     * @brief The web audio client backend interface.
     */
    std::unique_ptr<firebolt::rialto::client::WebAudioClientBackendInterface> m_clientBackend;

    /**
     * @brief Whether the web audio backend is currently open.
     */
    std::atomic<bool> m_isOpen;

    /**
     * @brief Vector to store the gst sample buffers.
     */
    std::queue<GstBuffer *> m_dataBuffers;

    /**
     * @brief The timer factory.
     */
    std::shared_ptr<ITimerFactory> m_timerFactory;

    /**
     * @brief The push samples timer.
     */
    std::unique_ptr<ITimer> m_pushSamplesTimer;

    /**
     * @brief The preferred number of frames to be written.
     */
    uint32_t m_preferredFrames;

    /**
     * @brief The maximum number of frames that can be written.
     */
    uint32_t m_maximumFrames;

    /**
     * @brief Whether defered play is supported.
     */
    bool m_supportDeferredPlay;

    /**
     * @brief Whether the sink element has received EOS.
     */
    bool m_isEos;

    /**
     * @brief The number of bytes in the frame.
     */
    uint32_t m_frameSize;

    /**
     * @brief The mutex protecting m_dataBuffers size reads
     */
    std::mutex m_queueSizeMutex;

    /**
     * @brief The condition variable used to block webaudio chain function
     *        when m_dataBuffers queue size is too large
     */
    std::condition_variable m_queueSizeCv;

    /**
     * @brief The current web audio player mime type.
     */
    std::string m_mimeType;

    /**
     * @brief The current web audio player config.
     */
    firebolt::rialto::WebAudioConfig m_config;

    /**
     * @brief The reference to delegate object.
     */
    IPlaybackDelegate &m_delegate;
};
