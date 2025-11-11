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

#ifndef FIREBOLT_RIALTO_SERVER_WEB_AUDIO_PLAYER_SERVER_INTERNAL_H_
#define FIREBOLT_RIALTO_SERVER_WEB_AUDIO_PLAYER_SERVER_INTERNAL_H_

#include "IGstWebAudioPlayer.h"
#include "IMainThread.h"
#include "ITimer.h"
#include "IWebAudioPlayer.h"
#include "IWebAudioPlayerServerInternal.h"

#include <memory>
#include <stdint.h>
#include <string>

namespace firebolt::rialto::server
{
/**
 * @brief IWebAudioPlayer factory class definition.
 */
class WebAudioPlayerServerInternalFactory : public IWebAudioPlayerServerInternalFactory
{
public:
    WebAudioPlayerServerInternalFactory() = default;
    ~WebAudioPlayerServerInternalFactory() override = default;

    std::unique_ptr<IWebAudioPlayer> createWebAudioPlayer(std::weak_ptr<IWebAudioPlayerClient> client,
                                                          const std::string &audioMimeType, const uint32_t priority,
                                                          std::weak_ptr<const WebAudioConfig> config) const override;

    std::unique_ptr<IWebAudioPlayerServerInternal> createWebAudioPlayerServerInternal(
        std::weak_ptr<IWebAudioPlayerClient> client, const std::string &audioMimeType, const uint32_t priority,
        std::weak_ptr<const WebAudioConfig> config, const std::shared_ptr<ISharedMemoryBuffer> &shmBuffer, int handle,
        const std::shared_ptr<firebolt::rialto::server::IMainThreadFactory> &mainThreadFactory,
        const std::shared_ptr<firebolt::rialto::server::IGstWebAudioPlayerFactory> &gstPlayerFactory,
        std::weak_ptr<firebolt::rialto::common::ITimerFactory> timerFactory) const override;
};

/**
 * @brief The definition of the WebAudioPlayerServerInternal.
 */
class WebAudioPlayerServerInternal : public IWebAudioPlayerServerInternal, public IGstWebAudioPlayerClient
{
public:
    /**
     * @brief The constructor.
     *
     * @param[in] client            : The Web Audio Player client.
     * @param[in] audioMimeType     : The audio encoding format, currently only "audio/x-raw" (PCM).
     * @param[in] priority          : Priority value for this pipeline.
     * @param[in] config            : Additional type dependent configuration data or nullptr.
     * @param[in] shmBuffer         : The shared memory buffer.
     * @param[in] handle            : The handle for this WebAudioPlayer.
     * @param[in] mainThreadFactory : The main thread factory.
     * @param[in] gstPlayerFactory  : The gstreamer player factory.
     * @param[in] timerFactory      : The timer factory.
     */
    WebAudioPlayerServerInternal(std::weak_ptr<IWebAudioPlayerClient> client, const std::string &audioMimeType,
                                 const uint32_t priority, std::weak_ptr<const WebAudioConfig> config,
                                 const std::shared_ptr<ISharedMemoryBuffer> &shmBuffer, int handle,
                                 const std::shared_ptr<IMainThreadFactory> &mainThreadFactory,
                                 const std::shared_ptr<IGstWebAudioPlayerFactory> &gstPlayerFactory,
                                 std::weak_ptr<common::ITimerFactory> timerFactory);

    /**
     * @brief Virtual destructor.
     */
    virtual ~WebAudioPlayerServerInternal();

    bool play() override;

    bool pause() override;

    bool setEos() override;

    bool getBufferAvailable(uint32_t &availableFrames, std::shared_ptr<WebAudioShmInfo> &webAudioShmInfo) override;

    bool getBufferDelay(uint32_t &delayFrames) override;

    bool writeBuffer(const uint32_t numberOfFrames, void *data) override;

    bool getDeviceInfo(uint32_t &preferredFrames, uint32_t &maximumFrames, bool &supportDeferredPlay) override;

    bool setVolume(double volume) override;

    bool getVolume(double &volume) override;

    std::weak_ptr<IWebAudioPlayerClient> getClient() override;

    void notifyState(WebAudioPlayerState state) override;

    void ping(std::unique_ptr<IHeartbeatHandler> &&heartbeatHandler) override;

protected:
    /**
     * @brief The web audio player client.
     */
    std::shared_ptr<IWebAudioPlayerClient> m_webAudioPlayerClient;

