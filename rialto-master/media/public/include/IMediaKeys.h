/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
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

#ifndef FIREBOLT_RIALTO_I_MEDIA_KEYS_H_
#define FIREBOLT_RIALTO_I_MEDIA_KEYS_H_

/**
 * @file IMediaKeys.h
 *
 * The definition of the IMediaKeys interface.
 *
 * This interface defines the public API of Rialto for EME decryption of AV content.
 */

#include <memory>
#include <string>
#include <vector>

#include "IMediaKeysClient.h"
#include "MediaCommon.h"

namespace firebolt::rialto
{
class IMediaKeys;

/**
 * @brief IMediaKeys factory class, returns a concrete implementation of IMediaKeys
 */
class IMediaKeysFactory
{
public:
    IMediaKeysFactory() = default;
    virtual ~IMediaKeysFactory() = default;

    /**
     * @brief Create a IMediaKeysFactory instance.
     *
     * @retval the factory instance or null on error.
     */
    static std::shared_ptr<IMediaKeysFactory> createFactory();

    /**
     * @brief IMediaKeys factory method, returns a concrete implementation of IMediaKeys
     *
     * @param[in] keySystem           : The key system for which to create a Media Keys instance.
     *
     * @retval the new media keys instance or null on error.
     */
    virtual std::unique_ptr<IMediaKeys> createMediaKeys(const std::string &keySystem) const = 0;
};

/**
 * @brief The definition of the IMediaKeys interface.
 *
 * This interface defines the public API of Rialto for EME decryption of AV content
 * which should be implemented by both Rialto Client & Rialto Server.
 */
class IMediaKeys
{
public:
    IMediaKeys() = default;
    virtual ~IMediaKeys() = default;

    IMediaKeys(const IMediaKeys &) = delete;
    IMediaKeys &operator=(const IMediaKeys &) = delete;
    IMediaKeys(IMediaKeys &&) = delete;
    IMediaKeys &operator=(IMediaKeys &&) = delete;

    /**
     * @brief Selects the specified keyId for the key session. Netflix specific API.
     *
     * @param[in] keySessionId : The key session id for the session.
     * @param[in] keyId        : The key id to select.
     *
     * @retval an error status.
     */
    virtual MediaKeyErrorStatus selectKeyId(int32_t keySessionId, const std::vector<uint8_t> &keyId) = 0;

    /**
     * @brief Returns true if the Key Session object contains the specified key.
     *
     * @param[in] keySessionId : The key session id for the session.
     * @param[in] keyId        : The key id.
     *
     * @retval true if it contains the key.
     */
    virtual bool containsKey(int32_t keySessionId, const std::vector<uint8_t> &keyId) = 0;

    /**
     * @brief Creates a session and returns the session id.
     *
     * This method creates a new session and returns the session id in
     * the specified string. Any other errors will result in MediaKeyErrorStatus:FAIL.
     *
     * @param[in]  sessionType : The session type.
     * @param[in]  client      : Client object for callbacks
     * @param[in]  isLDL       : Is this an LDL
     * @param[out] keySessionId: The key session id
     *
     * @retval an error status.
     */
    virtual MediaKeyErrorStatus createKeySession(KeySessionType sessionType, std::weak_ptr<IMediaKeysClient> client,
                                                 bool isLDL, int32_t &keySessionId) = 0;

    /**
     * @brief Generates a licence request.
     *
     * This method triggers generation of a licence request. If the session
     * id does not exist an MediaKeyErrorStatus:BAD_SESSION_ID is returned. If the
     * session type or init data type is not supported a
     * MediaKeyErrorStatus:NOT_SUPPORTED value is be returned. Any other
     * errors will result in MediaKeyErrorStatus:FAIL.
     *
     * @param[in]  keySessionId : The key session id for the session.
     * @param[in]  initDataType : The init data type.
     * @param[in]  initData     : The init data.
     *
     * @retval an error status.
     */
    virtual MediaKeyErrorStatus generateRequest(int32_t keySessionId, InitDataType initDataType,
                                                const std::vector<uint8_t> &initData) = 0;

    /**
     * @brief Loads an existing key session
     *
     * This method loads an existing key session. If the session id does
     * not exist an MediaKeyErrorStatus:BAD_SESSION_ID is returned. If the
     * session type or init data type is not supported a
     * MediaKeyErrorStatus:NOT_SUPPORTED value must be returned. If the session
     * state is invalid an MediaKeyErrorStatus:INVALID_STATE is returned. Any
     * other errors will result in MediaKeyErrorStatus:FAIL.
     *
     * @param[in] keySessionId : The key session id for the session.
     *
     * @retval an error status.
     */
    virtual MediaKeyErrorStatus loadSession(int32_t keySessionId) = 0;

