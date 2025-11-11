/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2022 
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

#include "MainThread.h"
#include "RialtoServerLogging.h"
#include <string>
#include <utility>
#include <vector>

namespace firebolt::rialto::server
{
std::weak_ptr<IMainThread> MainThreadFactory::m_mainThread;
std::mutex MainThreadFactory::m_creationMutex;

std::shared_ptr<IMainThreadFactory> IMainThreadFactory::createFactory()
{
    std::shared_ptr<IMainThreadFactory> factory;
    try
    {
        factory = std::make_shared<MainThreadFactory>();
    }
    catch (const std::exception &e)
    {
        RIALTO_SERVER_LOG_ERROR("Failed to create the main thread factory, reason: %s", e.what());
    }

    return factory;
}

std::shared_ptr<IMainThread> MainThreadFactory::getMainThread() const
{
    std::lock_guard<std::mutex> lock{m_creationMutex};

    std::shared_ptr<IMainThread> mainThread = m_mainThread.lock();
    if (!mainThread)
    {
        try
        {
            mainThread = std::make_shared<MainThread>();
        }
        catch (const std::exception &e)
        {
            RIALTO_SERVER_LOG_ERROR("Failed to create the main thread, reason: %s", e.what());
        }

        m_mainThread = mainThread;
    }

    return mainThread;
}

MainThread::MainThread() : m_isMainThreadRunning{true}, m_mainThreadClientId{0}, m_nextClientId{1}
{
    RIALTO_SERVER_LOG_DEBUG("MainThread is constructed");
    m_thread = std::thread(std::bind(&MainThread::mainThreadLoop, this));

    // Register itself
    m_registeredClients.insert(m_mainThreadClientId);
}

MainThread::~MainThread()
{
    RIALTO_SERVER_LOG_DEBUG("MainThread is destructed");
    auto shutdownTask = [this]() { m_isMainThreadRunning = false; };
    enqueueTask(m_mainThreadClientId, shutdownTask);
    m_thread.join();
}

void MainThread::mainThreadLoop()
{
    while (m_isMainThreadRunning)
    {
        const std::shared_ptr<TaskInfo> kTaskInfo = waitForTask();
        if (m_registeredClients.find(kTaskInfo->clientId) != m_registeredClients.end())
        {
            kTaskInfo->task();
        }
        else
        {
            RIALTO_SERVER_LOG_WARN("Task ignored, client '%d' not registered", kTaskInfo->clientId);
        }

        if (nullptr != kTaskInfo->cv)
        {
            std::unique_lock<std::mutex> lockTask(*(kTaskInfo->mutex));
            kTaskInfo->cv->notify_one();
        }
    }
}

const std::shared_ptr<MainThread::TaskInfo> MainThread::waitForTask()
{
    std::unique_lock<std::mutex> lock(m_taskQueueMutex);
    if (m_taskQueue.empty())
    {
        m_taskQueueCv.wait(lock, [this] { return !m_taskQueue.empty(); });
    }
    const std::shared_ptr<TaskInfo> kTaskInfo = m_taskQueue.front();
    m_taskQueue.pop_front();
    return kTaskInfo;
}

int32_t MainThread::registerClient()
{
    uint32_t clientId = m_nextClientId++;

    auto task = [&, clientId]()
    {
        RIALTO_SERVER_LOG_INFO("Registering client '%u'", clientId);
        m_registeredClients.insert(clientId);
    };
    enqueueTask(m_mainThreadClientId, task);

    return clientId;
}

void MainThread::unregisterClient(uint32_t clientId)
{
    RIALTO_SERVER_LOG_INFO("Unregistering client '%u'", clientId);
    m_registeredClients.erase(clientId);
}

void MainThread::enqueueTask(uint32_t clientId, Task task)
{
    std::shared_ptr<TaskInfo> newTask = std::make_shared<TaskInfo>();
    newTask->clientId = clientId;
    newTask->task = task;
    {
        std::unique_lock<std::mutex> lock(m_taskQueueMutex);
        m_taskQueue.push_back(newTask);
    }
    m_taskQueueCv.notify_one();
}

void MainThread::enqueueTaskAndWait(uint32_t clientId, Task task)
{
    std::shared_ptr<TaskInfo> newTask = std::make_shared<TaskInfo>();
    newTask->clientId = clientId;
    newTask->task = task;
    newTask->mutex = std::make_unique<std::mutex>();
    newTask->cv = std::make_unique<std::condition_variable>();

    {
        std::unique_lock<std::mutex> lockTask(*(newTask->mutex));
        {
            std::unique_lock<std::mutex> lockQueue(m_taskQueueMutex);
            m_taskQueue.push_back(newTask);
        }
        m_taskQueueCv.notify_one();

        newTask->cv->wait(lockTask);
    }
}

void MainThread::enqueuePriorityTaskAndWait(uint32_t clientId, Task task)
{
    std::shared_ptr<TaskInfo> newTask = std::make_shared<TaskInfo>();
    newTask->clientId = clientId;
    newTask->task = task;
    newTask->mutex = std::make_unique<std::mutex>();
    newTask->cv = std::make_unique<std::condition_variable>();

    {
        std::unique_lock<std::mutex> lockTask(*(newTask->mutex));
        {
            std::unique_lock<std::mutex> lockQueue(m_taskQueueMutex);
            m_taskQueue.push_front(newTask);
        }
        m_taskQueueCv.notify_one();

        newTask->cv->wait(lockTask);
    }
}
} // namespace firebolt::rialto::server
