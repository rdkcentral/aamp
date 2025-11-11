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

#include <stdexcept>

#include "MediaKeySession.h"
#include "MediaKeysCommon.h"
#include "RialtoServerLogging.h"

namespace firebolt::rialto::server
{
std::shared_ptr<IMediaKeySessionFactory> IMediaKeySessionFactory::createFactory()
{
    std::shared_ptr<IMediaKeySessionFactory> factory;

    try
    {
        factory = std::make_shared<MediaKeySessionFactory>();
    }
    catch (const std::exception &e)
    {
        RIALTO_SERVER_LOG_ERROR("Failed to create the media key session factory, reason: %s", e.what());
    }

    return factory;
}

std::unique_ptr<IMediaKeySession> MediaKeySessionFactory::createMediaKeySession(
    const std::string &keySystem, int32_t keySessionId, const firebolt::rialto::wrappers::IOcdmSystem &ocdmSystem,
    KeySessionType sessionType, std::weak_ptr<IMediaKeysClient> client, bool isLDL) const
{
    std::unique_ptr<IMediaKeySession> mediaKeys;
    try
    {
        mediaKeys = std::make_unique<server::MediaKeySession>(keySystem, keySessionId, ocdmSystem, sessionType, client,
                                                              isLDL, server::IMainThreadFactory::createFactory());
    }
    catch (const std::exception &e)
    {
        RIALTO_SERVER_LOG_ERROR("Failed to create the media key session, reason: %s", e.what());
    }

    return mediaKeys;
}

MediaKeySession::MediaKeySession(const std::string &keySystem, int32_t keySessionId,
                                 const firebolt::rialto::wrappers::IOcdmSystem &ocdmSystem, KeySessionType sessionType,
                                 std::weak_ptr<IMediaKeysClient> client, bool isLDL,
                                 const std::shared_ptr<IMainThreadFactory> &mainThreadFactory)
    : m_kKeySystem(keySystem), m_kKeySessionId(keySessionId), m_kSessionType(sessionType), m_mediaKeysClient(client),
      m_kIsLDL(isLDL), m_isSessionConstructed(false), m_isSessionClosed(false), m_licenseRequested(false),
      m_ongoingOcdmOperation(false), m_ocdmError(false)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");

    m_mainThread = mainThreadFactory->getMainThread();
    if (!m_mainThread)
    {
        throw std::runtime_error("Failed to get the main thread");
    }
    m_mainThreadClientId = m_mainThread->registerClient();

    m_ocdmSession = ocdmSystem.createSession(this);
    if (!m_ocdmSession)
    {
        throw std::runtime_error("Ocdm session could not be created");
    }
    RIALTO_SERVER_LOG_MIL("New OCDM session created");
}

MediaKeySession::~MediaKeySession()
{
    RIALTO_SERVER_LOG_DEBUG("entry:");

    if (m_isSessionConstructed)
    {
        if (!m_isSessionClosed)
        {
            if (MediaKeyErrorStatus::OK != closeKeySession())
            {
                RIALTO_SERVER_LOG_ERROR("Failed to close the key session");
            }
        }
        if (MediaKeyErrorStatus::OK != m_ocdmSession->destructSession())
        {
            RIALTO_SERVER_LOG_ERROR("Failed to destruct the key session");
        }
    }

    m_mainThread->unregisterClient(m_mainThreadClientId);
}

