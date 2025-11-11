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

#ifndef FIREBOLT_RIALTO_SERVER_GST_PROTECTION_METADATA_HELPER_H_
#define FIREBOLT_RIALTO_SERVER_GST_PROTECTION_METADATA_HELPER_H_

#include "IGstProtectionMetadataHelper.h"
#include "IGstWrapper.h"
#include <memory>

namespace firebolt::rialto::server
{
class GstProtectionMetadataHelper : public IGstProtectionMetadataHelper
{
public:
    explicit GstProtectionMetadataHelper(const std::shared_ptr<firebolt::rialto::wrappers::IGstWrapper> &gstWrapper)
        : m_gstWrapper(gstWrapper)
    {
    }
    ~GstProtectionMetadataHelper() override = default;
    GstMeta *addProtectionMetadata(GstBuffer *gstBuffer, GstRialtoProtectionData &data) override;
    GstRialtoProtectionData *getProtectionMetadataData(GstBuffer *gstBuffer) override;
    void removeProtectionMetadata(GstBuffer *gstBuffer) override;

private:
    /**
     * @brief The gstreamer wrapper object.
     */
    std::shared_ptr<firebolt::rialto::wrappers::IGstWrapper> m_gstWrapper;
};
}; // namespace firebolt::rialto::server

#endif // FIREBOLT_RIALTO_SERVER_GST_PROTECTION_METADATA_HELPER_H_
