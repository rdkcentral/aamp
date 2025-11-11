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

#pragma once

#include <functional>
#include <memory>

class Message
{
public:
    virtual ~Message() {}
    virtual void handle() = 0;
    virtual void skip() {};
};

class IMessageQueue
{
public:
    virtual ~IMessageQueue() = default;
    virtual void start() = 0;
    virtual void stop() = 0;
    virtual void clear() = 0;
    virtual std::shared_ptr<Message> waitForMessage() = 0;
    virtual bool postMessage(const std::shared_ptr<Message> &msg) = 0;
    virtual void processMessages() = 0;
    virtual bool scheduleInEventLoop(const std::function<void()> &func) = 0;
    virtual bool callInEventLoop(const std::function<void()> &func) = 0;
};

class IMessageQueueFactory
{
public:
    virtual ~IMessageQueueFactory() = default;
    static std::shared_ptr<IMessageQueueFactory> createFactory();
    virtual std::unique_ptr<IMessageQueue> createMessageQueue() const = 0;
};