MediaKeyErrorStatus MediaKeySession::generateRequest(InitDataType initDataType, const std::vector<uint8_t> &initData)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");
    // Set the request flag for the onLicenseRequest callback
    m_licenseRequested = true;

    // Only construct session if it hasnt previously been constructed
    if (!m_isSessionConstructed)
    {
        initOcdmErrorChecking();

        MediaKeyErrorStatus status =
            m_ocdmSession->constructSession(m_kSessionType, initDataType, &initData[0], initData.size());
        if (MediaKeyErrorStatus::OK != status)
        {
            RIALTO_SERVER_LOG_ERROR("Failed to construct the key session");
            m_licenseRequested = false;
        }
        else
        {
            m_isSessionConstructed = true;

            if (!m_queuedDrmHeader.empty())
            {
                RIALTO_SERVER_LOG_DEBUG("Setting queued drm header after session construction");
                setDrmHeader(m_queuedDrmHeader);
                m_queuedDrmHeader.clear();
            }
            if (isPlayreadyKeySystem())
            {
                // Ocdm-playready does not notify onProcessChallenge when complete.
                // Fetch the challenge manually.
                getChallenge();
            }
        }

        if ((checkForOcdmErrors("generateRequest")) && (MediaKeyErrorStatus::OK == status))
        {
            status = MediaKeyErrorStatus::FAIL;
        }

        return status;
    }

    return MediaKeyErrorStatus::OK;
}

void MediaKeySession::getChallenge()
{
    RIALTO_SERVER_LOG_DEBUG("entry:");
    auto task = [&]()
    {
        uint32_t challengeSize = 0;
        MediaKeyErrorStatus status = m_ocdmSession->getChallengeData(m_kIsLDL, nullptr, &challengeSize);
        if (challengeSize == 0)
        {
            RIALTO_SERVER_LOG_ERROR("Failed to get the challenge data size, no onLicenseRequest will be generated");
            return;
        }
        std::vector<uint8_t> challenge(challengeSize, 0x00);
        status = m_ocdmSession->getChallengeData(m_kIsLDL, &challenge[0], &challengeSize);
        if (MediaKeyErrorStatus::OK != status)
        {
            RIALTO_SERVER_LOG_ERROR("Failed to get the challenge data, no onLicenseRequest will be generated");
            return;
        }

        std::string url;
        onProcessChallenge(url.c_str(), &challenge[0], challengeSize);
    };
    m_mainThread->enqueueTask(m_mainThreadClientId, task);
}

MediaKeyErrorStatus MediaKeySession::loadSession()
{
    initOcdmErrorChecking();

    MediaKeyErrorStatus status = m_ocdmSession->load();
    if (MediaKeyErrorStatus::OK != status)
    {
        RIALTO_SERVER_LOG_ERROR("Failed to load the key session");
    }

    if ((checkForOcdmErrors("loadSession")) && (MediaKeyErrorStatus::OK == status))
    {
        status = MediaKeyErrorStatus::FAIL;
    }

    return status;
}

MediaKeyErrorStatus MediaKeySession::updateSession(const std::vector<uint8_t> &responseData)
{
    initOcdmErrorChecking();

    MediaKeyErrorStatus status;
    if (isPlayreadyKeySystem())
    {
        status = m_ocdmSession->storeLicenseData(&responseData[0], responseData.size());
        if (MediaKeyErrorStatus::OK != status)
        {
            RIALTO_SERVER_LOG_ERROR("Failed to store the license data for the key session");
        }
    }
    else
    {
        status = m_ocdmSession->update(&responseData[0], responseData.size());
        if (MediaKeyErrorStatus::OK != status)
        {
            RIALTO_SERVER_LOG_ERROR("Failed to update the key session");
        }
    }

    if ((checkForOcdmErrors("updateSession")) && (MediaKeyErrorStatus::OK == status))
    {
        status = MediaKeyErrorStatus::FAIL;
    }

    return status;
}

MediaKeyErrorStatus MediaKeySession::decrypt(GstBuffer *encrypted, GstCaps *caps)
{
    initOcdmErrorChecking();

    MediaKeyErrorStatus status = m_ocdmSession->decryptBuffer(encrypted, caps);
    if (MediaKeyErrorStatus::OK != status)
    {
        RIALTO_SERVER_LOG_ERROR("Failed to decrypt buffer");
    }

    if ((checkForOcdmErrors("decrypt")) && (MediaKeyErrorStatus::OK == status))
    {
        status = MediaKeyErrorStatus::FAIL;
    }

    return status;
}

