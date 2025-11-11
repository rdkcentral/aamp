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

#ifndef FIREBOLT_RIALTO_SERVER_I_MEDIA_KEY_SESSION_H_
#define FIREBOLT_RIALTO_SERVER_I_MEDIA_KEY_SESSION_H_

#include "IMediaKeysClient.h"
#include "IOcdmSystem.h"
#include "MediaCommon.h"
#include <memory>
#include <string>
#include <vector>

namespace firebolt::rialto::server
{
class IMediaKeySession;

/**
 * @brief IMediaKeySession factory class, returns a concrete implementation of IMediaKeySession
 */
class IMediaKeySessionFactory
{
public:
    IMediaKeySessionFactory() = default;
    virtual ~IMediaKeySessionFactory() = default;

    /**
     * @brief Create a IMediaKeySessionFactory instance.
     *
     * @retval the factory instance or null on error.
     */
    static std::shared_ptr<IMediaKeySessionFactory> createFactory();

    /**
     * @brief IMediaKeySession factory method, returns a concrete implementation of IMediaKeySession
     *
     * @param[in]  keySystem    : The key system for this session.
     * @param[in]  keySessionId : The key session id for this session.
     * @param[in]  ocdmSystem   : The ocdm system object to create the session on.
     * @param[in]  sessionType  : The session type.
     * @param[in]  client       : Client object for callbacks.
     * @param[in]  isLDL        : Is this an LDL.
     *
     * @retval the new media keys instance or null on error.
     */
    virtual std::unique_ptr<IMediaKeySession>
    createMediaKeySession(const std::string &keySystem, int32_t keySessionId,
                          const firebolt::rialto::wrappers::IOcdmSystem &ocdmSystem, KeySessionType sessionType,
                          std::weak_ptr<IMediaKeysClient> client, bool isLDL) const = 0;
};

/**
 * @brief The definition of the IMediaKeySession interface.
 */
class IMediaKeySession
{
public:
    IMediaKeySession() = default;
    virtual ~IMediaKeySession() = default;

    IMediaKeySession(const IMediaKeySession &) = delete;
    IMediaKeySession &operator=(const IMediaKeySession &) = delete;
    IMediaKeySession(IMediaKeySession &&) = delete;
    IMediaKeySession &operator=(IMediaKeySession &&) = delete;

    /**
     * @brief Generates a licence request.
     *
     * @param[in]  initDataType : The init data type.
     * @param[in]  initData     : The init data.
     *
     * @retval an error status.
     */
    virtual MediaKeyErrorStatus generateRequest(InitDataType initDataType, const std::vector<uint8_t> &initData) = 0;

    /**
     * @brief Loads the existing key session.
     *
     * @retval an error status.
     */
    virtual MediaKeyErrorStatus loadSession() = 0;

    /**
     * @brief Updates the key session's state.
     *
     * @param[in] responseData : The license response data.
     *
     * @retval an error status.
     */
    virtual MediaKeyErrorStatus updateSession(const std::vector<uint8_t> &responseData) = 0;

    /**
     * @brief Decrypts the buffer.
     *
     * @param[in]  encrypted        : Gstreamer buffer containing encrypted data and related meta data. If applicable,
     *                                decrypted data will be stored here after this call returns.
     * @param[in] caps              : The gst caps of buffer.
     *
     * @retval an error status.
     */
    virtual MediaKeyErrorStatus decrypt(GstBuffer *encrypted, GstCaps *caps) = 0;

    /**
     * @brief Closes the key session.
     *
     * @retval an error status.
     */
    virtual MediaKeyErrorStatus closeKeySession() = 0;

    /**
     * @brief Removes the key session.
     *
     * @retval an error status.
     */
    virtual MediaKeyErrorStatus removeKeySession() = 0;

    /**
     * @brief Get the internal CDM key session ID
     *
     * @param[out]  cdmKeySessionId  : The internal CDM key session ID
     *
     * @retval the return status.
     */
    virtual MediaKeyErrorStatus getCdmKeySessionId(std::string &cdmKeySessionId) = 0;

    /**
     * @brief Returns true if the Key Session object contains the specified key.
     *
     * @param[in] keyId        : The key id.
     *
     * @retval true if it contains the key.
     */
    virtual bool containsKey(const std::vector<uint8_t> &keyId) = 0;

    /**
     * @brief Set DRM Header for a key session
     *
     * This method updates a key session's DRM header. If the session id does
     * not exist an MediaKeyErrorStatus:BAD_SESSION_ID is returned.If the session
     * state is invalid an MediaKeyErrorStatus:INVALID_STATE is returned. Any
     * other errors will result in MediaKeyErrorStatus:FAIL.
     *
     * @param[in] requestData  : The request data.
     *
     * @retval an error status.
     */
    virtual MediaKeyErrorStatus setDrmHeader(const std::vector<uint8_t> &requestData) = 0;

    /**
     * @brief Get the last cdm specific DRM error code
     *
     * @param[out] errorCode : the error code.
     *
     * @retval the return status value.
     */
    virtual MediaKeyErrorStatus getLastDrmError(uint32_t &errorCode) = 0;

    /**
     * @brief Selects the specified keyId for the key session. Playready specific API.
     *
     * @param[in] keyId        : The key id to select.
     *
     * @retval an error status.
     */
    virtual MediaKeyErrorStatus selectKeyId(const std::vector<uint8_t> &keyId) = 0;

    /**
     * @brief Checks, if key system of media key session is  playready.
     *
     * @retval true if key system is  playready
     */
    virtual bool isPlayreadyKeySystem() const = 0;
};
} // namespace firebolt::rialto::server

#endif // FIREBOLT_RIALTO_SERVER_I_MEDIA_KEY_SESSION_H_
