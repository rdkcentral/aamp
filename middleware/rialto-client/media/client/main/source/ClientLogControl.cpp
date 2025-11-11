/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2024 
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

#include "ClientLogControl.h"
#include "RialtoClientLogging.h"

namespace
{
const std::vector<RIALTO_COMPONENT> kAllComponentsToLog{RIALTO_COMPONENT_CLIENT, RIALTO_COMPONENT_IPC,
                                                        RIALTO_COMPONENT_COMMON};

firebolt::rialto::IClientLogHandler::Level convertLevel(const RIALTO_DEBUG_LEVEL &level)
{
    switch (level)
    {
    case RIALTO_DEBUG_LEVEL_FATAL:
        return firebolt::rialto::IClientLogHandler::Level::Fatal;
    case RIALTO_DEBUG_LEVEL_ERROR:
        return firebolt::rialto::IClientLogHandler::Level::Error;
    case RIALTO_DEBUG_LEVEL_WARNING:
        return firebolt::rialto::IClientLogHandler::Level::Warning;
    case RIALTO_DEBUG_LEVEL_MILESTONE:
        return firebolt::rialto::IClientLogHandler::Level::Milestone;
    case RIALTO_DEBUG_LEVEL_INFO:
        return firebolt::rialto::IClientLogHandler::Level::Info;
    case RIALTO_DEBUG_LEVEL_DEBUG:
        return firebolt::rialto::IClientLogHandler::Level::Debug;
    case RIALTO_DEBUG_LEVEL_EXTERNAL:
        return firebolt::rialto::IClientLogHandler::Level::External;
    case RIALTO_DEBUG_LEVEL_DEFAULT:
        return firebolt::rialto::IClientLogHandler::Level::Milestone;
    }
    return firebolt::rialto::IClientLogHandler::Level::Debug;
}
} // namespace

namespace firebolt::rialto
{
std::shared_ptr<IClientLogControlFactory> IClientLogControlFactory::createFactory()
{
    return client::ClientLogControlFactory::createFactory();
}
} // namespace firebolt::rialto

namespace firebolt::rialto::client
{
std::shared_ptr<IClientLogControlFactory> ClientLogControlFactory::createFactory()
{
    return std::make_shared<client::ClientLogControlFactory>();
}

IClientLogControl &ClientLogControlFactory::createClientLogControl()
{
    static std::unique_ptr<IClientLogControl> clientLogControl{std::make_unique<ClientLogControl>()};
    return *clientLogControl;
}

ClientLogControl::ClientLogControl()
{
    RIALTO_CLIENT_LOG_DEBUG("entry:");
}

ClientLogControl::~ClientLogControl()
{
    RIALTO_CLIENT_LOG_DEBUG("entry:");
    std::unique_lock<std::mutex> lock{m_logHandlerMutex};
    if (m_logHandler)
        cancelLogHandler();
}

bool ClientLogControl::registerLogHandler(const std::shared_ptr<IClientLogHandler> &handler, bool ignoreLogLevels)
{
    std::unique_lock<std::mutex> lock{m_logHandlerMutex};

    if (m_logHandler)
    {
        if (handler)
        {
            RIALTO_CLIENT_LOG_WARN("Replacing old log handler");
        }
        cancelLogHandler();
    }

    m_logHandler = handler;

    if (!handler)
        return true;

    for (auto component : kAllComponentsToLog)
    {
        if (firebolt::rialto::logging::setLogHandler(component,
                                                     std::bind(&ClientLogControl::forwardLog, this, component,
                                                               std::placeholders::_1, std::placeholders::_2,
                                                               std::placeholders::_3, std::placeholders::_4,
                                                               std::placeholders::_5, std::placeholders::_6),
                                                     ignoreLogLevels) !=
            firebolt::rialto::logging::RIALTO_LOGGING_STATUS_OK)
        {
            RIALTO_CLIENT_LOG_WARN("Unable to register log handler");
            cancelLogHandler();
            return false;
        }
    }
    return true;
}

void ClientLogControl::cancelLogHandler()
{
    RIALTO_CLIENT_LOG_INFO("Cancelling log handler");
    for (auto component : kAllComponentsToLog)
    {
        firebolt::rialto::logging::setLogHandler(component, nullptr, false);
    }
    m_logHandler = nullptr;
}

void ClientLogControl::forwardLog(RIALTO_COMPONENT component, RIALTO_DEBUG_LEVEL level, const char *file, int line,
                                  const char *function, const char *message, std::size_t messageLen)
{
    // Take a local copy to ensure thread safety
    std::shared_ptr<IClientLogHandler> logHandler{m_logHandler};
    if (logHandler)
    {
        logHandler->log(convertLevel(level), std::string(file), line, std::string(function), std::string(message));
    }
}
}; // namespace firebolt::rialto::client
