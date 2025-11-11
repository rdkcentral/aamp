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

#ifndef FIREBOLT_RIALTO_CLIENT_I_MEDIA_KEYS_IPC_FACTORY_H_
#define FIREBOLT_RIALTO_CLIENT_I_MEDIA_KEYS_IPC_FACTORY_H_

/**
 * @file IMediaKeys.h
 *
 * The definition of the IMediaKeys interface.
 *
 * This interface defines the public API of Rialto for EME decryption of AV content.
 */

#include <memory>
#include <string>
#include <vector>

#include "IMediaKeys.h"
#include "MediaCommon.h"

namespace firebolt::rialto::client
{
/**
 * @brief IMediaKeys factory class, returns a concrete implementation of IMediaKeys for Ipc
 */
class IMediaKeysIpcFactory
{
public:
    IMediaKeysIpcFactory() = default;
    virtual ~IMediaKeysIpcFactory() = default;

    /**
     * @brief Create a IMediaKeysIpcFactory instance.
     *
     * @retval the factory instance or null on error.
     */
    static std::shared_ptr<IMediaKeysIpcFactory> createFactory();

    /**
     * @brief IMediaKeys factory method, returns a concrete implementation of IMediaKeys for Ipc
     *
     * @param[in] keySystem : The key system for which to create a Media Keys instance
     *
     * @retval the new media keys for ipc instance or null on error.
     */
    virtual std::unique_ptr<IMediaKeys> createMediaKeysIpc(const std::string &keySystem) const = 0;
};

}; // namespace firebolt::rialto::client

#endif // FIREBOLT_RIALTO_CLIENT_I_MEDIA_KEYS_IPC_FACTORY_H_
