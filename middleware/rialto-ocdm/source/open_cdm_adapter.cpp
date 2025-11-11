/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2022 Sky UK
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

#include "Logger.h"
#include "OpenCDMSession.h"
#include <opencdm/open_cdm_adapter.h>

namespace
{
const Logger kLog{"open_cdm_adapter"};
} // namespace

OpenCDMError opencdm_gstreamer_session_decrypt_ex(struct OpenCDMSession *session, GstBuffer *buffer,
                                                  GstBuffer *subSample, const uint32_t subSampleCount, GstBuffer *IV,
                                                  GstBuffer *keyID, uint32_t initWithLast15, GstCaps *caps)
{
    if (nullptr == session)
    {
        kLog << error << "Failed to decrypt - session is NULL";
        return ERROR_FAIL;
    }
    session->addProtectionMeta(buffer, subSample, subSampleCount, IV, keyID, initWithLast15);
    return ERROR_NONE;
}

OpenCDMError opencdm_gstreamer_session_decrypt(struct OpenCDMSession *session, GstBuffer *buffer, GstBuffer *subSample,
                                               const uint32_t subSampleCount, GstBuffer *IV, GstBuffer *keyID,
                                               uint32_t initWithLast15)
{
    return opencdm_gstreamer_session_decrypt_ex(session, buffer, subSample, subSampleCount, IV, keyID, initWithLast15,
                                                nullptr);
}

OpenCDMError opencdm_gstreamer_session_decrypt_buffer(struct OpenCDMSession *session, GstBuffer *buffer, GstCaps *caps)
{
    if (nullptr == session)
    {
        kLog << error << "Failed to decrypt - session is NULL";
        return ERROR_FAIL;
    }

    if (!session->addProtectionMeta(buffer))
    {
        kLog << error << "Failed to decrypt - could not append protection meta";
        return ERROR_FAIL;
    }

    return ERROR_NONE;
}

OpenCDMError opencdm_gstreamer_transform_caps(GstCaps **caps)
{
    return ERROR_NONE;
}
