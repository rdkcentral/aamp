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

#include "EnvVariableParser.h"
#include "LogFileHandle.h"
#include <algorithm>
#include <string>
#include <unistd.h>
#include <vector>

namespace
{
std::string getRialtoDebug()
{
    const char *kDebugVar = getenv("RIALTO_DEBUG");
    if (kDebugVar)
    {
        return std::string(kDebugVar);
    }
    return "";
}

std::string getRialtoConsoleLog()
{
    const char *kDebugVar = getenv("RIALTO_CONSOLE_LOG");
    if (kDebugVar)
    {
        return std::string(kDebugVar);
    }
    return "";
}

std::string getRialtoLogPath()
{
    const char *kLogPathEnvVar = getenv("RIALTO_LOG_PATH");
    if (kLogPathEnvVar)
    {
        return std::string(kLogPathEnvVar);
    }
    return "";
}

inline bool isNumber(const std::string &str)
{
    return std::find_if(str.begin(), str.end(), [](unsigned char c) { return !std::isdigit(c); }) == str.end();
}

std::vector<std::string> split(std::string s, const std::string &delimiter)
{
    std::vector<std::string> result;
    size_t pos = 0;
    while ((pos = s.find(delimiter)) != std::string::npos)
    {
        result.push_back(s.substr(0, pos));
        s.erase(0, pos + delimiter.length());
    }
    result.push_back(s);
    return result;
}

std::map<std::string, int> parseEnvVar(const std::string &envVar)
{
    std::map<std::string, int> flagValues;
    auto componentsWithLevels{split(envVar, ";")};
    for (const auto &item : componentsWithLevels)
    {
        auto componentWithLevel{split(item, ":")};
        if (componentWithLevel.size() != 2 || !isNumber(componentWithLevel[1]))
            continue;
        flagValues[componentWithLevel[0]] = std::stoi(componentWithLevel[1]);
    }
    return flagValues;
}

RIALTO_DEBUG_LEVEL levelFromNumber(int level)
{
    switch (level)
    {
    case 0:
        return RIALTO_DEBUG_LEVEL(RIALTO_DEBUG_LEVEL_FATAL);
    case 1:
        return RIALTO_DEBUG_LEVEL(RIALTO_DEBUG_LEVEL_FATAL | RIALTO_DEBUG_LEVEL_ERROR);
    case 2:
        return RIALTO_DEBUG_LEVEL(RIALTO_DEBUG_LEVEL_FATAL | RIALTO_DEBUG_LEVEL_ERROR | RIALTO_DEBUG_LEVEL_WARNING);
    case 3:
        return RIALTO_DEBUG_LEVEL(RIALTO_DEBUG_LEVEL_FATAL | RIALTO_DEBUG_LEVEL_ERROR | RIALTO_DEBUG_LEVEL_WARNING |
                                  RIALTO_DEBUG_LEVEL_MILESTONE);
    case 4:
        return RIALTO_DEBUG_LEVEL(RIALTO_DEBUG_LEVEL_FATAL | RIALTO_DEBUG_LEVEL_ERROR | RIALTO_DEBUG_LEVEL_WARNING |
                                  RIALTO_DEBUG_LEVEL_MILESTONE | RIALTO_DEBUG_LEVEL_INFO);
    case 5:
        return RIALTO_DEBUG_LEVEL(RIALTO_DEBUG_LEVEL_FATAL | RIALTO_DEBUG_LEVEL_ERROR | RIALTO_DEBUG_LEVEL_WARNING |
                                  RIALTO_DEBUG_LEVEL_MILESTONE | RIALTO_DEBUG_LEVEL_INFO | RIALTO_DEBUG_LEVEL_DEBUG);
    }
    return RIALTO_DEBUG_LEVEL_DEFAULT;
}

RIALTO_COMPONENT componentFromStr(const std::string &component)
{
    if ("client" == component)
    {
        return RIALTO_COMPONENT_CLIENT;
    }
    if ("sessionserver" == component)
    {
        return RIALTO_COMPONENT_SERVER;
    }
    if ("ipc" == component)
    {
        return RIALTO_COMPONENT_IPC;
    }
    if ("servermanager" == component)
    {
        return RIALTO_COMPONENT_SERVER_MANAGER;
    }
    if ("common" == component)
    {
        return RIALTO_COMPONENT_COMMON;
    }
    return RIALTO_COMPONENT_LAST;
}
} // namespace

namespace firebolt::rialto::logging
{
EnvVariableParser::EnvVariableParser()
    : m_debugLevels{{RIALTO_COMPONENT_CLIENT, RIALTO_DEBUG_LEVEL_DEFAULT},
                    {RIALTO_COMPONENT_SERVER, RIALTO_DEBUG_LEVEL_DEFAULT},
                    {RIALTO_COMPONENT_IPC, RIALTO_DEBUG_LEVEL_DEFAULT},
                    {RIALTO_COMPONENT_SERVER_MANAGER, RIALTO_DEBUG_LEVEL_DEFAULT},
                    {RIALTO_COMPONENT_COMMON, RIALTO_DEBUG_LEVEL_DEFAULT}},
      m_logToConsole{false}, m_logFilePath{getRialtoLogPath()}
{
    configureRialtoDebug();
    configureRialtoConsoleLog();
    configureFileLogging();
}

void EnvVariableParser::configureRialtoDebug()
{
    std::string debugFlagEnvVar = getRialtoDebug();
    if (debugFlagEnvVar.empty())
        return;
    if (isNumber(debugFlagEnvVar))
    {
        int level = std::stoi(debugFlagEnvVar);
        for (auto &elem : m_debugLevels)
            elem.second = levelFromNumber(level);
        return;
    }
    auto flagValues{parseEnvVar(debugFlagEnvVar)};
    auto defaultFlagIter = flagValues.find("*");
    if (defaultFlagIter != flagValues.end())
    {
        for (auto &elem : m_debugLevels)
            elem.second = levelFromNumber(defaultFlagIter->second);
        flagValues.erase(defaultFlagIter);
    }
    for (const auto &value : flagValues)
    {
        auto currentLevelIter = m_debugLevels.find(componentFromStr(value.first));
        if (currentLevelIter != m_debugLevels.end())
        {
            currentLevelIter->second = levelFromNumber(value.second);
        }
    }
}

void EnvVariableParser::configureRialtoConsoleLog()
{
    std::string debugFlagEnvVar = getRialtoConsoleLog();
    if (debugFlagEnvVar == "1")
    {
        m_logToConsole = true;
    }
}

void EnvVariableParser::configureFileLogging()
{
    if (isFileLoggingEnabled())
    {
        LogFileHandle::instance().init(m_logFilePath);
    }
}

RIALTO_DEBUG_LEVEL EnvVariableParser::getLevel(const RIALTO_COMPONENT &component) const
{
    if (RIALTO_COMPONENT_EXTERNAL == component)
    {
        return RIALTO_DEBUG_LEVEL_EXTERNAL;
    }
    auto levelIter = m_debugLevels.find(component);
    if (levelIter == m_debugLevels.end())
        return RIALTO_DEBUG_LEVEL_DEFAULT;
    return levelIter->second;
}

bool EnvVariableParser::isConsoleLoggingEnabled() const
{
    return m_logToConsole;
}

bool EnvVariableParser::isFileLoggingEnabled() const
{
    return !m_logFilePath.empty();
}
} // namespace firebolt::rialto::logging