MediaKeyErrorStatus MediaKeySession::closeKeySession()
{
    initOcdmErrorChecking();

    MediaKeyErrorStatus status;
    if (isPlayreadyKeySystem())
    {
        if (MediaKeyErrorStatus::OK != m_ocdmSession->cancelChallengeData())
        {
            RIALTO_SERVER_LOG_WARN("Failed to cancel the challenge data for the key session");
        }

        if (MediaKeyErrorStatus::OK != m_ocdmSession->cleanDecryptContext())
        {
            RIALTO_SERVER_LOG_WARN("Failed to clean the decrypt context for the key session");
        }
        status = MediaKeyErrorStatus::OK;
        RIALTO_SERVER_LOG_MIL("OCDM session closed");
    }
    else
    {
        status = m_ocdmSession->close();
        if (MediaKeyErrorStatus::OK != status)
        {
            RIALTO_SERVER_LOG_ERROR("Failed to Close the key session");
        }
    }
    m_isSessionClosed = (MediaKeyErrorStatus::OK == status);

    if ((checkForOcdmErrors("closeKeySession")) && (MediaKeyErrorStatus::OK == status))
    {
        status = MediaKeyErrorStatus::FAIL;
    }

    return status;
}

MediaKeyErrorStatus MediaKeySession::removeKeySession()
{
    initOcdmErrorChecking();

    MediaKeyErrorStatus status = m_ocdmSession->remove();
    if (MediaKeyErrorStatus::OK != status)
    {
        RIALTO_SERVER_LOG_ERROR("Failed to remove the key session");
    }

    if ((checkForOcdmErrors("removeKeySession")) && (MediaKeyErrorStatus::OK == status))
    {
        status = MediaKeyErrorStatus::FAIL;
    }

    return status;
}

MediaKeyErrorStatus MediaKeySession::getCdmKeySessionId(std::string &cdmKeySessionId)
{
    initOcdmErrorChecking();

    MediaKeyErrorStatus status = m_ocdmSession->getCdmKeySessionId(cdmKeySessionId);
    if (MediaKeyErrorStatus::OK != status)
    {
        RIALTO_SERVER_LOG_ERROR("Failed to get cdm key session id");
    }

    if ((checkForOcdmErrors("getCdmKeySessionId")) && (MediaKeyErrorStatus::OK == status))
    {
        status = MediaKeyErrorStatus::FAIL;
    }

    return status;
}

bool MediaKeySession::containsKey(const std::vector<uint8_t> &keyId)
{
    uint32_t result = m_ocdmSession->hasKeyId(keyId.data(), keyId.size());

    return static_cast<bool>(result);
}

MediaKeyErrorStatus MediaKeySession::setDrmHeader(const std::vector<uint8_t> &requestData)
{
    initOcdmErrorChecking();

    if (!m_isSessionConstructed)
    {
        RIALTO_SERVER_LOG_INFO("Session not yet constructed, queueing drm header to be set after construction");
        m_queuedDrmHeader = requestData;
        return MediaKeyErrorStatus::OK;
    }

    MediaKeyErrorStatus status = m_ocdmSession->setDrmHeader(requestData.data(), requestData.size());
    if (MediaKeyErrorStatus::OK != status)
    {
        RIALTO_SERVER_LOG_ERROR("Failed to set drm header");
    }

    if ((checkForOcdmErrors("setDrmHeader")) && (MediaKeyErrorStatus::OK == status))
    {
        status = MediaKeyErrorStatus::FAIL;
    }

    return status;
}

MediaKeyErrorStatus MediaKeySession::getLastDrmError(uint32_t &errorCode)
{
    initOcdmErrorChecking();

    MediaKeyErrorStatus status = m_ocdmSession->getLastDrmError(errorCode);
    if (MediaKeyErrorStatus::OK != status)
    {
        RIALTO_SERVER_LOG_ERROR("Failed to get last drm error");
    }

    if ((checkForOcdmErrors("getLastDrmError")) && (MediaKeyErrorStatus::OK == status))
    {
        status = MediaKeyErrorStatus::FAIL;
    }

    return status;
}

