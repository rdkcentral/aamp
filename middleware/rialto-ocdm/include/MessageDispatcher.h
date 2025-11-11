/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2023 Sky UK
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

#ifndef MESSAGE_DISPATCHER_H_
#define MESSAGE_DISPATCHER_H_

#include "IMessageDispatcher.h"
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <vector>

class MessageDispatcher : public IMessageDispatcher, public firebolt::rialto::IMediaKeysClient
{
    class MessageDispatcherClient : public IMessageDispatcherClient
    {
    public:
        MessageDispatcherClient(MessageDispatcher &dispatcher, firebolt::rialto::IMediaKeysClient *client);
        ~MessageDispatcherClient() override;

    private:
        MessageDispatcher &m_dispatcher;
        firebolt::rialto::IMediaKeysClient *m_client;
    };

public:
    MessageDispatcher() = default;
    ~MessageDispatcher() override = default;

    std::unique_ptr<IMessageDispatcherClient> createClient(firebolt::rialto::IMediaKeysClient *client) override;

    void onLicenseRequest(int32_t keySessionId, const std::vector<unsigned char> &licenseRequestMessage,
                          const std::string &url) override;
    void onLicenseRenewal(int32_t keySessionId, const std::vector<unsigned char> &licenseRenewalMessage) override;
    void onKeyStatusesChanged(int32_t keySessionId, const firebolt::rialto::KeyStatusVector &keyStatuses) override;

private:
    void addClient(firebolt::rialto::IMediaKeysClient *client);
    void removeClient(firebolt::rialto::IMediaKeysClient *client);

private:
    std::mutex m_mutex;
    std::set<firebolt::rialto::IMediaKeysClient *> m_clients;
};

#endif // MESSAGE_DISPATCHER_H_
