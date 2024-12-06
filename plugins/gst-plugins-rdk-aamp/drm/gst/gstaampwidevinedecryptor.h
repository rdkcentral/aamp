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
 * @file gstaampwidevinedecryptor.h
 * @brief aamp Widevine decryptor plugin declarations
 */


#ifndef _GST_AAMPWIDEVINEDECRYPTOR_H_
#define _GST_AAMPWIDEVINEDECRYPTOR_H_

#include <gst/gst.h>
#include <gst/base/gstbasetransform.h>
#include "AampDRMSessionManager.h"
#include "priv_aamp.h"

#include "gstaampcdmidecryptor.h"  // For base gobject

// Declared static here because this string exists in libaamp.so
// and libgstaampplugin.so  This string needs to match the start
// of the gsteamer plugin name as created by the macros.
static const char* GstPluginNameWV = "aampwidevinedecryptor";

G_BEGIN_DECLS

#define WIDEVINE_PROTECTION_SYSTEM_ID "edef8ba9-79d6-4ace-a3c8-27dcd51d21ed"

#define GST_TYPE_AAMPWIDEVINEDECRYPTOR             (gst_aampwidevinedecryptor_get_type())
#define GST_AAMPWIDEVINEDECRYPTOR(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_AAMPWIDEVINEDECRYPTOR, GstAampwidevinedecryptor))
#define GST_AAMPWIDEVINEDECRYPTOR_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_AAMPWIDEVINEDECRYPTOR, GstAampwidevinedecryptorClass))
#define GST_IS_AAMPWIDEVINEDECRYPTOR(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_AAMPWIDEVINEDECRYPTOR))
#define GST_IS_AAMPWIDEVINEDECRYPTOR_CLASS(obj)    (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_AAMPWIDEVINEDECRYPTOR))

typedef struct _GstAampwidevinedecryptor GstAampwidevinedecryptor;
typedef struct _GstAampwidevinedecryptorClass GstAampwidevinedecryptorClass;
//typedef struct _GstAampwidevinedecryptorPrivate GstAampwidevinedecryptorPrivate;

/**
 * @struct _GstAampwidevinedecryptor
 * @brief GstElement structure override for Widevine decryptor
 */
struct _GstAampwidevinedecryptor
{
    GstAampCDMIDecryptor                parent;
//    GstAampwidevinedecryptorPrivate    priv;
};

/**
 * @struct _GstAampwidevinedecryptorClass
 * @brief GstElementClass structure override for Widevine decryptor
 */
struct _GstAampwidevinedecryptorClass
{
    GstAampCDMIDecryptorClass parentClass;
};

/**
 * @brief Get type of Widevine decryptor
 * @retval Type of Widevine decryptor
 */
GType gst_aampwidevinedecryptor_get_type (void);

G_END_DECLS


#endif
