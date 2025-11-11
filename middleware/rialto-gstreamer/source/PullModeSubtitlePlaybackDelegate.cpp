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

#include "PullModeSubtitlePlaybackDelegate.h"
#include "GstreamerCatLog.h"

#define GST_CAT_DEFAULT rialtoGStreamerCat

PullModeSubtitlePlaybackDelegate::PullModeSubtitlePlaybackDelegate(GstElement *sink) : PullModePlaybackDelegate(sink)
{
    m_mediaSourceType = firebolt::rialto::MediaSourceType::SUBTITLE;
    m_isAsync = false;
}

GstStateChangeReturn PullModeSubtitlePlaybackDelegate::changeState(GstStateChange transition)
{
    switch (transition)
    {
    case GST_STATE_CHANGE_READY_TO_PAUSED:
    {
        if (!attachToMediaClientAndSetStreamsNumber())
        {
            return GST_STATE_CHANGE_FAILURE;
        }
    }
    default:
        break;
    }
    return PullModePlaybackDelegate::changeState(transition);
}

gboolean PullModeSubtitlePlaybackDelegate::handleEvent(GstPad *pad, GstObject *parent, GstEvent *event)
{
    switch (GST_EVENT_TYPE(event))
    {
    case GST_EVENT_CAPS:
    {
        GstCaps *caps;
        gst_event_parse_caps(event, &caps);
        if (m_sourceAttached)
        {
            GST_INFO_OBJECT(m_sink, "Source already attached. Skip calling attachSource");
            break;
        }

        GST_INFO_OBJECT(m_sink, "Attaching SUBTITLE source with caps %" GST_PTR_FORMAT, caps);

        std::unique_ptr<firebolt::rialto::IMediaPipeline::MediaSource> subtitleSource{createMediaSource(caps)};
        if (subtitleSource)
        {
            std::shared_ptr<GStreamerMSEMediaPlayerClient> client = m_mediaPlayerManager.getMediaPlayerClient();
            if ((!client) || (!client->attachSource(subtitleSource, RIALTO_MSE_BASE_SINK(m_sink), shared_from_this())))
            {
                GST_ERROR_OBJECT(m_sink, "Failed to attach SUBTITLE source");
            }
            else
            {
                m_sourceAttached = true;
                if (m_isMuteQueued)
                {
                    client->setMute(m_isMuted, m_sourceId);
                    m_isMuteQueued = false;
                }

                {
                    std::unique_lock lock{m_mutex};
                    if (m_isTextTrackIdentifierQueued)
                    {
                        client->setTextTrackIdentifier(m_textTrackIdentifier);
                        m_isTextTrackIdentifierQueued = false;
                    }

                    if (m_queuedOffset)
                    {
                        GST_DEBUG_OBJECT(m_sink, "Setting subtitle offset to: %" GST_TIME_FORMAT,
                                         GST_TIME_ARGS(m_queuedOffset.value()));
                        client->setSubtitleOffset(m_sourceId, m_queuedOffset.value());
                        m_queuedOffset.reset();
                    }
                }

                // check if READY -> PAUSED was requested before source was attached
                if (GST_STATE_NEXT(m_sink) == GST_STATE_PAUSED)
                {
                    client->pause(m_sourceId);
                }
            }
        }
        else
        {
            GST_ERROR_OBJECT(m_sink, "Failed to create SUBTITLE source");
        }

        break;
    }
    case GST_EVENT_CUSTOM_DOWNSTREAM:
    case GST_EVENT_CUSTOM_DOWNSTREAM_OOB:
    {
        if (gst_event_has_name(event, "set-pts-offset"))
        {
            GST_DEBUG_OBJECT(m_sink, "Set pts offset event received");
            const GstStructure *structure{gst_event_get_structure(event)};
            guint64 ptsOffset{GST_CLOCK_TIME_NONE};
            if (gst_structure_get_uint64(structure, "pts-offset", &ptsOffset) == TRUE)
            {
                std::unique_lock lock{m_sinkMutex};

                std::shared_ptr<GStreamerMSEMediaPlayerClient> client = m_mediaPlayerManager.getMediaPlayerClient();
                if (!client || !m_sourceAttached)
                {
                    GST_DEBUG_OBJECT(m_sink, "offset setting enqueued");
                    m_queuedOffset = static_cast<int64_t>(ptsOffset);
                }
                else
                {
                    GST_DEBUG_OBJECT(m_sink, "Setting subtitle offset to: %" GST_TIME_FORMAT, GST_TIME_ARGS(ptsOffset));
                    client->setSubtitleOffset(m_sourceId, ptsOffset);
                }
            }
            else
            {
                GST_WARNING_OBJECT(m_sink, "Unable to set pts offset. Value not present");
            }
        }
        break;
    }
    default:
        break;
    }
    return PullModePlaybackDelegate::handleEvent(pad, parent, event);
}

