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

#ifndef OPENCDM_SYSTEM_H_
#define OPENCDM_SYSTEM_H_

#include "OpenCDMSession.h"
#include <opencdm/open_cdm.h>
#include <stdint.h>
#include <string>
#include <vector>

struct OpenCDMSystem
{
    OpenCDMSystem() = default;
    virtual ~OpenCDMSystem() = default;
    OpenCDMSystem(const OpenCDMSystem &) = default;
    OpenCDMSystem(OpenCDMSystem &&) = default;
    OpenCDMSystem &operator=(OpenCDMSystem &&) = default;
    OpenCDMSystem &operator=(const OpenCDMSystem &) = default;

    virtual const std::string &keySystem() const = 0;
    virtual const std::string &metadata() const = 0;
    virtual OpenCDMSession *createSession(const LicenseType licenseType, OpenCDMSessionCallbacks *callbacks,
                                          void *userData, const std::string &initDataType,
                                          const std::vector<uint8_t> &initData) const = 0;
    virtual bool getDrmTime(uint64_t &drmTime) const = 0;
    virtual bool getLdlSessionsLimit(uint32_t &ldlLimit) const = 0;
    virtual bool getKeyStoreHash(std::vector<unsigned char> &keyStoreHash) const = 0;
    virtual bool getDrmStoreHash(std::vector<unsigned char> &drmStoreHash) const = 0;
    virtual bool deleteKeyStore() const = 0;
    virtual bool deleteDrmStore() const = 0;
    virtual bool getMetricSystemData(std::vector<uint8_t> &buffer) const = 0;
};

#endif // OPENCDM_SYSTEM_H_
