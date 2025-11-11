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

#include "MediaKeysClient.h"
#include "IIpcServer.h"
#include "RialtoServerLogging.h"
#include "mediakeysmodule.pb.h"

namespace
{
firebolt::rialto::KeyStatusesChangedEvent_KeyStatus convertKeyStatus(const firebolt::rialto::KeyStatus &keyStatus)
{
    switch (keyStatus)
    {
    case firebolt::rialto::KeyStatus::USABLE:
    {
        return firebolt::rialto::KeyStatusesChangedEvent_KeyStatus_USABLE;
    }
    case firebolt::rialto::KeyStatus::EXPIRED:
    {
        return firebolt::rialto::KeyStatusesChangedEvent_KeyStatus_EXPIRED;
    }
    case firebolt::rialto::KeyStatus::OUTPUT_RESTRICTED:
    {
        return firebolt::rialto::KeyStatusesChangedEvent_KeyStatus_OUTPUT_RESTRICTED;
    }
    case firebolt::rialto::KeyStatus::PENDING:
    {
        return firebolt::rialto::KeyStatusesChangedEvent_KeyStatus_PENDING;
    }
    case firebolt::rialto::KeyStatus::INTERNAL_ERROR:
    {
        return firebolt::rialto::KeyStatusesChangedEvent_KeyStatus_INTERNAL_ERROR;
    }
    case firebolt::rialto::KeyStatus::RELEASED:
    {
        return firebolt::rialto::KeyStatusesChangedEvent_KeyStatus_RELEASED;
    }
    }
    return firebolt::rialto::KeyStatusesChangedEvent_KeyStatus_INTERNAL_ERROR;
}
} // namespace

namespace firebolt::rialto::server::ipc
{
MediaKeysClient::MediaKeysClient(int mediaKeysHandle, const std::shared_ptr<::firebolt::rialto::ipc::IClient> &ipcClient)
    : m_mediaKeysHandle{mediaKeysHandle}, m_ipcClient{ipcClient}
{
}

MediaKeysClient::~MediaKeysClient() {}

void MediaKeysClient::onLicenseRequest(int32_t keySessionId, const std::vector<unsigned char> &licenseRequestMessage,
                                       const std::string &url)
{
    RIALTO_SERVER_LOG_DEBUG("Sending LicenseRequestEvent");

    auto event = std::make_shared<firebolt::rialto::LicenseRequestEvent>();
    for (auto it = licenseRequestMessage.begin(); it != licenseRequestMessage.end(); it++)
    {
        event->add_license_request_message(*it);
    }
    event->set_media_keys_handle(m_mediaKeysHandle);
    event->set_key_session_id(keySessionId);
    event->set_url(url);

    m_ipcClient->sendEvent(event);
}

void MediaKeysClient::onLicenseRenewal(int32_t keySessionId, const std::vector<unsigned char> &licenseRenewalMessage)
{
    RIALTO_SERVER_LOG_DEBUG("Sending LicenseRenewalEvent");

    auto event = std::make_shared<firebolt::rialto::LicenseRenewalEvent>();
    for (auto it = licenseRenewalMessage.begin(); it != licenseRenewalMessage.end(); it++)
    {
        event->add_license_renewal_message(*it);
    }
    event->set_media_keys_handle(m_mediaKeysHandle);
    event->set_key_session_id(keySessionId);

    m_ipcClient->sendEvent(event);
}

void MediaKeysClient::onKeyStatusesChanged(int32_t keySessionId, const KeyStatusVector &keyStatuses)
{
    RIALTO_SERVER_LOG_DEBUG("Sending KeyStatusesChangedEvent");

    auto event = std::make_shared<firebolt::rialto::KeyStatusesChangedEvent>();
    for (auto it = keyStatuses.begin(); it != keyStatuses.end(); it++)
    {
        ::firebolt::rialto::KeyStatusesChangedEvent_KeyStatusPair *keyStatusPair = event->add_key_statuses();
        for (auto it2 = it->first.begin(); it2 != it->first.end(); it2++)
        {
            keyStatusPair->add_key_id(*it2);
        }
        keyStatusPair->set_key_status(convertKeyStatus(it->second));
    }
    event->set_media_keys_handle(m_mediaKeysHandle);
    event->set_key_session_id(keySessionId);

    m_ipcClient->sendEvent(event);
}
} // namespace firebolt::rialto::server::ipc
