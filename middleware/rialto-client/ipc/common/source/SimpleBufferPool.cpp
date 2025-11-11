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

#include <algorithm>

#include "IpcLogging.h"
#include "SimpleBufferPool.h"

SimpleBufferPool::SimpleBufferPool() : m_staticBuf(nullptr), m_staticBufSize(0)
{
    // allocate a 64kb 'static' buffer and split it up into smaller individual buffers
    m_staticBufSize = 64 * 1024;
    m_staticBuf = new uint8_t[m_staticBufSize];

    uint8_t *p = m_staticBuf;
    for (int i = 0; i < 8; i++)
    {
        m_staticBufMetaData.emplace(p, BufInfo{256, true});
        p += 256;
    }
    for (int i = 0; i < 6; i++)
    {
        m_staticBufMetaData.emplace(p, BufInfo{1024, true});
        p += 1024;
    }
    for (int i = 0; i < 2; i++)
    {
        m_staticBufMetaData.emplace(p, BufInfo{4096, true});
        p += 4096;
    }
    for (int i = 0; i < 1; i++)
    {
        m_staticBufMetaData.emplace(p, BufInfo{16384, true});
        p += 16384;
    }
    for (int i = 0; i < 1; i++)
    {
        m_staticBufMetaData.emplace(p, BufInfo{32768, true});
        p += 32768;
    }
}

SimpleBufferPool::~SimpleBufferPool()
{
    delete[] m_staticBuf;
}

void *SimpleBufferPool::allocateImpl(size_t bytes)
{
    // try and find a free static buffer that is big enough
    {
        std::lock_guard<std::mutex> locker(m_staticBufLock);

        auto entry = std::find_if(m_staticBufMetaData.begin(), m_staticBufMetaData.end(), [&](const auto &metadata)
                                  { return (metadata.second.free && (metadata.second.size >= bytes)); });
        if (m_staticBufMetaData.end() != entry)
        {
            entry->second.free = false;
            return entry->first;
        }
    }

    // RIALTO_IPC_LOG_DEBUG("no static buffers for alloc of size %zu", bytes);

    // failed, so revert to dynamic allocation
    return malloc(bytes);
}

void SimpleBufferPool::deallocate(void *p)
{
    // if the pointer is not within our static area then assume it was dynamically
    // allocated, in which case just free it
    if ((p < m_staticBuf) || (p >= (m_staticBuf + m_staticBufSize)))
    {
        free(p);
        return;
    }

    std::lock_guard<std::mutex> locker(m_staticBufLock);

    // else find the meta-data on the buffer pointer and mark as freed
    auto it = m_staticBufMetaData.find(p);
    if (it == m_staticBufMetaData.end())
    {
        RIALTO_IPC_LOG_FATAL("trying to free an unknown buffer from the pool!");
        return;
    }

    it->second.free = true;
}
