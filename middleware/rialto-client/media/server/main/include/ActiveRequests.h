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

#ifndef FIREBOLT_RIALTO_SERVER_ACTIVE_REQUESTS_H_
#define FIREBOLT_RIALTO_SERVER_ACTIVE_REQUESTS_H_

#include "IActiveRequests.h"
#include <map>
#include <memory>
#include <mutex>

namespace firebolt::rialto::server
{
class ActiveRequests : public IActiveRequests
{
public:
    class ActiveRequestsData
    {
    public:
        ActiveRequestsData(MediaSourceType type, std::uint32_t maxMediaBytes)
            : m_type(type), m_bytesWritten(0), m_maxMediaBytes(maxMediaBytes)
        {
        }
        ~ActiveRequestsData();
        ActiveRequestsData(ActiveRequestsData &&) = default;
        ActiveRequestsData &operator=(ActiveRequestsData &&) = default;

        ActiveRequestsData(const ActiveRequestsData &) = delete;
        ActiveRequestsData &operator=(const ActiveRequestsData &) = delete;

        AddSegmentStatus addSegment(const std::unique_ptr<IMediaPipeline::MediaSegment> &segment);

        MediaSourceType getType() const { return m_type; }
        const IMediaPipeline::MediaSegmentVector &getSegments() const { return m_segments; }

    private:
        MediaSourceType m_type;
        std::uint32_t m_bytesWritten;
        std::uint32_t m_maxMediaBytes;
        IMediaPipeline::MediaSegmentVector m_segments;
    };

    ActiveRequests();
    ~ActiveRequests() override = default;
    ActiveRequests(const ActiveRequests &) = delete;
    ActiveRequests(ActiveRequests &&) = delete;
    ActiveRequests &operator=(const ActiveRequests &) = delete;
    ActiveRequests &operator=(ActiveRequests &&) = delete;

    std::uint32_t insert(const MediaSourceType &mediaSourceType, std::uint32_t maxMediaBytes) override;
    MediaSourceType getType(std::uint32_t requestId) const override;
    void erase(std::uint32_t requestId) override;
    void erase(const MediaSourceType &mediaSourceType) override;
    void clear() override;
    AddSegmentStatus addSegment(std::uint32_t requestId,
                                const std::unique_ptr<IMediaPipeline::MediaSegment> &segment) override;
    const IMediaPipeline::MediaSegmentVector &getSegments(std::uint32_t requestId) const override;

private:
    mutable std::mutex m_mutex;
    std::uint32_t m_currentId;
    std::map<std::uint32_t, ActiveRequestsData> m_requestMap;
    // only used in server-only mode
    std::map<std::uint32_t, std::unique_ptr<IMediaPipeline::IMediaPipeline::MediaSegment>> m_addedSegments;
};

} // namespace firebolt::rialto::server

#endif // FIREBOLT_RIALTO_SERVER_ACTIVE_REQUESTS_H_
