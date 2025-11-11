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

#ifndef FIREBOLT_RIALTO_IPC_IPC_CHANNEL_IMPL_H_
#define FIREBOLT_RIALTO_IPC_IPC_CHANNEL_IMPL_H_

#include "FileDescriptor.h"
#include "IIpcChannel.h"
#include "IpcClientControllerImpl.h"
#include "SimpleBufferPool.h"

#include "rialtoipc-transport.pb.h"
#include <google/protobuf/service.h>

#include <atomic>
#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <sys/socket.h>

namespace firebolt::rialto::ipc
{
class ChannelFactory : public IChannelFactory
{
public:
    ChannelFactory() = default;
    ~ChannelFactory() override = default;

    std::shared_ptr<IChannel> createChannel(int sockFd) override;
    std::shared_ptr<IChannel> createChannel(const std::string &socketPath) override;
};

class ChannelImpl final : public IChannel
{
public:
    explicit ChannelImpl(int sockFd);
    explicit ChannelImpl(const std::string &socketPath);
    ~ChannelImpl() final;

    void disconnect() override;
    bool isConnected() const override;

    int fd() const override;
    bool wait(int timeoutMSecs) override;
    bool process() override;
    bool unsubscribe(int eventTag) override;

    void CallMethod(const google::protobuf::MethodDescriptor *method, google::protobuf::RpcController *controller,
                    const google::protobuf::Message *request, google::protobuf::Message *response,
                    google::protobuf::Closure *done) override;

    using EventHandler = std::function<void(const std::shared_ptr<google::protobuf::Message> &msg)>;

    int subscribeImpl(const std::string &name, const google::protobuf::Descriptor *descriptor,
                      EventHandler &&handler) override;

private:
    void disconnectNoLock();

    bool processSocketEvent();
    void processTimeoutEvent();
    void processWakeEvent();

    void processServerMessage(const uint8_t *data, size_t len, std::vector<FileDescriptor> *fds);
    void processReplyFromServer(const ::firebolt::rialto::ipc::transport::MethodCallReply &reply,
                                std::vector<FileDescriptor> *fds);
    void processErrorFromServer(const ::firebolt::rialto::ipc::transport::MethodCallError &error);
    void processEventFromServer(const ::firebolt::rialto::ipc::transport::EventFromServer &event,
                                std::vector<FileDescriptor> *fds);

    bool createConnectedSocket(const std::string &socketPath);
    bool attachSocket(int sockFd);
    bool initChannel();
    void termChannel();
    bool isConnectedInternal() const; // to avoid calling virtual method in constructor

    static std::vector<FileDescriptor> readMessageFds(const struct msghdr *msg, size_t limit);
    static std::vector<int> getMessageFds(const google::protobuf::Message &message);

    static bool addReplyFileDescriptors(google::protobuf::Message *reply, std::vector<FileDescriptor> *fds);

    struct MethodCall
    {
        std::chrono::steady_clock::time_point timeoutDeadline;
        ClientControllerImpl *controller = nullptr;
        google::protobuf::Message *response = nullptr;
        google::protobuf::Closure *closure = nullptr;
    };

    void updateTimeoutTimer();

    static void complete(MethodCall *call);
    static void completeWithError(MethodCall *call, std::string reason);

private:
    int m_sock;
    int m_epollFd;
    int m_timerFd;
    int m_eventFd;

    SimpleBufferPool m_sendBufPool;

    mutable std::mutex m_lock;
    std::atomic<uint64_t> m_serialCounter;

    const std::chrono::milliseconds m_timeout;

    std::map<uint64_t, MethodCall> m_methodCalls;

    std::mutex m_eventsLock;

    int m_eventTagCounter;

    struct Event
    {
        int id;
        const google::protobuf::Descriptor *descriptor;
        EventHandler handler;
    };

    std::multimap<std::string, Event> m_eventHandlers;
};

} // namespace firebolt::rialto::ipc

#endif // FIREBOLT_RIALTO_IPC_IPC_CHANNEL_IMPL_H_
