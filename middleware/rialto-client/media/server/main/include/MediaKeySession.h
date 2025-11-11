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

#ifndef FIREBOLT_RIALTO_SERVER_MEDIA_KEY_SESSION_H_
#define FIREBOLT_RIALTO_SERVER_MEDIA_KEY_SESSION_H_

#include "IMainThread.h"
#include "IMediaKeySession.h"
#include "IOcdmSessionClient.h"
#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace firebolt::rialto::server
{
/**
 * @brief IMediaKeySessionFactory factory class definition.
 */
class MediaKeySessionFactory : public IMediaKeySessionFactory
{
public:
    MediaKeySessionFactory() = default;
    ~MediaKeySessionFactory() override = default;

    std::unique_ptr<IMediaKeySession> createMediaKeySession(const std::string &keySystem, int32_t keySessionId,
                                                            const firebolt::rialto::wrappers::IOcdmSystem &ocdmSystem,
                                                            KeySessionType sessionType,
                                                            std::weak_ptr<IMediaKeysClient> client,
                                                            bool isLDL) const override;
};

/**
 * @brief The definition of the MediaKeySession.
 */
class MediaKeySession : public IMediaKeySession, public firebolt::rialto::wrappers::IOcdmSessionClient
{
public:
    /**
     * @brief The constructor.
     *
     * @param[in]  keySystem            : The key system for this session.
     * @param[in]  keySessionId         : The key session id for this session.
     * @param[in]  ocdmSystem           : The ocdm system object to create the session on.
     * @param[in]  sessionType          : The session type.
     * @param[in]  client               : Client object for callbacks.
     * @param[in]  isLDL                : Is this an LDL.
     * @param[in]  mainThreadFactory    : The main thread factory.
     */
    MediaKeySession(const std::string &keySystem, int32_t keySessionId,
                    const firebolt::rialto::wrappers::IOcdmSystem &ocdmSystem, KeySessionType sessionType,
                    std::weak_ptr<IMediaKeysClient> client, bool isLDL,
                    const std::shared_ptr<IMainThreadFactory> &mainThreadFactory);

    /**
     * @brief Virtual destructor.
     */
    virtual ~MediaKeySession();

    MediaKeyErrorStatus generateRequest(InitDataType initDataType, const std::vector<uint8_t> &initData) override;

    MediaKeyErrorStatus loadSession() override;

    MediaKeyErrorStatus updateSession(const std::vector<uint8_t> &responseData) override;

    MediaKeyErrorStatus decrypt(GstBuffer *encrypted, GstCaps *caps) override;

    MediaKeyErrorStatus closeKeySession() override;

    MediaKeyErrorStatus removeKeySession() override;

    MediaKeyErrorStatus getCdmKeySessionId(std::string &cdmKeySessionId) override;

    bool containsKey(const std::vector<uint8_t> &keyId) override;

    MediaKeyErrorStatus setDrmHeader(const std::vector<uint8_t> &requestData) override;

    MediaKeyErrorStatus getLastDrmError(uint32_t &errorCode) override;

    MediaKeyErrorStatus selectKeyId(const std::vector<uint8_t> &keyId) override;

    bool isPlayreadyKeySystem() const override;

    void onProcessChallenge(const char url[], const uint8_t challenge[], const uint16_t challengeLength) override;

    void onKeyUpdated(const uint8_t keyId[], const uint8_t keyIdLength) override;

    void onAllKeysUpdated() override;

    void onError(const char message[]) override;

private:
    /**
     * @brief KeySystem type of the MediaKeys.
     */
    const std::string m_kKeySystem;

    /**
     * @brief The key session id for this session.
     */
    const int32_t m_kKeySessionId;

    /**
     * @brief The key session type of this session.
     */
    const KeySessionType m_kSessionType;

    /**
     * @brief The media keys client object.
     */
    std::weak_ptr<IMediaKeysClient> m_mediaKeysClient;

    /**
     * @brief The IOcdmSession instance.
     */
    std::unique_ptr<firebolt::rialto::wrappers::IOcdmSession> m_ocdmSession;

    /**
     * @brief The mainThread object.
     */
    std::shared_ptr<IMainThread> m_mainThread;

    /**
     * @brief Is the session LDL.
     */
    const bool m_kIsLDL;

    /**
     * @brief Is the ocdm session constructed.
     */
    bool m_isSessionConstructed;

    /**
     * @brief Is the ocdm session closed.
     */
    bool m_isSessionClosed;

    /**
     * @brief Set to true if generateRequest has complete and waiting for license response.
     */
    std::atomic<bool> m_licenseRequested;

    /**
     * @brief Store of the updated key statuses from a onKeyUpdated.
     */
    KeyStatusVector m_updatedKeyStatuses;

    /**
     * @brief This objects id registered on the main thread
     */
    uint32_t m_mainThreadClientId;

    /**
     * @brief Currently selected key id (PR specific)
     */
    std::vector<uint8_t> m_selectedKeyId;

    /**
     * @brief Whether a Ocdm call is currently ongoing.
     */
    bool m_ongoingOcdmOperation;

    /**
     * @brief Whether Ocdm has returned an error via the onError callback.
     */
    bool m_ocdmError;

    /**
     * @brief Mutex protecting the ocdm error checking.
     */
    std::mutex m_ocdmErrorMutex;

    /**
     * @brief Drm header to be set once the session is constructed
     */
    std::vector<uint8_t> m_queuedDrmHeader;

    /**
     * @brief Posts a getChallenge task onto the main thread.
     *
     * The challenge data is retrieved from ocdm and notified on a onLicenseRequest.
     */
    void getChallenge();

    /**
     * @brief Initalises the ocdm error data which checks for onError callbacks.
     */
    void initOcdmErrorChecking();

    /**
     * @brief Checks if onError has been received.
     *
     * @param[in] operationStr : Operation name for logging purposes.
     *
     * @retval True if an error was received.
     */
    bool checkForOcdmErrors(const char *operationStr);
};
} // namespace firebolt::rialto::server

#endif // FIREBOLT_RIALTO_SERVER_MEDIA_KEY_SESSION_H_
