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

#include "FileDescriptor.h"
#include "IpcLogging.h"

#include <cerrno>
#include <fcntl.h>
#include <unistd.h>

namespace firebolt::rialto::ipc
{
FileDescriptor::FileDescriptor() : m_fd(-1) {}

FileDescriptor::FileDescriptor(int fd) : m_fd(-1)
{
    if (fd >= 0)
    {
        m_fd = fcntl(fd, F_DUPFD_CLOEXEC, 3);
        if (m_fd < 0)
            RIALTO_IPC_LOG_SYS_WARN(errno, "failed to dup supplied fd");
    }
}

FileDescriptor::FileDescriptor(const FileDescriptor &other) : m_fd(-1)
{
    if (other.m_fd >= 0)
    {
        m_fd = fcntl(other.m_fd, F_DUPFD_CLOEXEC, 3);
        if (m_fd < 0)
            RIALTO_IPC_LOG_SYS_WARN(errno, "failed to dup supplied fd");
    }
}

FileDescriptor &FileDescriptor::operator=(FileDescriptor &&other) noexcept
{
    if ((m_fd >= 0) && (::close(m_fd) != 0))
        RIALTO_IPC_LOG_SYS_ERROR(errno, "failed to close file descriptor");

    m_fd = other.m_fd;
    other.m_fd = -1;

    return *this;
}

FileDescriptor &FileDescriptor::operator=(const FileDescriptor &other)
{
    if (this == &other)
        return *this;

    if ((m_fd >= 0) && (::close(m_fd) != 0))
        RIALTO_IPC_LOG_SYS_ERROR(errno, "failed to close file descriptor");

    m_fd = -1;

    if (other.m_fd >= 0)
    {
        m_fd = fcntl(other.m_fd, F_DUPFD_CLOEXEC, 3);
        if (m_fd < 0)
            RIALTO_IPC_LOG_SYS_WARN(errno, "failed to dup supplied fd");
    }

    return *this;
}

FileDescriptor::~FileDescriptor()
{
    reset();
}

bool FileDescriptor::isValid() const
{
    return (m_fd >= 0);
}

int FileDescriptor::fd() const
{
    return m_fd;
}

void FileDescriptor::reset(int fd)
{
    if ((m_fd >= 0) && (::close(m_fd) != 0))
        RIALTO_IPC_LOG_SYS_ERROR(errno, "failed to close file descriptor");

    if (fd < 0)
    {
        m_fd = -1;
    }
    else
    {
        m_fd = fcntl(fd, F_DUPFD_CLOEXEC, 3);
        if (m_fd < 0)
            RIALTO_IPC_LOG_SYS_WARN(errno, "failed to dup supplied fd");
    }
}

void FileDescriptor::clear()
{
    reset();
}

int FileDescriptor::release()
{
    int descriptor = m_fd;
    m_fd = -1;
    return descriptor;
}

} // namespace firebolt::rialto::ipc
