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

#include "MediaKeysModuleService.h"
#include "ICdmService.h"
#include "MediaKeysClient.h"
#include "RialtoServerLogging.h"
#include <IIpcController.h>
#include <algorithm>
#include <cstdint>

namespace
{
int generateHandle()
{
    static int mediaKeysHandle{0};
    return mediaKeysHandle++;
}

firebolt::rialto::ProtoMediaKeyErrorStatus
convertMediaKeyErrorStatus(const firebolt::rialto::MediaKeyErrorStatus &errorStatus)
{
    switch (errorStatus)
    {
    case firebolt::rialto::MediaKeyErrorStatus::OK:
    {
        return firebolt::rialto::ProtoMediaKeyErrorStatus::OK;
    }
    case firebolt::rialto::MediaKeyErrorStatus::BAD_SESSION_ID:
    {
        return firebolt::rialto::ProtoMediaKeyErrorStatus::BAD_SESSION_ID;
    }
    case firebolt::rialto::MediaKeyErrorStatus::INTERFACE_NOT_IMPLEMENTED:
    {
        return firebolt::rialto::ProtoMediaKeyErrorStatus::INTERFACE_NOT_IMPLEMENTED;
    }
    case firebolt::rialto::MediaKeyErrorStatus::BUFFER_TOO_SMALL:
    {
        return firebolt::rialto::ProtoMediaKeyErrorStatus::BUFFER_TOO_SMALL;
    }
    case firebolt::rialto::MediaKeyErrorStatus::NOT_SUPPORTED:
    {
        return firebolt::rialto::ProtoMediaKeyErrorStatus::NOT_SUPPORTED;
    }
    case firebolt::rialto::MediaKeyErrorStatus::INVALID_STATE:
    {
        return firebolt::rialto::ProtoMediaKeyErrorStatus::INVALID_STATE;
    }
    case firebolt::rialto::MediaKeyErrorStatus::FAIL:
    {
        return firebolt::rialto::ProtoMediaKeyErrorStatus::FAIL;
    }
    }
    return firebolt::rialto::ProtoMediaKeyErrorStatus::FAIL;
}
firebolt::rialto::KeySessionType
convertKeySessionType(const firebolt::rialto::CreateKeySessionRequest_KeySessionType &protoKeySessionType)
{
    switch (protoKeySessionType)
    {
    case firebolt::rialto::CreateKeySessionRequest_KeySessionType::CreateKeySessionRequest_KeySessionType_TEMPORARY:
        return firebolt::rialto::KeySessionType::TEMPORARY;
    case firebolt::rialto::CreateKeySessionRequest_KeySessionType::CreateKeySessionRequest_KeySessionType_PERSISTENT_LICENCE:
        return firebolt::rialto::KeySessionType::PERSISTENT_LICENCE;
    case firebolt::rialto::CreateKeySessionRequest_KeySessionType::CreateKeySessionRequest_KeySessionType_PERSISTENT_RELEASE_MESSAGE:
        return firebolt::rialto::KeySessionType::PERSISTENT_RELEASE_MESSAGE;
    default:
        return firebolt::rialto::KeySessionType::UNKNOWN;
    }
}
firebolt::rialto::InitDataType covertInitDataType(firebolt::rialto::GenerateRequestRequest_InitDataType protoInitDataType)
{
    switch (protoInitDataType)
    {
    case firebolt::rialto::GenerateRequestRequest_InitDataType::GenerateRequestRequest_InitDataType_CENC:
        return firebolt::rialto::InitDataType::CENC;
    case firebolt::rialto::GenerateRequestRequest_InitDataType::GenerateRequestRequest_InitDataType_KEY_IDS:
        return firebolt::rialto::InitDataType::KEY_IDS;
    case firebolt::rialto::GenerateRequestRequest_InitDataType::GenerateRequestRequest_InitDataType_WEBM:
        return firebolt::rialto::InitDataType::WEBM;
    case firebolt::rialto::GenerateRequestRequest_InitDataType::GenerateRequestRequest_InitDataType_DRMHEADER:
        return firebolt::rialto::InitDataType::DRMHEADER;
    default:
        return firebolt::rialto::InitDataType::UNKNOWN;
    }
}
} // namespace

