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

#ifndef FIREBOLT_RIALTO_SERVER_SERVICE_I_CDM_SERVICE_H_
#define FIREBOLT_RIALTO_SERVER_SERVICE_I_CDM_SERVICE_H_

#include "IHeartbeatProcedure.h"
#include "IMediaKeysCapabilities.h"
#include "IMediaKeysServerInternal.h"
#include "MediaCommon.h"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace firebolt::rialto::server::service
{
class ICdmService
{
public:
    ICdmService() = default;
    virtual ~ICdmService() = default;

    ICdmService(const ICdmService &) = delete;
    ICdmService(ICdmService &&) = delete;
    ICdmService &operator=(const ICdmService &) = delete;
    ICdmService &operator=(ICdmService &&) = delete;

    virtual bool switchToActive() = 0;
    virtual void switchToInactive() = 0;

    virtual bool createMediaKeys(int mediaKeysHandle, std::string keySystem) = 0;
    virtual bool destroyMediaKeys(int mediaKeysHandle) = 0;
    virtual MediaKeyErrorStatus createKeySession(int mediaKeysHandle, KeySessionType sessionType,
                                                 const std::shared_ptr<IMediaKeysClient> &client, bool isLDL,
                                                 int32_t &keySessionId) = 0;
    virtual MediaKeyErrorStatus generateRequest(int mediaKeysHandle, int32_t keySessionId, InitDataType initDataType,
                                                const std::vector<uint8_t> &initData) = 0;
    virtual MediaKeyErrorStatus loadSession(int mediaKeysHandle, int32_t keySessionId) = 0;
    virtual MediaKeyErrorStatus updateSession(int mediaKeysHandle, int32_t keySessionId,
                                              const std::vector<uint8_t> &responseData) = 0;
    virtual MediaKeyErrorStatus closeKeySession(int mediaKeysHandle, int32_t keySessionId) = 0;
    virtual MediaKeyErrorStatus removeKeySession(int mediaKeysHandle, int32_t keySessionId) = 0;
    virtual MediaKeyErrorStatus getCdmKeySessionId(int mediaKeysHandle, int32_t keySessionId,
                                                   std::string &cdmKeySessionId) = 0;
    virtual bool containsKey(int mediaKeysHandle, int32_t keySessionId, const std::vector<uint8_t> &keyId) = 0;
    virtual MediaKeyErrorStatus setDrmHeader(int mediaKeysHandle, int32_t keySessionId,
                                             const std::vector<uint8_t> &requestData) = 0;
    virtual MediaKeyErrorStatus deleteDrmStore(int mediaKeysHandle) = 0;
    virtual MediaKeyErrorStatus deleteKeyStore(int mediaKeysHandle) = 0;
    virtual MediaKeyErrorStatus getDrmStoreHash(int mediaKeysHandle, std::vector<unsigned char> &drmStoreHash) = 0;
    virtual MediaKeyErrorStatus getKeyStoreHash(int mediaKeysHandle, std::vector<unsigned char> &keyStoreHash) = 0;
    virtual MediaKeyErrorStatus getLdlSessionsLimit(int mediaKeysHandle, uint32_t &ldlLimit) = 0;
    virtual MediaKeyErrorStatus getLastDrmError(int mediaKeysHandle, int32_t keySessionId, uint32_t &errorCode) = 0;
    virtual MediaKeyErrorStatus getDrmTime(int mediaKeysHandle, uint64_t &drmTime) = 0;
    virtual MediaKeyErrorStatus releaseKeySession(int mediaKeysHandle, int32_t keySessionId) = 0;
    virtual MediaKeyErrorStatus getMetricSystemData(int mediaKeysHandle, std::vector<uint8_t> &buffer) = 0;

    virtual std::vector<std::string> getSupportedKeySystems() = 0;
    virtual bool supportsKeySystem(const std::string &keySystem) = 0;
    virtual bool getSupportedKeySystemVersion(const std::string &keySystem, std::string &version) = 0;
    virtual bool isServerCertificateSupported(const std::string &keySystem) = 0;

    virtual void ping(const std::shared_ptr<IHeartbeatProcedure> &heartbeatProcedure) = 0;
};
} // namespace firebolt::rialto::server::service

#endif // FIREBOLT_RIALTO_SERVER_SERVICE_I_CDM_SERVICE_H_
