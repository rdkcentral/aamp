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

#include <algorithm>
#include <cinttypes>
#include <cstdarg>
#include <memory>
#include <utility>

#include <fcntl.h>
#include <poll.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <sys/timerfd.h>
#include <sys/un.h>
#include <unistd.h>

#include "IpcChannelImpl.h"
#include "IpcLogging.h"
#include "rialtoipc.pb.h"

#if !defined(SCM_MAX_FD)
#define SCM_MAX_FD 255
#endif

namespace
{
constexpr size_t kMaxMessageSize{128 * 1024};
const std::chrono::milliseconds kDefaultIpcTimeout{3000};

std::chrono::milliseconds getIpcTimeout()
{
    const char *kCustomTimeout = getenv("RIALTO_CLIENT_IPC_TIMEOUT");
    std::chrono::milliseconds timeout{kDefaultIpcTimeout};
    if (kCustomTimeout)
    {
        try
        {
            timeout = std::chrono::milliseconds{std::stoull(kCustomTimeout)};
            RIALTO_IPC_LOG_INFO("Using custom Ipc timeout: %sms", kCustomTimeout);
        }
        catch (const std::exception &e)
        {
            RIALTO_IPC_LOG_ERROR("Custom Ipc timeout invalid, ignoring: %s", kCustomTimeout);
        }
    }
    return timeout;
}
} // namespace

