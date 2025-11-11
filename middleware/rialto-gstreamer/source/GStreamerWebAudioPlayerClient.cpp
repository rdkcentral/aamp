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

#include "GStreamerWebAudioPlayerClient.h"
#include "GstreamerCatLog.h"

#include <string.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <thread>

#define GST_CAT_DEFAULT rialtoGStreamerCat

namespace
{
constexpr std::size_t kMaxQueueSize{40};
bool parseGstStructureFormat(const std::string &format, uint32_t &sampleSize, bool &isBigEndian, bool &isSigned,
                             bool &isFloat)
{
    if (format.size() != 5)
    {
        return false;
    }
    std::string sampleSizeStr = format.substr(1, 2);
    char *pEnd = NULL;
    errno = 0;
    sampleSize = strtoul(sampleSizeStr.c_str(), &pEnd, 10);
    if (errno == ERANGE)
    {
        return false;
    }

    isBigEndian = format.substr(3) == "BE";

    switch (format[0])
    {
    case 'S':
        isSigned = true;
        isFloat = false;
        break;
    case 'U':
        isSigned = false;
        isFloat = false;
        break;
    case 'F':
        isSigned = false;
        isFloat = true;
        break;
    default:
        return false;
        break;
    }
    return true;
}

bool operator!=(const firebolt::rialto::WebAudioPcmConfig &lac, const firebolt::rialto::WebAudioPcmConfig &rac)
{
    return lac.rate != rac.rate || lac.channels != rac.channels || lac.sampleSize != rac.sampleSize ||
           lac.isBigEndian != rac.isBigEndian || lac.isSigned != rac.isSigned || lac.isFloat != rac.isFloat;
}
} // namespace

GStreamerWebAudioPlayerClient::GStreamerWebAudioPlayerClient(
    std::unique_ptr<firebolt::rialto::client::WebAudioClientBackendInterface> &&webAudioClientBackend,
    std::unique_ptr<IMessageQueue> &&backendQueue, IPlaybackDelegate &delegate,
    std::shared_ptr<ITimerFactory> timerFactory)
    : m_backendQueue{std::move(backendQueue)}, m_clientBackend{std::move(webAudioClientBackend)}, m_isOpen{false},
      m_dataBuffers{}, m_timerFactory{timerFactory}, m_pushSamplesTimer{nullptr}, m_preferredFrames{0},
      m_maximumFrames{0}, m_supportDeferredPlay{false}, m_isEos{false}, m_frameSize{0}, m_mimeType{}, m_config{{}},
      m_delegate{delegate}
{
    m_backendQueue->start();
}

GStreamerWebAudioPlayerClient::~GStreamerWebAudioPlayerClient()
{
    m_backendQueue->stop();
}

bool GStreamerWebAudioPlayerClient::open(GstCaps *caps)
{
    GST_DEBUG("entry:");

    bool result = false;
    GstStructure *structure = gst_caps_get_structure(caps, 0);
    std::string audioMimeType = gst_structure_get_name(structure);
    const auto spacePosition = audioMimeType.find(' ');
    if (spacePosition != std::string::npos)
    {
        audioMimeType.resize(spacePosition);
    }
    const gchar *formatCStr{gst_structure_get_string(structure, "format")};
    std::string format{formatCStr ? formatCStr : ""};
    firebolt::rialto::WebAudioPcmConfig pcm;
    gint tmp;

    if (format.empty())
    {
        GST_ERROR("Format not found in caps");
        return result;
    }

    if (!gst_structure_get_int(structure, "rate", &tmp))
    {
        GST_ERROR("Rate not found in caps");
        return result;
    }
    pcm.rate = tmp;

    if (!gst_structure_get_int(structure, "channels", &tmp))
    {
        GST_ERROR("Rate not found in caps");
        return result;
    }
    pcm.channels = tmp;

    if (!parseGstStructureFormat(format, pcm.sampleSize, pcm.isBigEndian, pcm.isSigned, pcm.isFloat))
    {
        GST_ERROR("Can't parse format or it is not supported: %s", format.c_str());
        return result;
    }

    m_backendQueue->callInEventLoop(
        [&]()
        {
            // "configWorkaround" is used because there doesn't seem to be an easy way to use
            // make_shared in conjunction with an initalizer list, and adding a constructor to
            // WebAudioConfig stops initalizer lists (used elsewhere in the code) from working...
            firebolt::rialto::WebAudioConfig configWorkaround{pcm};
            std::shared_ptr<firebolt::rialto::WebAudioConfig> config =
                std::make_shared<firebolt::rialto::WebAudioConfig>(configWorkaround);

            // Only recreate player if the config has changed
            if (!m_isOpen || isNewConfig(audioMimeType, config))
            {
                if (m_isOpen)
                {
                    // Destroy the previously created player
                    m_clientBackend->destroyWebAudioBackend();
                }

                uint32_t priority = 1;
                if (m_clientBackend->createWebAudioBackend(shared_from_this(), audioMimeType, priority, config))
                {
                    if (!m_clientBackend->getDeviceInfo(m_preferredFrames, m_maximumFrames, m_supportDeferredPlay))
                    {
                        GST_ERROR("GetDeviceInfo failed, could not process samples");
                    }
                    m_frameSize = (pcm.sampleSize * pcm.channels) / CHAR_BIT;
                    m_isOpen = true;

                    // Store config
                    m_config.pcm = pcm;
                    m_mimeType = audioMimeType;
                }
                else
                {
                    GST_ERROR("Could not create web audio backend");
                    m_isOpen = false;
                }
                result = m_isOpen;
            }
        });

    return result;
}

