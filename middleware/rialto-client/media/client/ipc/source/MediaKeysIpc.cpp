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

#include "MediaKeysIpc.h"
#include "RialtoClientLogging.h"

namespace
{
firebolt::rialto::MediaKeyErrorStatus
convertMediaKeyErrorStatus(const firebolt::rialto::ProtoMediaKeyErrorStatus &errorStatus)
{
    switch (errorStatus)
    {
    case firebolt::rialto::ProtoMediaKeyErrorStatus::OK:
    {
        return firebolt::rialto::MediaKeyErrorStatus::OK;
    }
    case firebolt::rialto::ProtoMediaKeyErrorStatus::BAD_SESSION_ID:
    {
        return firebolt::rialto::MediaKeyErrorStatus::BAD_SESSION_ID;
    }
    case firebolt::rialto::ProtoMediaKeyErrorStatus::INTERFACE_NOT_IMPLEMENTED:
    {
        return firebolt::rialto::MediaKeyErrorStatus::INTERFACE_NOT_IMPLEMENTED;
    }
    case firebolt::rialto::ProtoMediaKeyErrorStatus::BUFFER_TOO_SMALL:
    {
        return firebolt::rialto::MediaKeyErrorStatus::BUFFER_TOO_SMALL;
    }
    case firebolt::rialto::ProtoMediaKeyErrorStatus::NOT_SUPPORTED:
    {
        return firebolt::rialto::MediaKeyErrorStatus::NOT_SUPPORTED;
    }
    case firebolt::rialto::ProtoMediaKeyErrorStatus::INVALID_STATE:
    {
        return firebolt::rialto::MediaKeyErrorStatus::INVALID_STATE;
    }
    case firebolt::rialto::ProtoMediaKeyErrorStatus::FAIL:
    {
        return firebolt::rialto::MediaKeyErrorStatus::FAIL;
    }
    }
    return firebolt::rialto::MediaKeyErrorStatus::FAIL;
}

firebolt::rialto::KeyStatus convertKeyStatus(const firebolt::rialto::KeyStatusesChangedEvent_KeyStatus &protoKeyStatus)
{
    switch (protoKeyStatus)
    {
    case firebolt::rialto::KeyStatusesChangedEvent_KeyStatus_USABLE:
    {
        return firebolt::rialto::KeyStatus::USABLE;
    }
    case firebolt::rialto::KeyStatusesChangedEvent_KeyStatus_EXPIRED:
    {
        return firebolt::rialto::KeyStatus::EXPIRED;
    }
    case firebolt::rialto::KeyStatusesChangedEvent_KeyStatus_OUTPUT_RESTRICTED:
    {
        return firebolt::rialto::KeyStatus::OUTPUT_RESTRICTED;
    }
    case firebolt::rialto::KeyStatusesChangedEvent_KeyStatus_PENDING:
    {
        return firebolt::rialto::KeyStatus::PENDING;
    }
    case firebolt::rialto::KeyStatusesChangedEvent_KeyStatus_INTERNAL_ERROR:
    {
        return firebolt::rialto::KeyStatus::INTERNAL_ERROR;
    }
    case firebolt::rialto::KeyStatusesChangedEvent_KeyStatus_RELEASED:
    {
        return firebolt::rialto::KeyStatus::RELEASED;
    }
    }
    return firebolt::rialto::KeyStatus::INTERNAL_ERROR;
}

const char *toString(const firebolt::rialto::MediaKeyErrorStatus &errorStatus)
{
    switch (errorStatus)
    {
    case firebolt::rialto::MediaKeyErrorStatus::OK:
    {
        return "OK";
    }
    case firebolt::rialto::MediaKeyErrorStatus::BAD_SESSION_ID:
    {
        return "BAD_SESSION_ID";
    }
    case firebolt::rialto::MediaKeyErrorStatus::INTERFACE_NOT_IMPLEMENTED:
    {
        return "INTERFACE_NOT_IMPLEMENTED";
    }
    case firebolt::rialto::MediaKeyErrorStatus::BUFFER_TOO_SMALL:
    {
        return "BUFFER_TOO_SMALL";
    }
    case firebolt::rialto::MediaKeyErrorStatus::NOT_SUPPORTED:
    {
        return "NOT_SUPPORTED";
    }
    case firebolt::rialto::MediaKeyErrorStatus::INVALID_STATE:
    {
        return "INVALID_STATE";
    }
    case firebolt::rialto::MediaKeyErrorStatus::FAIL:
    {
        return "FAIL";
    }
    }
    return "UNKNOWN";
}
} // namespace

