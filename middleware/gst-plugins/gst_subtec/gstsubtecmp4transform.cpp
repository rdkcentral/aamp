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
#include <gst/base/gstbasetransform.h>
#include "gstsubtecmp4transform.h"

#include <cstring>
#include <cstdint>

#include <string>
#include <vector>
#include <stdexcept>

GST_DEBUG_CATEGORY_STATIC (gst_subtecmp4transform_debug_category);
#define GST_CAT_DEFAULT gst_subtecmp4transform_debug_category

/* prototypes */


static void gst_subtecmp4transform_dispose (GObject * object);
static void gst_subtecmp4transform_finalize (GObject * object);

static GstCaps *gst_subtecmp4transform_transform_caps (GstBaseTransform * trans,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter);
static GstCaps *gst_subtecmp4transform_fixate_caps (GstBaseTransform * trans,
    GstPadDirection direction, GstCaps * caps, GstCaps * othercaps);
static gboolean gst_subtecmp4transform_accept_caps (GstBaseTransform * trans,
    GstPadDirection direction, GstCaps * caps);
static gboolean gst_subtecmp4transform_set_caps (GstBaseTransform * trans,
    GstCaps * incaps, GstCaps * outcaps);
static gboolean gst_subtecmp4transform_query (GstBaseTransform * trans,
    GstPadDirection direction, GstQuery * query);
static gboolean gst_subtecmp4transform_decide_allocation (GstBaseTransform * trans,
    GstQuery * query);
static gboolean gst_subtecmp4transform_filter_meta (GstBaseTransform * trans,
    GstQuery * query, GType api, const GstStructure * params);
static gboolean gst_subtecmp4transform_propose_allocation (GstBaseTransform * trans,
    GstQuery * decide_query, GstQuery * query);
static gboolean gst_subtecmp4transform_transform_size (GstBaseTransform * trans,
    GstPadDirection direction, GstCaps * caps, gsize size, GstCaps * othercaps,
    gsize * othersize);
static gboolean gst_subtecmp4transform_get_unit_size (GstBaseTransform * trans,
    GstCaps * caps, gsize * size);
static gboolean gst_subtecmp4transform_start (GstBaseTransform * trans);
static gboolean gst_subtecmp4transform_stop (GstBaseTransform * trans);
static gboolean gst_subtecmp4transform_sink_event (GstBaseTransform * trans,
    GstEvent * event);
static gboolean gst_subtecmp4transform_src_event (GstBaseTransform * trans,
    GstEvent * event);
static GstFlowReturn gst_subtecmp4transform_prepare_output_buffer (GstBaseTransform *
    trans, GstBuffer * input, GstBuffer ** outbuf);
static gboolean gst_subtecmp4transform_copy_metadata (GstBaseTransform * trans,
    GstBuffer * input, GstBuffer * outbuf);
static gboolean gst_subtecmp4transform_transform_meta (GstBaseTransform * trans,
    GstBuffer * outbuf, GstMeta * meta, GstBuffer * inbuf);
static void gst_subtecmp4transform_before_transform (GstBaseTransform * trans,
    GstBuffer * buffer);
static GstFlowReturn gst_subtecmp4transform_transform (GstBaseTransform * trans,
    GstBuffer * inbuf, GstBuffer * outbuf);
static GstFlowReturn gst_subtecmp4transform_transform_ip (GstBaseTransform * trans,
    GstBuffer * buf);

enum
{
  PROP_0
};

/* pad templates */

static GstStaticPadTemplate gst_subtecmp4transform_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/ttml+xml")
    );

static GstStaticPadTemplate gst_subtecmp4transform_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/mp4")
    );


/* class initialization */

G_DEFINE_TYPE_WITH_CODE (GstSubtecMp4Transform, gst_subtecmp4transform, GST_TYPE_BASE_TRANSFORM,
  GST_DEBUG_CATEGORY_INIT (gst_subtecmp4transform_debug_category, "subtecmp4transform", 0,
  "debug category for subtecmp4transform element"));

