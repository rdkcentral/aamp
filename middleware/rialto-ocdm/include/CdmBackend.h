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

#ifndef CDM_BACKEND_H_
#define CDM_BACKEND_H_

#include "ICdmBackend.h"
#include "Logger.h"
#include "MessageDispatcher.h"
#include <IControlClient.h>
#include <IMediaKeys.h>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

class CdmBackend : public ICdmBackend, public firebolt::rialto::IControlClient
{
public:
    CdmBackend(const std::string &keySystem, const std::shared_ptr<firebolt::rialto::IMediaKeysClient> &mediaKeysClient,
               const std::shared_ptr<firebolt::rialto::IMediaKeysFactory> &mediaKeysFactory);
    ~CdmBackend() override = default;

    void notifyApplicationState(firebolt::rialto::ApplicationState state) override;

    bool initialize(const firebolt::rialto::ApplicationState &initialState) override;

    bool selectKeyId(int32_t keySessionId, const std::vector<uint8_t> &keyId) override;
    bool containsKey(int32_t keySessionId, const std::vector<uint8_t> &keyId) override;
    bool createKeySession(firebolt::rialto::KeySessionType sessionType, bool isLDL, int32_t &keySessionId) override;
    bool generateRequest(int32_t keySessionId, firebolt::rialto::InitDataType initDataType,
                         const std::vector<uint8_t> &initData) override;
    bool loadSession(int32_t keySessionId) override;
    bool updateSession(int32_t keySessionId, const std::vector<uint8_t> &responseData) override;
    bool setDrmHeader(int32_t keySessionId, const std::vector<uint8_t> &requestData) override;
    bool closeKeySession(int32_t keySessionId) override;
    bool removeKeySession(int32_t keySessionId) override;
    bool deleteDrmStore() override;
    bool deleteKeyStore() override;
    bool getDrmStoreHash(std::vector<unsigned char> &drmStoreHash) override;
    bool getKeyStoreHash(std::vector<unsigned char> &keyStoreHash) override;
    bool getLdlSessionsLimit(uint32_t &ldlLimit) override;
    bool getLastDrmError(int32_t keySessionId, uint32_t &errorCode) override;
    bool getDrmTime(uint64_t &drmTime) override;
    bool getCdmKeySessionId(int32_t keySessionId, std::string &cdmKeySessionId) override;
    bool releaseKeySession(int32_t keySessionId) override;
    bool getMetricSystemData(std::vector<uint8_t> &buffer) override;

private:
    bool createMediaKeys();

private:
    Logger m_log;
    std::mutex m_mutex;
    std::condition_variable m_cv;
    firebolt::rialto::ApplicationState m_appState;
    const std::string m_keySystem;
    std::shared_ptr<firebolt::rialto::IMediaKeysClient> m_mediaKeysClient;
    std::shared_ptr<firebolt::rialto::IMediaKeysFactory> m_mediaKeysFactory;
    std::unique_ptr<firebolt::rialto::IMediaKeys> m_mediaKeys;
};

#endif // CDM_BACKEND_H_
