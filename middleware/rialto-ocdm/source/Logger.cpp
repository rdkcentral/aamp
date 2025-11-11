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

#include "Logger.h"
#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <unistd.h>

#ifdef USE_ETHANLOG

#include <ethanlog.h>

#define SYSTEM_LOG_FATAL(filename, function, line, ...) ethanlog(ETHAN_LOG_FATAL, filename, function, line, __VA_ARGS__)
#define SYSTEM_LOG_ERROR(filename, function, line, ...) ethanlog(ETHAN_LOG_ERROR, filename, function, line, __VA_ARGS__)
#define SYSTEM_LOG_WARN(filename, function, line, ...)                                                                 \
    ethanlog(ETHAN_LOG_WARNING, filename, function, line, __VA_ARGS__)
#define SYSTEM_LOG_MIL(filename, function, line, ...)                                                                  \
    ethanlog(ETHAN_LOG_MILESTONE, filename, function, line, __VA_ARGS__)
#define SYSTEM_LOG_INFO(filename, function, line, ...) ethanlog(ETHAN_LOG_INFO, filename, function, line, __VA_ARGS__)
#define SYSTEM_LOG_DEBUG(filename, function, line, ...) ethanlog(ETHAN_LOG_DEBUG, filename, function, line, __VA_ARGS__)

#else

#include <syslog.h>

#define SYSTEM_LOG_FATAL(filename, function, line, ...) syslog(LOG_CRIT, __VA_ARGS__)
#define SYSTEM_LOG_ERROR(filename, function, line, ...) syslog(LOG_ERR, __VA_ARGS__)
#define SYSTEM_LOG_WARN(filename, function, line, ...) syslog(LOG_WARNING, __VA_ARGS__)
#define SYSTEM_LOG_MIL(filename, function, line, ...) syslog(LOG_NOTICE, __VA_ARGS__)
#define SYSTEM_LOG_INFO(filename, function, line, ...) syslog(LOG_INFO, __VA_ARGS__)
#define SYSTEM_LOG_DEBUG(filename, function, line, ...) syslog(LOG_DEBUG, __VA_ARGS__)

#endif

namespace
{
Severity getLogLevel()
{
    const char *debugVar = getenv("RIALTO_DEBUG");
    if (debugVar)
    {
        std::string varStr{debugVar};
        if ("0" == varStr)
            return Severity::fatal;
        if ("1" == varStr)
            return Severity::error;
        if ("2" == varStr)
            return Severity::warn;
        if ("3" == varStr)
            return Severity::mil;
        if ("4" == varStr)
            return Severity::info;
        if ("5" == varStr)
            return Severity::debug;
    }
    return Severity::warn;
}

bool isConsoleLogEnabled()
{
    const char *debugVar = getenv("RIALTO_CONSOLE_LOG");
    if (debugVar)
    {
        return std::string(debugVar) == "1";
    }
    return false;
}

std::string getRialtoLogPath()
{
    const char *logPathEnvVar = getenv("RIALTO_LOG_PATH");
    if (logPathEnvVar)
    {
        return std::string(logPathEnvVar);
    }
    return "";
}

std::string toString(const Severity &severity)
{
    if (Severity::fatal == severity)
        return "ftl";
    if (Severity::error == severity)
        return "err";
    if (Severity::warn == severity)
        return "wrn";
    if (Severity::mil == severity)
        return "mil";
    if (Severity::info == severity)
        return "inf";
    if (Severity::debug == severity)
        return "dbg";
    return "???";
}

} // namespace

LogFile &LogFile::instance()
{
    static LogFile logFile;
    return logFile;
}

bool LogFile::write(const std::string &line)
{
    std::unique_lock<std::mutex> lock{m_mutex};
    m_file << line << std::endl;
    return true;
}

bool LogFile::isEnabled() const
{
    return m_file.is_open();
}

void LogFile::reset()
{
    tryCloseFile();
    tryOpenFile();
}

LogFile::LogFile()
{
    tryOpenFile();
}

LogFile::~LogFile()
{
    tryCloseFile();
}

void LogFile::tryOpenFile()
{
    std::string logPath{getRialtoLogPath()};
    if (!logPath.empty())
    {
        // Add suffix to have rialto client and rialto ocdm logs in separate files
        logPath += ".ocdm";
        m_file = std::fstream(logPath, std::fstream::out);
    }
}

void LogFile::tryCloseFile()
{
    if (m_file.is_open())
    {
        m_file.close();
    }
}

Flusher::Flusher(std::stringstream &stream, const std::string &componentName, const Severity &severity)
    : m_stream{stream}, m_severity{severity}
{
    if (LogFile::instance().isEnabled() || isConsoleLogEnabled())
    {
        const std::chrono::time_point<std::chrono::system_clock> now = std::chrono::system_clock::now();
        const std::time_t t_c = std::chrono::system_clock::to_time_t(now);
        m_stream << std::put_time(std::localtime(&t_c), "[%F %T]");
    }
    m_stream << "[" << componentName << "][" << toString(severity) << "]: ";
}

Flusher::~Flusher()
{
    if (getLogLevel() >= m_severity)
    {
        if (LogFile::instance().isEnabled())
        {
            LogFile::instance().write(m_stream.str());
        }
        else if (isConsoleLogEnabled())
        {
            std::cout << m_stream.str() << std::endl;
        }
        else
        {
            switch (m_severity)
            {
            case Severity::fatal:
                SYSTEM_LOG_FATAL(nullptr, nullptr, -1, "%s", m_stream.str().c_str());
                break;
            case Severity::error:
                SYSTEM_LOG_ERROR(nullptr, nullptr, -1, "%s", m_stream.str().c_str());
                break;
            case Severity::warn:
                SYSTEM_LOG_WARN(nullptr, nullptr, -1, "%s", m_stream.str().c_str());
                break;
            case Severity::mil:
                SYSTEM_LOG_MIL(nullptr, nullptr, -1, "%s", m_stream.str().c_str());
                break;
            case Severity::info:
                SYSTEM_LOG_INFO(nullptr, nullptr, -1, "%s", m_stream.str().c_str());
                break;
            case Severity::debug:
            default:
                SYSTEM_LOG_DEBUG(nullptr, nullptr, -1, "%s", m_stream.str().c_str());
                break;
            }
        }
    }
    m_stream.str("");
}

Logger::Logger(const std::string &componentName) : m_componentName{componentName} {}

Flusher Logger::operator<<(const Severity &severity) const
{
    return Flusher(m_stream, m_componentName, severity);
}
