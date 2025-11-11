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

#ifndef SIMPLE_BUFFER_POOL_H_
#define SIMPLE_BUFFER_POOL_H_

#include <map>
#include <memory>
#include <mutex>

class SimpleBufferPool
{
public:
    SimpleBufferPool();
    ~SimpleBufferPool();
    SimpleBufferPool(const SimpleBufferPool &) = delete;
    SimpleBufferPool(SimpleBufferPool &&) = delete;
    SimpleBufferPool &operator=(const SimpleBufferPool &) = delete;
    SimpleBufferPool &operator=(SimpleBufferPool &&) = delete;

    template <class T = uint8_t> T *allocate(size_t count)
    {
        return reinterpret_cast<T *>(allocateImpl(count * sizeof(T)));
    }

    template <class T = uint8_t> std::shared_ptr<T> allocateShared(size_t count)
    {
        return std::shared_ptr<T>(allocate(count), [this](T *p) { deallocate(p); });
    }

    void deallocate(void *p);

private:
    void *allocateImpl(size_t bytes);

private:
    uint8_t *m_staticBuf;
    size_t m_staticBufSize;

    struct BufInfo
    {
        size_t size;
        bool free;
    };

    std::mutex m_staticBufLock;
    std::map<void *, BufInfo> m_staticBufMetaData;
};

#endif // SIMPLE_BUFFER_POOL_H_