namespace firebolt::rialto::ipc
{

std::shared_ptr<IChannelFactory> IChannelFactory::createFactory()
{
    std::shared_ptr<IChannelFactory> factory;
    try
    {
        factory = std::make_shared<ChannelFactory>();
    }
    catch (const std::exception &e)
    {
        RIALTO_IPC_LOG_ERROR("Failed to create the ipc channel factory, reason: %s", e.what());
    }

    return factory;
}

std::shared_ptr<IChannel> ChannelFactory::createChannel(int sockFd)
{
    std::shared_ptr<IChannel> channel;
    try
    {
        channel = std::make_shared<ChannelImpl>(sockFd);
    }
    catch (const std::exception &e)
    {
        RIALTO_IPC_LOG_ERROR("Failed to create the ipc channel with socketFd %d, reason: %s", sockFd, e.what());
    }

    return channel;
}

std::shared_ptr<IChannel> ChannelFactory::createChannel(const std::string &socketPath)
{
    std::shared_ptr<IChannel> channel;
    try
    {
        channel = std::make_shared<ChannelImpl>(socketPath);
    }
    catch (const std::exception &e)
    {
        RIALTO_IPC_LOG_ERROR("Failed to create the ipc channel with socketPath %s, reason: %s", socketPath.c_str(),
                             e.what());
    }

    return channel;
}

ChannelImpl::ChannelImpl(int sock)
    : m_sock(-1), m_epollFd(-1), m_timerFd(-1), m_eventFd(-1), m_serialCounter(1), m_timeout(getIpcTimeout()),
      m_eventTagCounter(1)
{
    if (!attachSocket(sock))
    {
        throw std::runtime_error("Failed attach the socket");
    }
    if (!initChannel())
    {
        termChannel();
        throw std::runtime_error("Failed to initalise the channel");
    }
    if (!isConnectedInternal())
    {
        termChannel();
        throw std::runtime_error("Channel not connected");
    }
}

ChannelImpl::ChannelImpl(const std::string &socketPath)
    : m_sock(-1), m_epollFd(-1), m_timerFd(-1), m_eventFd(-1), m_serialCounter(1), m_timeout(getIpcTimeout()),
      m_eventTagCounter(1)
{
    if (!createConnectedSocket(socketPath))
    {
        throw std::runtime_error("Failed connect socket");
    }
    if (!initChannel())
    {
        termChannel();
        throw std::runtime_error("Failed to initalise the channel");
    }
    if (!isConnectedInternal())
    {
        termChannel();
        throw std::runtime_error("Channel not connected");
    }
}

ChannelImpl::~ChannelImpl()
{
    termChannel();
}

bool ChannelImpl::createConnectedSocket(const std::string &socketPath)
{
    int sock = socket(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
    if (sock < 0)
    {
        RIALTO_IPC_LOG_SYS_ERROR(errno, "failed to create socket");
        return false;
    }

    struct sockaddr_un addr = {0};
    memset(&addr, 0x00, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socketPath.c_str(), sizeof(addr.sun_path) - 1);

    if (::connect(sock, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) == -1)
    {
        RIALTO_IPC_LOG_SYS_ERROR(errno, "failed to connect to %s", socketPath.c_str());
        close(sock);
        return false;
    }

    m_sock = sock;

    return true;
}

bool ChannelImpl::attachSocket(int sockFd)
{
    // sanity check the supplied socket is of the right type
    struct sockaddr addr;
    socklen_t len = sizeof(addr);
    if ((getsockname(sockFd, &addr, &len) < 0) || (len < sizeof(sa_family_t)))
    {
        RIALTO_IPC_LOG_SYS_ERROR(errno, "failed to get name of supplied socket");
        return false;
    }
    if (addr.sa_family != AF_UNIX)
    {
        RIALTO_IPC_LOG_ERROR("supplied client socket is not a unix domain socket");
        return false;
    }

    int type = 0;
    len = sizeof(type);
    if ((getsockopt(sockFd, SOL_SOCKET, SO_TYPE, &type, &len) < 0) || (len != sizeof(type)))
    {
        RIALTO_IPC_LOG_SYS_ERROR(errno, "failed to get type of supplied socket");
        return false;
    }
    if (type != SOCK_SEQPACKET)
    {
        RIALTO_IPC_LOG_ERROR("supplied client socket is not of type SOCK_SEQPACKET");
        return false;
    }

    // set the O_NONBLOCKING flag on the socket
    int flags = fcntl(sockFd, F_GETFL);
    if ((flags < 0) || (fcntl(sockFd, F_SETFL, flags | O_NONBLOCK) < 0))
    {
        RIALTO_IPC_LOG_SYS_ERROR(errno, "failed to set socket to non-blocking mode");
        return false;
    }

    m_sock = sockFd;

    return true;
}

bool ChannelImpl::initChannel()
{
    // create epoll so can listen for timeouts as well as socket messages
    m_epollFd = epoll_create1(EPOLL_CLOEXEC);
    if (m_epollFd < 0)
    {
        RIALTO_IPC_LOG_SYS_ERROR(errno, "epoll failed");
        return false;
    }

    // add the socket to epoll
    epoll_event sockEvent = {.events = EPOLLIN, .data = {.fd = m_sock}};
    if (epoll_ctl(m_epollFd, EPOLL_CTL_ADD, m_sock, &sockEvent) != 0)
    {
        RIALTO_IPC_LOG_SYS_ERROR(errno, "epoll_ctl failed to add eventfd");
        return false;
    }

    m_timerFd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC | TFD_NONBLOCK);
    if (m_timerFd < 0)
    {
        RIALTO_IPC_LOG_SYS_ERROR(errno, "timerfd_create failed");
        return false;
    }

    // add the timer event to epoll
    epoll_event timerEvent = {.events = EPOLLIN, .data = {.fd = m_timerFd}};
    if (epoll_ctl(m_epollFd, EPOLL_CTL_ADD, m_timerFd, &timerEvent) != 0)
    {
        RIALTO_IPC_LOG_SYS_ERROR(errno, "epoll_ctl failed to add eventfd");
        return false;
    }

    // and lastly the eventfd to wake the poll loop
    m_eventFd = eventfd(0, EFD_CLOEXEC);
    if (m_eventFd < 0)
    {
        RIALTO_IPC_LOG_SYS_ERROR(errno, "eventfd create failed");
        return false;
    }

    // add the timer event to epoll
    epoll_event wakeEvent = {.events = EPOLLIN, .data = {.fd = m_eventFd}};
    if (epoll_ctl(m_epollFd, EPOLL_CTL_ADD, m_eventFd, &wakeEvent) != 0)
    {
        RIALTO_IPC_LOG_SYS_ERROR(errno, "epoll_ctl failed to add eventfd");
        return false;
    }

    return true;
}

void ChannelImpl::termChannel()
{
    // close the socket and the epoll and timer fds
    if (m_sock >= 0)
        disconnectNoLock();
    if ((m_epollFd >= 0) && (close(m_epollFd) != 0))
        RIALTO_IPC_LOG_SYS_ERROR(errno, "closing epoll fd failed");
    if ((m_timerFd >= 0) && (close(m_timerFd) != 0))
        RIALTO_IPC_LOG_SYS_ERROR(errno, "closing timer fd failed");
    if ((m_eventFd >= 0) && (close(m_eventFd) != 0))
        RIALTO_IPC_LOG_SYS_ERROR(errno, "closing event fd failed");

    // if any method calls are still outstanding then complete them with errors now
    for (auto &entry : m_methodCalls)
    {
        completeWithError(&entry.second, "Channel destructed");
    }

    m_methodCalls.clear();
}

void ChannelImpl::disconnect()
{
    // disconnect from the socket
    {
        std::lock_guard<std::mutex> locker(m_lock);

        if (m_sock < 0)
            return;

        disconnectNoLock();
    }

    // wake the wait(...) call so client code is blocked there it is woken
    if (m_eventFd >= 0)
    {
        uint64_t wakeup = 1;
        if (TEMP_FAILURE_RETRY(write(m_eventFd, &wakeup, sizeof(wakeup))) != sizeof(wakeup))
        {
            RIALTO_IPC_LOG_SYS_ERROR(errno, "failed to write to wake event fd");
        }
    }
}

// -----------------------------------------------------------------------------
/*!
    \internal


 */
void ChannelImpl::disconnectNoLock()
{
    if (m_sock < 0)
    {
        RIALTO_IPC_LOG_WARN("not connected\n");
        return;
    }

    // remove the socket from epoll
    if (epoll_ctl(m_epollFd, EPOLL_CTL_DEL, m_sock, nullptr) != 0)
    {
        RIALTO_IPC_LOG_SYS_ERROR(errno, "epoll_ctl failed to remove socket");
    }

    // shutdown and close the socket
    if (shutdown(m_sock, SHUT_RDWR) != 0)
    {
        RIALTO_IPC_LOG_SYS_ERROR(errno, "shutdown error");
    }
    if (close(m_sock) != 0)
    {
        RIALTO_IPC_LOG_SYS_ERROR(errno, "close error");
    }

    m_sock = -1;
}

bool ChannelImpl::isConnected() const
{
    return isConnectedInternal();
}

bool ChannelImpl::isConnectedInternal() const
{
    std::lock_guard<std::mutex> locker(m_lock);
    return (m_sock >= 0);
}

int ChannelImpl::fd() const
{
    return m_epollFd;
}

bool ChannelImpl::wait(int timeoutMSecs)
{
    if ((m_epollFd < 0) || !isConnected())
    {
        return false;
    }

    // wait for any event (with timeout)
    struct pollfd fds[2];
    fds[0].fd = m_epollFd;
    fds[0].events = POLLIN;

    int rc = TEMP_FAILURE_RETRY(poll(fds, 1, timeoutMSecs));
    if (rc < 0)
    {
        RIALTO_IPC_LOG_SYS_ERROR(errno, "poll failed?");
        return false;
    }

    return isConnected();
}

bool ChannelImpl::process()
{
    if (!isConnected())
        return false;

    struct epoll_event events[3];
    int rc = TEMP_FAILURE_RETRY(epoll_wait(m_epollFd, events, 3, 0));
    if (rc < 0)
    {
        RIALTO_IPC_LOG_SYS_ERROR(errno, "epoll_wait failed");
        return false;
    }

    enum
    {
        HaveSocketEvent = 0x1,
        HaveTimeoutEvent = 0x2,
        HaveWakeEvent = 0x4
    };
    unsigned eventsMask = 0;
    for (int i = 0; i < rc; i++)
    {
        if (events[i].data.fd == m_sock)
            eventsMask |= HaveSocketEvent;
        else if (events[i].data.fd == m_timerFd)
            eventsMask |= HaveTimeoutEvent;
        else if (events[i].data.fd == m_eventFd)
            eventsMask |= HaveWakeEvent;
    }

    if ((eventsMask & HaveSocketEvent) && !processSocketEvent())
    {
        std::map<uint64_t, MethodCall> callsToDelete;
        {
            std::lock_guard<std::mutex> locker(m_lock);
            callsToDelete = m_methodCalls;
            m_methodCalls.clear();
        }

        for (auto &entry : callsToDelete)
        {
            completeWithError(&entry.second, "Socket down");
        }
        return false;
    }

    if (eventsMask & HaveTimeoutEvent)
        processTimeoutEvent();

    if (eventsMask & HaveWakeEvent)
        processWakeEvent();

    return isConnected();
}

bool ChannelImpl::unsubscribe(int eventTag)
{
    std::lock_guard<std::mutex> locker(m_eventsLock);
    bool success = false;

    auto it = std::find_if(m_eventHandlers.begin(), m_eventHandlers.end(),
                           [&](const auto &item) { return item.second.id == eventTag; });
    if (m_eventHandlers.end() != it)
    {
        m_eventHandlers.erase(it);
        success = true;
    }

    return success;
}

// -----------------------------------------------------------------------------
/*!
    \internal


 */
bool ChannelImpl::processSocketEvent()
{
    static std::mutex bufLock;
    std::lock_guard<std::mutex> bufLocker(bufLock);

    static std::vector<uint8_t> dataBuf(kMaxMessageSize);
    static std::vector<uint8_t> ctrlBuf(CMSG_SPACE(SCM_MAX_FD * sizeof(int)));

    // read all messages from the client socket, we break out if the socket is closed
    // or EWOULDBLOCK is returned on a read (ie. no more messages to read)
    while (true)
    {
        struct msghdr msg = {nullptr};
        struct iovec io = {.iov_base = dataBuf.data(), .iov_len = dataBuf.size()};

        bzero(&msg, sizeof(msg));
        msg.msg_iov = &io;
        msg.msg_iovlen = 1;
        msg.msg_control = ctrlBuf.data();
        msg.msg_controllen = ctrlBuf.size();

        // read one message
        ssize_t rd = TEMP_FAILURE_RETRY(recvmsg(m_sock, &msg, MSG_CMSG_CLOEXEC));
        if (rd < 0)
        {
            if (errno != EWOULDBLOCK)
            {
                RIALTO_IPC_LOG_SYS_ERROR(errno, "error reading client socket");

                std::lock_guard<std::mutex> locker(m_lock);
                disconnectNoLock();
                return false;
            }

            break;
        }
        else if (rd == 0)
        {
            // server closed connection, and we've read all data
            RIALTO_IPC_LOG_INFO("socket remote end closed, disconnecting channel");

            std::lock_guard<std::mutex> locker(m_lock);
            disconnectNoLock();
            return false;
        }
        else if (msg.msg_flags & (MSG_TRUNC | MSG_CTRUNC))
        {
            RIALTO_IPC_LOG_WARN("received truncated message from server, discarding");

            // make sure to close all the fds, otherwise we'll leak them, this
            // will read the fds and return in a vector, which will then be
            // destroyed, closing all the fds
            readMessageFds(&msg, 16);
        }
        else
        {
            // if there is control data then assume fd(s) have been passed
            std::vector<FileDescriptor> fds;
            if (msg.msg_controllen > 0)
            {
                fds = readMessageFds(&msg, 32);
            }

            // process the message from the server
            processServerMessage(dataBuf.data(), rd, &fds);
        }
    }

    return true;
}

// -----------------------------------------------------------------------------
/*!
    \internal

    Called from process() to check if the timerfd has expired and if so cancel
    the any outstanding method calls that have now timed-out.

 */
void ChannelImpl::processTimeoutEvent()
{
    // read the timerfd to clear any expirations
    uint64_t expirations;
    ssize_t rd = TEMP_FAILURE_RETRY(read(m_timerFd, &expirations, sizeof(expirations)));
    if (rd < 0)
    {
        if (errno != EWOULDBLOCK)
            RIALTO_IPC_LOG_SYS_ERROR(errno, "error reading timerfd");
        return;
    }

    // check if any method call has no expired
    std::unique_lock<std::mutex> locker(m_lock);

    // stores the timed-out method calls
    std::vector<MethodCall> timedOuts;

    // remove the method calls that have expired
    const auto kNow = std::chrono::steady_clock::now();
    auto it = m_methodCalls.begin();
    while (it != m_methodCalls.end())
    {
        if (kNow >= it->second.timeoutDeadline)
        {
            timedOuts.emplace_back(it->second);
            it = m_methodCalls.erase(it);
        }
        else
        {
            ++it;
        }
    }

    // if we still have method calls available, then re-calculate the timer for the next timeout
    if (!m_methodCalls.empty())
    {
        updateTimeoutTimer();
    }

    // drop the lock and now terminate the timed out method calls
    locker.unlock();

    for (auto &call : timedOuts)
    {
        completeWithError(&call, "Timed out");
    }
}

// -----------------------------------------------------------------------------
/*!
    \internal

    Called from process() when the eventfd was used to wake the event loop. All
    the function does is read the eventfd to clear it's value.

 */
void ChannelImpl::processWakeEvent()
{
    uint64_t ignore;
    if (TEMP_FAILURE_RETRY(read(m_eventFd, &ignore, sizeof(ignore))) != sizeof(ignore))
    {
        RIALTO_IPC_LOG_SYS_ERROR(errno, "Failed to read wake eventfd to clear it");
    }
}

// -----------------------------------------------------------------------------
/*!
    \internal

    Updates the timerfd to the time of the next method call timeout.  If method
    calls are pending then the timer is disabled.

    This should be called whenever a new method is called or a method has
    completed.

    \note Must be called while holding the m_lock mutex.

 */
void ChannelImpl::updateTimeoutTimer()
{
    struct itimerspec ts = {{0}};

    // if no method calls then just disarm the timer
    if (!m_methodCalls.empty())
    {
        // otherwise, find the next soonest timeout
        std::chrono::steady_clock::time_point nextTimeout = std::chrono::steady_clock::time_point::max();
        auto nextTimeoutCall =
            std::min_element(m_methodCalls.begin(), m_methodCalls.end(), [](const auto &elem, const auto &currentMin)
                             { return elem.second.timeoutDeadline < currentMin.second.timeoutDeadline; });
        if (nextTimeoutCall != m_methodCalls.end())
        {
            nextTimeout = nextTimeoutCall->second.timeoutDeadline;
        }

        // set the timerfd to the next duration
        const std::chrono::microseconds kDuration =
            std::chrono::duration_cast<std::chrono::microseconds>(nextTimeout - std::chrono::steady_clock::now());
        if (kDuration <= std::chrono::microseconds::zero())
        {
            ts.it_value.tv_nsec = 1000;
        }
        else
        {
            ts.it_value.tv_sec = static_cast<time_t>(std::chrono::duration_cast<std::chrono::seconds>(kDuration).count());
            ts.it_value.tv_nsec = static_cast<int32_t>((kDuration.count() % 1000000) * 1000);
        }

        RIALTO_IPC_LOG_DEBUG("next timeout in %" PRId64 "us - %ld.%09lds", kDuration.count(), ts.it_value.tv_sec,
                             ts.it_value.tv_nsec);
    }

    // write the timeout value
    if (timerfd_settime(m_timerFd, 0, &ts, nullptr) != 0)
    {
        RIALTO_IPC_LOG_SYS_ERROR(errno, "failed to write to timerfd");
    }
}

// -----------------------------------------------------------------------------
/*!
    \internal

    Processes a single message from the server, it may be a method call response
    or an event.


 */
void ChannelImpl::processServerMessage(const uint8_t *data, size_t dataLen, std::vector<FileDescriptor> *fds)
{
    // parse the message
    transport::MessageFromServer message;
    if (!message.ParseFromArray(data, static_cast<int>(dataLen)))
    {
        RIALTO_IPC_LOG_ERROR("invalid message from server");
        return;
    }

    // check if an event or a reply to a request
    if (message.has_reply())
    {
        processReplyFromServer(message.reply(), fds);
    }
    else if (message.has_error())
    {
        processErrorFromServer(message.error());
    }
    else if (message.has_event())
    {
        processEventFromServer(message.event(), fds);
    }
    else
    {
        RIALTO_IPC_LOG_ERROR("message from server is missing reply or event type");
    }
}

// -----------------------------------------------------------------------------
/*!
    \internal


 */
void ChannelImpl::processReplyFromServer(const transport::MethodCallReply &reply, std::vector<FileDescriptor> *fds)
{
    RIALTO_IPC_LOG_DEBUG("processing reply from server");

    std::unique_lock<std::mutex> locker(m_lock);

    // find the original request
    const uint64_t kSerialId = reply.reply_id();
    auto it = m_methodCalls.find(kSerialId);
    if (it == m_methodCalls.end())
    {
        RIALTO_IPC_LOG_ERROR("failed to find request for received reply with id %" PRIu64 "", reply.reply_id());
        return;
    }

    // take the method call and remove from the map of outstanding calls
    MethodCall methodCall = it->second;
    m_methodCalls.erase(it);

    // update the timeout timer now a method call has been processed
    updateTimeoutTimer();

    // can now drop the lock
    locker.unlock();

    // this is an actual reply so try and read it
    if (!methodCall.response->ParseFromString(reply.reply_message()))
    {
        RIALTO_IPC_LOG_ERROR("failed to parse method reply from server");
        completeWithError(&methodCall, "Failed to parse reply message");
    }
    else if (!addReplyFileDescriptors(methodCall.response, fds))
    {
        RIALTO_IPC_LOG_ERROR("mismatch of file descriptors to the reply");
        completeWithError(&methodCall, "Mismatched file descriptors in message");
    }
    else if (methodCall.closure)
    {
        RIALTO_IPC_LOG_DEBUG("reply{ serial %" PRIu64 " } - %s { %s }", kSerialId,
                             methodCall.response->GetTypeName().c_str(), methodCall.response->ShortDebugString().c_str());

        complete(&methodCall);
    }
}

// -----------------------------------------------------------------------------
/*!
    \internal


 */
void ChannelImpl::processErrorFromServer(const transport::MethodCallError &error)
{
    RIALTO_IPC_LOG_DEBUG("processing error from server");

    std::unique_lock<std::mutex> locker(m_lock);

    // find the original request
    const uint64_t kSerialId = error.reply_id();
    auto it = m_methodCalls.find(kSerialId);
    if (it == m_methodCalls.end())
    {
        RIALTO_IPC_LOG_ERROR("failed to find request for received reply with id %" PRIu64 "", error.reply_id());
        return;
    }

    // take the method call and remove from the map of outstanding calls
    MethodCall methodCall = it->second;
    m_methodCalls.erase(it);

    // update the timeout timer now a method call has been processed
    updateTimeoutTimer();

    // can now drop the lock
    locker.unlock();

    RIALTO_IPC_LOG_DEBUG("error{ serial %" PRIu64 " } - %s", kSerialId, error.error_reason().c_str());

    // complete the call with an error
    completeWithError(&methodCall, error.error_reason());
}

// -----------------------------------------------------------------------------
/*!
    \internal


 */
void ChannelImpl::processEventFromServer(const transport::EventFromServer &event, std::vector<FileDescriptor> *fds)
{
    RIALTO_IPC_LOG_DEBUG("processing event from server");

    const std::string &kEventName = event.event_name();

    std::lock_guard<std::mutex> locker(m_eventsLock);

    auto range = m_eventHandlers.equal_range(kEventName);
    if (range.first == range.second)
    {
        RIALTO_IPC_LOG_WARN("no handler for event %s", kEventName.c_str());
        return;
    }

    const google::protobuf::Descriptor *kDescriptor = range.first->second.descriptor;

    const google::protobuf::Message *kPrototype =
        google::protobuf::MessageFactory::generated_factory()->GetPrototype(kDescriptor);
    if (!kPrototype)
    {
        RIALTO_IPC_LOG_ERROR("failed to create prototype for event %s", kEventName.c_str());
        return;
    }

    std::shared_ptr<google::protobuf::Message> message(kPrototype->New());
    if (!message)
    {
        RIALTO_IPC_LOG_ERROR("failed to create mutable message from prototype");
        return;
    }

    if (!message->ParseFromString(event.message()))
    {
        RIALTO_IPC_LOG_ERROR("failed to parse message for event %s", kEventName.c_str());
    }
    else if (!addReplyFileDescriptors(message.get(), fds))
    {
        RIALTO_IPC_LOG_ERROR("mismatch of file descriptors to the reply");
    }
    else
    {
        RIALTO_IPC_LOG_DEBUG("event{ %s } - %s { %s }", kEventName.c_str(), message->GetTypeName().c_str(),
                             message->ShortDebugString().c_str());

        for (auto it = range.first; it != range.second; ++it)
        {
            it->second.handler(message);
        }
    }
}

// -----------------------------------------------------------------------------
/*!
    \internal
    \static

    Reads all the file descriptors from a unix domain socket received \a msg.
    It returns the file descriptors as a vector of FileDescriptor objects,
    these objects safely store the fd and close them when they're destructed.

    The \a limit specifies the maximum number of fds to store, if more were
    sent then they are automatically closed and not returned in the vector.

 */
std::vector<FileDescriptor> ChannelImpl::readMessageFds(const struct msghdr *msg, size_t limit)
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
                        RIALTO_IPC_LOG_ERROR(
                            "received to many file descriptors, exceeding max per message, closing left overs");
                    }
                    else
                    {
                        firebolt::rialto::ipc::FileDescriptor fileDescriptor(kFds[i]);
                        if (!fileDescriptor.isValid())
                        {
                            RIALTO_IPC_LOG_ERROR("received invalid fd (couldn't dup)");
                        }
                        else
                        {
                            fds.emplace_back(std::move(fileDescriptor));
                        }
                    }

                    if (close(kFds[i]) != 0)
                        RIALTO_IPC_LOG_SYS_ERROR(errno, "failed to close received fd");
                }
            }
        }
    }

    return fds;
}