MediaKeyErrorStatus MediaKeySession::selectKeyId(const std::vector<uint8_t> &keyId)
{
    if (m_selectedKeyId == keyId)
    {
        return MediaKeyErrorStatus::OK;
    }

    initOcdmErrorChecking();

    MediaKeyErrorStatus status = m_ocdmSession->selectKeyId(keyId.size(), keyId.data());
    if (MediaKeyErrorStatus::OK == status)
    {
        RIALTO_SERVER_LOG_INFO("New keyId selected successfully");
        m_selectedKeyId = keyId;
    }

    if ((checkForOcdmErrors("selectKeyId")) && (MediaKeyErrorStatus::OK == status))
    {
        status = MediaKeyErrorStatus::FAIL;
    }

    return status;
}

bool MediaKeySession::isPlayreadyKeySystem() const
{
    return m_kKeySystem.find("playready") != std::string::npos;
}

void MediaKeySession::onProcessChallenge(const char url[], const uint8_t challenge[], const uint16_t challengeLength)
{
    std::string urlStr = url;
    std::vector<unsigned char> challengeVec = std::vector<unsigned char>{challenge, challenge + challengeLength};
    auto task = [&, urlStr, challengeVec]()
    {
        std::shared_ptr<IMediaKeysClient> client = m_mediaKeysClient.lock();
        if (client)
        {
            if (m_licenseRequested)
            {
                client->onLicenseRequest(m_kKeySessionId, challengeVec, urlStr);
                m_licenseRequested = false;
            }
            else
            {
                client->onLicenseRenewal(m_kKeySessionId, challengeVec);
            }
        }
    };
    m_mainThread->enqueueTask(m_mainThreadClientId, task);
}

void MediaKeySession::onKeyUpdated(const uint8_t keyId[], const uint8_t keyIdLength)
{
    std::vector<unsigned char> keyIdVec = std::vector<unsigned char>{keyId, keyId + keyIdLength};
    auto task = [&, keyIdVec]()
    {
        std::shared_ptr<IMediaKeysClient> client = m_mediaKeysClient.lock();
        if (client)
        {
            KeyStatus status = m_ocdmSession->getStatus(&keyIdVec[0], keyIdVec.size());
            m_updatedKeyStatuses.push_back(std::make_pair(keyIdVec, status));
        }
    };
    m_mainThread->enqueueTask(m_mainThreadClientId, task);
}

void MediaKeySession::onAllKeysUpdated()
{
    auto task = [&]()
    {
        std::shared_ptr<IMediaKeysClient> client = m_mediaKeysClient.lock();
        if (client)
        {
            client->onKeyStatusesChanged(m_kKeySessionId, m_updatedKeyStatuses);
            m_updatedKeyStatuses.clear();
        }
    };
    m_mainThread->enqueueTask(m_mainThreadClientId, task);
}

void MediaKeySession::onError(const char message[])
{
    RIALTO_SERVER_LOG_ERROR("Ocdm returned error: %s", message);

    std::lock_guard<std::mutex> lock(m_ocdmErrorMutex);
    if (!m_ongoingOcdmOperation)
    {
        RIALTO_SERVER_LOG_WARN("Received an asycronous OCDM error, ignoring");
    }
    else
    {
        m_ocdmError = true;
    }
}

void MediaKeySession::initOcdmErrorChecking()
{
    std::lock_guard<std::mutex> lock(m_ocdmErrorMutex);
    m_ongoingOcdmOperation = true;
    m_ocdmError = false;
}

bool MediaKeySession::checkForOcdmErrors(const char *operationStr)
{
    bool error = false;

    std::lock_guard<std::mutex> lock(m_ocdmErrorMutex);
    if (m_ocdmError)
    {
        RIALTO_SERVER_LOG_ERROR("MediaKeySession received an onError callback, operation '%s' failed", operationStr);
        error = true;
    }
    m_ongoingOcdmOperation = false;

    return error;
}

}; // namespace firebolt::rialto::server
