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
 * @file gstplayreadydecryptor.h
 * @brief  Playready decryptor plugin declarations
 */

#ifndef _GST_PLAYREADYDECRYPTOR_H_
#define _GST_PLAYREADYDECRYPTOR_H_

#include <gst/gst.h>
#include <gst/base/gstbasetransform.h>
#include "DrmSessionManager.h"

#include "gstcdmidecryptor.h"  // For base gobject

// Declared static here because this string exists in player and gstplugin .so
// libraries  This string needs to match the start
// of the gsteamer plugin name as created by the macros.
static const char* GstPluginNamePR = "playreadydecryptor";

G_BEGIN_DECLS

#define PLAYREADY_PROTECTION_SYSTEM_ID "9a04f079-9840-4286-ab92-e65be0885f95"

#define GST_TYPE_PLAYREADYDECRYPTOR             (gst_playreadydecryptor_get_type())
#define GST_PLAYREADYDECRYPTOR(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_PLAYREADYDECRYPTOR, Gstplayreadydecryptor))
#define GST_PLAYREADYDECRYPTOR_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_PLAYREADYDECRYPTOR, GstplayreadydecryptorClass))
#define GST_IS_PLAYREADYDECRYPTOR(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_PLAYREADYDECRYPTOR))
#define GST_IS_PLAYREADYDECRYPTOR_CLASS(obj)    (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_PLAYREADYDECRYPTOR))

typedef struct _Gstplayreadydecryptor Gstplayreadydecryptor;
typedef struct _GstplayreadydecryptorClass GstplayreadydecryptorClass;

/**
 * @struct _Gstplayreadydecryptor
 * @brief GstElement structure override for playready decryptor
 */
struct _Gstplayreadydecryptor
{
	GstCDMIDecryptor                parent;
};

/**
 * @struct _GstplayreadydecryptorClass
 * @brief GstElementClass structure override for playready decryptor
 */
struct _GstplayreadydecryptorClass
{
	GstCDMIDecryptorClass parentClass;
};


/**
 * @brief Get type of playready decryptor
 * @retval Type of playready decryptor
 */
GType gst_playreadydecryptor_get_type (void);

G_END_DECLS


#endif