namespace firebolt::rialto::server::ipc
{
std::shared_ptr<IMediaKeysModuleServiceFactory> IMediaKeysModuleServiceFactory::createFactory()
{
    std::shared_ptr<IMediaKeysModuleServiceFactory> factory;

    try
    {
        factory = std::make_shared<MediaKeysModuleServiceFactory>();
    }
    catch (const std::exception &e)
    {
        RIALTO_SERVER_LOG_ERROR("Failed to create the media keys module service factory, reason: %s", e.what());
    }

    return factory;
}

std::shared_ptr<IMediaKeysModuleService> MediaKeysModuleServiceFactory::create(service::ICdmService &cdmService) const
{
    std::shared_ptr<IMediaKeysModuleService> mediaKeysModule;

    try
    {
        mediaKeysModule = std::make_shared<MediaKeysModuleService>(cdmService);
    }
    catch (const std::exception &e)
    {
        RIALTO_SERVER_LOG_ERROR("Failed to create the media keys module service, reason: %s", e.what());
    }

    return mediaKeysModule;
}

MediaKeysModuleService::MediaKeysModuleService(service::ICdmService &cdmService) : m_cdmService{cdmService} {}

MediaKeysModuleService::~MediaKeysModuleService() {}

void MediaKeysModuleService::clientConnected(const std::shared_ptr<::firebolt::rialto::ipc::IClient> &ipcClient)
{
    RIALTO_SERVER_LOG_INFO("Client Connected!");
    {
        m_clientMediaKeysHandles.emplace(ipcClient, std::set<int>());
    }
    ipcClient->exportService(shared_from_this());
}

void MediaKeysModuleService::clientDisconnected(const std::shared_ptr<::firebolt::rialto::ipc::IClient> &ipcClient)
{
    RIALTO_SERVER_LOG_INFO("Client disconnected!");
    std::set<int> mediaKeysHandles;
    {
        auto handleIter = m_clientMediaKeysHandles.find(ipcClient);
        if (handleIter == m_clientMediaKeysHandles.end())
        {
            RIALTO_SERVER_LOG_ERROR("unknown client disconnected");
            return;
        }
        mediaKeysHandles = handleIter->second; // copy to avoid deadlock
        m_clientMediaKeysHandles.erase(handleIter);
    }
    for (const auto &mediaKeysHandle : mediaKeysHandles)
    {
        m_cdmService.destroyMediaKeys(mediaKeysHandle);
    }
}

void MediaKeysModuleService::createMediaKeys(::google::protobuf::RpcController *controller,
                                             const ::firebolt::rialto::CreateMediaKeysRequest *request,
                                             ::firebolt::rialto::CreateMediaKeysResponse *response,
                                             ::google::protobuf::Closure *done)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");
    auto ipcController = dynamic_cast<firebolt::rialto::ipc::IController *>(controller);
    if (!ipcController)
    {
        RIALTO_SERVER_LOG_ERROR("ipc library provided incompatible controller object");
        controller->SetFailed("ipc library provided incompatible controller object");
        done->Run();
        return;
    }

    int mediaKeysHandle = generateHandle();
    bool mediaKeysCreated = m_cdmService.createMediaKeys(mediaKeysHandle, request->key_system());
    if (mediaKeysCreated)
    {
        // Assume that IPC library works well and client is present
        m_clientMediaKeysHandles[ipcController->getClient()].insert(mediaKeysHandle);
        response->set_media_keys_handle(mediaKeysHandle);
    }
    else
    {
        RIALTO_SERVER_LOG_ERROR("Create media keys failed");
        controller->SetFailed("Operation failed");
    }
    done->Run();
}