    /**
     * @brief Shared memory buffer.
     */
    std::shared_ptr<ISharedMemoryBuffer> m_shmBuffer;

    /**
     * @brief The mainThread object.
     */
    std::shared_ptr<IMainThread> m_mainThread;

    /**
     * @brief This objects id registered on the main thread.
     */
    uint32_t m_mainThreadClientId;

    /**
     * @brief This priority of the WebAudioPlayer object.
     */
    const uint32_t m_priority;

    /**
     * @brief The gstreamer player.
     */
    std::unique_ptr<IGstWebAudioPlayer> m_gstPlayer;

    /**
     * @brief The id of the shared memory partition.
     */
    const int m_shmId;

    /**
     * @brief Pointer to the start of the shared buffer.
     */
    uint8_t *m_shmPtr;

    /**
     * @brief Offset of the web audio partition relative to the start of the shared memory.
     */
    uint32_t m_partitionOffset;

    /**
     * @brief Length of the shared buffer partition.
     */
    uint32_t m_maxDataLength;

    /**
     * @brief The details of the free space in the shared buffer partition.
     */
    WebAudioShmInfo m_availableBuffer;

    /**
     * @brief True if a writeBuffer call is expected.
     */
    bool m_expectWriteBuffer;

    /**
     * @brief Factory for creating timers.
     */
    std::shared_ptr<common::ITimerFactory> m_timerFactory;

    /**
     * @brief Timer set to write data to gstreamer.
     */
    std::unique_ptr<firebolt::rialto::common::ITimer> m_writeDataTimer;

    /**
     * @brief The bytes per frame for this audio playback.
     */
    uint32_t m_bytesPerFrame;

    /**
     * @brief Whether EOS has been requested at the end of the buffer.
     */
    bool m_isEosRequested;

    /**
     * @brief Initalises the WebAudioPlayer.
     *
     * @param[in] audioMimeType     : The audio encoding format.
     * @param[in] config            : Additional type dependent configuration data or nullptr.
     * @param[in] gstPlayerFactory  : The gstreamer player factory.
     *
     * @retval true on success.
     */
    bool initWebAudioPlayerInternal(const std::string &audioMimeType, std::weak_ptr<const WebAudioConfig> config,
                                    const std::shared_ptr<IGstWebAudioPlayerFactory> &gstPlayerFactory);

    /**
     * @brief Initalises the GstWebAudioPlayer.
     *
     * @param[in] audioMimeType     : The audio encoding format.
     * @param[in] config            : Additional type dependent configuration data or nullptr.
     * @param[in] gstPlayerFactory  : The gstreamer player factory.
     *
     * @retval true on success.
     */
    bool initGstWebAudioPlayer(const std::string &audioMimeType, std::weak_ptr<const WebAudioConfig> config,
                               const std::shared_ptr<IGstWebAudioPlayerFactory> &gstPlayerFactory);

    /**
     * @brief Write audio frames internally, only to be called on the main thread.
     *
     * @param[in]  numberOfFrames : Number of frames of audio in the shared memory.
     *
     * @retval true on success.
     */
    bool writeBufferInternal(const uint32_t numberOfFrames);

    /**
     * @brief Write the data that has been stored in the shared memory to gstreamer.
     *
     * Uses the available buffer variable to calculate the data to pass to gstreamer for writting.
     *
     * @retval true if all stored data was written to gstreamer.
     */
    bool writeStoredBuffers();

    /**
     * @brief Update the available buffer variable with the new bytes written to the shared memory and gstreamer.
     *
     * @param[in]  bytesWrittenToShm : Number of bytes newly written to the shared memory.
     * @param[in]  bytesWrittenToGst : Number of bytes newly written to gstreamer.
     */
    void updateAvailableBuffer(uint32_t bytesWrittenToShm, uint32_t bytesWrittenToGst);

    /**
     * @brief Handles the timeout of the write data timer.
     */
    void handleWriteDataTimer();

    /**
     * @brief Gets the number of queued frames in the shared memory.
     *
     * @retval The number of frames queued.
     */
    uint32_t getQueuedFramesInShm();
};

}; // namespace firebolt::rialto::server

#endif // FIREBOLT_RIALTO_SERVER_WEB_AUDIO_PLAYER_SERVER_INTERNAL_H_
