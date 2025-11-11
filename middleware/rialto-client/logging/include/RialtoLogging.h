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

#ifndef FIREBOLT_RIALTO_LOGGING_RIALTO_LOGGING_H_
#define FIREBOLT_RIALTO_LOGGING_RIALTO_LOGGING_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdarg.h>
#include <stdint.h>

    /**
     * Defined log levels used to set the required output logs for the component
     */
    enum RIALTO_DEBUG_LEVEL
    {
        RIALTO_DEBUG_LEVEL_FATAL = (1u << 0),
        RIALTO_DEBUG_LEVEL_ERROR = (1u << 1),
        RIALTO_DEBUG_LEVEL_WARNING = (1u << 2),
        RIALTO_DEBUG_LEVEL_MILESTONE = (1u << 3),
        RIALTO_DEBUG_LEVEL_INFO = (1u << 4),
        RIALTO_DEBUG_LEVEL_DEBUG = (1u << 5),
        RIALTO_DEBUG_LEVEL_EXTERNAL =
            (1u << 6), // Level controlled by an external variable, like e.g. GST_DEBUG. Keep it as last level.

        RIALTO_DEBUG_LEVEL_DEFAULT = (RIALTO_DEBUG_LEVEL_FATAL | RIALTO_DEBUG_LEVEL_ERROR | RIALTO_DEBUG_LEVEL_WARNING |
                                      RIALTO_DEBUG_LEVEL_MILESTONE)
    };

    /**
     * Possible components for logging.
     */
    enum RIALTO_COMPONENT
    {
        RIALTO_COMPONENT_DEFAULT = 0u,
        RIALTO_COMPONENT_CLIENT,
        RIALTO_COMPONENT_SERVER,
        RIALTO_COMPONENT_IPC,
        RIALTO_COMPONENT_SERVER_MANAGER,
        RIALTO_COMPONENT_COMMON,
        RIALTO_COMPONENT_EXTERNAL, // External component, like e.g. GStreamer. Keep it as last component.
        RIALTO_COMPONENT_LAST,
    };

    /**
     * Utility functions used by the logging macros to produce the log output
     */
    extern void rialtoLogVPrintf(enum RIALTO_COMPONENT component, enum RIALTO_DEBUG_LEVEL level, const char *file,
                                 const char *func, int line, const char *fmt, va_list ap)
        __attribute__((format(printf, 6, 0)));
    extern void rialtoLogPrintf(enum RIALTO_COMPONENT component, enum RIALTO_DEBUG_LEVEL level, const char *file,
                                const char *func, int line, const char *fmt, ...) __attribute__((format(printf, 6, 7)));
    extern void rialtoLogSysPrintf(enum RIALTO_COMPONENT component, int err, enum RIALTO_DEBUG_LEVEL level,
                                   const char *file, const char *func, int line, const char *fmt, ...)
        __attribute__((format(printf, 7, 8)));

    /**
     * Macros to be used for logging
     */

