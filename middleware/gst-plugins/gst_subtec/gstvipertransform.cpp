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

/**
 * SECTION:element-gstvipertransform
 *
 * The vipertransform element performs the necessary transforms for legacy linear content.
 * These are needed to overcome limitations in the TTML delivery
 *
 */

#include <gst/gst.h>
#include <gst/base/gstbasetransform.h>
#include "gstvipertransform.h"

#include <stdlib.h>
#include <string.h>
#include <string>
#include <tuple>
#include <vector>

namespace { constexpr auto XmlStart = "<?xml"; }

GST_DEBUG_CATEGORY_STATIC (gst_vipertransform_debug_category);
#define GST_CAT_DEFAULT gst_vipertransform_debug_category

static GstBaseTransformClass* parentClass = nullptr;

/* prototypes */
static void gst_vipertransform_dispose (GObject * object);
static void gst_vipertransform_finalize (GObject * object);

static GstFlowReturn gst_vipertransform_transform (GstBaseTransform * trans,
	GstBuffer * inbuf, GstBuffer * outbuf);
static void gst_vipertransform_before_transform (GstBaseTransform * trans,
	GstBuffer * buf);
static gboolean gst_vipertransform_set_caps (GstBaseTransform * trans,
	GstCaps * incaps, GstCaps * outcaps);
static gboolean gst_vipertransform_event (GstBaseTransform * sink, GstEvent * event);

static GstStaticPadTemplate gst_vipertransform_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
	GST_PAD_SRC,
	GST_PAD_ALWAYS,
	GST_STATIC_CAPS("application/ttml+xml; text/vtt")
	);

static GstStaticPadTemplate gst_vipertransform_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
	GST_PAD_SINK,
	GST_PAD_ALWAYS,
	GST_STATIC_CAPS("application/ttml+xml; text/vtt")
	);


G_DEFINE_TYPE_WITH_CODE (GstViperTransform, gst_vipertransform, GST_TYPE_BASE_TRANSFORM,
  GST_DEBUG_CATEGORY_INIT (gst_vipertransform_debug_category,
  "vipertransform", 0,
  "debug category for vipertransform element"));

static void
gst_vipertransform_class_init (GstViperTransformClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstBaseTransformClass *base_transform_class = GST_BASE_TRANSFORM_CLASS (klass);

  parentClass = GST_BASE_TRANSFORM_CLASS(g_type_class_peek_parent(klass));

  gst_element_class_add_static_pad_template (GST_ELEMENT_CLASS(klass),
	  &gst_vipertransform_src_template);
  gst_element_class_add_static_pad_template (GST_ELEMENT_CLASS(klass),
	  &gst_vipertransform_sink_template);

  gst_element_class_set_static_metadata (GST_ELEMENT_CLASS(klass),
	  "vipertransform", "Formatter/Subtitle", "Transforms TTML buffers for broadcast", "Comcast");

  gobject_class->dispose = gst_vipertransform_dispose;
  gobject_class->finalize = gst_vipertransform_finalize;

  base_transform_class->set_caps = GST_DEBUG_FUNCPTR (gst_vipertransform_set_caps);
  base_transform_class->transform = GST_DEBUG_FUNCPTR (gst_vipertransform_transform);
  base_transform_class->before_transform = GST_DEBUG_FUNCPTR (gst_vipertransform_before_transform);
  base_transform_class->sink_event = GST_DEBUG_FUNCPTR (gst_vipertransform_event);
}


static gboolean gst_vipertransform_event (GstBaseTransform * trans, GstEvent * event)
{
  GstViperTransform *vipertransform = GST_VIPERTRANSFORM (trans);

  GST_TRACE_OBJECT(vipertransform, "%s eventType %s", __func__, gst_event_type_get_name(GST_EVENT_TYPE(event)));

  switch(GST_EVENT_TYPE(event))
  {
	case GST_EVENT_FLUSH_START:
		vipertransform->m_content_type = ViperContentType::UNKNOWN;
		vipertransform->m_linear_begin_offset = 0;
	  break;
	case GST_EVENT_FLUSH_STOP:
		vipertransform->m_content_type = ViperContentType::UNKNOWN;
		vipertransform->m_linear_begin_offset = 0;
	  break;
  }

  GST_DEBUG_OBJECT (vipertransform, "event");

  return GST_BASE_TRANSFORM_CLASS(G_OBJECT_CLASS (gst_vipertransform_parent_class))->sink_event(trans, event);
}

