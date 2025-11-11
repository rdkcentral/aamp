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

#ifndef FIREBOLT_RIALTO_SERVER_GST_PROTECTION_METADATA_H_ // NOLINT(build/header_guard)
#define FIREBOLT_RIALTO_SERVER_GST_PROTECTION_METADATA_H_

#include "IDecryptionService.h"
#include <gst/gst.h>

G_BEGIN_DECLS

#define GST_RIALTO_PROTECTION_METADATA_GET_TYPE (rialto_eme_protection_metadata_get_type())
#define GST_RIALTO_PROTECTION_METADATA_INFO (rialto_mse_protection_metadata_get_info())

struct GstRialtoProtectionData
{
    int32_t keySessionId = 0;
    uint32_t subsampleCount = 0;
    uint32_t initWithLast15 = 0;
    GstBuffer *key = nullptr;
    GstBuffer *iv = nullptr;
    GstBuffer *subsamples = nullptr;
    firebolt::rialto::CipherMode cipherMode = firebolt::rialto::CipherMode::UNKNOWN;
    uint32_t crypt = 0;
    uint32_t skip = 0;
    bool encryptionPatternSet = false;
    firebolt::rialto::server::IDecryptionService *decryptionService = nullptr;
};

struct _GstRialtoProtectionMetadata
{
    GstMeta parent;
    GstRialtoProtectionData data;
};

typedef struct _GstRialtoProtectionMetadata GstRialtoProtectionMetadata;

GType rialto_eme_protection_metadata_get_type();              // NOLINT(build/function_format)
const GstMetaInfo *rialto_mse_protection_metadata_get_info(); // NOLINT(build/function_format)
G_END_DECLS

#endif // FIREBOLT_RIALTO_SERVER_GST_PROTECTION_METADATA_H_
