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

#include "WebAudioPlayerModuleService.h"
#include "IWebAudioPlayerService.h"
#include "RialtoServerLogging.h"
#include "WebAudioPlayerClient.h"
#include <IIpcController.h>
#include <algorithm>
#include <cstdint>

namespace
{
int generateHandle()
{
    static int webAudioPlayerHandle{0};
    return webAudioPlayerHandle++;
}
} // namespace

namespace firebolt::rialto::server::ipc
{
std::shared_ptr<IWebAudioPlayerModuleServiceFactory> IWebAudioPlayerModuleServiceFactory::createFactory()
{
    std::shared_ptr<IWebAudioPlayerModuleServiceFactory> factory;

    try
    {
        factory = std::make_shared<WebAudioPlayerModuleServiceFactory>();
    }
    catch (const std::exception &e)
    {
        RIALTO_SERVER_LOG_ERROR("Failed to create the web audio player module service factory, reason: %s", e.what());
    }

    return factory;
}

std::shared_ptr<IWebAudioPlayerModuleService>
WebAudioPlayerModuleServiceFactory::create(service::IWebAudioPlayerService &webAudioPlayerService) const
{
    std::shared_ptr<IWebAudioPlayerModuleService> webAudioPlayerModule;

    try
    {
        webAudioPlayerModule = std::make_shared<WebAudioPlayerModuleService>(webAudioPlayerService);
    }
    catch (const std::exception &e)
    {
        RIALTO_SERVER_LOG_ERROR("Failed to create the web audio player module service, reason: %s", e.what());
    }

    return webAudioPlayerModule;
}

WebAudioPlayerModuleService::WebAudioPlayerModuleService(service::IWebAudioPlayerService &webAudioPlayerService)
    : m_webAudioPlayerService{webAudioPlayerService}
{
}

WebAudioPlayerModuleService::~WebAudioPlayerModuleService() {}

void WebAudioPlayerModuleService::clientConnected(const std::shared_ptr<::firebolt::rialto::ipc::IClient> &ipcClient)
{
    RIALTO_SERVER_LOG_INFO("Client Connected!");
    {
        m_clientWebAudioPlayerHandles.emplace(ipcClient, std::set<int>());
    }
    ipcClient->exportService(shared_from_this());
}

void WebAudioPlayerModuleService::clientDisconnected(const std::shared_ptr<::firebolt::rialto::ipc::IClient> &ipcClient)
{
    RIALTO_SERVER_LOG_INFO("Client disconnected!");
    std::set<int> webAudioPlayerHandles;
    {
        auto handleIter = m_clientWebAudioPlayerHandles.find(ipcClient);
        if (handleIter == m_clientWebAudioPlayerHandles.end())
        {
            RIALTO_SERVER_LOG_ERROR("unknown client disconnected");
            return;
        }
        webAudioPlayerHandles = handleIter->second; // copy to avoid deadlock
        m_clientWebAudioPlayerHandles.erase(handleIter);
    }
    for (const auto &webAudioPlayerHandle : webAudioPlayerHandles)
    {
        m_webAudioPlayerService.destroyWebAudioPlayer(webAudioPlayerHandle);
    }
}

void WebAudioPlayerModuleService::createWebAudioPlayer(::google::protobuf::RpcController *controller,
                                                       const ::firebolt::rialto::CreateWebAudioPlayerRequest *request,
                                                       ::firebolt::rialto::CreateWebAudioPlayerResponse *response,
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

    std::shared_ptr<WebAudioConfig> config = std::make_shared<WebAudioConfig>();
    if (request->has_config())
    {
        if (request->config().has_pcm())
        {
            config->pcm.rate = request->config().pcm().rate();
            config->pcm.channels = request->config().pcm().channels();
            config->pcm.sampleSize = request->config().pcm().sample_size();
            config->pcm.isBigEndian = request->config().pcm().is_big_endian();
            config->pcm.isSigned = request->config().pcm().is_signed();
            config->pcm.isFloat = request->config().pcm().is_float();
        }
    }
    int handle = generateHandle();
    bool webAudioPlayerCreated =
        m_webAudioPlayerService.createWebAudioPlayer(handle,
                                                     std::make_shared<WebAudioPlayerClient>(handle,
                                                                                            ipcController->getClient()),
                                                     request->audio_mime_type(), request->priority(), config);
    if (webAudioPlayerCreated)
    {
        // Assume that IPC library works well and client is present
        m_clientWebAudioPlayerHandles[ipcController->getClient()].insert(handle);
        response->set_web_audio_player_handle(handle);
    }
    else
    {
        RIALTO_SERVER_LOG_ERROR("Create web audio player failed");
        controller->SetFailed("Operation failed");
    }
    done->Run();
}

void WebAudioPlayerModuleService::destroyWebAudioPlayer(::google::protobuf::RpcController *controller,
                                                        const ::firebolt::rialto::DestroyWebAudioPlayerRequest *request,
                                                        ::firebolt::rialto::DestroyWebAudioPlayerResponse *response,
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
    if (!m_webAudioPlayerService.destroyWebAudioPlayer(request->web_audio_player_handle()))
    {
        RIALTO_SERVER_LOG_ERROR("Destroy web audio player failed");
        controller->SetFailed("Operation failed");
        done->Run();
        return;
    }
    auto handleIter = m_clientWebAudioPlayerHandles.find(ipcController->getClient());
    if (handleIter != m_clientWebAudioPlayerHandles.end())
    {
        handleIter->second.erase(request->web_audio_player_handle());
    }
    done->Run();
}

void WebAudioPlayerModuleService::play(::google::protobuf::RpcController *controller,
                                       const ::firebolt::rialto::WebAudioPlayRequest *request,
                                       ::firebolt::rialto::WebAudioPlayResponse *response,
                                       ::google::protobuf::Closure *done)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");
    if (!m_webAudioPlayerService.play(request->web_audio_player_handle()))
    {
        RIALTO_SERVER_LOG_ERROR("play failed");
        controller->SetFailed("Operation failed");
    }
    done->Run();
}