static void
gst_vipertransform_init (GstViperTransform *vipertransform)
{
  GST_DEBUG_OBJECT (vipertransform, "init");
  vipertransform->m_content_type = ViperContentType::UNKNOWN;
}

static gboolean gst_vipertransform_set_caps (GstBaseTransform * trans,
	GstCaps * incaps, GstCaps * outcaps)
{
  GstViperTransform *vipertransform = GST_VIPERTRANSFORM (trans);

  GST_DEBUG_OBJECT (vipertransform, "set_caps");

  const GstStructure *s = gst_caps_get_structure (incaps, 0);
  const gchar* caps_name = gst_structure_get_name(s);

  GST_DEBUG_OBJECT (vipertransform, "caps %s", caps_name);

  // If not TTML set to passthrough mode
  if (strcmp("application/ttml+xml", caps_name))
  {
	gst_base_transform_set_passthrough(trans, TRUE);
	GST_DEBUG_OBJECT (vipertransform, "Setting passthrough");
  }

  vipertransform->m_search_first_begin = true;
  vipertransform->m_linear_begin_offset = 0;

  return TRUE;
}

void
gst_vipertransform_dispose (GObject * object)
{
  GstViperTransform *vipertransform = GST_VIPERTRANSFORM (object);

  GST_DEBUG_OBJECT (vipertransform, "dispose");

  G_OBJECT_CLASS (gst_vipertransform_parent_class)->dispose (object);
}

void
gst_vipertransform_finalize (GObject * object)
{
  GstViperTransform *vipertransform = GST_VIPERTRANSFORM (object);

  GST_DEBUG_OBJECT (vipertransform, "finalize");

  G_OBJECT_CLASS (gst_vipertransform_parent_class)->finalize (object);
}

/**
 * @brief Convert a time string (HHHH:MM:SS.000) to milliseconds
 *
 * @param timeString
 * @return gint64
 */
gint64 convertTime(const std::string &timeString)
{
	gint64 retMs = 0;
	int hours, mins;
	float secs;

	if (std::sscanf(timeString.c_str(), "%d:%d:%f", &hours, &mins, &secs))
	{
		retMs += static_cast<gint64>(hours) * 3600;
		retMs += mins * 60;
		retMs *= 1000;
		retMs += static_cast<gint64>(secs * 1000.0);
	}

	return retMs;
}

/**
 * @brief Find a specific tag and return the contents, position and contents length
 *
 * @param instring
 * @param tag
 * @param start_pos
 * @return std::tuple<std::string, std::size_t, std::size_t>
 */
std::tuple<std::string, std::size_t, std::size_t> findTag(const std::string &instring, const std::string &tag, std::size_t start_pos)
{
	std::string tag_start{tag+"=\""};
	std::size_t pos = start_pos;

	pos = instring.find(tag_start, pos);

	if (pos == std::string::npos)
	return {"", std::string::npos, 0};

	pos += tag_start.length(); // Takes us to contents
	std::size_t endpos = instring.find_first_of('"', pos); // following "
	std::size_t len = endpos - pos;
	std::string tag_contents = instring.substr(pos, len);

	return {tag_contents, pos, len};
}


/**
 * @brief Search the TTML for the first begin="" tag. We will use this to calculate a timestamp offset
 * eg if the first begin tag is begin="458:34:12:123" then we take this and subtract the
 * buffer PTS ms to get an offset for the start of the stream
 * PTS 0          2          4
 * time|----------|--b----b--|-b------b-etc
 * ttml  empty     1st b=2:40
 *
 * 2:40 - 2s = 2:38 offset at the start of the stream
 *
 * We can then use this offset to sync the subtitles and audio PTS
 *
 * @param ss
 * @param firstBeginMs
 * @return true
 * @return false
 */
static bool findFirstBegin(const std::string &ttml, gint64 &firstBeginMs)
{
  std::size_t pos;
  std::string tag_contents;

  std::tie(tag_contents, pos, std::ignore) = findTag(ttml, "begin", 0);

  if (pos != std::string::npos)
  {
	firstBeginMs = convertTime(tag_contents);
	return true;
  }

  return false;
}

