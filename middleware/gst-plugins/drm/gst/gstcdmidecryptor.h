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


#ifndef _GST_CDMIDECRYPTOR_H_
#define _GST_CDMIDECRYPTOR_H_


#include <gst/gst.h>
#include <gst/base/gstbasetransform.h>
#include "DrmSessionManager.h"
#include "DrmCallbacks.h"

G_BEGIN_DECLS

#define GST_TYPE_CDMI_DECRYPTOR            (gst_cdmidecryptor_get_type())
#define GST_CDMI_DECRYPTOR(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_CDMI_DECRYPTOR, GstCDMIDecryptor))
#define GST_CDMI_DECRYPTOR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_CDMI_DECRYPTOR, GstCDMIDecryptorClass))
#define GST_IS_CDMI_DECRYPTOR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_CDMI_DECRYPTOR))
#define GST_IS_CDMI_DECRYPTOR_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_CDMI_DECRYPTOR))

typedef struct _GstCDMIDecryptor GstCDMIDecryptor;
typedef struct _GstCDMIDecryptorClass GstCDMIDecryptorClass;

/**
 * @struct _GstCDMIDecryptor
 * @brief GstElement structure override for CDMI decryptor
 */
struct _GstCDMIDecryptor
{
    GstBaseTransform                base_cdmidecryptor;
    class DrmSessionManager*    sessionManager;
    class DrmSession*           drmSession;
    class DrmCallbacks *     player;
    gboolean                        streamReceived;
    gboolean                        canWait;
    gboolean                        firstsegprocessed;
    GstMediaType                   mediaType;

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
    void**                           svpCtx;
};

/**
 * @struct _GstCDMIDecryptorClass
 * @brief GstElementClass structure override for CDMI decryptor
 */
struct _GstCDMIDecryptorClass
{
    GstBaseTransformClass           base_cdmidecryptor_class;
};

/**
 * @brief Get type of CDMI decryptor
 * @retval Type of CDMI decryptor
 */
GType gst_cdmidecryptor_get_type (void);

G_END_DECLS

#endif
