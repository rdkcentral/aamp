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

#ifndef FIREBOLT_RIALTO_CLIENT_IPC_MODULE_H_
#define FIREBOLT_RIALTO_CLIENT_IPC_MODULE_H_

#include "IIpcClient.h"
#include <memory>
#include <stdint.h>
#include <string>
#include <thread>
#include <vector>

namespace firebolt::rialto::client
{
/**
 * @brief The implementation of a IpcModule.
 *
 * This class defines common Ipc APIs for Ipc modules.
 */
class IpcModule
{
public:
    /**
     * @brief The constructor.
     *
     * @param[in] ipcClient             : The ipc client
     */
    explicit IpcModule(IIpcClient &ipcClient);

    /**
     * @brief Virtual destructor.
     */
    virtual ~IpcModule();

protected:
    /**
     * @brief The ipc client singleton object.
     */
    IIpcClient &m_ipc;

    /**
     * @brief The ipc communication channel.
     */
    std::weak_ptr<ipc::IChannel> m_ipcChannel;

    /**
     * @brief Subscribed event tags.
     */
    std::vector<int> m_eventTags;

    /**
     * @brief Create the Rpc stubs for the derived object.
     *
     * @param[in] ipcChannel      : The ipc channel
     *
     * @retval true if the rpc stubs are created successfully, false otherwise.
     */
    virtual bool createRpcStubs(const std::shared_ptr<ipc::IChannel> &ipcChannel) = 0;

    /**
     * @brief Subscribes to the Ipc events for the derived object.
     *
     * @param[in] ipcChannel      : The ipc channel
     *
     * @retval true if the events are subscribed successfully, false otherwise.
     */
    virtual bool subscribeToEvents(const std::shared_ptr<ipc::IChannel> &ipcChannel) = 0;

    /**
     * @brief Unsubscribes to all Ipc events.
     *
     * @param[in] ipcChannel      : The ipc channel
     *
     * @retval true if the events are unsubscribed successfully, false otherwise.
     */
    bool unsubscribeFromAllEvents(const std::shared_ptr<ipc::IChannel> &ipcChannel);

    /**
     * @brief Attach the connected ipc client channel to the MediaKeysIpc object.
     *
     * @retval true if the channel attached successfully, false otherwise.
     */
    bool attachChannel();

    /**
     * @brief Detach the ipc client channel from the MediaKeysIpc object.
     */
    void detachChannel();

    /**
     * @brief If the channel is disconnected, this method will reattach the channel.
     *
     * @retval true if channel now connected, false otherwise.
     */
    bool reattachChannelIfRequired();

private:
    /**
     * @brief Get connected IPC Channel
     *
     * @retval Ipc Channel if connected, nullptr otherwise
     */
    std::shared_ptr<ipc::IChannel> getConnectedChannel();
};

}; // namespace firebolt::rialto::client

#endif // FIREBOLT_RIALTO_CLIENT_IPC_MODULE_H_
