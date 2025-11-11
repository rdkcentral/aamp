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

#ifndef FIREBOLT_RIALTO_SERVER_I_ACTIVE_REQUESTS_H_
#define FIREBOLT_RIALTO_SERVER_I_ACTIVE_REQUESTS_H_

#include "MediaCommon.h"
#include <IMediaPipeline.h>
#include <cstdint>
#include <memory>

namespace firebolt::rialto::server
{
class IActiveRequests
{
public:
    IActiveRequests() = default;
    virtual ~IActiveRequests() = default;

    IActiveRequests(const IActiveRequests &) = delete;
    IActiveRequests(IActiveRequests &&) = delete;
    IActiveRequests &operator=(const IActiveRequests &) = delete;
    IActiveRequests &operator=(IActiveRequests &&) = delete;

    virtual std::uint32_t insert(const MediaSourceType &mediaSourceType, std::uint32_t maxMediaBytes) = 0;
    virtual MediaSourceType getType(std::uint32_t requestId) const = 0;
    virtual void erase(std::uint32_t requestId) = 0;
    virtual void erase(const MediaSourceType &mediaSourceType) = 0;
    virtual void clear() = 0;
    virtual AddSegmentStatus addSegment(std::uint32_t requestId,
                                        const std::unique_ptr<IMediaPipeline::MediaSegment> &segment) = 0;
    virtual const IMediaPipeline::MediaSegmentVector &getSegments(std::uint32_t requestId) const = 0;
};
} // namespace firebolt::rialto::server

#endif // FIREBOLT_RIALTO_SERVER_I_ACTIVE_REQUESTS_H_
