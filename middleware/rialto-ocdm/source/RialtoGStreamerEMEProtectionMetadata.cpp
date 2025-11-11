/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2024 Sky UK
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

#include "RialtoGStreamerEMEProtectionMetadata.h"
#include <gst/gstconfig.h>

// NOLINTNEXTLINE(build/function_format)
static gboolean rialto_eme_protection_metadata_init(GstMeta *meta, gpointer params, GstBuffer *buffer)
{
    // NOLINTNEXTLINE(readability/casting)
    GstRialtoProtectionMetadata *emeta = reinterpret_cast<GstRialtoProtectionMetadata *>(meta);

    emeta->info = NULL;

    return TRUE;
}

// NOLINTNEXTLINE(build/function_format)
static gboolean rialto_eme_protection_metadata_free(GstMeta *meta, GstBuffer *buffer)
{
    // NOLINTNEXTLINE(readability/casting)
    GstRialtoProtectionMetadata *emeta = reinterpret_cast<GstRialtoProtectionMetadata *>(meta);

    if (emeta->info)
    {
        gst_structure_free(emeta->info);
        emeta->info = nullptr;
    }

    return TRUE;
}

// NOLINTNEXTLINE(build/function_format)
GST_EXPORT GType rialto_eme_protection_metadata_get_type()
{
    static GType g_type{0};
    static const gchar *api_tags[] = {"rialto", "protection", NULL};

    if (g_once_init_enter(&g_type))
    {
        GType _type = gst_meta_api_type_register("GstRialtoProtectionMetadataAPI", api_tags);
        g_once_init_leave(&g_type, _type);
    }
    return g_type;
}

// NOLINTNEXTLINE(build/function_format)
const GstMetaInfo *rialto_mse_protection_metadata_get_info()
{
    static const GstMetaInfo *metainfo = NULL;
    if (g_once_init_enter(&metainfo))
    {
        const GstMetaInfo *gstMeta =
            gst_meta_register(GST_RIALTO_PROTECTION_METADATA_GET_TYPE, "GstRialtoProtectionMetadata",
                              sizeof(GstRialtoProtectionMetadata),
                              (GstMetaInitFunction)rialto_eme_protection_metadata_init,
                              (GstMetaFreeFunction)rialto_eme_protection_metadata_free, (GstMetaTransformFunction)NULL);

        g_once_init_leave(&metainfo, gstMeta);
    }
    return metainfo;
}

// NOLINTNEXTLINE(build/function_format)
GstRialtoProtectionMetadata *rialto_mse_add_protection_metadata(GstBuffer *gstBuffer, GstStructure *info)
{
    GstRialtoProtectionMetadata *metadata = reinterpret_cast<GstRialtoProtectionMetadata *>(
        gst_buffer_add_meta(gstBuffer, GST_RIALTO_PROTECTION_METADATA_INFO, NULL));
    metadata->info = info;
    return metadata;
}
