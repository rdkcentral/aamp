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

#ifndef FIREBOLT_RIALTO_CLIENT_MEDIA_KEYS_CAPABILITIES_IPC_H_
#define FIREBOLT_RIALTO_CLIENT_MEDIA_KEYS_CAPABILITIES_IPC_H_

#include "IEventThread.h"
#include "IMediaKeysCapabilitiesIpcFactory.h"
#include "IpcModule.h"
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "mediakeyscapabilitiesmodule.pb.h"

namespace firebolt::rialto::client
{
/**
 * @brief IMediaKeysCapabilitiesIpc factory class definition.
 */
class MediaKeysCapabilitiesIpcFactory : public IMediaKeysCapabilitiesIpcFactory
{
public:
    MediaKeysCapabilitiesIpcFactory() = default;
    ~MediaKeysCapabilitiesIpcFactory() override = default;

    /**
     * @brief Weak pointer to the singleton object.
     */
    static std::weak_ptr<IMediaKeysCapabilities> m_mediaKeysCapabilitiesIpc;

    /**
     * @brief Mutex protection for creation of the MediaKeysCapabilitiesIpc object.
     */
    static std::mutex m_creationMutex;

    std::shared_ptr<IMediaKeysCapabilities> getMediaKeysCapabilitiesIpc() const override;
};

/**
 * @brief The definition of the MediaKeysCapabilitiesIpc.
 */
class MediaKeysCapabilitiesIpc : public IMediaKeysCapabilities, public IpcModule
{
public:
    /**
     * @brief The constructor.
     *
     * @param[in] ipcClient      : The ipc client
     */
    explicit MediaKeysCapabilitiesIpc(IIpcClient &ipcClientFactory);

    /**
     * @brief Virtual destructor.
     */
    virtual ~MediaKeysCapabilitiesIpc();

    std::vector<std::string> getSupportedKeySystems() override;

    bool supportsKeySystem(const std::string &keySystem) override;

    bool getSupportedKeySystemVersion(const std::string &keySystem, std::string &version) override;

    bool isServerCertificateSupported(const std::string &keySystem) override;

private:
    /**
     * @brief The ipc protobuf media keys capabilities stub.
     */
    std::unique_ptr<::firebolt::rialto::MediaKeysCapabilitiesModule_Stub> m_mediaKeysCapabilitiesStub;

    bool createRpcStubs(const std::shared_ptr<ipc::IChannel> &ipcChannel) override;

    bool subscribeToEvents(const std::shared_ptr<ipc::IChannel> &ipcChannel) override { return true; }
};

}; // namespace firebolt::rialto::client

#endif // FIREBOLT_RIALTO_CLIENT_MEDIA_KEYS_CAPABILITIES_IPC_H_
