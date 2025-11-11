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

#include "IpcClient.h"
#include "RialtoClientLogging.h"

namespace firebolt::rialto::client
{
IIpcClientAccessor &IIpcClientAccessor::instance()
{
    static IpcClientAccessor factory;
    return factory;
}

IIpcClient &IpcClientAccessor::getIpcClient() const
{
    static IpcClient ipcClient{ipc::IChannelFactory::createFactory(), ipc::IControllerFactory::createFactory(),
                               ipc::IBlockingClosureFactory::createFactory()};
    return ipcClient;
}

IpcClient::IpcClient(const std::shared_ptr<ipc::IChannelFactory> &ipcChannelFactory,
                     const std::shared_ptr<ipc::IControllerFactory> &ipcControllerFactory,
                     const std::shared_ptr<ipc::IBlockingClosureFactory> &blockingClosureFactory)
    : m_ipcControllerFactory(ipcControllerFactory), m_ipcChannelFactory(ipcChannelFactory),
      m_blockingClosureFactory(blockingClosureFactory), m_disconnecting(false)
{
    // For now, always connect the client on construction
    if (!connect())
    {
        throw std::runtime_error("Could not connect client");
    }
}

IpcClient::~IpcClient()
{
    if (!disconnect())
    {
        RIALTO_CLIENT_LOG_WARN("Could not disconnect client");
    }
}

bool IpcClient::connect()
{
    if (m_ipcChannel)
    {
        RIALTO_CLIENT_LOG_INFO("Client already connected");
        return true;
    }

    // Verify that the version of the library that we linked against is
    // compatible with the version of the headers we compiled against.
    GOOGLE_PROTOBUF_VERIFY_VERSION;

    // check if either of following env vars are set to determine the location of the rialto socket
    //  - RIALTO_SOCKET_PATH should specify the absolute path to the socket to connect to
    //  - RIALTO_SOCKET_FD should specify the number of a file descriptor of the socket to connect to
    const char *kRialtoPath = getenv("RIALTO_SOCKET_PATH");
    const char *kRialtoFd = getenv("RIALTO_SOCKET_FD");
    if (kRialtoFd)
    {
        char *end = nullptr;
        int fd = strtol(kRialtoFd, &end, 10);
        if ((errno != 0) || (kRialtoFd == end) || (*end != '\0'))
        {
            RIALTO_CLIENT_LOG_SYS_ERROR(errno, "Invalid value set in RIALTO_SOCKET_FD env var");
            return false;
        }

        m_ipcChannel = m_ipcChannelFactory->createChannel(fd);
    }
    else if (kRialtoPath)
    {
        m_ipcChannel = m_ipcChannelFactory->createChannel(kRialtoPath);
    }
    else
    {
        RIALTO_CLIENT_LOG_ERROR("No rialto socket specified");
        return false;
    }

    // check if the channel was opened
    if (!m_ipcChannel)
    {
        RIALTO_CLIENT_LOG_ERROR("Failed to open a connection to the ipc socket");
        return false;
    }

    // spin up the thread that runs the IPC event loop
    m_ipcThread = std::thread(&IpcClient::processIpcThread, this);
    if (!m_ipcThread.joinable())
    {
        RIALTO_CLIENT_LOG_ERROR("Failed to create thread for IPC");
        return false;
    }

    return true;
}

bool IpcClient::disconnect()
{
    // Increase reference in case client disconnects from another thread
    std::shared_ptr<ipc::IChannel> ipcChannel = m_ipcChannel;
    if (!ipcChannel)
    {
        // The ipc channel may have disconnected unexpectedly, join the ipc thread if possible
        if (m_ipcThread.joinable())
            m_ipcThread.join();

        RIALTO_CLIENT_LOG_INFO("Client already disconnect");
        return true;
    }

    RIALTO_CLIENT_LOG_INFO("closing IPC channel");
    m_disconnecting = true;

    // disconnect from the server, this should terminate the thread so join that too
    ipcChannel->disconnect();

    if (m_ipcThread.joinable())
        m_ipcThread.join();

    // destroy the IPC channel
    m_ipcChannel.reset();

    m_disconnecting = false;

    return true;
}

void IpcClient::processIpcThread()
{
    pthread_setname_np(pthread_self(), "rialto-ipc");

    RIALTO_CLIENT_LOG_INFO("started ipc thread");

    while (m_ipcChannel->process())
    {
        m_ipcChannel->wait(-1);
    }

    if (!m_disconnecting)
    {
        RIALTO_CLIENT_LOG_ERROR("The ipc channel unexpectedly disconnected, destroying the channel");

        // Safe to destroy the ipc objects in the ipc thread as the client has already disconnected.
        // This ensures the channel is destructed and that all ongoing ipc calls are unblocked.
        m_ipcChannel.reset();

        auto connectionObserver{m_connectionObserver.lock()};
        if (connectionObserver)
        {
            connectionObserver->onConnectionBroken();
        }
    }

    RIALTO_CLIENT_LOG_INFO("exiting ipc thread");
}

std::weak_ptr<::firebolt::rialto::ipc::IChannel> IpcClient::getChannel() const
{
    return m_ipcChannel;
}

std::shared_ptr<ipc::IBlockingClosure> IpcClient::createBlockingClosure()
{
    // Increase reference in case client disconnects from another thread
    std::shared_ptr<ipc::IChannel> ipcChannel = m_ipcChannel;
    if (!ipcChannel)
    {
        RIALTO_CLIENT_LOG_ERROR("ipc channel not connected");
        return nullptr;
    }

    // check which thread we're being called from, this determines if we pump
    // event loop from within the wait() method or not
    if (m_ipcThread.get_id() == std::this_thread::get_id())
        return m_blockingClosureFactory->createBlockingClosurePoll(ipcChannel);
    else
        return m_blockingClosureFactory->createBlockingClosureSemaphore();
}

std::shared_ptr<google::protobuf::RpcController> IpcClient::createRpcController()
{
    return m_ipcControllerFactory->create();
}

bool IpcClient::reconnect()
{
    RIALTO_CLIENT_LOG_INFO("Trying to reconnect channel");
    if (disconnect())
    {
        return connect();
    }
    return false;
}

void IpcClient::registerConnectionObserver(const std::weak_ptr<IConnectionObserver> &observer)
{
    m_connectionObserver = observer;
}

}; // namespace firebolt::rialto::client
