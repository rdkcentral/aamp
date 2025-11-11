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

#include "MessageDispatcher.h"

MessageDispatcher::MessageDispatcherClient::MessageDispatcherClient(MessageDispatcher &dispatcher,
                                                                    firebolt::rialto::IMediaKeysClient *client)
    : m_dispatcher{dispatcher}, m_client{client}
{
    m_dispatcher.addClient(m_client);
}

MessageDispatcher::MessageDispatcherClient::~MessageDispatcherClient()
{
    m_dispatcher.removeClient(m_client);
}

std::unique_ptr<IMessageDispatcherClient> MessageDispatcher::createClient(firebolt::rialto::IMediaKeysClient *client)
{
    return std::make_unique<MessageDispatcherClient>(*this, client);
}

void MessageDispatcher::addClient(firebolt::rialto::IMediaKeysClient *client)
{
    std::unique_lock<std::mutex> lock{m_mutex};
    m_clients.emplace(client);
}

void MessageDispatcher::removeClient(firebolt::rialto::IMediaKeysClient *client)
{
    std::unique_lock<std::mutex> lock{m_mutex};
    m_clients.erase(client);
}

void MessageDispatcher::onLicenseRequest(int32_t keySessionId, const std::vector<unsigned char> &licenseRequestMessage,
                                         const std::string &url)
{
    std::unique_lock<std::mutex> lock{m_mutex};
    for (auto *client : m_clients)
    {
        client->onLicenseRequest(keySessionId, licenseRequestMessage, url);
    }
}

void MessageDispatcher::onLicenseRenewal(int32_t keySessionId, const std::vector<unsigned char> &licenseRenewalMessage)
{
    std::unique_lock<std::mutex> lock{m_mutex};
    for (auto *client : m_clients)
    {
        client->onLicenseRenewal(keySessionId, licenseRenewalMessage);
    }
}

void MessageDispatcher::onKeyStatusesChanged(int32_t keySessionId, const firebolt::rialto::KeyStatusVector &keyStatuses)
{
    std::unique_lock<std::mutex> lock{m_mutex};
    for (auto *client : m_clients)
    {
        client->onKeyStatusesChanged(keySessionId, keyStatuses);
    }
}
