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

#include "KeyIdMap.h"

namespace firebolt::rialto::client
{
KeyIdMap &KeyIdMap::instance()
{
    static KeyIdMap keyIdMap;
    return keyIdMap;
}

void KeyIdMap::addSession(std::int32_t keySessionId)
{
    std::unique_lock<std::mutex> lock{m_mutex};
    m_keyIdMap.insert(std::make_pair(keySessionId, std::vector<uint8_t>()));
}

bool KeyIdMap::updateKey(std::int32_t keySessionId, const std::vector<std::uint8_t> &keyId)
{
    std::unique_lock<std::mutex> lock{m_mutex};
    auto keyIdIter{m_keyIdMap.find(keySessionId)};
    if (m_keyIdMap.end() == keyIdIter)
    {
        return false;
    }
    keyIdIter->second = keyId;
    return true;
}

std::vector<std::uint8_t> KeyIdMap::get(std::int32_t keySessionId) const
{
    std::unique_lock<std::mutex> lock{m_mutex};
    auto keyIdIter{m_keyIdMap.find(keySessionId)};
    if (m_keyIdMap.end() == keyIdIter)
    {
        return std::vector<std::uint8_t>();
    }
    return keyIdIter->second;
}

void KeyIdMap::erase(std::int32_t keySessionId)
{
    std::unique_lock<std::mutex> lock{m_mutex};
    m_keyIdMap.erase(keySessionId);
}
} // namespace firebolt::rialto::client
