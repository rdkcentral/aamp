/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2024 
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

#include "GstProtectionMetadataHelperFactory.h"
#include "GstRialtoTextTrackSinkPrivate.h"
#include "GstTextTrackSinkFactory.h"
#include "RialtoServerLogging.h"
#include <atomic>
#include <cinttypes>
#include <cstdlib>
#include <gst/base/gstbasetransform.h>
#include <gst/gst.h>
#include <stdexcept>
G_BEGIN_DECLS

enum
{
    PROP_0,
    PROP_MUTE,
    PROP_TEXT_TRACK_IDENTIFIER,
    PROP_VIDEO_DECODER,
    PROP_POSITION,
    PROP_OFFSET,
    PROP_LAST
};

#define GST_RIALTO_TEXT_TRACK_SINK_TYPE (gst_rialto_text_track_sink_get_type())
#define GST_RIALTO_TEXT_TRACK_SINK(obj)                                                                                \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_RIALTO_TEXT_TRACK_SINK_TYPE, GstRialtoTextTrackSink))
#define GST_RIALTO_TEXT_TRACK_SINK_CLASS(klass)                                                                        \
    (G_TYPE_CHECK_CLASS_CAST((klass), GST_RIALTO_TEXT_TRACK_SINK_TYPE, GstRialtoTextTrackSinkClass))

typedef struct _GstRialtoTextTrackSink GstRialtoTextTrackSink;
typedef struct _GstRialtoTextTrackSinkClass GstRialtoTextTrackSinkClass;
typedef struct firebolt::rialto::server::GstRialtoTextTrackSinkPrivate GstRialtoTextTrackSinkPrivate;

GType gst_rialto_text_track_sink_get_type(void); // NOLINT(build/function_format)

struct _GstRialtoTextTrackSink
{
    GstBaseSink parent;
    GstRialtoTextTrackSinkPrivate *priv;
};

struct _GstRialtoTextTrackSinkClass
{
    GstBaseSink parentClass;
};

G_END_DECLS

static GstStaticPadTemplate sinkTemplate =
    GST_STATIC_PAD_TEMPLATE("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
                            GST_STATIC_CAPS("application/ttml+xml; text/vtt; application/x-subtitle-vtt; text/x-raw; "
                                            "subtitle/x-subtitle-cc"));

GST_DEBUG_CATEGORY(gst_rialto_text_track_sink_debug_category);
#define GST_CAT_DEFAULT gst_rialto_text_track_sink_debug_category

#define gst_rialto_text_track_sink_parent_class parent_class
G_DEFINE_TYPE_WITH_PRIVATE(GstRialtoTextTrackSink, gst_rialto_text_track_sink, GST_TYPE_BASE_SINK);

static void gst_rialto_text_track_sink_finalize(GObject *object);         // NOLINT(build/function_format)
static GstFlowReturn gst_rialto_text_track_sink_render(GstBaseSink *sink, // NOLINT(build/function_format)
                                                       GstBuffer *buffer);
static gboolean gst_rialto_text_track_sink_set_caps(GstBaseSink *sink, GstCaps *caps); // NOLINT(build/function_format)
static gboolean gst_rialto_text_track_sink_start(GstBaseSink *sink);                   // NOLINT(build/function_format)
static gboolean gst_rialto_text_track_sink_stop(GstBaseSink *sink);                    // NOLINT(build/function_format)
static gboolean gst_rialto_text_track_sink_event(GstBaseSink *sink, GstEvent *event);  // NOLINT(build/function_format)
static GstStateChangeReturn
gst_rialto_text_track_sink_change_state(GstElement *element, GstStateChange transition); // NOLINT(build/function_format)
static void gst_rialto_text_track_sink_get_property(GObject *object, guint propId, // NOLINT(build/function_format)
                                                    GValue *value, GParamSpec *pspec);
static void gst_rialto_text_track_sink_set_property(GObject *object, guint propId, // NOLINT(build/function_format)
                                                    const GValue *value, GParamSpec *pspec);