bool GStreamerWebAudioPlayerClient::close()
{
    GST_DEBUG("entry:");

    m_backendQueue->callInEventLoop(
        [&]()
        {
            m_clientBackend->destroyWebAudioBackend();
            m_pushSamplesTimer.reset();
            m_isOpen = false;
        });

    return true;
}

bool GStreamerWebAudioPlayerClient::play()
{
    GST_DEBUG("entry:");

    bool result = false;
    m_backendQueue->callInEventLoop(
        [&]()
        {
            if (m_isOpen)
            {
                result = m_clientBackend->play();
            }
            else
            {
                GST_ERROR("No web audio backend");
            }
        });

    return result;
}

bool GStreamerWebAudioPlayerClient::pause()
{
    GST_DEBUG("entry:");

    bool result = false;
    m_backendQueue->callInEventLoop(
        [&]()
        {
            if (m_isOpen)
            {
                result = m_clientBackend->pause();
            }
            else
            {
                GST_ERROR("No web audio backend");
            }
        });

    return result;
}

bool GStreamerWebAudioPlayerClient::setEos()
{
    GST_DEBUG("entry:");

    bool result = false;
    m_backendQueue->callInEventLoop(
        [&]()
        {
            if (m_isOpen && !m_isEos)
            {
                m_isEos = true;
                if (m_dataBuffers.empty())
                {
                    result = m_clientBackend->setEos();
                }
                else
                {
                    pushSamples();
                    result = true;
                }
            }
            else
            {
                GST_DEBUG("No web audio backend, valid scenario");
            }
        });

    return result;
}

bool GStreamerWebAudioPlayerClient::isOpen()
{
    GST_DEBUG("entry:");

    bool result = false;
    m_backendQueue->callInEventLoop([&]() { result = m_isOpen; });

    return result;
}

void GStreamerWebAudioPlayerClient::notifyPushSamplesTimerExpired()
{
    m_backendQueue->scheduleInEventLoop([&]() { pushSamples(); });
}

bool GStreamerWebAudioPlayerClient::notifyNewSample(GstBuffer *buf)
{
    GST_DEBUG("entry:");

    bool result = false;

    {
        std::unique_lock lock{m_queueSizeMutex};
        m_queueSizeCv.wait(lock, [&]() { return m_dataBuffers.size() < kMaxQueueSize; });
    }

    m_backendQueue->callInEventLoop(
        [&]()
        {
            if (buf)
            {
                if (m_pushSamplesTimer)
                {
                    m_pushSamplesTimer->cancel();
                    m_pushSamplesTimer.reset();
                }
                {
                    std::unique_lock lock{m_queueSizeMutex};
                    m_dataBuffers.push(buf);
                }
                pushSamples();
                result = true;
            }
        });

    return result;
}

