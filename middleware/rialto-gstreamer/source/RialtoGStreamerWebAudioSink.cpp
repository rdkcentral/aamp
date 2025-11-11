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

#include <gst/gst.h>

#include "Constants.h"
#include "PushModeAudioPlaybackDelegate.h"
#include "RialtoGStreamerWebAudioSink.h"

using namespace firebolt::rialto::client;

GST_DEBUG_CATEGORY_STATIC(RialtoWebAudioSinkDebug);
#define GST_CAT_DEFAULT RialtoWebAudioSinkDebug

#define rialto_web_audio_sink_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE(RialtoWebAudioSink, rialto_web_audio_sink, GST_TYPE_ELEMENT,
                        G_ADD_PRIVATE(RialtoWebAudioSink)
                            GST_DEBUG_CATEGORY_INIT(RialtoWebAudioSinkDebug, "rialtowebaudiosink", 0,
                                                    "rialto web audio sink"));
enum
{
    PROP_0,
    PROP_TS_OFFSET,
    PROP_VOLUME,
    PROP_LAST
};

void rialto_web_audio_sink_initialise_delegate(RialtoWebAudioSink *sink,
                                               const std::shared_ptr<IPlaybackDelegate> &delegate)
{
    std::unique_lock lock{sink->priv->m_sinkMutex};
    sink->priv->m_delegate = delegate;

    for (auto &[type, value] : sink->priv->m_queuedProperties)
    {
        delegate->setProperty(type, &value);
        g_value_unset(&value);
    }
    sink->priv->m_queuedProperties.clear();
}

static std::shared_ptr<IPlaybackDelegate> rialto_web_audio_sink_get_delegate(RialtoWebAudioSink *sink)
{
    std::unique_lock lock{sink->priv->m_sinkMutex};
    if (!sink->priv->m_delegate)
    {
        GST_ERROR_OBJECT(sink, "Sink delegate not initialized");
    }
    return sink->priv->m_delegate;
}

static void rialto_web_audio_sink_handle_get_property(RialtoWebAudioSink *sink,
                                                      const IPlaybackDelegate::Property &property, GValue *value)
{
    if (auto delegate = rialto_web_audio_sink_get_delegate(sink))
    {
        delegate->getProperty(property, value);
    }
    else // Copy queued value if present
    {
        std::unique_lock lock{sink->priv->m_sinkMutex};
        if (sink->priv->m_queuedProperties.find(property) != sink->priv->m_queuedProperties.end())
        {
            g_value_copy(&sink->priv->m_queuedProperties[property], value);
        }
    }
}

static void rialto_web_audio_sink_handle_set_property(RialtoWebAudioSink *sink,
                                                      const IPlaybackDelegate::Property &property, const GValue *value)
{
    if (auto delegate = rialto_web_audio_sink_get_delegate(sink))
    {
        delegate->setProperty(property, value);
    }
    else
    {
        std::unique_lock lock{sink->priv->m_sinkMutex};
        sink->priv->m_queuedProperties[property] = G_VALUE_INIT;
        g_value_init(&(sink->priv->m_queuedProperties[property]), G_VALUE_TYPE(value));
        g_value_copy(value, &(sink->priv->m_queuedProperties[property]));
    }
}

static gboolean rialto_web_audio_sink_send_event(GstElement *element, GstEvent *event)
{
    RialtoWebAudioSink *sink = RIALTO_WEB_AUDIO_SINK(element);
    if (auto delegate = rialto_web_audio_sink_get_delegate(sink))
    {
        return delegate->handleSendEvent(event);
    }
    return FALSE;
}

static GstStateChangeReturn rialto_web_audio_sink_change_state(GstElement *element, GstStateChange transition)
{
    RialtoWebAudioSink *sink = RIALTO_WEB_AUDIO_SINK(element);
    if (GST_STATE_CHANGE_NULL_TO_READY == transition)
    {
        GST_INFO_OBJECT(sink, "RialtoWebAudioSink state change to READY. Initializing delegate");
        rialto_web_audio_sink_initialise_delegate(sink, std::make_shared<PushModeAudioPlaybackDelegate>(element));
    }
    if (auto delegate = rialto_web_audio_sink_get_delegate(sink))
    {
        GstStateChangeReturn status = delegate->changeState(transition);
        if (GST_STATE_CHANGE_FAILURE != status)
        {
            GstStateChangeReturn result = GST_ELEMENT_CLASS(parent_class)->change_state(element, transition);
            if (G_UNLIKELY(result == GST_STATE_CHANGE_FAILURE))
            {
                GST_WARNING_OBJECT(sink, "State change failed");
                return result;
            }
            else if (result == GST_STATE_CHANGE_ASYNC)
            {
                return GST_STATE_CHANGE_ASYNC;
            }
        }
        return status;
    }
    return GST_STATE_CHANGE_FAILURE;
}

static gboolean rialto_web_audio_sink_event(GstPad *pad, GstObject *parent, GstEvent *event)
{
    if (auto delegate = rialto_web_audio_sink_get_delegate(RIALTO_WEB_AUDIO_SINK(parent)))
    {
        return delegate->handleEvent(pad, parent, event);
    }
    return FALSE;
}

static void rialto_web_audio_sink_get_property(GObject *object, guint propId, GValue *value, GParamSpec *pspec)
{
    switch (propId)
    {
    case PROP_TS_OFFSET:
    {
        rialto_web_audio_sink_handle_get_property(RIALTO_WEB_AUDIO_SINK(object), IPlaybackDelegate::Property::TsOffset,
                                                  value);
    }

    case PROP_VOLUME:
    {
        g_value_set_double(value, kDefaultVolume);
        rialto_web_audio_sink_handle_get_property(RIALTO_WEB_AUDIO_SINK(object), IPlaybackDelegate::Property::Volume,
                                                  value);
        break;
    }

    default:
    {
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, pspec);
        break;
    }
    }
}