// -----------------------------------------------------------------------------
/*!
    \static
    \internal

    Places the received file descriptors into the protobuf message.

    It works by iterating over the fields in the message, finding ones that are
    marked as 'field_is_fd' and then replacing the received integer value with
    an actual file descriptor.

 */
bool ChannelImpl::addReplyFileDescriptors(google::protobuf::Message *reply,
                                          std::vector<firebolt::rialto::ipc::FileDescriptor> *fds)
{
    auto fdIterator = fds->begin();

    const google::protobuf::Descriptor *kDescriptor = reply->GetDescriptor();
    const google::protobuf::Reflection *kReflection = nullptr;

    const int n = kDescriptor->field_count();
    for (int i = 0; i < n; i++)
    {
        auto fieldDescriptor = kDescriptor->field(i);
        if (fieldDescriptor->options().HasExtension(::firebolt::rialto::ipc::field_is_fd) &&
            fieldDescriptor->options().GetExtension(::firebolt::rialto::ipc::field_is_fd))
        {
            if (fieldDescriptor->type() != google::protobuf::FieldDescriptor::TYPE_INT32)
            {
                RIALTO_IPC_LOG_ERROR("field is marked as containing an fd but not an int32 type");
                return false;
            }

            if (!kReflection)
            {
                kReflection = reply->GetReflection();
            }

            if (kReflection->HasField(*reply, fieldDescriptor))
            {
                if (fdIterator == fds->end())
                {
                    RIALTO_IPC_LOG_ERROR("field is marked as containing an fd but none or too few were supplied");
                    return false;
                }

                kReflection->SetInt32(reply, fieldDescriptor, fdIterator->fd());
                ++fdIterator;
            }
        }
    }

    if (fdIterator != fds->end())
    {
        RIALTO_IPC_LOG_ERROR("received too many file descriptors in the message");
        return false;
    }

    // we now need to release all the fds stored in the vector, otherwise they
    // will be closed when the vector is destroyed.  From now onwards it is the
    // caller's responsibility to close the fds in the returned protobuf message
    // object
    for (firebolt::rialto::ipc::FileDescriptor &fd : *fds)
    {
        fd.release();
    }

    return true;
}

