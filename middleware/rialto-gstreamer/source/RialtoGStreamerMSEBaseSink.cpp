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

#define USE_GLIB 1

#include <cstring>
#include <limits>

#include <gst/gst.h>

#include "ControlBackend.h"
#include "GStreamerUtils.h"
#include "IClientLogControl.h"
#include "IMediaPipeline.h"
#include "LogToGstHandler.h"
#include "RialtoGStreamerMSEBaseSink.h"
#include "RialtoGStreamerMSEBaseSinkPrivate.h"

GST_DEBUG_CATEGORY_STATIC(RialtoMSEBaseSinkDebug);
#define GST_CAT_DEFAULT RialtoMSEBaseSinkDebug

#define rialto_mse_base_sink_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE(RialtoMSEBaseSink, rialto_mse_base_sink, GST_TYPE_ELEMENT,
                        G_ADD_PRIVATE(RialtoMSEBaseSink)
                            GST_DEBUG_CATEGORY_INIT(RialtoMSEBaseSinkDebug, "rialtomsebasesink", 0,
                                                    "rialto mse base sink"));

enum
{
    PROP_0,
    PROP_IS_SINGLE_PATH_STREAM,
    PROP_N_STREAMS,
    PROP_HAS_DRM,
    PROP_STATS,
    PROP_LAST
};

enum
{
    SIGNAL_UNDERFLOW,
    SIGNAL_LAST
};

static guint g_signals[SIGNAL_LAST] = {0};

void rialto_mse_base_sink_initialise_delegate(RialtoMSEBaseSink *sink, const std::shared_ptr<IPlaybackDelegate> &delegate)
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

static std::shared_ptr<IPlaybackDelegate> rialto_mse_base_sink_get_delegate(RialtoMSEBaseSink *sink)
{
    std::unique_lock lock{sink->priv->m_sinkMutex};
    if (!sink->priv->m_delegate)
    {
        GST_ERROR_OBJECT(sink, "Sink delegate not initialized");
    }
    return sink->priv->m_delegate;
}

static gboolean rialto_mse_base_sink_send_event(GstElement *element, GstEvent *event)
{
    if (auto delegate = rialto_mse_base_sink_get_delegate(RIALTO_MSE_BASE_SINK(element)))
    {
        return delegate->handleSendEvent(event);
    }
    return FALSE;
}

gboolean rialto_mse_base_sink_event(GstPad *pad, GstObject *parent, GstEvent *event)
{
    if (auto delegate = rialto_mse_base_sink_get_delegate(RIALTO_MSE_BASE_SINK(parent)))
    {
        return delegate->handleEvent(pad, parent, event);
    }
    return FALSE;
}

GstFlowReturn rialto_mse_base_sink_chain(GstPad *pad, GstObject *parent, GstBuffer *buf)
{
    if (auto delegate = rialto_mse_base_sink_get_delegate(RIALTO_MSE_BASE_SINK(parent)))
    {
        return delegate->handleBuffer(buf);
    }
    return GST_FLOW_ERROR;
}

static gboolean rialto_mse_base_sink_query(GstElement *element, GstQuery *query)
{
    RialtoMSEBaseSink *sink = RIALTO_MSE_BASE_SINK(element);
    if (auto delegate = rialto_mse_base_sink_get_delegate(sink))
    {
        std::optional<gboolean> result{delegate->handleQuery(query)};
        if (result.has_value())
        {
            return result.value();
        }
        GstElement *parent = GST_ELEMENT(&sink->parent);
        return GST_ELEMENT_CLASS(parent_class)->query(parent, query);
    }
    return FALSE;
}

