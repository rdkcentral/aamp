/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2023 
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

#ifndef FIREBOLT_RIALTO_CLIENT_IPC_CLIENT_H_
#define FIREBOLT_RIALTO_CLIENT_IPC_CLIENT_H_

#include "IIpcClient.h"
#include <atomic>
#include <memory>

namespace firebolt::rialto::client
{
class IpcClientAccessor : public IIpcClientAccessor
{
public:
    ~IpcClientAccessor() override = default;
    IIpcClient &getIpcClient() const override;
};

/**
 * @brief The definition of the IpcClient.
 */
class IpcClient : public IIpcClient
{
public:
    /**
     * @brief The constructor.
     */
    IpcClient(const std::shared_ptr<ipc::IChannelFactory> &ipcChannelFactory,
              const std::shared_ptr<ipc::IControllerFactory> &ipcControllerFactory,
              const std::shared_ptr<ipc::IBlockingClosureFactory> &blockingClosureFactory);

    /**
     * @brief Virtual destructor.
     */
    ~IpcClient() override;

    std::weak_ptr<ipc::IChannel> getChannel() const override;

    std::shared_ptr<ipc::IBlockingClosure> createBlockingClosure() override;

    std::shared_ptr<google::protobuf::RpcController> createRpcController() override;

    bool reconnect() override;

    void registerConnectionObserver(const std::weak_ptr<IConnectionObserver> &observer) override;

protected:
    /**
     * @brief The ipc thread.
     */
    std::thread m_ipcThread;

    /**
     * @brief The connected ipc communication channel.
     */
    std::shared_ptr<ipc::IChannel> m_ipcChannel;

    /**
     * @brief Factory for creating the ipc controllers.
     */
    std::shared_ptr<ipc::IControllerFactory> m_ipcControllerFactory;

    /**
     * @brief Factory for creating a connected ipc channel.
     */
    std::shared_ptr<ipc::IChannelFactory> m_ipcChannelFactory;

    /**
     * @brief Factory for creating a blocking closure.
     */
    std::shared_ptr<ipc::IBlockingClosureFactory> m_blockingClosureFactory;

    /**
     * @brief Whether disconnection of ipc has been requested by the client and is ongoing.
     */
    std::atomic<bool> m_disconnecting;

    /**
     * @brief Current connection status observer
     */
    std::weak_ptr<IConnectionObserver> m_connectionObserver;

    /**
     * @brief The processing loop for the ipc thread.
     */
    void processIpcThread();

    /**
     * @brief Establish connection between Rialto Server and Rialto Client
     */
    bool connect();

    /**
     * @brief Close connection between Rialto Server and Rialto Client
     */
    bool disconnect();
};
} // namespace firebolt::rialto::client

#endif // FIREBOLT_RIALTO_CLIENT_IPC_CLIENT_H_
