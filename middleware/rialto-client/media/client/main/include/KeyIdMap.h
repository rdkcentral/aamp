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

#ifndef FIREBOLT_RIALTO_CLIENT_KEY_IDMAP_H_
#define FIREBOLT_RIALTO_CLIENT_KEY_IDMAP_H_

#include <cstdint>
#include <map>
#include <mutex>
#include <vector>

namespace firebolt::rialto::client
{
class KeyIdMap
{
public:
    static KeyIdMap &instance();
    void addSession(std::int32_t keySessionId);
    bool updateKey(std::int32_t keySessionId, const std::vector<std::uint8_t> &keyId);
    std::vector<std::uint8_t> get(std::int32_t keySessionId) const;
    void erase(std::int32_t keySessionId);

private:
    KeyIdMap() = default;
    ~KeyIdMap() = default;
    KeyIdMap(const KeyIdMap &) = delete;
    KeyIdMap(KeyIdMap &&) = delete;
    KeyIdMap &operator=(const KeyIdMap &) = delete;
    KeyIdMap &operator=(KeyIdMap &&) = delete;

private:
    mutable std::mutex m_mutex;
    std::map<std::int32_t, std::vector<std::uint8_t>> m_keyIdMap;
};
} // namespace firebolt::rialto::client

#endif // FIREBOLT_RIALTO_CLIENT_KEY_IDMAP_H_