/**
 * @brief Find the offset in ms between the buffer PTS and the first "begin" tag in the TTML
 *
 * @param ttml
 * @param buf
 * @param offset_ms
 * @return true
 * @return false
 */
static bool find_offset_ms_from_pts(const std::string &ttml, GstBuffer *buf, gint64 &offset_ms)
{
  gint64 first_begin_ms = 0;

  if (findFirstBegin(ttml, first_begin_ms))
  {
	guint64 pts_ms = GST_BUFFER_PTS(buf) / GST_MSECOND;
	auto duration_ms = GST_BUFFER_DURATION(buf) / GST_MSECOND;
	offset_ms = first_begin_ms - pts_ms;
  }
  else
	return false;

  return true;
}

/**
 * @brief Get the restamp pts offset from the buffer
 *
 * @param buf
 * @return gint64
 */
static gint64 GetRestampPTSOffsetMS(GstBuffer * buf)
{
	return GST_BUFFER_OFFSET(buf);
}

/**
 * @brief Check if there are more than one xml docs in the current segment
 *
 * @param vipertransform
 * @param ttml
 * @return true
 * @return false
 */
static bool is_harmonic_uhd(GstViperTransform *vipertransform, const std::string &ttml)
{
  // Check for more than one <?xml> element (Harmonic UHD case)
  const char *xml_marker = XmlStart;
  auto first_marker = ttml.find(xml_marker);
  auto second_marker = ttml.find(xml_marker, first_marker+1);

  return (second_marker != std::string::npos);
}

/**
 * @brief Split Harmonic muxed buffers into a string vector
 * In the case of Harmonic linear content, the TTML docs are concatenated in a single segment
 * The timestamps are referenced to the start of the TTML doc, so the structure will be as follows:
 * <?xml ...>    <------First TTML doc
 * <tt ...>
 * <head>...</head>
 * <body>
 *   <p begin="00:00:00.000" end="00:00:01.000">
 *     <span...>Text from 0s - 1s</span>
 *   </p>
 * </body>
 * </tt>
 * <?xml ...>    <------Second TTML doc
 * <tt ...>
 * <head>...</head>
 * <body>
 *   <p begin="00:00:00.000" end="00:00:01.000">
 *     <span...>Text from 1s - 2s</span>
 *   </p>
 * </body>
 * </tt>
 *
 * @param vipertransform
 * @param ttml
 * @return true
 * @return false
 */
static std::vector<std::string> split_buffer(const std::string &ttml)
{
  std::vector<std::string> ttml_vec;
  std::size_t pos=0, start_pos=0, end_pos = 0;

  while ((start_pos = ttml.find(XmlStart, pos)) != std::string::npos)
  {
	end_pos = ttml.find(XmlStart, start_pos+1);
	auto sub = ttml.substr(start_pos, end_pos);

	ttml_vec.emplace_back(std::move(sub));
	pos = end_pos;
  }

  return ttml_vec;
}

/**
 * @brief Check if the incoming content needs an offset. If so, parse it from the TTML
 *
 * @param GstBaseTransform *trans
 * @param GstBuffer *buf
 */
