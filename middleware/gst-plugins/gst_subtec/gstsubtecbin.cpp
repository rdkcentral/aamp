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
 * SECTION:element-gstsubtecbin
 *
 * Plugs the subtec pipeline - adding mp4transform and/or vipertransform
 * elements as required
 */

#include <gst/gst.h>
#include "gstsubtecbin.h"

GST_DEBUG_CATEGORY_STATIC (gst_subtecbin_debug_category);
#define GST_CAT_DEFAULT gst_subtecbin_debug_category

static void gst_subtecbin_dispose (GObject * object);
static void gst_subtecbin_finalize (GObject * object);
static void gst_subtecbin_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * spec);
static void gst_subtecbin_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);
static bool try_lock_element_state (GstSubtecBin * subtecbin);


static GstStaticPadTemplate gst_subtecbin_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/ttml+xml; text/vtt; application/mp4;")
    );

/* class initialization */
G_DEFINE_TYPE_WITH_CODE (GstSubtecBin, gst_subtecbin, GST_TYPE_BIN,
  GST_DEBUG_CATEGORY_INIT (gst_subtecbin_debug_category, "subtecbin", 0,
  "debug category for subtecbin element"));

enum
{
  PROP_0,
  PROP_MUTE,
  PROP_NO_EOS,
  PROP_ASYNC,
  PROP_SYNC,
  PROP_SUBTEC_SOCKET,
  PROP_PTS_OFFSET,
  PROP_ATTRIBUTE_VALUES
};

