/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2023 
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
#include "WebAudioPlayerIpc.h"
#include "RialtoClientLogging.h"
#include "RialtoCommonIpc.h"
#include "webaudioplayermodule.pb.h"
#include <IWebAudioPlayer.h>

namespace firebolt::rialto::client
{

std::shared_ptr<IWebAudioPlayerIpcFactory> IWebAudioPlayerIpcFactory::getFactory()
{
    std::shared_ptr<IWebAudioPlayerIpcFactory> factory;

    try
    {
        factory = std::make_shared<WebAudioPlayerIpcFactory>();
    }
    catch (const std::exception &e)
    {
        RIALTO_CLIENT_LOG_ERROR("Failed to create the web audio player ipc factory, reason: %s", e.what());
    }

    return factory;
}

std::unique_ptr<IWebAudioPlayerIpc>
WebAudioPlayerIpcFactory::createWebAudioPlayerIpc(IWebAudioPlayerIpcClient *client, const std::string &audioMimeType,
                                                  const uint32_t priority, std::weak_ptr<const WebAudioConfig> config,
                                                  std::weak_ptr<IIpcClient> ipcClientParam)
{
    std::unique_ptr<IWebAudioPlayerIpc> webAudioPlayerIpc;
    try
    {
        std::shared_ptr<IIpcClient> ipcClient = ipcClientParam.lock();
        webAudioPlayerIpc =
            std::make_unique<WebAudioPlayerIpc>(client, audioMimeType, priority, config,
                                                ipcClient ? *ipcClient : IIpcClientAccessor::instance().getIpcClient(),
                                                firebolt::rialto::common::IEventThreadFactory::createFactory());
    }
    catch (const std::exception &e)
    {
        RIALTO_CLIENT_LOG_ERROR("Failed to create the web audio player ipc, reason: %s", e.what());
    }

    return webAudioPlayerIpc;
}

WebAudioPlayerIpc::WebAudioPlayerIpc(IWebAudioPlayerIpcClient *client, const std::string &audioMimeType,
                                     const uint32_t priority, std::weak_ptr<const WebAudioConfig> config,
                                     IIpcClient &ipcClient,
                                     const std::shared_ptr<common::IEventThreadFactory> &eventThreadFactory)
    : IpcModule(ipcClient), m_webAudioPlayerIpcClient(client),
      m_eventThread(eventThreadFactory->createEventThread("rialto-web-audio-player-events")), m_webAudioPlayerHandle(-1)
{
    if (!attachChannel())
    {
        throw std::runtime_error("Failed attach to the ipc channel");
    }

    if (!createWebAudioPlayer(audioMimeType, priority, config))
    {
        detachChannel();
        throw std::runtime_error("Could not create the web audio player");
    }
}

WebAudioPlayerIpc::~WebAudioPlayerIpc()
{
    // destroy web audio player session
    destroyWebAudioPlayer();

    // detach the Ipc channel
    detachChannel();

    // destroy the thread processing async notifications
    m_eventThread.reset();
}

bool WebAudioPlayerIpc::createRpcStubs(const std::shared_ptr<ipc::IChannel> &ipcChannel)
{
    m_webAudioPlayerStub = std::make_unique<::firebolt::rialto::WebAudioPlayerModule_Stub>(ipcChannel.get());
    if (!m_webAudioPlayerStub)
    {
        return false;
    }
    return true;
}

bool WebAudioPlayerIpc::subscribeToEvents(const std::shared_ptr<ipc::IChannel> &ipcChannel)
{
    if (!ipcChannel)
    {
        return false;
    }

    int eventTag = ipcChannel->subscribe<firebolt::rialto::WebAudioPlayerStateEvent>(
        [this](const std::shared_ptr<firebolt::rialto::WebAudioPlayerStateEvent> &event)
        { m_eventThread->add(&WebAudioPlayerIpc::onPlaybackStateUpdated, this, event); });
    if (eventTag < 0)
        return false;
    m_eventTags.push_back(eventTag);

    return true;
}

void WebAudioPlayerIpc::onPlaybackStateUpdated(const std::shared_ptr<firebolt::rialto::WebAudioPlayerStateEvent> &event)
{
    if (event->web_audio_player_handle() == m_webAudioPlayerHandle)
    {
        firebolt::rialto::WebAudioPlayerState playerState = firebolt::rialto::WebAudioPlayerState::UNKNOWN;
        switch (event->state())
        {
        case firebolt::rialto::WebAudioPlayerStateEvent_WebAudioPlayerState_UNKNOWN:
            playerState = firebolt::rialto::WebAudioPlayerState::UNKNOWN;
            break;
        case firebolt::rialto::WebAudioPlayerStateEvent_WebAudioPlayerState_IDLE:
            playerState = firebolt::rialto::WebAudioPlayerState::IDLE;
            break;
        case firebolt::rialto::WebAudioPlayerStateEvent_WebAudioPlayerState_PLAYING:
            playerState = firebolt::rialto::WebAudioPlayerState::PLAYING;
            break;
        case firebolt::rialto::WebAudioPlayerStateEvent_WebAudioPlayerState_PAUSED:
            playerState = firebolt::rialto::WebAudioPlayerState::PAUSED;
            break;
        case firebolt::rialto::WebAudioPlayerStateEvent_WebAudioPlayerState_END_OF_STREAM:
            playerState = firebolt::rialto::WebAudioPlayerState::END_OF_STREAM;
            break;
        case firebolt::rialto::WebAudioPlayerStateEvent_WebAudioPlayerState_FAILURE:
            playerState = firebolt::rialto::WebAudioPlayerState::FAILURE;
            break;
        default:
            RIALTO_CLIENT_LOG_WARN("Recieved unknown web audio player state");
            break;
        }

        m_webAudioPlayerIpcClient->notifyState(playerState);
    }
}

bool WebAudioPlayerIpc::play()
{
    if (!reattachChannelIfRequired())
    {
        RIALTO_CLIENT_LOG_ERROR("Reattachment of the ipc channel failed, ipc disconnected");
        return false;
    }

    firebolt::rialto::WebAudioPlayRequest request;
    request.set_web_audio_player_handle(m_webAudioPlayerHandle);

    firebolt::rialto::WebAudioPlayResponse response;
    auto ipcController = m_ipc.createRpcController();
    auto blockingClosure = m_ipc.createBlockingClosure();
    m_webAudioPlayerStub->play(ipcController.get(), &request, &response, blockingClosure.get());

    // wait for the call to complete
    blockingClosure->wait();

    // check the result
    if (ipcController->Failed())
    {
        RIALTO_CLIENT_LOG_ERROR("Failed to play due to '%s'", ipcController->ErrorText().c_str());
        return false;
    }

    return true;
}

bool WebAudioPlayerIpc::pause()
{
    if (!reattachChannelIfRequired())
    {
        RIALTO_CLIENT_LOG_ERROR("Reattachment of the ipc channel failed, ipc disconnected");
        return false;
    }

    firebolt::rialto::WebAudioPauseRequest request;
    request.set_web_audio_player_handle(m_webAudioPlayerHandle);

    firebolt::rialto::WebAudioPauseResponse response;
    auto ipcController = m_ipc.createRpcController();
    auto blockingClosure = m_ipc.createBlockingClosure();
    m_webAudioPlayerStub->pause(ipcController.get(), &request, &response, blockingClosure.get());

    // wait for the call to complete
    blockingClosure->wait();

    // check the result
    if (ipcController->Failed())
    {
        RIALTO_CLIENT_LOG_ERROR("Failed to pause due to '%s'", ipcController->ErrorText().c_str());
        return false;
    }

    return true;
}

bool WebAudioPlayerIpc::setEos()
{
    if (!reattachChannelIfRequired())
    {
        RIALTO_CLIENT_LOG_ERROR("Reattachment of the ipc channel failed, ipc disconnected");
        return false;
    }

    firebolt::rialto::WebAudioSetEosRequest request;
    request.set_web_audio_player_handle(m_webAudioPlayerHandle);

    firebolt::rialto::WebAudioSetEosResponse response;
    auto ipcController = m_ipc.createRpcController();
    auto blockingClosure = m_ipc.createBlockingClosure();
    m_webAudioPlayerStub->setEos(ipcController.get(), &request, &response, blockingClosure.get());

    // wait for the call to complete
    blockingClosure->wait();

    // check the result
    if (ipcController->Failed())
    {
        RIALTO_CLIENT_LOG_ERROR("Failed to set eos due to '%s'", ipcController->ErrorText().c_str());
        return false;
    }

    return true;
}

bool WebAudioPlayerIpc::getBufferAvailable(uint32_t &availableFrames,
                                           const std::shared_ptr<WebAudioShmInfo> &webAudioShmInfo)
{
    if (!webAudioShmInfo)
    {
        RIALTO_CLIENT_LOG_ERROR("webAudioShmInfo parameter can't be null!");
        return false;
    }

    if (!reattachChannelIfRequired())
    {
        RIALTO_CLIENT_LOG_ERROR("Reattachment of the ipc channel failed, ipc disconnected");
        return false;
    }

    firebolt::rialto::WebAudioGetBufferAvailableRequest request;
    request.set_web_audio_player_handle(m_webAudioPlayerHandle);

    firebolt::rialto::WebAudioGetBufferAvailableResponse response;
    auto ipcController = m_ipc.createRpcController();
    auto blockingClosure = m_ipc.createBlockingClosure();
    m_webAudioPlayerStub->getBufferAvailable(ipcController.get(), &request, &response, blockingClosure.get());

    // wait for the call to complete
    blockingClosure->wait();

    // check the result
    if (ipcController->Failed())
    {
        RIALTO_CLIENT_LOG_ERROR("Failed to get buffer available due to '%s'", ipcController->ErrorText().c_str());
        return false;
    }

    availableFrames = response.available_frames();
    webAudioShmInfo->offsetMain = response.shm_info().offset_main();
    webAudioShmInfo->lengthMain = response.shm_info().length_main();
    webAudioShmInfo->offsetWrap = response.shm_info().offset_wrap();
    webAudioShmInfo->lengthWrap = response.shm_info().length_wrap();

    return true;
}

bool WebAudioPlayerIpc::getBufferDelay(uint32_t &delayFrames)
{
    if (!reattachChannelIfRequired())
    {
        RIALTO_CLIENT_LOG_ERROR("Reattachment of the ipc channel failed, ipc disconnected");
        return false;
    }

    firebolt::rialto::WebAudioGetBufferDelayRequest request;
    request.set_web_audio_player_handle(m_webAudioPlayerHandle);

    firebolt::rialto::WebAudioGetBufferDelayResponse response;
    auto ipcController = m_ipc.createRpcController();
    auto blockingClosure = m_ipc.createBlockingClosure();
    m_webAudioPlayerStub->getBufferDelay(ipcController.get(), &request, &response, blockingClosure.get());

    // wait for the call to complete
    blockingClosure->wait();

    // check the result
    if (ipcController->Failed())
    {
        RIALTO_CLIENT_LOG_ERROR("Failed to get buffer delay source due to '%s'", ipcController->ErrorText().c_str());
        return false;
    }

    delayFrames = response.delay_frames();

    return true;
}

bool WebAudioPlayerIpc::writeBuffer(const uint32_t numberOfFrames)
{
    if (!reattachChannelIfRequired())
    {
        RIALTO_CLIENT_LOG_ERROR("Reattachment of the ipc channel failed, ipc disconnected");
        return false;
    }

    firebolt::rialto::WebAudioWriteBufferRequest request;

    request.set_number_of_frames(numberOfFrames);
    request.set_web_audio_player_handle(m_webAudioPlayerHandle);

    firebolt::rialto::WebAudioWriteBufferResponse response;
    auto ipcController = m_ipc.createRpcController();
    auto blockingClosure = m_ipc.createBlockingClosure();
    m_webAudioPlayerStub->writeBuffer(ipcController.get(), &request, &response, blockingClosure.get());

    // wait for the call to complete
    blockingClosure->wait();

    // check the result
    if (ipcController->Failed())
    {
        RIALTO_CLIENT_LOG_ERROR("Failed to write to the buffer due to '%s'", ipcController->ErrorText().c_str());
        return false;
    }

    return true;
}

bool WebAudioPlayerIpc::getDeviceInfo(uint32_t &preferredFrames, uint32_t &maximumFrames, bool &supportDeferredPlay)
{
    if (!reattachChannelIfRequired())
    {
        RIALTO_CLIENT_LOG_ERROR("Reattachment of the ipc channel failed, ipc disconnected");
        return false;
    }

    firebolt::rialto::WebAudioGetDeviceInfoRequest request;
    request.set_web_audio_player_handle(m_webAudioPlayerHandle);

    firebolt::rialto::WebAudioGetDeviceInfoResponse response;
    auto ipcController = m_ipc.createRpcController();
    auto blockingClosure = m_ipc.createBlockingClosure();
    m_webAudioPlayerStub->getDeviceInfo(ipcController.get(), &request, &response, blockingClosure.get());

    // wait for the call to complete
    blockingClosure->wait();

    // check the result
    if (ipcController->Failed())
    {
        RIALTO_CLIENT_LOG_ERROR("Failed to get device info source due to '%s'", ipcController->ErrorText().c_str());
        return false;
    }

    preferredFrames = response.preferred_frames();
    maximumFrames = response.maximum_frames();
    supportDeferredPlay = response.support_deferred_play();

    return true;
}

bool WebAudioPlayerIpc::setVolume(double volume)
{
    if (!reattachChannelIfRequired())
    {
        RIALTO_CLIENT_LOG_ERROR("Reattachment of the ipc channel failed, ipc disconnected");
        return false;
    }

    firebolt::rialto::WebAudioSetVolumeRequest request;
    request.set_volume(volume);
    request.set_web_audio_player_handle(m_webAudioPlayerHandle);

    firebolt::rialto::WebAudioSetVolumeResponse response;
    auto ipcController = m_ipc.createRpcController();
    auto blockingClosure = m_ipc.createBlockingClosure();
    m_webAudioPlayerStub->setVolume(ipcController.get(), &request, &response, blockingClosure.get());

    // wait for the call to complete
    blockingClosure->wait();

    // check the result
    if (ipcController->Failed())
    {
        RIALTO_CLIENT_LOG_ERROR("Failed to set volume due to '%s'", ipcController->ErrorText().c_str());
        return false;
    }

    return true;
}

bool WebAudioPlayerIpc::getVolume(double &volume)
{
    if (!reattachChannelIfRequired())
    {
        RIALTO_CLIENT_LOG_ERROR("Reattachment of the ipc channel failed, ipc disconnected");
        return false;
    }

    firebolt::rialto::WebAudioGetVolumeRequest request;
    request.set_web_audio_player_handle(m_webAudioPlayerHandle);

    firebolt::rialto::WebAudioGetVolumeResponse response;
    auto ipcController = m_ipc.createRpcController();
    auto blockingClosure = m_ipc.createBlockingClosure();
    m_webAudioPlayerStub->getVolume(ipcController.get(), &request, &response, blockingClosure.get());

    // wait for the call to complete
    blockingClosure->wait();

    // check the result
    if (ipcController->Failed())
    {
        RIALTO_CLIENT_LOG_ERROR("Failed to get volume due to '%s'", ipcController->ErrorText().c_str());
        return false;
    }

    volume = response.volume();
    return true;
}

bool WebAudioPlayerIpc::createWebAudioPlayer(const std::string &audioMimeType, const uint32_t priority,
                                             std::weak_ptr<const WebAudioConfig> webAudioConfig)
{
    if (!reattachChannelIfRequired())
    {
        RIALTO_CLIENT_LOG_ERROR("Reattachment of the ipc channel failed, ipc disconnected");
        return false;
    }

    firebolt::rialto::CreateWebAudioPlayerRequest request;
    request.set_audio_mime_type(audioMimeType);
    request.set_priority(priority);
    std::shared_ptr<const WebAudioConfig> kConfig = webAudioConfig.lock();
    if (kConfig)
    {
        ::firebolt::rialto::CreateWebAudioPlayerRequest_WebAudioPcmConfig pcm_config;
        pcm_config.set_rate(kConfig->pcm.rate);
        pcm_config.set_channels(kConfig->pcm.channels);
        pcm_config.set_sample_size(kConfig->pcm.sampleSize);
        pcm_config.set_is_big_endian(kConfig->pcm.isBigEndian);
        pcm_config.set_is_signed(kConfig->pcm.isSigned);
        pcm_config.set_is_float(kConfig->pcm.isFloat);
        request.mutable_config()->mutable_pcm()->CopyFrom(pcm_config);
    }

    firebolt::rialto::CreateWebAudioPlayerResponse response;
    auto ipcController = m_ipc.createRpcController();
    auto blockingClosure = m_ipc.createBlockingClosure();
    m_webAudioPlayerStub->createWebAudioPlayer(ipcController.get(), &request, &response, blockingClosure.get());

    // wait for the call to complete
    blockingClosure->wait();

    // check the result
    if (ipcController->Failed())
    {
        RIALTO_CLIENT_LOG_ERROR("Failed to create web audio player due to '%s'", ipcController->ErrorText().c_str());
        return false;
    }

    m_webAudioPlayerHandle = response.web_audio_player_handle();

    return true;
}

void WebAudioPlayerIpc::destroyWebAudioPlayer()
{
    if (!reattachChannelIfRequired())
    {
        RIALTO_CLIENT_LOG_ERROR("Reattachment of the ipc channel failed, ipc disconnected");
        return;
    }

    firebolt::rialto::DestroyWebAudioPlayerRequest request;
    request.set_web_audio_player_handle(m_webAudioPlayerHandle);

    firebolt::rialto::DestroyWebAudioPlayerResponse response;
    auto ipcController = m_ipc.createRpcController();
    auto blockingClosure = m_ipc.createBlockingClosure();
    m_webAudioPlayerStub->destroyWebAudioPlayer(ipcController.get(), &request, &response, blockingClosure.get());

    // wait for the call to complete
    blockingClosure->wait();

    // check the result
    if (ipcController->Failed())
    {
        RIALTO_CLIENT_LOG_ERROR("Failed to destroy web audio player due to '%s'", ipcController->ErrorText().c_str());
    }
}

}; // namespace firebolt::rialto::client
