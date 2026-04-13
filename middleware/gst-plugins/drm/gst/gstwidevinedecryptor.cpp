/*
* Copyright 2018 RDK Management
*
* This library is free software; you can redistribute it and/or
* modify it under the terms of the GNU Lesser General Public
* License as published by the Free Software Foundation, version 2.1
* of the license.
*
* This library is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
* Lesser General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public
* License along with this library; if not, write to the
* Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
* Boston, MA 02110-1301, USA.
*/
/**
 * @file gstwidevinedecryptor.cpp
 * @brief widevine decryptor plugin definitions
 */
#include <gst/gst.h>
#include <gst/base/gstbasetransform.h>
#include <gst/base/gstbytereader.h>
#include "gstwidevinedecryptor.h"

#define FUNCTION_DEBUG 1
#ifdef FUNCTION_DEBUG
#define DEBUG_FUNC()    g_warning("####### %s : %d ####\n", __FUNCTION__, __LINE__);
#else
#define DEBUG_FUNC()
#endif

/* prototypes */
static void gst_widevinedecryptor_finalize(GObject*);

/* class initialization */
#define gst_widevinedecryptor_parent_class parent_class
G_DEFINE_TYPE(Gstwidevinedecryptor, gst_widevinedecryptor, GST_TYPE_CDMI_DECRYPTOR);

GST_DEBUG_CATEGORY(gst_widevinedecryptor_debug_category);
#define GST_CAT_DEFAULT gst_widevinedecryptor_debug_category


/* pad templates */

static GstStaticPadTemplate gst_widevinedecryptor_src_template =
        GST_STATIC_PAD_TEMPLATE("src", GST_PAD_SRC, GST_PAD_ALWAYS,
        GST_STATIC_CAPS("video/x-h264;video/x-h264(memory:SecMem);audio/mpeg;video/x-h265;video/x-h265(memory:SecMem);audio/x-eac3;audio/x-gst-fourcc-ec_3;audio/x-ac3;audio/x-opus"));
 
static GstStaticPadTemplate gst_widevinedecryptor_sink_template =
        GST_STATIC_PAD_TEMPLATE("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
                GST_STATIC_CAPS(
                        "application/x-cenc, original-media-type=(string)video/x-h264, protection-system=(string)" WIDEVINE_PROTECTION_SYSTEM_ID "; "
                        "application/x-cenc, original-media-type=(string)video/x-h265, protection-system=(string)" WIDEVINE_PROTECTION_SYSTEM_ID "; "
                        "application/x-cenc, original-media-type=(string)audio/x-eac3, protection-system=(string)" WIDEVINE_PROTECTION_SYSTEM_ID "; "
                        "application/x-cenc, original-media-type=(string)audio/x-ac3, protection-system=(string)" WIDEVINE_PROTECTION_SYSTEM_ID "; "
                        "application/x-cenc, original-media-type=(string)audio/x-opus, protection-system=(string)" WIDEVINE_PROTECTION_SYSTEM_ID "; "
			"application/x-cenc, original-media-type=(string)audio/x-gst-fourcc-ec_3, protection-system=(string)" WIDEVINE_PROTECTION_SYSTEM_ID "; "
                        "application/x-cenc, original-media-type=(string)audio/mpeg, protection-system=(string)" WIDEVINE_PROTECTION_SYSTEM_ID));

static GstStaticPadTemplate gst_widevinedecryptor_dummy_sink_template =
        GST_STATIC_PAD_TEMPLATE("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
                GST_STATIC_CAPS("widevine/x-unused")); // unused?


static void gst_widevinedecryptor_class_init(GstwidevinedecryptorClass * klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GstElementClass* elementClass = GST_ELEMENT_CLASS(klass);

    DEBUG_FUNC();

	gobject_class->finalize = gst_widevinedecryptor_finalize;

	/* Setting up pads and setting metadata should be moved to
	 base_class_init if you intend to subclass this class. */
	gst_element_class_add_static_pad_template(elementClass, &gst_widevinedecryptor_src_template);
	gst_element_class_add_static_pad_template(elementClass, &gst_widevinedecryptor_sink_template);

	gst_element_class_set_static_metadata(elementClass,
	        "Decrypt Widevine encrypted contents",
	        GST_ELEMENT_FACTORY_KLASS_DECRYPTOR,
	        "Decrypts streams encrypted using Widevine Encryption.",
	        "Comcast");
}

static void gst_widevinedecryptor_init(Gstwidevinedecryptor *widevinedecryptor)
{
    DEBUG_FUNC();
    g_warning("HariPriya: gst_widevinedecryptor_init - Creating new Widevine decryptor element=%p, ref_count=%u, name=%s, timestamp=%lld",
            widevinedecryptor,
            G_OBJECT(widevinedecryptor)->ref_count,
            GST_ELEMENT_NAME(widevinedecryptor),
            (long long)g_get_monotonic_time());
}

static void gst_widevinedecryptor_finalize(GObject * object)
{
    DEBUG_FUNC();
    
    // HariPriya: Check element validity and ref count at finalize
    guint ref_count = G_OBJECT(object)->ref_count;
    gboolean is_valid_element = GST_IS_ELEMENT(object);
    GstElement *element = GST_ELEMENT_CAST(object);
    GstState current_state = GST_STATE_NULL;
    GstState pending_state = GST_STATE_VOID_PENDING;
    const gchar* element_name = "<invalid>";
    
    if (is_valid_element) {
        current_state = GST_STATE(element);
        pending_state = GST_STATE_PENDING(element);
        element_name = GST_ELEMENT_NAME(element);
    }
    
    long long timestamp = g_get_monotonic_time();
    g_warning("HariPriya: gst_widevinedecryptor_finalize ENTER - element=%p, ref_count=%u, is_valid_element=%d, current_state=%d, pending_state=%d, name=%s, timestamp=%lld",
              object, ref_count, is_valid_element, current_state, pending_state, element_name, timestamp);
    
    // Check parent element
    if (is_valid_element && GST_ELEMENT_PARENT(element)) {
        GstElement* parent = GST_ELEMENT_PARENT(element);
        g_warning("HariPriya: gst_widevinedecryptor_finalize - parent=%p, parent_name=%s, parent_ref_count=%u",
                  parent, GST_ELEMENT_NAME(parent), G_OBJECT(parent)->ref_count);
    }
    
    GST_CALL_PARENT(G_OBJECT_CLASS, finalize, (object));
    
    g_warning("HariPriya: gst_widevinedecryptor_finalize EXIT - element=%p, timestamp=%lld", object, (long long)g_get_monotonic_time());
}
