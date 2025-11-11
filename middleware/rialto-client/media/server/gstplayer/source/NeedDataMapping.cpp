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

#include "NeedDataMapping.h"

namespace firebolt::rialto::server
{
bool NeedDataMapping::isNeedDataScheduled(GstAppSrc *src) const
{
    std::unique_lock lock{m_mutex};
    return m_scheduledNeedDatas.find(src) != m_scheduledNeedDatas.end();
}

void NeedDataMapping::setNeedDataScheduled(GstAppSrc *src)
{
    std::unique_lock lock{m_mutex};
    m_scheduledNeedDatas.insert(src);
}

void NeedDataMapping::clearNeedDataScheduled(GstAppSrc *src)
{
    std::unique_lock lock{m_mutex};
    m_scheduledNeedDatas.erase(src);
}
} // namespace firebolt::rialto::server
