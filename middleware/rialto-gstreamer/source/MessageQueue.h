/*
 * Copyright (C) 2022   
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#pragma once

#include "IMessageQueue.h"
#include <condition_variable>
#include <deque>
#include <functional>
#include <gst/gst.h>
#include <memory>
#include <mutex>
#include <thread>

class CallInEventLoopMessage : public Message
{
public:
    explicit CallInEventLoopMessage(const std::function<void()> &func);
    void handle() override;
    void skip() override;
    void wait();

private:
    const std::function<void()> m_func;
    std::mutex m_callInEventLoopMutex;
    std::condition_variable m_callInEventLoopCondVar;
    bool m_done;
};

class ScheduleInEventLoopMessage : public Message
{
public:
    explicit ScheduleInEventLoopMessage(const std::function<void()> &func);
    void handle() override;

private:
    const std::function<void()> m_func;
};

class MessageQueueFactory : public IMessageQueueFactory
{
public:
    std::unique_ptr<IMessageQueue> createMessageQueue() const override;
};
namespace rialto
{
class MessageQueue : public IMessageQueue
{
public:
    MessageQueue();
    ~MessageQueue();

    void start() override;
    void stop() override;
    void clear() override;
    // Wait for a message to appear on the queue.
    std::shared_ptr<Message> waitForMessage() override;
    // Posts a message to the queue.
    bool postMessage(const std::shared_ptr<Message> &msg) override;
    void processMessages() override;
    bool scheduleInEventLoop(const std::function<void()> &func) override;
    bool callInEventLoop(const std::function<void()> &func) override;

protected:
    void doStop();
    void doClear();
    // We need to have a non-virtual method, which can be called in class destructor
    bool callInEventLoopInternal(const std::function<void()> &func);

protected:
    std::condition_variable m_condVar;
    std::mutex m_mutex;
    std::deque<std::shared_ptr<Message>> m_queue;
    std::thread m_workerThread;
    bool m_running;
};
} // namespace rialto