static gboolean gst_rialto_text_track_sink_query(GstElement *element, GstQuery *query); // NOLINT(build/function_format)

static void gst_rialto_text_track_sink_class_init(GstRialtoTextTrackSinkClass *klass) // NOLINT(build/function_format)
{
    GST_DEBUG_CATEGORY_INIT(gst_rialto_text_track_sink_debug_category, "rialto_text_track_sink", 0,
                            "TextTrack Sink for Rialto");

    GObjectClass *gobjectClass = G_OBJECT_CLASS(klass);
    GstElementClass *elementClass = GST_ELEMENT_CLASS(klass);
    GstBaseSinkClass *baseSinkClass = GST_BASE_SINK_CLASS(klass);

    gobjectClass->finalize = GST_DEBUG_FUNCPTR(gst_rialto_text_track_sink_finalize);
    gobjectClass->get_property = GST_DEBUG_FUNCPTR(gst_rialto_text_track_sink_get_property);
    gobjectClass->set_property = GST_DEBUG_FUNCPTR(gst_rialto_text_track_sink_set_property);
    baseSinkClass->start = GST_DEBUG_FUNCPTR(gst_rialto_text_track_sink_start);
    baseSinkClass->stop = GST_DEBUG_FUNCPTR(gst_rialto_text_track_sink_stop);
    baseSinkClass->render = GST_DEBUG_FUNCPTR(gst_rialto_text_track_sink_render);
    baseSinkClass->set_caps = GST_DEBUG_FUNCPTR(gst_rialto_text_track_sink_set_caps);
    baseSinkClass->event = GST_DEBUG_FUNCPTR(gst_rialto_text_track_sink_event);
    elementClass->change_state = GST_DEBUG_FUNCPTR(gst_rialto_text_track_sink_change_state);
    elementClass->query = GST_DEBUG_FUNCPTR(gst_rialto_text_track_sink_query);

    g_object_class_install_property(gobjectClass, PROP_MUTE,
                                    g_param_spec_boolean("mute", "Mute", "Mute subtitles", FALSE, G_PARAM_READWRITE));

    g_object_class_install_property(gobjectClass, PROP_TEXT_TRACK_IDENTIFIER,
                                    g_param_spec_string("text-track-identifier", "Text Track Identifier",
                                                        "Identifier of text track", nullptr,
                                                        GParamFlags(G_PARAM_READWRITE)));

    g_object_class_install_property(gobjectClass, PROP_VIDEO_DECODER,
                                    g_param_spec_uint64("video-decoder", "Video Decoder", "Video Decoder", 0,
                                                        G_MAXUINT64, 0, GParamFlags(G_PARAM_WRITABLE)));

    g_object_class_install_property(gobjectClass, PROP_POSITION,
                                    g_param_spec_uint64("position", "Position", "Position", 0, G_MAXUINT64, 0,
                                                        GParamFlags(G_PARAM_READWRITE)));

    g_object_class_install_property(gobjectClass, PROP_OFFSET,
                                    g_param_spec_uint64("offset", "Offset", "Offset", 0, G_MAXUINT64, 0,
                                                        GParamFlags(G_PARAM_WRITABLE)));

    gst_element_class_add_pad_template(elementClass, gst_static_pad_template_get(&sinkTemplate));
    gst_element_class_set_static_metadata(elementClass, "Rialto TextTrack Sink", "Sink/Parser/Subtitle",
                                          "Rialto TextTrack Sink", "SKY");
}

static void gst_rialto_text_track_sink_init(GstRialtoTextTrackSink *self) // NOLINT(build/function_format)
{
    GstRialtoTextTrackSinkPrivate *priv =
        reinterpret_cast<GstRialtoTextTrackSinkPrivate *>(gst_rialto_text_track_sink_get_instance_private(self));

    self->priv = new (priv) GstRialtoTextTrackSinkPrivate();

    gst_base_sink_set_async_enabled(GST_BASE_SINK(self), FALSE);
}