namespace firebolt::rialto::client
{
std::shared_ptr<IMediaKeysIpcFactory> IMediaKeysIpcFactory::createFactory()
{
    std::shared_ptr<IMediaKeysIpcFactory> factory;

    try
    {
        factory = std::make_shared<MediaKeysIpcFactory>();
    }
    catch (const std::exception &e)
    {
        RIALTO_CLIENT_LOG_ERROR("Failed to create the media keys ipc factory, reason: %s", e.what());
    }

    return factory;
}

std::unique_ptr<IMediaKeys> MediaKeysIpcFactory::createMediaKeysIpc(const std::string &keySystem) const
{
    std::unique_ptr<IMediaKeys> mediaKeysIpc;
    try
    {
        mediaKeysIpc =
            std::make_unique<client::MediaKeysIpc>(keySystem, IIpcClientAccessor::instance().getIpcClient(),
                                                   firebolt::rialto::common::IEventThreadFactory::createFactory());
    }
    catch (const std::exception &e)
    {
        RIALTO_CLIENT_LOG_ERROR("Failed to create the media keys ipc, reason: %s", e.what());
    }

    return mediaKeysIpc;
}

MediaKeysIpc::MediaKeysIpc(const std::string &keySystem, IIpcClient &ipcClient,
                           const std::shared_ptr<common::IEventThreadFactory> &eventThreadFactory)
    : IpcModule(ipcClient), m_eventThread(eventThreadFactory->createEventThread("rialto-media-keys-events"))
{
    RIALTO_CLIENT_LOG_DEBUG("entry:");

    if (!attachChannel())
    {
        throw std::runtime_error("Failed attach to the ipc channel");
    }

    if (!createMediaKeys(keySystem))
    {
        detachChannel();
        throw std::runtime_error("Could not create the media keys instance");
    }
}

MediaKeysIpc::~MediaKeysIpc()
{
    RIALTO_CLIENT_LOG_DEBUG("entry:");

    destroyMediaKeys();

    detachChannel();

    m_eventThread.reset();
}

bool MediaKeysIpc::createRpcStubs(const std::shared_ptr<ipc::IChannel> &ipcChannel)
{
    m_mediaKeysStub = std::make_unique<::firebolt::rialto::MediaKeysModule_Stub>(ipcChannel.get());
    if (!m_mediaKeysStub)
    {
        return false;
    }
    return true;
}

bool MediaKeysIpc::subscribeToEvents(const std::shared_ptr<ipc::IChannel> &ipcChannel)
{
    if (!ipcChannel)
    {
        return false;
    }

    int eventTag = ipcChannel->subscribe<firebolt::rialto::LicenseRequestEvent>(
        [this](const std::shared_ptr<firebolt::rialto::LicenseRequestEvent> &event)
        { m_eventThread->add(&MediaKeysIpc::onLicenseRequest, this, event); });
    if (eventTag < 0)
        return false;
    m_eventTags.push_back(eventTag);

    eventTag = ipcChannel->subscribe<firebolt::rialto::LicenseRenewalEvent>(
        [this](const std::shared_ptr<firebolt::rialto::LicenseRenewalEvent> &event)
        { m_eventThread->add(&MediaKeysIpc::onLicenseRenewal, this, event); });
    if (eventTag < 0)
        return false;
    m_eventTags.push_back(eventTag);

    eventTag = ipcChannel->subscribe<firebolt::rialto::KeyStatusesChangedEvent>(
        [this](const std::shared_ptr<firebolt::rialto::KeyStatusesChangedEvent> &event)
        { m_eventThread->add(&MediaKeysIpc::onKeyStatusesChanged, this, event); });
    if (eventTag < 0)
        return false;
    m_eventTags.push_back(eventTag);

    return true;
}

bool MediaKeysIpc::createMediaKeys(const std::string &keySystem)
{
    if (!reattachChannelIfRequired())
    {
        RIALTO_CLIENT_LOG_ERROR("Reattachment of the ipc channel failed, ipc disconnected");
        return false;
    }

    firebolt::rialto::CreateMediaKeysRequest request;

    request.set_key_system(keySystem);

    firebolt::rialto::CreateMediaKeysResponse response;
    auto ipcController = m_ipc.createRpcController();
    auto blockingClosure = m_ipc.createBlockingClosure();
    m_mediaKeysStub->createMediaKeys(ipcController.get(), &request, &response, blockingClosure.get());

    // wait for the call to complete
    blockingClosure->wait();

    // check the result
    if (ipcController->Failed())
    {
        RIALTO_CLIENT_LOG_ERROR("failed to create media keys due to '%s'", ipcController->ErrorText().c_str());
        return false;
    }

    m_mediaKeysHandle = response.media_keys_handle();

    return true;
}

void MediaKeysIpc::destroyMediaKeys()
{
    if (!reattachChannelIfRequired())
    {
        RIALTO_CLIENT_LOG_ERROR("Reattachment of the ipc channel failed, ipc disconnected");
        return;
    }

    firebolt::rialto::DestroyMediaKeysRequest request;
    request.set_media_keys_handle(m_mediaKeysHandle);

    firebolt::rialto::DestroyMediaKeysResponse response;
    auto ipcController = m_ipc.createRpcController();
    auto blockingClosure = m_ipc.createBlockingClosure();
    m_mediaKeysStub->destroyMediaKeys(ipcController.get(), &request, &response, blockingClosure.get());

    // wait for the call to complete
    blockingClosure->wait();

    // check the result
    if (ipcController->Failed())
    {
        RIALTO_CLIENT_LOG_ERROR("failed to destroy media keys due to '%s'", ipcController->ErrorText().c_str());
    }
}

MediaKeyErrorStatus MediaKeysIpc::selectKeyId(int32_t keySessionId, const std::vector<uint8_t> &keyId)
{
    RIALTO_CLIENT_LOG_ERROR("Not Implemented");
    return MediaKeyErrorStatus::FAIL;
}

bool MediaKeysIpc::containsKey(int32_t keySessionId, const std::vector<uint8_t> &keyId)
{
    if (!reattachChannelIfRequired())
    {
        RIALTO_CLIENT_LOG_ERROR("Reattachment of the ipc channel failed, ipc disconnected");
        return false;
    }

    firebolt::rialto::ContainsKeyRequest request;
    request.set_media_keys_handle(m_mediaKeysHandle);
    request.set_key_session_id(keySessionId);
    for (const auto &keyIdItem : keyId)
    {
        request.add_key_id(keyIdItem);
    }

    firebolt::rialto::ContainsKeyResponse response;
    // Default return value to false
    response.set_contains_key(false);

    auto ipcController = m_ipc.createRpcController();
    auto blockingClosure = m_ipc.createBlockingClosure();
    m_mediaKeysStub->containsKey(ipcController.get(), &request, &response, blockingClosure.get());

    // wait for the call to complete
    blockingClosure->wait();

    if (ipcController->Failed())
    {
        RIALTO_CLIENT_LOG_ERROR("containsKey failed due to '%s'", ipcController->ErrorText().c_str());
        return false;
    }

    return response.contains_key();
}

MediaKeyErrorStatus MediaKeysIpc::createKeySession(KeySessionType sessionType, std::weak_ptr<IMediaKeysClient> client,
                                                   bool isLDL, int32_t &keySessionId)
{
    if (!reattachChannelIfRequired())
    {
        RIALTO_CLIENT_LOG_ERROR("Reattachment of the ipc channel failed, ipc disconnected");
        return MediaKeyErrorStatus::FAIL;
    }

    CreateKeySessionRequest_KeySessionType protoSessionType = CreateKeySessionRequest_KeySessionType_UNKNOWN;
    switch (sessionType)
    {
    case KeySessionType::TEMPORARY:
        protoSessionType = CreateKeySessionRequest_KeySessionType_TEMPORARY;
        break;
    case KeySessionType::PERSISTENT_LICENCE:
        protoSessionType = CreateKeySessionRequest_KeySessionType_PERSISTENT_LICENCE;
        break;
    case KeySessionType::PERSISTENT_RELEASE_MESSAGE:
        protoSessionType = CreateKeySessionRequest_KeySessionType_PERSISTENT_RELEASE_MESSAGE;
        break;
    default:
        RIALTO_CLIENT_LOG_WARN("Recieved unknown key session type");
        break;
    }

    firebolt::rialto::CreateKeySessionRequest request;
    request.set_media_keys_handle(m_mediaKeysHandle);
    request.set_session_type(protoSessionType);
    request.set_is_ldl(isLDL);

    firebolt::rialto::CreateKeySessionResponse response;
    // Default error status to FAIL
    response.set_error_status(ProtoMediaKeyErrorStatus::FAIL);

    auto ipcController = m_ipc.createRpcController();
    auto blockingClosure = m_ipc.createBlockingClosure();
    m_mediaKeysStub->createKeySession(ipcController.get(), &request, &response, blockingClosure.get());

    // wait for the call to complete
    blockingClosure->wait();

    MediaKeyErrorStatus status =
        getMediaKeyErrorStatusFromResponse("createKeySession", ipcController, response.error_status());
    if (MediaKeyErrorStatus::OK == status)
    {
        keySessionId = response.key_session_id();
        m_mediaKeysIpcClient = client;
    }

    return status;
}

MediaKeyErrorStatus MediaKeysIpc::generateRequest(int32_t keySessionId, InitDataType initDataType,
                                                  const std::vector<uint8_t> &initData)
{
    if (!reattachChannelIfRequired())
    {
        RIALTO_CLIENT_LOG_ERROR("Reattachment of the ipc channel failed, ipc disconnected");
        return MediaKeyErrorStatus::FAIL;
    }

    GenerateRequestRequest_InitDataType protoInitDataType = GenerateRequestRequest_InitDataType_UNKNOWN;
    switch (initDataType)
    {
    case InitDataType::CENC:
        protoInitDataType = GenerateRequestRequest_InitDataType_CENC;
        break;
    case InitDataType::KEY_IDS:
        protoInitDataType = GenerateRequestRequest_InitDataType_KEY_IDS;
        break;
    case InitDataType::WEBM:
        protoInitDataType = GenerateRequestRequest_InitDataType_WEBM;
        break;
    case InitDataType::DRMHEADER:
        protoInitDataType = GenerateRequestRequest_InitDataType_DRMHEADER;
        break;
    default:
        RIALTO_CLIENT_LOG_WARN("Recieved unknown init data type");
        break;
    }

    firebolt::rialto::GenerateRequestRequest request;
    request.set_media_keys_handle(m_mediaKeysHandle);
    request.set_key_session_id(keySessionId);
    request.set_init_data_type(protoInitDataType);

    for (auto it = initData.begin(); it != initData.end(); it++)
    {
        request.add_init_data(*it);
    }

    firebolt::rialto::GenerateRequestResponse response;
    // Default error status to FAIL
    response.set_error_status(ProtoMediaKeyErrorStatus::FAIL);

    auto ipcController = m_ipc.createRpcController();
    auto blockingClosure = m_ipc.createBlockingClosure();
    m_mediaKeysStub->generateRequest(ipcController.get(), &request, &response, blockingClosure.get());

    // wait for the call to complete
    blockingClosure->wait();

    return getMediaKeyErrorStatusFromResponse("generateRequest", ipcController, response.error_status());
}

MediaKeyErrorStatus MediaKeysIpc::loadSession(int32_t keySessionId)
{
    if (!reattachChannelIfRequired())
    {
        RIALTO_CLIENT_LOG_ERROR("Reattachment of the ipc channel failed, ipc disconnected");
        return MediaKeyErrorStatus::FAIL;
    }

    firebolt::rialto::LoadSessionRequest request;
    request.set_media_keys_handle(m_mediaKeysHandle);
    request.set_key_session_id(keySessionId);

    firebolt::rialto::LoadSessionResponse response;
    // Default error status to FAIL
    response.set_error_status(ProtoMediaKeyErrorStatus::FAIL);

    auto ipcController = m_ipc.createRpcController();
    auto blockingClosure = m_ipc.createBlockingClosure();
    m_mediaKeysStub->loadSession(ipcController.get(), &request, &response, blockingClosure.get());

    // wait for the call to complete
    blockingClosure->wait();

    return getMediaKeyErrorStatusFromResponse("loadSession", ipcController, response.error_status());
}

MediaKeyErrorStatus MediaKeysIpc::updateSession(int32_t keySessionId, const std::vector<uint8_t> &responseData)
{
    if (!reattachChannelIfRequired())
    {
        RIALTO_CLIENT_LOG_ERROR("Reattachment of the ipc channel failed, ipc disconnected");
        return MediaKeyErrorStatus::FAIL;
    }

    firebolt::rialto::UpdateSessionRequest request;
    request.set_media_keys_handle(m_mediaKeysHandle);
    request.set_key_session_id(keySessionId);

    for (auto it = responseData.begin(); it != responseData.end(); it++)
    {
        request.add_response_data(*it);
    }

    firebolt::rialto::UpdateSessionResponse response;
    // Default error status to FAIL
    response.set_error_status(ProtoMediaKeyErrorStatus::FAIL);

    auto ipcController = m_ipc.createRpcController();
    auto blockingClosure = m_ipc.createBlockingClosure();
    m_mediaKeysStub->updateSession(ipcController.get(), &request, &response, blockingClosure.get());

    // wait for the call to complete
    blockingClosure->wait();

    return getMediaKeyErrorStatusFromResponse("updateSession", ipcController, response.error_status());
}

MediaKeyErrorStatus MediaKeysIpc::setDrmHeader(int32_t keySessionId, const std::vector<uint8_t> &requestData)
{
    if (!reattachChannelIfRequired())
    {
        RIALTO_CLIENT_LOG_ERROR("Reattachment of the ipc channel failed, ipc disconnected");
        return MediaKeyErrorStatus::FAIL;
    }

    firebolt::rialto::SetDrmHeaderRequest request;
    request.set_media_keys_handle(m_mediaKeysHandle);
    request.set_key_session_id(keySessionId);
    for (const auto &requestDataItem : requestData)
    {
        request.add_request_data(requestDataItem);
    }

    firebolt::rialto::SetDrmHeaderResponse response;
    // Default error status to FAIL
    response.set_error_status(ProtoMediaKeyErrorStatus::FAIL);

    auto ipcController = m_ipc.createRpcController();
    auto blockingClosure = m_ipc.createBlockingClosure();
    m_mediaKeysStub->setDrmHeader(ipcController.get(), &request, &response, blockingClosure.get());

    // wait for the call to complete
    blockingClosure->wait();

    return getMediaKeyErrorStatusFromResponse("setDrmHeader", ipcController, response.error_status());
}

MediaKeyErrorStatus MediaKeysIpc::closeKeySession(int32_t keySessionId)
{
    if (!reattachChannelIfRequired())
    {
        RIALTO_CLIENT_LOG_ERROR("Reattachment of the ipc channel failed, ipc disconnected");
        return MediaKeyErrorStatus::FAIL;
    }

    // Reset client
    m_mediaKeysIpcClient.reset();

    firebolt::rialto::CloseKeySessionRequest request;
    request.set_media_keys_handle(m_mediaKeysHandle);
    request.set_key_session_id(keySessionId);

    firebolt::rialto::CloseKeySessionResponse response;
    // Default error status to FAIL
    response.set_error_status(ProtoMediaKeyErrorStatus::FAIL);

    auto ipcController = m_ipc.createRpcController();
    auto blockingClosure = m_ipc.createBlockingClosure();
    m_mediaKeysStub->closeKeySession(ipcController.get(), &request, &response, blockingClosure.get());

    // wait for the call to complete
    blockingClosure->wait();

    return getMediaKeyErrorStatusFromResponse("closeKeySession", ipcController, response.error_status());
}

MediaKeyErrorStatus MediaKeysIpc::removeKeySession(int32_t keySessionId)
{
    if (!reattachChannelIfRequired())
    {
        RIALTO_CLIENT_LOG_ERROR("Reattachment of the ipc channel failed, ipc disconnected");
        return MediaKeyErrorStatus::FAIL;
    }

    firebolt::rialto::RemoveKeySessionRequest request;
    request.set_media_keys_handle(m_mediaKeysHandle);
    request.set_key_session_id(keySessionId);

    firebolt::rialto::RemoveKeySessionResponse response;
    // Default error status to FAIL
    response.set_error_status(ProtoMediaKeyErrorStatus::FAIL);

    auto ipcController = m_ipc.createRpcController();
    auto blockingClosure = m_ipc.createBlockingClosure();
    m_mediaKeysStub->removeKeySession(ipcController.get(), &request, &response, blockingClosure.get());

    // wait for the call to complete
    blockingClosure->wait();

    return getMediaKeyErrorStatusFromResponse("removeKeySession", ipcController, response.error_status());
}

MediaKeyErrorStatus MediaKeysIpc::deleteDrmStore()
{
    if (!reattachChannelIfRequired())
    {
        RIALTO_CLIENT_LOG_ERROR("Reattachment of the ipc channel failed, ipc disconnected");
        return MediaKeyErrorStatus::FAIL;
    }

    firebolt::rialto::DeleteDrmStoreRequest request;
    request.set_media_keys_handle(m_mediaKeysHandle);

    firebolt::rialto::DeleteDrmStoreResponse response;
    // Default error status to FAIL
    response.set_error_status(ProtoMediaKeyErrorStatus::FAIL);

    auto ipcController = m_ipc.createRpcController();
    auto blockingClosure = m_ipc.createBlockingClosure();
    m_mediaKeysStub->deleteDrmStore(ipcController.get(), &request, &response, blockingClosure.get());

    // wait for the call to complete
    blockingClosure->wait();

    return getMediaKeyErrorStatusFromResponse("deleteDrmStore", ipcController, response.error_status());
}

MediaKeyErrorStatus MediaKeysIpc::deleteKeyStore()
{
    if (!reattachChannelIfRequired())
    {
        RIALTO_CLIENT_LOG_ERROR("Reattachment of the ipc channel failed, ipc disconnected");
        return MediaKeyErrorStatus::FAIL;
    }

    firebolt::rialto::DeleteKeyStoreRequest request;
    request.set_media_keys_handle(m_mediaKeysHandle);

    firebolt::rialto::DeleteKeyStoreResponse response;
    // Default error status to FAIL
    response.set_error_status(ProtoMediaKeyErrorStatus::FAIL);

    auto ipcController = m_ipc.createRpcController();
    auto blockingClosure = m_ipc.createBlockingClosure();
    m_mediaKeysStub->deleteKeyStore(ipcController.get(), &request, &response, blockingClosure.get());

    // wait for the call to complete
    blockingClosure->wait();

    return getMediaKeyErrorStatusFromResponse("deleteKeyStore", ipcController, response.error_status());
}

MediaKeyErrorStatus MediaKeysIpc::getDrmStoreHash(std::vector<unsigned char> &drmStoreHash)
{
    if (!reattachChannelIfRequired())
    {
        RIALTO_CLIENT_LOG_ERROR("Reattachment of the ipc channel failed, ipc disconnected");
        return MediaKeyErrorStatus::FAIL;
    }

    firebolt::rialto::GetDrmStoreHashRequest request;
    request.set_media_keys_handle(m_mediaKeysHandle);

    firebolt::rialto::GetDrmStoreHashResponse response;
    // Default error status to FAIL
    response.set_error_status(ProtoMediaKeyErrorStatus::FAIL);

    auto ipcController = m_ipc.createRpcController();
    auto blockingClosure = m_ipc.createBlockingClosure();
    m_mediaKeysStub->getDrmStoreHash(ipcController.get(), &request, &response, blockingClosure.get());

    // wait for the call to complete
    blockingClosure->wait();

    if (ProtoMediaKeyErrorStatus::OK == response.error_status())
    {
        drmStoreHash = std::vector<unsigned char>(response.drm_store_hash().begin(), response.drm_store_hash().end());
    }

    return getMediaKeyErrorStatusFromResponse("getDrmStoreHash", ipcController, response.error_status());
}

MediaKeyErrorStatus MediaKeysIpc::getKeyStoreHash(std::vector<unsigned char> &keyStoreHash)
{
    if (!reattachChannelIfRequired())
    {
        RIALTO_CLIENT_LOG_ERROR("Reattachment of the ipc channel failed, ipc disconnected");
        return MediaKeyErrorStatus::FAIL;
    }

    firebolt::rialto::GetKeyStoreHashRequest request;
    request.set_media_keys_handle(m_mediaKeysHandle);

    firebolt::rialto::GetKeyStoreHashResponse response;
    // Default error status to FAIL
    response.set_error_status(ProtoMediaKeyErrorStatus::FAIL);

    auto ipcController = m_ipc.createRpcController();
    auto blockingClosure = m_ipc.createBlockingClosure();
    m_mediaKeysStub->getKeyStoreHash(ipcController.get(), &request, &response, blockingClosure.get());

    // wait for the call to complete
    blockingClosure->wait();

    if (ProtoMediaKeyErrorStatus::OK == response.error_status())
    {
        keyStoreHash = std::vector<unsigned char>(response.key_store_hash().begin(), response.key_store_hash().end());
    }

    return getMediaKeyErrorStatusFromResponse("getKeyStoreHash", ipcController, response.error_status());
}

MediaKeyErrorStatus MediaKeysIpc::getLdlSessionsLimit(uint32_t &ldlLimit)
{
    if (!reattachChannelIfRequired())
    {
        RIALTO_CLIENT_LOG_ERROR("Reattachment of the ipc channel failed, ipc disconnected");
        return MediaKeyErrorStatus::FAIL;
    }

    firebolt::rialto::GetLdlSessionsLimitRequest request;
    request.set_media_keys_handle(m_mediaKeysHandle);

    firebolt::rialto::GetLdlSessionsLimitResponse response;
    // Default error status to FAIL
    response.set_error_status(ProtoMediaKeyErrorStatus::FAIL);

    auto ipcController = m_ipc.createRpcController();
    auto blockingClosure = m_ipc.createBlockingClosure();
    m_mediaKeysStub->getLdlSessionsLimit(ipcController.get(), &request, &response, blockingClosure.get());

    // wait for the call to complete
    blockingClosure->wait();

    if (ProtoMediaKeyErrorStatus::OK == response.error_status())
    {
        ldlLimit = response.ldl_limit();
    }

    return getMediaKeyErrorStatusFromResponse("getLdlSessionsLimit", ipcController, response.error_status());
}

MediaKeyErrorStatus MediaKeysIpc::getLastDrmError(int32_t keySessionId, uint32_t &errorCode)
{
    if (!reattachChannelIfRequired())
    {
        RIALTO_CLIENT_LOG_ERROR("Reattachment of the ipc channel failed, ipc disconnected");
        return MediaKeyErrorStatus::FAIL;
    }

    firebolt::rialto::GetLastDrmErrorRequest request;
    request.set_media_keys_handle(m_mediaKeysHandle);
    request.set_key_session_id(keySessionId);

    firebolt::rialto::GetLastDrmErrorResponse response;
    // Default error status to FAIL
    response.set_error_status(ProtoMediaKeyErrorStatus::FAIL);

    auto ipcController = m_ipc.createRpcController();
    auto blockingClosure = m_ipc.createBlockingClosure();
    m_mediaKeysStub->getLastDrmError(ipcController.get(), &request, &response, blockingClosure.get());

    // wait for the call to complete
    blockingClosure->wait();

    if (ProtoMediaKeyErrorStatus::OK == response.error_status())
    {
        errorCode = response.error_code();
    }

    return getMediaKeyErrorStatusFromResponse("getLastDrmError", ipcController, response.error_status());
}

MediaKeyErrorStatus MediaKeysIpc::getDrmTime(uint64_t &drmTime)
{
    if (!reattachChannelIfRequired())
    {
        RIALTO_CLIENT_LOG_ERROR("Reattachment of the ipc channel failed, ipc disconnected");
        return MediaKeyErrorStatus::FAIL;
    }

    firebolt::rialto::GetDrmTimeRequest request;
    request.set_media_keys_handle(m_mediaKeysHandle);

    firebolt::rialto::GetDrmTimeResponse response;
    // Default error status to FAIL
    response.set_error_status(ProtoMediaKeyErrorStatus::FAIL);

    auto ipcController = m_ipc.createRpcController();
    auto blockingClosure = m_ipc.createBlockingClosure();
    m_mediaKeysStub->getDrmTime(ipcController.get(), &request, &response, blockingClosure.get());

    // wait for the call to complete
    blockingClosure->wait();

    if (ProtoMediaKeyErrorStatus::OK == response.error_status())
    {
        drmTime = response.drm_time();
    }

    return getMediaKeyErrorStatusFromResponse("getDrmTime", ipcController, response.error_status());
}

MediaKeyErrorStatus MediaKeysIpc::getCdmKeySessionId(int32_t keySessionId, std::string &cdmKeySessionId)
{
    if (!reattachChannelIfRequired())
    {
        RIALTO_CLIENT_LOG_ERROR("Reattachment of the ipc channel failed, ipc disconnected");
        return MediaKeyErrorStatus::FAIL;
    }

    firebolt::rialto::GetCdmKeySessionIdRequest request;
    request.set_media_keys_handle(m_mediaKeysHandle);
    request.set_key_session_id(keySessionId);

    firebolt::rialto::GetCdmKeySessionIdResponse response;
    // Default error status to FAIL
    response.set_error_status(ProtoMediaKeyErrorStatus::FAIL);

    auto ipcController = m_ipc.createRpcController();
    auto blockingClosure = m_ipc.createBlockingClosure();
    m_mediaKeysStub->getCdmKeySessionId(ipcController.get(), &request, &response, blockingClosure.get());

    // wait for the call to complete
    blockingClosure->wait();

    cdmKeySessionId = response.cdm_key_session_id();

    return getMediaKeyErrorStatusFromResponse("getCdmKeySessionId", ipcController, response.error_status());
}

MediaKeyErrorStatus MediaKeysIpc::releaseKeySession(int32_t keySessionId)
{
    if (!reattachChannelIfRequired())
    {
        RIALTO_CLIENT_LOG_ERROR("Reattachment of the ipc channel failed, ipc disconnected");
        return MediaKeyErrorStatus::FAIL;
    }

    // Reset client
    m_mediaKeysIpcClient.reset();

    firebolt::rialto::ReleaseKeySessionRequest request;
    request.set_media_keys_handle(m_mediaKeysHandle);
    request.set_key_session_id(keySessionId);

    firebolt::rialto::ReleaseKeySessionResponse response;
    // Default error status to FAIL
    response.set_error_status(ProtoMediaKeyErrorStatus::FAIL);

    auto ipcController = m_ipc.createRpcController();
    auto blockingClosure = m_ipc.createBlockingClosure();
    m_mediaKeysStub->releaseKeySession(ipcController.get(), &request, &response, blockingClosure.get());

    // wait for the call to complete
    blockingClosure->wait();

    return getMediaKeyErrorStatusFromResponse("releaseKeySession", ipcController, response.error_status());
}

void MediaKeysIpc::onLicenseRequest(const std::shared_ptr<firebolt::rialto::LicenseRequestEvent> &event)
{
    std::shared_ptr<IMediaKeysClient> mediaKeysIpcClient = m_mediaKeysIpcClient.lock();

    /* Ignore event if not for this session or no client */
    if ((event->media_keys_handle() == m_mediaKeysHandle) && (mediaKeysIpcClient))
    {
        std::vector<unsigned char> messageVector = std::vector<unsigned char>{event->license_request_message().begin(),
                                                                              event->license_request_message().end()};
        mediaKeysIpcClient->onLicenseRequest(event->key_session_id(), messageVector, event->url());
    }
}

void MediaKeysIpc::onLicenseRenewal(const std::shared_ptr<firebolt::rialto::LicenseRenewalEvent> &event)
{
    std::shared_ptr<IMediaKeysClient> mediaKeysIpcClient = m_mediaKeysIpcClient.lock();

    /* Ignore event if not for this session or no client */
    if ((event->media_keys_handle() == m_mediaKeysHandle) && (mediaKeysIpcClient))
    {
        std::vector<unsigned char> messageVector = std::vector<unsigned char>{event->license_renewal_message().begin(),
                                                                              event->license_renewal_message().end()};
        mediaKeysIpcClient->onLicenseRenewal(event->key_session_id(), messageVector);
    }
}

void MediaKeysIpc::onKeyStatusesChanged(const std::shared_ptr<firebolt::rialto::KeyStatusesChangedEvent> &event)
{
    std::shared_ptr<IMediaKeysClient> mediaKeysIpcClient = m_mediaKeysIpcClient.lock();

    /* Ignore event if not for this session or no client */
    if ((event->media_keys_handle() == m_mediaKeysHandle) && (mediaKeysIpcClient))
    {
        KeyStatusVector keyStatuses;
        for (auto it = event->key_statuses().begin(); it != event->key_statuses().end(); it++)
        {
            std::vector<unsigned char> keyVector = std::vector<unsigned char>{it->key_id().begin(), it->key_id().end()};
            KeyStatus keyStatus = convertKeyStatus(it->key_status());
            keyStatuses.push_back(std::make_pair(keyVector, keyStatus));
        }
        mediaKeysIpcClient->onKeyStatusesChanged(event->key_session_id(), keyStatuses);
    }
}

MediaKeyErrorStatus
MediaKeysIpc::getMediaKeyErrorStatusFromResponse(const std::string methodName,
                                                 const std::shared_ptr<google::protobuf::RpcController> &controller,
                                                 ProtoMediaKeyErrorStatus status)
{
    if (controller->Failed())
    {
        RIALTO_CLIENT_LOG_ERROR("%s failed due to '%s'", methodName.c_str(), controller->ErrorText().c_str());
        return MediaKeyErrorStatus::FAIL;
    }
    MediaKeyErrorStatus returnStatus = convertMediaKeyErrorStatus(status);
    if (MediaKeyErrorStatus::OK != returnStatus)
    {
        RIALTO_CLIENT_LOG_ERROR("%s failed due to MediaKeyErrorStatus '%s'", methodName.c_str(), toString(returnStatus));
        return returnStatus;
    }

    return returnStatus;
}

MediaKeyErrorStatus MediaKeysIpc::getMetricSystemData(std::vector<uint8_t> &buffer)
{
    if (!reattachChannelIfRequired())
    {
        RIALTO_CLIENT_LOG_ERROR("Reattachment of the ipc channel failed, ipc disconnected");
        return MediaKeyErrorStatus::FAIL;
    }

    firebolt::rialto::GetMetricSystemDataRequest request;
    request.set_media_keys_handle(m_mediaKeysHandle);

    firebolt::rialto::GetMetricSystemDataResponse response;
    // Default error status to FAIL
    response.set_error_status(ProtoMediaKeyErrorStatus::FAIL);

    auto ipcController = m_ipc.createRpcController();
    auto blockingClosure = m_ipc.createBlockingClosure();
    m_mediaKeysStub->getMetricSystemData(ipcController.get(), &request, &response, blockingClosure.get());

    // wait for the call to complete
    blockingClosure->wait();

    if (ProtoMediaKeyErrorStatus::OK == response.error_status())
    {
        buffer = std::vector<uint8_t>(response.buffer().begin(), response.buffer().end());
    }

    return getMediaKeyErrorStatusFromResponse("getMetricSystemData", ipcController, response.error_status());
}

}; // namespace firebolt::rialto::client
