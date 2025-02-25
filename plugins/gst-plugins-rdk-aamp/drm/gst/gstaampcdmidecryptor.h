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


#ifndef _GST_AAMPCDMIDECRYPTOR_H_
#define _GST_AAMPCDMIDECRYPTOR_H_


#include <gst/gst.h>
#include <gst/base/gstbasetransform.h>
#include "AampDRMSessionManager.h"
#include "priv_aamp.h"


G_BEGIN_DECLS

#define GST_TYPE_AAMP_CDMI_DECRYPTOR            (gst_aampcdmidecryptor_get_type())
#define GST_AAMP_CDMI_DECRYPTOR(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_AAMP_CDMI_DECRYPTOR, GstAampCDMIDecryptor))
#define GST_AAMP_CDMI_DECRYPTOR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_AAMP_CDMI_DECRYPTOR, GstAampCDMIDecryptorClass))
#define GST_IS_AAMP_CDMI_DECRYPTOR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_AAMP_CDMI_DECRYPTOR))
#define GST_IS_AAMP_CDMI_DECRYPTOR_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_AAMP_CDMI_DECRYPTOR))

typedef struct _GstAampCDMIDecryptor GstAampCDMIDecryptor;
typedef struct _GstAampCDMIDecryptorClass GstAampCDMIDecryptorClass;

/**
 * @struct _GstAampCDMIDecryptor
 * @brief GstElement structure override for CDMI decryptor
 */
struct _GstAampCDMIDecryptor
{
    GstBaseTransform                base_aampcdmidecryptor;
    class AampDRMSessionManager*    sessionManager;
    class DrmSession*           drmSession;
    class PrivateInstanceAAMP *     aamp;
    gboolean                        streamReceived;
    gboolean                        canWait;
    gboolean                        firstsegprocessed;
    AampMediaType                   streamtype;

    GMutex                          mutex;
    GCond                           condition;

    GstEvent*                       protectionEvent;
    const gchar*                    selectedProtection;
    gushort                         decryptFailCount;
    gushort			    hdcpOpProtectionFailCount;
    gboolean                        notifyDecryptError;
    gboolean                        streamEncrypted;
    gboolean                        ignoreSVP; //No need for svp for clearKey streams
    GstCaps*                        sinkCaps;
    //GstBuffer*                    initDataBuffer;
    void*                           svpCtx;
};

/**
 * @struct _GstAampCDMIDecryptorClass
 * @brief GstElementClass structure override for CDMI decryptor
 */
struct _GstAampCDMIDecryptorClass
{
    GstBaseTransformClass           base_aampcdmidecryptor_class;
};

/**
 * @brief Get type of CDMI decryptor
 * @retval Type of CDMI decryptor
 */
GType gst_aampcdmidecryptor_get_type (void);

G_END_DECLS

#endif
