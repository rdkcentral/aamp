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

#include <algorithm>
#include <exception>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "CdmService.h"
#include "RialtoServerLogging.h"

namespace firebolt::rialto::server::service
{
CdmService::CdmService(std::shared_ptr<IMediaKeysServerInternalFactory> &&mediaKeysFactory,
                       std::shared_ptr<IMediaKeysCapabilitiesFactory> &&mediaKeysCapabilitiesFactory)
    : m_mediaKeysFactory{mediaKeysFactory}, m_mediaKeysCapabilitiesFactory{mediaKeysCapabilitiesFactory},
      m_isActive{false}
{
    RIALTO_SERVER_LOG_DEBUG("CdmService is constructed");
}

CdmService::~CdmService()
{
    RIALTO_SERVER_LOG_DEBUG("CdmService is destructed");
}

bool CdmService::switchToActive()
{
    RIALTO_SERVER_LOG_INFO("Switching SessionServer to Active state.");
    m_isActive = true;
    return true;
}

void CdmService::switchToInactive()
{
    RIALTO_SERVER_LOG_INFO("Switching SessionServer to Inactive state. Cleaning resources...");
    m_isActive = false;

    {
        std::lock_guard<std::mutex> lock{m_mediaKeysMutex};
        m_mediaKeys.clear();
    }
}

bool CdmService::createMediaKeys(int mediaKeysHandle, std::string keySystem)
{
    RIALTO_SERVER_LOG_DEBUG("CdmService requested to create new media keys handle: %d", mediaKeysHandle);
    if (!m_isActive)
    {
        RIALTO_SERVER_LOG_ERROR("Skip to create media keys handle: %d - Session Server in Inactive state",
                                mediaKeysHandle);
        return false;
    }

    {
        std::lock_guard<std::mutex> lock{m_mediaKeysMutex};
        if (m_mediaKeys.find(mediaKeysHandle) != m_mediaKeys.end())
        {
            RIALTO_SERVER_LOG_ERROR("Media keys handle: %d already exists", mediaKeysHandle);
            return false;
        }
        m_mediaKeys.emplace(std::make_pair(mediaKeysHandle, m_mediaKeysFactory->createMediaKeysServerInternal(keySystem)));
        if (!m_mediaKeys.at(mediaKeysHandle))
        {
            RIALTO_SERVER_LOG_ERROR("Could not create MediaKeys for media keys handle: %d", mediaKeysHandle);
            m_mediaKeys.erase(mediaKeysHandle);
            return false;
        }
    }

    RIALTO_SERVER_LOG_INFO("New media keys handle: %d created", mediaKeysHandle);
    return true;
}

bool CdmService::destroyMediaKeys(int mediaKeysHandle)
{
    RIALTO_SERVER_LOG_DEBUG("CdmService requested to destroy media keys handle: %d", mediaKeysHandle);

    {
        std::lock_guard<std::mutex> lock{m_mediaKeysMutex};
        auto mediaKeysIter = m_mediaKeys.find(mediaKeysHandle);
        if (mediaKeysIter == m_mediaKeys.end())
        {
            RIALTO_SERVER_LOG_ERROR("Media keys handle: %d does not exists", mediaKeysHandle);
            return false;
        }
        m_mediaKeys.erase(mediaKeysIter);
    }

    RIALTO_SERVER_LOG_INFO("Media keys handle: %d destroyed", mediaKeysHandle);
    return true;
}

MediaKeyErrorStatus CdmService::createKeySession(int mediaKeysHandle, KeySessionType sessionType,
                                                 const std::shared_ptr<IMediaKeysClient> &client, bool isLDL,
                                                 int32_t &keySessionId)
{
    RIALTO_SERVER_LOG_DEBUG("CdmService requested to create key session: %d", mediaKeysHandle);

    std::lock_guard<std::mutex> lock{m_mediaKeysMutex};
    auto mediaKeysIter = m_mediaKeys.find(mediaKeysHandle);
    if (mediaKeysIter == m_mediaKeys.end())
    {
        RIALTO_SERVER_LOG_ERROR("Media keys handle: %d does not exists", mediaKeysHandle);
        return MediaKeyErrorStatus::FAIL;
    }

    MediaKeyErrorStatus status = mediaKeysIter->second->createKeySession(sessionType, client, isLDL, keySessionId);
    if (MediaKeyErrorStatus::OK == status)
    {
        if (m_mediaKeysClients.find(keySessionId) != m_mediaKeysClients.end())
        {
            RIALTO_SERVER_LOG_ERROR("Media keys client for key session: %d already exists", keySessionId);
            static_cast<void>(removeKeySessionInternal(mediaKeysHandle, keySessionId));
            return MediaKeyErrorStatus::FAIL;
        }
        m_mediaKeysClients.emplace(std::make_pair(keySessionId, client));
    }

    return status;
}

MediaKeyErrorStatus CdmService::generateRequest(int mediaKeysHandle, int32_t keySessionId, InitDataType initDataType,
                                                const std::vector<uint8_t> &initData)
{
    RIALTO_SERVER_LOG_DEBUG("CdmService requested to generate request: %d", mediaKeysHandle);

    std::lock_guard<std::mutex> lock{m_mediaKeysMutex};
    auto mediaKeysIter = m_mediaKeys.find(mediaKeysHandle);
    if (mediaKeysIter == m_mediaKeys.end())
    {
        RIALTO_SERVER_LOG_ERROR("Media keys handle: %d does not exists", mediaKeysHandle);
        return MediaKeyErrorStatus::FAIL;
    }
    return mediaKeysIter->second->generateRequest(keySessionId, initDataType, initData);
}

MediaKeyErrorStatus CdmService::loadSession(int mediaKeysHandle, int32_t keySessionId)
{
    RIALTO_SERVER_LOG_DEBUG("CdmService requested to load session: %d", mediaKeysHandle);

    std::lock_guard<std::mutex> lock{m_mediaKeysMutex};
    auto mediaKeysIter = m_mediaKeys.find(mediaKeysHandle);
    if (mediaKeysIter == m_mediaKeys.end())
    {
        RIALTO_SERVER_LOG_ERROR("Media keys handle: %d does not exists", mediaKeysHandle);
        return MediaKeyErrorStatus::FAIL;
    }
    return mediaKeysIter->second->loadSession(keySessionId);
}

MediaKeyErrorStatus CdmService::updateSession(int mediaKeysHandle, int32_t keySessionId,
                                              const std::vector<uint8_t> &responseData)
{
    RIALTO_SERVER_LOG_DEBUG("CdmService requested to update session: %d", mediaKeysHandle);

    std::lock_guard<std::mutex> lock{m_mediaKeysMutex};
    auto mediaKeysIter = m_mediaKeys.find(mediaKeysHandle);
    if (mediaKeysIter == m_mediaKeys.end())
    {
        RIALTO_SERVER_LOG_ERROR("Media keys handle: %d does not exists", mediaKeysHandle);
        return MediaKeyErrorStatus::FAIL;
    }
    return mediaKeysIter->second->updateSession(keySessionId, responseData);
}

MediaKeyErrorStatus CdmService::closeKeySession(int mediaKeysHandle, int32_t keySessionId)
{
    RIALTO_SERVER_LOG_DEBUG("CdmService requested to close key session: %d", mediaKeysHandle);

    std::lock_guard<std::mutex> lock{m_mediaKeysMutex};
    auto mediaKeysIter = m_mediaKeys.find(mediaKeysHandle);
    if (mediaKeysIter == m_mediaKeys.end())
    {
        RIALTO_SERVER_LOG_ERROR("Media keys handle: %d does not exists", mediaKeysHandle);
        return MediaKeyErrorStatus::FAIL;
    }
    return mediaKeysIter->second->closeKeySession(keySessionId);
}

MediaKeyErrorStatus CdmService::removeKeySession(int mediaKeysHandle, int32_t keySessionId)
{
    RIALTO_SERVER_LOG_DEBUG("CdmService requested to remove key session: %d", mediaKeysHandle);

    std::lock_guard<std::mutex> lock{m_mediaKeysMutex};
    return removeKeySessionInternal(mediaKeysHandle, keySessionId);
}

MediaKeyErrorStatus CdmService::removeKeySessionInternal(int mediaKeysHandle, int32_t keySessionId)
{
    auto mediaKeysIter = m_mediaKeys.find(mediaKeysHandle);
    if (mediaKeysIter == m_mediaKeys.end())
    {
        RIALTO_SERVER_LOG_ERROR("Media keys handle: %d does not exists", mediaKeysHandle);
        return MediaKeyErrorStatus::FAIL;
    }

    MediaKeyErrorStatus status = mediaKeysIter->second->removeKeySession(keySessionId);
    if (MediaKeyErrorStatus::OK == status)
    {
        auto mediaKeysClientsIter = m_mediaKeysClients.find(keySessionId);
        if (mediaKeysClientsIter != m_mediaKeysClients.end())
        {
            m_mediaKeysClients.erase(mediaKeysClientsIter);
        }
    }

    return status;
}

MediaKeyErrorStatus CdmService::getCdmKeySessionId(int mediaKeysHandle, int32_t keySessionId, std::string &cdmKeySessionId)
{
    RIALTO_SERVER_LOG_DEBUG("CdmService requested to get cdm key session id: %d", mediaKeysHandle);

    std::lock_guard<std::mutex> lock{m_mediaKeysMutex};
    auto mediaKeysIter = m_mediaKeys.find(mediaKeysHandle);
    if (mediaKeysIter == m_mediaKeys.end())
    {
        RIALTO_SERVER_LOG_ERROR("Media keys handle: %d does not exists", mediaKeysHandle);
        return MediaKeyErrorStatus::FAIL;
    }

    MediaKeyErrorStatus status = mediaKeysIter->second->getCdmKeySessionId(keySessionId, cdmKeySessionId);
    return status;
}

bool CdmService::containsKey(int mediaKeysHandle, int32_t keySessionId, const std::vector<uint8_t> &keyId)
{
    RIALTO_SERVER_LOG_DEBUG("CdmService requested to check if key is present: %d", mediaKeysHandle);

    std::lock_guard<std::mutex> lock{m_mediaKeysMutex};
    auto mediaKeysIter = m_mediaKeys.find(mediaKeysHandle);
    if (mediaKeysIter == m_mediaKeys.end())
    {
        RIALTO_SERVER_LOG_ERROR("Media keys handle: %d does not exists", mediaKeysHandle);
        return false;
    }

    return mediaKeysIter->second->containsKey(keySessionId, keyId);
}

MediaKeyErrorStatus CdmService::setDrmHeader(int mediaKeysHandle, int32_t keySessionId,
                                             const std::vector<uint8_t> &requestData)
{
    RIALTO_SERVER_LOG_DEBUG("CdmService requested to set drm header: %d", mediaKeysHandle);

    std::lock_guard<std::mutex> lock{m_mediaKeysMutex};
    auto mediaKeysIter = m_mediaKeys.find(mediaKeysHandle);
    if (mediaKeysIter == m_mediaKeys.end())
    {
        RIALTO_SERVER_LOG_ERROR("Media keys handle: %d does not exists", mediaKeysHandle);
        return MediaKeyErrorStatus::FAIL;
    }

    return mediaKeysIter->second->setDrmHeader(keySessionId, requestData);
}

MediaKeyErrorStatus CdmService::deleteDrmStore(int mediaKeysHandle)
{
    RIALTO_SERVER_LOG_DEBUG("CdmService requested to delete drm store: %d", mediaKeysHandle);

    std::lock_guard<std::mutex> lock{m_mediaKeysMutex};
    auto mediaKeysIter = m_mediaKeys.find(mediaKeysHandle);
    if (mediaKeysIter == m_mediaKeys.end())
    {
        RIALTO_SERVER_LOG_ERROR("Media keys handle: %d does not exists", mediaKeysHandle);
        return MediaKeyErrorStatus::FAIL;
    }
    return mediaKeysIter->second->deleteDrmStore();
}

MediaKeyErrorStatus CdmService::deleteKeyStore(int mediaKeysHandle)
{
    RIALTO_SERVER_LOG_DEBUG("CdmService requested to delete key store: %d", mediaKeysHandle);

    std::lock_guard<std::mutex> lock{m_mediaKeysMutex};
    auto mediaKeysIter = m_mediaKeys.find(mediaKeysHandle);
    if (mediaKeysIter == m_mediaKeys.end())
    {
        RIALTO_SERVER_LOG_ERROR("Media keys handle: %d does not exists", mediaKeysHandle);
        return MediaKeyErrorStatus::FAIL;
    }

    return mediaKeysIter->second->deleteKeyStore();
}

MediaKeyErrorStatus CdmService::getDrmStoreHash(int mediaKeysHandle, std::vector<unsigned char> &drmStoreHash)
{
    RIALTO_SERVER_LOG_DEBUG("CdmService requested to get drm store hash: %d", mediaKeysHandle);

    std::lock_guard<std::mutex> lock{m_mediaKeysMutex};
    auto mediaKeysIter = m_mediaKeys.find(mediaKeysHandle);
    if (mediaKeysIter == m_mediaKeys.end())
    {
        RIALTO_SERVER_LOG_ERROR("Media keys handle: %d does not exists", mediaKeysHandle);
        return MediaKeyErrorStatus::FAIL;
    }
    return mediaKeysIter->second->getDrmStoreHash(drmStoreHash);
}

MediaKeyErrorStatus CdmService::getKeyStoreHash(int mediaKeysHandle, std::vector<unsigned char> &keyStoreHash)
{
    RIALTO_SERVER_LOG_DEBUG("CdmService requested to get key store hash: %d", mediaKeysHandle);

    std::lock_guard<std::mutex> lock{m_mediaKeysMutex};
    auto mediaKeysIter = m_mediaKeys.find(mediaKeysHandle);
    if (mediaKeysIter == m_mediaKeys.end())
    {
        RIALTO_SERVER_LOG_ERROR("Media keys handle: %d does not exists", mediaKeysHandle);
        return MediaKeyErrorStatus::FAIL;
    }
    return mediaKeysIter->second->getKeyStoreHash(keyStoreHash);
}

MediaKeyErrorStatus CdmService::getLdlSessionsLimit(int mediaKeysHandle, uint32_t &ldlLimit)
{
    RIALTO_SERVER_LOG_DEBUG("CdmService requested to get ldl sessions limit: %d", mediaKeysHandle);

    std::lock_guard<std::mutex> lock{m_mediaKeysMutex};
    auto mediaKeysIter = m_mediaKeys.find(mediaKeysHandle);
    if (mediaKeysIter == m_mediaKeys.end())
    {
        RIALTO_SERVER_LOG_ERROR("Media keys handle: %d does not exists", mediaKeysHandle);
        return MediaKeyErrorStatus::FAIL;
    }
    return mediaKeysIter->second->getLdlSessionsLimit(ldlLimit);
}

MediaKeyErrorStatus CdmService::getLastDrmError(int mediaKeysHandle, int32_t keySessionId, uint32_t &errorCode)
{
    RIALTO_SERVER_LOG_DEBUG("CdmService requested to get last drm error: %d", mediaKeysHandle);

    std::lock_guard<std::mutex> lock{m_mediaKeysMutex};
    auto mediaKeysIter = m_mediaKeys.find(mediaKeysHandle);
    if (mediaKeysIter == m_mediaKeys.end())
    {
        RIALTO_SERVER_LOG_ERROR("Media keys handle: %d does not exists", mediaKeysHandle);
        return MediaKeyErrorStatus::FAIL;
    }
    return mediaKeysIter->second->getLastDrmError(keySessionId, errorCode);
}

MediaKeyErrorStatus CdmService::getDrmTime(int mediaKeysHandle, uint64_t &drmTime)
{
    RIALTO_SERVER_LOG_DEBUG("CdmService requested to get drm time: %d", mediaKeysHandle);

    std::lock_guard<std::mutex> lock{m_mediaKeysMutex};
    auto mediaKeysIter = m_mediaKeys.find(mediaKeysHandle);
    if (mediaKeysIter == m_mediaKeys.end())
    {
        RIALTO_SERVER_LOG_ERROR("Media keys handle: %d does not exists", mediaKeysHandle);
        return MediaKeyErrorStatus::FAIL;
    }
    return mediaKeysIter->second->getDrmTime(drmTime);
}

MediaKeyErrorStatus CdmService::releaseKeySession(int mediaKeysHandle, int32_t keySessionId)
{
    RIALTO_SERVER_LOG_DEBUG("CdmService requested to release key session: %d", mediaKeysHandle);

    std::lock_guard<std::mutex> lock{m_mediaKeysMutex};
    auto mediaKeysIter = m_mediaKeys.find(mediaKeysHandle);
    if (mediaKeysIter == m_mediaKeys.end())
    {
        RIALTO_SERVER_LOG_ERROR("Media keys handle: %d does not exists", mediaKeysHandle);
        return MediaKeyErrorStatus::FAIL;
    }
    return mediaKeysIter->second->releaseKeySession(keySessionId);
}

std::vector<std::string> CdmService::getSupportedKeySystems()
{
    RIALTO_SERVER_LOG_DEBUG("CdmService requested to getSupportedKeySystems");

    if (!m_isActive)
    {
        RIALTO_SERVER_LOG_ERROR("Skip to get supported key systems: Session Server in Inactive state");
        return {};
    }

    auto mediaKeysCapabilities = m_mediaKeysCapabilitiesFactory->getMediaKeysCapabilities();
    if (!mediaKeysCapabilities)
    {
        RIALTO_SERVER_LOG_ERROR("Failed to create the mediaKeysCapabilities object");
        return {};
    }
    return mediaKeysCapabilities->getSupportedKeySystems();
}

bool CdmService::supportsKeySystem(const std::string &keySystem)
{
    RIALTO_SERVER_LOG_DEBUG("CdmService requested to supportsKeySystem");

    if (!m_isActive)
    {
        RIALTO_SERVER_LOG_ERROR("Skip to get supported key systems: Session Server in Inactive state");
        return false;
    }

    auto mediaKeysCapabilities = m_mediaKeysCapabilitiesFactory->getMediaKeysCapabilities();
    if (!mediaKeysCapabilities)
    {
        RIALTO_SERVER_LOG_ERROR("Failed to create the mediaKeysCapabilities object");
        return false;
    }
    return mediaKeysCapabilities->supportsKeySystem(keySystem);
}

bool CdmService::getSupportedKeySystemVersion(const std::string &keySystem, std::string &version)
{
    RIALTO_SERVER_LOG_DEBUG("CdmService requested to getSupportedKeySystemVersion");

    if (!m_isActive)
    {
        RIALTO_SERVER_LOG_ERROR("Skip to get supported key systems: Session Server in Inactive state");
        return false;
    }

    auto mediaKeysCapabilities = m_mediaKeysCapabilitiesFactory->getMediaKeysCapabilities();
    if (!mediaKeysCapabilities)
    {
        RIALTO_SERVER_LOG_ERROR("Failed to create the mediaKeysCapabilities object");
        return false;
    }
    return mediaKeysCapabilities->getSupportedKeySystemVersion(keySystem, version);
}

bool CdmService::isServerCertificateSupported(const std::string &keySystem)
{
    RIALTO_SERVER_LOG_DEBUG("CdmService requested to isServerCertificateSupported");

    if (!m_isActive)
    {
        RIALTO_SERVER_LOG_ERROR("Skip to check if server cert is supported: Session Server in Inactive state");
        return false;
    }

    auto mediaKeysCapabilities = m_mediaKeysCapabilitiesFactory->getMediaKeysCapabilities();
    if (!mediaKeysCapabilities)
    {
        RIALTO_SERVER_LOG_ERROR("Failed to create the mediaKeysCapabilities object");
        return false;
    }
    return mediaKeysCapabilities->isServerCertificateSupported(keySystem);
}

MediaKeyErrorStatus CdmService::decrypt(int32_t keySessionId, GstBuffer *encrypted, GstCaps *caps)
{
    RIALTO_SERVER_LOG_DEBUG("CdmService requested to decrypt, key session id: %d", keySessionId);

    std::lock_guard<std::mutex> lock{m_mediaKeysMutex};
    auto mediaKeysIter = std::find_if(m_mediaKeys.begin(), m_mediaKeys.end(),
                                      [&](const auto &iter) { return iter.second->hasSession(keySessionId); });
    if (mediaKeysIter == m_mediaKeys.end())
    {
        RIALTO_SERVER_LOG_ERROR("Media keys handle for mksId: %d does not exists", keySessionId);
        return MediaKeyErrorStatus::FAIL;
    }
    return mediaKeysIter->second->decrypt(keySessionId, encrypted, caps);
}

bool CdmService::isPlayreadyKeySystem(int32_t keySessionId)
{
    RIALTO_SERVER_LOG_DEBUG("CdmService requested to check if key system is Playready, key session id: %d", keySessionId);

    std::lock_guard<std::mutex> lock{m_mediaKeysMutex};
    auto mediaKeysIter = std::find_if(m_mediaKeys.begin(), m_mediaKeys.end(),
                                      [&](const auto &iter) { return iter.second->hasSession(keySessionId); });
    if (mediaKeysIter == m_mediaKeys.end())
    {
        RIALTO_SERVER_LOG_ERROR("Media keys handle for mksId: %d does not exists", keySessionId);
        return false;
    }
    return mediaKeysIter->second->isPlayreadyKeySystem(keySessionId);
}

MediaKeyErrorStatus CdmService::selectKeyId(int32_t keySessionId, const std::vector<uint8_t> &keyId)
{
    RIALTO_SERVER_LOG_DEBUG("CdmService requested to select key id, key session id: %d", keySessionId);

    std::lock_guard<std::mutex> lock{m_mediaKeysMutex};
    auto mediaKeysIter = std::find_if(m_mediaKeys.begin(), m_mediaKeys.end(),
                                      [&](const auto &iter) { return iter.second->hasSession(keySessionId); });
    if (mediaKeysIter == m_mediaKeys.end())
    {
        RIALTO_SERVER_LOG_ERROR("Media keys handle for mksId: %d does not exists", keySessionId);
        return MediaKeyErrorStatus::FAIL;
    }
    return mediaKeysIter->second->selectKeyId(keySessionId, keyId);
}

void CdmService::incrementSessionIdUsageCounter(int32_t keySessionId)
{
    std::lock_guard<std::mutex> lock{m_mediaKeysMutex};
    auto mediaKeysIter = std::find_if(m_mediaKeys.begin(), m_mediaKeys.end(),
                                      [&](const auto &iter) { return iter.second->hasSession(keySessionId); });
    if (mediaKeysIter == m_mediaKeys.end())
    {
        RIALTO_SERVER_LOG_ERROR("Media keys handle for mksId: %d does not exists", keySessionId);
        return;
    }

    mediaKeysIter->second->incrementSessionIdUsageCounter(keySessionId);
}

void CdmService::decrementSessionIdUsageCounter(int32_t keySessionId)
{
    std::lock_guard<std::mutex> lock{m_mediaKeysMutex};
    auto mediaKeysIter = std::find_if(m_mediaKeys.begin(), m_mediaKeys.end(),
                                      [&](const auto &iter) { return iter.second->hasSession(keySessionId); });
    if (mediaKeysIter == m_mediaKeys.end())
    {
        RIALTO_SERVER_LOG_ERROR("Media keys handle for mksId: %d does not exists", keySessionId);
        return;
    }

    mediaKeysIter->second->decrementSessionIdUsageCounter(keySessionId);
}

void CdmService::ping(const std::shared_ptr<IHeartbeatProcedure> &heartbeatProcedure)
{
    std::lock_guard<std::mutex> lock{m_mediaKeysMutex};
    for (const auto &mediaKeyPair : m_mediaKeys)
    {
        auto &mediaKeys = mediaKeyPair.second;
        mediaKeys->ping(heartbeatProcedure->createHandler());
    }
}

MediaKeyErrorStatus CdmService::getMetricSystemData(int mediaKeysHandle, std::vector<uint8_t> &buffer)
{
    RIALTO_SERVER_LOG_DEBUG("CdmService requested to get metric system data: %d", mediaKeysHandle);

    std::lock_guard<std::mutex> lock{m_mediaKeysMutex};
    auto mediaKeysIter = m_mediaKeys.find(mediaKeysHandle);
    if (mediaKeysIter == m_mediaKeys.end())
    {
        RIALTO_SERVER_LOG_ERROR("Media keys handle: %d does not exists", mediaKeysHandle);
        return MediaKeyErrorStatus::FAIL;
    }
    return mediaKeysIter->second->getMetricSystemData(buffer);
}
} // namespace firebolt::rialto::server::service
