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

#include "IpcServerImpl.h"
#include "IIpcServerFactory.h"
#include "IpcClientImpl.h"
#include "IpcLogging.h"
#include "IpcServerControllerImpl.h"

#include "rialtoipc.pb.h"

#include <google/protobuf/service.h>

#include <algorithm>
#include <cinttypes>
#include <cstdarg>
#include <string>
#include <utility>
#include <vector>

#include <fcntl.h>
#include <poll.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#define WAKE_EVENT_ID uint64_t(0)
#define FIRST_LISTENING_SOCKET_ID uint64_t(1)
#define FIRST_CLIENT_ID uint64_t(10000)

namespace firebolt::rialto::ipc
{
const size_t ServerImpl::m_kMaxMessageLen = (128 * 1024);

std::shared_ptr<IServerFactory> IServerFactory::createFactory()
{
    std::shared_ptr<IServerFactory> factory;
    try
    {
        factory = std::make_shared<ServerFactory>();
    }
    catch (const std::exception &e)
    {
        RIALTO_IPC_LOG_ERROR("Failed to create the server factory, reason: %s", e.what());
    }

    return factory;
}

std::shared_ptr<IServer> ServerFactory::create()
{
    return std::make_shared<ServerImpl>();
}

ServerImpl::ServerImpl()
    : m_pollFd(-1), m_wakeEventFd(-1), m_socketIdCounter(FIRST_LISTENING_SOCKET_ID), m_clientIdCounter(FIRST_CLIENT_ID),
      m_recvDataBuf{0}, m_recvCtrlBuf{0}
{
    // create the eventfd use to wake the poll loop
    m_wakeEventFd = eventfd(0, EFD_CLOEXEC);
    if (m_wakeEventFd < 0)
    {
        RIALTO_IPC_LOG_SYS_ERROR(errno, "eventfd failed");
        return;
    }

    // create epoll loop
    m_pollFd = epoll_create1(EPOLL_CLOEXEC);
    if (m_pollFd < 0)
    {
        RIALTO_IPC_LOG_SYS_ERROR(errno, "epoll_create1 failed");
        return;
    }

    // add the wake event to epoll
    epoll_event event = {.events = EPOLLIN, .data = {.u64 = WAKE_EVENT_ID}};
    if (epoll_ctl(m_pollFd, EPOLL_CTL_ADD, m_wakeEventFd, &event) != 0)
    {
        RIALTO_IPC_LOG_SYS_ERROR(errno, "epoll_ctl failed to add eventfd");
    }
}

ServerImpl::~ServerImpl()
{
    if ((m_pollFd >= 0) && (close(m_pollFd) != 0))
        RIALTO_IPC_LOG_SYS_ERROR(errno, "failed to close epoll");

    if ((m_wakeEventFd >= 0) && (close(m_wakeEventFd) != 0))
        RIALTO_IPC_LOG_SYS_ERROR(errno, "failed to close eventfd");

    for (const auto &entry : m_sockets)
    {
        const Socket &kSocket = entry.second;

        if (kSocket.isOwned)
        {
            if (unlink(kSocket.sockPath.c_str()) != 0)
                RIALTO_IPC_LOG_SYS_ERROR(errno, "failed to remove socket @ '%s'", kSocket.sockPath.c_str());
            if (close(kSocket.sockFd) != 0)
                RIALTO_IPC_LOG_SYS_ERROR(errno, "failed to close listening socket");

            if (unlink(kSocket.lockPath.c_str()) != 0)
                RIALTO_IPC_LOG_SYS_ERROR(errno, "failed to remove socket lock file @ '%s'", kSocket.lockPath.c_str());
            if (close(kSocket.lockFd) != 0)
                RIALTO_IPC_LOG_SYS_ERROR(errno, "failed to close socket lock file");
        }
    }
}

// -----------------------------------------------------------------------------
/*!
    \static
    \internal

    Creates (if required) and takes the file lock associated with the socket
    path in the \a socket object.

 */
bool ServerImpl::getSocketLock(Socket *socket)
{
    std::string lockPath = socket->sockPath + ".lock";
    int fileDescriptor = open(lockPath.c_str(), O_CREAT | O_CLOEXEC, (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP));
    if (fileDescriptor < 0)
    {
        RIALTO_IPC_LOG_SYS_ERROR(errno, "failed to create / open lockfile @ '%s' (check permissions)", lockPath.c_str());
        return false;
    }

    if (flock(fileDescriptor, LOCK_EX | LOCK_NB) < 0)
    {
        RIALTO_IPC_LOG_SYS_ERROR(errno, "failed to lock lockfile @ '%s', maybe another server is running",
                                 lockPath.c_str());
        close(fileDescriptor);
        return false;
    }

    struct stat sbuf = {0};
    if (stat(socket->sockPath.c_str(), &sbuf) < 0)
    {
        if (errno != ENOENT)
        {
            RIALTO_IPC_LOG_SYS_ERROR(errno, "did not manage to stat existing socket @ '%s'", socket->sockPath.c_str());
            close(fileDescriptor);
            return false;
        }
    }
    else if ((sbuf.st_mode & S_IWUSR) || (sbuf.st_mode & S_IWGRP))
    {
        unlink(socket->sockPath.c_str());
    }

    socket->lockFd = fileDescriptor;
    socket->lockPath = std::move(lockPath);

    return true;
}

// -----------------------------------------------------------------------------
/*!
    \static
    \internal

    Closes the file descriptors and the deletes the files stored in the \a socket
    object.

 */
void ServerImpl::closeListeningSocket(Socket *socket)
{
    if (!socket->isOwned)
    {
        return;
    }

    if (!socket->sockPath.empty() && (unlink(socket->sockPath.c_str()) != 0) && (errno != ENOENT))
        RIALTO_IPC_LOG_SYS_ERROR(errno, "failed to remove socket @ '%s'", socket->sockPath.c_str());
    if ((socket->sockFd >= 0) && (close(socket->sockFd) != 0))
        RIALTO_IPC_LOG_SYS_ERROR(errno, "failed to close listening socket");

    if (!socket->lockPath.empty() && (unlink(socket->lockPath.c_str()) != 0) && (errno != ENOENT))
        RIALTO_IPC_LOG_SYS_ERROR(errno, "failed to remove socket lock file @ '%s'", socket->lockPath.c_str());
    if ((socket->lockFd >= 0) && (close(socket->lockFd) != 0))
        RIALTO_IPC_LOG_SYS_ERROR(errno, "failed to close socket lock file");

    socket->sockFd = -1;
    socket->sockPath.clear();

    socket->lockFd = -1;
    socket->lockPath.clear();
}

bool ServerImpl::addSocket(const std::string &socketPath,
                           std::function<void(const std::shared_ptr<IClient> &)> clientConnectedCb,
                           std::function<void(const std::shared_ptr<IClient> &)> clientDisconnectedCb)
{
    // store the path
    Socket socket;
    socket.sockPath = socketPath;

    // create the socket
    socket.sockFd = ::socket(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
    if (socket.sockFd == -1)
    {
        RIALTO_IPC_LOG_SYS_ERROR(errno, "socket error");
        return false;
    }

    // get the socket lock
    if (!getSocketLock(&socket))
    {
        closeListeningSocket(&socket);
        return false;
    }

    // bind to the given path
    struct sockaddr_un addr = {0};
    memset(&addr, 0x00, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socketPath.c_str(), sizeof(addr.sun_path) - 1);

    if (bind(socket.sockFd, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) == -1)
    {
        RIALTO_IPC_LOG_SYS_ERROR(errno, "bind error");

        closeListeningSocket(&socket);
        return false;
    }

    // put in listening mode
    if (listen(socket.sockFd, 1) == -1)
    {
        RIALTO_IPC_LOG_SYS_ERROR(errno, "listen error");

        closeListeningSocket(&socket);
        return false;
    }

    // create an id for the listening socket
    const uint64_t kSocketId = m_socketIdCounter++;
    if (kSocketId >= FIRST_CLIENT_ID)
    {
        // should never happen, we'd run out of file descriptors before
        // we hit the 10k limit on listening sockets
        RIALTO_IPC_LOG_ERROR("too many listening sockets");

        closeListeningSocket(&socket);
        return false;
    }

    // add the socket to epoll
    epoll_event event = {.events = EPOLLIN, .data = {.u64 = kSocketId}};
    if (epoll_ctl(m_pollFd, EPOLL_CTL_ADD, socket.sockFd, &event) != 0)
    {
        RIALTO_IPC_LOG_SYS_ERROR(errno, "epoll_ctl failed to add listening socket");

        closeListeningSocket(&socket);
        return false;
    }

    // store the client connected / disconnected callbacks
    socket.connectedCb = clientConnectedCb;
    socket.disconnectedCb = clientDisconnectedCb;

    // add to the internal map
    std::lock_guard<std::mutex> locker(m_socketsLock);
    m_sockets.emplace(kSocketId, std::move(socket));

    RIALTO_IPC_LOG_INFO("added listening socket '%s' to server", socketPath.c_str());

    return true;
}

bool ServerImpl::addSocket(int fd, std::function<void(const std::shared_ptr<IClient> &)> clientConnectedCb,
                           std::function<void(const std::shared_ptr<IClient> &)> clientDisconnectedCb)
{
    // store the path
    Socket socket;
    socket.isOwned = false;
    socket.sockFd = fd;

    // put in listening mode
    if (listen(socket.sockFd, 1) == -1)
    {
        RIALTO_IPC_LOG_SYS_ERROR(errno, "listen error");
        return false;
    }

    // create an id for the listening socket
    const uint64_t kSocketId = m_socketIdCounter++;
    if (kSocketId >= FIRST_CLIENT_ID)
    {
        // should never happen, we'd run out of file descriptors before
        // we hit the 10k limit on listening sockets
        RIALTO_IPC_LOG_ERROR("too many listening sockets");
        return false;
    }

    // add the socket to epoll
    epoll_event event = {.events = EPOLLIN, .data = {.u64 = kSocketId}};
    if (epoll_ctl(m_pollFd, EPOLL_CTL_ADD, socket.sockFd, &event) != 0)
    {
        RIALTO_IPC_LOG_SYS_ERROR(errno, "epoll_ctl failed to add listening socket");
        return false;
    }

    // store the client connected / disconnected callbacks
    socket.connectedCb = clientConnectedCb;
    socket.disconnectedCb = clientDisconnectedCb;

    // add to the internal map
    std::lock_guard<std::mutex> locker(m_socketsLock);
    m_sockets.emplace(kSocketId, std::move(socket));

    RIALTO_IPC_LOG_INFO("added listening socket with fd: %d to server", fd);

    return true;
}

std::shared_ptr<IClient> ServerImpl::addClient(int socketFd,
                                               std::function<void(const std::shared_ptr<IClient> &)> clientDisconnectedCb)
{
    // sanity check the supplied socket is of the right type
    struct sockaddr addr;
    socklen_t len = sizeof(sockaddr);
    if ((getsockname(socketFd, &addr, &len) < 0) || (len < sizeof(sa_family_t)))
    {
        RIALTO_IPC_LOG_SYS_ERROR(errno, "failed to get name of supplied socket");
        return nullptr;
    }
    if (addr.sa_family != AF_UNIX)
    {
        RIALTO_IPC_LOG_ERROR("supplied client socket is not a unix domain socket");
        return nullptr;
    }

    int type = 0;
    len = sizeof(type);
    if ((getsockopt(socketFd, SOL_SOCKET, SO_TYPE, &type, &len) < 0) || (len != sizeof(type)))
    {
        RIALTO_IPC_LOG_SYS_ERROR(errno, "failed to get type of supplied socket");
        return nullptr;
    }
    if (type != SOCK_SEQPACKET)
    {
        RIALTO_IPC_LOG_ERROR("supplied client socket is not of type SOCK_SEQPACKET");
        return nullptr;
    }

    // dup the socket and set the O_CLOEXEC bit
    int duppedFd = fcntl(socketFd, F_DUPFD_CLOEXEC, 3);
    if (duppedFd < 0)
    {
        RIALTO_IPC_LOG_SYS_ERROR(errno, "failed to get type of supplied socket");
        return nullptr;
    }

    // ensure the SOCK_NONBLOCK flag is set
    int flags = fcntl(duppedFd, F_GETFL);
    if ((flags < 0) || (fcntl(duppedFd, F_SETFL, flags | O_NONBLOCK) < 0))
    {
        RIALTO_IPC_LOG_SYS_ERROR(errno, "failed to set socket to non-blocking mode");
        close(duppedFd);
        return nullptr;
    }

    // finally, add the socket to the list of clients
    auto client = addClientSocket(duppedFd, "", std::move(clientDisconnectedCb));
    if (!client)
    {
        close(duppedFd);
    }

    return client;
}

int ServerImpl::fd() const
{
    return m_pollFd;
}

// -----------------------------------------------------------------------------
/*!
    \internal

    Writes to the eventfd to wake the event loop.  Typically called when requesting
    it to shutdown or external code has requested that a client be disconnected.

 */
void ServerImpl::wakeEventLoop() const
{
    if (m_wakeEventFd < 0)
    {
        RIALTO_IPC_LOG_ERROR("invalid wake event fd");
    }
    else
    {
        uint64_t value = 1;
        if (TEMP_FAILURE_RETRY(::write(m_wakeEventFd, &value, sizeof(value))) != sizeof(value))
        {
            RIALTO_IPC_LOG_SYS_ERROR(errno, "failed to write to the event fd");
        }
    }
}

bool ServerImpl::wait(int timeoutMSecs)
{
    if (m_pollFd < 0)
    {
        return false;
    }

    // wait for any event (with timeout)
    struct pollfd fds[2];
    fds[0].fd = m_pollFd;
    fds[0].events = POLLIN;

    int rc = TEMP_FAILURE_RETRY(poll(fds, 1, timeoutMSecs));
    if (rc < 0)
    {
        RIALTO_IPC_LOG_SYS_ERROR(errno, "poll failed?");
        return false;
    }

    return true;
}

bool ServerImpl::process()
{
    if (m_pollFd < 0)
    {
        RIALTO_IPC_LOG_ERROR("missing epoll");
        return false;
    }

    // read up to 32 events
    const int kMaxEvents = 32;
    struct epoll_event events[kMaxEvents];

    int rc = TEMP_FAILURE_RETRY(epoll_wait(m_pollFd, events, kMaxEvents, 0));
    if (rc < 0)
    {
        RIALTO_IPC_LOG_SYS_ERROR(errno, "epoll_wait failed");
        return false;
    }

    // process the events (maybe 0 if timed out)
    for (int i = 0; i < rc; i++)
    {
        const struct epoll_event &kEvent = events[i];

        // check if a wake event, in which case just clear the eventfd
        if (kEvent.data.u64 == WAKE_EVENT_ID)
        {
            uint64_t ignore;
            if (TEMP_FAILURE_RETRY(::read(m_wakeEventFd, &ignore, sizeof(ignore))) != sizeof(ignore))
            {
                RIALTO_IPC_LOG_SYS_ERROR(errno, "failed to read wake eventfd");
            }
        }

        // check for events on the listening socket
        else if (kEvent.data.u64 < FIRST_CLIENT_ID)
        {
            if (kEvent.events & EPOLLIN)
                processNewConnection(kEvent.data.u64);
            if (kEvent.events & EPOLLERR)
                RIALTO_IPC_LOG_ERROR("error occurred on listening socket");
        }

        // otherwise, the event must have come from a socket
        else
        {
            processClientSocket(kEvent.data.u64, kEvent.events);
        }
    }

    // if we have client sockets that are condemned then we need to shut down
    // and close them as well as remove from epoll
    std::unique_lock<std::mutex> locker(m_clientsLock);
    if (!m_condemnedClients.empty())
    {
        // take a copy of the set so we can process without the lock held
        std::set<uint64_t> theCondemned;
        m_condemnedClients.swap(theCondemned);

        for (uint64_t clientId : theCondemned)
        {
            auto it = m_clients.find(clientId);
            if (it == m_clients.end())
            {
                RIALTO_IPC_LOG_ERROR("failed to find condemned client");
                continue;
            }

            ClientDetails details = it->second;

            // remove from the list of clients
            m_clients.erase(it);

            // drop the lock while closing the connection and removing from epoll
            locker.unlock();

            // remove the socket from epoll and close it
            if (details.sock >= 0)
            {
                if (epoll_ctl(m_pollFd, EPOLL_CTL_DEL, details.sock, nullptr) != 0)
                    RIALTO_IPC_LOG_SYS_ERROR(errno, "failed to remove socket from epoll");

                if (shutdown(details.sock, SHUT_RDWR) != 0)
                    RIALTO_IPC_LOG_SYS_ERROR(errno, "failed to shutdown socket");
                if (close(details.sock) != 0)
                    RIALTO_IPC_LOG_SYS_ERROR(errno, "failed to close socket");
            }

            // let the installed handler know a client has disconnected
            if (details.disconnectedCb)
                details.disconnectedCb(details.client);

            // ensure client object is destructed without the client lock held
            details.client.reset();

            // re-take the lock for the next client
            locker.lock();
        }
    }

    return true;
}

// -----------------------------------------------------------------------------
/*!
    \internal

    Adds the \a socketFd to the internal list of client sockets.  This is called
    when a new connection is accepted on a listening socket, or when a client
    fd is added via ServerImpl::addClient(...).

    Returns a nullptr if failed to add the socket.

 */
std::shared_ptr<ClientImpl> ServerImpl::addClientSocket(int socketFd, const std::string &listeningSocketPath,
                                                        std::function<void(const std::shared_ptr<IClient> &)> disconnectedCb)
{
    // get the client credentials
    struct ucred clientCreds = {0};
    socklen_t clientCredsLen = sizeof(clientCreds);

    if ((getsockopt(socketFd, SOL_SOCKET, SO_PEERCRED, &clientCreds, &clientCredsLen) < 0) ||
        (clientCredsLen != sizeof(clientCreds)))
    {
        RIALTO_IPC_LOG_SYS_ERROR(errno, "failed to get client's details");
        return nullptr;
    }

    // create a new unique client id for the connection
    const uint64_t kClientId = m_clientIdCounter++;

    // add the new socket to the poll loop
    epoll_event event = {.events = EPOLLIN, .data = {.u64 = kClientId}};
    if (epoll_ctl(m_pollFd, EPOLL_CTL_ADD, socketFd, &event) != 0)
    {
        RIALTO_IPC_LOG_SYS_ERROR(errno, "epoll_ctl failed to add client socket");
        return nullptr;
    }

    // create initial client object for the socket
    auto client = std::make_shared<ClientImpl>(shared_from_this(), kClientId, clientCreds);

    // and the details for the internal list
    ClientDetails clientDetails;
    clientDetails.sock = socketFd;
    clientDetails.disconnectedCb = std::move(disconnectedCb);
    clientDetails.client = client;

    // add to the set of clients
    {
        std::lock_guard<std::mutex> locker(m_clientsLock);
        m_clients.emplace(kClientId, clientDetails);
    }

    RIALTO_IPC_LOG_INFO("new client connected - giving id %" PRIu64, kClientId);

    return client;
}

// -----------------------------------------------------------------------------
/*!
    \internal

    Called when an event occurs on the listening socket. The code first accepts
    the connection and retrieves the client details, it then calls the installed
    handler to determine if we should drop this connection or not.


 */
void ServerImpl::processNewConnection(uint64_t socketId)
{
    RIALTO_IPC_LOG_DEBUG("processing new connection");

    std::unique_lock<std::mutex> socketLocker(m_socketsLock);

    // find matching socket object
    auto it = m_sockets.find(socketId);
    if (it == m_sockets.end())
    {
        RIALTO_IPC_LOG_ERROR("failed to find listening socket with id %" PRIu64, socketId);
        return;
    }

    const Socket &kSocket = it->second;

    // accept the connection from the client
    struct sockaddr clientAddr = {0};
    socklen_t clientAddrLen = sizeof(clientAddr);

    int clientSock = accept4(kSocket.sockFd, &clientAddr, &clientAddrLen, SOCK_NONBLOCK | SOCK_CLOEXEC);
    if (clientSock < 0)
    {
        RIALTO_IPC_LOG_SYS_ERROR(errno, "failed to accept client connection");
        return;
    }

    const std::string kSockPath = kSocket.sockPath;
    std::function<void(const std::shared_ptr<IClient> &)> connectedCb = kSocket.connectedCb;
    std::function<void(const std::shared_ptr<IClient> &)> disconnectedCb = kSocket.disconnectedCb;

    socketLocker.unlock();

    // attempt to add the socket to the client list
    auto client = addClientSocket(clientSock, kSockPath, std::move(disconnectedCb));
    if (!client)
    {
        close(clientSock);
        return;
    }

    // notify the handler that a new connection has been made
    if (connectedCb)
    {
        // tell the handler we have a new client
        connectedCb(client);
    }
}

// -----------------------------------------------------------------------------
/*!
    \internal
    \static

    Reads all the file descriptors from a message. It returns the file descriptors
    as a vector of FileDescriptor objects, these objects safely store the fd and
    close them when they're destructed.

 */
static std::vector<FileDescriptor> readMessageFds(const struct msghdr *msg, size_t limit)
{
    std::vector<FileDescriptor> fds;

    for (struct cmsghdr *cmsg = CMSG_FIRSTHDR(msg); cmsg != nullptr;
         cmsg = CMSG_NXTHDR(const_cast<struct msghdr *>(msg), cmsg))
    {
        if ((cmsg->cmsg_level == SOL_SOCKET) && (cmsg->cmsg_type == SCM_RIGHTS))
        {
            const unsigned kFdsLength = cmsg->cmsg_len - CMSG_LEN(0);
            if ((kFdsLength < sizeof(int)) || ((kFdsLength % sizeof(int)) != 0))
            {
                RIALTO_IPC_LOG_ERROR("invalid fd array size");
            }
            else
            {
                const size_t n = kFdsLength / sizeof(int);
                RIALTO_IPC_LOG_DEBUG("received %zu fds", n);

                fds.reserve(std::min(limit, n));

                const int *kFds = reinterpret_cast<int *>(CMSG_DATA(cmsg));
                for (size_t i = 0; i < n; i++)
                {
                    RIALTO_IPC_LOG_DEBUG("received fd %d", kFds[i]);

                    if (fds.size() >= limit)
                    {
                        RIALTO_IPC_LOG_ERROR("received to many file descriptors, "
                                             "exceeding max per message, closing left overs");
                    }
                    else
                    {
                        FileDescriptor fd(kFds[i]);
                        if (!fd.isValid())
                        {
                            RIALTO_IPC_LOG_ERROR("received invalid fd (couldn't dup)");
                        }
                        else
                        {
                            fds.emplace_back(std::move(fd));
                        }
                    }

                    if (close(kFds[i]) != 0)
                    {
                        RIALTO_IPC_LOG_SYS_ERROR(errno, "failed to close received fd");
                    }
                }
            }
        }
    }

    return fds;
}

// -----------------------------------------------------------------------------
/*!
    \internal

    Processes an event from a client socket.

 */
void ServerImpl::processClientSocket(uint64_t clientId, unsigned events)
{
    // take the lock while accessing the client list
    std::unique_lock<std::mutex> locker(m_clientsLock);

    auto it = m_clients.find(clientId);
    if (it == m_clients.end())
    {
        // should never happen
        RIALTO_IPC_LOG_ERROR("received an event from a socket with no matching client");
        return;
    }

    // check if the client is marked for closure, if so then just ignore the data
    if (m_condemnedClients.count(clientId) != 0)
    {
        return;
    }

    // get the socket that corresponds to the client connection
    const int kSockFd = it->second.sock;

    // get the client object
    std::shared_ptr<ClientImpl> clientObj = it->second.client;

    // can safely release the lock now we have the clientId and client object
    locker.unlock();

    // if there was an error disconnect the socket
    if (events & EPOLLERR)
    {
        RIALTO_IPC_LOG_ERROR("error detected on client socket - disconnecting client");
        disconnectClient(clientId);
        return;
    }

    if (events & EPOLLIN)
    {
        // read all messages from the client socket, we break out if the socket is closed
        // or EWOULDBLOCK is returned on a read (ie. no more messages to read)
        while (true)
        {
            struct msghdr msg = {nullptr};
            struct iovec io = {.iov_base = m_recvDataBuf, .iov_len = sizeof(m_recvDataBuf)};

            bzero(&msg, sizeof(msg));
            msg.msg_iov = &io;
            msg.msg_iovlen = 1;
            msg.msg_control = m_recvCtrlBuf;
            msg.msg_controllen = sizeof(m_recvCtrlBuf);

            // read one message
            ssize_t rd = TEMP_FAILURE_RETRY(recvmsg(kSockFd, &msg, MSG_CMSG_CLOEXEC));
            if (rd < 0)
            {
                if (errno != EWOULDBLOCK)
                {
                    RIALTO_IPC_LOG_SYS_ERROR(errno, "error reading client socket");
                    disconnectClient(clientId);
                }

                break;
            }
            else if (rd == 0)
            {
                // client closed connection, and we've read all data, add to the condemned set
                // so is cleaned up once all the events are processed
                disconnectClient(clientId);

                break;
            }
            else if (msg.msg_flags & (MSG_TRUNC | MSG_CTRUNC))
            {
                RIALTO_IPC_LOG_WARN("received message from client %" PRIu64 " truncated, discarding", clientId);

                // make sure to close all the fds, otherwise we'll leak them
                readMessageFds(&msg, 16);
            }
            else
            {
                // if there is control data then assume fd(s) have been passed
                if (msg.msg_controllen > 0)
                {
                    processClientMessage(clientObj, m_recvDataBuf, rd, readMessageFds(&msg, 16));
                }
                else
                {
                    processClientMessage(clientObj, m_recvDataBuf, rd);
                }
            }
        }
    }
}

// -----------------------------------------------------------------------------
/*!
    \internal

    Places the received file descriptors into the request message.

    It works by iterating over the fields in the message, finding ones that are
    marked as 'field_is_fd' and then replacing the received integer value with
    an actual file descriptor.

 */
static bool addRequestFileDescriptors(google::protobuf::Message *request, const std::vector<FileDescriptor> &requestFds)
{
    auto fdIterator = requestFds.begin();

    const google::protobuf::Descriptor *kDescriptor = request->GetDescriptor();
    const google::protobuf::Reflection *kReflection = nullptr;

    const int n = kDescriptor->field_count();
    for (int i = 0; i < n; i++)
    {
        auto fieldDescriptor = kDescriptor->field(i);
        if (fieldDescriptor->options().HasExtension(field_is_fd) && fieldDescriptor->options().GetExtension(field_is_fd))
        {
            if (fieldDescriptor->type() != google::protobuf::FieldDescriptor::TYPE_INT32)
            {
                RIALTO_IPC_LOG_ERROR("field is marked as containing an fd but not an int32 type");
                return false;
            }

            if (!kReflection)
            {
                kReflection = request->GetReflection();
            }

            if (kReflection->HasField(*request, fieldDescriptor))
            {
                if (fdIterator == requestFds.end())
                {
                    RIALTO_IPC_LOG_ERROR("field is marked as containing an fd but one was supplied");
                    return false;
                }

                kReflection->SetInt32(request, fieldDescriptor, fdIterator->fd());
                ++fdIterator;
            }
        }
    }

    if (fdIterator != requestFds.end())
    {
        RIALTO_IPC_LOG_ERROR("received too many file descriptors in the message");
        return false;
    }

    return true;
}

// -----------------------------------------------------------------------------
/*!
    \internal

    Processes a message received on a client socket.

 */
void ServerImpl::processClientMessage(const std::shared_ptr<ClientImpl> &client, const uint8_t *data, size_t dataLen,
                                      const std::vector<FileDescriptor> &fds)
{
    RIALTO_IPC_LOG_DEBUG("processing client message of size %zu bytes (%zu fds) from client %" PRId64, dataLen,
                         fds.size(), client->id());

    // parse the message
    transport::MessageToServer message;
    if (!message.ParseFromArray(data, static_cast<int>(dataLen)))
    {
        RIALTO_IPC_LOG_ERROR("invalid request");
        return;
    }

    if (message.has_call())
    {
        processMethodCall(client, message.call(), fds);
    }
    else
    {
        RIALTO_IPC_LOG_WARN("received unknown message type from client");
    }
}

// -----------------------------------------------------------------------------
/*!
    \internal

    Processes a method call requst from a client.

 */
void ServerImpl::processMethodCall(const std::shared_ptr<ClientImpl> &client, const transport::MethodCall &call,
                                   const std::vector<FileDescriptor> &fds)
{
    // try and find the service with the given name
    const std::string &kServiceName = call.service_name();
    auto it = client->m_services.find(kServiceName);
    if (it == client->m_services.end())
    {
        RIALTO_IPC_LOG_ERROR("unknown service request '%s'", kServiceName.c_str());

        sendErrorReply(client, call.serial_id(), "Unknown service '%s'", kServiceName.c_str());
        return;
    }

    std::shared_ptr<google::protobuf::Service> service = it->second;

    // try and find the method
    const std::string &kMethodName = call.method_name();
    const google::protobuf::MethodDescriptor *kMethod = service->GetDescriptor()->FindMethodByName(kMethodName);
    if (!kMethod)
    {
        RIALTO_IPC_LOG_ERROR("no method with name '%s'", kMethodName.c_str());

        sendErrorReply(client, call.serial_id(), "Unknown method '%s'", kMethodName.c_str());
        return;
    }

    // check if the method is expecting a reply
    const bool kNoReply = kMethod->options().HasExtension(no_reply) && kMethod->options().GetExtension(no_reply);

    // parse the request data
    google::protobuf::Message *requestMessage = service->GetRequestPrototype(kMethod).New();
    if (!requestMessage->ParseFromString(call.request_message()))
    {
        RIALTO_IPC_LOG_ERROR("failed to parse method from array");
    }
    else if (!addRequestFileDescriptors(requestMessage, fds))
    {
        RIALTO_IPC_LOG_ERROR("mismatch of file descriptors to the request");
    }
    else
    {
        RIALTO_IPC_LOG_DEBUG("call{ serial %" PRIu64 " } - %s.%s { %s }", call.serial_id(), kServiceName.c_str(),
                             kMethodName.c_str(), requestMessage->ShortDebugString().c_str());

        auto *controller = new ServerControllerImpl(client, call.serial_id());

        if (kNoReply)
        {
            // we should not send a reply for this call, so call the code to handle the
            // request, but no need to pass a controller, response or closure object
            static google::protobuf::internal::FunctionClosure0 nullClosure(&google::protobuf::DoNothing, false);
            service->CallMethod(kMethod, controller, requestMessage, nullptr, &nullClosure);

            delete controller;
        }
        else
        {
            // create a response
            google::protobuf::Message *responseMessage = service->GetResponsePrototype(kMethod).New();

            // this is finally where we call the service implementation to process the request
            service->CallMethod(kMethod, controller, requestMessage, responseMessage,
                                google::protobuf::NewCallback(this, &ServerImpl::handleResponse, controller,
                                                              responseMessage));
        }
    }

    delete requestMessage;
}

// -----------------------------------------------------------------------------
/*!
    \internal
    \threadsafe

    Sends a message to the given client if still connected.

 */
void ServerImpl::sendReply(uint64_t clientId, const std::shared_ptr<msghdr> &msg)
{
    // now take the lock (so the socket is not closed beneath us) and send the reply
    std::lock_guard<std::mutex> locker(m_clientsLock);

    auto it = m_clients.find(clientId);
    if (it == m_clients.end())
    {
        RIALTO_IPC_LOG_WARN("socket removed before error reply could be sent");
    }
    else if (it->second.sock < 0)
    {
        RIALTO_IPC_LOG_WARN("socket closed before error reply could be sent");
    }
    else if (!msg)
    {
        RIALTO_IPC_LOG_WARN("invalid msg to send on socket, ignoring");
    }
    else if (TEMP_FAILURE_RETRY(sendmsg(it->second.sock, msg.get(), MSG_NOSIGNAL)) !=
             static_cast<ssize_t>(msg->msg_iov->iov_len))
    {
        RIALTO_IPC_LOG_SYS_ERROR(errno, "failed to send the complete error reply message");
    }
}

// -----------------------------------------------------------------------------
/*!
    \internal

    Sends an error back to the client with a reason string in \a format.

 */
void ServerImpl::sendErrorReply(const std::shared_ptr<ClientImpl> &client, uint64_t serialId, const char *format, ...)
{
    // construct the error string
    va_list ap;
    va_start(ap, format);
    char reason[512];
    vsnprintf(reason, sizeof(reason), format, ap);
    va_end(ap);

    // construct the reply message
    auto msg = populateErrorReply(client, serialId, reason);

    // and send it
    sendReply(client->id(), msg);
}

// -----------------------------------------------------------------------------
/*!
    \internal
    \threadsafe

    Called once the service handler code has processed the request and
    completed it.  This may be called from a different thread if the service
    implementation decided to off-load the request to a processing thread.

 */
void ServerImpl::handleResponse(ServerControllerImpl *controller, google::protobuf::Message *response)
{
    if (!controller || !controller->m_kClient)
    {
        RIALTO_IPC_LOG_ERROR("missing controller or attached client");
        return;
    }

    const std::shared_ptr<const ClientImpl> kClient = controller->m_kClient;
    const uint64_t kClientId = kClient->id();

    std::shared_ptr<msghdr> message;
    if (!controller->m_failed)
    {
        message = populateReply(kClient, controller->m_kSerialId, response);
    }
    else
    {
        message = populateErrorReply(kClient, controller->m_kSerialId, controller->m_failureReason);
    }

    // no longer need the controller or the response objects
    delete response;
    delete controller;

    // send the reply message to the given client
    sendReply(kClientId, message);
}

// -----------------------------------------------------------------------------
/*!
    \internal

    Gets all the file descriptors that are stored in the \a response message
    and returns them as a vector of ints.

    It works by iterating over the fields in the message, finding ones that are
    marked as 'field_is_fd' and inserting the fd value into control part of the
    message.  The actual integer sent in the data part of the message is set to
    -1.

 */
static std::vector<int> getResponseFileDescriptors(google::protobuf::Message *response)
{
    std::vector<int> fds;

    // process any file descriptors from the response message
    const google::protobuf::Descriptor *kDescriptor = response->GetDescriptor();
    const google::protobuf::Reflection *kReflection = nullptr;

    const int n = kDescriptor->field_count();
    for (int i = 0; i < n; i++)
    {
        auto fieldDescriptor = kDescriptor->field(i);
        if (fieldDescriptor->options().HasExtension(field_is_fd) && fieldDescriptor->options().GetExtension(field_is_fd))
        {
            if (fieldDescriptor->type() != google::protobuf::FieldDescriptor::TYPE_INT32)
            {
                RIALTO_IPC_LOG_ERROR("field is marked as containing an fd but not an int32 type");
                return {};
            }

            if (!kReflection)
            {
                kReflection = response->GetReflection();
            }

            if (kReflection->HasField(*response, fieldDescriptor))
            {
                fds.push_back(kReflection->GetInt32(*response, fieldDescriptor));
                kReflection->SetInt32(response, fieldDescriptor, -1);
            }
        }
    }

    return fds;
}

// -----------------------------------------------------------------------------
/*!
    \internal
    \static

    Populates tha socket message buffer with the reply data for the RPC request.

 */
std::shared_ptr<msghdr> ServerImpl::populateReply(const std::shared_ptr<const ClientImpl> &client, uint64_t serialId,
                                                  google::protobuf::Message *response)
{
    // create the base reply
    transport::MessageFromServer message;
    transport::MethodCallReply *reply = message.mutable_reply();
    if (!reply)
    {
        RIALTO_IPC_LOG_ERROR("failed to create mutable reply object");
        return nullptr;
    }

    // convert the message response to a data string
    std::string respString = response->SerializeAsString();

    // wrap in a transport response and send that
    reply->set_reply_id(serialId);
    reply->set_reply_message(std::move(respString));

    // next need to check if the response message has any file descriptors in
    // it that need to be attached
    const std::vector<int> kFds = getResponseFileDescriptors(response);
    const size_t kRequiredCtrlLen = kFds.empty() ? 0 : CMSG_SPACE(sizeof(int) * kFds.size());

    // calculate the size of the reply
    const size_t kRequiredDataLen = message.ByteSizeLong();
    if (kRequiredDataLen > m_kMaxMessageLen)
    {
        RIALTO_IPC_LOG_ERROR("reply exceeds maximum message limit (%zu, max %zu)", kRequiredDataLen, m_kMaxMessageLen);

        // error message is too big, replace with a generic error
        return populateErrorReply(client, serialId, "Internal error - reply message to large");
    }

    // build the socket message to send
    auto msgBuf =
        m_sendBufPool.allocateShared<uint8_t>(sizeof(msghdr) + sizeof(iovec) + kRequiredCtrlLen + kRequiredDataLen);

    auto *header = reinterpret_cast<msghdr *>(msgBuf.get());
    bzero(header, sizeof(msghdr));

    auto *ctrl = reinterpret_cast<uint8_t *>(msgBuf.get() + sizeof(msghdr));
    header->msg_control = ctrl;
    header->msg_controllen = kRequiredCtrlLen;

    auto *iov = reinterpret_cast<iovec *>(msgBuf.get() + sizeof(msghdr) + kRequiredCtrlLen);
    header->msg_iov = iov;
    header->msg_iovlen = 1;

    auto *data = reinterpret_cast<uint8_t *>(msgBuf.get() + sizeof(msghdr) + kRequiredCtrlLen + sizeof(iovec));
    iov->iov_base = data;
    iov->iov_len = kRequiredDataLen;

    // copy in the data
    message.SerializeWithCachedSizesToArray(data);

    // add the fds
    if (!kFds.empty())
    {
        struct cmsghdr *cmsg = CMSG_FIRSTHDR(header);
        if (!cmsg)
        {
            RIALTO_IPC_LOG_ERROR("odd, failed to get the first cmsg header");
            return nullptr;
        }

        cmsg->cmsg_level = SOL_SOCKET;
        cmsg->cmsg_type = SCM_RIGHTS;
        cmsg->cmsg_len = CMSG_LEN(sizeof(int) * kFds.size());
        memcpy(CMSG_DATA(cmsg), kFds.data(), sizeof(int) * kFds.size());
        header->msg_controllen = cmsg->cmsg_len;
    }

    RIALTO_IPC_LOG_DEBUG("reply{ serial %" PRIu64 " } - { %s }", serialId, response->ShortDebugString().c_str());

    // std::reinterpret_pointer_cast is only implemented in C++17 and newer, so for
    // now do it manually
    return std::shared_ptr<msghdr>(msgBuf, reinterpret_cast<msghdr *>(msgBuf.get()));
}

// -----------------------------------------------------------------------------
/*!
    \internal

    Populates tha socket message buffer with a reply error message.

 */
std::shared_ptr<msghdr> ServerImpl::populateErrorReply(const std::shared_ptr<const ClientImpl> &client,
                                                       uint64_t serialId, const std::string &reason)
{
    // create the base reply
    transport::MessageFromServer message;
    transport::MethodCallError *error = message.mutable_error();
    error->set_reply_id(serialId);
    error->set_error_reason(reason);

    // check the message will fit
    size_t replySize = message.ByteSizeLong();
    if (replySize > m_kMaxMessageLen)
    {
        RIALTO_IPC_LOG_ERROR("error reply exceeds max message size");

        // error message is to big, replace with a generic error
        error->set_error_reason("Error message truncated");
        replySize = message.ByteSizeLong();
    }

    RIALTO_IPC_LOG_DEBUG("error{ serial %" PRIu64 " } - \"%s\"", serialId, reason.c_str());

    // construct the message to send on the socket
    auto msgBuf = m_sendBufPool.allocateShared<uint8_t>(sizeof(msghdr) + sizeof(iovec) + replySize);

    auto *header = reinterpret_cast<msghdr *>(msgBuf.get());
    bzero(header, sizeof(msghdr));

    auto *iov = reinterpret_cast<iovec *>(msgBuf.get() + sizeof(msghdr));
    header->msg_iov = iov;
    header->msg_iovlen = 1;

    auto *data = reinterpret_cast<uint8_t *>(msgBuf.get() + sizeof(msghdr) + sizeof(iovec));
    iov->iov_base = data;
    iov->iov_len = replySize;

    // serialise the reply to the message buffer
    message.SerializeWithCachedSizesToArray(data);

    // std::reinterpret_pointer_cast is only implemented in C++17 and newer, so for
    // now do it manually
    return std::shared_ptr<msghdr>(msgBuf, reinterpret_cast<msghdr *>(msgBuf.get()));
}

// -----------------------------------------------------------------------------
/*!
    \threadsafe

    Returns \c true if the client with \a clientId is currently connected to
    the server.

 */
bool ServerImpl::isClientConnected(uint64_t clientId) const
{
    std::unique_lock<std::mutex> locker(m_clientsLock);
    return (m_clients.count(clientId) > 0);
}

// -----------------------------------------------------------------------------
/*!
    \threadsafe

    May be called internally when there is an error on the socket, or externally
    (possibly from a different thread) if the handler or service code decides to
    close the client connection.

 */
void ServerImpl::disconnectClient(uint64_t clientId)
{
    std::unique_lock<std::mutex> locker(m_clientsLock);
    m_condemnedClients.insert(clientId);
    locker.unlock();

    wakeEventLoop();
}

// -----------------------------------------------------------------------------
/*!
    \threadsafe

    Called via the IAVBusClient interface when an async event should be sent.

    This may be called from any thread, or from within the rpc message handler.

    The \a clientId is the client to send the event to.

 */
bool ServerImpl::sendEvent(uint64_t clientId, const std::shared_ptr<google::protobuf::Message> &eventMessage)
{
    // gets the file descriptors from the event message
    const std::vector<int> kFds = getResponseFileDescriptors(eventMessage.get());
    const size_t kRequiredCtrlLen = kFds.empty() ? 0 : CMSG_SPACE(sizeof(int) * kFds.size());

    // create the base reply
    transport::MessageFromServer message;
    transport::EventFromServer *event = message.mutable_event();
    if (!event)
    {
        RIALTO_IPC_LOG_ERROR("failed to create mutable event object");
        return false;
    }

    event->set_event_name(eventMessage->GetTypeName());

    // convert the event to a data string
    std::string respString = eventMessage->SerializeAsString();

    // wrap in a transport response and send that
    event->set_message(std::move(respString));

    // check the reply will fit
    size_t requiredDataLen = message.ByteSizeLong();
    if (requiredDataLen > m_kMaxMessageLen)
    {
        RIALTO_IPC_LOG_ERROR("event message to big to fit in buffer (size %zu, max size %zu)", requiredDataLen,
                             m_kMaxMessageLen);
        return false;
    }

    // build the socket message to send
    auto msgBuf =
        m_sendBufPool.allocateShared<uint8_t>(sizeof(msghdr) + sizeof(iovec) + kRequiredCtrlLen + requiredDataLen);

    auto *header = reinterpret_cast<msghdr *>(msgBuf.get());
    bzero(header, sizeof(msghdr));

    auto *ctrl = reinterpret_cast<uint8_t *>(msgBuf.get() + sizeof(msghdr));
    header->msg_control = ctrl;
    header->msg_controllen = kRequiredCtrlLen;

    auto *iov = reinterpret_cast<iovec *>(msgBuf.get() + sizeof(msghdr) + kRequiredCtrlLen);
    header->msg_iov = iov;
    header->msg_iovlen = 1;

    auto *data = reinterpret_cast<uint8_t *>(msgBuf.get() + sizeof(msghdr) + kRequiredCtrlLen + sizeof(iovec));
    iov->iov_base = data;
    iov->iov_len = requiredDataLen;

    // copy in the data
    message.SerializeWithCachedSizesToArray(data);

    // add the fds
    if (!kFds.empty())
    {
        struct cmsghdr *cmsg = CMSG_FIRSTHDR(header);
        if (!cmsg)
        {
            RIALTO_IPC_LOG_ERROR("odd, failed to get the first cmsg header");
            return false;
        }

        cmsg->cmsg_level = SOL_SOCKET;
        cmsg->cmsg_type = SCM_RIGHTS;
        cmsg->cmsg_len = CMSG_LEN(sizeof(int) * kFds.size());
        memcpy(CMSG_DATA(cmsg), kFds.data(), sizeof(int) * kFds.size());
        header->msg_controllen = cmsg->cmsg_len;
    }

    // finally, take the lock (so the socket is not closed beneath us) and send the reply
    std::unique_lock<std::mutex> locker(m_clientsLock);

    auto it = m_clients.find(clientId);
    if (it == m_clients.end() || it->second.sock < 0)
    {
        RIALTO_IPC_LOG_WARN("socket closed before event could be sent");
        return false;
    }
    else if (TEMP_FAILURE_RETRY(sendmsg(it->second.sock, header, MSG_NOSIGNAL)) != static_cast<ssize_t>(requiredDataLen))
    {
        RIALTO_IPC_LOG_SYS_ERROR(errno, "failed to send the complete event message");
        return false;
    }

    locker.unlock();

    RIALTO_IPC_LOG_DEBUG("event{ %s } - { %s }", eventMessage->GetTypeName().c_str(),
                         eventMessage->ShortDebugString().c_str());

    return true;
}
} // namespace firebolt::rialto::ipc
