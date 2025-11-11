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

#ifndef FIREBOLT_RIALTO_LOGGING_ENV_VARIABLE_PARSER_H_
#define FIREBOLT_RIALTO_LOGGING_ENV_VARIABLE_PARSER_H_
#ifdef __cplusplus

#include "RialtoLogging.h"
#include <map>
#include <string>

namespace firebolt::rialto::logging
{
class EnvVariableParser
{
public:
    EnvVariableParser();
    ~EnvVariableParser() = default;
    EnvVariableParser(const EnvVariableParser &) = delete;
    EnvVariableParser(EnvVariableParser &&) = delete;
    EnvVariableParser &operator=(const EnvVariableParser &) = delete;
    EnvVariableParser &operator=(EnvVariableParser &&) = delete;

    RIALTO_DEBUG_LEVEL getLevel(const RIALTO_COMPONENT &component) const;
    bool isConsoleLoggingEnabled() const;
    bool isFileLoggingEnabled() const;

private:
    void configureRialtoDebug();
    void configureRialtoConsoleLog();
    void configureFileLogging();

private:
    std::map<RIALTO_COMPONENT, RIALTO_DEBUG_LEVEL> m_debugLevels;
    bool m_logToConsole;
    std::string m_logFilePath;
};
} // namespace firebolt::rialto::logging

#endif // defined(__cplusplus)
#endif // FIREBOLT_RIALTO_LOGGING_ENV_VARIABLE_PARSER_H_
