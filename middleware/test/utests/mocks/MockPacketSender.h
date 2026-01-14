/*
* If not stated otherwise in this file or this component's license file the
* following copyright and licenses apply:
*
* Copyright 2024 RDK Management
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

#ifndef PLAYER_MOCK_PACKET_SENDER_H
#define PLAYER_MOCK_PACKET_SENDER_H

#include "PacketSender.hpp"
#include <gmock/gmock.h>

class MockPacketSender : public PacketSender {
public:
    static MockPacketSender* instance;

    static PacketSender* Instance() {
        if (!instance) {
            instance = new MockPacketSender();
        }
        return instance;
    }

    bool initSocket(const char* socket_path) override {
        std::cout << "[Mocked] initSocket called for " << socket_path << std::endl;
        return true; // Pretend success
    }

    bool Init() override {
        std::cout << "[Mocked] Init called" << std::endl;
        return true;
    }
};

// define static
MockPacketSender* MockPacketSender::instance = nullptr;

#endif /* PLAYER_MOCK_PACKET_SENDER_H*/