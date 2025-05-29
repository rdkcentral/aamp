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

#include <gst/gst.h>
#include <gst/base/gstbasetransform.h>
#include <gst/base/gstbytereader.h>
#include "gstverimatrixdecryptor.h"

#define FUNCTION_DEBUG 1
#ifdef FUNCTION_DEBUG
#define DEBUG_FUNC()    g_warning("####### %s : %d ####\n", __FUNCTION__, __LINE__);
#else
#define DEBUG_FUNC()
#endif

/* prototypes */
static void gst_verimatrixdecryptor_finalize(GObject*);

/* class initialization */
#define gst_verimatrixdecryptor_parent_class parent_class
G_DEFINE_TYPE(Gstverimatrixdecryptor, gst_verimatrixdecryptor, GST_TYPE_CDMI_DECRYPTOR);

GST_DEBUG_CATEGORY(gst_verimatrixdecryptor_debug_category);
#define GST_CAT_DEFAULT gst_verimatrixdecryptor_debug_category


/* pad templates */

static GstStaticPadTemplate gst_verimatrixdecryptor_src_template =
        GST_STATIC_PAD_TEMPLATE("src", GST_PAD_SRC, GST_PAD_ALWAYS,
        GST_STATIC_CAPS("video/x-h264;audio/mpeg;video/x-h265;audio/x-eac3;audio/x-gst-fourcc-ec_3;audio/x-ac3"));

static GstStaticPadTemplate gst_verimatrixdecryptor_sink_template =
        GST_STATIC_PAD_TEMPLATE("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
                GST_STATIC_CAPS(
                        "application/x-cenc, original-media-type=(string)video/x-h264, protection-system=(string)" VERIMATRIX_PROTECTION_SYSTEM_ID "; "
                        "application/x-cenc, original-media-type=(string)video/x-h265, protection-system=(string)" VERIMATRIX_PROTECTION_SYSTEM_ID "; "
                        "application/x-cenc, original-media-type=(string)audio/x-eac3, protection-system=(string)" VERIMATRIX_PROTECTION_SYSTEM_ID "; "
                        "application/x-cenc, original-media-type=(string)audio/x-ac3, protection-system=(string)" VERIMATRIX_PROTECTION_SYSTEM_ID "; "
			"application/x-cenc, original-media-type=(string)audio/x-gst-fourcc-ec_3, protection-system=(string)" VERIMATRIX_PROTECTION_SYSTEM_ID "; "
                        "application/x-cenc, original-media-type=(string)audio/mpeg, protection-system=(string)" VERIMATRIX_PROTECTION_SYSTEM_ID));

static GstStaticPadTemplate gst_verimatrixdecryptor_dummy_sink_template =
        GST_STATIC_PAD_TEMPLATE("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
                GST_STATIC_CAPS("verimatrix/x-unused"));


static void gst_verimatrixdecryptor_class_init(GstverimatrixdecryptorClass * klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GstElementClass* elementClass = GST_ELEMENT_CLASS(klass);

    DEBUG_FUNC();

	gobject_class->finalize = gst_verimatrixdecryptor_finalize;

	gst_element_class_add_static_pad_template(elementClass, &gst_verimatrixdecryptor_src_template);
	gst_element_class_add_static_pad_template(elementClass, &gst_verimatrixdecryptor_sink_template);

	gst_element_class_set_static_metadata(elementClass,
	        "Decrypt Verimatrix encrypted contents",
	        GST_ELEMENT_FACTORY_KLASS_DECRYPTOR,
	        "Decrypts streams encrypted using Verimatrix Encryption.",
	        "Comcast");
}

static void gst_verimatrixdecryptor_init(Gstverimatrixdecryptor *verimatrixdecryptor)
{
    DEBUG_FUNC();
}

static void gst_verimatrixdecryptor_finalize(GObject * object)
{
    DEBUG_FUNC();
    GST_CALL_PARENT(G_OBJECT_CLASS, finalize, (object));
}



