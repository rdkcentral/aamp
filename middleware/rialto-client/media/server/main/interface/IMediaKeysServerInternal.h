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

#ifndef FIREBOLT_RIALTO_SERVER_I_MEDIA_KEYS_SERVER_INTERNAL_H_
#define FIREBOLT_RIALTO_SERVER_I_MEDIA_KEYS_SERVER_INTERNAL_H_

/**
 * @file IMediaKeysServerInternal.h
 *
 * The definition of the IMediaKeys interface.
 *
 * This interface defines the server internal API of Rialto for EME decryption of AV content.
 */

#include "IHeartbeatHandler.h"
#include "IMediaKeys.h"
#include "MediaCommon.h"
#include <cstdint>
#include <gst/gst.h>
#include <memory>
#include <string>
#include <vector>

namespace firebolt::rialto::server
{
class IMediaKeysServerInternal;
/**
 * @brief IMediaKeysServerInternal factory class, returns a concrete implementation of IMediaKeysServerInternal
 */
class IMediaKeysServerInternalFactory : public IMediaKeysFactory
{
public:
    IMediaKeysServerInternalFactory() = default;
    ~IMediaKeysServerInternalFactory() override = default;

    /**
     * @brief Create a IMediaKeysServerInternalFactory instance.
     *
     * @retval the factory instance or null on error.
     */
    static std::shared_ptr<IMediaKeysServerInternalFactory> createFactory();

    /**
     * @brief IMediaKeysServerInternal factory method, returns a concrete implementation of IMediaKeysServerInternal
     *
     * @param[in] keySystem : The key system for which to create a Media Keys instance
     *
     * @retval the new media keys instance or null on error.
     */
    virtual std::unique_ptr<IMediaKeysServerInternal> createMediaKeysServerInternal(const std::string &keySystem) const = 0;
};

/**
 * @brief The definition of the IMediaKeysServerInternal interface.
 *
 * This interface defines the public API of Rialto for EME decryption of AV content
 * which should be implemented by Rialto Server.
 */
class IMediaKeysServerInternal : public IMediaKeys
{
public:
    /**
     * @brief Decrypts the buffer.
     *
     * Encryption metadata shall be attached to the encrypted buffer as protection meta prior to this call.
     *
     * @param[in] keySessionId    : The session id for the session.
     * @param[in] encrypted       : Gstreamer buffer containing encrypted data and related meta data. If applicable,
     *                              decrypted data will be stored here after this call returns.
     * @param[in] caps            : The gst caps of buffer.
     *
     * @retval an error status.
     */
    virtual MediaKeyErrorStatus decrypt(int32_t keySessionId, GstBuffer *encrypted, GstCaps *caps) = 0;

    /**
     * @brief Checks if session with given id is handled by this MediaKeys instance
     *
     * @param[in] keySessionId    : The session id for the session.
     *
     * @retval true if session is handled by this MediaKeys instance
     */
    virtual bool hasSession(int32_t keySessionId) const = 0;

    /**
     * @brief Checks, if key system of media key session is Playready.
     *
     * @param[in] keySessionId    : The session id for the session.
     *
     * @retval true if key system is Playready
     */
    virtual bool isPlayreadyKeySystem(int32_t keySessionId) const = 0;

    /**
     * @brief Increments number of buffers using keySessionId
     *
     * @param[in] keySessionId    : The session id for the session.
     *
     */

    virtual void incrementSessionIdUsageCounter(int32_t keySessionId) = 0;
    /**
     * @brief Decrements number of buffers using keySessionId
     *
     * @param[in] keySessionId    : The session id for the session.
     *
     */
    virtual void decrementSessionIdUsageCounter(int32_t keySessionId) = 0;

    /**
     * @brief Checks, if MediaKeys main thread is not deadlocked
     *
     * @param[in] heartbeatHandler    : The heartbeat handler instance.
     *
     */
    virtual void ping(std::unique_ptr<IHeartbeatHandler> &&heartbeatHandler) = 0;
};
} // namespace firebolt::rialto::server

#endif // FIREBOLT_RIALTO_SERVER_I_MEDIA_KEYS_SERVER_INTERNAL_H_
