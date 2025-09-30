/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
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

#ifndef FIREBOLT_RIALTO_I_CLIENT_LOG_CONTROL_H_
#define FIREBOLT_RIALTO_I_CLIENT_LOG_CONTROL_H_

/**
 * @file IClientLogControl.h
 *
 * The definition of the IClientLogControl interface.
 *
 * Via this interface the logging of the Rialto Client can
 * be redirected to a user specified call handler
 */

#include <memory>
#include <stdint.h>

#include "ControlCommon.h"
#include "IClientLogHandler.h"
#include "IControlClient.h"

namespace firebolt::rialto
{
class IClientLogControl;

/**
 * @brief IClientLogControl factory class, returns a concrete implementation of IClientLogControl
 */
class IClientLogControlFactory
{
public:
    IClientLogControlFactory() = default;
    virtual ~IClientLogControlFactory() = default;

    /**
     * @brief Creates the IClientLogControlFactory
     *
     * @retval the factory instance or null on error.
     */
    static std::shared_ptr<IClientLogControlFactory> createFactory();

    /**
     * @brief IClientLogControl factory method, returns a concrete singleton implementation of IClientLogControl
     *
     * @retval the new IClientLogControl instance or null on error.
     */
    virtual IClientLogControl &createClientLogControl() = 0;
};

/**
 * @brief The definition of the IClientLogControl interface.
 *
 * This interface defines the public API for controlling Rialto client's
 * log handling. This class is a singleton
 *
 */
class IClientLogControl
{
public:
    IClientLogControl() = default;
    virtual ~IClientLogControl() = default;

    IClientLogControl(const IClientLogControl &) = delete;
    IClientLogControl &operator=(const IClientLogControl &) = delete;
    IClientLogControl(IClientLogControl &&) = delete;
    IClientLogControl &operator=(IClientLogControl &&) = delete;

    /**
     * @brief Register a new log handler
     *
     * @param[in]  handler         : The user can pass in an object that will henceforth
     *                               receive callbacks to it's log(...) method for rialto client log events.
     *                               Please see IClientLogHandler for the parameters that log() must accept.
     *                               To cancel callbacks then call this method again but pass a nullptr for
     *                               the handler.
     * @param[in]  ignoreLogLevels : If true then the handler will receive ALL log level messages regardless of the
     *                               currently configured log level
     *
     * @retval true if successful
     */
    virtual bool registerLogHandler(const std::shared_ptr<IClientLogHandler> &handler, bool ignoreLogLevels) = 0;
};

}; // namespace firebolt::rialto

#endif // FIREBOLT_RIALTO_I_CLIENT_LOG_CONTROL_H_
