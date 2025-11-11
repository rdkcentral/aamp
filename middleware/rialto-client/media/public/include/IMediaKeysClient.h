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

#ifndef FIREBOLT_RIALTO_I_MEDIA_KEYS_CLIENT_H_
#define FIREBOLT_RIALTO_I_MEDIA_KEYS_CLIENT_H_

/**
 * @file IMediaKeysClient.h
 *
 * The definition of the IMediaKeysClient interface.
 *
 * This interface defines the public API of Rialto for EME decryption of AV content.
 * This is the Rialto media keys client abstract base class. It should be
 * implemented by any object that wishes to be notified by changes in the state
 * of a key session.
 */

#include "MediaCommon.h"
#include <string>
#include <vector>

namespace firebolt::rialto
{
/**
 * @brief The definition of the IMediaKeysClient interface.
 *
 * This interface defines the public API of Rialto for EME decryption of AV content
 * which should be implemented by both Rialto Client & Rialto Server.
 */
class IMediaKeysClient
{
public:
    /**
     * @brief The virtual destructor.
     */
    virtual ~IMediaKeysClient() {}

    /**
     * @brief Notification that a license is required
     *
     * @param[in] keySessionId  : The idea of the session triggering the callback
     * @param[in] licenseRequestMessage : The license request message blob
     * @param[in] url                   : URL to which the message should be sent or empty string
     */
    virtual void onLicenseRequest(int32_t keySessionId, const std::vector<unsigned char> &licenseRequestMessage,
                                  const std::string &url) = 0;

    /**
     * @brief Notification that a license renewal is required
     *
     * @param[in] keySessionId  : The idea of the session triggering the callback
     * @param[in] licenseRenewalMessage : The license renewal message blob
     */
    virtual void onLicenseRenewal(int32_t keySessionId, const std::vector<unsigned char> &licenseRenewalMessage) = 0;

    /**
     * @brief Notification that the status of one or more keys in the key session has changed
     *
     * @param[in] keySessionId  : The idea of the session triggering the callback
     * @param[in] keyStatuses   : Vector of key ID/key status pairs
     */
    virtual void onKeyStatusesChanged(int32_t keySessionId, const KeyStatusVector &keyStatuses) = 0;
};

}; // namespace firebolt::rialto

#endif // FIREBOLT_RIALTO_I_MEDIA_KEYS_CLIENT_H_
