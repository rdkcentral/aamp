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

#ifndef FIREBOLT_RIALTO_CLIENT_I_IPC_CLIENT_H_
#define FIREBOLT_RIALTO_CLIENT_I_IPC_CLIENT_H_

#include <memory>
#include <stdint.h>
#include <string>
#include <thread>

#include <IBlockingClosure.h>
#include <IIpcChannel.h>
#include <IIpcControllerFactory.h>

#include "IConnectionObserver.h"

namespace firebolt::rialto::client
{
class IIpcClient;

/**
 * @brief IIpcClient accessor class definition.
 */
class IIpcClientAccessor
{
public:
    virtual ~IIpcClientAccessor() = default;
    IIpcClientAccessor(const IIpcClientAccessor &) = delete;
    IIpcClientAccessor &operator=(const IIpcClientAccessor &) = delete;
    IIpcClientAccessor(IIpcClientAccessor &&) = delete;
    IIpcClientAccessor &operator=(IIpcClientAccessor &&) = delete;

    /**
     * @brief Get a IControlIpcAccessor instance.
     *
     * @retval the accessor instance
     */
    static IIpcClientAccessor &instance();

    /**
     * @brief Get IpcClient object.
     *
     * @retval the reference to IpcClient singleton object
     */
    virtual IIpcClient &getIpcClient() const = 0;

protected:
    IIpcClientAccessor() = default;
};

/**
 * @brief The definition of the IIpcClient interface.
 *
 * This interface defines the control ipc APIs that are used to communicate with the Rialto server.
 */
class IIpcClient
{
public:
    IIpcClient() = default;
    virtual ~IIpcClient() = default;

    IIpcClient(const IIpcClient &) = delete;
    IIpcClient &operator=(const IIpcClient &) = delete;
    IIpcClient(IIpcClient &&) = delete;
    IIpcClient &operator=(IIpcClient &&) = delete;

    /**
     * @brief Gets a weak ptr to the Ipc channel created by the IpcClient.
     *
     * @retval the ipc channel weak ptr.
     */
    virtual std::weak_ptr<ipc::IChannel> getChannel() const = 0;

    /**
     * @brief Create the blocking closure to be passed to the RPC stubs.
     *
     * @retval the blocking closure or null on error.
     */
    virtual std::shared_ptr<ipc::IBlockingClosure> createBlockingClosure() = 0;

    /**
     * @brief Create the rpc controller to be passed to the RPC stubs.
     *
     * @retval the rpc controller or null on error.
     */
    virtual std::shared_ptr<google::protobuf::RpcController> createRpcController() = 0;

    /**
     * @brief Reconnect channel.
     *
     * @retval true on success.
     */
    virtual bool reconnect() = 0;

    /**
     * @brief Registers new connection observer.
     */
    virtual void registerConnectionObserver(const std::weak_ptr<IConnectionObserver> &observer) = 0;
};

}; // namespace firebolt::rialto::client

#endif // FIREBOLT_RIALTO_CLIENT_I_IPC_CLIENT_H_