void MediaKeysModuleService::destroyMediaKeys(::google::protobuf::RpcController *controller,
                                              const ::firebolt::rialto::DestroyMediaKeysRequest *request,
                                              ::firebolt::rialto::DestroyMediaKeysResponse *response,
                                              ::google::protobuf::Closure *done)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");
    auto ipcController = dynamic_cast<firebolt::rialto::ipc::IController *>(controller);
    if (!ipcController)
    {
        RIALTO_SERVER_LOG_ERROR("ipc library provided incompatible controller object");
        controller->SetFailed("ipc library provided incompatible controller object");
        done->Run();
        return;
    }
    if (!m_cdmService.destroyMediaKeys(request->media_keys_handle()))
    {
        RIALTO_SERVER_LOG_ERROR("Destroy session failed");
        controller->SetFailed("Operation failed");
        done->Run();
        return;
    }
    auto handleIter = m_clientMediaKeysHandles.find(ipcController->getClient());
    if (handleIter != m_clientMediaKeysHandles.end())
    {
        handleIter->second.erase(request->media_keys_handle());
    }
    done->Run();
}

void MediaKeysModuleService::containsKey(::google::protobuf::RpcController *controller,
                                         const ::firebolt::rialto::ContainsKeyRequest *request,
                                         ::firebolt::rialto::ContainsKeyResponse *response,
                                         ::google::protobuf::Closure *done)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");

    bool result = m_cdmService.containsKey(request->media_keys_handle(), request->key_session_id(),
                                           std::vector<std::uint8_t>{request->key_id().begin(), request->key_id().end()});
    response->set_contains_key(result);
    done->Run();
}

void MediaKeysModuleService::createKeySession(::google::protobuf::RpcController *controller,
                                              const ::firebolt::rialto::CreateKeySessionRequest *request,
                                              ::firebolt::rialto::CreateKeySessionResponse *response,
                                              ::google::protobuf::Closure *done)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");
    auto ipcController = dynamic_cast<firebolt::rialto::ipc::IController *>(controller);
    if (!ipcController)
    {
        RIALTO_SERVER_LOG_ERROR("ipc library provided incompatible controller object");
        controller->SetFailed("ipc library provided incompatible controller object");
        done->Run();
        return;
    }

    int32_t keySessionId;
    MediaKeyErrorStatus status =
        m_cdmService.createKeySession(request->media_keys_handle(), convertKeySessionType(request->session_type()),
                                      std::make_shared<MediaKeysClient>(request->media_keys_handle(),
                                                                        ipcController->getClient()),
                                      request->is_ldl(), keySessionId);
    if (MediaKeyErrorStatus::OK == status)
    {
        response->set_key_session_id(keySessionId);
    }
    response->set_error_status(convertMediaKeyErrorStatus(status));
    done->Run();
}

void MediaKeysModuleService::generateRequest(::google::protobuf::RpcController *controller,
                                             const ::firebolt::rialto::GenerateRequestRequest *request,
                                             ::firebolt::rialto::GenerateRequestResponse *response,
                                             ::google::protobuf::Closure *done)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");

    MediaKeyErrorStatus status = m_cdmService.generateRequest(request->media_keys_handle(), request->key_session_id(),
                                                              covertInitDataType(request->init_data_type()),
                                                              std::vector<std::uint8_t>{request->init_data().begin(),
                                                                                        request->init_data().end()});
    response->set_error_status(convertMediaKeyErrorStatus(status));
    done->Run();
}

void MediaKeysModuleService::loadSession(::google::protobuf::RpcController *controller,
                                         const ::firebolt::rialto::LoadSessionRequest *request,
                                         ::firebolt::rialto::LoadSessionResponse *response,
                                         ::google::protobuf::Closure *done)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");

    MediaKeyErrorStatus status = m_cdmService.loadSession(request->media_keys_handle(), request->key_session_id());
    response->set_error_status(convertMediaKeyErrorStatus(status));
    done->Run();
}

void MediaKeysModuleService::updateSession(::google::protobuf::RpcController *controller,
                                           const ::firebolt::rialto::UpdateSessionRequest *request,
                                           ::firebolt::rialto::UpdateSessionResponse *response,
                                           ::google::protobuf::Closure *done)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");

    MediaKeyErrorStatus status = m_cdmService.updateSession(request->media_keys_handle(), request->key_session_id(),
                                                            std::vector<std::uint8_t>{request->response_data().begin(),
                                                                                      request->response_data().end()});
    response->set_error_status(convertMediaKeyErrorStatus(status));
    done->Run();
}

