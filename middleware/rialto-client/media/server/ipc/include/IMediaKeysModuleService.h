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

#ifndef FIREBOLT_RIALTO_SERVER_IPC_I_MEDIA_KEYS_MODULE_SERVICE_H_
#define FIREBOLT_RIALTO_SERVER_IPC_I_MEDIA_KEYS_MODULE_SERVICE_H_

#include "ICdmService.h"
#include "mediakeysmodule.pb.h"
#include <IIpcServer.h>
#include <memory>

namespace firebolt::rialto::server::ipc
{
class IMediaKeysModuleService;

/**
 * @brief IMediaKeysModuleService factory class, returns a concrete implementation of IMediaKeysModuleService
 */
class IMediaKeysModuleServiceFactory
{
public:
    IMediaKeysModuleServiceFactory() = default;
    virtual ~IMediaKeysModuleServiceFactory() = default;

    /**
     * @brief Create a IMediaKeysModuleServiceFactory instance.
     *
     * @retval the factory instance or null on error.
     */
    static std::shared_ptr<IMediaKeysModuleServiceFactory> createFactory();

    /**
     * @brief Creates a MediaKeysModuleService object.
     *
     * @param[in] cdmService : The service for cdm objects.
     *
     * @retval the rialto controller ipc instance or null on error.
     */
    virtual std::shared_ptr<IMediaKeysModuleService> create(service::ICdmService &cdmService) const = 0;
};

/**
 * @brief The definition of the IMediaKeysModuleService interface.
 */
class IMediaKeysModuleService : public ::firebolt::rialto::MediaKeysModule,
                                public std::enable_shared_from_this<IMediaKeysModuleService>
{
public:
    IMediaKeysModuleService() = default;
    virtual ~IMediaKeysModuleService() = default;

    IMediaKeysModuleService(const IMediaKeysModuleService &) = delete;
    IMediaKeysModuleService(IMediaKeysModuleService &&) = delete;
    IMediaKeysModuleService &operator=(const IMediaKeysModuleService &) = delete;
    IMediaKeysModuleService &operator=(IMediaKeysModuleService &&) = delete;

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

#endif // FIREBOLT_RIALTO_SERVER_IPC_I_MEDIA_KEYS_MODULE_SERVICE_H_
