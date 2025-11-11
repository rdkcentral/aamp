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

#ifndef FIREBOLT_RIALTO_SERVER_IPC_I_CONTROL_MODULE_SERVICE_H_
#define FIREBOLT_RIALTO_SERVER_IPC_I_CONTROL_MODULE_SERVICE_H_

#include "IControlService.h"
#include "IPlaybackService.h"
#include "controlmodule.pb.h"
#include <IIpcServer.h>
#include <memory>

namespace firebolt::rialto::server::ipc
{
class IControlModuleService;

/**
 * @brief IControlModuleService factory class, returns a concrete implementation of IControlModuleService
 */
class IControlModuleServiceFactory
{
public:
    IControlModuleServiceFactory() = default;
    virtual ~IControlModuleServiceFactory() = default;

    /**
     * @brief Create a IControlModuleServiceFactory instance.
     *
     * @retval the factory instance or null on error.
     */
    static std::shared_ptr<IControlModuleServiceFactory> createFactory();

    /**
     * @brief Creates a ControlModuleService object.
     *
     * @retval the control ipc instance or null on error.
     */
    virtual std::shared_ptr<IControlModuleService> create(service::IPlaybackService &playbackService,
                                                          service::IControlService &controlService) const = 0;
};

/**
 * @brief The definition of the IControlModuleService interface.
 */
class IControlModuleService : public ::firebolt::rialto::ControlModule,
                              public std::enable_shared_from_this<IControlModuleService>
{
public:
    IControlModuleService() = default;
    virtual ~IControlModuleService() = default;

    IControlModuleService(const IControlModuleService &) = delete;
    IControlModuleService(IControlModuleService &&) = delete;
    IControlModuleService &operator=(const IControlModuleService &) = delete;
    IControlModuleService &operator=(IControlModuleService &&) = delete;

    /**
     * @brief Connect to the ipc client.
     *
     * @param[in] ipcClient : The ipc client to connect to.
     */
    virtual void clientConnected(const std::shared_ptr<::firebolt::rialto::ipc::IClient> &ipcClient) = 0;

    /**
     * @brief Disconnect from the ipc client.
     *
     * @param[in] ipcClient : The ipc client to disconnect to.
     */
    virtual void clientDisconnected(const std::shared_ptr<::firebolt::rialto::ipc::IClient> &ipcClient) = 0;
};

} // namespace firebolt::rialto::server::ipc

#endif // FIREBOLT_RIALTO_SERVER_IPC_I_CONTROL_MODULE_SERVICE_H_
