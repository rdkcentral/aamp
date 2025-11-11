/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2023 Sky UK
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

#include "CdmBackend.h"

CdmBackend::CdmBackend(const std::string &keySystem,
                       const std::shared_ptr<firebolt::rialto::IMediaKeysClient> &mediaKeysClient,
                       const std::shared_ptr<firebolt::rialto::IMediaKeysFactory> &mediaKeysFactory)
    : m_log{"CdmBackend"}, m_appState{firebolt::rialto::ApplicationState::UNKNOWN}, m_keySystem{keySystem},
      m_mediaKeysClient{mediaKeysClient}, m_mediaKeysFactory{mediaKeysFactory}
{
}

void CdmBackend::notifyApplicationState(firebolt::rialto::ApplicationState state)
{
    std::unique_lock<std::mutex> lock{m_mutex};
    if (state == m_appState)
    {
        return;
    }
    if (firebolt::rialto::ApplicationState::RUNNING == state)
    {
        m_log << info << "Rialto state changed to: RUNNING";
        if (createMediaKeys())
        {
            m_appState = state;
            m_cv.notify_one();
        }
    }
    else
    {
        m_log << info << "Rialto state changed to: INACTIVE";
        m_mediaKeys.reset();
        m_appState = state;
    }
}

bool CdmBackend::initialize(const firebolt::rialto::ApplicationState &initialState)
{
    std::unique_lock<std::mutex> lock{m_mutex};
    if (firebolt::rialto::ApplicationState::UNKNOWN != m_appState)
    {
        // CdmBackend initialized by Rialto Client thread in notifyApplicationState()
        return true;
    }
    if (firebolt::rialto::ApplicationState::RUNNING == initialState)
    {
        if (!createMediaKeys())
        {
            return false;
        }
    }
    m_log << info << "CdmBackend initialized in "
          << (firebolt::rialto::ApplicationState::RUNNING == initialState ? "RUNNING" : "INACTIVE") << " state";
    m_appState = initialState;
    return true;
}

bool CdmBackend::selectKeyId(int32_t keySessionId, const std::vector<uint8_t> &keyId)
{
    std::unique_lock<std::mutex> lock{m_mutex};
    if (!m_mediaKeys)
    {
        return false;
    }
    return firebolt::rialto::MediaKeyErrorStatus::OK == m_mediaKeys->selectKeyId(keySessionId, keyId);
}

bool CdmBackend::containsKey(int32_t keySessionId, const std::vector<uint8_t> &keyId)
{
    std::unique_lock<std::mutex> lock{m_mutex};
    if (!m_mediaKeys)
    {
        return false;
    }
    return m_mediaKeys->containsKey(keySessionId, keyId);
}

bool CdmBackend::createKeySession(firebolt::rialto::KeySessionType sessionType, bool isLDL, int32_t &keySessionId)
{
    std::unique_lock<std::mutex> lock{m_mutex};
    // Sometimes app tries to create session before reaching RUNNING state. We have to wait for it.
    m_cv.wait_for(lock, std::chrono::seconds(1),
                  [this]() { return firebolt::rialto::ApplicationState::RUNNING == m_appState; });
    if (!m_mediaKeys)
    {
        return false;
    }
    return firebolt::rialto::MediaKeyErrorStatus::OK ==
           m_mediaKeys->createKeySession(sessionType, m_mediaKeysClient, isLDL, keySessionId);
}

bool CdmBackend::generateRequest(int32_t keySessionId, firebolt::rialto::InitDataType initDataType,
                                 const std::vector<uint8_t> &initData)
{
    std::unique_lock<std::mutex> lock{m_mutex};
    if (!m_mediaKeys)
    {
        return false;
    }
    return firebolt::rialto::MediaKeyErrorStatus::OK ==
           m_mediaKeys->generateRequest(keySessionId, initDataType, initData);
}

bool CdmBackend::loadSession(int32_t keySessionId)
{
    std::unique_lock<std::mutex> lock{m_mutex};
    if (!m_mediaKeys)
    {
        return false;
    }
    return firebolt::rialto::MediaKeyErrorStatus::OK == m_mediaKeys->loadSession(keySessionId);
}

bool CdmBackend::updateSession(int32_t keySessionId, const std::vector<uint8_t> &responseData)
{
    std::unique_lock<std::mutex> lock{m_mutex};
    if (!m_mediaKeys)
    {
        return false;
    }
    return firebolt::rialto::MediaKeyErrorStatus::OK == m_mediaKeys->updateSession(keySessionId, responseData);
}

bool CdmBackend::setDrmHeader(int32_t keySessionId, const std::vector<uint8_t> &requestData)
{
    std::unique_lock<std::mutex> lock{m_mutex};
    if (!m_mediaKeys)
    {
        return false;
    }
    return firebolt::rialto::MediaKeyErrorStatus::OK == m_mediaKeys->setDrmHeader(keySessionId, requestData);
}

