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

#ifndef FIREBOLT_RIALTO_SERVER_I_GST_DECRYPTOR_ELEMENT_FACTORY_H_
#define FIREBOLT_RIALTO_SERVER_I_GST_DECRYPTOR_ELEMENT_FACTORY_H_

#include "IDecryptionService.h"
#include "IGstWrapper.h"
#include <gst/gst.h>
#include <memory>

namespace firebolt::rialto::server
{
/**
 * @brief IGstDecryptorElement factory class, for the creation of a GstDecryptorElement.
 */
class IGstDecryptorElementFactory
{
public:
    IGstDecryptorElementFactory() = default;
    virtual ~IGstDecryptorElementFactory() = default;

    /**
     * @brief Creates a IGstDecryptorElementFactory instance.
     *
     * @retval the factory instance or null on error.
     */
    static std::shared_ptr<IGstDecryptorElementFactory> createFactory();

    /**
     * @brief Creates a IGstDecryptorElement.
     *
     * @param[in] decryptionService : The service used to decrypt frames.
     * @param[in] gstWrapper        : The gstreamer wrapper.
     *
     * @retval a decryptor element instance or null on error.
     */
    virtual GstElement *
    createDecryptorElement(const gchar *name, firebolt::rialto::server::IDecryptionService *decryptionService,
                           const std::shared_ptr<firebolt::rialto::wrappers::IGstWrapper> &gstWrapper) const = 0;
};

}; // namespace firebolt::rialto::server

#endif // FIREBOLT_RIALTO_SERVER_I_GST_DECRYPTOR_ELEMENT_FACTORY_H_