void MediaKeysModuleService::setDrmHeader(::google::protobuf::RpcController *controller,
                                          const ::firebolt::rialto::SetDrmHeaderRequest *request,
                                          ::firebolt::rialto::SetDrmHeaderResponse *response,
                                          ::google::protobuf::Closure *done)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");

    MediaKeyErrorStatus status = m_cdmService.setDrmHeader(request->media_keys_handle(), request->key_session_id(),
                                                           std::vector<std::uint8_t>{request->request_data().begin(),
                                                                                     request->request_data().end()});
    response->set_error_status(convertMediaKeyErrorStatus(status));
    done->Run();
}

void MediaKeysModuleService::closeKeySession(::google::protobuf::RpcController *controller,
                                             const ::firebolt::rialto::CloseKeySessionRequest *request,
                                             ::firebolt::rialto::CloseKeySessionResponse *response,
                                             ::google::protobuf::Closure *done)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");

    MediaKeyErrorStatus status = m_cdmService.closeKeySession(request->media_keys_handle(), request->key_session_id());
    response->set_error_status(convertMediaKeyErrorStatus(status));
    done->Run();
}

void MediaKeysModuleService::removeKeySession(::google::protobuf::RpcController *controller,
                                              const ::firebolt::rialto::RemoveKeySessionRequest *request,
                                              ::firebolt::rialto::RemoveKeySessionResponse *response,
                                              ::google::protobuf::Closure *done)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");

    MediaKeyErrorStatus status = m_cdmService.removeKeySession(request->media_keys_handle(), request->key_session_id());
    response->set_error_status(convertMediaKeyErrorStatus(status));
    done->Run();
}

void MediaKeysModuleService::deleteDrmStore(::google::protobuf::RpcController *controller,
                                            const ::firebolt::rialto::DeleteDrmStoreRequest *request,
                                            ::firebolt::rialto::DeleteDrmStoreResponse *response,
                                            ::google::protobuf::Closure *done)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");

    MediaKeyErrorStatus status = m_cdmService.deleteDrmStore(request->media_keys_handle());
    response->set_error_status(convertMediaKeyErrorStatus(status));
    done->Run();
}

void MediaKeysModuleService::deleteKeyStore(::google::protobuf::RpcController *controller,
                                            const ::firebolt::rialto::DeleteKeyStoreRequest *request,
                                            ::firebolt::rialto::DeleteKeyStoreResponse *response,
                                            ::google::protobuf::Closure *done)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");

    MediaKeyErrorStatus status = m_cdmService.deleteKeyStore(request->media_keys_handle());
    response->set_error_status(convertMediaKeyErrorStatus(status));
    done->Run();
}

void MediaKeysModuleService::getDrmStoreHash(::google::protobuf::RpcController *controller,
                                             const ::firebolt::rialto::GetDrmStoreHashRequest *request,
                                             ::firebolt::rialto::GetDrmStoreHashResponse *response,
                                             ::google::protobuf::Closure *done)
{
    std::vector<unsigned char> drmStoreHash;
    RIALTO_SERVER_LOG_DEBUG("entry:");

    MediaKeyErrorStatus status = m_cdmService.getDrmStoreHash(request->media_keys_handle(), drmStoreHash);
    response->set_error_status(convertMediaKeyErrorStatus(status));
    for (const auto &item : drmStoreHash)
    {
        response->add_drm_store_hash(item);
    }
    done->Run();
}

void MediaKeysModuleService::getKeyStoreHash(::google::protobuf::RpcController *controller,
                                             const ::firebolt::rialto::GetKeyStoreHashRequest *request,
                                             ::firebolt::rialto::GetKeyStoreHashResponse *response,
                                             ::google::protobuf::Closure *done)
{
    std::vector<unsigned char> keyStoreHash;
    RIALTO_SERVER_LOG_DEBUG("entry:");

    MediaKeyErrorStatus status = m_cdmService.getKeyStoreHash(request->media_keys_handle(), keyStoreHash);
    response->set_error_status(convertMediaKeyErrorStatus(status));
    for (const auto &item : keyStoreHash)
    {
        response->add_key_store_hash(item);
    }
    done->Run();
}

