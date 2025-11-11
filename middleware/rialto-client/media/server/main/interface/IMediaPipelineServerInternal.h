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

#ifndef FIREBOLT_RIALTO_SERVER_I_MEDIA_PIPELINE_SERVER_INTERNAL_H_
#define FIREBOLT_RIALTO_SERVER_I_MEDIA_PIPELINE_SERVER_INTERNAL_H_

/**
 * @file IMediaPipelineServerInternal.h
 *
 * The definition of the IMediaPipelineServerInternal interface.
 *
 * This interface defines the server internal APIs for playback of AV content.
 */

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "IDecryptionService.h"
#include "IHeartbeatHandler.h"
#include "IMediaPipeline.h"
#include "ISharedMemoryBuffer.h"
#include <MediaCommon.h>

namespace firebolt::rialto::server
{
class IMediaPipelineServerInternal;

/**
 * @brief IMediaPipelineServerInternal factory class, returns a concrete implementation of IMediaPipelineServerInternal
 */
class IMediaPipelineServerInternalFactory : public IMediaPipelineFactory
{
public:
    virtual ~IMediaPipelineServerInternalFactory() = default;

    /**
     * @brief Create a IMediaPipelineServerInternalFactory instance.
     *
     * @retval the factory instance or null on error.
     */
    static std::shared_ptr<IMediaPipelineServerInternalFactory> createFactory();

    /**
     * @brief IMediaPipelineServerInternal factory method, returns a concrete implementation of IMediaPipeline
     *
     * @param[in] client            : The Rialto media player client.
     * @param[in] videoRequirements : The video decoder requirements for the MediaPipeline session
     * @param[in] sessionId         : The session id for this MediaPipeline.
     * @param[in] shmBuffer         : The shared buffer object.
     * @param[in] decryptionService : The decryption service object.
     *
     * @retval the new backend instance or null on error.
     */
    virtual std::unique_ptr<IMediaPipelineServerInternal> createMediaPipelineServerInternal(
        std::weak_ptr<IMediaPipelineClient> client, const VideoRequirements &videoRequirements, int sessionId,
        const std::shared_ptr<ISharedMemoryBuffer> &shmBuffer, IDecryptionService &decryptionService) const = 0;
};

/**
 * @brief The definition of the IMediaPipelineServerInternal interface.
 *
 * This interface defines the internal server APIs for playback of AV content which
 * should be implemented by Rialto Server only.
 */
class IMediaPipelineServerInternal : public IMediaPipeline
{
public:
    /**
     * @brief Returns data requested using notifyNeedMediaData().
     *
     * This is a server only implementation. The data from the client has already been
     * writen to the shared memory.
     *
     * A successful notifyNeedMediaData() request will return at least one frame
     * of data but may return less than originally requested.
     *
     * @param[in] status            : The status
     * @param[in] numFrames         : The number of frames written.
     * @param[in] needDataRequestId : Need data request id
     */
    virtual bool haveData(MediaSourceStatus status, uint32_t numFrames, uint32_t needDataRequestId) = 0;

    /**
     * @brief Checks if MediaPipeline threads are not deadlocked
     *
     * @param[out] heartbeatHandler : The heartbeat handler instance
     */
    virtual void ping(std::unique_ptr<IHeartbeatHandler> &&heartbeatHandler) = 0;
};

}; // namespace firebolt::rialto::server

#endif // FIREBOLT_RIALTO_SERVER_I_MEDIA_PIPELINE_SERVER_INTERNAL_H_
