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

#ifndef FIREBOLT_RIALTO_SERVER_IPC_I_WEB_AUDIO_PLAYER_MODULE_SERVICE_H_
#define FIREBOLT_RIALTO_SERVER_IPC_I_WEB_AUDIO_PLAYER_MODULE_SERVICE_H_

#include "IWebAudioPlayerService.h"
#include "webaudioplayermodule.pb.h"
#include <IIpcServer.h>
#include <memory>

namespace firebolt::rialto::server::ipc
{
class IWebAudioPlayerModuleService;

/**
 * @brief IWebAudioPlayerModuleService factory class, returns a concrete implementation of IWebAudioPlayerModuleService
 */
class IWebAudioPlayerModuleServiceFactory
{
public:
    IWebAudioPlayerModuleServiceFactory() = default;
    virtual ~IWebAudioPlayerModuleServiceFactory() = default;

    /**
     * @brief Create a IWebAudioPlayerModuleServiceFactory instance.
     *
     * @retval the factory instance or null on error.
     */
    static std::shared_ptr<IWebAudioPlayerModuleServiceFactory> createFactory();

    /**
     * @brief Creates a WebAudioPlayerModuleService object.
     *
     * @retval the web audio player ipc instance or null on error.
     */
    virtual std::shared_ptr<IWebAudioPlayerModuleService>
    create(service::IWebAudioPlayerService &webAudioPlayerService) const = 0;
};

/**
 * @brief The definition of the IWebAudioPlayerModuleService interface.
 */
class IWebAudioPlayerModuleService : public ::firebolt::rialto::WebAudioPlayerModule,
                                     public std::enable_shared_from_this<IWebAudioPlayerModuleService>
{
public:
    IWebAudioPlayerModuleService() = default;
    virtual ~IWebAudioPlayerModuleService() = default;

    IWebAudioPlayerModuleService(const IWebAudioPlayerModuleService &) = delete;
    IWebAudioPlayerModuleService(IWebAudioPlayerModuleService &&) = delete;
    IWebAudioPlayerModuleService &operator=(const IWebAudioPlayerModuleService &) = delete;
    IWebAudioPlayerModuleService &operator=(IWebAudioPlayerModuleService &&) = delete;

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

#endif // FIREBOLT_RIALTO_SERVER_IPC_I_WEB_AUDIO_PLAYER_MODULE_SERVICE_H_
