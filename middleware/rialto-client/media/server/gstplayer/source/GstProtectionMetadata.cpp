/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2022 
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "GstProtectionMetadata.h"
#include "RialtoServerLogging.h"
#include <gst/gstconfig.h>
#include <stdio.h>

static gboolean rialto_eme_protection_metadata_init(GstMeta *meta, gpointer params, // NOLINT(build/function_format)
                                                    GstBuffer *buffer)
{
    GstRialtoProtectionMetadata *emeta = reinterpret_cast<GstRialtoProtectionMetadata *>(meta);
    const GstRialtoProtectionData *data = static_cast<GstRialtoProtectionData *>(params);
    emeta->data = *data;

    if (emeta->data.decryptionService)
    {
        emeta->data.decryptionService->incrementSessionIdUsageCounter(emeta->data.keySessionId);
    }

    return true;
}

static gboolean rialto_eme_protection_metadata_free(GstMeta *meta, GstBuffer *buffer) // NOLINT(build/function_format)
{
    GstRialtoProtectionMetadata *protectionMeta = reinterpret_cast<GstRialtoProtectionMetadata *>(meta);

    if (protectionMeta->data.decryptionService)
    {
        protectionMeta->data.decryptionService->decrementSessionIdUsageCounter(protectionMeta->data.keySessionId);
    }

    if (protectionMeta->data.subsamples)
    {
        gst_buffer_unref(protectionMeta->data.subsamples);
    }
    if (protectionMeta->data.iv)
    {
        gst_buffer_unref(protectionMeta->data.iv);
    }
    if (protectionMeta->data.key)
    {
        gst_buffer_unref(protectionMeta->data.key);
    }

    return true;
}

GST_EXPORT GType rialto_eme_protection_metadata_get_type() // NOLINT(build/function_format)
{
    static GType g_type;
    static const gchar *api_tags[] = {"rialto", "protection", NULL};

    if (g_once_init_enter(&g_type))
    {
        GType _type = gst_meta_api_type_register("GstRialtoProtectionMetadataAPI", api_tags);
        g_once_init_leave(&g_type, _type);
    }
    return g_type;
}

const GstMetaInfo *rialto_mse_protection_metadata_get_info() // NOLINT(build/function_format)
{
    static const GstMetaInfo *kMetainfo = NULL;
    if (g_once_init_enter(&kMetainfo))
    {
        const GstMetaInfo *kGstMeta =
            gst_meta_register(GST_RIALTO_PROTECTION_METADATA_GET_TYPE, "GstRialtoProtectionMetadata",
                              sizeof(GstRialtoProtectionMetadata),
                              (GstMetaInitFunction)rialto_eme_protection_metadata_init,
                              (GstMetaFreeFunction)rialto_eme_protection_metadata_free, (GstMetaTransformFunction)NULL);

        g_once_init_leave(&kMetainfo, kGstMeta);
    }
    return kMetainfo;
}
