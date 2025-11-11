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

#ifndef FIREBOLT_RIALTO_SERVER_GST_WEB_AUDIO_PLAYER_H_
#define FIREBOLT_RIALTO_SERVER_GST_WEB_AUDIO_PLAYER_H_

#include "IGlibWrapper.h"
#include "IGstDispatcherThread.h"
#include "IGstDispatcherThreadClient.h"
#include "IGstInitialiser.h"
#include "IGstSrc.h"
#include "IGstWebAudioPlayer.h"
#include "IGstWebAudioPlayerPrivate.h"
#include "IGstWrapper.h"
#include "IWorkerThread.h"
#include "WebAudioPlayerContext.h"
#include "tasks/IPlayerTask.h"
#include "tasks/IWebAudioPlayerTaskFactory.h"
#include <memory>
#include <queue>
#include <string>

namespace firebolt::rialto::server
{
/**
 * @brief IGstWebAudioPlayer factory class definition.
 */
class GstWebAudioPlayerFactory : public IGstWebAudioPlayerFactory
{
public:
    /**
     * @brief Weak pointer to the singleton factory object.
     */
    static std::weak_ptr<IGstWebAudioPlayerFactory> m_factory;

    std::unique_ptr<IGstWebAudioPlayer> createGstWebAudioPlayer(IGstWebAudioPlayerClient *client,
                                                                const uint32_t priority) override;
};

/**
 * @brief The definition of the GstWebAudioPlayer.
 */
class GstWebAudioPlayer : public IGstWebAudioPlayer, public IGstWebAudioPlayerPrivate, public IGstDispatcherThreadClient
{
public:
    /**
     * @brief The constructor.
     *
     * @param[in] client                       : The gstreamer player client.
     * @param[in] priority                     : Priority value for this pipeline.
     * @param[in] gstWrapper                   : The gstreamer wrapper.
     * @param[in] glibWrapper                  : The glib wrapper.
     * @param[in] gstInitialiser               : The gst initialiser
     * @param[in] gstSrcFactory                : The gstreamer rialto src factory.
     * @param[in] taskFactory                  : The task factory
     * @param[in] workerThreadFactory          : The worker thread factory
     * @param[in] gstDispatcherThreadFactory   : The gst dispatcher thread factory
     */
    GstWebAudioPlayer(IGstWebAudioPlayerClient *client, const uint32_t priority,
                      const std::shared_ptr<firebolt::rialto::wrappers::IGstWrapper> &gstWrapper,
                      const std::shared_ptr<firebolt::rialto::wrappers::IGlibWrapper> &glibWrapper,
                      const IGstInitialiser &gstInitialiser, const std::shared_ptr<IGstSrcFactory> &gstSrcFactory,
                      std::unique_ptr<IWebAudioPlayerTaskFactory> taskFactory,
                      std::unique_ptr<IWorkerThreadFactory> workerThreadFactory,
                      std::unique_ptr<IGstDispatcherThreadFactory> gstDispatcherThreadFactory);

    /**
     * @brief Virtual destructor.
     */
    virtual ~GstWebAudioPlayer();

    void setCaps(const std::string &audioMimeType, std::weak_ptr<const WebAudioConfig> config) override;
    void play() override;
    void pause() override;
    void setVolume(double volume) override;
    bool getVolume(double &volume) override;
    uint32_t writeBuffer(uint8_t *mainPtr, uint32_t mainLength, uint8_t *wrapPtr, uint32_t wrapLength) override;
    void setEos() override;
    uint64_t getQueuedBytes() override;

    bool changePipelineState(GstState newState) override;
    void stopWorkerThread() override;
    void handleBusMessage(GstMessage *message, bool priority) override;
    void ping(std::unique_ptr<IHeartbeatHandler> &&heartbeatHandler) override;

private:
    /**
     * @brief Initialises the player pipeline for WebAudio playback.
     *
     * @param[in] priority  : Priority value for this pipeline.
     *
     * @retval true on success false otherwise.
     */
    bool initWebAudioPipeline(const uint32_t priority);

    /**
     * @brief Creates a amlhalasink audio sink element and adds it to the pipeline.
     *
     * @retval constructed sink element or nullptr on failure.
     */
    GstElement *createAmlhalaSink();

    /**
     * @brief Creates a rtkaudiosink audio sink element and adds it to the pipeline.
     *
     * @retval constructed sink element or nullptr on failure.
     */
    GstElement *createRtkAudioSink();

    /**
     * @brief Creates a autoaudiosink sink element and adds it to the pipeline.
     *
     * @retval constructed sink element or nullptr on failure.
     */
    GstElement *createAutoSink();

    /**
     * @brief Links the sink, audio convert and audio resample to the src.
     *
     * @param[in] sink  : The constructed sink to link to the src.
     *
     * @retval true on success false otherwise.
     */
    bool linkElementsToSrc(GstElement *sink);

    /**
     * @brief Terminates the player pipeline for WebAudio playback.
     */
    void termWebAudioPipeline();

    /**
     * @brief Shutdown and destroys the worker thread.
     */
    void resetWorkerThread();

private:
    /**
     * @brief The player context.
     */
    WebAudioPlayerContext m_context;

    /**
     * @brief The gstreamer player client.
     */
    IGstWebAudioPlayerClient *m_gstPlayerClient{nullptr};

    /**
     * @brief The gstreamer wrapper object.
     */
    std::shared_ptr<firebolt::rialto::wrappers::IGstWrapper> m_gstWrapper;

    /**
     * @brief The glib wrapper object.
     */
    std::shared_ptr<firebolt::rialto::wrappers::IGlibWrapper> m_glibWrapper;

    /**
     * @brief Thread for handling player tasks.
     */
    std::unique_ptr<IWorkerThread> m_workerThread;

    /**
     * @brief Thread for handling gst bus callbacks
     */
    std::unique_ptr<IGstDispatcherThread> m_gstDispatcherThread;

    /**
     * @brief The GstWebAudioPlayer task factory
     */
    std::unique_ptr<IWebAudioPlayerTaskFactory> m_taskFactory;
};
} // namespace firebolt::rialto::server

#endif // FIREBOLT_RIALTO_SERVER_GST_WEB_AUDIO_PLAYER_H_