// -----------------------------------------------------------------------------
/*!
    \internal
    \static


 */
void ChannelImpl::complete(MethodCall *call)
{
    if (call->closure)
    {
        call->closure->Run();
        call->closure = nullptr;
    }
}

// -----------------------------------------------------------------------------
/*!
    \internal
    \static


 */
void ChannelImpl::completeWithError(MethodCall *call, std::string reason)
{
    RIALTO_IPC_LOG_DEBUG("completing method call with error '%s'", reason.c_str());

    if (call->controller)
    {
        call->controller->setMethodCallFailed(std::move(reason));
        call->controller = nullptr;
    }

    if (call->closure)
    {
        call->closure->Run();
        call->closure = nullptr;
    }
}

// -----------------------------------------------------------------------------
/*!
    \static

    Iterates through the message and finds any file descriptor type fields, if
    found it adds the fds to the returned vector.

 */
std::vector<int> ChannelImpl::getMessageFds(const google::protobuf::Message &message)
{
    std::vector<int> fds;

    auto descriptor = message.GetDescriptor();
    const int n = descriptor->field_count();
    for (int i = 0; i < n; i++)
    {
        auto fieldDescriptor = descriptor->field(i);
        if (fieldDescriptor->options().HasExtension(::firebolt::rialto::ipc::field_is_fd) &&
            fieldDescriptor->options().GetExtension(::firebolt::rialto::ipc::field_is_fd))
        {
            if (fieldDescriptor->type() != google::protobuf::FieldDescriptor::TYPE_INT32)
            {
                RIALTO_IPC_LOG_ERROR("field '%s' is marked as containing an fd but not an int32 type",
                                     fieldDescriptor->full_name().c_str());
            }
            else
            {
                auto reflection = message.GetReflection();
                int fileDescriptor = reflection->GetInt32(message, fieldDescriptor);
                fds.emplace_back(fileDescriptor);
            }
        }
    }

    return fds;
}