static void
gst_subtecmp4transform_class_init (GstSubtecMp4TransformClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstBaseTransformClass *base_transform_class = GST_BASE_TRANSFORM_CLASS (klass);

  gst_element_class_add_static_pad_template (GST_ELEMENT_CLASS(klass),
      &gst_subtecmp4transform_src_template);
  gst_element_class_add_static_pad_template (GST_ELEMENT_CLASS(klass),
      &gst_subtecmp4transform_sink_template);

  gst_element_class_set_static_metadata (GST_ELEMENT_CLASS(klass),
      "Subtec ISO MP4 demuxer", "Demuxer/Subtitle", "Pulls subtitle data from stpp box", "Comcast");

  base_transform_class->transform_caps = GST_DEBUG_FUNCPTR (gst_subtecmp4transform_transform_caps);
  base_transform_class->transform_ip = GST_DEBUG_FUNCPTR (gst_subtecmp4transform_transform_ip);
}

static void
gst_subtecmp4transform_init (GstSubtecMp4Transform *subtecmp4transform)
{
}

static GstCaps *
gst_subtecmp4transform_transform_caps (GstBaseTransform * trans, GstPadDirection direction,
    GstCaps * caps, GstCaps * filter)
{
  GstSubtecMp4Transform *subtecmp4transform = GST_SUBTECMP4TRANSFORM (trans);
  GstCaps *othercaps;

  static GstStaticCaps ttml_caps =
      GST_STATIC_CAPS ("application/ttml+xml");
  static GstStaticCaps vtt_caps =
      GST_STATIC_CAPS ("text/vtt");
  static GstStaticCaps mp4_caps =
      GST_STATIC_CAPS ("application/mp4");

  GST_DEBUG_OBJECT (subtecmp4transform, "transform_caps caps");
  GST_DEBUG_OBJECT (subtecmp4transform, "direction %s from caps %" GST_PTR_FORMAT,
      direction == GST_PAD_SRC ? "src" : "sink", caps);
  GST_DEBUG_OBJECT (subtecmp4transform, "filter %s NULL", filter == NULL ? "is" : "is not");

  /* Copy other caps and modify as appropriate */
  /* This works for the simplest cases, where the transform modifies one
   * or more fields in the caps structure.  It does not work correctly
   * if passthrough caps are preferred. */
  const GstStructure *s = gst_caps_get_structure (caps, 0);

    if (direction == GST_PAD_SRC) 
    {
      /* transform caps going upstream */
      if (gst_structure_has_name (s, "application/ttml+xml")) 
      {
        othercaps = gst_caps_new_empty();
        othercaps = gst_caps_merge(othercaps, gst_static_caps_get(&mp4_caps));
        /* transform caps going downstream */
      }
      else
      {
        othercaps = gst_caps_copy(caps);
      }
    } 
    else 
    {
      if (gst_structure_has_name (s, "application/mp4")) 
      {
        othercaps = gst_caps_new_empty();
        othercaps = gst_caps_merge(othercaps, gst_static_caps_get(&ttml_caps));
        const GValue* viper_hd = gst_structure_get_value(s, "viper_ttml_format");
        if (NULL != viper_hd)
          gst_caps_set_value(othercaps, "viper_ttml_format", viper_hd);

        /* transform caps going downstream */
      }
      else if (gst_structure_has_name (s, "text/vtt")) 
      {
        othercaps = gst_caps_new_empty();
        othercaps = gst_caps_merge(othercaps, gst_static_caps_get(&vtt_caps));
        /* transform caps going downstream */
      }
      else
      {
        othercaps = gst_caps_copy(caps);
      }
    }

  GST_DEBUG_OBJECT (subtecmp4transform, "othercaps %" GST_PTR_FORMAT, othercaps);

  if (filter) {
    GstCaps *intersect;

    intersect = gst_caps_intersect (othercaps, filter);
    gst_caps_unref (othercaps);

    return intersect;
  } else {
    return othercaps;
  }
}

static std::uint32_t parse32(std::uint8_t *ptr, std::size_t len, std::size_t offset)
{
  if (offset + 4 > len) throw std::out_of_range("len too short");

  std::uint32_t value = 0;

  const std::uint32_t byte0 = static_cast<std::uint32_t>(ptr[offset]) & 0xFF;
  const std::uint32_t byte1 = static_cast<std::uint32_t>(ptr[offset + 1]) & 0xFF;
  const std::uint32_t byte2 = static_cast<std::uint32_t>(ptr[offset + 2]) & 0xFF;
  const std::uint32_t byte3 = static_cast<std::uint32_t>(ptr[offset + 3]) & 0xFF;

  value |= byte3;
  value |= byte2 << 8;
  value |= byte1 << 16;
  value |= byte0 << 24;

  return value;
}

