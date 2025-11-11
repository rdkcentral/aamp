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

#ifndef RIALTOG_STREAMEREME_PROTECTION_METADATA_H_
#define RIALTOG_STREAMEREME_PROTECTION_METADATA_H_

#include <gst/gst.h>

G_BEGIN_DECLS

#define GST_RIALTO_PROTECTION_METADATA_GET_TYPE (rialto_eme_protection_metadata_get_type())
#define GST_RIALTO_PROTECTION_METADATA_INFO (rialto_mse_protection_metadata_get_info())

struct _GstRialtoProtectionMetadata
{
    GstMeta parent;
    GstStructure *info;
};

typedef struct _GstRialtoProtectionMetadata GstRialtoProtectionMetadata;

// NOLINTNEXTLINE(build/function_format)
GType rialto_eme_protection_metadata_get_type();
// NOLINTNEXTLINE(build/function_format)
const GstMetaInfo *rialto_mse_protection_metadata_get_info();
// NOLINTNEXTLINE(build/function_format)
GstRialtoProtectionMetadata *rialto_mse_add_protection_metadata(GstBuffer *gstBuffer, GstStructure *info);

G_END_DECLS

#endif // RIALTOG_STREAMEREME_PROTECTION_METADATA_H_
