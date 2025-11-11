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

#ifndef FIREBOLT_RIALTO_SERVER_IPC_IPC_FACTORY_H_
#define FIREBOLT_RIALTO_SERVER_IPC_IPC_FACTORY_H_

#include "IIpcFactory.h"
#include <memory>

namespace firebolt::rialto::server::ipc
{
class IpcFactory : public IIpcFactory
{
public:
    IpcFactory() = default;
    ~IpcFactory() override = default;
    IpcFactory(const IpcFactory &) = delete;
    IpcFactory(IpcFactory &&) = delete;
    IpcFactory &operator=(const IpcFactory &) = delete;
    IpcFactory &operator=(IpcFactory &&) = delete;

    std::unique_ptr<IApplicationManagementServer>
    createApplicationManagementServer(service::ISessionServerManager &sessionServerManager) const override;
    std::unique_ptr<ISessionManagementServer>
    createSessionManagementServer(service::IPlaybackService &playbackService, service::ICdmService &cdmService,
                                  service::IControlService &controlService) const override;
};
} // namespace firebolt::rialto::server::ipc

#endif // FIREBOLT_RIALTO_SERVER_IPC_IPC_FACTORY_H_
