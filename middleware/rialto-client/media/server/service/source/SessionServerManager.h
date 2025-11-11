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

#ifndef FIREBOLT_RIALTO_SERVER_SERVICE_SESSION_SERVER_MANAGER_H_
#define FIREBOLT_RIALTO_SERVER_SERVICE_SESSION_SERVER_MANAGER_H_

#include "IApplicationManagementServer.h"
#include "ICdmService.h"
#include "IControlService.h"
#include "IHeartbeatProcedure.h"
#include "IIpcFactory.h"
#include "IPlaybackService.h"
#include "ISessionManagementServer.h"
#include "ISessionServerManager.h"
#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>

namespace firebolt::rialto::server::service
{
class SessionServerManager : public ISessionServerManager
{
public:
    SessionServerManager(const ipc::IIpcFactory &ipcFactory, IPlaybackService &playbackService, ICdmService &cdmService,
                         IControlService &controlService,
                         std::unique_ptr<IHeartbeatProcedureFactory> &&heartbeatProcedureFactory);
    ~SessionServerManager() override;
    SessionServerManager(const SessionServerManager &) = delete;
    SessionServerManager(SessionServerManager &&) = delete;
    SessionServerManager &operator=(const SessionServerManager &) = delete;
    SessionServerManager &operator=(SessionServerManager &&) = delete;

    bool initialize(int argc, char *argv[]) override;
    void startService() override;
    bool configureIpc(const std::string &socketName, unsigned int socketPermissions, const std::string &socketOwner,
                      const std::string &socketGroup) override;
    bool configureIpc(int32_t socketFd) override;
    bool configureServices(const common::SessionServerState &state, const common::MaxResourceCapabilitites &maxResource,
                           const std::string &clientDisplayName, const std::string &appName) override;
    bool setState(const common::SessionServerState &state) override;
    void setLogLevels(RIALTO_DEBUG_LEVEL defaultLogLevels, RIALTO_DEBUG_LEVEL clientLogLevels,
                      RIALTO_DEBUG_LEVEL sessionServerLogLevels, RIALTO_DEBUG_LEVEL ipcLogLevels,
                      RIALTO_DEBUG_LEVEL serverManagerLogLevels, RIALTO_DEBUG_LEVEL commonLogLevels) override;
    bool ping(std::int32_t id, const std::shared_ptr<IAckSender> &ackSender) override;

private:
    bool switchToActive();
    bool switchToInactive();
    bool switchToNotRunning();
    void stopService();

private:
    IPlaybackService &m_playbackService;
    ICdmService &m_cdmService;
    IControlService &m_controlService;
    std::unique_ptr<IHeartbeatProcedureFactory> m_heartbeatProcedureFactory;
    std::unique_ptr<ipc::IApplicationManagementServer> m_applicationManagementServer;
    std::unique_ptr<ipc::ISessionManagementServer> m_sessionManagementServer;
    std::mutex m_serviceMutex;
    std::condition_variable m_serviceCv;
    bool m_isServiceRunning;
    std::atomic<common::SessionServerState> m_currentState;
};
} // namespace firebolt::rialto::server::service

#endif // FIREBOLT_RIALTO_SERVER_SERVICE_SESSION_SERVER_MANAGER_H_