void WebAudioPlayerModuleService::pause(::google::protobuf::RpcController *controller,
                                        const ::firebolt::rialto::WebAudioPauseRequest *request,
                                        ::firebolt::rialto::WebAudioPauseResponse *response,
                                        ::google::protobuf::Closure *done)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");
    if (!m_webAudioPlayerService.pause(request->web_audio_player_handle()))
    {
        RIALTO_SERVER_LOG_ERROR("pause failed");
        controller->SetFailed("Operation failed");
    }
    done->Run();
}

void WebAudioPlayerModuleService::setEos(::google::protobuf::RpcController *controller,
                                         const ::firebolt::rialto::WebAudioSetEosRequest *request,
                                         ::firebolt::rialto::WebAudioSetEosResponse *response,
                                         ::google::protobuf::Closure *done)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");
    if (!m_webAudioPlayerService.setEos(request->web_audio_player_handle()))
    {
        RIALTO_SERVER_LOG_ERROR("setEos failed");
        controller->SetFailed("Operation failed");
    }
    done->Run();
}

void WebAudioPlayerModuleService::getBufferAvailable(::google::protobuf::RpcController *controller,
                                                     const ::firebolt::rialto::WebAudioGetBufferAvailableRequest *request,
                                                     ::firebolt::rialto::WebAudioGetBufferAvailableResponse *response,
                                                     ::google::protobuf::Closure *done)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");

    uint32_t availableFrames{};
    std::shared_ptr<WebAudioShmInfo> shmInfo = std::make_shared<WebAudioShmInfo>();
    if (!m_webAudioPlayerService.getBufferAvailable(request->web_audio_player_handle(), availableFrames, shmInfo))
    {
        RIALTO_SERVER_LOG_ERROR("getBufferAvailable failed");
        controller->SetFailed("Operation failed");
    }
    else
    {
        response->set_available_frames(availableFrames);
        auto responseShmInfo = response->mutable_shm_info();
        responseShmInfo->set_offset_main(shmInfo->offsetMain);
        responseShmInfo->set_length_main(shmInfo->lengthMain);
        responseShmInfo->set_offset_wrap(shmInfo->offsetWrap);
        responseShmInfo->set_length_wrap(shmInfo->lengthWrap);
    }
    done->Run();
}