static void rialto_web_audio_sink_set_property(GObject *object, guint propId, const GValue *value, GParamSpec *pspec)
{
    switch (propId)
    {
    case PROP_TS_OFFSET:
    {
        rialto_web_audio_sink_handle_set_property(RIALTO_WEB_AUDIO_SINK(object), IPlaybackDelegate::Property::TsOffset,
                                                  value);
        break;
    }

    case PROP_VOLUME:
    {
        rialto_web_audio_sink_handle_set_property(RIALTO_WEB_AUDIO_SINK(object), IPlaybackDelegate::Property::Volume,
                                                  value);
        break;
    }

    default:
    {
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, pspec);
        break;
    }
    }
}

static GstFlowReturn rialto_web_audio_sink_chain(GstPad *pad, GstObject *parent, GstBuffer *buf)
{
    if (auto delegate = rialto_web_audio_sink_get_delegate(RIALTO_WEB_AUDIO_SINK(parent)))
    {
        return delegate->handleBuffer(buf);
    }
    return GST_FLOW_ERROR;
}

static void rialto_web_audio_sink_setup_supported_caps(GstElementClass *elementClass)
{
    GstCaps *caps = gst_caps_from_string("audio/x-raw");
    GstPadTemplate *sinktempl = gst_pad_template_new("sink", GST_PAD_SINK, GST_PAD_ALWAYS, caps);
    gst_element_class_add_pad_template(elementClass, sinktempl);
    gst_caps_unref(caps);
}

static bool rialto_web_audio_sink_initialise_sinkpad(RialtoWebAudioSink *sink)
{
    GstPadTemplate *pad_template =
        gst_element_class_get_pad_template(GST_ELEMENT_CLASS(G_OBJECT_GET_CLASS(sink)), "sink");
    if (!pad_template)
    {
        GST_ERROR_OBJECT(sink, "Could not find sink pad template");
        return false;
    }

    GstPad *sinkPad = gst_pad_new_from_template(pad_template, "sink");
    if (!sinkPad)
    {
        GST_ERROR_OBJECT(sink, "Could not create sinkpad");
        return false;
    }

    gst_element_add_pad(GST_ELEMENT_CAST(sink), sinkPad);

    gst_pad_set_event_function(sinkPad, rialto_web_audio_sink_event);
    gst_pad_set_chain_function(sinkPad, rialto_web_audio_sink_chain);

    return true;
}

static void rialto_web_audio_sink_init(RialtoWebAudioSink *sink)
{
    GST_INFO_OBJECT(sink, "Init: %" GST_PTR_FORMAT, sink);
    sink->priv = static_cast<RialtoWebAudioSinkPrivate *>(rialto_web_audio_sink_get_instance_private(sink));
    new (sink->priv) RialtoWebAudioSinkPrivate();

    GST_OBJECT_FLAG_SET(sink, GST_ELEMENT_FLAG_SINK);
    if (!rialto_web_audio_sink_initialise_sinkpad(sink))
    {
        GST_ERROR_OBJECT(sink, "Failed to initialise AUDIO sink. Sink pad initialisation failed.");
        return;
    }
}

static void rialto_web_audio_sink_finalize(GObject *object)
{
    RialtoWebAudioSink *sink = RIALTO_WEB_AUDIO_SINK(object);
    RialtoWebAudioSinkPrivate *priv = sink->priv;
    GST_INFO_OBJECT(sink, "Finalize: %" GST_PTR_FORMAT " %" GST_PTR_FORMAT, sink, priv);

    priv->~RialtoWebAudioSinkPrivate();

    GST_CALL_PARENT(G_OBJECT_CLASS, finalize, (object));
}

static void rialto_web_audio_sink_class_init(RialtoWebAudioSinkClass *klass)
{
    GObjectClass *gobjectClass = G_OBJECT_CLASS(klass);
    GstElementClass *elementClass = GST_ELEMENT_CLASS(klass);

    gst_element_class_set_metadata(elementClass, "Rialto Web Audio sink", "Generic", "A sink for Rialto Web Audio",
                                   "   ");

    gobjectClass->finalize = rialto_web_audio_sink_finalize;
    gobjectClass->get_property = rialto_web_audio_sink_get_property;
    gobjectClass->set_property = rialto_web_audio_sink_set_property;

    elementClass->change_state = rialto_web_audio_sink_change_state;
    elementClass->send_event = rialto_web_audio_sink_send_event;

    g_object_class_install_property(gobjectClass, PROP_TS_OFFSET,
                                    g_param_spec_int64("ts-offset",
                                                       "ts-offset", "Not supported, RialtoWebAudioSink does not require the synchronisation of sources",
                                                       G_MININT64, G_MAXINT64, 0,
                                                       GParamFlags(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

    g_object_class_install_property(gobjectClass, PROP_VOLUME,
                                    g_param_spec_double("volume", "Volume", "Volume of this stream", 0, 1.0,
                                                        kDefaultVolume,
                                                        GParamFlags(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

    rialto_web_audio_sink_setup_supported_caps(elementClass);

    gst_element_class_set_details_simple(elementClass, "Rialto Web Audio Sink", "Decoder/Audio/Sink/Audio",
                                         "Communicates with Rialto Server", "   ");
}