static void gst_rialto_text_track_sink_finalize(GObject *object) // NOLINT(build/function_format)
{
    GstRialtoTextTrackSink *self = GST_RIALTO_TEXT_TRACK_SINK(object);
    GstRialtoTextTrackSinkPrivate *priv =
        reinterpret_cast<GstRialtoTextTrackSinkPrivate *>(gst_rialto_text_track_sink_get_instance_private(self));

    priv->~GstRialtoTextTrackSinkPrivate();

    GST_CALL_PARENT(G_OBJECT_CLASS, finalize, (object));
}

static gboolean gst_rialto_text_track_sink_start(GstBaseSink *sink) // NOLINT(build/function_format)
{
    const char *wayland_display = std::getenv("WAYLAND_DISPLAY");
    if (!wayland_display)
    {
        GST_ERROR_OBJECT(sink, "Failed to get WAYLAND_DISPLAY env variable");
        return false;
    }

    std::string display{wayland_display};
    GstRialtoTextTrackSink *self = GST_RIALTO_TEXT_TRACK_SINK(sink);
    try
    {
        self->priv->m_textTrackSession =
            firebolt::rialto::server::ITextTrackSessionFactory::getFactory().createTextTrackSession(display);
    }
    catch (const std::exception &e)
    {
        GST_ERROR_OBJECT(sink, "Failed to create TextTrackSession. Reason '%s'", e.what());
        return false;
    }

    GST_INFO_OBJECT(sink, "Successfully started TextTrack sink");
    return true;
}

static gboolean gst_rialto_text_track_sink_stop(GstBaseSink *sink) // NOLINT(build/function_format)
{
    GstRialtoTextTrackSink *self = GST_RIALTO_TEXT_TRACK_SINK(sink);
    self->priv->m_textTrackSession.reset();

    GST_INFO_OBJECT(sink, "Successfully stopped TextTrack sink");
    return true;
}

static GstFlowReturn gst_rialto_text_track_sink_render(GstBaseSink *sink, GstBuffer *buffer) // NOLINT(build/function_format)
{
    GstRialtoTextTrackSink *textTrackSink = GST_RIALTO_TEXT_TRACK_SINK(sink);

    GstMapInfo info;
    if (gst_buffer_map(buffer, &info, GST_MAP_READ))
    {
        std::string data(reinterpret_cast<char *>(info.data), info.size);
        int64_t displayOffset{0};
        if (GST_BUFFER_OFFSET_NONE != GST_BUFFER_OFFSET(buffer))
        {
            displayOffset = static_cast<int64_t>(GST_BUFFER_OFFSET(buffer));
        }
        textTrackSink->priv->m_textTrackSession->sendData(data, 0 - displayOffset);

        gst_buffer_unmap(buffer, &info);
    }
    else
    {
        GST_ERROR_OBJECT(textTrackSink, "Failed to map buffer");
        return GST_FLOW_ERROR;
    }

    return GST_FLOW_OK;
}

static gboolean gst_rialto_text_track_sink_set_position(GstRialtoTextTrackSink *textTrackSink) // NOLINT(build/function_format)
{
    if (!textTrackSink->priv->m_textTrackSession)
    {
        GST_ERROR_OBJECT(textTrackSink, "Session is NULL");
        return FALSE;
    }

    uint64_t positionWithOffset = textTrackSink->priv->m_position.value_or(0) + textTrackSink->priv->m_offset.value_or(0);

    GST_DEBUG_OBJECT(textTrackSink,
                     "Setting position to %" GST_TIME_FORMAT " (pts %" GST_TIME_FORMAT ", offset %" GST_TIME_FORMAT ")",
                     GST_TIME_ARGS(positionWithOffset), GST_TIME_ARGS(textTrackSink->priv->m_position.value_or(0)),
                     GST_TIME_ARGS(textTrackSink->priv->m_offset.value_or(0)));

    textTrackSink->priv->m_textTrackSession->setPosition(positionWithOffset / GST_MSECOND);
    return TRUE;
}

