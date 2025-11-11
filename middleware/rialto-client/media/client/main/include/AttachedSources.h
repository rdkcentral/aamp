/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2023 
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

#ifndef FIREBOLT_RIALTO_CLIENT_ATTACHED_SOURCES_H_
#define FIREBOLT_RIALTO_CLIENT_ATTACHED_SOURCES_H_

#include "MediaCommon.h"
#include <cstdint>
#include <map>
#include <mutex>

namespace firebolt::rialto::client
{
class AttachedSources
{
    struct Source
    {
        MediaSourceType type{MediaSourceType::UNKNOWN};
        bool isFlushing{false};
    };

public:
    AttachedSources() = default;
    ~AttachedSources() = default;
    AttachedSources(const AttachedSources &) = delete;
    AttachedSources(AttachedSources &&) = delete;
    AttachedSources &operator=(const AttachedSources &) = delete;
    AttachedSources &operator=(AttachedSources &&) = delete;

    void add(std::uint32_t id, const MediaSourceType &mediaSourceType);
    void remove(std::uint32_t id);
    MediaSourceType getType(std::uint32_t id) const;
    bool isFlushing(std::uint32_t id) const;
    void setFlushing(std::uint32_t id, bool flushing);

private:
    mutable std::mutex m_mutex;
    std::map<std::uint32_t, Source> m_attachedSources;
};
} // namespace firebolt::rialto::client

#endif // FIREBOLT_RIALTO_CLIENT_ATTACHED_SOURCES_H_
