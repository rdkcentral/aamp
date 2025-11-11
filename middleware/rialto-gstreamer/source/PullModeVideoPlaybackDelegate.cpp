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

#include "PullModeVideoPlaybackDelegate.h"
#include "GStreamerEMEUtils.h"
#include "GStreamerMSEUtils.h"
#include "GstreamerCatLog.h"

#define GST_CAT_DEFAULT rialtoGStreamerCat

PullModeVideoPlaybackDelegate::PullModeVideoPlaybackDelegate(GstElement *sink) : PullModePlaybackDelegate(sink)
{
    m_mediaSourceType = firebolt::rialto::MediaSourceType::VIDEO;
    m_isAsync = true;
}

GstStateChangeReturn PullModeVideoPlaybackDelegate::changeState(GstStateChange transition)
{
    switch (transition)
    {
    case GST_STATE_CHANGE_READY_TO_PAUSED:
    {
        if (!attachToMediaClientAndSetStreamsNumber(m_maxWidth, m_maxHeight))
        {
            return GST_STATE_CHANGE_FAILURE;
        }

        std::shared_ptr<GStreamerMSEMediaPlayerClient> client = m_mediaPlayerManager.getMediaPlayerClient();
        if (!client)
        {
            GST_ERROR_OBJECT(m_sink, "MediaPlayerClient is nullptr");
            return GST_STATE_CHANGE_FAILURE;
        }

        std::unique_lock lock{m_propertyMutex};
        if (m_rectangleSettingQueued)
        {
            GST_DEBUG_OBJECT(m_sink, "Set queued video rectangle");
            m_rectangleSettingQueued = false;
            client->setVideoRectangle(m_videoRectangle);
        }
        break;
    }
    default:
        break;
    }
    return PullModePlaybackDelegate::changeState(transition);
}

gboolean PullModeVideoPlaybackDelegate::handleEvent(GstPad *pad, GstObject *parent, GstEvent *event)
{
    switch (GST_EVENT_TYPE(event))
    {
    case GST_EVENT_CAPS:
    {
        GstCaps *caps{nullptr};
        gst_event_parse_caps(event, &caps);
        if (m_sourceAttached)
        {
            GST_INFO_OBJECT(m_sink, "Source already attached. Skip calling attachSource");
            break;
        }

        GST_INFO_OBJECT(m_sink, "Attaching VIDEO source with caps %" GST_PTR_FORMAT, caps);

        std::unique_ptr<firebolt::rialto::IMediaPipeline::MediaSource> vsource = createMediaSource(caps);
        if (vsource)
        {
            std::shared_ptr<GStreamerMSEMediaPlayerClient> client = m_mediaPlayerManager.getMediaPlayerClient();
            if ((!client) || (!client->attachSource(vsource, RIALTO_MSE_BASE_SINK(m_sink), shared_from_this())))
            {
                GST_ERROR_OBJECT(m_sink, "Failed to attach VIDEO source");
            }
            else
            {
                m_sourceAttached = true;

                // check if READY -> PAUSED was requested before source was attached
                if (GST_STATE_NEXT(m_sink) == GST_STATE_PAUSED)
                {
                    client->pause(m_sourceId);
                }
                std::unique_lock lock{m_propertyMutex};
                if (m_immediateOutputQueued)
                {
                    GST_DEBUG_OBJECT(m_sink, "Set queued immediate-output");
                    m_immediateOutputQueued = false;
                    if (!client->setImmediateOutput(m_sourceId, m_immediateOutput))
                    {
                        GST_ERROR_OBJECT(m_sink, "Could not set immediate-output");
                    }
                }
                if (m_syncmodeStreamingQueued)
                {
                    GST_DEBUG_OBJECT(m_sink, "Set queued syncmode-streaming");
                    m_syncmodeStreamingQueued = false;
                    if (!client->setStreamSyncMode(m_sourceId, m_syncmodeStreaming))
                    {
                        GST_ERROR_OBJECT(m_sink, "Could not set syncmode-streaming");
                    }
                }
                if (m_videoMuteQueued)
                {
                    GST_DEBUG_OBJECT(m_sink, "Set queued show-video-window");
                    m_videoMuteQueued = false;
                    client->setMute(m_videoMute, m_sourceId);
                }
            }
        }
        else
        {
            GST_ERROR_OBJECT(m_sink, "Failed to create VIDEO source");
        }

        break;
    }
    default:
        break;
    }
    return PullModePlaybackDelegate::handleEvent(pad, parent, event);
}

