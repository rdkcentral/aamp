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

#ifndef FIREBOLT_RIALTO_SERVER_I_DECRYPTION_SERVICE_H_
#define FIREBOLT_RIALTO_SERVER_I_DECRYPTION_SERVICE_H_

#include "MediaCommon.h"
#include <cstdint>
#include <gst/gst.h>
#include <vector>

namespace firebolt::rialto::server
{
class IDecryptionService
{
public:
    virtual ~IDecryptionService() = default;
    virtual MediaKeyErrorStatus decrypt(int32_t keySessionId, GstBuffer *encrypted, GstCaps *caps) = 0;
    virtual bool isPlayreadyKeySystem(int32_t keySessionId) = 0;
    virtual MediaKeyErrorStatus selectKeyId(int32_t keySessionId, const std::vector<uint8_t> &keyId) = 0;
    virtual void incrementSessionIdUsageCounter(int32_t keySessionId) = 0;
    virtual void decrementSessionIdUsageCounter(int32_t keySessionId) = 0;
};
} // namespace firebolt::rialto::server

#endif // FIREBOLT_RIALTO_SERVER_I_DECRYPTION_SERVICE_H_
