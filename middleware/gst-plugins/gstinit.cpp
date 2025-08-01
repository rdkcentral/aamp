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
 * @file gstinit.cpp
 * @brief gstreamer plugin initialization
 */

#include <gst/gst.h>
#include <cstdio>
#ifdef DRM_BUILD_PROFILE
#include "gstplayreadydecryptor.h"
#include "gstwidevinedecryptor.h"
#include "gstclearkeydecryptor.h"
#include "gstverimatrixdecryptor.h"
#endif


/**
 * @brief plugin_init , invoked by gstreamer core on load. Registers player gstreamer elements.
 * @param plugin GstPlugin to which elements should be registered
 * @retval status of operation
 */
static gboolean plugin_init(GstPlugin * plugin)
{
	gboolean ret =  false;
#ifdef DRM_BUILD_PROFILE
	ret = gst_element_register(plugin, GstPluginNamePR,
			GST_RANK_PRIMARY, GST_TYPE_PLAYREADYDECRYPTOR );
	if(ret)
	{
		printf("player plugin_init registered %s element\n", GstPluginNamePR);
	}
	else
	{
		printf("player plugin_init FAILED to register %s element\n", GstPluginNamePR);
	}
	ret = gst_element_register(plugin, GstPluginNameWV,
			GST_RANK_PRIMARY, GST_TYPE_WIDEVINEDECRYPTOR );
	if(ret)
	{
		printf("player plugin_init registered %s element\n", GstPluginNameWV);
	}
	else
	{
		printf("player plugin_init FAILED to register %s element\n", GstPluginNameWV);
	}
	ret = gst_element_register(plugin, GstPluginNameCK,
			GST_RANK_PRIMARY, GST_TYPE_CLEARKEYDECRYPTOR );
	if(ret)
	{
		printf("player plugin_init registered %s element\n", GstPluginNameCK);
	}
	else
	{
		printf("player plugin_init FAILED to register %s element\n", GstPluginNameCK);
	}
	ret = gst_element_register(plugin, GstPluginNameVMX,
			GST_RANK_PRIMARY, GST_TYPE_VERIMATRIXDECRYPTOR );
	if(ret)
	{
		printf("player plugin_init registered %s element\n", GstPluginNameVMX);
	}
	else
	{
		printf("player plugin_init FAILED to register %s element\n", GstPluginNameVMX);
	}
#else
#endif
	return ret;
}

#ifndef VERSION
#define VERSION "0.0.1"
#endif
#ifndef PACKAGE
#define PACKAGE "RDK"
#endif
#ifndef PACKAGE_NAME
#define PACKAGE_NAME "plugin"
#endif
#ifndef GST_PACKAGE_ORIGIN
#define GST_PACKAGE_ORIGIN "https://rdkcentral.com/"
#endif

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
		GST_VERSION_MINOR,
		plugin,
		"Interface Player",
		plugin_init, VERSION, "LGPL", PACKAGE_NAME, GST_PACKAGE_ORIGIN)