bool CdmBackend::closeKeySession(int32_t keySessionId)
{
    std::unique_lock<std::mutex> lock{m_mutex};
    if (!m_mediaKeys)
    {
        return false;
    }
    return firebolt::rialto::MediaKeyErrorStatus::OK == m_mediaKeys->closeKeySession(keySessionId);
}

bool CdmBackend::removeKeySession(int32_t keySessionId)
{
    std::unique_lock<std::mutex> lock{m_mutex};
    if (!m_mediaKeys)
    {
        return false;
    }
    return firebolt::rialto::MediaKeyErrorStatus::OK == m_mediaKeys->removeKeySession(keySessionId);
}

bool CdmBackend::deleteDrmStore()
{
    std::unique_lock<std::mutex> lock{m_mutex};
    if (!m_mediaKeys)
    {
        return false;
    }
    return firebolt::rialto::MediaKeyErrorStatus::OK == m_mediaKeys->deleteDrmStore();
}

bool CdmBackend::deleteKeyStore()
{
    std::unique_lock<std::mutex> lock{m_mutex};
    if (!m_mediaKeys)
    {
        return false;
    }
    return firebolt::rialto::MediaKeyErrorStatus::OK == m_mediaKeys->deleteKeyStore();
}

bool CdmBackend::getDrmStoreHash(std::vector<unsigned char> &drmStoreHash)
{
    std::unique_lock<std::mutex> lock{m_mutex};
    if (!m_mediaKeys)
    {
        return false;
    }
    return firebolt::rialto::MediaKeyErrorStatus::OK == m_mediaKeys->getDrmStoreHash(drmStoreHash);
}

bool CdmBackend::getKeyStoreHash(std::vector<unsigned char> &keyStoreHash)
{
    std::unique_lock<std::mutex> lock{m_mutex};
    if (!m_mediaKeys)
    {
        return false;
    }
    return firebolt::rialto::MediaKeyErrorStatus::OK == m_mediaKeys->getKeyStoreHash(keyStoreHash);
}

bool CdmBackend::getLdlSessionsLimit(uint32_t &ldlLimit)
{
    std::unique_lock<std::mutex> lock{m_mutex};
    if (!m_mediaKeys)
    {
        return false;
    }
    return firebolt::rialto::MediaKeyErrorStatus::OK == m_mediaKeys->getLdlSessionsLimit(ldlLimit);
}

bool CdmBackend::getLastDrmError(int32_t keySessionId, uint32_t &errorCode)
{
    std::unique_lock<std::mutex> lock{m_mutex};
    if (!m_mediaKeys)
    {
        return false;
    }
    return firebolt::rialto::MediaKeyErrorStatus::OK == m_mediaKeys->getLastDrmError(keySessionId, errorCode);
}

bool CdmBackend::getDrmTime(uint64_t &drmTime)
{
    std::unique_lock<std::mutex> lock{m_mutex};
    if (!m_mediaKeys)
    {
        return false;
    }
    return firebolt::rialto::MediaKeyErrorStatus::OK == m_mediaKeys->getDrmTime(drmTime);
}

bool CdmBackend::getCdmKeySessionId(int32_t keySessionId, std::string &cdmKeySessionId)
{
    std::unique_lock<std::mutex> lock{m_mutex};
    if (!m_mediaKeys)
    {
        return false;
    }
    return firebolt::rialto::MediaKeyErrorStatus::OK == m_mediaKeys->getCdmKeySessionId(keySessionId, cdmKeySessionId);
}

bool CdmBackend::releaseKeySession(int32_t keySessionId)
{
    std::unique_lock<std::mutex> lock{m_mutex};
    if (!m_mediaKeys)
    {
        return false;
    }
    return firebolt::rialto::MediaKeyErrorStatus::OK == m_mediaKeys->releaseKeySession(keySessionId);
}

bool CdmBackend::createMediaKeys()
{
    if (!m_mediaKeysFactory)
    {
        m_log << error << "Failed to initialize media keys - not possible to create factory";
        return false;
    }

    m_mediaKeys = m_mediaKeysFactory->createMediaKeys(m_keySystem);
    if (!m_mediaKeys)
    {
        m_log << error << "Failed to initialize media keys - not possible to create media keys";
        return false;
    }
    return true;
}

bool CdmBackend::getMetricSystemData(std::vector<uint8_t> &buffer)
{
    std::unique_lock<std::mutex> lock{m_mutex};
    if (!m_mediaKeys)
    {
        return false;
    }
    return firebolt::rialto::MediaKeyErrorStatus::OK == m_mediaKeys->getMetricSystemData(buffer);
}
