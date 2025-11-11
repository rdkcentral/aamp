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

#ifndef FIREBOLT_RIALTO_SERVER_SERVICE_CDM_SERVICE_H_
#define FIREBOLT_RIALTO_SERVER_SERVICE_CDM_SERVICE_H_

#include "ICdmService.h"
#include "IDecryptionService.h"
#include <atomic>
#include <condition_variable>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace firebolt::rialto::server::service
{
class CdmService : public ICdmService, public IDecryptionService
{
public:
    CdmService(std::shared_ptr<IMediaKeysServerInternalFactory> &&mediaKeysFactory,
               std::shared_ptr<IMediaKeysCapabilitiesFactory> &&mediaKeysCapabilitiesFactory);
    virtual ~CdmService();

    bool switchToActive() override;
    void switchToInactive() override;

    bool createMediaKeys(int mediaKeysHandle, std::string keySystem) override;
    bool destroyMediaKeys(int mediaKeysHandle) override;
    MediaKeyErrorStatus createKeySession(int mediaKeysHandle, KeySessionType sessionType,
                                         const std::shared_ptr<IMediaKeysClient> &client, bool isLDL,
                                         int32_t &keySessionId) override;
    MediaKeyErrorStatus generateRequest(int mediaKeysHandle, int32_t keySessionId, InitDataType initDataType,
                                        const std::vector<uint8_t> &initData) override;
    MediaKeyErrorStatus loadSession(int mediaKeysHandle, int32_t keySessionId) override;
    MediaKeyErrorStatus updateSession(int mediaKeysHandle, int32_t keySessionId,
                                      const std::vector<uint8_t> &responseData) override;
    MediaKeyErrorStatus closeKeySession(int mediaKeysHandle, int32_t keySessionId) override;
    MediaKeyErrorStatus removeKeySession(int mediaKeysHandle, int32_t keySessionId) override;
    MediaKeyErrorStatus getCdmKeySessionId(int mediaKeysHandle, int32_t keySessionId,
                                           std::string &cdmKeySessionId) override;
    bool containsKey(int mediaKeysHandle, int32_t keySessionId, const std::vector<uint8_t> &keyId) override;
    MediaKeyErrorStatus setDrmHeader(int mediaKeysHandle, int32_t keySessionId,
                                     const std::vector<uint8_t> &requestData) override;
    MediaKeyErrorStatus deleteDrmStore(int mediaKeysHandle) override;
    MediaKeyErrorStatus deleteKeyStore(int mediaKeysHandle) override;
    MediaKeyErrorStatus getDrmStoreHash(int mediaKeysHandle, std::vector<unsigned char> &drmStoreHash) override;
    MediaKeyErrorStatus getKeyStoreHash(int mediaKeysHandle, std::vector<unsigned char> &keyStoreHash) override;
    MediaKeyErrorStatus getLdlSessionsLimit(int mediaKeysHandle, uint32_t &ldlLimit) override;
    MediaKeyErrorStatus getLastDrmError(int mediaKeysHandle, int32_t keySessionId, uint32_t &errorCode) override;
    MediaKeyErrorStatus getDrmTime(int mediaKeysHandle, uint64_t &drmTime) override;
    MediaKeyErrorStatus releaseKeySession(int mediaKeysHandle, int32_t keySessionId) override;
    MediaKeyErrorStatus getMetricSystemData(int mediaKeysHandle, std::vector<uint8_t> &buffer) override;

    std::vector<std::string> getSupportedKeySystems() override;
    bool supportsKeySystem(const std::string &keySystem) override;
    bool getSupportedKeySystemVersion(const std::string &keySystem, std::string &version) override;
    bool isServerCertificateSupported(const std::string &keySystem) override;
    MediaKeyErrorStatus decrypt(int32_t keySessionId, GstBuffer *encrypted, GstCaps *caps) override;
    bool isPlayreadyKeySystem(int32_t keySessionId) override;
    MediaKeyErrorStatus selectKeyId(int32_t keySessionId, const std::vector<uint8_t> &keyId) override;
    void incrementSessionIdUsageCounter(int32_t keySessionId) override;
    void decrementSessionIdUsageCounter(int32_t keySessionId) override;
    void ping(const std::shared_ptr<IHeartbeatProcedure> &heartbeatProcedure) override;

private:
    std::shared_ptr<IMediaKeysServerInternalFactory> m_mediaKeysFactory;
    std::shared_ptr<IMediaKeysCapabilitiesFactory> m_mediaKeysCapabilitiesFactory;
    std::atomic<bool> m_isActive;
    std::map<int, std::unique_ptr<IMediaKeysServerInternal>> m_mediaKeys;
    std::map<int, std::shared_ptr<IMediaKeysClient>> m_mediaKeysClients;
    std::mutex m_mediaKeysMutex;

    MediaKeyErrorStatus removeKeySessionInternal(int mediaKeysHandle, int32_t keySessionId);
};
} // namespace firebolt::rialto::server::service

#endif // FIREBOLT_RIALTO_SERVER_SERVICE_CDM_SERVICE_H_
