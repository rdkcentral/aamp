/*
 * Copyright (C) 2022 RDK Management
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
#include <gst/base/gstbasesink.h>
#include "gstsubtecsink.h"
#include <cstring>
#include <array>

GST_DEBUG_CATEGORY_STATIC (gst_subtecsink_debug_category);
#define GST_CAT_DEFAULT gst_subtecsink_debug_category

static GstBaseSinkClass* parentClass = nullptr;

/* prototypes */
static void gst_subtecsink_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);
static void gst_subtecsink_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);
static void gst_subtecsink_dispose (GObject * object);
static void gst_subtecsink_finalize (GObject * object);

static void set_mute(GstSubtecSink *subtecsink);
static void set_attribute(GstSubtecSink *subtecsink);

static gboolean gst_subtecsink_set_caps (GstBaseSink * sink, GstCaps * caps);
static gboolean gst_subtecsink_start (GstBaseSink * sink);
static gboolean gst_subtecsink_stop (GstBaseSink * sink);
static gboolean gst_subtecsink_query (GstBaseSink * sink, GstQuery * query);
static gboolean gst_subtecsink_event (GstBaseSink * sink, GstEvent * event);
static GstFlowReturn gst_subtecsink_render (GstBaseSink * sink,
    GstBuffer * buffer);
static GstStateChangeReturn gst_subtecsink_change_state(GstElement *element, 
    GstStateChange transition);
static GstFlowReturn gst_subtecsink_prepare (GstBaseSink * sink,
    GstBuffer * buffer);
static GstFlowReturn gst_subtecsink_preroll (GstBaseSink * sink,
    GstBuffer * buffer);

enum
{
  PROP_0,
  PROP_MUTE,
  PROP_NO_EOS,
  PROP_SUBTEC_SOCKET,
  PROP_PTS_OFFSET,
  PROP_ATTRIBUTE_VALUES
};

enum
{
  FONT_COLOR_INDEX = 0,
  BACKGROUND_COLOR_INDEX,
  FONT_OPACITY_INDEX,
  BACKGROUND_OPACITY_INDEX,
  FONT_STYLE_INDEX,
  FONT_SIZE_INDEX,
  FONT_ITALIC_INDEX,
  FONT_UNDERLINE_INDEX,
  BORDER_TYPE_INDEX,
  BORDER_COLOR_INDEX,
  WIN_COLOR_INDEX,
  WIN_OPACITY_INDEX,
  EDGE_TYPE_INDEX,
  EDGE_COLOR_INDEX
};

static GstStaticPadTemplate gst_subtecsink_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/ttml+xml; text/vtt")
    );


G_DEFINE_TYPE_WITH_CODE (GstSubtecSink, gst_subtecsink, GST_TYPE_BASE_SINK,
  GST_DEBUG_CATEGORY_INIT (gst_subtecsink_debug_category, "subtecsink", 0,
  "debug category for subtecsink element"));

