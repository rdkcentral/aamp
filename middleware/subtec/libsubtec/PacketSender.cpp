/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2025 RDK Management
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

#include <chrono>

#include "SubtecPacket.hpp"
#include "PacketSender.hpp"
#include "PlayerLogManager.h" // Included for MW_LOG

#define MAX_SNDBUF_SIZE (8*1024*1024)

void runWorkerTask(void *ctx)
{
    try {
        PacketSender *pkt = reinterpret_cast<PacketSender*>(ctx);
        pkt->senderTask();
    }
    catch (const std::exception& e) {
        MW_LOG_WARN("PacketSender: Error in run %s", e.what());
    }
}

PacketSender *PacketSender::Instance()
{
    static PacketSender instance;
    return &instance;
}

PacketSender::~PacketSender()
{
    PacketSender::Close();
}

void PacketSender::Close()
{
    closeSenderTask();
    if (mSubtecSocketHandle)
        ::close(mSubtecSocketHandle);
    mSubtecSocketHandle = 0;
}

void PacketSender::Flush()
{
    flushPacketQueue();
}

bool PacketSender::Init()
{
    return Init(SOCKET_PATH);
}

bool PacketSender::Init(const char *socket_path)
{
    bool ret = true;
    std::unique_lock<std::mutex> lock(mStartMutex);

    MW_LOG_INFO("PacketSender::Init with %s", socket_path);

    if (!running)
    {
        mSocketPath = socket_path;
        ret = initSocket(socket_path) && initSenderTask();
        if (!ret) {
            MW_LOG_WARN("SenderTask failed to init");
        }
        else
        MW_LOG_WARN("senderTask started");
    }
    else
    MW_LOG_WARN("PacketSender::Init already running");
        
    return ret;
}

void PacketSender::SendPacket(PacketPtr && packet)
{
    std::unique_lock<std::mutex> lock(mPktMutex);
    uint32_t type = packet->getType();
    std::string typeString = Packet::getTypeString(type);
    MW_LOG_TRACE("PacketSender:  queue size %zu type %s:%d counter:%d",
        mPacketQueue.size(), typeString.c_str(), type, packet->getCounter());

    mPacketQueue.push(std::move(packet));
    mCv.notify_all();
}

void PacketSender::senderTask()
{
    std::unique_lock<std::mutex> lock(mPktMutex);
    do {
        running = true;
        mCv.wait(lock);
        while (!mPacketQueue.empty())
        {
            sendPacket(std::move(mPacketQueue.front()));
            mPacketQueue.pop();
            MW_LOG_TRACE("PacketSender:  queue size %zu", mPacketQueue.size());
        }
    } while(running);
}

bool PacketSender::IsRunning()
{
    std::unique_lock<std::mutex> lock(mPktMutex);
    return running.load();
}

void PacketSender::flushPacketQueue()
{
    std::queue<PacketPtr> empty;
    std::unique_lock<std::mutex> lock(mPktMutex);

    empty.swap(mPacketQueue);
}

void PacketSender::sendPacket(PacketPtr && pkt)
{
    if(!pkt)
    {
        MW_LOG_ERR("PacketSender: pkt is null pointer");
	 return;
    }
    auto buffer = pkt->getBytes();
    size_t size =  static_cast<ssize_t>(buffer.size());
    if (size > mSockBufSize && size < MAX_SNDBUF_SIZE)
    {
	int newSize = (int)buffer.size();
	if (::setsockopt(mSubtecSocketHandle, SOL_SOCKET, SO_SNDBUF, &newSize, sizeof(newSize)) == -1)
	{
        MW_LOG_WARN("::setsockopt() SO_SNDBUF failed");
	}
	else
	{
	    mSockBufSize = newSize;
	    MW_LOG_INFO("new socket buffer size %d", mSockBufSize);
	}
    }
    auto written = ::write(mSubtecSocketHandle, &buffer[0], size);
    MW_LOG_TRACE("PacketSender: Written %ld bytes with size %zu", static_cast<long>(written), size);

    //Socket reconnect in case packet write fails
    if (written == -1) {
        mPktWriteFailCtr++;
        MW_LOG_TRACE("PacketSender: Write returned -1 with error: %s", strerror(errno));
    } else {
        mPktWriteFailCtr = 0;
    }

    //Try reconnect after every 5 failed packet writes
    if (mPktWriteFailCtr > 5) {
        MW_LOG_INFO("PacketSender: Written is -1 for over 5 consecutive packets. Try to reconnect socket");

        struct sockaddr_un addr;

        (void) std::memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        (void) std::strncpy(addr.sun_path, mSocketPath.c_str(), sizeof(addr.sun_path));
        addr.sun_path[sizeof(addr.sun_path) - 1] = 0;

        if (::connect(mSubtecSocketHandle, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) != 0) {
            MW_LOG_WARN("PacketSender: cannot reconnect to address \'%s\'", mSocketPath.c_str());
        } else {
            MW_LOG_INFO("PacketSender: successful reconnect to address \'%s\'", mSocketPath.c_str());
        }
        mPktWriteFailCtr = 0;
	}
}

bool PacketSender::initSenderTask()
{
    try {
        mSendThread = std::thread(runWorkerTask, this);
        MW_LOG_INFO("Thread created for runWorkerTask [%zx]", std::hash<std::thread::id>()(mSendThread.get_id()));
    }
    catch (const std::exception& e) {
        MW_LOG_WARN("PacketSender: Error in initSenderTask: %s", e.what());
        return false;
    }
    
    return true;
}

void PacketSender::closeSenderTask()
{
    if (running)
    {
        running = false;
        mCv.notify_all();
        if (mSendThread.joinable())
        {
            mSendThread.join();        
        }
    }

}

bool PacketSender::initSocket(const char *socket_path)
{
    mSubtecSocketHandle = ::socket(AF_UNIX, SOCK_DGRAM, 0);
    if (mSubtecSocketHandle == -1)
    {
        MW_LOG_WARN("PacketSender: Unable to init socket");
        return false;
    }
    
    struct sockaddr_un addr;
    
    (void) std::memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    (void) std::strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path));
    addr.sun_path[sizeof(addr.sun_path) - 1] = 0;

    socklen_t optlen = sizeof(mSockBufSize);
    if(::getsockopt(mSubtecSocketHandle, SOL_SOCKET, SO_SNDBUF, &mSockBufSize, &optlen) != 0)
    {
	    MW_LOG_WARN("PacketSender: getsockopt Fails");
    }
    mSockBufSize = mSockBufSize / 2;  //kernel returns twice the value of actual buffer
    MW_LOG_INFO("SockBuffer size : %d", mSockBufSize);

    if (::connect(mSubtecSocketHandle, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) != 0)
    {
        ::close(mSubtecSocketHandle);
        MW_LOG_WARN("PacketSender: cannot connect to address \'%s\'", socket_path);
        return false;
    }
    MW_LOG_INFO("PacketSender: Initialized with socket_path %s", socket_path);

    return true;
}
