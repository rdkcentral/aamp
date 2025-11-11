/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2022 Sky UK
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

#ifndef FIREBOLT_RIALTO_IPC_IPC_SERVER_IMPL_H_
#define FIREBOLT_RIALTO_IPC_IPC_SERVER_IMPL_H_

#include "FileDescriptor.h"
#include "IIpcServer.h"
#include "IIpcServerFactory.h"
#include "IpcServerControllerImpl.h"
#include "SimpleBufferPool.h"

#include "rialtoipc-transport.pb.h"

#include <sys/socket.h>

#include <atomic>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

#if !defined(SCM_MAX_FD)
#define SCM_MAX_FD 255
#endif

namespace firebolt::rialto::ipc
{
class ClientImpl;

class ServerFactory : public IServerFactory
{
public:
    ServerFactory() = default;
    ~ServerFactory() override = default;

    std::shared_ptr<IServer> create() override;
};

class ServerImpl final : public ::firebolt::rialto::ipc::IServer, public std::enable_shared_from_this<ServerImpl>
{
public:
    explicit ServerImpl();
    ~ServerImpl() final;

public:
    bool addSocket(const std::string &socketPath, std::function<void(const std::shared_ptr<IClient> &)> clientConnectedCb,
                   std::function<void(const std::shared_ptr<IClient> &)> clientDisconnectedCb) override;
    bool addSocket(int fd, std::function<void(const std::shared_ptr<IClient> &)> clientConnectedCb,
                   std::function<void(const std::shared_ptr<IClient> &)> clientDisconnectedCb) override;

    std::shared_ptr<IClient>
    addClient(int socketFd, std::function<void(const std::shared_ptr<IClient> &)> clientDisconnectedCb) override;

    int fd() const override;
    bool wait(int timeoutMSecs) override;
    bool process() override;

protected:
    friend class ClientImpl;
    bool sendEvent(uint64_t clientId, const std::shared_ptr<google::protobuf::Message> &message);
    bool isClientConnected(uint64_t clientId) const;
    void disconnectClient(uint64_t clientId);

private:
    struct Socket;
    static bool getSocketLock(Socket *socket);
    static void closeListeningSocket(Socket *socket);

    void wakeEventLoop() const;

    void processNewConnection(uint64_t socketId);

    void processClientSocket(uint64_t clientId, unsigned events);
    void processClientMessage(const std::shared_ptr<ClientImpl> &client, const uint8_t *data, size_t dataLen,
                              const std::vector<FileDescriptor> &fds = {});

    void processMethodCall(const std::shared_ptr<ClientImpl> &client, const transport::MethodCall &call,
                           const std::vector<FileDescriptor> &fds);

    std::shared_ptr<ClientImpl> addClientSocket(int socketFd, const std::string &listeningSocketPath,
                                                std::function<void(const std::shared_ptr<IClient> &)> disconnectedCb);

    void sendReply(uint64_t clientId, const std::shared_ptr<msghdr> &msg);

    void sendErrorReply(const std::shared_ptr<ClientImpl> &client, uint64_t serialId, const char *format, ...)
        __attribute__((format(printf, 4, 5)));

    void handleResponse(ServerControllerImpl *controller, google::protobuf::Message *response);

    std::shared_ptr<msghdr> populateReply(const std::shared_ptr<const ClientImpl> &client, uint64_t serialId,
                                          google::protobuf::Message *response);
    std::shared_ptr<msghdr> populateErrorReply(const std::shared_ptr<const ClientImpl> &client, uint64_t serialId,
                                               const std::string &reason);

private:
    static const size_t m_kMaxMessageLen;

    int m_pollFd;
    int m_wakeEventFd;

    std::atomic<uint64_t> m_socketIdCounter;
    std::atomic<uint64_t> m_clientIdCounter;

    struct Socket
    {
        int sockFd = -1;
        int lockFd = -1;
        std::string sockPath;
        std::string lockPath;
        std::function<void(const std::shared_ptr<IClient> &)> connectedCb;
        std::function<void(const std::shared_ptr<IClient> &)> disconnectedCb;
        bool isOwned = true;
    };

    std::mutex m_socketsLock;
    std::map<uint64_t, Socket> m_sockets;

    struct ClientDetails
    {
        int sock = -1;
        std::shared_ptr<ClientImpl> client;
        std::function<void(const std::shared_ptr<IClient> &)> disconnectedCb;
    };

    mutable std::mutex m_clientsLock;

    std::map<uint64_t, ClientDetails> m_clients;
    std::set<uint64_t> m_condemnedClients;

    uint8_t m_recvDataBuf[128 * 1024];
    uint8_t m_recvCtrlBuf[SCM_MAX_FD * sizeof(int)];

    SimpleBufferPool m_sendBufPool;
};

} // namespace firebolt::rialto::ipc

#endif // FIREBOLT_RIALTO_IPC_IPC_SERVER_IMPL_H_