static gboolean gst_rialto_text_track_sink_set_caps(GstBaseSink *sink, GstCaps *caps) // NOLINT(build/function_format)
{
    GST_INFO_OBJECT(sink, "Setting caps %" GST_PTR_FORMAT, caps);
    GstRialtoTextTrackSink *textTrackSink = GST_RIALTO_TEXT_TRACK_SINK(sink);

    GstStructure *structure = gst_caps_get_structure(caps, 0);
    const gchar *mimeName = gst_structure_get_name(structure);

    if (g_str_has_prefix(mimeName, "text/vtt") || g_str_has_prefix(mimeName, "application/x-subtitle-vtt"))
    {
        GST_INFO_OBJECT(sink, "Setting session to WebVTT");
        textTrackSink->priv->m_textTrackSession->setSessionWebVTTSelection();
    }
    else if (g_str_has_prefix(mimeName, "application/ttml+xml"))
    {
        GST_INFO_OBJECT(sink, "Setting session to TTML");
        textTrackSink->priv->m_textTrackSession->setSessionTTMLSelection();
    }
    else if (g_str_has_prefix(mimeName, "subtitle/x-subtitle-cc"))
    {
        GST_INFO_OBJECT(sink, "Setting session to CC");
        std::string identifier = "CC1";
        if (textTrackSink->priv->m_textTrackIdentifier.empty())
        {
            GST_WARNING_OBJECT(sink, "No text track identifier set, defaulting to %s", identifier.c_str());
        }
        else
        {
            identifier = textTrackSink->priv->m_textTrackIdentifier;
        }
        textTrackSink->priv->m_textTrackSession->setSessionCCSelection(identifier);

        if (textTrackSink->priv->m_videoDecoderIdentifier)
        {
            textTrackSink->priv->m_textTrackSession->associateVideoDecoder(textTrackSink->priv->m_videoDecoderIdentifier);
        }
    }
    else
    {
        GST_ERROR_OBJECT(sink, "Invalid mime name '%s'", mimeName);
        return FALSE;
    }

    textTrackSink->priv->m_textTrackSession->mute(textTrackSink->priv->m_isMuted);

    std::unique_lock lock{textTrackSink->priv->m_mutex};
    textTrackSink->priv->m_capsSet = true;
    bool wasAnyQueued = textTrackSink->priv->m_queuedPosition.has_value() ||
                        textTrackSink->priv->m_queuedOffset.has_value();
    if (textTrackSink->priv->m_queuedPosition.has_value())
    {
        textTrackSink->priv->m_position = textTrackSink->priv->m_queuedPosition;
        textTrackSink->priv->m_queuedPosition.reset();
    }
    if (textTrackSink->priv->m_queuedOffset.has_value())
    {
        textTrackSink->priv->m_offset = textTrackSink->priv->m_queuedOffset;
        textTrackSink->priv->m_queuedOffset.reset();
    }
    if (wasAnyQueued)
    {
        gst_rialto_text_track_sink_set_position(textTrackSink);
    }

    return TRUE;
}

