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
 * @file gstaampsrc.cpp
 * @brief aampsrc gstreamer element specific defines
 */


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <string.h>
#include "gstaampsrc.h"

GST_DEBUG_CATEGORY_STATIC (gst_aampsrc_debug_category);
#define GST_CAT_DEFAULT gst_aampsrc_debug_category

/* prototypes */

static GstStateChangeReturn
gst_aampsrc_change_state(GstElement * element, GstStateChange transition);
static void gst_aampsrc_set_property(GObject * object, guint property_id, const GValue * value, GParamSpec * pspec);
static void gst_aampsrc_get_property(GObject * object, guint property_id, GValue * value, GParamSpec * pspec);
static void gst_aampsrc_finalize(GObject * object);
static gboolean gst_aampsrc_query(GstBaseSrc * element, GstQuery * query);

/**
 * @enum GstAampsrcProperties
 * @brief aampsrc properties
 */
enum
{
	PROP_0,
	PROP_LOCATION
};

static GstStaticPadTemplate gst_aampsrc_src_template = GST_STATIC_PAD_TEMPLATE("src", GST_PAD_SRC, GST_PAD_ALWAYS,
		GST_STATIC_CAPS("application/x-aamp;"));

/**
 * @brief get_uri implementation for URI handler interface
 * @param[in] handler URI handler
 * @retval uri of asset
 */
static gchar *gst_aamp_uriif_get_uri(GstURIHandler * handler)
{
	GstAampSrc *aampsrc = GST_AAMPSRC(handler);
	gchar * ret = NULL;

	if (NULL != aampsrc->location)
	{
		ret = g_strdup(aampsrc->location);
		memcpy(aampsrc->location, "aamp", 4);
	}
	return ret;
}


/**
 * @brief Set URI of asset
 * @param[in] aampsrc pointer of gstaampsrc
 * @param[in] uri URI to be set
 * @retval TRUE on success, FALSE on failure
 */
static gboolean gst_aamp_set_location(GstAampSrc *aampsrc, const gchar * uri)
{
	gboolean ret = TRUE;
	if (NULL == uri)
	{
		GST_ERROR_OBJECT(aampsrc, "NULL uri\n");
		ret = FALSE;
	}
	else if (memcmp(uri, "aamp", 4))
	{
		GST_ERROR_OBJECT(aampsrc, "Invalid url %s\n", uri);
		ret = FALSE;
	}
	else
	{
		GST_DEBUG_OBJECT(aampsrc, "uri %s\n", uri);
		if (aampsrc->location)
		{
			g_free(aampsrc->location);
		}
		aampsrc->location = g_strdup(uri);
		memcpy(aampsrc->location, "http", 4);
	}
	return ret;
}

/**
 * @brief set_uri implementation for URI handler interface
 * @param[in] handler URI handler
 * @param[in] uri URI to be set
 * @param[in] error
 * @retval TRUE on success, FALSE on failure
 */
static gboolean gst_aamp_uriif_set_uri(GstURIHandler * handler, const gchar * uri, GError **error)
{
	GstAampSrc *aampsrc = GST_AAMPSRC(handler);
	return gst_aamp_set_location (aampsrc, uri );
}

/**
 * @brief get_protocols implementation for URI handler interface
 * @param[in] type unused
 * @retval protocols supported
 */
static const gchar * const *gst_aamp_uriif_get_protocols(GType type)
{
	static const gchar *protocols[] = { "aamp", "aamps", NULL };
	return protocols;
}

/**
 * @brief get_type implementation for URI handler interface
 * @param[in] type unused
 * @retval GST_URI_SRC
 */
static GstURIType gst_aamp_uriif_get_type(GType type)
{
	return GST_URI_SRC;
}

/**
 * @brief initialize URI handler interface
 * @param[out] interface URI handler interface
 * @param[in] interface_data Unused
 */
