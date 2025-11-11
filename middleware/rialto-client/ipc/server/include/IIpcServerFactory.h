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

#ifndef FIREBOLT_RIALTO_IPC_I_IPC_SERVER_FACTORY_H_
#define FIREBOLT_RIALTO_IPC_I_IPC_SERVER_FACTORY_H_

#include "IIpcServer.h"

#include <memory>

namespace firebolt::rialto::ipc
{
class IServerFactory
{
public:
    IServerFactory() = default;
    virtual ~IServerFactory() = default;

    /**
     * @brief Create a IServerFactory instance.
     *
     * @retval the factory instance or null on error.
     */
    static std::shared_ptr<IServerFactory> createFactory();

    /**
     * @brief Create a IServer object.
     *
     * @retval the server instance or null on error.
     */
    virtual std::shared_ptr<IServer> create() = 0;
};

} // namespace firebolt::rialto::ipc

#endif // FIREBOLT_RIALTO_IPC_I_IPC_SERVER_FACTORY_H_
