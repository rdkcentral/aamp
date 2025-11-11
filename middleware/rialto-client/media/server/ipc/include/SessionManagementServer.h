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

#ifndef FIREBOLT_RIALTO_SERVER_IPC_SESSION_MANAGEMENT_SERVER_H_
#define FIREBOLT_RIALTO_SERVER_IPC_SESSION_MANAGEMENT_SERVER_H_

#include "IControlModuleService.h"
#include "IControlService.h"
#include "ILinuxWrapper.h"
#include "IMediaKeysCapabilitiesModuleService.h"
#include "IMediaKeysModuleService.h"
#include "IMediaPipelineCapabilitiesModuleService.h"
#include "IMediaPipelineModuleService.h"
#include "IPlaybackService.h"
#include "ISessionManagementServer.h"
#include "IWebAudioPlayerModuleService.h"
#include "SetLogLevelsService.h"
#include <IIpcServer.h>
#include <IIpcServerFactory.h>
#include <atomic>
#include <memory>
#include <string>
#include <thread>

namespace firebolt::rialto::server::ipc
{
class SessionManagementServer : public ISessionManagementServer
{
public:
    SessionManagementServer(
        const std::shared_ptr<firebolt::rialto::ipc::IServerFactory> &serverFactory,
        const std::shared_ptr<IMediaPipelineModuleServiceFactory> &mediaPipelineModuleFactory,
        const std::shared_ptr<IMediaPipelineCapabilitiesModuleServiceFactory> &mediaPipelineCapabilitiesModuleFactory,
        const std::shared_ptr<IMediaKeysModuleServiceFactory> &mediaKeysModuleFactory,
        const std::shared_ptr<IMediaKeysCapabilitiesModuleServiceFactory> &mediaKeysCapabilitiesModuleFactory,
        const std::shared_ptr<IWebAudioPlayerModuleServiceFactory> &webAudioPlayerModuleFactory,
        const std::shared_ptr<IControlModuleServiceFactory> &controlModuleFactory,
        service::IPlaybackService &playbackService, service::ICdmService &cdmService,
        service::IControlService &controlService);
    ~SessionManagementServer() override;
    SessionManagementServer(const SessionManagementServer &) = delete;
    SessionManagementServer(SessionManagementServer &&) = delete;
    SessionManagementServer &operator=(const SessionManagementServer &) = delete;
    SessionManagementServer &operator=(SessionManagementServer &&) = delete;

    bool initialize(const std::string &socketName, unsigned int socketPermissions, const std::string &socketOwner,
                    const std::string &socketGroup) override;
    bool initialize(int32_t socketFd) override;
    void start() override;
    void stop() override;
    void setLogLevels(RIALTO_DEBUG_LEVEL defaultLogLevels, RIALTO_DEBUG_LEVEL clientLogLevels,
                      RIALTO_DEBUG_LEVEL ipcLogLevels, RIALTO_DEBUG_LEVEL commonLogLevels) override;

private:
    void onClientConnected(const std::shared_ptr<::firebolt::rialto::ipc::IClient> &client);
    void onClientDisconnected(const std::shared_ptr<::firebolt::rialto::ipc::IClient> &client);
    size_t getBufferSizeForPasswordStructureCalls() const;

private:
    std::atomic<bool> m_isRunning;
    std::thread m_ipcServerThread;
    std::shared_ptr<::firebolt::rialto::ipc::IServer> m_ipcServer;
    std::shared_ptr<IMediaPipelineModuleService> m_mediaPipelineModule;
    std::shared_ptr<IMediaPipelineCapabilitiesModuleService> m_mediaPipelineCapabilitiesModule;
    std::shared_ptr<IMediaKeysModuleService> m_mediaKeysModule;
    std::shared_ptr<IMediaKeysCapabilitiesModuleService> m_mediaKeysCapabilitiesModule;
    std::shared_ptr<IWebAudioPlayerModuleService> m_webAudioPlayerModule;
    std::shared_ptr<IControlModuleService> m_controlModule;
    SetLogLevelsService m_setLogLevelsService;
};
} // namespace firebolt::rialto::server::ipc

#endif // FIREBOLT_RIALTO_SERVER_IPC_SESSION_MANAGEMENT_SERVER_H_
