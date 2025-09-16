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
 * @file gstaampclearkeydecryptor.cpp
 * @brief aamp clearkey decryptor plugin definitions
 */
#ifndef UBUNTU
// avoid ubuntu-specific segFault

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/base/gstbasetransform.h>
#include <gst/base/gstbytereader.h>
#include "gstaampclearkeydecryptor.h"
//#define FUNCTION_DEBUG 1
#ifdef FUNCTION_DEBUG
#define DEBUG_FUNC()    g_warning("####### %s : %d ####\n", __FUNCTION__, __LINE__);
#else
#define DEBUG_FUNC()
#endif

/* prototypes */
static void gst_aampclearkeydecryptor_finalize(GObject*);

/* class initialization */
#define gst_aampclearkeydecryptor_parent_class parent_class
G_DEFINE_TYPE(GstAampclearkeydecryptor, gst_aampclearkeydecryptor, GST_TYPE_AAMP_CDMI_DECRYPTOR);

GST_DEBUG_CATEGORY(gst_aampclearkeydecryptor_debug_category);
#define GST_CAT_DEFAULT gst_aampclearkeydecryptor_debug_category


/* pad templates */

static GstStaticPadTemplate gst_aampclearkeydecryptor_src_template =
        GST_STATIC_PAD_TEMPLATE("src", GST_PAD_SRC, GST_PAD_ALWAYS,
        GST_STATIC_CAPS("video/x-h264;audio/mpeg;video/x-h265;audio/x-eac3;audio/x-gst-fourcc-ec_3;audio/x-ac3"));

static GstStaticPadTemplate gst_aampclearkeydecryptor_sink_template =
        GST_STATIC_PAD_TEMPLATE("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
                GST_STATIC_CAPS(
                        "application/x-cenc, original-media-type=(string)video/x-h264, protection-system=(string)" CLEARKEY_PROTECTION_SYSTEM_ID "; "
                        "application/x-cenc, original-media-type=(string)video/x-h265, protection-system=(string)" CLEARKEY_PROTECTION_SYSTEM_ID "; "
                        "application/x-cenc, original-media-type=(string)audio/x-eac3, protection-system=(string)" CLEARKEY_PROTECTION_SYSTEM_ID "; "
                        "application/x-cenc, original-media-type=(string)audio/x-ac3, protection-system=(string)" CLEARKEY_PROTECTION_SYSTEM_ID "; "
                        "application/x-cenc, original-media-type=(string)audio/x-gst-fourcc-ec_3, protection-system=(string)" CLEARKEY_PROTECTION_SYSTEM_ID "; "
                        "application/x-cenc, original-media-type=(string)audio/mpeg, protection-system=(string)" CLEARKEY_PROTECTION_SYSTEM_ID));

static GstStaticPadTemplate gst_aampclearkeydecryptor_dummy_sink_template =
        GST_STATIC_PAD_TEMPLATE("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
                GST_STATIC_CAPS("clearkey/x-unused")); // unused?

/**
 * @brief clearkey decryptor class initialization
 * @param klass Gstreamer Class
 */
static void gst_aampclearkeydecryptor_class_init(
        GstAampclearkeydecryptorClass * klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GstElementClass* elementClass = GST_ELEMENT_CLASS(klass);

    DEBUG_FUNC();

    gobject_class->finalize = gst_aampclearkeydecryptor_finalize;

    /* Setting up pads and setting metadata should be moved to
    base_class_init if you intend to subclass this class. */
    gst_element_class_add_static_pad_template(elementClass, &gst_aampclearkeydecryptor_src_template);
    gst_element_class_add_static_pad_template(elementClass, &gst_aampclearkeydecryptor_sink_template);

    gst_element_class_set_static_metadata(elementClass,
            "Decrypt ClearKey encrypted contents",
            GST_ELEMENT_FACTORY_KLASS_DECRYPTOR,
            "Decrypts streams encrypted using ClearKey Encryption.",
            "Comcast");
}

/**
 * @brief ClearKey decryptor element initialization
 * @param aampclearkeydecryptor clearkey decryptor element pointer
 */
static void gst_aampclearkeydecryptor_init(GstAampclearkeydecryptor *aampclearkeydecryptor)
{
    DEBUG_FUNC();
}

/**
 * @brief clearkey decryptor element termination
 * @param object clearkey decryptor element pointer
 */
static void gst_aampclearkeydecryptor_finalize(GObject * object)
{
    DEBUG_FUNC();
    GST_CALL_PARENT(G_OBJECT_CLASS, finalize, (object));
}
#endif // UBUNTU