static void
gst_subtecbin_class_init (GstSubtecBinClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstBinClass *base_sink_class = GST_BIN_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS(klass);

  GST_LOG("gst_subtecbin_class_init");

  gst_element_class_add_static_pad_template (GST_ELEMENT_CLASS(klass),
      &gst_subtecbin_sink_template);

  gst_element_class_set_static_metadata (GST_ELEMENT_CLASS(klass),
      "OOB Subtec data sink", "Sink/Parser/Subtitle", "Packs TTML or WebVTT data into SubTtxRend APP suitable packets", "Comcast");

  gobject_class->set_property = gst_subtecbin_set_property;
  gobject_class->get_property = gst_subtecbin_get_property;

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
                                  PROP_ASYNC,
                                  g_param_spec_boolean("async", "Async", "Sets async on children (require preroll before returning async_done)",
                                  TRUE,
                                  (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property(gobject_class,
                                  PROP_SYNC,
                                  g_param_spec_boolean("sync", "Sync", "Sets sync on children (synchronize render on clock)",
                                  TRUE,
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

  gobject_class->dispose = gst_subtecbin_dispose;
  gobject_class->finalize = gst_subtecbin_finalize;
}

static void gst_subtecbin_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstSubtecBin *subtecbin = GST_SUBTECBIN (object);

  GST_DEBUG_OBJECT (subtecbin, "set_property %d", prop_id);

  switch (prop_id) {
    case PROP_MUTE:
      if (subtecbin->sink)
        g_object_set_property(G_OBJECT(subtecbin->sink), "mute", value);
      subtecbin->mute = g_value_get_boolean(value);
      break;
    case PROP_NO_EOS:
      if (subtecbin->sink)
        g_object_set_property(G_OBJECT(subtecbin->sink), "no-eos", value);
      subtecbin->no_eos = g_value_get_boolean(value);
      break;
    case PROP_ASYNC:
      if (subtecbin->sink)
        g_object_set_property(G_OBJECT(subtecbin->sink), "async", value);
      subtecbin->async = g_value_get_boolean(value);
      break;
    case PROP_SYNC:
      if (subtecbin->sink)
        g_object_set_property(G_OBJECT(subtecbin->sink), "sync", value);
      subtecbin->sync = g_value_get_boolean(value);
      break;
    case PROP_SUBTEC_SOCKET:
      if (subtecbin->sink)
        g_object_set_property(G_OBJECT(subtecbin->sink), "subtec-socket", value);
      subtecbin->subtec_socket = g_value_get_string(value);
      break;
    case PROP_PTS_OFFSET:
    {
      if (subtecbin->sink)
        g_object_set_property(G_OBJECT(subtecbin->sink), "pts-offset", value);
      subtecbin->pts_offset = g_value_get_uint64(value);
    }
      break;
    case PROP_ATTRIBUTE_VALUES:
      if (subtecbin->sink)
        g_object_set_property(G_OBJECT(subtecbin->sink), "attribute-values", value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

void
gst_subtecbin_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GstSubtecBin *subtecbin = GST_SUBTECBIN (object);

  GST_DEBUG_OBJECT (subtecbin, "get_property %d", property_id);

  switch (property_id) {
    case PROP_MUTE:
      g_value_set_boolean(value, subtecbin->mute);
      break;
    case PROP_NO_EOS:
      g_value_set_boolean(value, subtecbin->no_eos);
      break;
    case PROP_ASYNC:
      g_value_set_boolean(value, subtecbin->async);
      break;
    case PROP_SYNC:
      g_value_set_boolean(value, subtecbin->sync);
      break;
    case PROP_SUBTEC_SOCKET:
      g_value_set_string(value, subtecbin->subtec_socket.c_str());
      break;
    case PROP_PTS_OFFSET:
      g_value_set_uint64(value, subtecbin->pts_offset);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static bool
try_lock_element_state (GstSubtecBin * subtecbin)
{
  guint count = 10;
  bool locked = GST_STATE_TRYLOCK(GST_ELEMENT(subtecbin));
  while (!locked && (--count > 0))
  {
    g_usleep(10000); //10ms wait
    locked = GST_STATE_TRYLOCK(GST_ELEMENT(subtecbin));
  }
  return locked;
}

static void
type_found (GstElement * typefind, guint probability,
    GstCaps * caps, GstSubtecBin * subtecbin)
{
  GST_DEBUG_OBJECT (subtecbin, "subs_typefind found caps %" GST_PTR_FORMAT, caps);
  subtecbin->sink = gst_element_factory_make("subtecsink", NULL);

  g_return_if_fail (subtecbin->sink != NULL);

  g_object_set(G_OBJECT(subtecbin->sink), "mute", subtecbin->mute, NULL);
  g_object_set(G_OBJECT(subtecbin->sink), "no-eos", subtecbin->no_eos, NULL);
  g_object_set(G_OBJECT(subtecbin->sink), "async", subtecbin->async, NULL);
  g_object_set(G_OBJECT(subtecbin->sink), "sync", subtecbin->sync, NULL);
  g_object_set(G_OBJECT(subtecbin->sink), "pts-offset", subtecbin->pts_offset, NULL);

  GstElementFactory *ttml_transform_factory = NULL;
  GList *sub_parser_factories =  gst_element_factory_list_filter (gst_element_factory_list_get_elements(GST_ELEMENT_FACTORY_TYPE_MEDIA_SUBTITLE, 
                                  GST_RANK_PRIMARY), 
                                  caps,
                                  GST_PAD_SINK, 
                                  TRUE);
  GList *tmp = sub_parser_factories;

  for (; tmp; tmp = tmp->next) {
    GstElementFactory *factory = (GstElementFactory *) tmp->data;
    GST_DEBUG_OBJECT(subtecbin, "factory name %s can sink caps %d", gst_plugin_feature_get_name ((GstPluginFeature *) factory), gst_element_factory_can_sink_all_caps(factory, caps));
    if (gst_element_factory_can_sink_all_caps(factory, caps))
    {
      if (gst_element_factory_list_is_type(factory, GST_ELEMENT_FACTORY_TYPE_DEMUXER))
      {
        subtecbin->demux = gst_element_factory_create(factory, NULL);        

        GstPad *demux_pad = gst_element_get_static_pad(subtecbin->demux, "src");
        GstCaps *demux_caps = gst_pad_get_pad_template_caps(demux_pad);

        GST_DEBUG_OBJECT(subtecbin, "seek allowed caps %" GST_PTR_FORMAT, demux_caps);
        auto ttml_formatter_factory = gst_element_factory_find("vipertransform");
        if (gst_element_factory_can_sink_all_caps(ttml_formatter_factory, demux_caps))
        {
          subtecbin->formatter = gst_element_factory_make("vipertransform", NULL);
        }
      }
    }
  }
  gst_plugin_feature_list_free(sub_parser_factories);

  GList *chain = NULL;

  if (subtecbin->demux)
  {
    GST_DEBUG_OBJECT(subtecbin, "demuxer"); 
    chain = g_list_append(chain, gst_object_ref(subtecbin->demux));
  }
  if (subtecbin->formatter)
  {
    GST_DEBUG_OBJECT(subtecbin, "formatter"); 
    chain = g_list_append(chain, gst_object_ref(subtecbin->formatter));
  }
  if (subtecbin->sink)
  {
    GST_DEBUG_OBJECT(subtecbin, "sink"); 
    chain = g_list_append(chain, gst_object_ref(subtecbin->sink));
  }

  tmp = chain;

  // Try to acquire the element stream mutex before calling gst_element_sync_state_with_parent()
  // If an attempt to set the pipeline state to NULL occurs during startup, this can lead to
  // a deadlock between pad and element mutexes
  if (!try_lock_element_state(subtecbin))
  {
    GST_WARNING("%s unable to acquire element state lock!", __func__);
    goto lock_failed;
  }

  gst_element_sync_state_with_parent(GST_ELEMENT(subtecbin));
  for(; tmp; tmp = tmp->next)
  {
    GstElement *current = GST_ELEMENT(tmp->data);
    GST_DEBUG_OBJECT(subtecbin, "element %s", gst_element_get_name(current));
    gst_bin_add(GST_BIN(subtecbin), current);
    if (tmp->prev)
      gst_element_link(GST_ELEMENT(tmp->prev->data), current);
    else
      gst_element_link(typefind, current);
    gst_object_unref(current);
  }

  tmp = chain;
  for(; tmp; tmp = tmp->next)
  {
    gst_element_sync_state_with_parent(GST_ELEMENT(tmp->data));
  }

  GST_STATE_UNLOCK(GST_ELEMENT(subtecbin));
lock_failed:

  if (chain)
  {
    g_list_free(chain);
  }
}

static void
gst_subtecbin_init (GstSubtecBin *subtecbin)
{
  GST_DEBUG_OBJECT(subtecbin, "init1");

  subtecbin->typefind = gst_element_factory_make("typefind", "subs_typefind");
  gst_bin_add(GST_BIN(subtecbin), subtecbin->typefind);
  gst_element_add_pad (GST_ELEMENT (subtecbin), gst_ghost_pad_new("sink", gst_element_get_static_pad(subtecbin->typefind, "sink")));
  gst_element_sync_state_with_parent(GST_ELEMENT(subtecbin));

  g_signal_connect(subtecbin->typefind, "have-type", G_CALLBACK(type_found), subtecbin);
}

void
gst_subtecbin_dispose (GObject * object)
{
  GstSubtecBin *subtecbin = GST_SUBTECBIN (object);

  GST_DEBUG_OBJECT (subtecbin, "dispose");

  /* clean up as possible.  may be called multiple times */
}

void
gst_subtecbin_finalize (GObject * object)
{
  GstSubtecBin *subtecbin = GST_SUBTECBIN (object);

  GST_DEBUG_OBJECT (subtecbin, "finalize");

  /* clean up object here */
}


static gboolean
plugin_init (GstPlugin * plugin)
{
  return gst_element_register (plugin, "subtecbin", GST_RANK_PRIMARY,
      GST_TYPE_SUBTECBIN);
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
    subtecbin,
    "SubTtxRend autoplug bin",
    plugin_init, VERSION, "LGPL", PACKAGE_NAME, GST_PACKAGE_ORIGIN)
