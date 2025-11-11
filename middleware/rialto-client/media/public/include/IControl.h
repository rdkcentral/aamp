/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2022 
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

#ifndef FIREBOLT_RIALTO_I_CONTROL_H_
#define FIREBOLT_RIALTO_I_CONTROL_H_

/**
 * @file IControl.h
 *
 * The definition of the IControl interface.
 *
 * This interface defines the public API of Rialto for control of Rialto,
 * including the IPC connection and shared memory.
 */

#include <memory>
#include <stdint.h>

#include "ControlCommon.h"
#include "IControlClient.h"

namespace firebolt::rialto
{
class IControl;

/**
 * @brief IControl factory class, returns a concrete implementation of IControl
 */
class IControlFactory
{
public:
    IControlFactory() = default;
    virtual ~IControlFactory() = default;

    /**
     * @brief Creates a IControlFactory instance.
     *
     * @retval the factory instance or null on error.
     */
    static std::shared_ptr<IControlFactory> createFactory();

    /**
     * @brief IControl factory method, returns a concrete implementation of IControl
     *
     * @retval the new IControl instance or null on error.
     */
    virtual std::shared_ptr<IControl> createControl() const = 0;
};

/**
 * @brief The definition of the IControl interface.
 *
 * This interface defines the public API for control of the ipc and shared memory.
 *
 */
class IControl
{
public:
    IControl() = default;
    virtual ~IControl() = default;

    IControl(const IControl &) = delete;
    IControl &operator=(const IControl &) = delete;
    IControl(IControl &&) = delete;
    IControl &operator=(IControl &&) = delete;

    /**
     * @brief Register new IControlClient. This method will hold a shared_ptr
     *        to the client until the destruction of the IControl object.
     *        At destruction the client will be unregistered and the
     *        shared_ptr will be released.
     *
     * @param[in]  client   : Client object for callbacks
     * @param[out] appState : Current application state
     *
     * @retval true on success, false otherwise.
     */
    virtual bool registerClient(std::weak_ptr<IControlClient> client, ApplicationState &appState) = 0;
};

}; // namespace firebolt::rialto

#endif // FIREBOLT_RIALTO_I_CONTROL_H_
