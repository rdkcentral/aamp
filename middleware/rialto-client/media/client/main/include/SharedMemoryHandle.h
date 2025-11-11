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

#ifndef FIREBOLT_RIALTO_CLIENT_SHARED_MEMORY_HANDLE_H_
#define FIREBOLT_RIALTO_CLIENT_SHARED_MEMORY_HANDLE_H_

#include "ISharedMemoryHandle.h"

namespace firebolt::rialto::client
{
class SharedMemoryHandle : public ISharedMemoryHandle
{
public:
    SharedMemoryHandle(std::int32_t shmFd, std::uint32_t shmBufferLen);
    ~SharedMemoryHandle() override;

    std::uint8_t *getShm() const override;

private:
    /**
     * @brief The shared memory file descriptor.
     */
    std::int32_t m_shmFd;

    /**
     * @brief The shared memory buffer length.
     */
    std::uint32_t m_shmBufferLen;

    /**
     * @brief The shared memory buffer pointer.
     */
    std::uint8_t *m_shmBuffer;
};
} // namespace firebolt::rialto::client

#endif // FIREBOLT_RIALTO_CLIENT_SHARED_MEMORY_HANDLE_H_