static void gst_aamp_uriif_handler_init(gpointer interface, gpointer interface_data)
{
	GstURIHandlerInterface *iface = (GstURIHandlerInterface *) interface;

	iface->get_uri = gst_aamp_uriif_get_uri;
	iface->set_uri = gst_aamp_uriif_set_uri;
	iface->get_protocols = gst_aamp_uriif_get_protocols;
	iface->get_type = gst_aamp_uriif_get_type;
}
#define AAMP_TYPE_INIT_CODE { \
	GST_DEBUG_CATEGORY_INIT (gst_aampsrc_debug_category, "aampsrc", 0, \
		"debug category for aampsrc element"); \
	G_IMPLEMENT_INTERFACE (GST_TYPE_URI_HANDLER, \
			gst_aamp_uriif_handler_init) \
	}

/* class initialization */
G_DEFINE_TYPE_WITH_CODE (GstAampSrc, gst_aampsrc, GST_TYPE_PUSH_SRC, AAMP_TYPE_INIT_CODE);

/**
 * @brief Override of basesrc_class->create, stub
 * @param[in] src pointer of gstaampsrc
 * @param[in] offset Unused
 * @param[in] size Unused
 * @param[out] buf dummy buffer
 * @retval
 */
static GstFlowReturn gst_aampsrc_create(GstBaseSrc *src, guint64 offset, guint size,
        GstBuffer **buf)
{
	GstFlowReturn ret;
	GstAampSrc *aampsrc = GST_AAMPSRC(src);
	if ( aampsrc->first_buffer_sent )
	{
		g_mutex_lock (&aampsrc->mutex);
		g_cond_wait(&aampsrc->block_push_cond, &aampsrc->mutex);
		g_mutex_unlock (&aampsrc->mutex);
		GST_DEBUG_OBJECT(src, "create stub : after wait");
	}
	else
	{
		aampsrc->first_buffer_sent = TRUE;
	}
	*buf = gst_buffer_new_allocate(NULL,8, NULL);
	ret = GST_FLOW_OK;
	return ret;
}


/**
 * @brief Override of element_class->send_event, pass all events to connected Pad
 * @param[in] element pointer of gstaampsrc
 * @param[in] event gstreamer event
 * @retval status of operation
 */
static gboolean gst_aampsrc_send_event(GstElement * element, GstEvent * event)
{
	gboolean ret = TRUE;
	GST_DEBUG_OBJECT(element, " EVENT %s\n", gst_event_type_get_name(GST_EVENT_TYPE(event)));
	if (GST_EVENT_SEEK == GST_EVENT_TYPE(event))
	{
		GstPad *peerPad = gst_pad_get_peer(GST_BASE_SRC_PAD(element));
		if (peerPad)
		{
			ret = gst_pad_push_event(peerPad, event);
			gst_object_unref(peerPad);
		}
	}
	return ret;
}


/**
 * @brief Invoked by gstreamer core to initialize class.
 * @param[in] klass GstAampSrcClass pointer
 */
