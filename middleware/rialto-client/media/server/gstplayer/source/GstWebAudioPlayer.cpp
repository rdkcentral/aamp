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

#include "GstWebAudioPlayer.h"
#include "GstDispatcherThread.h"
#include "RialtoServerLogging.h"
#include "WorkerThread.h"
#include "tasks/webAudio/WebAudioPlayerTaskFactory.h"
#include <stdexcept>

namespace
{
constexpr uint32_t kMaxWriteBufferTimeoutMs{2000};
} // namespace

namespace firebolt::rialto::server
{
std::weak_ptr<IGstWebAudioPlayerFactory> GstWebAudioPlayerFactory::m_factory;

std::shared_ptr<IGstWebAudioPlayerFactory> IGstWebAudioPlayerFactory::getFactory()
{
    std::shared_ptr<IGstWebAudioPlayerFactory> factory = GstWebAudioPlayerFactory::m_factory.lock();

    if (!factory)
    {
        try
        {
            factory = std::make_shared<GstWebAudioPlayerFactory>();
        }
        catch (const std::exception &e)
        {
            RIALTO_SERVER_LOG_ERROR("Failed to create the gstreamer player factory, reason: %s", e.what());
        }

        GstWebAudioPlayerFactory::m_factory = factory;
    }

    return factory;
}

std::unique_ptr<IGstWebAudioPlayer> GstWebAudioPlayerFactory::createGstWebAudioPlayer(IGstWebAudioPlayerClient *client,
                                                                                      const uint32_t priority)
{
    std::unique_ptr<IGstWebAudioPlayer> gstPlayer;

    try
    {
        auto gstWrapperFactory = firebolt::rialto::wrappers::IGstWrapperFactory::getFactory();
        auto glibWrapperFactory = firebolt::rialto::wrappers::IGlibWrapperFactory::getFactory();
        std::shared_ptr<firebolt::rialto::wrappers::IGstWrapper> gstWrapper;
        std::shared_ptr<firebolt::rialto::wrappers::IGlibWrapper> glibWrapper;
        if ((!gstWrapperFactory) || (!(gstWrapper = gstWrapperFactory->getGstWrapper())))
        {
            throw std::runtime_error("Cannot create GstWrapper");
        }
        if ((!glibWrapperFactory) || (!(glibWrapper = glibWrapperFactory->getGlibWrapper())))
        {
            throw std::runtime_error("Cannot create GlibWrapper");
        }
        gstPlayer = std::make_unique<GstWebAudioPlayer>(client, priority, gstWrapper, glibWrapper,
                                                        IGstInitialiser::instance(), IGstSrcFactory::getFactory(),
                                                        std::make_unique<WebAudioPlayerTaskFactory>(client, gstWrapper,
                                                                                                    glibWrapper),
                                                        std::make_unique<WorkerThreadFactory>(),
                                                        std::make_unique<GstDispatcherThreadFactory>());
    }
    catch (const std::exception &e)
    {
        RIALTO_SERVER_LOG_ERROR("Failed to create the gstreamer player, reason: %s", e.what());
    }

    return gstPlayer;
}

GstWebAudioPlayer::GstWebAudioPlayer(IGstWebAudioPlayerClient *client, const uint32_t priority,
                                     const std::shared_ptr<firebolt::rialto::wrappers::IGstWrapper> &gstWrapper,
                                     const std::shared_ptr<firebolt::rialto::wrappers::IGlibWrapper> &glibWrapper,
                                     const IGstInitialiser &gstInitialiser,
                                     const std::shared_ptr<IGstSrcFactory> &gstSrcFactory,
                                     std::unique_ptr<IWebAudioPlayerTaskFactory> taskFactory,
                                     std::unique_ptr<IWorkerThreadFactory> workerThreadFactory,
                                     std::unique_ptr<IGstDispatcherThreadFactory> gstDispatcherThreadFactory)
    : m_gstPlayerClient(client), m_gstWrapper{gstWrapper}, m_glibWrapper{glibWrapper},
      m_taskFactory{std::move(taskFactory)}
{
    RIALTO_SERVER_LOG_DEBUG("GstWebAudioPlayer is constructed.");

    gstInitialiser.waitForInitialisation();

    if ((!gstSrcFactory) || (!(m_context.gstSrc = gstSrcFactory->getGstSrc())))
    {
        throw std::runtime_error("Cannot create GstSrc");
    }

    // Ensure that rialtosrc has been initalised
    m_context.gstSrc->initSrc();

    // Start task thread
    if ((!workerThreadFactory) || (!(m_workerThread = workerThreadFactory->createWorkerThread())))
    {
        throw std::runtime_error("Failed to create the worker thread");
    }

    if (!initWebAudioPipeline(priority))
    {
        termWebAudioPipeline();
        resetWorkerThread();
        throw std::runtime_error("Failed to initalise the pipeline");
    }

    if ((!gstDispatcherThreadFactory) ||
        (!(m_gstDispatcherThread =
               gstDispatcherThreadFactory->createGstDispatcherThread(*this, m_context.pipeline, m_gstWrapper))))
    {
        termWebAudioPipeline();
        resetWorkerThread();
        throw std::runtime_error("Failed to create the dispatcher thread");
    }
}

GstWebAudioPlayer::~GstWebAudioPlayer()
{
    RIALTO_SERVER_LOG_DEBUG("GstWebAudioPlayer is destructed.");

    m_gstDispatcherThread.reset();

    resetWorkerThread();

    termWebAudioPipeline();
}

bool GstWebAudioPlayer::initWebAudioPipeline(const uint32_t priority)
{
    m_context.pipeline = m_gstWrapper->gstPipelineNew(std::string("webaudiopipeline" + std::to_string(priority)).c_str());
    if (!m_context.pipeline)
    {
        RIALTO_SERVER_LOG_ERROR("Failed to create the webaudiopipeline");
        return false;
    }

    RIALTO_SERVER_LOG_MIL("RialtoServer's webaudio pipeline constructed");

    // Create and initalise appsrc
    m_context.source = m_gstWrapper->gstElementFactoryMake("appsrc", "audsrc");
    if (!m_context.source)
    {
        RIALTO_SERVER_LOG_ERROR("Failed to create the appsrc");
        return false;
    }
    m_gstWrapper->gstAppSrcSetMaxBytes(GST_APP_SRC(m_context.source), kMaxWebAudioBytes);
    m_glibWrapper->gObjectSet(m_context.source, "format", GST_FORMAT_TIME, nullptr);

    // Perform sink specific initalisation
    GstPluginFeature *feature = nullptr;
    GstRegistry *reg = m_gstWrapper->gstRegistryGet();
    if (!reg)
    {
        RIALTO_SERVER_LOG_ERROR("Failed get the gst registry");
        return false;
    }

    GstElement *sink = nullptr;
    if (nullptr != (feature = m_gstWrapper->gstRegistryLookupFeature(reg, "amlhalasink")))
    {
        // LLama
        RIALTO_SERVER_LOG_INFO("Use amlhalasink");
        sink = createAmlhalaSink();
        m_gstWrapper->gstObjectUnref(feature);
    }
    else if (nullptr != (feature = m_gstWrapper->gstRegistryLookupFeature(reg, "rtkaudiosink")))
    {
        // XiOne
        RIALTO_SERVER_LOG_INFO("Use rtkaudiosink");
        sink = createRtkAudioSink();
        m_gstWrapper->gstObjectUnref(feature);
    }
    else
    {
        RIALTO_SERVER_LOG_INFO("Use autoaudiosink");
        sink = createAutoSink();
    }

    if (sink)
    {
        return linkElementsToSrc(sink);
    }
    else
    {
        m_gstWrapper->gstObjectUnref(m_context.source);
        m_context.source = nullptr;
    }
    return false;
}

GstElement *GstWebAudioPlayer::createAmlhalaSink()
{
    GstElement *sink = m_gstWrapper->gstElementFactoryMake("amlhalasink", "webaudiosink");
    if (!sink)
    {
        RIALTO_SERVER_LOG_ERROR("Failed create the amlhalasink");
        return nullptr;
    }
    m_glibWrapper->gObjectSet(G_OBJECT(sink), "direct-mode", FALSE, NULL);

    return sink;
}

GstElement *GstWebAudioPlayer::createRtkAudioSink()
{
    GstElement *sink = m_gstWrapper->gstElementFactoryMake("rtkaudiosink", "webaudiosink");
    if (!sink)
    {
        RIALTO_SERVER_LOG_ERROR("Failed create the rtkaudiosink");
        return nullptr;
    }
    m_glibWrapper->gObjectSet(G_OBJECT(sink), "media-tunnel", FALSE, NULL);
    m_glibWrapper->gObjectSet(G_OBJECT(sink), "audio-service", TRUE, NULL);

    return sink;
}

GstElement *GstWebAudioPlayer::createAutoSink()
{
    GstElement *sink = m_gstWrapper->gstElementFactoryMake("autoaudiosink", "webaudiosink");
    if (!sink)
    {
        RIALTO_SERVER_LOG_ERROR("Failed create the autoaudiosink");
        return nullptr;
    }

    return sink;
}

// NOTE:-
//   This method hands the responsibility for the destruction of both { "sink", "m_context.source" }
//   over to the pipeline (or if handover wasn't possible, it will unref)
bool GstWebAudioPlayer::linkElementsToSrc(GstElement *sink)
{
    bool status{true};

    GstElement *convert{nullptr};
    GstElement *resample{nullptr};
    GstElement *volume{nullptr};
    GstElement *queue{nullptr};

    convert = m_gstWrapper->gstElementFactoryMake("audioconvert", NULL);
    if (!convert)
    {
        RIALTO_SERVER_LOG_ERROR("Failed create the audioconvert");
        status = false;
    }

    if (status)
    {
        resample = m_gstWrapper->gstElementFactoryMake("audioresample", NULL);
        if (!resample)
        {
            RIALTO_SERVER_LOG_ERROR("Failed create the audioresample");
            status = false;
        }
    }

    if (status)
    {
        volume = m_gstWrapper->gstElementFactoryMake("volume", NULL);
        if (!volume)
        {
            RIALTO_SERVER_LOG_ERROR("Failed create the volume");
            status = false;
        }
    }
    if (status)
    {
        queue = m_gstWrapper->gstElementFactoryMake("queue", NULL);
        if (!queue)
        {
            RIALTO_SERVER_LOG_ERROR("Failed create the queue");
            status = false;
        }
        else
        {
            constexpr guint kWebAudioQueueSize{8192};
            m_glibWrapper->gObjectSet(queue, "max-size-bytes", kWebAudioQueueSize, nullptr);
        }
    }

    std::queue<GstElement *> elementsToAdd;
    elementsToAdd.push(m_context.source);
    if (convert)
        elementsToAdd.push(convert);
    if (resample)
        elementsToAdd.push(resample);
    if (volume)
        elementsToAdd.push(volume);
    if (queue)
        elementsToAdd.push(queue);
    elementsToAdd.push(sink);

    if (status)
    {
        // Add elements to the pipeline
        GstBin *pipelineBin = GST_BIN(m_context.pipeline);
        while (!elementsToAdd.empty())
        {
            if (m_gstWrapper->gstBinAdd(pipelineBin, elementsToAdd.front()))
                elementsToAdd.pop();
            else
            {
                RIALTO_SERVER_LOG_ERROR("Failed to add element to the bin");
                status = false;
                break;
            }
        }
    }

    if (status)
    {
        if ((!m_gstWrapper->gstElementLink(m_context.source, convert)) ||
            (!m_gstWrapper->gstElementLink(convert, resample)) || (!m_gstWrapper->gstElementLink(resample, volume)) ||
            (!m_gstWrapper->gstElementLink(volume, queue)) || (!m_gstWrapper->gstElementLink(queue, sink)))
        {
            RIALTO_SERVER_LOG_ERROR("Failed to link elements");
            status = false;
        }
        m_context.gstVolumeElement = GST_STREAM_VOLUME(volume);
    }

    if (!status)
    {
        // Unref anything that wasn't added to the pipeline
        while (!elementsToAdd.empty())
        {
            m_gstWrapper->gstObjectUnref(elementsToAdd.front());
            elementsToAdd.pop();
        }
        m_context.source = nullptr;
    }

    return status;
}

void GstWebAudioPlayer::termWebAudioPipeline()
{
    if (m_context.pipeline)
    {
        m_taskFactory->createStop(*this)->execute();
        GstBus *bus = m_gstWrapper->gstPipelineGetBus(GST_PIPELINE(m_context.pipeline));
        if (bus)
        {
            m_gstWrapper->gstBusSetSyncHandler(bus, nullptr, nullptr, nullptr);
            m_gstWrapper->gstObjectUnref(bus);
        }

        m_gstWrapper->gstObjectUnref(m_context.pipeline);
    }
    RIALTO_SERVER_LOG_MIL("RialtoServer's webaudio pipeline terminated.");
}

void GstWebAudioPlayer::resetWorkerThread()
{
    m_workerThread->enqueueTask(m_taskFactory->createShutdown(*this));
    m_workerThread->join();
    m_workerThread.reset();
}

void GstWebAudioPlayer::setCaps(const std::string &audioMimeType, std::weak_ptr<const WebAudioConfig> config)
{
    m_workerThread->enqueueTask(m_taskFactory->createSetCaps(m_context, audioMimeType, config));
}

void GstWebAudioPlayer::play()
{
    m_workerThread->enqueueTask(m_taskFactory->createPlay(*this));
}

void GstWebAudioPlayer::pause()
{
    m_workerThread->enqueueTask(m_taskFactory->createPause(*this));
}

void GstWebAudioPlayer::setVolume(double volume)
{
    m_workerThread->enqueueTask(m_taskFactory->createSetVolume(m_context, volume));
}

bool GstWebAudioPlayer::getVolume(double &volume)
{
    // Must be called on the main thread, otherwise the pipeline can be destroyed during the query.
    volume = m_gstWrapper->gstStreamVolumeGetVolume(m_context.gstVolumeElement, GST_STREAM_VOLUME_FORMAT_LINEAR);
    return true;
}

uint32_t GstWebAudioPlayer::writeBuffer(uint8_t *mainPtr, uint32_t mainLength, uint8_t *wrapPtr, uint32_t wrapLength)
{
    // Must block and wait for the data to be written from the shared buffer.
    std::unique_lock<std::mutex> lock(m_context.writeBufferMutex);
    m_workerThread->enqueueTask(m_taskFactory->createWriteBuffer(m_context, mainPtr, mainLength, wrapPtr, wrapLength));
    std::cv_status status = m_context.writeBufferCond.wait_for(lock, std::chrono::milliseconds(kMaxWriteBufferTimeoutMs));
    if (std::cv_status::timeout == status)
    {
        RIALTO_SERVER_LOG_ERROR("Timed out writing to the gstreamer buffers");
        return 0;
    }
    else
    {
        return m_context.lastBytesWritten;
    }
}

void GstWebAudioPlayer::setEos()
{
    m_workerThread->enqueueTask(m_taskFactory->createEos(m_context));
}

uint64_t GstWebAudioPlayer::getQueuedBytes()
{
    // Must be called on the main thread, otherwise the pipeline can be destroyed during the query.
    return m_gstWrapper->gstAppSrcGetCurrentLevelBytes(GST_APP_SRC(m_context.source));
}

bool GstWebAudioPlayer::changePipelineState(GstState newState)
{
    if (m_gstWrapper->gstElementSetState(m_context.pipeline, newState) == GST_STATE_CHANGE_FAILURE)
    {
        RIALTO_SERVER_LOG_ERROR("Change state failed - Gstreamer returned an error");
        if (m_gstPlayerClient)
            m_gstPlayerClient->notifyState(WebAudioPlayerState::FAILURE);
        return false;
    }
    return true;
}

void GstWebAudioPlayer::stopWorkerThread()
{
    if (m_workerThread)
    {
        m_workerThread->stop();
    }
}

void GstWebAudioPlayer::handleBusMessage(GstMessage *message, bool priority)
{
    if (m_workerThread)
    {
        m_workerThread->enqueueTask(m_taskFactory->createHandleBusMessage(m_context, *this, message));
    }
}

void GstWebAudioPlayer::ping(std::unique_ptr<IHeartbeatHandler> &&heartbeatHandler)
{
    if (m_workerThread)
    {
        m_workerThread->enqueueTask(m_taskFactory->createPing(std::move(heartbeatHandler)));
    }
}

}; // namespace firebolt::rialto::server
