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

#ifndef LOGGER_H_
#define LOGGER_H_

#include <fstream>
#include <mutex>
#include <sstream>
#include <string>

enum Severity
{
    fatal = 0,
    error = 1,
    warn = 2,
    mil = 3,
    info = 4,
    debug = 5
};

class LogFile
{
public:
    static LogFile &instance();
    bool write(const std::string &line);
    bool isEnabled() const;
    void reset();

private:
    LogFile();
    ~LogFile();

    void tryOpenFile();
    void tryCloseFile();

private:
    std::fstream m_file;
    std::mutex m_mutex;
};

class Logger;
class Flusher
{
public:
    Flusher(std::stringstream &stream, const std::string &componentName, const Severity &severity);
    ~Flusher();

    template <typename T> Flusher &operator<<(const T &text)
    {
        m_stream << text;
        return *this;
    }

private:
    std::stringstream &m_stream;
    Severity m_severity;
};

class Logger
{
public:
    explicit Logger(const std::string &componentName);
    Flusher operator<<(const Severity &) const;

private:
    const std::string m_componentName;
    mutable std::stringstream m_stream;
};

#endif // LOGGER_H_
