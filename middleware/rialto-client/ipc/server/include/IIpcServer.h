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

#ifndef FIREBOLT_RIALTO_IPC_I_IPC_SERVER_H_
#define FIREBOLT_RIALTO_IPC_I_IPC_SERVER_H_

#include <google/protobuf/message.h>
#include <google/protobuf/service.h>

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <utility>

namespace firebolt::rialto::ipc
{
/**
 * @brief Interface to object describing a client connected to the server.
 *
 *  An object of this interface is provided in the callbacks for when a client
 *  connects / disconnects.  The interface can be used to query details above
 *  the client and send it asynchronous events.
 */
class IClient
{
public:
    IClient() = default;
    virtual ~IClient() = default;

    IClient(const IClient &) = delete;
    IClient &operator=(const IClient &) = delete;
    IClient(IClient &&) = delete;
    IClient &operator=(IClient &&) = delete;

public:
    /**
     * @brief Gets the pid of the client that created the connection.
     *
     * \threadsafe
     *
     * @retval the pid.
     */
    virtual pid_t getClientPid() const = 0;

    /**
     * @brief Gets the userIdof the client that created the connection.
     *
     * \threadsafe
     *
     * @retval the userId.
     */
    virtual uid_t getClientUserId() const = 0;

    /**
     * @brief Gets the groupId of the client that created the connection.
     *
     * \threadsafe
     *
     * @retval the groupId.
     */
    virtual gid_t getClientGroupId() const = 0;

    /**
     * @brief Tells the server to disconnect the client.
     *
     * \threadsafe
     *
     * Typically you'd call this in the clientConnected method if you didn't want to accept the
     * client's connection.
     */
    virtual void disconnect() = 0;

    /**
     * @brief Adds a service implementation for the client.
     *
     * \threadsafe
     *
     * After this the client will be able to call methods on the service.
     *
     * @param[in] service  : service to export.
     */
    virtual void exportService(const std::shared_ptr<google::protobuf::Service> &service) = 0;

    /**
     * @brief Sends a message out to the client connection.
     *
     * \threadsafe
     *
     * This method is thread safe
     * and may be called from any context; internally it will queue the message
     * on the client's message queue and the server process will send it
     *
     * @param[in] message  : message to send.
     *
     * @retval true on success, false otherwise.
     */
    virtual bool sendEvent(const std::shared_ptr<google::protobuf::Message> &message) = 0;

    /**
     * @brief The server object.
     *
     * \threadsafe
     *
     * It is safe to hold a
     * shared_ptr to this object after the client has disconnected, but obviously
     * sendEvent will fail and data like pid and userId will be invalid.
     *
     * @retval true if the client is still conn
     */
    virtual bool isConnected() const = 0;
};

/**
 * @brief The server object.
 *
 *  You must run the server as part of an external event loop implementation, you
 *  can do that by setting up a loop that calls wait() and then process(). Or
 *  you can get the fd() and add it to an external poll loop, and when woken call
 *  process().
 */
class IServer
{
public:
    IServer() = default;
    virtual ~IServer() = default;

    IServer(const IServer &) = delete;
    IServer &operator=(const IServer &) = delete;
    IServer(IServer &&) = delete;
    IServer &operator=(IServer &&) = delete;

    /**
     * @brief Creates the listening socket to accept new connections.
     *
     * \threadsafe
     *
     * This does not blockwaiting for connections, it just creates the socket and adds to the poll loop.
     *
     * @retval true on success, false otherwise.
     */
    inline bool addSocket(const std::string &socketPath) { return addSocket(socketPath, nullptr, nullptr); }
    inline bool addSocket(const std::string &socketPath,
                          std::function<void(const std::shared_ptr<IClient> &)> clientConnectedCb)
    {
        return addSocket(socketPath, std::move(clientConnectedCb), nullptr);
    }
    virtual bool addSocket(const std::string &socketPath,
                           std::function<void(const std::shared_ptr<IClient> &)> clientConnectedCb,
                           std::function<void(const std::shared_ptr<IClient> &)> clientDisconnectedCb) = 0;
    virtual bool addSocket(int fd, std::function<void(const std::shared_ptr<IClient> &)> clientConnectedCb,
                           std::function<void(const std::shared_ptr<IClient> &)> clientDisconnectedCb) = 0;

    /**
     * @brief Create a client.
     *
     * \threadsafe
     *
     * Given a file descriptor corresponding to one end of a socket, this function
     * will create a client (std::shared_ptr<Client>) and add the new client to
     * the internal clients list.  At that point, the client is initialized and
     * ready to run, as if the client had connected to the servers listening socket.
     *
     * The other end of the socket can be passed to firebolt::rialto::ipc::Channel::connect(int sockFd)
     * function to create a client side IPC channel.
     *
     * When the other end of the socket is closed the clientDisconnectedCb will
     * be called.
     *
     * The socketFd must be a unix domain socket of type SOCK_SEQPACKET, if a different
     * socket type then this will fail and return a null std::shared_ptr.
     *
     * The server will dup the socketFd on success, so the caller can (and should)
     * close the socketFd after this call completes if it no longer needs the socket.
     *
     * @param[in] socketFd              : The file descriptor for the socket.
     * @param[in] clientDisconnectedCb  : Callback to be called on the closing of the socket.
     *
     * @retval client on success, nullptr otherwise.
     */
    inline std::shared_ptr<IClient> addClient(int socketFd) { return addClient(socketFd, nullptr); }
    virtual std::shared_ptr<IClient>
    addClient(int socketFd, std::function<void(const std::shared_ptr<IClient> &)> clientDisconnectedCb) = 0;

    /**
     * @brief Returns an fd that can be added to an external event loop.  This is NOT the
     * server socket fd.
     *
     * @retval >= 0 on success, -1 otherwise.
     */
    virtual int fd() const = 0;

    /** Waits for any I/O to occur on the socket or timeout from a method call.
     * The call will block for a maximum of  timeoutMSecs, to make the call wait
     * indefinitely pass -1.
     *
     * @param[in] timeoutMSecs  : The time to wait for I/O.
     *
     * @retval false if there was an error , true otherwise
     */
    inline bool wait() { return this->wait(-1); }
    virtual bool wait(int timeoutMSecs) = 0;

    /**
     * @brief This is the heart of the server, it is where we wait for new incoming
     * connections or data from clients, and send data back to clients.
     *
     * @retval false if there was an error, true otherwise
     */
    virtual bool process() = 0;
};

} // namespace firebolt::rialto::ipc

#endif // FIREBOLT_RIALTO_IPC_I_IPC_SERVER_H_