static bool parseName(std::uint8_t *ptr, std::size_t len, std::size_t offset, std::string &value)
{
  if (offset + 4 > len) return false;

  const char byte0 = static_cast<char>(ptr[offset]) & 0xFF;
  const char byte1 = static_cast<char>(ptr[offset + 1]) & 0xFF;
  const char byte2 = static_cast<char>(ptr[offset + 2]) & 0xFF;
  const char byte3 = static_cast<char>(ptr[offset + 3]) & 0xFF;

  value += byte0;
  value += byte1;
  value += byte2;
  value += byte3;

  return true;
}

static bool isContainer(const std::string &name)
{
  static const std::vector<std::string> containerNames{"moov", "trak", "mdia", "minf", "dinf", "stbl", "mvex"};

  for (const auto &container : containerNames)
  {
    if (!container.compare(name)) return true;
  }

  return false;
}

static bool printBoxes(GstSubtecMp4Transform *subtecmp4transform, uint8_t *boxes, size_t len)
{
  size_t offset = 0;

  GST_DEBUG_OBJECT (subtecmp4transform, "printBoxes: len %zu", len);

  while (offset < len)
  {
    std::size_t boxSize;
    std::string boxName;

    try
    {
      boxSize = parse32(boxes, len, offset);
      offset += 4;
      parseName(boxes, len, offset, boxName);
    }
    catch (const std::out_of_range &e)
    {
      GST_WARNING_OBJECT (subtecmp4transform, "parse error:%s", e.what());
      return false;
    }

    if (isContainer(boxName))
    {
      offset += 4;
    }
    else
      offset += boxSize - 4;

    GST_DEBUG_OBJECT (subtecmp4transform, "box name: %s size %zu offset %zu", boxName.c_str(), boxSize, offset);
  }

  return true;
}

static bool findBoxOffsetAndLength(GstSubtecMp4Transform *subtecmp4transform, uint8_t *buf, size_t len, const std::string &name, size_t &boxOffset, size_t &boxLength)
{
  size_t offset = 0;

  while (offset < len)
  {
    std::size_t boxSize;
    std::string boxName;

    try
    {
      boxSize = parse32(buf, len, offset);
      offset += 4;
      parseName(buf, len, offset, boxName);
    }
    catch (const std::out_of_range &e)
    {
      GST_WARNING_OBJECT (subtecmp4transform, "parse error:%s", e.what());
      return false;
    }

    GST_DEBUG_OBJECT (subtecmp4transform, "boxName %s name %s len %zu", boxName.c_str(), name.c_str(), boxSize);

    if (!boxName.compare(name))
    {
      boxOffset = offset + 4;
      boxLength = boxSize;

      return true;
    }

    if (isContainer(boxName))
    {
      offset += 4;
    }
    else
      offset += boxSize - 4;
  }

  return false;
}

static GstFlowReturn
gst_subtecmp4transform_transform_ip (GstBaseTransform * trans, GstBuffer * buf)
{
  GstSubtecMp4Transform *subtecmp4transform = GST_SUBTECMP4TRANSFORM (trans);

  GST_DEBUG_OBJECT (subtecmp4transform, "transform_ip");

  GstMapInfo map;

  gst_buffer_map(buf, &map, (GstMapFlags)GST_MAP_READWRITE);

  size_t offset = 0, length = 0;

	if (!findBoxOffsetAndLength(subtecmp4transform, reinterpret_cast<uint8_t *>(map.data), map.size, "ftyp", offset, length))
	{
		if (findBoxOffsetAndLength(subtecmp4transform, reinterpret_cast<uint8_t *>(map.data), map.size, "mdat", offset, length))
    {
      GST_DEBUG_OBJECT (subtecmp4transform, "offset %zu length %zu", offset, length);
      memcpy(map.data, map.data + offset, length);
      gst_buffer_resize(buf, 0, length);
    }
  }
  else
  {
    GST_DEBUG_OBJECT (subtecmp4transform, "DROPPING FRAME");
    gst_buffer_unmap(buf, &map);
    return GST_BASE_TRANSFORM_FLOW_DROPPED;
  }

  gst_buffer_unmap(buf, &map);

  return GST_FLOW_OK;
}

static gboolean
plugin_init (GstPlugin * plugin)
{

  return gst_element_register (plugin, "subtecmp4transform", GST_RANK_PRIMARY,
      GST_TYPE_SUBTECMP4TRANSFORM);
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
    subtecmp4transform,
    "MP4 demuxer for subtitle packets",
    plugin_init, VERSION, "LGPL", PACKAGE_NAME, GST_PACKAGE_ORIGIN)