static void gst_vipertransform_before_transform (GstBaseTransform * trans,
	GstBuffer * buf)
{
	GstViperTransform *vipertransform = GST_VIPERTRANSFORM (trans);

	GST_DEBUG_OBJECT (vipertransform, "before_transform PTS %" GST_TIME_FORMAT " mode %d", GST_TIME_ARGS(GST_BUFFER_PTS(buf)), (int)vipertransform->m_content_type);

	if (gst_base_transform_is_passthrough(trans))
	{
		GST_DEBUG_OBJECT (vipertransform, "Passthrough set");
		return;
	}

	//Parse TTML string from buffer
	std::string ttml;
	{
		GstMapInfo inmap;
		if (gst_buffer_map(buf, &inmap, (GstMapFlags)GST_MAP_READ))
		{
			GST_DEBUG_OBJECT (vipertransform, "map.size %lu", inmap.size);
			ttml.assign(reinterpret_cast<const char*>(inmap.data), inmap.size);
			gst_buffer_unmap(buf, &inmap);
		}
		else
		{
			GST_ERROR_OBJECT (vipertransform, "Could not open for reading");
			return;
		}
	}

	//If there is more than one segment in the buffer, just take the first one
	if (is_harmonic_uhd(vipertransform, ttml))
	{
		auto ttml_vec = split_buffer(ttml);
		if (!ttml_vec.empty())
		{
		ttml = ttml_vec.front();
		GST_DEBUG_OBJECT (vipertransform, "uhd[0] %s", ttml.c_str());
		}
	}

	gint64 first_begin_ms = 0;
	 //Find the gap between the PTS and the first "begin" tag

	if (findFirstBegin(ttml, first_begin_ms))
	{
		gint64 restamp_pts_offset_ms = GetRestampPTSOffsetMS(buf);
		gint64 offset_from_pts_ms = first_begin_ms - (GST_BUFFER_PTS(buf) / GST_MSECOND);

		GST_DEBUG_OBJECT(vipertransform, "first_begin_ms %" G_GINT64_FORMAT " offset_from_pts_ms %" G_GINT64_FORMAT " restamp_pts_offset_ms %" G_GINT64_FORMAT " buf duration %" G_GUINT64_FORMAT " lbo %" G_GINT64_FORMAT " ",
						first_begin_ms, offset_from_pts_ms, restamp_pts_offset_ms, GST_BUFFER_DURATION(buf) / GST_MSECOND, vipertransform->m_linear_begin_offset);

		if (ViperContentType::UNKNOWN == vipertransform->m_content_type)
		{
			//If first cue time does not line up with PTS, this is linear content and needs an offset
			// If the offset is greater than the buffer duration, we need to set the offset
			// to the first cue time
			// If the offset is less than the buffer duration, we need to set the offset
			// to the restamp offset
			if (std::abs(offset_from_pts_ms - restamp_pts_offset_ms) > GST_BUFFER_DURATION(buf) / GST_MSECOND)
			{
				vipertransform->m_content_type = ViperContentType::LINEAR_OFFSET;
				if (vipertransform->m_linear_begin_offset == 0 || abs(offset_from_pts_ms) < vipertransform->m_linear_begin_offset)
				{
					vipertransform->m_linear_begin_offset = offset_from_pts_ms;
				}
			}
			// If no misalignment was found, correct for any PTS re-stamping offset applied by Player for linear content
			else if (restamp_pts_offset_ms != 0)
			{
				vipertransform->m_linear_begin_offset = restamp_pts_offset_ms;
				vipertransform->m_content_type = ViperContentType::LINEAR_OFFSET;
			}
			else
			{
				//First cue time lines up with PTS so no offset needed
				vipertransform->m_content_type = ViperContentType::PASSTHROUGH;
				gst_base_transform_set_passthrough(trans, TRUE);
			}
		}
		//See if we can get the offset a bit closer
		//If the new offset is less, go for that
		//If it's equal to the last, we're probably at the beginning of the segment so stick with this
		else if (ViperContentType::LINEAR_OFFSET == vipertransform->m_content_type)
		{
			if ((std::abs(offset_from_pts_ms - restamp_pts_offset_ms) > GST_BUFFER_DURATION(buf) / GST_MSECOND))
			{
				vipertransform->m_linear_begin_offset = offset_from_pts_ms;
			}
			else
			{
				vipertransform->m_linear_begin_offset = restamp_pts_offset_ms;
			}
		}
		else
		{
			GST_DEBUG_OBJECT (vipertransform, "Empty segment - no cue found");
		}
	}
	return;
}

/**
 * @brief Add the offset to the GstBuffer if required. Also split the incoming buffer
 * if needed for Harmonic content
 *
 * @param trans
 * @param inbuf
 * @param outbuf
 * @return GstFlowReturn
 */
