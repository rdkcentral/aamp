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

#ifndef FIREBOLT_RIALTO_CLIENT_MEDIA_KEYS_IPC_H_
#define FIREBOLT_RIALTO_CLIENT_MEDIA_KEYS_IPC_H_

#include "IEventThread.h"
#include "IMediaKeysClient.h"
#include "IMediaKeysIpcFactory.h"
#include "IpcModule.h"
#include <memory>
#include <string>
#include <vector>

#include "mediakeysmodule.pb.h"

namespace firebolt::rialto::client
{
/**
 * @brief IMediaKeysIpc factory class definition.
 */
class MediaKeysIpcFactory : public IMediaKeysIpcFactory
{
public:
    MediaKeysIpcFactory() = default;
    ~MediaKeysIpcFactory() override = default;

    std::unique_ptr<IMediaKeys> createMediaKeysIpc(const std::string &keySystem) const override;
};

/**
 * @brief The definition of the MediaKeysIpc.
 */
class MediaKeysIpc : public IMediaKeys, public IpcModule
{
public:
    /**
     * @brief The constructor.
     *
     * @param[in] keySystem             : The key system for which to create a Media Keys Ipc instance
     * @param[in] ipcClient             : The ipc client
     * @param[in] eventThreadFactory    : The event thread factory
     */
    MediaKeysIpc(const std::string &keySystem, IIpcClient &ipcClient,
                 const std::shared_ptr<common::IEventThreadFactory> &eventThreadFactory);

    /**
     * @brief Virtual destructor.
     */
    virtual ~MediaKeysIpc();

    MediaKeyErrorStatus selectKeyId(int32_t keySessionId, const std::vector<uint8_t> &keyId) override;

    bool containsKey(int32_t keySessionId, const std::vector<uint8_t> &keyId) override;

    MediaKeyErrorStatus createKeySession(KeySessionType sessionType, std::weak_ptr<IMediaKeysClient> client, bool isLDL,
                                         int32_t &keySessionId) override;

    MediaKeyErrorStatus generateRequest(int32_t keySessionId, InitDataType initDataType,
                                        const std::vector<uint8_t> &initData) override;

    MediaKeyErrorStatus loadSession(int32_t keySessionId) override;

    MediaKeyErrorStatus updateSession(int32_t keySessionId, const std::vector<uint8_t> &responseData) override;

    MediaKeyErrorStatus setDrmHeader(int32_t keySessionId, const std::vector<uint8_t> &requestData) override;

    MediaKeyErrorStatus closeKeySession(int32_t keySessionId) override;

    MediaKeyErrorStatus removeKeySession(int32_t keySessionId) override;

    MediaKeyErrorStatus deleteDrmStore() override;

    MediaKeyErrorStatus deleteKeyStore() override;

    MediaKeyErrorStatus getDrmStoreHash(std::vector<unsigned char> &drmStoreHash) override;

    MediaKeyErrorStatus getKeyStoreHash(std::vector<unsigned char> &keyStoreHash) override;

    MediaKeyErrorStatus getLdlSessionsLimit(uint32_t &ldlLimit) override;

    MediaKeyErrorStatus getLastDrmError(int32_t keySessionId, uint32_t &errorCode) override;

    MediaKeyErrorStatus getDrmTime(uint64_t &drmTime) override;

    MediaKeyErrorStatus getCdmKeySessionId(int32_t keySessionId, std::string &cdmKeySessionId) override;

    MediaKeyErrorStatus releaseKeySession(int32_t keySessionId) override;

    MediaKeyErrorStatus getMetricSystemData(std::vector<uint8_t> &buffer) override;

private:
    /**
     * @brief The ipc protobuf media keys stub.
     */
    std::unique_ptr<::firebolt::rialto::MediaKeysModule_Stub> m_mediaKeysStub;

    /**
     * @brief The media key handle for the current session.
     */
    std::atomic<int> m_mediaKeysHandle;

    /**
     * @brief Thread for handling media player events from the server.
     */
    std::unique_ptr<common::IEventThread> m_eventThread;

    /**
     * @brief The media keys client ipc.
     */
    std::weak_ptr<IMediaKeysClient> m_mediaKeysIpcClient;

    bool createRpcStubs(const std::shared_ptr<ipc::IChannel> &ipcChannel) override;

    bool subscribeToEvents(const std::shared_ptr<ipc::IChannel> &ipcChannel) override;

    /**
     * @brief Handler for a license request from the server.
     *
     * @param[in] event : The license request event structure.
     */
    void onLicenseRequest(const std::shared_ptr<rialto::LicenseRequestEvent> &event);

    /**
     * @brief Handler for a license renewal from the server.
     *
     * @param[in] event : The license renewal event structure.
     */
    void onLicenseRenewal(const std::shared_ptr<rialto::LicenseRenewalEvent> &event);

    /**
     * @brief Handler for a key statues change from the server.
     *
     * @param[in] event : The key statues change event structure.
     */
    void onKeyStatusesChanged(const std::shared_ptr<rialto::KeyStatusesChangedEvent> &event);

    /**
     * @brief Create a new media keys instance.
     *
     * @retval true on success, false otherwise.
     */
    bool createMediaKeys(const std::string &keySystem);

    /**
     * @brief Destroy the current media keys instance.
     */
    void destroyMediaKeys();

    /**
     * @brief Checks the Ipc controller and ProtoMediaKeyErrorStatus for failures and return the MediaKeyErrorStatus.
     *
     * @param[in] methodName : The name of the ipc method.
     * @param[in] controller : The rpc controller object.
     * @param[in] status     : The protobuf response status.
     *
     * @retval The MediaKeyErrorStatus to return.
     */
    MediaKeyErrorStatus
    getMediaKeyErrorStatusFromResponse(const std::string methodName,
                                       const std::shared_ptr<google::protobuf::RpcController> &controller,
                                       ProtoMediaKeyErrorStatus status);
};

}; // namespace firebolt::rialto::client

#endif // FIREBOLT_RIALTO_CLIENT_MEDIA_KEYS_IPC_H_