// -----------------------------------------------------------------------------
/*!
    \overload

    \quote
        Call the given method of the remote service.  The signature of this
        procedure looks the same as Service::CallMethod(), but the requirements
        are less strict in one important way:  the request and response objects
        need not be of any specific class as long as their descriptors are
        method->input_type() and method->output_type().

 */
void ChannelImpl::CallMethod(const google::protobuf::MethodDescriptor *method, // NOLINT(build/function_format)
                             google::protobuf::RpcController *controller, const google::protobuf::Message *request,
                             google::protobuf::Message *response, google::protobuf::Closure *done)
{
    MethodCall methodCall{std::chrono::steady_clock::now() + m_timeout,
                          dynamic_cast<ClientControllerImpl *>(controller), response, done};

    //
    const uint64_t kSerialId = m_serialCounter++;

    // create the transport request
    transport::MessageToServer message;
    transport::MethodCall *call = message.mutable_call();
    call->set_serial_id(kSerialId);
    call->set_service_name(method->service()->full_name());
    call->set_method_name(method->name());

    // copy in the actual message data
    std::string reqString = request->SerializeAsString();
    call->set_request_message(std::move(reqString));

    const size_t kRequiredDataLen = message.ByteSizeLong();
    if (kRequiredDataLen > kMaxMessageSize)
    {
        RIALTO_IPC_LOG_ERROR("method call to big to send (%zu, max %zu", kRequiredDataLen, kMaxMessageSize);
        completeWithError(&methodCall, "Method call to big");
        return;
    }

    // extract the fds from the message
    const std::vector<int> kFds = getMessageFds(*request);
    const size_t kRequiredCtrlLen = kFds.empty() ? 0 : CMSG_SPACE(sizeof(int) * kFds.size());

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

    // next check if the request is sending any fd's
    if (!kFds.empty())
    {
        struct cmsghdr *cmsg = CMSG_FIRSTHDR(header);
        if (!cmsg)
        {
            RIALTO_IPC_LOG_ERROR("odd, failed to get the first cmsg header");
            completeWithError(&methodCall, "Internal error");
            return;
        }

        cmsg->cmsg_level = SOL_SOCKET;
        cmsg->cmsg_type = SCM_RIGHTS;
        cmsg->cmsg_len = CMSG_LEN(sizeof(int) * kFds.size());
        memcpy(CMSG_DATA(cmsg), kFds.data(), sizeof(int) * kFds.size());
        header->msg_controllen = cmsg->cmsg_len;
    }

    // check if the method is expecting a reply
    const bool kNoReplyExpected = method->options().HasExtension(::firebolt::rialto::ipc::no_reply) &&
                                  method->options().GetExtension(::firebolt::rialto::ipc::no_reply);

    // finally, send the message
    std::unique_lock<std::mutex> locker(m_lock);

    if (m_sock < 0)
    {
        locker.unlock();
        completeWithError(&methodCall, "Not connected");
    }
    else if (sendmsg(m_sock, header, MSG_NOSIGNAL) != static_cast<ssize_t>(kRequiredDataLen))
    {
        locker.unlock();
        completeWithError(&methodCall, "Failed to send message");
    }
    else
    {
        RIALTO_IPC_LOG_DEBUG("call{ serial %" PRIu64 " } - %s.%s { %s }", kSerialId, call->service_name().c_str(),
                             call->method_name().c_str(), request->ShortDebugString().c_str());

        if (kNoReplyExpected)
        {
            // no reply from server is expected, however if the caller supplied
            // a closure (it shouldn't) we should still call it now to indicate
            // the method call has been made
            if (done)
                done->Run();
        }
        else
        {
            // add the message to the queue so we pick-up the reply
            m_methodCalls.emplace(kSerialId, methodCall);

            // update the single timeout timer
            updateTimeoutTimer();
        }
    }
}

int ChannelImpl::subscribeImpl(const std::string &kEventName, const google::protobuf::Descriptor *descriptor,
                               EventHandler &&handler)
{
    std::lock_guard<std::mutex> locker(m_eventsLock);

    const int kTag = m_eventTagCounter++;
    m_eventHandlers.emplace(kEventName, Event{kTag, descriptor, std::move(handler)});

    return kTag;
}
}; // namespace firebolt::rialto::ipc