static void gst_aampsrc_class_init(GstAampSrcClass * klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
	GstElementClass *element_class = GST_ELEMENT_CLASS(klass);
	GstBaseSrcClass *basesrc_class = GST_BASE_SRC_CLASS(klass);

	gobject_class->set_property = gst_aampsrc_set_property;
	gobject_class->get_property = gst_aampsrc_get_property;
	g_object_class_install_property(gobject_class, PROP_LOCATION,
			g_param_spec_string("location", "location", "Location of aamp playlist", "undefined",
					(GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

	gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&gst_aampsrc_src_template));

	gst_element_class_set_static_metadata(GST_ELEMENT_CLASS(klass), "AAMP Source", "Source",
			"Advanced Adaptive Media Player Source", "Comcast");

	gobject_class->finalize = gst_aampsrc_finalize;
	element_class->change_state = GST_DEBUG_FUNCPTR(gst_aampsrc_change_state);
	basesrc_class->query = GST_DEBUG_FUNCPTR(gst_aampsrc_query);
	basesrc_class->create = GST_DEBUG_FUNCPTR(gst_aampsrc_create);
	element_class->send_event = GST_DEBUG_FUNCPTR(gst_aampsrc_send_event);
}


/**
 * @brief Invoked by gstreamer core to initialize element.
 * @param[in] aampsrc gstaampsrc pointer
 */
static void gst_aampsrc_init(GstAampSrc * aampsrc)
{
	aampsrc->location = NULL;
	aampsrc->first_buffer_sent = FALSE;
	g_mutex_init (&aampsrc->mutex);
	g_cond_init (&aampsrc->block_push_cond);
}


/**
 * @brief Set element property. Invoked by gstreamer core
 * @param[in] object gstaampsrc pointer
 * @param[in] property_id id of property
 * @param[in] value contains property value
 * @param[in] pspec Unused
 */
void gst_aampsrc_set_property(GObject * object, guint property_id, const GValue * value, GParamSpec * pspec)
{
	GstAampSrc *aampsrc = GST_AAMPSRC(object);

	GST_DEBUG_OBJECT(aampsrc, "set_property");

	switch (property_id)
	{
		case PROP_LOCATION:
		{
			const gchar *location;
			location = g_value_get_string(value);
			gst_aamp_set_location (aampsrc, location );
			break;
		}

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
			break;
	}
}


/**
 * @brief Get element property. Invoked by gstreamer core
 * @param[in] object gstaampsrc pointer
 * @param[in] property_id id of property
 * @param[out] value contains property value
 * @param[in] pspec Unused
 */
void gst_aampsrc_get_property(GObject * object, guint property_id, GValue * value, GParamSpec * pspec)
{
	GstAampSrc *aampsrc = GST_AAMPSRC(object);

	GST_DEBUG_OBJECT(aampsrc, "get_property");

	switch (property_id)
	{
		case PROP_LOCATION:
		{
			const gchar* location = aampsrc->location;
			if (!location )
			{
				location = "undefined";
			}
			g_value_set_string(value, location);
			break;
		}
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
			break;
	}
}


/**
 * @brief Invoked by gstreamer core to finalize element.
 * @param[in] object gstaampsrc pointer
 */
void gst_aampsrc_finalize(GObject * object)
{
	GstAampSrc *aampsrc = GST_AAMPSRC(object);

	GST_DEBUG_OBJECT(aampsrc, "finalize");

	/* clean up object here */

	G_OBJECT_CLASS(gst_aampsrc_parent_class)->finalize(object);

	if (aampsrc->location)
	{
		g_free(aampsrc->location);
		aampsrc->location = NULL;
	}
	g_mutex_clear (&aampsrc->mutex);
	g_cond_clear (&aampsrc->block_push_cond);
}


/**
 * @brief Invoked by gstreamer core to change element state.
 * @param[in] element gstaampsrc pointer
 * @param[in] trans state
 * @retval status of state change operation
 */
static GstStateChangeReturn gst_aampsrc_change_state(GstElement * element, GstStateChange trans)
{
	GstStateChangeReturn ret;
	GstAampSrc *aampsrc = GST_AAMPSRC(element);

	switch (trans)
	{
		case GST_STATE_CHANGE_READY_TO_PAUSED:
			aampsrc->first_buffer_sent = FALSE;
			break;
		case GST_STATE_CHANGE_PAUSED_TO_READY:
			g_mutex_lock (&aampsrc->mutex);
			g_cond_signal(&aampsrc->block_push_cond);
			g_mutex_unlock (&aampsrc->mutex);
			break;
		default:
			break;
	}
	ret = GST_ELEMENT_CLASS(gst_aampsrc_parent_class)->change_state(element, trans);

	return ret;
}


/**
 * @brief Element query override .invoked by gstreamer core
 * @param[in] element gstaampsrc pointer
 * @param[in] query gstreamer query
 * @retval TRUE if query is handled, FALSE if not handled
 */
static gboolean gst_aampsrc_query(GstBaseSrc * element, GstQuery * query)
{
	GstAampSrc *aampsrc = GST_AAMPSRC(element);
	gboolean ret = FALSE;

	if (GST_QUERY_URI == GST_QUERY_TYPE(query))
	{
		gst_query_set_uri(query, aampsrc->location);
		ret = TRUE;
	}
	else if (GST_QUERY_LATENCY == GST_QUERY_TYPE(query))
	{
		gst_query_set_latency(query, TRUE, 0, 0);
		ret = TRUE;
	}
	else
	{
		GST_DEBUG_OBJECT(aampsrc, " query %s\n", gst_query_type_get_name(GST_QUERY_TYPE(query)));
		ret = GST_BASE_SRC_CLASS(gst_aampsrc_parent_class)->query(element, query);
	}

	return ret;
}



