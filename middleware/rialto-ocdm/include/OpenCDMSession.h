/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2022 Sky UK
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

#ifndef OPENCDM_SESSION_H_
#define OPENCDM_SESSION_H_

#include <MediaCommon.h>
#include <functional>
#include <opencdm/open_cdm.h>
#include <stdint.h>
#include <string>
#include <utility>
#include <vector>

struct _GstCaps;
struct _GstBuffer;
typedef struct _GstCaps GstCaps;
typedef struct _GstBuffer GstBuffer;

class OpenCDMSession
{
public:
    typedef std::function<void(const std::vector<uint8_t> &license_renewal_message, uint64_t session_id)> LicenseRenewalCallback;
    typedef std::function<void(const std::vector<std::pair<std::vector<uint8_t>, firebolt::rialto::KeyStatus>> &key_statuses,
                               uint64_t session_id)>
        KeyStatusesChangedCallback;

    OpenCDMSession() = default;
    virtual ~OpenCDMSession() = default;

    virtual bool initialize() = 0;
    virtual bool initialize(bool) = 0;
    virtual bool generateRequest(const std::string &initDataType, const std::vector<uint8_t> &initData,
                                 const std::vector<uint8_t> &cdmData) = 0;
    virtual bool loadSession() = 0;
    virtual bool updateSession(const std::vector<uint8_t> &license) = 0;
    virtual bool getChallengeData(std::vector<uint8_t> &challengeData) = 0;
    virtual bool containsKey(const std::vector<uint8_t> &keyId) = 0;
    virtual bool setDrmHeader(const std::vector<uint8_t> &drmHeader) = 0;
    virtual bool selectKeyId(const std::vector<uint8_t> &keyId) = 0;
    virtual void addProtectionMeta(GstBuffer *buffer, GstBuffer *subSample, const uint32_t subSampleCount,
                                   GstBuffer *IV, GstBuffer *keyID, uint32_t initWithLast15) = 0;
    virtual bool addProtectionMeta(GstBuffer *buffer) = 0;
    virtual bool closeSession() = 0;
    virtual bool removeSession() = 0;
    virtual KeyStatus status(const std::vector<uint8_t> &key) const = 0;

    virtual const std::string &getSessionId() const = 0;
    virtual uint32_t getLastDrmError() const = 0;
};

#endif // OPENCDM_SESSION_H_