void PullModeVideoPlaybackDelegate::getProperty(const Property &type, GValue *value)
{
    switch (type)
    {
    case Property::WindowSet:
    {
        std::unique_lock lock{m_propertyMutex};
        auto client = m_mediaPlayerManager.getMediaPlayerClient();
        if (!client)
        {
            // Return the default value and
            // queue a setting event (for the default value) so that it will become true when
            // the client connects...
            GST_DEBUG_OBJECT(m_sink, "Return default rectangle setting, and queue an event to set the default upon "
                                     "client connect");
            m_rectangleSettingQueued = true;
            g_value_set_string(value, m_videoRectangle.c_str());
        }
        else
        {
            lock.unlock();
            g_value_set_string(value, client->getVideoRectangle().c_str());
        }
        break;
    }
    case Property::MaxVideoWidth:
    {
        g_value_set_uint(value, m_maxWidth);
        break;
    }
    case Property::MaxVideoHeight:
    {
        g_value_set_uint(value, m_maxHeight);
        break;
    }
    case Property::FrameStepOnPreroll:
    {
        g_value_set_boolean(value, m_stepOnPrerollEnabled);
        break;
    }
    case Property::ImmediateOutput:
    {
        std::unique_lock lock{m_propertyMutex};
        auto client = m_mediaPlayerManager.getMediaPlayerClient();
        if (!client)
        {
            // Return the default value and
            // queue a setting event (for the default value) so that it will become true when
            // the client connects...
            GST_DEBUG_OBJECT(m_sink, "Return default immediate-output setting, and queue an event to set the default "
                                     "upon client connect");
            m_immediateOutputQueued = true;
            g_value_set_boolean(value, m_immediateOutput);
        }
        else
        {
            bool immediateOutput{m_immediateOutput};
            lock.unlock();
            if (!client->getImmediateOutput(m_sourceId, immediateOutput))
            {
                GST_ERROR_OBJECT(m_sink, "Could not get immediate-output");
            }
            g_value_set_boolean(value, immediateOutput);
        }
        break;
    }
    default:
    {
        PullModePlaybackDelegate::getProperty(type, value);
        break;
    }
    }
}

void PullModeVideoPlaybackDelegate::setProperty(const Property &type, const GValue *value)
{
    std::shared_ptr<GStreamerMSEMediaPlayerClient> client = m_mediaPlayerManager.getMediaPlayerClient();
    switch (type)
    {
    case Property::WindowSet:
    {
        const gchar *rectangle = g_value_get_string(value);
        if (!rectangle)
        {
            GST_WARNING_OBJECT(m_sink, "Rectangle string not valid");
            break;
        }
        std::string videoRectangle{rectangle};
        std::unique_lock lock{m_propertyMutex};
        m_videoRectangle = videoRectangle;
        if (!client)
        {
            GST_DEBUG_OBJECT(m_sink, "Rectangle setting enqueued");
            m_rectangleSettingQueued = true;
        }
        else
        {
            lock.unlock();
            client->setVideoRectangle(videoRectangle);
        }
        break;
    }
    case Property::MaxVideoWidth:
        m_maxWidth = g_value_get_uint(value);
        break;
    case Property::MaxVideoHeight:
        m_maxHeight = g_value_get_uint(value);
        break;
    case Property::FrameStepOnPreroll:
    {
        bool stepOnPrerollEnabled = g_value_get_boolean(value);
        if (client && stepOnPrerollEnabled && !m_stepOnPrerollEnabled)
        {
            GST_INFO_OBJECT(m_sink, "Frame stepping on preroll");
            client->renderFrame(m_sourceId);
        }
        m_stepOnPrerollEnabled = stepOnPrerollEnabled;
        break;
    }
    case Property::ImmediateOutput:
    {
        bool immediateOutput = (g_value_get_boolean(value) != FALSE);
        std::unique_lock lock{m_propertyMutex};
        m_immediateOutput = immediateOutput;
        if (!client)
        {
            GST_DEBUG_OBJECT(m_sink, "Immediate output setting enqueued");
            m_immediateOutputQueued = true;
        }
        else
        {
            lock.unlock();
            if (!client->setImmediateOutput(m_sourceId, immediateOutput))
            {
                GST_ERROR_OBJECT(m_sink, "Could not set immediate-output");
            }
        }
        break;
    }
    case Property::SyncmodeStreaming:
    {
        bool syncmodeStreaming = (g_value_get_boolean(value) != FALSE);
        std::unique_lock lock{m_propertyMutex};
        m_syncmodeStreaming = syncmodeStreaming;
        if (!client)
        {
            GST_DEBUG_OBJECT(m_sink, "Syncmode streaming setting enqueued");
            m_syncmodeStreamingQueued = true;
        }
        else
        {
            lock.unlock();
            if (!client->setStreamSyncMode(m_sourceId, syncmodeStreaming))
            {
                GST_ERROR_OBJECT(m_sink, "Could not set syncmode-streaming");
            }
        }
        break;
    }
    case Property::ShowVideoWindow:
    {
        bool videoMute = (g_value_get_boolean(value) == FALSE);
        std::unique_lock lock{m_propertyMutex};
        m_videoMute = videoMute;
        if (!client || !m_sourceAttached)
        {
            GST_DEBUG_OBJECT(m_sink, "Show video window setting enqueued");
            m_videoMuteQueued = true;
        }
        else
        {
            lock.unlock();
            client->setMute(videoMute, m_sourceId);
        }
        break;
    }
    default:
    {
        PullModePlaybackDelegate::setProperty(type, value);
        break;
    }
    }
}

