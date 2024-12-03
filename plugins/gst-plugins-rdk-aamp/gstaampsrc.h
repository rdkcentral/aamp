/*
* Copyright 2018 RDK Management
*
* This library is free software; you can redistribute it and/or
* modify it under the terms of the GNU Library General Public
* License as published by the Free Software Foundation, version 2
* of the license.
*
* This library is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
* Library General Public License for more details.
*
* You should have received a copy of the GNU Library General Public
* License along with this library; if not, write to the
* Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
* Boston, MA 02110-1301, USA.
*/

/**
 * @file gstaampsrc.h
 * @brief  aampsrc gstreamer element specific declarations
 */


#ifndef _GST_AAMPSRC_H_
#define _GST_AAMPSRC_H_

#include <gst/gst.h>
#include <gst/base/gstpushsrc.h>

G_BEGIN_DECLS

#define GST_TYPE_AAMPSRC   (gst_aampsrc_get_type())
#define GST_AAMPSRC(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_AAMPSRC,GstAampSrc))
#define GST_AAMPSRC_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_AAMPSRC,GstAampSrcClass))
#define GST_IS_AAMPSRC(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_AAMPSRC))
#define GST_IS_AAMPSRC_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_AAMPSRC))

/**
 * @struct GstAampSrc 
 * @brief AampSrc GstElement extension
 */
typedef struct _GstAampSrc GstAampSrc;
/**
 * @struct GstAampSrcclass
 * @brief AampSrc GstElementClass extension
 */
typedef struct _GstAampSrcClass GstAampSrcClass;

/**
 * @struct _GstAampSrc
 * @brief AampSrc GstElement extension
 */
struct _GstAampSrc
{
	GstPushSrc base_aampsrc;
	gchar *location;
	gboolean first_buffer_sent;
	GCond block_push_cond;
	gboolean block_push;
	GMutex mutex;
//	GstPad *srcpad;
};

/**
 * @struct _GstAampSrcClass
 * @brief  AampSrc GstElementClass extension
 */
struct _GstAampSrcClass
{
	GstPushSrcClass base_aampsrc_class;
};


/**
 * @brief Get type of aampsrc gstreamer element
 * @retval type of aampsrc element
 */
GType gst_aampsrc_get_type(void);

G_END_DECLS

#endif
