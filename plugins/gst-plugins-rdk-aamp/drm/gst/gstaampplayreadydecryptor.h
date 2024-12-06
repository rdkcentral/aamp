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
 * @file gstaampplayreadydecryptor.h
 * @brief aamp Playready decryptor plugin declarations
 */

#ifndef _GST_AAMPPLAYREADYDECRYPTOR_H_
#define _GST_AAMPPLAYREADYDECRYPTOR_H_

#include <gst/gst.h>
#include <gst/base/gstbasetransform.h>
#include "AampDRMSessionManager.h"
#include "priv_aamp.h"

#include "gstaampcdmidecryptor.h"  // For base gobject

// Declared static here because this string exists in libaamp.so
// and libgstaampplugin.so  This string needs to match the start
// of the gsteamer plugin name as created by the macros.
static const char* GstPluginNamePR = "aampplayreadydecryptor";

G_BEGIN_DECLS

#define PLAYREADY_PROTECTION_SYSTEM_ID "9a04f079-9840-4286-ab92-e65be0885f95"

#define GST_TYPE_AAMPPLAYREADYDECRYPTOR             (gst_aampplayreadydecryptor_get_type())
#define GST_AAMPPLAYREADYDECRYPTOR(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_AAMPPLAYREADYDECRYPTOR, GstAampplayreadydecryptor))
#define GST_AAMPPLAYREADYDECRYPTOR_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_AAMPPLAYREADYDECRYPTOR, GstAampplayreadydecryptorClass))
#define GST_IS_AAMPPLAYREADYDECRYPTOR(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_AAMPPLAYREADYDECRYPTOR))
#define GST_IS_AAMPPLAYREADYDECRYPTOR_CLASS(obj)    (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_AAMPPLAYREADYDECRYPTOR))

typedef struct _GstAampplayreadydecryptor GstAampplayreadydecryptor;
typedef struct _GstAampplayreadydecryptorClass GstAampplayreadydecryptorClass;

/**
 * @struct _GstAampplayreadydecryptor
 * @brief GstElement structure override for playready decryptor
 */
struct _GstAampplayreadydecryptor
{
    GstAampCDMIDecryptor                parent;
};

/**
 * @struct _GstAampplayreadydecryptorClass
 * @brief GstElementClass structure override for playready decryptor
 */
struct _GstAampplayreadydecryptorClass
{
    GstAampCDMIDecryptorClass parentClass;
};


/**
 * @brief Get type of playready decryptor
 * @retval Type of playready decryptor
 */
GType gst_aampplayreadydecryptor_get_type (void);

G_END_DECLS


#endif
