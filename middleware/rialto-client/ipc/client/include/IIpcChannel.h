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

#ifndef FIREBOLT_RIALTO_IPC_I_IPC_CHANNEL_H_
#define FIREBOLT_RIALTO_IPC_I_IPC_CHANNEL_H_

#include <google/protobuf/service.h>

#include <functional>
#include <memory>
#include <string>
#include <utility>

namespace firebolt::rialto::ipc
{
class IChannel;

/**
 * @brief IChannel factory class, returns a new connected IChannel object
 */
class IChannelFactory
{
public:
    IChannelFactory() = default;
    virtual ~IChannelFactory() = default;

    /**
     * @brief Create a IChannelFactory instance.
     *
     * @retval the factory instance or null on error.
     */
    static std::shared_ptr<IChannelFactory> createFactory();

    /**
     * @brief Create a connected IChannel object.
     *
     * \threadsafe
     *
     * @param[in] socketPath    : The path of the socket to connect.
     *
     * @retval the connected ipc channel instance or null on error.
     */
    virtual std::shared_ptr<IChannel> createChannel(const std::string &socketPath) = 0;

    /**
     * @brief Create a connected IChannel object.
     *
     * \threadsafe
     *
     * Creates an IPC channel around the supplied socket fd.
     * The call takes ownership of the fd and it will close it when the channel is
     * destructed.  The exception to this is if the call fails, in which case the
     * caller should close the socket themselves.
     *
     * @param[in] sockFd    : The fd of the connected socket.
     *
     * @retval the connected ipc channel instance or null on error.
     */
    virtual std::shared_ptr<IChannel> createChannel(int sockFd) = 0;
};

class IChannel : public google::protobuf::RpcChannel
{
public:
    IChannel() = default;
    virtual ~IChannel() = default;

    IChannel(const IChannel &) = delete;
    IChannel &operator=(const IChannel &) = delete;
    IChannel(IChannel &&) = delete;
    IChannel &operator=(IChannel &&) = delete;

    /**
     * @brief Disconnect the IPC channel from the socket.
     *
     * \threadsafe
     */
    virtual void disconnect() = 0;

    /**
     * @brief Query whether the IPC channel is connected.
     *
     * \threadsafe
     *
     * @retval true if connected, false otherwise.
     */
    virtual bool isConnected() const = 0;

    /**
     * @brief Get the epoll file descriptor.
     *
     *  \threadsafe
     *
     * @retval >= 0 on success, -1 otherwise.
     */
    virtual int fd() const = 0;

    /**
     * @brief Waits for any I/O to occur on the socket or timeout from a method call.
     *
     * The call will block for a maximum of timeoutMSecs, to make the call wait
     * indefinitely pass -1.
     *
     * @param[in] timeoutMSecs  : The time to wait for I/O.
     *
     * @retval false if there was an error or the channel was disconnected, true otherwise
     */
    inline bool wait() { return this->wait(-1); }
    virtual bool wait(int timeoutMSecs) = 0;

    /**
     * @brief Processes at most one event on the channel, this may be a socket message or
     * a method call timeout.
     *
     *  \threadsafe
     *
     * @retval false if there was an error or the channel was disconnected, true otherwise
     */
    virtual bool process() = 0;

    /**
     * @brief Subscribe for event message.
     *
     * @param[in] handler   : The handler to be called when the event is triggered.
     *
     * @retval the tag of the subscribed event, -1 on error.
     */
    template <class Message> inline int subscribe(std::function<void(const std::shared_ptr<Message> &message)> handler)
    {
        if (!handler)
            return -1;

        return this->subscribeImpl(Message::default_instance().GetTypeName(), Message::default_instance().GetDescriptor(),
                                   [handler_ = std::move(handler)](const std::shared_ptr<google::protobuf::Message> &msg)
                                   { handler_(std::dynamic_pointer_cast<Message>(msg)); });
    }

    /**
     * @brief Unsubscribe event.
     *
     * @param[in] eventTag   : The tag of the event to unsubscribe too.
     *
     * @retval true on success.
     */
    virtual bool unsubscribe(int eventTag) = 0;

private:
    virtual int subscribeImpl(const std::string &eventName, const google::protobuf::Descriptor *descriptor,
                              std::function<void(const std::shared_ptr<google::protobuf::Message> &msg)> &&handler) = 0;
};

} // namespace firebolt::rialto::ipc

#endif // FIREBOLT_RIALTO_IPC_I_IPC_CHANNEL_H_
