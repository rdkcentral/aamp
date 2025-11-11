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

#include "IpcFactory.h"
#include "ApplicationManagementServer.h"
#include "IControlModuleService.h"
#include "IIpcServer.h"
#include "IMediaKeysCapabilitiesModuleService.h"
#include "IMediaKeysModuleService.h"
#include "IMediaPipelineCapabilitiesModuleService.h"
#include "IMediaPipelineModuleService.h"
#include "IServerManagerModuleServiceFactory.h"
#include "IWebAudioPlayerModuleService.h"
#include "SessionManagementServer.h"

namespace firebolt::rialto::server::ipc
{
std::unique_ptr<IApplicationManagementServer>
IpcFactory::createApplicationManagementServer(service::ISessionServerManager &sessionServerManager) const
{
    return std::make_unique<
        ApplicationManagementServer>(firebolt::rialto::ipc::IServerFactory::createFactory(),
                                     firebolt::rialto::server::ipc::IServerManagerModuleServiceFactory::createFactory(),
                                     sessionServerManager);
}

std::unique_ptr<ISessionManagementServer>
IpcFactory::createSessionManagementServer(service::IPlaybackService &playbackService, service::ICdmService &cdmService,
                                          service::IControlService &controlService) const
{
    return std::make_unique<
        SessionManagementServer>(firebolt::rialto::ipc::IServerFactory::createFactory(),
                                 firebolt::rialto::server::ipc::IMediaPipelineModuleServiceFactory::createFactory(),
                                 firebolt::rialto::server::ipc::IMediaPipelineCapabilitiesModuleServiceFactory::createFactory(),
                                 firebolt::rialto::server::ipc::IMediaKeysModuleServiceFactory::createFactory(),
                                 firebolt::rialto::server::ipc::IMediaKeysCapabilitiesModuleServiceFactory::createFactory(),
                                 firebolt::rialto::server::ipc::IWebAudioPlayerModuleServiceFactory::createFactory(),
                                 firebolt::rialto::server::ipc::IControlModuleServiceFactory::createFactory(),
                                 playbackService, cdmService, controlService);
}
} // namespace firebolt::rialto::server::ipc