void PullModeVideoPlaybackDelegate::handleQos(uint64_t processed, uint64_t dropped) const
{
    GstBus *bus = gst_element_get_bus(m_sink);
    /* Hardcode isLive to FALSE and set invalid timestamps */
    GstMessage *message = gst_message_new_qos(GST_OBJECT(m_sink), FALSE, GST_CLOCK_TIME_NONE, GST_CLOCK_TIME_NONE,
                                              GST_CLOCK_TIME_NONE, GST_CLOCK_TIME_NONE);

    gst_message_set_qos_stats(message, GST_FORMAT_BUFFERS, processed, dropped);
    gst_bus_post(bus, message);
    gst_object_unref(bus);
}

std::unique_ptr<firebolt::rialto::IMediaPipeline::MediaSource>
PullModeVideoPlaybackDelegate::createMediaSource(GstCaps *caps) const
{
    GstStructure *structure = gst_caps_get_structure(caps, 0);
    const gchar *strct_name = gst_structure_get_name(structure);

    firebolt::rialto::SegmentAlignment alignment = get_segment_alignment(structure);
    std::shared_ptr<firebolt::rialto::CodecData> codecData = get_codec_data(structure);
    firebolt::rialto::StreamFormat format = get_stream_format(structure);

    gint width{0};
    gint height{0};
    gst_structure_get_int(structure, "width", &width);
    gst_structure_get_int(structure, "height", &height);

    if (strct_name)
    {
        std::string mimeType{};
        if (g_str_has_prefix(strct_name, "video/x-h264"))
        {
            mimeType = "video/h264";
        }
        else if (g_str_has_prefix(strct_name, "video/x-h265"))
        {
            mimeType = "video/h265";

            uint32_t dolbyVisionProfile{0};
            if (get_dv_profile(structure, dolbyVisionProfile))
            {
                return std::make_unique<firebolt::rialto::IMediaPipeline::MediaSourceVideoDolbyVision>(mimeType,
                                                                                                       dolbyVisionProfile,
                                                                                                       m_hasDrm, width,
                                                                                                       height, alignment,
                                                                                                       format, codecData);
            }
        }
        else
        {
            mimeType = strct_name;
        }

        GST_INFO_OBJECT(m_sink, "%s video media source created", mimeType.c_str());
        return std::make_unique<firebolt::rialto::IMediaPipeline::MediaSourceVideo>(mimeType, m_hasDrm, width, height,
                                                                                    alignment, format, codecData);
    }
    else
    {
        GST_ERROR_OBJECT(m_sink,
                         "Empty caps' structure name! Failed to set mime type when constructing video media source");
    }

    return nullptr;
}