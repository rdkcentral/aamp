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
 * @file gstaampplayreadydecryptor.cpp
 * @brief aamp Playready decryptor plugin definitions
 */
#ifndef UBUNTU
// avoid ubuntu-specific segFault

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/base/gstbasetransform.h>
#include <gst/base/gstbytereader.h>
#include "gstaampplayreadydecryptor.h"
//#define FUNCTION_DEBUG 1
#ifdef FUNCTION_DEBUG
#define DEBUG_FUNC()    g_warning("####### %s : %d ####\n", __FUNCTION__, __LINE__);
#else
#define DEBUG_FUNC()
#endif

/* prototypes */
static void gst_aampplayreadydecryptor_finalize(GObject*);

/* class initialization */
#define gst_aampplayreadydecryptor_parent_class parent_class
G_DEFINE_TYPE(GstAampplayreadydecryptor, gst_aampplayreadydecryptor, GST_TYPE_AAMP_CDMI_DECRYPTOR);

GST_DEBUG_CATEGORY(gst_aampplayreadydecryptor_debug_category);
#define GST_CAT_DEFAULT gst_aampplayreadydecryptor_debug_category


/* pad templates */

static GstStaticPadTemplate gst_aampplayreadydecryptor_src_template =
        GST_STATIC_PAD_TEMPLATE("src", GST_PAD_SRC, GST_PAD_ALWAYS,
        GST_STATIC_CAPS("video/x-h264;video/x-h264(memory:SecMem);audio/mpeg;video/x-h265;video/x-h265(memory:SecMem);audio/x-eac3;audio/x-gst-fourcc-ec_3;audio/x-ac3"));
 
static GstStaticPadTemplate gst_aampplayreadydecryptor_sink_template =
        GST_STATIC_PAD_TEMPLATE("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
                GST_STATIC_CAPS(
                        "application/x-cenc, original-media-type=(string)video/x-h264, protection-system=(string)" PLAYREADY_PROTECTION_SYSTEM_ID "; "
                        "application/x-cenc, original-media-type=(string)video/x-h265, protection-system=(string)" PLAYREADY_PROTECTION_SYSTEM_ID "; "
                        "application/x-cenc, original-media-type=(string)audio/x-eac3, protection-system=(string)" PLAYREADY_PROTECTION_SYSTEM_ID "; "
                        "application/x-cenc, original-media-type=(string)audio/x-ac3, protection-system=(string)" PLAYREADY_PROTECTION_SYSTEM_ID "; "
			"application/x-cenc, original-media-type=(string)audio/x-gst-fourcc-ec_3, protection-system=(string)" PLAYREADY_PROTECTION_SYSTEM_ID "; "
                        "application/x-cenc, original-media-type=(string)audio/mpeg, protection-system=(string)" PLAYREADY_PROTECTION_SYSTEM_ID));

static GstStaticPadTemplate gst_aampplayreadydecryptor_dummy_sink_template =
        GST_STATIC_PAD_TEMPLATE("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
                GST_STATIC_CAPS("playready/x-unused"));

/**
 * @brief Playready decryptor class initialization
 * @param klass Gstreamer Class
 */
static void gst_aampplayreadydecryptor_class_init(
        GstAampplayreadydecryptorClass * klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GstElementClass* elementClass = GST_ELEMENT_CLASS(klass);

    DEBUG_FUNC();

	gobject_class->finalize = gst_aampplayreadydecryptor_finalize;

	/* Setting up pads and setting metadata should be moved to
	 base_class_init if you intend to subclass this class. */
	gst_element_class_add_static_pad_template(elementClass, &gst_aampplayreadydecryptor_src_template);
	gst_element_class_add_static_pad_template(elementClass, &gst_aampplayreadydecryptor_sink_template);

	gst_element_class_set_static_metadata(elementClass,
	        "Decrypt PlayReady encrypted contents",
	        GST_ELEMENT_FACTORY_KLASS_DECRYPTOR,
	        "Decrypts streams encrypted using PlayReady Encryption.",
	        "Comcast");
}

/**
 * @brief Playready decryptor element initialization
 * @param aampplayreadydecryptor playready decryptor element pointer
 */
static void gst_aampplayreadydecryptor_init(GstAampplayreadydecryptor *aampplayreadydecryptor)
{
    DEBUG_FUNC();
}

/**
 * @brief Playready decryptor element termination
 * @param object playready decryptor element pointer
 */
static void gst_aampplayreadydecryptor_finalize(GObject * object)
{
    DEBUG_FUNC();
    GST_CALL_PARENT(G_OBJECT_CLASS, finalize, (object));
}
#endif // UBUNTU
