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

#ifndef I_CDM_BACKEND_H_
#define I_CDM_BACKEND_H_

#include <ControlCommon.h>
#include <IMediaKeysClient.h>
#include <MediaCommon.h>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class ICdmBackend
{
public:
    virtual ~ICdmBackend() = default;

    virtual bool initialize(const firebolt::rialto::ApplicationState &initialState) = 0;
    virtual bool selectKeyId(int32_t keySessionId, const std::vector<uint8_t> &keyId) = 0;
    virtual bool containsKey(int32_t keySessionId, const std::vector<uint8_t> &keyId) = 0;
    virtual bool createKeySession(firebolt::rialto::KeySessionType sessionType, bool isLDL, int32_t &keySessionId) = 0;
    virtual bool generateRequest(int32_t keySessionId, firebolt::rialto::InitDataType initDataType,
                                 const std::vector<uint8_t> &initData) = 0;
    virtual bool loadSession(int32_t keySessionId) = 0;
    virtual bool updateSession(int32_t keySessionId, const std::vector<uint8_t> &responseData) = 0;
    virtual bool setDrmHeader(int32_t keySessionId, const std::vector<uint8_t> &requestData) = 0;
    virtual bool closeKeySession(int32_t keySessionId) = 0;
    virtual bool removeKeySession(int32_t keySessionId) = 0;
    virtual bool deleteDrmStore() = 0;
    virtual bool deleteKeyStore() = 0;
    virtual bool getDrmStoreHash(std::vector<unsigned char> &drmStoreHash) = 0;
    virtual bool getKeyStoreHash(std::vector<unsigned char> &keyStoreHash) = 0;
    virtual bool getLdlSessionsLimit(uint32_t &ldlLimit) = 0;
    virtual bool getLastDrmError(int32_t keySessionId, uint32_t &errorCode) = 0;
    virtual bool getDrmTime(uint64_t &drmTime) = 0;
    virtual bool getCdmKeySessionId(int32_t keySessionId, std::string &cdmKeySessionId) = 0;
    virtual bool releaseKeySession(int32_t keySessionId) = 0;
    virtual bool getMetricSystemData(std::vector<uint8_t> &buffer) = 0;
};

#endif // I_CDM_BACKEND_H_
