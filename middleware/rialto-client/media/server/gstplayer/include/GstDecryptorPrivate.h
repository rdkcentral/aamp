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

#ifndef FIREBOLT_RIALTO_SERVER_GST_DECRYPTOR_PRIVATE_H_
#define FIREBOLT_RIALTO_SERVER_GST_DECRYPTOR_PRIVATE_H_

#include "IDecryptionService.h"
#include "IGlibWrapper.h"
#include "IGstProtectionMetadataHelper.h"
#include "IGstWrapper.h"
#include <gst/gst.h>
#include <memory>

namespace firebolt::rialto::server
{
class GstRialtoDecryptorPrivate
{
public:
    /**
     * @brief The constructor.

     * @param[in] parentElement         : The parent decryptor element.
     * @param[in] gstWrapperFactory     : The gstreamer wrapper factory.
     * @param[in] glibWrapperFactory    : The glib wrapper factory.
     */
    GstRialtoDecryptorPrivate(GstBaseTransform *parentElement,
                              const std::shared_ptr<firebolt::rialto::wrappers::IGstWrapperFactory> &gstWrapperFactory,
                              const std::shared_ptr<firebolt::rialto::wrappers::IGlibWrapperFactory> &glibWrapperFactory);

    /**
     * @brief Decrypts the gst buffer.
     *
     * @param[in] buffer   : The gst buffer to decrypt.
     * @param[in] caps     : The gst caps of buffer.
     *
     * @retval the gst flow return status.
     */
    GstFlowReturn decrypt(GstBuffer *buffer, GstCaps *caps);

    /**
     * @brief Set the decryption service object.
     *
     * @param[in] decryptionService     : The service that performs decryption.
     */
    void setDecryptionService(IDecryptionService *decryptionService);

    /**
     * @brief Set the protection metadata object
     *
     * @param[in] metadataWrapper     : The protection metadata object
     */
    void setProtectionMetadataWrapper(std::unique_ptr<IGstProtectionMetadataHelper> &&metadataWrapper);

private:
    /**
     * @brief The gstreamer wrapper object.
     */
    std::shared_ptr<firebolt::rialto::wrappers::IGstWrapper> m_gstWrapper;

    /**
     * @brief The glib wrapper object.
     */
    std::shared_ptr<firebolt::rialto::wrappers::IGlibWrapper> m_glibWrapper;

    /**
     * @brief The parent decryptor element.
     */
    GstBaseTransform *m_decryptorElement;

    /**
     * @brief The service to perform decryption.
     */
    IDecryptionService *m_decryptionService;

    /**
     * @brief The protection metadata object
     */
    std::unique_ptr<IGstProtectionMetadataHelper> m_metadataWrapper;

    /**
     * @brief Creates the protection meta structure.
     *
     * @param[in]  protectionData   : The rialto protection data.
     *
     * @retval the gst structure of the protection meta.
     */
    GstStructure *createProtectionMetaInfo(GstRialtoProtectionData *protectionData);
};
}; // namespace firebolt::rialto::server

#endif // FIREBOLT_RIALTO_SERVER_GST_DECRYPTOR_PRIVATE_H_
