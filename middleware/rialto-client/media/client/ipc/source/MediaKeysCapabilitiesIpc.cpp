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

#include "MediaKeysCapabilitiesIpc.h"
#include "RialtoClientLogging.h"

namespace firebolt::rialto::client
{
std::weak_ptr<IMediaKeysCapabilities> MediaKeysCapabilitiesIpcFactory::m_mediaKeysCapabilitiesIpc;
std::mutex MediaKeysCapabilitiesIpcFactory::m_creationMutex;

std::shared_ptr<IMediaKeysCapabilitiesIpcFactory> IMediaKeysCapabilitiesIpcFactory::createFactory()
{
    std::shared_ptr<IMediaKeysCapabilitiesIpcFactory> factory;

    try
    {
        factory = std::make_shared<MediaKeysCapabilitiesIpcFactory>();
    }
    catch (const std::exception &e)
    {
        RIALTO_CLIENT_LOG_ERROR("Failed to create the media keys capabilities ipc factory, reason: %s", e.what());
    }

    return factory;
}

std::shared_ptr<IMediaKeysCapabilities> MediaKeysCapabilitiesIpcFactory::getMediaKeysCapabilitiesIpc() const
{
    std::lock_guard<std::mutex> lock{m_creationMutex};

    std::shared_ptr<IMediaKeysCapabilities> mediaKeysCapabilitiesIpc =
        MediaKeysCapabilitiesIpcFactory::m_mediaKeysCapabilitiesIpc.lock();

    if (!mediaKeysCapabilitiesIpc)
    {
        try
        {
            mediaKeysCapabilitiesIpc =
                std::make_shared<client::MediaKeysCapabilitiesIpc>(IIpcClientAccessor::instance().getIpcClient());
        }
        catch (const std::exception &e)
        {
            RIALTO_CLIENT_LOG_ERROR("Failed to create the media keys capabilities ipc, reason: %s", e.what());
        }

        MediaKeysCapabilitiesIpcFactory::m_mediaKeysCapabilitiesIpc = mediaKeysCapabilitiesIpc;
    }

    return mediaKeysCapabilitiesIpc;
}

MediaKeysCapabilitiesIpc::MediaKeysCapabilitiesIpc(IIpcClient &ipcClient) : IpcModule(ipcClient)
{
    RIALTO_CLIENT_LOG_DEBUG("entry:");

    if (!attachChannel())
    {
        throw std::runtime_error("Failed attach to the ipc channel");
    }
}

MediaKeysCapabilitiesIpc::~MediaKeysCapabilitiesIpc()
{
    RIALTO_CLIENT_LOG_DEBUG("entry:");

    detachChannel();
}

bool MediaKeysCapabilitiesIpc::createRpcStubs(const std::shared_ptr<ipc::IChannel> &ipcChannel)
{
    m_mediaKeysCapabilitiesStub =
        std::make_unique<::firebolt::rialto::MediaKeysCapabilitiesModule_Stub>(ipcChannel.get());
    if (!m_mediaKeysCapabilitiesStub)
    {
        return false;
    }
    return true;
}

std::vector<std::string> MediaKeysCapabilitiesIpc::getSupportedKeySystems()
{
    if (!reattachChannelIfRequired())
    {
        RIALTO_CLIENT_LOG_ERROR("Reattachment of the ipc channel failed, ipc disconnected");
        return {};
    }

    firebolt::rialto::GetSupportedKeySystemsRequest request;
    firebolt::rialto::GetSupportedKeySystemsResponse response;
    auto ipcController = m_ipc.createRpcController();
    auto blockingClosure = m_ipc.createBlockingClosure();
    m_mediaKeysCapabilitiesStub->getSupportedKeySystems(ipcController.get(), &request, &response, blockingClosure.get());

    // wait for the call to complete
    blockingClosure->wait();

    // check the result
    if (ipcController->Failed())
    {
        RIALTO_CLIENT_LOG_ERROR("failed to get supported key systems due to '%s'", ipcController->ErrorText().c_str());
        return {};
    }

    return std::vector<std::string>{response.key_systems().begin(), response.key_systems().end()};
}

bool MediaKeysCapabilitiesIpc::supportsKeySystem(const std::string &keySystem)
{
    if (!reattachChannelIfRequired())
    {
        RIALTO_CLIENT_LOG_ERROR("Reattachment of the ipc channel failed, ipc disconnected");
        return false;
    }

    firebolt::rialto::SupportsKeySystemRequest request;
    request.set_key_system(keySystem);

    firebolt::rialto::SupportsKeySystemResponse response;
    auto ipcController = m_ipc.createRpcController();
    auto blockingClosure = m_ipc.createBlockingClosure();
    m_mediaKeysCapabilitiesStub->supportsKeySystem(ipcController.get(), &request, &response, blockingClosure.get());

    // wait for the call to complete
    blockingClosure->wait();

    // check the result
    if (ipcController->Failed())
    {
        RIALTO_CLIENT_LOG_ERROR("failed to supports key system due to '%s'", ipcController->ErrorText().c_str());
        return false;
    }

    return response.is_supported();
}

bool MediaKeysCapabilitiesIpc::getSupportedKeySystemVersion(const std::string &keySystem, std::string &version)
{
    if (!reattachChannelIfRequired())
    {
        RIALTO_CLIENT_LOG_ERROR("Reattachment of the ipc channel failed, ipc disconnected");
        return false;
    }

    firebolt::rialto::GetSupportedKeySystemVersionRequest request;
    request.set_key_system(keySystem);

    firebolt::rialto::GetSupportedKeySystemVersionResponse response;
    auto ipcController = m_ipc.createRpcController();
    auto blockingClosure = m_ipc.createBlockingClosure();
    m_mediaKeysCapabilitiesStub->getSupportedKeySystemVersion(ipcController.get(), &request, &response,
                                                              blockingClosure.get());

    // wait for the call to complete
    blockingClosure->wait();

    // check the result
    if (ipcController->Failed())
    {
        RIALTO_CLIENT_LOG_ERROR("failed to get supported key system version due to '%s'",
                                ipcController->ErrorText().c_str());
        return false;
    }
    version = response.version();

    return true;
}

bool MediaKeysCapabilitiesIpc::isServerCertificateSupported(const std::string &keySystem)
{
    if (!reattachChannelIfRequired())
    {
        RIALTO_CLIENT_LOG_ERROR("Reattachment of the ipc channel failed, ipc disconnected");
        return false;
    }

    firebolt::rialto::IsServerCertificateSupportedRequest request;
    request.set_key_system(keySystem);

    firebolt::rialto::IsServerCertificateSupportedResponse response;
    auto ipcController = m_ipc.createRpcController();
    auto blockingClosure = m_ipc.createBlockingClosure();
    m_mediaKeysCapabilitiesStub->isServerCertificateSupported(ipcController.get(), &request, &response,
                                                              blockingClosure.get());

    // wait for the call to complete
    blockingClosure->wait();

    // check the result
    if (ipcController->Failed())
    {
        RIALTO_CLIENT_LOG_ERROR("failed to check if server certificate is supported due to '%s'",
                                ipcController->ErrorText().c_str());
        return false;
    }

    return response.is_supported();
}

}; // namespace firebolt::rialto::client
