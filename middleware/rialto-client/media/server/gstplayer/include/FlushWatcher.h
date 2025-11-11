/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2025 
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

#ifndef FIREBOLT_RIALTO_SERVER_FLUSH_WATCHER_H_
#define FIREBOLT_RIALTO_SERVER_FLUSH_WATCHER_H_

#include "IFlushWatcher.h"
#include <map>
#include <mutex>

namespace firebolt::rialto::server
{
class FlushWatcher : public IFlushWatcher
{
public:
    FlushWatcher() = default;
    ~FlushWatcher() override = default;

    void setFlushing(const MediaSourceType &type, bool async) override;
    void setFlushed(const MediaSourceType &type) override;
    bool isFlushOngoing() const override;
    bool isAsyncFlushOngoing() const override;

private:
    mutable std::mutex m_mutex;
    std::map<MediaSourceType, bool> m_flushingSources;
};
} // namespace firebolt::rialto::server
#endif // FIREBOLT_RIALTO_SERVER_FLUSH_WATCHER_H_
