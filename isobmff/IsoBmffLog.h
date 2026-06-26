/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2026 RDK Management
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

/**
 * @file IsoBmffLog.h
 * @brief Runtime-injected logging interface for the isobmff static library.
 *
 * The library has zero dependency on AampLogManager.h or PlayerLogManager.h.
 * Callers construct an IsoBmff::Logger and pass it to IsoBmffBuffer /
 * IsoBmffHelper constructors.  When no logger is provided the default-
 * constructed Logger has a null func, and all ISOBMFF_LOG_* calls are no-ops.
 */

#ifndef __ISOBMFF_LOG_H__
#define __ISOBMFF_LOG_H__

#include <functional>
#include <string>
#include <cstdio>

namespace IsoBmff {

enum class LogLevel { TRACE = 0, INFO = 1, WARN = 2, MIL = 3, ERR = 4 };

using LogFunction = std::function<void(LogLevel level, std::string&& msg)>;

struct Logger
{
    LogFunction func;
    LogLevel    minLevel{LogLevel::WARN};

    bool IsEnabled(LogLevel level) const noexcept
    {
        return func && level >= minLevel;
    }
};

namespace Log {

// Format a printf-style message into a std::string
template<typename... Args>
inline std::string Format(const char* fmt, Args&&... args)
{
    char buf[2048];
    std::snprintf(buf, sizeof(buf), fmt, std::forward<Args>(args)...);
    return std::string(buf);
}

inline std::string Format(const char* msg)
{
    return std::string(msg);
}

} // namespace Log
} // namespace IsoBmff

#define ISOBMFF_LOG(logger, level, ...) \
    do { \
        if ((logger).IsEnabled(level)) { \
            (logger).func(level, IsoBmff::Log::Format(__VA_ARGS__)); \
        } \
    } while(0)

#define ISOBMFF_LOG_TRACE(logger, ...) ISOBMFF_LOG((logger), IsoBmff::LogLevel::TRACE, __VA_ARGS__)
#define ISOBMFF_LOG_INFO(logger, ...)  ISOBMFF_LOG((logger), IsoBmff::LogLevel::INFO,  __VA_ARGS__)
#define ISOBMFF_LOG_WARN(logger, ...)  ISOBMFF_LOG((logger), IsoBmff::LogLevel::WARN,  __VA_ARGS__)
#define ISOBMFF_LOG_MIL(logger, ...)   ISOBMFF_LOG((logger), IsoBmff::LogLevel::MIL,   __VA_ARGS__)
#define ISOBMFF_LOG_ERR(logger, ...)   ISOBMFF_LOG((logger), IsoBmff::LogLevel::ERR,   __VA_ARGS__)

#endif /* __ISOBMFF_LOG_H__ */