void WebAudioPlayerModuleService::getBufferDelay(::google::protobuf::RpcController *controller,
                                                 const ::firebolt::rialto::WebAudioGetBufferDelayRequest *request,
                                                 ::firebolt::rialto::WebAudioGetBufferDelayResponse *response,
                                                 ::google::protobuf::Closure *done)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");

    uint32_t delayFrames{};
    if (!m_webAudioPlayerService.getBufferDelay(request->web_audio_player_handle(), delayFrames))
    {
        RIALTO_SERVER_LOG_ERROR("getBufferDelay failed");
        controller->SetFailed("Operation failed");
    }
    else
    {
        response->set_delay_frames(delayFrames);
    }
    done->Run();
}

void WebAudioPlayerModuleService::writeBuffer(::google::protobuf::RpcController *controller,
                                              const ::firebolt::rialto::WebAudioWriteBufferRequest *request,
                                              ::firebolt::rialto::WebAudioWriteBufferResponse *response,
                                              ::google::protobuf::Closure *done)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");
    if (!m_webAudioPlayerService.writeBuffer(request->web_audio_player_handle(), request->number_of_frames(), nullptr))
    {
        RIALTO_SERVER_LOG_ERROR("writeBuffer failed");
        controller->SetFailed("Operation failed");
    }
    done->Run();
}

void WebAudioPlayerModuleService::getDeviceInfo(::google::protobuf::RpcController *controller,
                                                const ::firebolt::rialto::WebAudioGetDeviceInfoRequest *request,
                                                ::firebolt::rialto::WebAudioGetDeviceInfoResponse *response,
                                                ::google::protobuf::Closure *done)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");

    uint32_t preferredFrames{};
    uint32_t maximumFrames{};
    bool supportDeferredPlay{};
    if (!m_webAudioPlayerService.getDeviceInfo(request->web_audio_player_handle(), preferredFrames, maximumFrames,
                                               supportDeferredPlay))
    {
        RIALTO_SERVER_LOG_ERROR("getDeviceInfo failed");
        controller->SetFailed("Operation failed");
    }
    else
    {
        response->set_preferred_frames(preferredFrames);
        response->set_maximum_frames(maximumFrames);
        response->set_support_deferred_play(supportDeferredPlay);
    }
    done->Run();
}

void WebAudioPlayerModuleService::setVolume(::google::protobuf::RpcController *controller,
                                            const ::firebolt::rialto::WebAudioSetVolumeRequest *request,
                                            ::firebolt::rialto::WebAudioSetVolumeResponse *response,
                                            ::google::protobuf::Closure *done)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");
    if (!m_webAudioPlayerService.setVolume(request->web_audio_player_handle(), request->volume()))
    {
        RIALTO_SERVER_LOG_ERROR("setVolume failed");
        controller->SetFailed("Operation failed");
    }
    done->Run();
}

void WebAudioPlayerModuleService::getVolume(::google::protobuf::RpcController *controller,
                                            const ::firebolt::rialto::WebAudioGetVolumeRequest *request,
                                            ::firebolt::rialto::WebAudioGetVolumeResponse *response,
                                            ::google::protobuf::Closure *done)
{
    RIALTO_SERVER_LOG_DEBUG("entry:");

    double volume{};
    if (!m_webAudioPlayerService.getVolume(request->web_audio_player_handle(), volume))
    {
        RIALTO_SERVER_LOG_ERROR("getVolume failed");
        controller->SetFailed("Operation failed");
    }
    else
    {
        response->set_volume(volume);
    }
    done->Run();
}

} // namespace firebolt::rialto::server::ipc