static GstFlowReturn
gst_vipertransform_transform (GstBaseTransform * trans, GstBuffer * inbuf,
	GstBuffer * outbuf)
{
  GstViperTransform *vipertransform = GST_VIPERTRANSFORM (trans);

  GST_DEBUG_OBJECT (vipertransform, "transform");

  std::string ttml;

  {
	GstMapInfo inmap, outmap;
	if (gst_buffer_map(inbuf, &inmap, (GstMapFlags)GST_MAP_READ) && gst_buffer_map(outbuf, &outmap, (GstMapFlags)GST_MAP_WRITE))
	{
	  ttml.assign(reinterpret_cast<char *>(inmap.data), inmap.size);
	  memcpy(outmap.data, inmap.data, inmap.size);
	  gst_buffer_unmap(outbuf, &outmap);
	  gst_buffer_unmap(inbuf, &inmap);
	}
	else
	{
	  GST_ERROR_OBJECT (vipertransform, "Could not map buffers");
	  return GST_FLOW_ERROR;
	}
  }


  if (is_harmonic_uhd(vipertransform, ttml))
  {
	std::vector<std::string> ttml_vec;

	GST_DEBUG_OBJECT (vipertransform, "Harmonic transform");

	//Split inbuf into a vector of xml docs
	ttml_vec = split_buffer(ttml);

	if (ttml_vec.empty())
	{
	  GST_DEBUG_OBJECT (vipertransform, "vector empty - passing data straight through");
	  return GST_FLOW_OK;
	}

	int segment_duration;
	GstClockTime pts = GST_BUFFER_PTS(inbuf);

	//Useful for debugging via filesrc (filesrc sets pts/dts/duration to -1)
	if (static_cast<gint64>(GST_BUFFER_DURATION(inbuf)) == -1)
	{
	  segment_duration = ttml_vec.size() * GST_SECOND;
	  pts = 0;
	}
	else
	  segment_duration = GST_BUFFER_DURATION(inbuf) / ttml_vec.size();

	auto segment_count = 0;

	//Split single, concatenated buffer into separate buffers
	//Add an offset to the buffer object for use by the sink
	//Then push them as new, separate buffers to the pipeline
	for (const auto &item : ttml_vec)
	{
	  GstBuffer *buf;
	  GstMemory *mem;

	  guint64 pts_ms = pts / GST_MSECOND;
	  guint64 duration_ms = segment_duration / GST_MSECOND;

	  gchar *data = (gchar *)g_malloc(item.size());
	  memcpy(data, item.c_str(), item.size());

	  buf = gst_buffer_new_wrapped(data, item.size());
	  //Retimestamp new buffer if required
	  GST_BUFFER_PTS(buf) = pts + ((segment_count * segment_duration));
	  GST_BUFFER_DTS(buf) = pts + ((segment_count * segment_duration));
	  GST_BUFFER_DURATION(buf) = segment_duration;

	  //Incoming Harmonic content might be either
	  // a. TTML timestamped relative to the start of the fragment or
	  // b. MediaKind-style with TTML timestamp being absolute from some fairly arbitrary zero point
	  if (vipertransform->m_linear_begin_offset != 0)
		GST_BUFFER_OFFSET(buf) = vipertransform->m_linear_begin_offset;
	  else
		GST_BUFFER_OFFSET(buf) = 0 - (pts_ms + (duration_ms * segment_count));

	  GST_DEBUG_OBJECT (vipertransform, "pts_ms: %" G_GUINT64_FORMAT " duration_ms %" G_GUINT64_FORMAT " offset ms %" G_GUINT64_FORMAT " segment_count %d", pts_ms, duration_ms, GST_BUFFER_OFFSET(buf), segment_count);

	  //Push fragment as a separate GstBuffer
	  gst_pad_push(gst_element_get_static_pad(&trans->element, "src"), buf);
	  segment_count++;
	}

	return GST_BASE_TRANSFORM_FLOW_DROPPED;
  }
  else if (vipertransform->m_linear_begin_offset != 0)
  {
	GST_BUFFER_OFFSET(outbuf) = vipertransform->m_linear_begin_offset;
  }

  return GST_FLOW_OK;
}

static gboolean
plugin_init (GstPlugin * plugin)
{
  return gst_element_register (plugin, "vipertransform", GST_RANK_PRIMARY + 1,
	  GST_TYPE_VIPERTRANSFORM);
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
	vipertransform,
	"Applies any necessary transforms for TTML content",
	plugin_init, VERSION, "LGPL", PACKAGE_NAME, GST_PACKAGE_ORIGIN)
