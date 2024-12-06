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
 * @file gstaampverimatrixdecryptor.h
 * @brief aamp Widevine decryptor plugin declarations
 */


#ifndef _GST_AAMPVERIMATRIXDECRYPTOR_H_
#define _GST_AAMPVERIMATRIXDECRYPTOR_H_

#include <gst/gst.h>
#include <gst/base/gstbasetransform.h>
#include "AampDRMSessionManager.h"
#include "priv_aamp.h"

#include "gstaampcdmidecryptor.h"  // For base gobject

// Declared static here because this string exists in libaamp.so
// and libgstaampplugin.so  This string needs to match the start
// of the gsteamer plugin name as created by the macros.
static const char* GstPluginNameVMX = "aampverimatrixdecryptor";

G_BEGIN_DECLS

#define VERIMATRIX_PROTECTION_SYSTEM_ID "9a27dd82-fde2-4725-8cbc-4234aa06ec09"

#define GST_TYPE_AAMPVERIMATRIXDECRYPTOR             (gst_aampverimatrixdecryptor_get_type())
#define GST_AAMPVERIMATRIXDECRYPTOR(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_AAMPVERIMATRIXDECRYPTOR, GstAampverimatrixdecryptor))
#define GST_AAMPVERIMATRIXDECRYPTOR_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_AAMPVERIMATRIXDECRYPTOR, GstAampverimatrixdecryptorClass))
#define GST_IS_AAMPVERIMATRIXDECRYPTOR(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_AAMPVERIMATRIXDECRYPTOR))
#define GST_IS_AAMPVERIMATRIXDECRYPTOR_CLASS(obj)    (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_AAMPVERIMATRIXDECRYPTOR))

typedef struct _GstAampverimatrixdecryptor GstAampverimatrixdecryptor;
typedef struct _GstAampverimatrixdecryptorClass GstAampverimatrixdecryptorClass;
//typedef struct _GstAampverimatrixdecryptorPrivate GstAampverimatrixdecryptorPrivate;

/**
 * @struct _GstAampverimatrixdecryptor
 * @brief GstElement structure override for Widevine decryptor
 */
struct _GstAampverimatrixdecryptor
{
    GstAampCDMIDecryptor                parent;
//    GstAampverimatrixdecryptorPrivate    priv;
};

/**
 * @struct _GstAampverimatrixdecryptorClass
 * @brief GstElementClass structure override for Widevine decryptor
 */
struct _GstAampverimatrixdecryptorClass
{
    GstAampCDMIDecryptorClass parentClass;
};

/**
 * @brief Get type of Verimatrix decryptor
 * @retval Type of Verimatrix decryptor
 */
GType gst_aampverimatrixdecryptor_get_type (void);

G_END_DECLS


#endif
