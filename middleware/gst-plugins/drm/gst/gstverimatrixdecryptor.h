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
 * @file gstverimatrixdecryptor.h
 * @brief Widevine decryptor plugin declarations
 */


#ifndef _GST_VERIMATRIXDECRYPTOR_H_
#define _GST_VERIMATRIXDECRYPTOR_H_

#include <gst/gst.h>
#include <gst/base/gstbasetransform.h>
#include "DrmSessionManager.h"

#include "gstcdmidecryptor.h"  // For base gobject

// Declared static here because this string exists in player and gstplugin .so
// library files  This string needs to match the start
// of the gsteamer plugin name as created by the macros.
static const char* GstPluginNameVMX = "verimatrixdecryptor";

G_BEGIN_DECLS

#define VERIMATRIX_PROTECTION_SYSTEM_ID "9a27dd82-fde2-4725-8cbc-4234aa06ec09"

#define GST_TYPE_VERIMATRIXDECRYPTOR             (gst_verimatrixdecryptor_get_type())
#define GST_VERIMATRIXDECRYPTOR(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_VERIMATRIXDECRYPTOR, Gstverimatrixdecryptor))
#define GST_VERIMATRIXDECRYPTOR_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_VERIMATRIXDECRYPTOR, GstverimatrixdecryptorClass))
#define GST_IS_VERIMATRIXDECRYPTOR(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_VERIMATRIXDECRYPTOR))
#define GST_IS_VERIMATRIXDECRYPTOR_CLASS(obj)    (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_VERIMATRIXDECRYPTOR))

typedef struct _Gstverimatrixdecryptor Gstverimatrixdecryptor;
typedef struct _GstverimatrixdecryptorClass GstverimatrixdecryptorClass;
//typedef struct _GstverimatrixdecryptorPrivate GstverimatrixdecryptorPrivate;

/**
 * @struct _Gstverimatrixdecryptor
 * @brief GstElement structure override for Widevine decryptor
 */
struct _Gstverimatrixdecryptor
{
    GstCDMIDecryptor                parent;
//    GstverimatrixdecryptorPrivate    priv;
};

/**
 * @struct _GstverimatrixdecryptorClass
 * @brief GstElementClass structure override for Widevine decryptor
 */
struct _GstverimatrixdecryptorClass
{
    GstCDMIDecryptorClass parentClass;
};

/**
 * @brief Get type of Verimatrix decryptor
 * @retval Type of Verimatrix decryptor
 */
GType gst_verimatrixdecryptor_get_type (void);

G_END_DECLS


#endif
