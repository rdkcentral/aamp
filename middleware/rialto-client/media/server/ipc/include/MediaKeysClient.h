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

#ifndef FIREBOLT_RIALTO_SERVER_IPC_MEDIA_KEYS_CLIENT_H_
#define FIREBOLT_RIALTO_SERVER_IPC_MEDIA_KEYS_CLIENT_H_

#include "IIpcServer.h"
#include "IMediaKeysClient.h"
#include <memory>
#include <string>
#include <vector>

namespace firebolt::rialto::server::ipc
{
class MediaKeysClient : public IMediaKeysClient
{
public:
    MediaKeysClient(int mediaKeysHandle, const std::shared_ptr<::firebolt::rialto::ipc::IClient> &ipcClient);
    ~MediaKeysClient() override;

    void onLicenseRequest(int32_t keySessionId, const std::vector<unsigned char> &licenseRequestMessage,
                          const std::string &url) override;
    void onLicenseRenewal(int32_t keySessionId, const std::vector<unsigned char> &licenseRenewalMessage) override;
    void onKeyStatusesChanged(int32_t keySessionId, const KeyStatusVector &keyStatuses) override;

private:
    int m_mediaKeysHandle;
    std::shared_ptr<::firebolt::rialto::ipc::IClient> m_ipcClient;
};
} // namespace firebolt::rialto::server::ipc

#endif // FIREBOLT_RIALTO_SERVER_IPC_MEDIA_KEYS_CLIENT_H_