void PullModeSubtitlePlaybackDelegate::getProperty(const Property &type, GValue *value)
{
    std::shared_ptr<GStreamerMSEMediaPlayerClient> client{m_mediaPlayerManager.getMediaPlayerClient()};

    switch (type)
    {
    case Property::Mute:
    {
        if (!client)
        {
            g_value_set_boolean(value, m_isMuted);
            return;
        }
        g_value_set_boolean(value, client->getMute(m_sourceId));
        break;
    }
    case Property::TextTrackIdentifier:
    {
        {
            std::unique_lock lock{m_mutex};
            if (!client)
            {
                g_value_set_string(value, m_textTrackIdentifier.c_str());
                return;
            }
        }
        g_value_set_string(value, client->getTextTrackIdentifier().c_str());

        break;
    }
    case Property::WindowId:
    {
        g_value_set_uint(value, m_videoId);
        break;
    }
    case Property::Async:
    {
        g_value_set_boolean(value, m_isAsync);
        break;
    }
    default:
    {
        PullModePlaybackDelegate::getProperty(type, value);
        break;
    }
    }
}

void PullModeSubtitlePlaybackDelegate::setProperty(const Property &type, const GValue *value)
{
    std::shared_ptr<GStreamerMSEMediaPlayerClient> client = m_mediaPlayerManager.getMediaPlayerClient();

    switch (type)
    {
    case Property::Mute:
        m_isMuted = g_value_get_boolean(value);
        if (!client || !m_sourceAttached)
        {
            m_isMuteQueued = true;
            return;
        }

        client->setMute(m_isMuted, m_sourceId);

        break;
    case Property::TextTrackIdentifier:
    {
        const gchar *textTrackIdentifier = g_value_get_string(value);
        if (!textTrackIdentifier)
        {
            GST_WARNING_OBJECT(m_sink, "TextTrackIdentifier string not valid");
            break;
        }

        std::unique_lock lock{m_mutex};
        m_textTrackIdentifier = std::string(textTrackIdentifier);
        if (!client || !m_sourceAttached)
        {
            GST_DEBUG_OBJECT(m_sink, "Text track identifier setting enqueued");
            m_isTextTrackIdentifierQueued = true;
        }
        else
        {
            client->setTextTrackIdentifier(m_textTrackIdentifier);
        }

        break;
    }
    case Property::WindowId:
    {
        m_videoId = g_value_get_uint(value);
        break;
    }
    case Property::Async:
    {
        m_isAsync = g_value_get_boolean(value);
        break;
    }
    default:
    {
        PullModePlaybackDelegate::setProperty(type, value);
        break;
    }
    }
}

void PullModeSubtitlePlaybackDelegate::handleQos(uint64_t processed, uint64_t dropped) const
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
PullModeSubtitlePlaybackDelegate::createMediaSource(GstCaps *caps) const
{
    GstStructure *structure = gst_caps_get_structure(caps, 0);
    const gchar *mimeName = gst_structure_get_name(structure);
    if (mimeName)
    {
        std::string mimeType{};
        if (g_str_has_prefix(mimeName, "text/vtt") || g_str_has_prefix(mimeName, "application/x-subtitle-vtt"))
        {
            mimeType = "text/vtt";
        }
        else if (g_str_has_prefix(mimeName, "application/ttml+xml"))
        {
            mimeType = "text/ttml";
        }
        else if (g_str_has_prefix(mimeName, "closedcaption/x-cea-608") ||
                 g_str_has_prefix(mimeName, "closedcaption/x-cea-708") ||
                 g_str_has_prefix(mimeName, "application/x-cea-608") ||
                 g_str_has_prefix(mimeName, "application/x-cea-708") ||
                 g_str_has_prefix(mimeName, "application/x-subtitle-cc"))
        {
            mimeType = "text/cc";
        }
        else
        {
            mimeType = mimeName;
        }

        GST_INFO_OBJECT(m_sink, "%s subtitle media source created", mimeType.c_str());
        return std::make_unique<firebolt::rialto::IMediaPipeline::MediaSourceSubtitle>(mimeType, m_textTrackIdentifier);
    }
    else
    {
        GST_ERROR_OBJECT(m_sink,
                         "Empty caps' structure name! Failed to set mime type when constructing subtitle media source");
    }

    return nullptr;
}