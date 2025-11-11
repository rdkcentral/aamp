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

#include "GstProtectionMetadataHelper.h"
#include "GstProtectionMetadataHelperFactory.h"
#include "RialtoServerLogging.h"

namespace firebolt::rialto::server
{
std::shared_ptr<IGstProtectionMetadataHelperFactory> IGstProtectionMetadataHelperFactory::createFactory()
{
    std::shared_ptr<IGstProtectionMetadataHelperFactory> factory;

    try
    {
        factory = std::make_shared<GstProtectionMetadataHelperFactory>();
    }
    catch (const std::exception &e)
    {
        RIALTO_SERVER_LOG_ERROR("Failed to create the protection metadata wrapper factory, reason: %s", e.what());
    }

    return factory;
}

std::unique_ptr<IGstProtectionMetadataHelper> GstProtectionMetadataHelperFactory::createProtectionMetadataWrapper(
    const std::shared_ptr<firebolt::rialto::wrappers::IGstWrapper> &gstWrapper) const
{
    return std::make_unique<GstProtectionMetadataHelper>(gstWrapper);
}

GstMeta *GstProtectionMetadataHelper::addProtectionMetadata(GstBuffer *gstBuffer, GstRialtoProtectionData &data)
{
    return m_gstWrapper->gstBufferAddMeta(gstBuffer, GST_RIALTO_PROTECTION_METADATA_INFO, &data);
}

GstRialtoProtectionData *GstProtectionMetadataHelper::getProtectionMetadataData(GstBuffer *gstBuffer)
{
    GstMeta *meta = m_gstWrapper->gstBufferGetMeta(gstBuffer, GST_RIALTO_PROTECTION_METADATA_GET_TYPE);
    if (!meta)
    {
        return nullptr;
    }

    GstRialtoProtectionMetadata *protectionMetadata = reinterpret_cast<GstRialtoProtectionMetadata *>(meta);
    return &protectionMetadata->data;
}

void GstProtectionMetadataHelper::removeProtectionMetadata(GstBuffer *gstBuffer)
{
    GstMeta *meta = m_gstWrapper->gstBufferGetMeta(gstBuffer, GST_RIALTO_PROTECTION_METADATA_GET_TYPE);
    if (meta)
    {
        if (!m_gstWrapper->gstBufferRemoveMeta(gstBuffer, meta))
        {
            RIALTO_SERVER_LOG_ERROR("Failed to remove metadata");
        }
    }
}

} // namespace firebolt::rialto::server
