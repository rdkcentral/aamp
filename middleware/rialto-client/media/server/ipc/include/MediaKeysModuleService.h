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

#ifndef FIREBOLT_RIALTO_SERVER_IPC_MEDIA_KEYS_MODULE_SERVICE_H_
#define FIREBOLT_RIALTO_SERVER_IPC_MEDIA_KEYS_MODULE_SERVICE_H_

#include "ICdmService.h"
#include "IMediaKeysModuleService.h"
#include <map>
#include <memory>
#include <set>

namespace firebolt::rialto::server::ipc
{
class MediaKeysModuleServiceFactory : public IMediaKeysModuleServiceFactory
{
public:
    MediaKeysModuleServiceFactory() = default;
    virtual ~MediaKeysModuleServiceFactory() = default;

    std::shared_ptr<IMediaKeysModuleService> create(service::ICdmService &cdmService) const override;
};

class MediaKeysModuleService : public IMediaKeysModuleService
{
public:
    explicit MediaKeysModuleService(service::ICdmService &cdmService);
    ~MediaKeysModuleService() override;

    void clientConnected(const std::shared_ptr<::firebolt::rialto::ipc::IClient> &ipcClient) override;
    void clientDisconnected(const std::shared_ptr<::firebolt::rialto::ipc::IClient> &ipcClient) override;

    void createMediaKeys(::google::protobuf::RpcController *controller,
                         const ::firebolt::rialto::CreateMediaKeysRequest *request,
                         ::firebolt::rialto::CreateMediaKeysResponse *response,
                         ::google::protobuf::Closure *done) override;
    void destroyMediaKeys(::google::protobuf::RpcController *controller,
                          const ::firebolt::rialto::DestroyMediaKeysRequest *request,
                          ::firebolt::rialto::DestroyMediaKeysResponse *response,
                          ::google::protobuf::Closure *done) override;
    void containsKey(::google::protobuf::RpcController *controller, const ::firebolt::rialto::ContainsKeyRequest *request,
                     ::firebolt::rialto::ContainsKeyResponse *response, ::google::protobuf::Closure *done) override;
    void createKeySession(::google::protobuf::RpcController *controller,
                          const ::firebolt::rialto::CreateKeySessionRequest *request,
                          ::firebolt::rialto::CreateKeySessionResponse *response,
                          ::google::protobuf::Closure *done) override;
    void generateRequest(::google::protobuf::RpcController *controller,
                         const ::firebolt::rialto::GenerateRequestRequest *request,
                         ::firebolt::rialto::GenerateRequestResponse *response,
                         ::google::protobuf::Closure *done) override;
    void loadSession(::google::protobuf::RpcController *controller, const ::firebolt::rialto::LoadSessionRequest *request,
                     ::firebolt::rialto::LoadSessionResponse *response, ::google::protobuf::Closure *done) override;
    void updateSession(::google::protobuf::RpcController *controller,
                       const ::firebolt::rialto::UpdateSessionRequest *request,
                       ::firebolt::rialto::UpdateSessionResponse *response, ::google::protobuf::Closure *done) override;
    void setDrmHeader(::google::protobuf::RpcController *controller,
                      const ::firebolt::rialto::SetDrmHeaderRequest *request,
                      ::firebolt::rialto::SetDrmHeaderResponse *response, ::google::protobuf::Closure *done) override;
    void closeKeySession(::google::protobuf::RpcController *controller,
                         const ::firebolt::rialto::CloseKeySessionRequest *request,
                         ::firebolt::rialto::CloseKeySessionResponse *response,
                         ::google::protobuf::Closure *done) override;
    void removeKeySession(::google::protobuf::RpcController *controller,
                          const ::firebolt::rialto::RemoveKeySessionRequest *request,
                          ::firebolt::rialto::RemoveKeySessionResponse *response,
                          ::google::protobuf::Closure *done) override;
    void deleteDrmStore(::google::protobuf::RpcController *controller,
                        const ::firebolt::rialto::DeleteDrmStoreRequest *request,
                        ::firebolt::rialto::DeleteDrmStoreResponse *response, ::google::protobuf::Closure *done) override;
    void deleteKeyStore(::google::protobuf::RpcController *controller,
                        const ::firebolt::rialto::DeleteKeyStoreRequest *request,
                        ::firebolt::rialto::DeleteKeyStoreResponse *response, ::google::protobuf::Closure *done) override;
    void getDrmStoreHash(::google::protobuf::RpcController *controller,
                         const ::firebolt::rialto::GetDrmStoreHashRequest *request,
                         ::firebolt::rialto::GetDrmStoreHashResponse *response,
                         ::google::protobuf::Closure *done) override;
    void getKeyStoreHash(::google::protobuf::RpcController *controller,
                         const ::firebolt::rialto::GetKeyStoreHashRequest *request,
                         ::firebolt::rialto::GetKeyStoreHashResponse *response,
                         ::google::protobuf::Closure *done) override;
    void getLdlSessionsLimit(::google::protobuf::RpcController *controller,
                             const ::firebolt::rialto::GetLdlSessionsLimitRequest *request,
                             ::firebolt::rialto::GetLdlSessionsLimitResponse *response,
                             ::google::protobuf::Closure *done) override;
    void getLastDrmError(::google::protobuf::RpcController *controller,
                         const ::firebolt::rialto::GetLastDrmErrorRequest *request,
                         ::firebolt::rialto::GetLastDrmErrorResponse *response,
                         ::google::protobuf::Closure *done) override;
    void getDrmTime(::google::protobuf::RpcController *controller, const ::firebolt::rialto::GetDrmTimeRequest *request,
                    ::firebolt::rialto::GetDrmTimeResponse *response, ::google::protobuf::Closure *done) override;
    void getCdmKeySessionId(::google::protobuf::RpcController *controller,
                            const ::firebolt::rialto::GetCdmKeySessionIdRequest *request,
                            ::firebolt::rialto::GetCdmKeySessionIdResponse *response,
                            ::google::protobuf::Closure *done) override;
    void releaseKeySession(::google::protobuf::RpcController *controller,
                           const ::firebolt::rialto::ReleaseKeySessionRequest *request,
                           ::firebolt::rialto::ReleaseKeySessionResponse *response,
                           ::google::protobuf::Closure *done) override;
    void getMetricSystemData(::google::protobuf::RpcController *controller,
                             const ::firebolt::rialto::GetMetricSystemDataRequest *request,
                             ::firebolt::rialto::GetMetricSystemDataResponse *response,
                             ::google::protobuf::Closure *done) override;

private:
    service::ICdmService &m_cdmService;
    std::map<std::shared_ptr<::firebolt::rialto::ipc::IClient>, std::set<int>> m_clientMediaKeysHandles;
};
} // namespace firebolt::rialto::server::ipc

#endif // FIREBOLT_RIALTO_SERVER_IPC_MEDIA_KEYS_MODULE_SERVICE_H_
