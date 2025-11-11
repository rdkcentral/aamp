/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2023 Sky UK
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

#include "LogFileHandle.h"

#include <fcntl.h>
#include <unistd.h>

namespace firebolt::rialto::logging
{
LogFileHandle &LogFileHandle::instance()
{
    static LogFileHandle handle;
    return handle;
}

void LogFileHandle::init(const std::string &path)
{
    if (-1 == m_fd)
    {
        m_fd = open(path.c_str(), O_CLOEXEC | O_CREAT | O_WRONLY | O_TRUNC, 0664);
    }
}

int LogFileHandle::fd() const
{
    return m_fd;
}

bool LogFileHandle::isOpen() const
{
    return m_fd != -1;
}

LogFileHandle::LogFileHandle() : m_fd{-1} {}

LogFileHandle::~LogFileHandle()
{
    if (-1 != m_fd)
    {
        close(m_fd);
    }
}
} // namespace firebolt::rialto::logging