static gboolean gst_rialto_text_track_sink_event(GstBaseSink *sink, GstEvent *event) // NOLINT(build/function_format)
{
    GstRialtoTextTrackSink *textTrackSink = GST_RIALTO_TEXT_TRACK_SINK(sink);
    GST_DEBUG_OBJECT(textTrackSink, "handling event %" GST_PTR_FORMAT, event);

    switch (GST_EVENT_TYPE(event))
    {
    case GST_EVENT_FLUSH_START:
    {
        if (!textTrackSink->priv->m_textTrackSession->resetSession(textTrackSink->priv->m_isMuted))
        {
            GST_ERROR_OBJECT(textTrackSink, "Failed to reset TextTrack session");
        }
        break;
    }
    case GST_EVENT_FLUSH_STOP:
    {
        break;
    }
    case GST_EVENT_CUSTOM_DOWNSTREAM:
    case GST_EVENT_CUSTOM_DOWNSTREAM_OOB:
    {
        if (gst_event_has_name(event, "current-pts"))
        {
            uint64_t pts = 0;
            const GstStructure *structure = gst_event_get_structure(event);
            if (structure)
            {
                if (gst_structure_get_uint64(structure, "pts", &pts))
                {
                    if (pts == GST_CLOCK_TIME_NONE)
                    {
                        GST_ERROR_OBJECT(textTrackSink, "Invalid PTS value");
                        return FALSE;
                    }

                    std::unique_lock lock{textTrackSink->priv->m_mutex};
                    textTrackSink->priv->m_position = pts;

                    gst_rialto_text_track_sink_set_position(textTrackSink);
                }
                else
                {
                    GST_ERROR_OBJECT(textTrackSink, "Failed to get PTS from structure");
                    return FALSE;
                }
            }
            else
            {
                GST_ERROR_OBJECT(textTrackSink, "Failed to get structure from event");
                return FALSE;
            }
        }
        break;
    }
    default:
    {
        break;
    }
    }

    return GST_BASE_SINK_CLASS(parent_class)->event(sink, event);
}

static GstStateChangeReturn
gst_rialto_text_track_sink_change_state(GstElement *element, GstStateChange transition) // NOLINT(build/function_format)
{
    GstRialtoTextTrackSink *textTrackSink = GST_RIALTO_TEXT_TRACK_SINK(element);

    GstState current_state = GST_STATE_TRANSITION_CURRENT(transition);
    GstState next_state = GST_STATE_TRANSITION_NEXT(transition);
    GST_INFO_OBJECT(textTrackSink, "State change: (%s) -> (%s)", gst_element_state_get_name(current_state),
                    gst_element_state_get_name(next_state));

    switch (transition)
    {
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
    {
        if (!textTrackSink->priv->m_textTrackSession->play())
        {
            GST_ERROR_OBJECT(textTrackSink, "Failed to play textTrack session");
            return GST_STATE_CHANGE_FAILURE;
        }
        break;
    }
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
    {
        if (!textTrackSink->priv->m_textTrackSession->pause())
        {
            GST_ERROR_OBJECT(textTrackSink, "Failed to pause textTrack session");
            return GST_STATE_CHANGE_FAILURE;
        }

        break;
    }
    default:
        break;
    }

    GstStateChangeReturn result = GST_ELEMENT_CLASS(parent_class)->change_state(element, transition);
    if (G_UNLIKELY(result == GST_STATE_CHANGE_FAILURE))
    {
        GST_WARNING_OBJECT(textTrackSink, "State change failed");
        return result;
    }

    return GST_STATE_CHANGE_SUCCESS;
}

static void gst_rialto_text_track_sink_get_property(GObject *object, guint propId, // NOLINT(build/function_format)
                                                    GValue *value, GParamSpec *pspec)
{
    GstRialtoTextTrackSink *textTrackSink = GST_RIALTO_TEXT_TRACK_SINK(object);
    if (!textTrackSink)
    {
        GST_ERROR_OBJECT(textTrackSink, "Sink not initalised");
        return;
    }
    GstRialtoTextTrackSinkPrivate *priv = textTrackSink->priv;

    switch (propId)
    {
    case PROP_MUTE:
    {
        g_value_set_boolean(value, priv->m_isMuted.load());
        break;
    }
    case PROP_TEXT_TRACK_IDENTIFIER:
    {
        g_value_set_string(value, priv->m_textTrackIdentifier.c_str());
        break;
    }
    case PROP_POSITION:
    {
        // Thunder ITextTrack does not provide getPosition API so we are unable to determine current position
        g_value_set_uint64(value, GST_CLOCK_TIME_NONE);
        break;
    }
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, pspec);
        break;
    }
}