static void
gst_subtecsink_class_init (GstSubtecSinkClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstBaseSinkClass *base_sink_class = GST_BASE_SINK_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS(klass);

  parentClass = GST_BASE_SINK_CLASS(g_type_class_peek_parent(klass));

  GST_LOG("gst_subtecsink_class_init");

  gst_element_class_add_static_pad_template (GST_ELEMENT_CLASS(klass),
      &gst_subtecsink_sink_template);

  gst_element_class_set_static_metadata (GST_ELEMENT_CLASS(klass),
      "OOB Subtec data sink", "Sink/Parser/Subtitle", "Packs TTML or WebVTT data into SubTtxRend APP suitable packets", "Comcast");


  gobject_class->set_property = gst_subtecsink_set_property;
  gobject_class->get_property = gst_subtecsink_get_property;
  gobject_class->dispose = gst_subtecsink_dispose;
  gobject_class->finalize = gst_subtecsink_finalize;

  g_object_class_install_property(gobject_class,
                                  PROP_MUTE,
                                  g_param_spec_boolean("mute", "Mute", "Mutes the subtitles",
                                  FALSE,
                                  (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property(gobject_class,
                                  PROP_NO_EOS,
                                  g_param_spec_boolean("no-eos", "No EOS", "Eats the EOS and stops the stream exiting",
                                  FALSE,
                                  (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property(gobject_class,
                                  PROP_SUBTEC_SOCKET,
                                  g_param_spec_string("subtec-socket", "Subtec socket", "Alternative subtec socket (default /var/run/subttx/pes_data_main)",
                                  NULL,
                                  (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property(gobject_class,
                                  PROP_PTS_OFFSET,
                                  g_param_spec_uint64("pts-offset", "PTS offset", "PTS offset for mpeg-2 ts HLS streams",
                                  0, G_MAXUINT64, 0,
                                  (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property(gobject_class,
                                  PROP_ATTRIBUTE_VALUES,
                                  g_param_spec_boxed("attribute-values", "Attribute values", "GstStructure specifying attributes",
                                  GST_TYPE_STRUCTURE,
                                  (GParamFlags)(G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS)));


  base_sink_class->set_caps = GST_DEBUG_FUNCPTR (gst_subtecsink_set_caps);
  base_sink_class->start = GST_DEBUG_FUNCPTR (gst_subtecsink_start);
  base_sink_class->stop = GST_DEBUG_FUNCPTR (gst_subtecsink_stop);
  base_sink_class->query = GST_DEBUG_FUNCPTR (gst_subtecsink_query);
  base_sink_class->event = GST_DEBUG_FUNCPTR (gst_subtecsink_event);
  base_sink_class->render = GST_DEBUG_FUNCPTR (gst_subtecsink_render);
  base_sink_class->prepare = GST_DEBUG_FUNCPTR (gst_subtecsink_prepare);
  base_sink_class->preroll = GST_DEBUG_FUNCPTR (gst_subtecsink_preroll);

  element_class->change_state = GST_DEBUG_FUNCPTR (gst_subtecsink_change_state);
}

static GstFlowReturn
gst_subtecsink_prepare (GstBaseSink * sink, GstBuffer * buffer)
{
  GstSubtecSink *subtecsink = GST_SUBTECSINK (sink);

  GST_DEBUG_OBJECT (subtecsink, "prepare PTS %" GST_TIME_FORMAT, GST_TIME_ARGS(GST_BUFFER_PTS(buffer)));

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_subtecsink_preroll (GstBaseSink * sink, GstBuffer * buffer)
{
  GstSubtecSink *subtecsink = GST_SUBTECSINK (sink);

  GST_DEBUG_OBJECT (subtecsink, "preroll PTS %" GST_TIME_FORMAT, GST_TIME_ARGS(GST_BUFFER_PTS(buffer)));

  return GST_FLOW_OK;
}


GstStateChangeReturn gst_subtecsink_change_state(GstElement *element, GstStateChange transition)
{
    GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
    GstSubtecSink *subtecsink = GST_SUBTECSINK(element);

    GST_DEBUG_OBJECT(subtecsink, "change_state 0x%X", transition);

    switch (transition)
    {
        case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
        {
            GST_DEBUG_OBJECT(subtecsink, "changing state to playing");
            if (subtecsink->m_channel)
            {
              subtecsink->m_channel->SendResumePacket();

              if (subtecsink->m_mute)
                subtecsink->m_channel->SendMutePacket();
              else
                subtecsink->m_channel->SendUnmutePacket();

              set_attribute(subtecsink);
            }
            break;
        }
        default:
            break;
    }
    ret = GST_ELEMENT_CLASS (parentClass)->change_state(element, transition);
    if (ret == GST_STATE_CHANGE_FAILURE)
    {
        return ret;
    }

    switch (transition)
    {
        case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
        {
            GST_DEBUG_OBJECT(subtecsink, "changing state to paused");
	          if (subtecsink->m_channel) 
            {
              subtecsink->m_channel->SendMutePacket();
              subtecsink->m_channel->SendPausePacket();
            }
            break;
        }
        default:
            break;
    }

    return ret;
}

static void
gst_subtecsink_init (GstSubtecSink *subtecsink)
{
  GST_DEBUG_OBJECT(subtecsink, "init");

  gst_base_sink_set_async_enabled(GST_BASE_SINK(subtecsink), FALSE);

  subtecsink->m_attribute_mask = 0;
  subtecsink->m_attribute_values = {0};
  subtecsink->m_pts_offset = 0;
}

void
set_mute(GstSubtecSink *subtecsink)
{
  if (subtecsink->m_mute)
    if (subtecsink->m_channel) subtecsink->m_channel->SendMutePacket(); else GST_WARNING_OBJECT (subtecsink, "Mute failed due to NULL channel");
  else
    if (subtecsink->m_channel) subtecsink->m_channel->SendUnmutePacket(); else GST_WARNING_OBJECT (subtecsink, "Unmute failed due to NULL channel");
}

void
set_attribute(GstSubtecSink *subtecsink)
{
  if (subtecsink->m_channel)
  {
    subtecsink->m_channel->SendCCSetAttributePacket(0, subtecsink->m_attribute_mask, subtecsink->m_attribute_values);
  }
  else
  {
    GST_WARNING_OBJECT (subtecsink, "SetAttribute failed due to NULL channel");
  }
}

void
gst_subtecsink_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstSubtecSink *subtecsink = GST_SUBTECSINK (object);

  GST_DEBUG_OBJECT (subtecsink, "set_property");

  switch (property_id) {
    case PROP_MUTE:
    {
        auto mute = g_value_get_boolean (value);
        GST_TRACE_OBJECT(subtecsink, "%s current mute %s setting mute to %s", __func__, subtecsink->m_mute ? "true" : "false", mute ? "true" : "false");
        
        //Pause senderthread
        subtecsink->m_mute = mute;
        set_mute(subtecsink);
    }
      break;
    case PROP_NO_EOS:
    {
      auto no_eos = g_value_get_boolean(value);
      GST_TRACE_OBJECT(subtecsink, "%s setting no_eos to %s", __func__, no_eos ? "true" : "false");
      subtecsink->m_no_eos = no_eos;
    }
      break;
    case PROP_SUBTEC_SOCKET:
    {
      auto subtec_socket = g_value_get_string(value);
      GST_TRACE_OBJECT(subtecsink, "%s setting subtec_socket to %s", __func__, subtec_socket);
      subtecsink->m_subtec_socket = subtec_socket;
    }
      break;
    case PROP_PTS_OFFSET:
    {
      auto pts_offset = g_value_get_uint64(value);
      GST_TRACE_OBJECT(subtecsink, "%s setting pts_offset to %" G_GUINT64_FORMAT, __func__, pts_offset);
      subtecsink->m_pts_offset = pts_offset;
      subtecsink->m_send_timestamp = true;
    }
      break;
    case PROP_ATTRIBUTE_VALUES:
    {
      const GstStructure *attributevalues = gst_value_get_structure(value);
      if (attributevalues != NULL)
      {
	gst_structure_get_uint(attributevalues, "font_color", &subtecsink->m_attribute_values[FONT_COLOR_INDEX]);
	gst_structure_get_uint(attributevalues, "background_color", &subtecsink->m_attribute_values[BACKGROUND_COLOR_INDEX]);
	gst_structure_get_uint(attributevalues, "font_opacity", &subtecsink->m_attribute_values[FONT_OPACITY_INDEX]);
	gst_structure_get_uint(attributevalues, "background_opacity", &subtecsink->m_attribute_values[BACKGROUND_OPACITY_INDEX]);
	gst_structure_get_uint(attributevalues, "font_style", &subtecsink->m_attribute_values[FONT_STYLE_INDEX]);
	gst_structure_get_uint(attributevalues, "font_size", &subtecsink->m_attribute_values[FONT_SIZE_INDEX]);
	gst_structure_get_uint(attributevalues, "window_color", &subtecsink->m_attribute_values[WIN_COLOR_INDEX]);
	gst_structure_get_uint(attributevalues, "window_opacity", &subtecsink->m_attribute_values[WIN_OPACITY_INDEX]);
	gst_structure_get_uint(attributevalues, "edge_type", &subtecsink->m_attribute_values[EDGE_TYPE_INDEX]);
	gst_structure_get_uint(attributevalues, "edge_color", &subtecsink->m_attribute_values[EDGE_COLOR_INDEX]);
	gst_structure_get_uint(attributevalues, "attribute_mask", &subtecsink->m_attribute_mask);

	set_attribute(subtecsink);
      }
      else
      {
	      GST_ERROR_OBJECT(subtecsink, "%s attribute value NULL", __func__);
      }
    }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_subtecsink_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GstSubtecSink *subtecsink = GST_SUBTECSINK (object);

  GST_DEBUG_OBJECT (subtecsink, "get_property %d", property_id);

  switch (property_id) {
    case PROP_MUTE:
      g_value_set_boolean (value, subtecsink->m_mute);
      break;
    case PROP_NO_EOS:
      g_value_set_boolean (value, subtecsink->m_no_eos);
      break;
    case PROP_SUBTEC_SOCKET:
      g_value_set_string (value, subtecsink->m_subtec_socket.c_str());
      break;
    case PROP_PTS_OFFSET:
      g_value_set_uint64 (value, subtecsink->m_pts_offset);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_subtecsink_dispose (GObject * object)
{
  GstSubtecSink *subtecsink = GST_SUBTECSINK (object);

  GST_DEBUG_OBJECT (subtecsink, "dispose");

	if (subtecsink->m_channel) subtecsink->m_channel->SendResetAllPacket();

  G_OBJECT_CLASS (gst_subtecsink_parent_class)->dispose (object);
}

void
gst_subtecsink_finalize (GObject * object)
{
  GstSubtecSink *subtecsink = GST_SUBTECSINK (object);

  GST_DEBUG_OBJECT (subtecsink, "finalize");

  G_OBJECT_CLASS (gst_subtecsink_parent_class)->finalize (object);
}

/* notify subclass of new caps */
static gboolean
gst_subtecsink_set_caps (GstBaseSink * sink, GstCaps * caps)
{
  GstSubtecSink *subtecsink = GST_SUBTECSINK (sink);

  GST_DEBUG_OBJECT (subtecsink, "set_caps");

  const GstStructure *s = gst_caps_get_structure (caps, 0);
  const gchar* caps_name = gst_structure_get_name(s);

  GST_DEBUG_OBJECT (subtecsink, "set_caps name %s", caps_name);

  if (!strcmp("text/vtt", caps_name) || !strcmp("application/x-subtitle-vtt", caps_name))
    subtecsink->m_channel = SubtecChannel::SubtecChannelFactory(SubtecChannel::ChannelType::WEBVTT);
  else if (!strcmp("application/ttml+xml", caps_name))
    subtecsink->m_channel = SubtecChannel::SubtecChannelFactory(SubtecChannel::ChannelType::TTML);
  else
  {
    GST_ERROR_OBJECT(subtecsink, "Unknown caps - cannot create subtec channel");
    return FALSE;
  }

	subtecsink->m_channel->SendResetAllPacket();
  subtecsink->m_channel->SendSelectionPacket(1920, 1080);
  set_mute(subtecsink);
  set_attribute(subtecsink);

  return TRUE;
}


static gboolean
gst_subtecsink_start (GstBaseSink * sink)
{
  GstSubtecSink *subtecsink = GST_SUBTECSINK (sink);

  GST_DEBUG_OBJECT (subtecsink, "start");
  bool ret = false;
  
  if (subtecsink->m_subtec_socket.empty())
    ret = SubtecChannel::InitComms();
  else
    ret = SubtecChannel::InitComms(subtecsink->m_subtec_socket.c_str());

	if (!ret)
	{
		GST_WARNING_OBJECT (subtecsink, "Init failed - subtitle parsing disabled");
	}

  subtecsink->m_send_timestamp = true;

  return ret;
}

static gboolean
gst_subtecsink_stop (GstBaseSink * sink)
{
  GstSubtecSink *subtecsink = GST_SUBTECSINK (sink);
	if (subtecsink->m_channel) 
  {
    subtecsink->m_channel->SendResetChannelPacket();
    subtecsink->m_channel->SendResetAllPacket();
  }

  GST_DEBUG_OBJECT (subtecsink, "stop");

  return TRUE;
}

static gboolean
gst_subtecsink_query (GstBaseSink * sink, GstQuery * query)
{
  GstSubtecSink *subtecsink = GST_SUBTECSINK (sink);

  GST_DEBUG_OBJECT (subtecsink, "query %s", GST_QUERY_TYPE_NAME(query));

  return GST_BASE_SINK_CLASS(G_OBJECT_CLASS (gst_subtecsink_parent_class))->query(sink, query);
}

/**
 * @brief Get the subtec timestamp in ms - applies the offset sent from upstream if applicable
 * 
 * @param sink 
 * @param pts 
 * @param offset 
 * @return std::uint64_t 
 */
static std::uint64_t get_timestamp_ms(GstBaseSink * sink, GstClockTime pts, GstClockTime offset)
{
  GstSubtecSink *subtecsink = GST_SUBTECSINK (sink);
  
  //Get timestamp - if segment time > pts, this means we have skipped into the middle of the segment
  auto timestampMs = static_cast<std::uint64_t>(std::max(pts, sink->segment.time) / GST_MSECOND);

  GST_DEBUG_OBJECT (subtecsink, "offset %" G_GINT64_FORMAT " pts %" GST_TIME_FORMAT , offset, GST_TIME_ARGS(pts));

  // Offset -1 means no offset
  if (static_cast<gint64>(offset) > 0)
  {
    GST_DEBUG_OBJECT (subtecsink, "add %" GST_TIME_FORMAT " offset to %" GST_TIME_FORMAT " to give %" GST_TIME_FORMAT,
      GST_TIME_ARGS(offset), GST_TIME_ARGS(timestampMs*GST_MSECOND), GST_TIME_ARGS((timestampMs+offset)*GST_MSECOND));
    timestampMs += offset;
  }
  //For debug - this is what you get from filesrc when debugging using gst-launch1.0
  else if (static_cast<gint64>(pts) < 0)
  {
    GST_DEBUG_OBJECT (subtecsink, "offset is -1 and pts is -1 - setting time to 0");
    timestampMs = 0;
  }

  return timestampMs;
}

static gboolean
gst_subtecsink_event (GstBaseSink * sink, GstEvent * event)
{
  GstSubtecSink *subtecsink = GST_SUBTECSINK (sink);
  gboolean res = TRUE;
  GST_TRACE_OBJECT(subtecsink, "%s eventType %s", __func__, gst_event_type_get_name(GST_EVENT_TYPE(event)));

  switch(GST_EVENT_TYPE(event))
  {
    case GST_EVENT_FLUSH_START:
      if (subtecsink->m_channel) subtecsink->m_channel->SendResetChannelPacket();
      break;
    case GST_EVENT_FLUSH_STOP:
      if (subtecsink->m_channel) subtecsink->m_channel->SendSelectionPacket(1920, 1080);
      subtecsink->m_send_timestamp = true;
      set_mute(subtecsink);
      set_attribute(subtecsink);
      break;
    case GST_EVENT_SEGMENT:
    {
      const GstSegment *segment;
      gst_event_parse_segment(event, &segment);
      subtecsink->m_segmentstart = segment->start;
      GST_DEBUG_OBJECT(subtecsink, "segment.start: %" G_GUINT64_FORMAT, segment->start);
    }
      break;
    case GST_EVENT_EOS:
      //Useful for debug with fakesrc
      if (subtecsink->m_no_eos)
      {
        GST_DEBUG_OBJECT(subtecsink, "Eating EOS (nomnom)");
        return TRUE;
      }
      else
      {
        GST_DEBUG_OBJECT(subtecsink, "Received EOS");
      }
      break;
    case GST_EVENT_CUSTOM_DOWNSTREAM:
    {
      const GstStructure *structure = gst_event_get_structure(event);
      if (gst_structure_has_name(structure, "sub_clock_sync"))
      {
        GstClockTime pts = g_value_get_uint64(gst_structure_get_value(structure, "current-pts"));
        auto timestampMs = get_timestamp_ms(sink, pts, subtecsink->m_pts_offset);
        GST_DEBUG_OBJECT(subtecsink,
                        "%s generating timestamp %u",
                        __func__,
                        static_cast<std::uint32_t>(timestampMs));
        subtecsink->m_channel->SendTimestampPacket((timestampMs));
        subtecsink->m_send_timestamp = false;
      }
      gst_event_unref(event);
      res = TRUE;
      goto drop;
    }
    break;
    default:
    break;
  }

  GST_DEBUG_OBJECT (subtecsink, "event");

  res = GST_BASE_SINK_CLASS(G_OBJECT_CLASS (gst_subtecsink_parent_class))->event(sink, event);
drop:
  return res;
}

static GstFlowReturn
gst_subtecsink_render (GstBaseSink * sink, GstBuffer * buffer)
{
  GstSubtecSink *subtecsink = GST_SUBTECSINK (sink);

  GST_DEBUG_OBJECT (subtecsink, "render PTS %" GST_TIME_FORMAT, GST_TIME_ARGS(GST_BUFFER_PTS(buffer)));

  GstMapInfo map;
  std::vector<std::uint8_t> dataBuffer;

  if (gst_buffer_map(buffer, &map, GST_MAP_READ))
  {
      auto inputData = static_cast<uint8_t*>(map.data);
      auto inputSize = static_cast<uint32_t>(map.size);
      std::string instr(const_cast<const char*>(reinterpret_cast<char *>(map.data)), map.size);

      GST_TRACE("%s unpacking GstBuffer size: %d\n", __func__, inputSize);

      for (std::uint32_t i = 0; i < inputSize; i++)
      {
          dataBuffer.push_back(inputData[i]);
      }

      gst_buffer_unmap(buffer, &map);
  }
  else
  {
      GST_WARNING("%s error unpacking GstBuffer!", __func__);
  }

  if (!dataBuffer.empty())
  {
    if (subtecsink->m_send_timestamp)
    {
      //On seek, segment "time" will be ahead of buffer PTS - otherwise just use PTS
      auto timestampMs = get_timestamp_ms(sink, subtecsink->m_segmentstart, subtecsink->m_pts_offset);

      GST_DEBUG_OBJECT(subtecsink,
                        "%s generating timestamp %u",
                        __func__,
                        static_cast<std::uint32_t>(timestampMs));

      subtecsink->m_channel->SendTimestampPacket((timestampMs));
      subtecsink->m_send_timestamp = false;
    }

    auto offset = static_cast<gint64>(GST_BUFFER_OFFSET(buffer));
    if (offset == -1) 
      offset = 0;
      
    GST_DEBUG_OBJECT (subtecsink, "sending data packet with offset %" G_GINT64_FORMAT " buffer %" G_GUINT64_FORMAT, offset, GST_BUFFER_OFFSET(buffer));
    subtecsink->m_channel->SendDataPacket(std::move(dataBuffer), 0 - offset);
  }

  return GST_FLOW_OK;
}

static gboolean
plugin_init (GstPlugin * plugin)
{

  return gst_element_register (plugin, "subtecsink", GST_RANK_PRIMARY,
      GST_TYPE_SUBTECSINK);
}

#ifndef VERSION
#define VERSION "0.1.0"
#endif
#ifndef PACKAGE
#define PACKAGE "GstreamerPlugins"
#endif
#ifndef PACKAGE_NAME
#define PACKAGE_NAME "GstreamerPlugins"
#endif
#ifndef GST_PACKAGE_ORIGIN
#define GST_PACKAGE_ORIGIN "http://comcast.com/"
#endif

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    subtecsink,
    "SubTtxRend text/ttml sink",
    plugin_init, VERSION, "LGPL", PACKAGE_NAME, GST_PACKAGE_ORIGIN)

