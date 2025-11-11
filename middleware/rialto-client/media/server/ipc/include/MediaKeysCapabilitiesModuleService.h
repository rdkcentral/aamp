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

#ifndef FIREBOLT_RIALTO_SERVER_IPC_MEDIA_KEYS_CAPABILITIES_MODULE_SERVICE_H_
#define FIREBOLT_RIALTO_SERVER_IPC_MEDIA_KEYS_CAPABILITIES_MODULE_SERVICE_H_

#include "ICdmService.h"
#include "IMediaKeysCapabilitiesModuleService.h"
#include <map>
#include <memory>
#include <set>

namespace firebolt::rialto::server::ipc
{
class MediaKeysCapabilitiesModuleServiceFactory : public IMediaKeysCapabilitiesModuleServiceFactory
{
public:
    MediaKeysCapabilitiesModuleServiceFactory() = default;
    virtual ~MediaKeysCapabilitiesModuleServiceFactory() = default;

    std::shared_ptr<IMediaKeysCapabilitiesModuleService> create(service::ICdmService &cdmService) const override;
};

class MediaKeysCapabilitiesModuleService : public IMediaKeysCapabilitiesModuleService
{
public:
    explicit MediaKeysCapabilitiesModuleService(service::ICdmService &cdmService);
    ~MediaKeysCapabilitiesModuleService() override;

    void clientConnected(const std::shared_ptr<::firebolt::rialto::ipc::IClient> &ipcClient) override;
    void clientDisconnected(const std::shared_ptr<::firebolt::rialto::ipc::IClient> &ipcClient) override;

    void getSupportedKeySystems(::google::protobuf::RpcController *controller,
                                const ::firebolt::rialto::GetSupportedKeySystemsRequest *request,
                                ::firebolt::rialto::GetSupportedKeySystemsResponse *response,
                                ::google::protobuf::Closure *done) override;
    void supportsKeySystem(::google::protobuf::RpcController *controller,
                           const ::firebolt::rialto::SupportsKeySystemRequest *request,
                           ::firebolt::rialto::SupportsKeySystemResponse *response,
                           ::google::protobuf::Closure *done) override;
    void getSupportedKeySystemVersion(::google::protobuf::RpcController *controller,
                                      const ::firebolt::rialto::GetSupportedKeySystemVersionRequest *request,
                                      ::firebolt::rialto::GetSupportedKeySystemVersionResponse *response,
                                      ::google::protobuf::Closure *done) override;
    void isServerCertificateSupported(::google::protobuf::RpcController *controller,
                                      const ::firebolt::rialto::IsServerCertificateSupportedRequest *request,
                                      ::firebolt::rialto::IsServerCertificateSupportedResponse *response,
                                      ::google::protobuf::Closure *done) override;

private:
    service::ICdmService &m_cdmService;
};
} // namespace firebolt::rialto::server::ipc

#endif // FIREBOLT_RIALTO_SERVER_IPC_MEDIA_KEYS_CAPABILITIES_MODULE_SERVICE_H_
