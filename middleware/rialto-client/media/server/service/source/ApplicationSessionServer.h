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

#ifndef FIREBOLT_RIALTO_SERVER_APPLICATION_SESSION_SERVER_H_
#define FIREBOLT_RIALTO_SERVER_APPLICATION_SESSION_SERVER_H_

#include "CdmService.h"
#include "ControlService.h"
#include "IApplicationSessionServer.h"
#include "IControlServerInternal.h"
#include "IHeartbeatProcedure.h"
#include "IMediaKeysCapabilities.h"
#include "IMediaKeysServerInternal.h"
#include "IMediaPipelineCapabilities.h"
#include "IMediaPipelineServerInternal.h"
#include "ISharedMemoryBuffer.h"
#include "IWebAudioPlayerServerInternal.h"
#include "IpcFactory.h"
#include "PlaybackService.h"
#include "SessionServerManager.h"
#include <memory>

namespace firebolt::rialto::server
{
class ApplicationSessionServerFactory : public IApplicationSessionServerFactory
{
public:
    ApplicationSessionServerFactory() = default;
    ~ApplicationSessionServerFactory() override = default;

    std::unique_ptr<IApplicationSessionServer> createApplicationSessionServer() const override;
};

class ApplicationSessionServer : public IApplicationSessionServer
{
public:
    ApplicationSessionServer() = default;
    ~ApplicationSessionServer() override = default;

    bool init(int argc, char *argv[]) override;
    void startService() override;

private:
    firebolt::rialto::server::ipc::IpcFactory m_ipcFactory;
    firebolt::rialto::server::service::ControlService m_controlService{
        firebolt::rialto::server::IControlServerInternalFactory::createFactory()};
    firebolt::rialto::server::service::CdmService
        m_cdmService{firebolt::rialto::server::IMediaKeysServerInternalFactory::createFactory(),
                     firebolt::rialto::IMediaKeysCapabilitiesFactory::createFactory()};
    firebolt::rialto::server::service::PlaybackService
        m_playbackService{firebolt::rialto::server::IMediaPipelineServerInternalFactory::createFactory(),
                          firebolt::rialto::IMediaPipelineCapabilitiesFactory::createFactory(),
                          firebolt::rialto::server::IWebAudioPlayerServerInternalFactory::createFactory(),
                          firebolt::rialto::server::ISharedMemoryBufferFactory::createFactory(), m_cdmService};
    firebolt::rialto::server::service::SessionServerManager
        m_serviceManager{m_ipcFactory, m_playbackService, m_cdmService, m_controlService,
                         firebolt::rialto::server::IHeartbeatProcedureFactory::createFactory()};
};
} // namespace firebolt::rialto::server

#endif // FIREBOLT_RIALTO_SERVER_APPLICATION_SESSION_SERVER_H_
