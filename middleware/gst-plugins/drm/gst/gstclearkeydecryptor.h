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
 * @file gstclearkeydecryptor.h
 * @brief clear key decryptor plugin declarations
 */


#ifndef GSTCLEARKEYDECRYPTOR_H_
#define GSTCLEARKEYDECRYPTOR_H_


#include <gst/gst.h>
#include <gst/base/gstbasetransform.h>
#include "DrmSessionManager.h"

#include "gstcdmidecryptor.h"  // For base gobject

// Declared static here because this string exists in player and gstplugin .so
// library files  This string needs to match the start
// of the gsteamer plugin name as created by the macros.
static const char* GstPluginNameCK = "clearkeydecryptor";

G_BEGIN_DECLS

#define CLEARKEY_PROTECTION_SYSTEM_ID "1077efec-c0b2-4d02-ace3-3c1e52e2fb4b"

#define GST_TYPE_CLEARKEYDECRYPTOR             (gst_clearkeydecryptor_get_type())
#define GST_CLEARKEYDECRYPTOR(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_CLEARKEYDECRYPTOR, Gstclearkeydecryptor))
#define GST_CLEARKEYDECRYPTOR_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_CLEARKEYDECRYPTOR, GstclearkeydecryptorClass))
#define GST_IS_CLEARKEYDECRYPTOR(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_CLEARKEYDECRYPTOR))
#define GST_IS_CLEARKEYDECRYPTOR_CLASS(obj)    (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_CLEARKEYDECRYPTOR))

typedef struct _Gstclearkeydecryptor Gstclearkeydecryptor;
typedef struct _GstclearkeydecryptorClass GstclearkeydecryptorClass;

/**
 * @struct _Gstclearkeydecryptor
 * @brief GstElement structure override for clearkey decryptor
 */
struct _Gstclearkeydecryptor
{
	GstCDMIDecryptor                parent;
};

/**
 * @struct _GstclearkeydecryptorClass
 * @brief GstElementClass structure override for clearkey decryptor
 */
struct _GstclearkeydecryptorClass
{
	GstCDMIDecryptorClass parentClass;
};


/**
 * @brief Get type of clearkey decryptor
 * @retval Type of clearkey decryptor
 */
GType gst_clearkeydecryptor_get_type (void);

G_END_DECLS


#endif /* GSTCLEARKEYDECRYPTOR_H_ */
