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

#ifndef ACTIVE_SESSIONS_H_
#define ACTIVE_SESSIONS_H_

#include "ICdmBackend.h"
#include "IMessageDispatcher.h"
#include "OpenCDMSession.h"
#include <MediaCommon.h>
#include <map>
#include <memory>
#include <mutex>
#include <opencdm/open_cdm.h>
#include <string>
#include <vector>

class ActiveSessions
{
public:
    static ActiveSessions &instance();

    OpenCDMSession *create(const std::shared_ptr<ICdmBackend> &cdm,
                           const std::shared_ptr<IMessageDispatcher> &messageDispatcher, const LicenseType &sessionType,
                           OpenCDMSessionCallbacks *callbacks, void *context, const std::string &initDataType,
                           const std::vector<uint8_t> &initData);
    OpenCDMSession *get(const std::vector<uint8_t> &keyId);
    void remove(OpenCDMSession *session);

private:
    ActiveSessions() = default;
    ~ActiveSessions() = default;

private:
    std::mutex m_mutex;
    std::map<OpenCDMSession *, int> m_activeSessions;
};

#endif // ACTIVE_SESSIONS_H_
