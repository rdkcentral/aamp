/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2023 RDK Management
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
#include "PacketSender.hpp"
#include "PlayerLogManager.h"

PacketSender* PacketSender::Instance()
{
    static PacketSender fakeInstance;
    return &fakeInstance;
}

// Define destructor to satisfy linker
PacketSender::~PacketSender() 
{
    MW_LOG_INFO("Fake PacketSender::~PacketSender called");
}

bool PacketSender::Init(const char* socket_path)
{
    MW_LOG_INFO("Fake PacketSender::Init called with %s", socket_path ? socket_path : "null");
    return true; // Always succeed in tests
}

bool PacketSender::Init()
{
    return Init("/tmp/fake.sock");
}

void PacketSender::SendPacket(PacketPtr&& packet)
{
    if (packet) {
        MW_LOG_INFO("Fake PacketSender::SendPacket type=%d counter=%d",
                    packet->getType(), packet->getCounter());
    } else {
        MW_LOG_WARN("Fake PacketSender::SendPacket called with null packet");
    }
}

bool PacketSender::IsRunning()
{
    return true; // Pretend always running
}

void PacketSender::Flush()
{
    MW_LOG_INFO("Fake PacketSender::Flush called");
}

void PacketSender::Close()
{
    MW_LOG_INFO("Fake PacketSender::Close called");
}
