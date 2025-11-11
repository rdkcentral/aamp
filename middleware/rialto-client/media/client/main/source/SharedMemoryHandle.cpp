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

#include "SharedMemoryHandle.h"
#include "RialtoClientLogging.h"
#include <stdexcept>
#include <sys/mman.h>
#include <sys/un.h>
#include <unistd.h>

namespace firebolt::rialto::client
{
SharedMemoryHandle::SharedMemoryHandle(std::int32_t shmFd, std::uint32_t shmBufferLen)
    : m_shmFd{shmFd}, m_shmBufferLen{shmBufferLen}
{
    if ((-1 == m_shmFd) || (0U == m_shmBufferLen))
    {
        throw std::runtime_error("Shared buffer invalid");
    }

    m_shmBuffer = reinterpret_cast<uint8_t *>(mmap(NULL, m_shmBufferLen, PROT_READ | PROT_WRITE, MAP_SHARED, m_shmFd, 0));
    if (MAP_FAILED == m_shmBuffer)
    {
        close(m_shmFd);
        m_shmFd = -1;
        m_shmBuffer = nullptr;
        m_shmBufferLen = 0U;
        throw std::runtime_error("Failed to map databuffer: " + std::string(strerror(errno)));
    }
}

SharedMemoryHandle::~SharedMemoryHandle()
{
    if (-1 == m_shmFd)
    {
        RIALTO_CLIENT_LOG_WARN("Shared memory not initalised");
        return;
    }

    int32_t ret = munmap(m_shmBuffer, m_shmBufferLen);
    if (-1 == ret)
    {
        RIALTO_CLIENT_LOG_ERROR("Failed to unmap databuffer: %s", strerror(errno));
    }
    else
    {
        RIALTO_CLIENT_LOG_INFO("Shared buffer was successfully terminated");
    }

    close(m_shmFd);
    m_shmBuffer = nullptr;
    m_shmFd = -1;
    m_shmBufferLen = 0U;
}

std::uint8_t *SharedMemoryHandle::getShm() const
{
    return m_shmBuffer;
}
} // namespace firebolt::rialto::client
