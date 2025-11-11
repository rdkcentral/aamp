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

#include "ActiveSessions.h"
#include "OpenCDMSessionPrivate.h"
#include <algorithm>

ActiveSessions &ActiveSessions::instance()
{
    static ActiveSessions activeSessions;
    return activeSessions;
}

OpenCDMSession *ActiveSessions::create(const std::shared_ptr<ICdmBackend> &cdm,
                                       const std::shared_ptr<IMessageDispatcher> &messageDispatcher,
                                       const LicenseType &sessionType, OpenCDMSessionCallbacks *callbacks, void *context,
                                       const std::string &initDataType, const std::vector<uint8_t> &initData)
{
    std::unique_lock<std::mutex> lock{m_mutex};
    OpenCDMSession *newSession =
        new OpenCDMSessionPrivate(cdm, messageDispatcher, sessionType, callbacks, context, initDataType, initData);
    m_activeSessions.insert(std::make_pair(newSession, 1));
    return newSession;
}

OpenCDMSession *ActiveSessions::get(const std::vector<uint8_t> &keyId)
{
    std::unique_lock<std::mutex> lock{m_mutex};
    auto sessionIter{std::find_if(m_activeSessions.begin(), m_activeSessions.end(), [&](const auto &iter)
                                  { return iter.first->status(keyId) != KeyStatus::InternalError; })};
    if (sessionIter != m_activeSessions.end())
    {
        ++sessionIter->second;
        return sessionIter->first;
    }
    return nullptr;
}

void ActiveSessions::remove(OpenCDMSession *session)
{
    std::unique_lock<std::mutex> lock{m_mutex};
    auto sessionIter{m_activeSessions.find(session)};
    if (sessionIter != m_activeSessions.end())
    {
        --sessionIter->second;
        if (0 == sessionIter->second)
        {
            delete sessionIter->first;
            m_activeSessions.erase(sessionIter);
        }
    }
}