static GstStateChangeReturn rialto_mse_base_sink_change_state(GstElement *element, GstStateChange transition)
{
    RialtoMSEBaseSink *sink = RIALTO_MSE_BASE_SINK(element);
    if (auto delegate = rialto_mse_base_sink_get_delegate(sink))
    {
        GstStateChangeReturn status = delegate->changeState(transition);
        if (GST_STATE_CHANGE_FAILURE != status)
        {
            if (GST_STATE_CHANGE_READY_TO_NULL == transition)
            {
                sink->priv->m_delegate.reset();
            }
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

void rialto_mse_base_sink_handle_get_property(RialtoMSEBaseSink *sink, const IPlaybackDelegate::Property &property,
                                              GValue *value)
{
    if (auto delegate = rialto_mse_base_sink_get_delegate(sink))
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

void rialto_mse_base_sink_handle_set_property(RialtoMSEBaseSink *sink, const IPlaybackDelegate::Property &property,
                                              const GValue *value)
{
    if (auto delegate = rialto_mse_base_sink_get_delegate(sink))
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

static void rialto_mse_base_sink_get_property(GObject *object, guint propId, GValue *value, GParamSpec *pspec)
{
    switch (propId)
    {
    case PROP_IS_SINGLE_PATH_STREAM:
        // Set default value if it can't be acquired
        g_value_set_boolean(value, FALSE);
        rialto_mse_base_sink_handle_get_property(RIALTO_MSE_BASE_SINK(object),
                                                 IPlaybackDelegate::Property::IsSinglePathStream, value);
        break;
    case PROP_N_STREAMS:
        // Set default value if it can't be acquired
        g_value_set_int(value, 1);
        rialto_mse_base_sink_handle_get_property(RIALTO_MSE_BASE_SINK(object),
                                                 IPlaybackDelegate::Property::NumberOfStreams, value);
        break;
    case PROP_HAS_DRM:
        // Set default value if it can't be acquired
        g_value_set_boolean(value, TRUE);
        rialto_mse_base_sink_handle_get_property(RIALTO_MSE_BASE_SINK(object), IPlaybackDelegate::Property::HasDrm,
                                                 value);
        break;
    case PROP_STATS:
        rialto_mse_base_sink_handle_get_property(RIALTO_MSE_BASE_SINK(object), IPlaybackDelegate::Property::Stats, value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, pspec);
        break;
    }
}

static void rialto_mse_base_sink_set_property(GObject *object, guint propId, const GValue *value, GParamSpec *pspec)
{
    switch (propId)
    {
    case PROP_IS_SINGLE_PATH_STREAM:
        rialto_mse_base_sink_handle_set_property(RIALTO_MSE_BASE_SINK(object),
                                                 IPlaybackDelegate::Property::IsSinglePathStream, value);
        break;
    case PROP_N_STREAMS:
        rialto_mse_base_sink_handle_set_property(RIALTO_MSE_BASE_SINK(object),
                                                 IPlaybackDelegate::Property::NumberOfStreams, value);
        break;
    case PROP_HAS_DRM:
        rialto_mse_base_sink_handle_set_property(RIALTO_MSE_BASE_SINK(object), IPlaybackDelegate::Property::HasDrm,
                                                 value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, pspec);
        break;
    }
}

void rialto_mse_base_handle_rialto_server_sent_buffer_underflow(RialtoMSEBaseSink *sink)
{
    GST_WARNING_OBJECT(sink, "Sending underflow signal");
    // send 2 last parameters just to be compatible with RDK's buffer-underflow-callback signal signature
    g_signal_emit(G_OBJECT(sink), g_signals[SIGNAL_UNDERFLOW], 0, 0, nullptr);
}

bool rialto_mse_base_sink_initialise_sinkpad(RialtoMSEBaseSink *sink)
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
    sink->priv->m_sinkPad = sinkPad;

    return true;
}

static void rialto_mse_base_sink_init(RialtoMSEBaseSink *sink)
{
    GST_INFO_OBJECT(sink, "Init: %" GST_PTR_FORMAT, sink);
    sink->priv = static_cast<RialtoMSEBaseSinkPrivate *>(rialto_mse_base_sink_get_instance_private(sink));
    new (sink->priv) RialtoMSEBaseSinkPrivate();

    GST_OBJECT_FLAG_SET(sink, GST_ELEMENT_FLAG_SINK);
}

static void rialto_mse_base_sink_finalize(GObject *object)
{
    RialtoMSEBaseSink *sink = RIALTO_MSE_BASE_SINK(object);
    RialtoMSEBaseSinkPrivate *priv = sink->priv;
    GST_INFO_OBJECT(sink, "Finalize: %" GST_PTR_FORMAT " %" GST_PTR_FORMAT, sink, priv);

    priv->~RialtoMSEBaseSinkPrivate();
    GST_CALL_PARENT(G_OBJECT_CLASS, finalize, (object));
}

static void rialto_mse_base_sink_class_init(RialtoMSEBaseSinkClass *klass)
{
    std::shared_ptr<firebolt::rialto::IClientLogHandler> logToGstHandler =
        std::make_shared<firebolt::rialto::LogToGstHandler>();
    if (!firebolt::rialto::IClientLogControlFactory::createFactory()->createClientLogControl().registerLogHandler(logToGstHandler,
                                                                                                                  true))
    {
        GST_ERROR("Unable to preRegister log handler");
    }

    GObjectClass *gobjectClass = G_OBJECT_CLASS(klass);
    GstElementClass *elementClass = GST_ELEMENT_CLASS(klass);

    gst_element_class_set_metadata(elementClass, "Rialto MSE base sink", "Generic", "A sink for Rialto", "   ");

    gobjectClass->finalize = rialto_mse_base_sink_finalize;
    gobjectClass->get_property = rialto_mse_base_sink_get_property;
    gobjectClass->set_property = rialto_mse_base_sink_set_property;
    elementClass->query = rialto_mse_base_sink_query;
    elementClass->send_event = rialto_mse_base_sink_send_event;
    elementClass->change_state = rialto_mse_base_sink_change_state;

    g_signals[SIGNAL_UNDERFLOW] = g_signal_new("buffer-underflow-callback", G_TYPE_FROM_CLASS(klass),
                                               (GSignalFlags)(G_SIGNAL_RUN_LAST), 0, nullptr, nullptr,
                                               g_cclosure_marshal_VOID__UINT_POINTER, G_TYPE_NONE, 2, G_TYPE_UINT,
                                               G_TYPE_POINTER);

    g_object_class_install_property(gobjectClass, PROP_IS_SINGLE_PATH_STREAM,
                                    g_param_spec_boolean("single-path-stream", "single path stream",
                                                         "is single path stream", FALSE, GParamFlags(G_PARAM_READWRITE)));

    g_object_class_install_property(gobjectClass, PROP_N_STREAMS,
                                    g_param_spec_int("streams-number", "streams number", "streams number", 1, G_MAXINT,
                                                     1, GParamFlags(G_PARAM_READWRITE)));

    g_object_class_install_property(gobjectClass, PROP_HAS_DRM,
                                    g_param_spec_boolean("has-drm", "has drm", "has drm", TRUE,
                                                         GParamFlags(G_PARAM_READWRITE)));
    g_object_class_install_property(gobjectClass, PROP_STATS,
                                    g_param_spec_pointer("stats", NULL, "pointer to a gst_structure",
                                                         GParamFlags(G_PARAM_READABLE)));
}