void GStreamerWebAudioPlayerClient::pushSamples()
{
    GST_DEBUG("entry:");
    if (!m_isOpen || m_dataBuffers.empty())
    {
        return;
    }

    uint32_t availableFrames = 0u;
    do
    {
        if (!m_clientBackend->getBufferAvailable(availableFrames))
        {
            GST_ERROR("getBufferAvailable failed, could not process the samples");
            // clear the queue if getBufferAvailable failed
            std::queue<GstBuffer *> empty;
            std::unique_lock lock{m_queueSizeMutex};
            std::swap(m_dataBuffers, empty);
            m_queueSizeCv.notify_one();
        }
        else if (0 != availableFrames)
        {
            bool writeFailure = false;
            GstBuffer *buffer = m_dataBuffers.front();
            gsize bufferSize = gst_buffer_get_size(buffer);
            auto framesToWrite = std::min(availableFrames, static_cast<uint32_t>(bufferSize / m_frameSize));
            if (framesToWrite > 0)
            {
                GstMapInfo bufferMap;
                if (!gst_buffer_map(buffer, &bufferMap, GST_MAP_READ))
                {
                    GST_ERROR("Could not map audio buffer, discarding buffer!");
                    writeFailure = true;
                }
                else
                {
                    if (!m_clientBackend->writeBuffer(framesToWrite, bufferMap.data))
                    {
                        GST_ERROR("Could not map audio buffer, discarding buffer!");
                        writeFailure = true;
                    }
                    gst_buffer_unmap(buffer, &bufferMap);
                }
            }

            if ((!writeFailure) && (framesToWrite * m_frameSize < bufferSize))
            {
                // Handle any leftover data
                uint32_t leftoverData = bufferSize - (availableFrames * m_frameSize);
                gst_buffer_resize(buffer, framesToWrite * m_frameSize, leftoverData);
                if ((leftoverData / m_frameSize == 0) && (m_dataBuffers.size() > 1))
                {
                    // If the leftover data is smaller than a frame, it must be processed with the next buffer
                    std::unique_lock lock{m_queueSizeMutex};
                    m_dataBuffers.pop();
                    m_dataBuffers.front() = gst_buffer_append(buffer, m_dataBuffers.front());
                    gst_buffer_unref(buffer);
                    m_queueSizeCv.notify_one();
                }
            }
            else
            {
                std::unique_lock lock{m_queueSizeMutex};
                m_dataBuffers.pop();
                gst_buffer_unref(buffer);
                m_queueSizeCv.notify_one();
            }
        }
    } while (!m_dataBuffers.empty() && availableFrames != 0);

    // If we still have samples stored that could not be pushed
    // This avoids any stoppages in the pushing of samples to the server if the consumption of
    // samples is slow.
    if (m_dataBuffers.size())
    {
        m_pushSamplesTimer =
            m_timerFactory->createTimer(std::chrono::milliseconds(10), [this]() { notifyPushSamplesTimerExpired(); });
    }
    else if (m_isEos)
    {
        m_clientBackend->setEos();
    }
}

bool GStreamerWebAudioPlayerClient::isNewConfig(const std::string &audioMimeType,
                                                std::weak_ptr<const firebolt::rialto::WebAudioConfig> webAudioConfig)
{
    if (audioMimeType != m_mimeType)
    {
        return true;
    }

    if (audioMimeType != "audio/x-raw")
    {
        GST_ERROR("Cannot compare none pcm config");
        return true;
    }

    std::shared_ptr<const firebolt::rialto::WebAudioConfig> config = webAudioConfig.lock();
    if (!config || config->pcm != m_config.pcm)
    {
        return true;
    }

    return false;
}

void GStreamerWebAudioPlayerClient::notifyState(firebolt::rialto::WebAudioPlayerState state)
{
    switch (state)
    {
    case firebolt::rialto::WebAudioPlayerState::END_OF_STREAM:
    {
        GST_INFO("Notify end of stream.");
        m_delegate.handleEos();
        m_isEos = false;
        break;
    }
    case firebolt::rialto::WebAudioPlayerState::FAILURE:
    {
        std::string errMessage = "Rialto server webaudio playback failed";
        GST_ERROR("%s", errMessage.c_str());
        m_delegate.handleError(errMessage);
        break;
    }
    case firebolt::rialto::WebAudioPlayerState::IDLE:
    {
        m_delegate.handleStateChanged(firebolt::rialto::PlaybackState::IDLE);
        break;
    }
    case firebolt::rialto::WebAudioPlayerState::PLAYING:
    {
        m_delegate.handleStateChanged(firebolt::rialto::PlaybackState::PLAYING);
        break;
    }
    case firebolt::rialto::WebAudioPlayerState::PAUSED:
    {
        m_delegate.handleStateChanged(firebolt::rialto::PlaybackState::PAUSED);
        break;
    }
    case firebolt::rialto::WebAudioPlayerState::UNKNOWN:
    default:
    {
        GST_WARNING("Web audio player sent unknown state");
        break;
    }
    }
}

bool GStreamerWebAudioPlayerClient::setVolume(double volume)
{
    bool status{false};
    m_backendQueue->callInEventLoop([&]() { status = m_clientBackend->setVolume(volume); });
    return status;
}

bool GStreamerWebAudioPlayerClient::getVolume(double &volume)
{
    bool status{false};
    m_backendQueue->callInEventLoop([&]() { status = m_clientBackend->getVolume(volume); });
    return status;
}
