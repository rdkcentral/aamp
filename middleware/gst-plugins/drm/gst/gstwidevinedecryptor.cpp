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

/*
 * The src pad template caps are built dynamically at class_init time using
 * gst_cdmidecryptor_build_src_caps_string() so that platform-specific secure
 * memory features (e.g. "memory:MediaTekSecure") are included without
 * hardcoding them here.  No GST_STATIC_PAD_TEMPLATE / GST_STATIC_CAPS needed
 * for the src pad.
 */

 
static GstStaticPadTemplate gst_widevinedecryptor_sink_template =
        GST_STATIC_PAD_TEMPLATE("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
                GST_STATIC_CAPS(
                        "application/x-cenc, original-media-type=(string)video/x-h264, protection-system=(string)" WIDEVINE_PROTECTION_SYSTEM_ID "; "
                        "application/x-cenc, original-media-type=(string)video/x-h265, protection-system=(string)" WIDEVINE_PROTECTION_SYSTEM_ID "; "
                        "application/x-cenc, original-media-type=(string)audio/x-eac3, protection-system=(string)" WIDEVINE_PROTECTION_SYSTEM_ID "; "
                        "application/x-cenc, original-media-type=(string)audio/x-ac3, protection-system=(string)" WIDEVINE_PROTECTION_SYSTEM_ID "; "
                        "application/x-cenc, original-media-type=(string)audio/x-opus, protection-system=(string)" WIDEVINE_PROTECTION_SYSTEM_ID "; "
			"application/x-cenc, original-media-type=(string)audio/x-gst-fourcc-ec_3, protection-system=(string)" WIDEVINE_PROTECTION_SYSTEM_ID "; "
                        "application/x-cenc, original-media-type=(string)audio/x-ac4, protection-system=(string)" WIDEVINE_PROTECTION_SYSTEM_ID "; "
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

       /* Build the src pad template dynamically so the correct platform memory
        * feature (e.g. "memory:MediaTekSecure" on MTK, "memory:SecMem" on
        * Amlogic) is included without any compile-time hardcoding. */
       const gchar *platformMemFeature = gst_cdmidecryptor_get_platform_memory_feature();
       gchar *srcCapsStr = gst_cdmidecryptor_build_src_caps_string(platformMemFeature);
       GstCaps *srcCaps = gst_caps_from_string(srcCapsStr);
       g_free(srcCapsStr);

       GstPadTemplate *srcTemplate = gst_pad_template_new("src", GST_PAD_SRC, GST_PAD_ALWAYS, srcCaps);
       gst_caps_unref(srcCaps);
       gst_element_class_add_pad_template(elementClass, srcTemplate);


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
}

static void gst_widevinedecryptor_finalize(GObject * object)
{
    DEBUG_FUNC();
    GST_CALL_PARENT(G_OBJECT_CLASS, finalize, (object));
}