    /**
     * @brief Updates a key session's state.
     *
     * This method updates a session's state. If the session id does
     * not exist an MediaKeyErrorStatus:BAD_SESSION_ID is returned. If the session
     * state is invalid an MediaKeyErrorStatus:INVALID_STATE is returned. Any
     * other errors will result in MediaKeyErrorStatus:FAIL.
     *
     * @param[in] keySessionId : The key session id for the session.
     * @param[in] responseData : The license response data.
     *
     * @retval an error status.
     */
    virtual MediaKeyErrorStatus updateSession(int32_t keySessionId, const std::vector<uint8_t> &responseData) = 0;

    /**
     * @brief Set DRM Header for a key session
     *
     * This method updates a key session's DRM header. If the session id does
     * not exist an MediaKeyErrorStatus:BAD_SESSION_ID is returned.If the session
     * state is invalid an MediaKeyErrorStatus:INVALID_STATE is returned. Any
     * other errors will result in MediaKeyErrorStatus:FAIL.
     *
     * @param[in] keySessionId : The session id for the session.
     * @param[in] requestData  : The request data.
     *
     * @retval an error status.
     */
    virtual MediaKeyErrorStatus setDrmHeader(int32_t keySessionId, const std::vector<uint8_t> &requestData) = 0;

    /**
     * @brief Closes a key session
     *
     * This method closes an open session. If the session id does
     * not exist an MediaKeyErrorStatus:BAD_SESSION_ID is returned. Any other
     * errors will result in MediaKeyErrorStatus:FAIL.
     *
     * @param[in] keySessionId : The key session id.
     *
     * @retval an error status.
     */
    virtual MediaKeyErrorStatus closeKeySession(int32_t keySessionId) = 0;

    /**
     * @brief Removes a key session
     *
     * This method removes an open session. If the session id does
     * not exist an MediaKeyErrorStatus:BAD_SESSION_ID is returned. Any other
     * errors will result in MediaKeyErrorStatus:FAIL.
     *
     * @param[in] keySessionId : The key session id.
     *
     * @retval an error status.
     */
    virtual MediaKeyErrorStatus removeKeySession(int32_t keySessionId) = 0;

    /**
     * @brief Delete the DRM store for the object's key system
     *
     * @retval the return status value.
     */
    virtual MediaKeyErrorStatus deleteDrmStore() = 0;

    /**
     * @brief Delete the key store for the object's key system
     *
     * @retval the return status value.
     */
    virtual MediaKeyErrorStatus deleteKeyStore() = 0;

    /**
     * @brief Gets a hash of the DRM store for the object's key system
     *
     * @param[out] drmStoreHash : the hash value
     *
     * @retval the return status value.
     */
    virtual MediaKeyErrorStatus getDrmStoreHash(std::vector<unsigned char> &drmStoreHash) = 0;

    /**
     * @brief Gets a hash of the Key store for the object's key system
     *
     * @param[out] keyStoreHash : the hash value
     *
     * @retval the return status value.
     */
    virtual MediaKeyErrorStatus getKeyStoreHash(std::vector<unsigned char> &keyStoreHash) = 0;

    /**
     * @brief Get the limit on the number of ldl key sessions for the object's key system
     *
     * @param[out] ldlLimit : the limit on the number of ldl key sessions.
     *
     * @retval the return status value.
     */
    virtual MediaKeyErrorStatus getLdlSessionsLimit(uint32_t &ldlLimit) = 0;

    /**
     * @brief Get the last cdm specific DRM error code
     *
     * @param[in] keySessionId : The key session id.
     * @param[out] errorCode : the error code.
     *
     * @retval the return status value.
     */
    virtual MediaKeyErrorStatus getLastDrmError(int32_t keySessionId, uint32_t &errorCode) = 0;

    /**
     * @brief Get the DRM system time for the object's key system
     *
     * @param[out]  drmTime : the DRM system time
     *
     * @retval the return status value.
     */
    virtual MediaKeyErrorStatus getDrmTime(uint64_t &drmTime) = 0;

    /**
     * @brief Get the internal CDM key session ID
     *
     * @param[in]   keySessionId    : The key session id for the session.
     * @param[out]  cdmKeySessionId : The internal CDM key session ID
     *
     * @retval the return status value.
     */
    virtual MediaKeyErrorStatus getCdmKeySessionId(int32_t keySessionId, std::string &cdmKeySessionId) = 0;

    /**
     * @brief Releases a key session
     *
     * This method releases an open session. If the session id does
     * not exist an MediaKeyErrorStatus:BAD_SESSION_ID is returned. Any other
     * errors will result in MediaKeyErrorStatus:FAIL.
     *
     * @param[in] keySessionId : The key session id.
     *
     * @retval an error status.
     */
    virtual MediaKeyErrorStatus releaseKeySession(int32_t keySessionId) = 0;

    /**
     * @brief Get metrics for a DRM system
     *
     * @param[out]  buffer : Buffer that can hold the metric data
     *
     * @retval an error status.
     */
    virtual MediaKeyErrorStatus getMetricSystemData(std::vector<uint8_t> &buffer) = 0;
};

}; // namespace firebolt::rialto

#endif // FIREBOLT_RIALTO_I_MEDIA_KEYS_H_
