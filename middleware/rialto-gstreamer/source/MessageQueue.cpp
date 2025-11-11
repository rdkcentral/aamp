/*
 * Copyright (C) 2023   
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

#include "MessageQueue.h"
#include "GstreamerCatLog.h"
#define GST_CAT_DEFAULT rialtoGStreamerCat
CallInEventLoopMessage::CallInEventLoopMessage(const std::function<void()> &func) : m_func(func), m_done{false} {}

void CallInEventLoopMessage::handle()
{
    std::unique_lock<std::mutex> lock(m_callInEventLoopMutex);
    m_func();
    m_done = true;
    m_callInEventLoopCondVar.notify_all();
}

void CallInEventLoopMessage::skip()
{
    std::unique_lock<std::mutex> lock(m_callInEventLoopMutex);
    m_done = true;
    m_callInEventLoopCondVar.notify_all();
}

void CallInEventLoopMessage::wait()
{
    std::unique_lock<std::mutex> lock(m_callInEventLoopMutex);
    m_callInEventLoopCondVar.wait(lock, [this]() { return m_done; });
}

ScheduleInEventLoopMessage::ScheduleInEventLoopMessage(const std::function<void()> &func) : m_func(func) {}

void ScheduleInEventLoopMessage::handle()
{
    m_func();
}

std::shared_ptr<IMessageQueueFactory> IMessageQueueFactory::createFactory()
{
    return std::make_shared<MessageQueueFactory>();
}

std::unique_ptr<IMessageQueue> MessageQueueFactory::createMessageQueue() const
{
    return std::make_unique<rialto::MessageQueue>();
}

namespace rialto
{
MessageQueue::MessageQueue() : m_running(false) {}

MessageQueue::~MessageQueue()
{
    doStop();
}

void MessageQueue::start()
{
    if (m_running)
    {
        // queue is running
        return;
    }
    m_running = true;
    std::thread startThread(&MessageQueue::processMessages, this);
    m_workerThread.swap(startThread);
}

void MessageQueue::stop()
{
    doStop();
}

void MessageQueue::clear()
{
    doClear();
}

std::shared_ptr<Message> MessageQueue::waitForMessage()
{
    std::unique_lock<std::mutex> lock(m_mutex);
    while (m_queue.empty())
    {
        m_condVar.wait(lock);
    }
    std::shared_ptr<Message> message = m_queue.front();
    m_queue.pop_front();
    return message;
}

bool MessageQueue::postMessage(const std::shared_ptr<Message> &msg)
{
    const std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_running)
    {
        GST_ERROR("Message queue is not running");
        return false;
    }
    m_queue.push_back(msg);
    m_condVar.notify_all();

    return true;
}

void MessageQueue::processMessages()
{
    do
    {
        std::shared_ptr<Message> message = waitForMessage();
        message->handle();
    } while (m_running);
}

bool MessageQueue::scheduleInEventLoop(const std::function<void()> &func)
{
    auto message = std::make_shared<ScheduleInEventLoopMessage>(func);
    if (!postMessage(message))
    {
        return false;
    }

    return true;
}

bool MessageQueue::callInEventLoop(const std::function<void()> &func)
{
    return callInEventLoopInternal(func);
}

bool MessageQueue::callInEventLoopInternal(const std::function<void()> &func)
{
    if (std::this_thread::get_id() != m_workerThread.get_id())
    {
        auto message = std::make_shared<CallInEventLoopMessage>(func);
        if (!postMessage(message))
        {
            return false;
        }
        message->wait();
    }
    else
    {
        func();
    }

    return true;
}

void MessageQueue::doStop()
{
    if (!m_running)
    {
        // queue is not running
        return;
    }
    callInEventLoopInternal([this]() { m_running = false; });

    if (m_workerThread.joinable())
        m_workerThread.join();

    doClear();
}

void MessageQueue::doClear()
{
    std::unique_lock<std::mutex> lock(m_mutex);
    while (!m_queue.empty())
    {
        m_queue.front()->skip();
        m_queue.pop_front();
    }
}
} // namespace rialto