static void gst_rialto_text_track_sink_set_property(GObject *object, guint propId, // NOLINT(build/function_format)
                                                    const GValue *value, GParamSpec *pspec)
{
    GstRialtoTextTrackSink *textTrackSink = GST_RIALTO_TEXT_TRACK_SINK(object);
    if (!textTrackSink)
    {
        GST_ERROR_OBJECT(textTrackSink, "Sink not initalised");
        return;
    }
    GstRialtoTextTrackSinkPrivate *priv = textTrackSink->priv;

    switch (propId)
    {
    case PROP_MUTE:
    {
        priv->m_isMuted = g_value_get_boolean(value);
        if (priv->m_textTrackSession)
        {
            priv->m_textTrackSession->mute(priv->m_isMuted);
        }
        break;
    }
    case PROP_TEXT_TRACK_IDENTIFIER:
    {
        priv->m_textTrackIdentifier = g_value_get_string(value);
        if (priv->m_textTrackSession && priv->m_textTrackSession->isClosedCaptions())
        {
            priv->m_textTrackSession->setSessionCCSelection(priv->m_textTrackIdentifier);
        }

        break;
    }
    case PROP_VIDEO_DECODER:
    {
        priv->m_videoDecoderIdentifier = g_value_get_uint64(value);
        if (priv->m_textTrackSession && priv->m_textTrackSession->isClosedCaptions())
        {
            priv->m_textTrackSession->associateVideoDecoder(priv->m_videoDecoderIdentifier);
        }

        break;
    }
    case PROP_POSITION:
    {
        guint64 position = g_value_get_uint64(value);
        std::unique_lock lock{priv->m_mutex};
        if (priv->m_textTrackSession && priv->m_capsSet)
        {
            priv->m_position = position;
            gst_rialto_text_track_sink_set_position(textTrackSink);
        }
        else
        {
            priv->m_queuedPosition = position;
        }
        break;
    }
    case PROP_OFFSET:
    {
        uint64_t offset = g_value_get_uint64(value);
        std::unique_lock lock{priv->m_mutex};
        if (priv->m_textTrackSession && priv->m_capsSet)
        {
            priv->m_offset = offset;
            gst_rialto_text_track_sink_set_position(textTrackSink);
        }
        else
        {
            priv->m_queuedOffset = offset;
        }
        break;
    }

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, pspec);
        break;
    }
}

static gboolean gst_rialto_text_track_sink_query(GstElement *element, GstQuery *query) // NOLINT(build/function_format)
{
    GstRialtoTextTrackSink *sink = GST_RIALTO_TEXT_TRACK_SINK(element);
    GST_DEBUG_OBJECT(sink, "handling query '%s'", GST_QUERY_TYPE_NAME(query));
    switch (GST_QUERY_TYPE(query))
    {
    case GST_QUERY_POSITION:
    {
        GstFormat fmt;
        gst_query_parse_position(query, &fmt, NULL);
        switch (fmt)
        {
        case GST_FORMAT_TIME:
        {
            // GST_CLOCK_TIME_NONE has to be returned here, because otherwise whole pipeline returns incorrect position
            gst_query_set_position(query, fmt, GST_CLOCK_TIME_NONE);
            break;
        }
        default:
            break;
        }
        return TRUE;
    }
    default:
        break;
    }
    GstElement *parent = GST_ELEMENT(&sink->parent);
    return GST_ELEMENT_CLASS(parent_class)->query(parent, query);
}

namespace firebolt::rialto::server
{
std::shared_ptr<IGstTextTrackSinkFactory> IGstTextTrackSinkFactory::createFactory()
{
    std::shared_ptr<IGstTextTrackSinkFactory> factory;

    try
    {
        factory = std::make_shared<GstTextTrackSinkFactory>();
    }
    catch (const std::exception &e)
    {
        RIALTO_SERVER_LOG_ERROR("Failed to create the textTrackSink element factory, reason: %s", e.what());
    }

    return factory;
}

GstElement *GstTextTrackSinkFactory::createGstTextTrackSink() const
{
    GstElement *elem = GST_ELEMENT(g_object_new(GST_RIALTO_TEXT_TRACK_SINK_TYPE, nullptr));

    return elem;
}

}; // namespace firebolt::rialto::server
