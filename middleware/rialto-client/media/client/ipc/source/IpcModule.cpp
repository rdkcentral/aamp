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

#include "IpcModule.h"
#include "RialtoClientLogging.h"

namespace firebolt::rialto::client
{
IpcModule::IpcModule(IIpcClient &ipcClient) : m_ipc{ipcClient}
{
    RIALTO_CLIENT_LOG_DEBUG("entry:");
}

IpcModule::~IpcModule()
{
    RIALTO_CLIENT_LOG_DEBUG("entry:");
}

bool IpcModule::unsubscribeFromAllEvents(const std::shared_ptr<ipc::IChannel> &ipcChannel)
{
    if (!ipcChannel)
    {
        return false;
    }

    bool result = true;
    for (auto it = m_eventTags.begin(); it != m_eventTags.end(); it++)
    {
        if (!ipcChannel->unsubscribe(*it))
        {
            result = false;
        }
    }
    m_eventTags.clear();

    return result;
}

bool IpcModule::attachChannel()
{
    // get the channel
    std::shared_ptr<ipc::IChannel> ipcChannel{getConnectedChannel()};

    // If not connected, try to recover and reconnect first
    if (!ipcChannel)
    {
        RIALTO_CLIENT_LOG_WARN("Channel not connected. Trying to recover...");
        if (!m_ipc.reconnect())
        {
            RIALTO_CLIENT_LOG_ERROR("Reconnection failed.");
            return false;
        }

        ipcChannel = getConnectedChannel();
        if (!ipcChannel)
        {
            RIALTO_CLIENT_LOG_ERROR("Failed to get the ipc channel");
            return false;
        }
    }

    // create the RPC stubs
    if (!createRpcStubs(ipcChannel))
    {
        RIALTO_CLIENT_LOG_ERROR("Could not create the ipc module stubs");
        return false;
    }

    // install listeners
    if (!subscribeToEvents(ipcChannel))
    {
        RIALTO_CLIENT_LOG_ERROR("Could not subscribe to ipc module events");
        unsubscribeFromAllEvents(ipcChannel);
        return false;
    }

    m_ipcChannel = ipcChannel;

    return true;
}

void IpcModule::detachChannel()
{
    // uninstalls listeners
    if (!unsubscribeFromAllEvents(m_ipcChannel.lock()))
    {
        RIALTO_CLIENT_LOG_ERROR("Failed to unsubscribe to some ipc module events, this can lead to core dumps in IPC");
    }

    // remove IPC
    m_ipcChannel.reset();
}

bool IpcModule::reattachChannelIfRequired()
{
    std::shared_ptr<ipc::IChannel> ipcChannel = m_ipcChannel.lock();
    if ((nullptr == ipcChannel) || (!ipcChannel->isConnected()))
    {
        RIALTO_CLIENT_LOG_INFO("Ipc channel no longer connected, attach new channel");
        detachChannel();
        if (!attachChannel())
        {
            RIALTO_CLIENT_LOG_ERROR("Failed attach to the ipc channel");
            return false;
        }
    }

    return true;
}

std::shared_ptr<ipc::IChannel> IpcModule::getConnectedChannel()
{
    std::shared_ptr<ipc::IChannel> ipcChannel = m_ipc.getChannel().lock();
    if (ipcChannel && ipcChannel->isConnected())
    {
        return ipcChannel;
    }
    return nullptr;
}
}; // namespace firebolt::rialto::client
