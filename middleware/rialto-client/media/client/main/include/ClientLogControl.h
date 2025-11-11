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

#ifndef FIREBOLT_RIALTO_CLIENT_CLIENT_LOG_CONTROL_H_
#define FIREBOLT_RIALTO_CLIENT_CLIENT_LOG_CONTROL_H_

#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "IClientLogControl.h"
#include "RialtoLogging.h"

namespace firebolt::rialto::client
{
/**
 * @brief IClientLogControl factory class definition.
 */
class ClientLogControlFactory : public IClientLogControlFactory
{
public:
    ClientLogControlFactory() = default;
    ~ClientLogControlFactory() override = default;

    IClientLogControl &createClientLogControl() override;

    /**
     * @brief Create the (singleton) object.
     *
     * @retval the factory instance or null on error.
     */
    static std::shared_ptr<IClientLogControlFactory> createFactory();
};

/**
 * @brief The definition of the Control.
 */
class ClientLogControl : public IClientLogControl
{
public:
    /**
     * @brief The constructor.
     */
    ClientLogControl();

    ClientLogControl(const ClientLogControl &) = delete;
    ClientLogControl &operator=(const ClientLogControl &) = delete;

    /**
     * @brief Virtual destructor.
     */
    ~ClientLogControl() override;

    bool registerLogHandler(const std::shared_ptr<IClientLogHandler> &handler, bool ignoreLogLevels) override;

private:
    void forwardLog(RIALTO_COMPONENT component, RIALTO_DEBUG_LEVEL level, const char *file, int line,
                    const char *function, const char *message, std::size_t messageLen);
    void cancelLogHandler();

    /**
     * @brief The registered log handler
     */
    std::shared_ptr<IClientLogHandler> m_logHandler;
    std::mutex m_logHandlerMutex;
};

}; // namespace firebolt::rialto::client

#endif // FIREBOLT_RIALTO_CLIENT_CLIENT_LOG_CONTROL_H_
