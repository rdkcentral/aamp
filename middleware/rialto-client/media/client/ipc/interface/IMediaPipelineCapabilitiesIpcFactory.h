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

#ifndef FIREBOLT_RIALTO_CLIENT_I_MEDIA_PIPELINE_CAPABILITIES_IPC_FACTORY_H_
#define FIREBOLT_RIALTO_CLIENT_I_MEDIA_PIPELINE_CAPABILITIES_IPC_FACTORY_H_

/**
 * @file IMediaPipelineCapabilities.h
 *
 * The definition of the IMediaPipelineCapabilities interface.
 */

#include "IMediaPipelineCapabilities.h"
#include <memory>
#include <string>
#include <vector>

namespace firebolt::rialto::client
{
/**
 * @brief IMediaPipelineCapabilities factory class, for getting the IMediaPipelineCapabilities singleton object for ipc.
 */
class IMediaPipelineCapabilitiesIpcFactory
{
public:
    IMediaPipelineCapabilitiesIpcFactory() = default;
    virtual ~IMediaPipelineCapabilitiesIpcFactory() = default;

    /**
     * @brief Gets the IMediaPipelineCapabilitiesIpcFactory instance.
     *
     * @retval the factory instance or null on error.
     */
    static std::shared_ptr<IMediaPipelineCapabilitiesIpcFactory> createFactory();

    /**
     * @brief Gets the IMediaPipelineCapabilities singleton object for ipc.
     *
     * @retval the MediaPipelineCapabilities for ipc instance or null on error.
     */
    virtual std::unique_ptr<IMediaPipelineCapabilities> createMediaPipelineCapabilitiesIpc() const = 0;
};

}; // namespace firebolt::rialto::client

#endif // FIREBOLT_RIALTO_CLIENT_I_MEDIA_PIPELINE_CAPABILITIES_IPC_FACTORY_H_
