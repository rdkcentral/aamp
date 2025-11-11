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

#ifndef FIREBOLT_RIALTO_CLIENT_CONTROL_IPC_H_
#define FIREBOLT_RIALTO_CLIENT_CONTROL_IPC_H_

#include "IConnectionObserver.h"
#include "IControlClient.h"
#include "IControlIpc.h"
#include "IEventThread.h"
#include "IpcModule.h"
#include <memory>

#include "controlmodule.pb.h"

namespace firebolt::rialto::client
{
class ControlIpc;

/**
 * @brief IControlIpc factory class definition.
 */
class ControlIpcFactory : public IControlIpcFactory
{
public:
    ControlIpcFactory() = default;
    ~ControlIpcFactory() override = default;

    std::shared_ptr<IControlIpc> createControlIpc(IControlClient *controlClient) override;

    /**
     * @brief Create the generic control factory object.
     *
     * @retval the generic control factory instance or null on error.
     */
    static std::shared_ptr<ControlIpcFactory> createFactory();
};

/**
 * @brief The definition of the ControlIpc.
 */
class ControlIpc : public IControlIpc, public IpcModule, public IConnectionObserver
{
public:
    /**
     * @brief The constructor.
     */
    ControlIpc(IControlClient *controlClient, IIpcClient &ipcClient,
               const std::shared_ptr<common::IEventThreadFactory> &eventThreadFactory);

    /**
     * @brief Virtual destructor.
     */
    virtual ~ControlIpc();

    bool getSharedMemory(int32_t &fd, uint32_t &size) override;
    bool registerClient() override;

private:
    bool createRpcStubs(const std::shared_ptr<ipc::IChannel> &ipcChannel) override;
    bool subscribeToEvents(const std::shared_ptr<ipc::IChannel> &ipcChannel) override;
    void onConnectionBroken() override;

    /**
     * @brief Handler for a application state update from the server.
     *
     * @param[in] event : The app state changed event structure.
     */
    void onApplicationStateUpdated(const std::shared_ptr<firebolt::rialto::ApplicationStateChangeEvent> &event);

    /**
     * @brief Handler for a ping from the server.
     *
     * @param[in] event : The ping event structure.
     */
    void onPing(const std::shared_ptr<firebolt::rialto::PingEvent> &event);

private:
    /**
     * @brief Control client for handling messages from server
     */
    IControlClient *m_controlClient;

    /**
     * @brief The control handle for the current instance.
     */
    std::atomic<int> m_controlHandle;

    /**
     * @brief Thread for handling media player events from the server.
     */
    std::unique_ptr<common::IEventThread> m_eventThread;

    /**
     * @brief RPC stubs for the Control APIs.
     */
    std::shared_ptr<::firebolt::rialto::ControlModule_Stub> m_controlStub;
};

}; // namespace firebolt::rialto::client

#endif // FIREBOLT_RIALTO_CLIENT_CONTROL_IPC_H_