void MediaKeysModuleService::getLdlSessionsLimit(::google::protobuf::RpcController *controller,
                                                 const ::firebolt::rialto::GetLdlSessionsLimitRequest *request,
                                                 ::firebolt::rialto::GetLdlSessionsLimitResponse *response,
                                                 ::google::protobuf::Closure *done)
{
    uint32_t ldlLimit{0};
    RIALTO_SERVER_LOG_DEBUG("entry:");

    MediaKeyErrorStatus status = m_cdmService.getLdlSessionsLimit(request->media_keys_handle(), ldlLimit);
    response->set_error_status(convertMediaKeyErrorStatus(status));
    response->set_ldl_limit(ldlLimit);
    done->Run();
}

void MediaKeysModuleService::getLastDrmError(::google::protobuf::RpcController *controller,
                                             const ::firebolt::rialto::GetLastDrmErrorRequest *request,
                                             ::firebolt::rialto::GetLastDrmErrorResponse *response,
                                             ::google::protobuf::Closure *done)
{
    uint32_t errorCode{0};
    RIALTO_SERVER_LOG_DEBUG("entry:");

    MediaKeyErrorStatus status =
        m_cdmService.getLastDrmError(request->media_keys_handle(), request->key_session_id(), errorCode);
    response->set_error_status(convertMediaKeyErrorStatus(status));
    response->set_error_code(errorCode);
    done->Run();
}

void MediaKeysModuleService::getDrmTime(::google::protobuf::RpcController *controller,
                                        const ::firebolt::rialto::GetDrmTimeRequest *request,
                                        ::firebolt::rialto::GetDrmTimeResponse *response,
                                        ::google::protobuf::Closure *done)
{
    uint64_t drmTime{0};
    RIALTO_SERVER_LOG_DEBUG("entry:");

    MediaKeyErrorStatus status = m_cdmService.getDrmTime(request->media_keys_handle(), drmTime);
    response->set_error_status(convertMediaKeyErrorStatus(status));
    response->set_drm_time(drmTime);
    done->Run();
}

void MediaKeysModuleService::getCdmKeySessionId(::google::protobuf::RpcController *controller,
                                                const ::firebolt::rialto::GetCdmKeySessionIdRequest *request,
                                                ::firebolt::rialto::GetCdmKeySessionIdResponse *response,
                                                ::google::protobuf::Closure *done)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");

    std::string cdmKeySessionId;
    MediaKeyErrorStatus status =
        m_cdmService.getCdmKeySessionId(request->media_keys_handle(), request->key_session_id(), cdmKeySessionId);
    response->set_error_status(convertMediaKeyErrorStatus(status));
    response->set_cdm_key_session_id(cdmKeySessionId);
    done->Run();
}

void MediaKeysModuleService::releaseKeySession(::google::protobuf::RpcController *controller,
                                               const ::firebolt::rialto::ReleaseKeySessionRequest *request,
                                               ::firebolt::rialto::ReleaseKeySessionResponse *response,
                                               ::google::protobuf::Closure *done)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");

    MediaKeyErrorStatus status = m_cdmService.releaseKeySession(request->media_keys_handle(), request->key_session_id());
    response->set_error_status(convertMediaKeyErrorStatus(status));
    done->Run();
}

void MediaKeysModuleService::getMetricSystemData(::google::protobuf::RpcController *controller,
                                                 const ::firebolt::rialto::GetMetricSystemDataRequest *request,
                                                 ::firebolt::rialto::GetMetricSystemDataResponse *response,
                                                 ::google::protobuf::Closure *done)
{
    std::vector<uint8_t> buffer;
    RIALTO_SERVER_LOG_DEBUG("entry:");

    MediaKeyErrorStatus status = m_cdmService.getMetricSystemData(request->media_keys_handle(), buffer);
    response->set_error_status(convertMediaKeyErrorStatus(status));
    for (const auto &buf : buffer)
    {
        response->add_buffer(buf);
    }
    done->Run();
}

} // namespace firebolt::rialto::server::ipc
