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

#ifndef FIREBOLT_RIALTO_CLIENT_CONTROL_H_
#define FIREBOLT_RIALTO_CLIENT_CONTROL_H_

#include <memory>
#include <string>
#include <vector>

#include "IClientController.h"
#include "IControl.h"
#include "IControlClient.h"
#include "RialtoLogging.h"

namespace firebolt::rialto::client
{
/**
 * @brief IControl factory class definition.
 */
class ControlFactory : public IControlFactory
{
public:
    ControlFactory() = default;
    ~ControlFactory() override = default;

    std::shared_ptr<IControl> createControl() const override;
    /**
     * @brief Create the control factory object.
     *
     * @retval the control factory instance or null on error.
     */
    static std::shared_ptr<ControlFactory> createFactory();
};

/**
 * @brief The definition of the Control.
 */
class Control : public IControl
{
public:
    /**
     * @brief The constructor.
     */
    explicit Control(IClientController &clientController);

    /**
     * @brief Virtual destructor.
     */
    ~Control() override;

    bool registerClient(std::weak_ptr<IControlClient> client, ApplicationState &appState) override;

private:
    /**
     * @brief The control clients which have been registered with m_clientController
     *        and are kept so that they can be unregistered upon destruction of this
     *        object
     */
    std::vector<std::shared_ptr<IControlClient>> m_clientsToUnregister;

    /**
     * @brief The rialto client controller object.
     */
    IClientController &m_clientController;
};

}; // namespace firebolt::rialto::client

#endif // FIREBOLT_RIALTO_CLIENT_CONTROL_H_