#ifdef RIALTO_LOG_FATAL_ENABLED
#define RIALTO_LOG_FATAL(component, fmt, args...)                                                                      \
    do                                                                                                                 \
    {                                                                                                                  \
        rialtoLogPrintf(component, RIALTO_DEBUG_LEVEL_FATAL, __FILE__, __FUNCTION__, __LINE__, fmt, ##args);           \
    } while (false)

#define RIALTO_LOG_SYS_FATAL(component, err, fmt, args...)                                                             \
    do                                                                                                                 \
    {                                                                                                                  \
        rialtoLogSysPrintf(component, err, RIALTO_DEBUG_LEVEL_FATAL, __FILE__, __FUNCTION__, __LINE__, fmt, ##args);   \
    } while (false)
#else
#define RIALTO_LOG_FATAL(component, fmt, args...)
#define RIALTO_LOG_SYS_FATAL(component, err, fmt, args...)
#endif

#ifdef RIALTO_LOG_ERROR_ENABLED
#define RIALTO_LOG_ERROR(component, fmt, args...)                                                                      \
    do                                                                                                                 \
    {                                                                                                                  \
        rialtoLogPrintf(component, RIALTO_DEBUG_LEVEL_ERROR, __FILE__, __FUNCTION__, __LINE__, fmt, ##args);           \
    } while (false)

#define RIALTO_LOG_SYS_ERROR(component, err, fmt, args...)                                                             \
    do                                                                                                                 \
    {                                                                                                                  \
        rialtoLogSysPrintf(component, err, RIALTO_DEBUG_LEVEL_ERROR, __FILE__, __FUNCTION__, __LINE__, fmt, ##args);   \
    } while (false)
#else
#define RIALTO_LOG_ERROR(component, fmt, args...)
#define RIALTO_LOG_SYS_ERROR(component, err, fmt, args...)
#endif

#ifdef RIALTO_LOG_WARN_ENABLED
#define RIALTO_LOG_WARN(component, fmt, args...)                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        rialtoLogPrintf(component, RIALTO_DEBUG_LEVEL_WARNING, __FILE__, __FUNCTION__, __LINE__, fmt, ##args);         \
    } while (false)
#define RIALTO_LOG_SYS_WARN(component, err, fmt, args...)                                                              \
    do                                                                                                                 \
    {                                                                                                                  \
        rialtoLogSysPrintf(component, err, RIALTO_DEBUG_LEVEL_WARNING, __FILE__, __FUNCTION__, __LINE__, fmt, ##args); \
    } while (false)
#else
#define RIALTO_LOG_WARN(component, fmt, args...)
#define RIALTO_LOG_SYS_WARN(component, err, fmt, args...)
#endif

#ifdef RIALTO_LOG_MIL_ENABLED
#define RIALTO_LOG_MIL(component, fmt, args...)                                                                        \
    do                                                                                                                 \
    {                                                                                                                  \
        rialtoLogPrintf(component, RIALTO_DEBUG_LEVEL_MILESTONE, __FILE__, __FUNCTION__, __LINE__, fmt, ##args);       \
    } while (false)
#else
#define RIALTO_LOG_MIL(component, fmt, args...)
#endif

#ifdef RIALTO_LOG_INFO_ENABLED
#define RIALTO_LOG_INFO(component, fmt, args...)                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        rialtoLogPrintf(component, RIALTO_DEBUG_LEVEL_INFO, __FILE__, __FUNCTION__, __LINE__, fmt, ##args);            \
    } while (false)
#else
#define RIALTO_LOG_INFO(component, fmt, args...)
#endif

#ifdef RIALTO_LOG_DEBUG_ENABLED
#define RIALTO_LOG_DEBUG(component, fmt, args...)                                                                      \
    do                                                                                                                 \
    {                                                                                                                  \
        rialtoLogPrintf(component, RIALTO_DEBUG_LEVEL_DEBUG, __FILE__, __FUNCTION__, __LINE__, fmt, ##args);           \
    } while (false)
#else
#define RIALTO_LOG_DEBUG(component, fmt, args...)
#endif

#define RIALTO_LOG_EXTERNAL(fmt, args...)                                                                              \
    do                                                                                                                 \
    {                                                                                                                  \
        rialtoLogPrintf(RIALTO_COMPONENT_EXTERNAL, RIALTO_DEBUG_LEVEL_EXTERNAL, __FILE__, __FUNCTION__, __LINE__, fmt, \
                        ##args);                                                                                       \
    } while (false)

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

#include <functional>

namespace firebolt::rialto::logging
{
/**
 * Error Return Status.
 */
enum RialtoLoggingStatus : uint32_t
{
    RIALTO_LOGGING_STATUS_OK = 0u,
    RIALTO_LOGGING_STATUS_ERROR,
};

/**
 * @brief Set the logging levels required.
 *
 *        See RIALTO_DEBUG_LEVEL_* for possible log levels. Multiple log levels can be set
 *        at once, for example: (RIALTO_DEBUG_LEVEL_FATAL | RIALTO_DEBUG_LEVEL_ERROR).
 *        Setting new log levels shall completely overwrite the previous log level set.
 * @retval  RIALTO_LOGGING_STATUS_OK on success. Note that if setLogHandler() has been
 *          called (for this component) with ignoreLogLevels==true then this will return
 *          RIALTO_LOGGING_STATUS_ERROR
 *
 * @param[in] logLevels : The levels of logging to set.
 * @param[in] component : The component to set the log levels on.
 */
RialtoLoggingStatus setLogLevels(RIALTO_COMPONENT component, RIALTO_DEBUG_LEVEL logLevels);

/**
 * @brief Get current logging levels.
 *
 *        See RIALTO_DEBUG_LEVEL_* for possible log levels.
 *        If setLogHandler() has been called with ignoreLogLevels==true then this will return RIALTO_DEBUG_LEVEL_EXTERNAL
 *
 * @param[in] component : The component to get the log levels from.
 */
RIALTO_DEBUG_LEVEL getLogLevels(RIALTO_COMPONENT component);

/**
 * @brief Set the log handler used to process the logs.
 *
 * After requesting a logHandler then it can be cancelled by
 * calling setLogHandler again with the handler passed as a nullptr
 *
 * @param[in] handler   : The log handler.
 * @param[in] component : The component to set the log handler on.
 */
using LogHandler = std::function<void(RIALTO_DEBUG_LEVEL level, const char *file, int line, const char *function,
                                      const char *message, std::size_t messageLen)>;
RialtoLoggingStatus setLogHandler(RIALTO_COMPONENT component, LogHandler handler, bool ignoreLogLevels);

/**
 * @brief Checks if rialto logs to console.
 *
 * @retval: Returns true, if rialto is logging to console
 */
bool isConsoleLoggingEnabled();

} // namespace firebolt::rialto::logging

#endif // defined(__cplusplus)

#endif // FIREBOLT_RIALTO_LOGGING_RIALTO_LOGGING_H_
