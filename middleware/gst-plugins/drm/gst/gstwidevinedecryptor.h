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
 * @file gstwidevinedecryptor.h
 * @brief Widevine decryptor plugin declarations
 */


#ifndef _GST_WIDEVINEDECRYPTOR_H_
#define _GST_WIDEVINEDECRYPTOR_H_

#include <gst/gst.h>
#include <gst/base/gstbasetransform.h>
#include "DrmSessionManager.h"

#include "gstcdmidecryptor.h"  // For base gobject

// Declared static here because this string exists in player and gstplugin .so
// library files  This string needs to match the start
// of the gsteamer plugin name as created by the macros.
static const char* GstPluginNameWV = "widevinedecryptor";

G_BEGIN_DECLS

#define WIDEVINE_PROTECTION_SYSTEM_ID "edef8ba9-79d6-4ace-a3c8-27dcd51d21ed"

#define GST_TYPE_WIDEVINEDECRYPTOR             (gst_widevinedecryptor_get_type())
#define GST_WIDEVINEDECRYPTOR(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_WIDEVINEDECRYPTOR, Gstwidevinedecryptor))
#define GST_WIDEVINEDECRYPTOR_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_WIDEVINEDECRYPTOR, GstwidevinedecryptorClass))
#define GST_IS_WIDEVINEDECRYPTOR(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_WIDEVINEDECRYPTOR))
#define GST_IS_WIDEVINEDECRYPTOR_CLASS(obj)    (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_WIDEVINEDECRYPTOR))

typedef struct _Gstwidevinedecryptor Gstwidevinedecryptor;
typedef struct _GstwidevinedecryptorClass GstwidevinedecryptorClass;
//typedef struct _GstwidevinedecryptorPrivate GstwidevinedecryptorPrivate;

/**
 * @struct _Gstwidevinedecryptor
 * @brief GstElement structure override for Widevine decryptor
 */
struct _Gstwidevinedecryptor
{
    GstCDMIDecryptor                parent;
//    GstwidevinedecryptorPrivate    priv;
};

/**
 * @struct _GstwidevinedecryptorClass
 * @brief GstElementClass structure override for Widevine decryptor
 */
struct _GstwidevinedecryptorClass
{
    GstCDMIDecryptorClass parentClass;
};

/**
 * @brief Get type of Widevine decryptor
 * @retval Type of Widevine decryptor
 */
GType gst_widevinedecryptor_get_type (void);

G_END_DECLS


#endif
