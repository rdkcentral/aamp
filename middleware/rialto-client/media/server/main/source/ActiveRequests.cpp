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

#include "ActiveRequests.h"
#include <cstring>
#include <stdexcept>

namespace firebolt::rialto::server
{
ActiveRequests::ActiveRequestsData::~ActiveRequestsData()
{
    for (std::unique_ptr<IMediaPipeline::MediaSegment> &segment : m_segments)
    {
        delete[] segment->getData();
    }
}

AddSegmentStatus ActiveRequests::ActiveRequestsData::addSegment(const std::unique_ptr<IMediaPipeline::MediaSegment> &segment)
{
    if (m_bytesWritten + segment->getDataLength() > m_maxMediaBytes)
        return AddSegmentStatus::NO_SPACE;

    std::unique_ptr<IMediaPipeline::MediaSegment> copiedSegment = segment->copy();

    uint8_t *data = new uint8_t[segment->getDataLength()];
    std::memcpy(data, segment->getData(), segment->getDataLength());

    copiedSegment->setData(segment->getDataLength(), data);
    m_segments.push_back(std::move(copiedSegment));

    m_bytesWritten += segment->getDataLength();
    return AddSegmentStatus::OK;
}

ActiveRequests::ActiveRequests() : m_currentId{0} {}

std::uint32_t ActiveRequests::insert(const MediaSourceType &mediaSourceType, std::uint32_t maxMediaBytes)
{
    std::unique_lock<std::mutex> lock{m_mutex};
    m_requestMap.insert(std::make_pair(m_currentId, ActiveRequestsData(mediaSourceType, maxMediaBytes)));
    return m_currentId++;
}

MediaSourceType ActiveRequests::getType(std::uint32_t requestId) const
{
    std::unique_lock<std::mutex> lock{m_mutex};
    auto requestIter{m_requestMap.find(requestId)};
    if (requestIter != m_requestMap.end())
    {
        return requestIter->second.getType();
    }
    return MediaSourceType::UNKNOWN;
}

void ActiveRequests::erase(std::uint32_t requestId)
{
    std::unique_lock<std::mutex> lock{m_mutex};
    m_requestMap.erase(requestId);
}

void ActiveRequests::erase(const MediaSourceType &mediaSourceType)
{
    std::unique_lock<std::mutex> lock{m_mutex};
    for (auto it = m_requestMap.begin(); it != m_requestMap.end();)
    {
        if (it->second.getType() == mediaSourceType)
        {
            it = m_requestMap.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void ActiveRequests::clear()
{
    std::unique_lock<std::mutex> lock{m_mutex};
    m_requestMap.clear();
}

AddSegmentStatus ActiveRequests::addSegment(std::uint32_t requestId,
                                            const std::unique_ptr<IMediaPipeline::MediaSegment> &segment)
{
    if (nullptr == segment || nullptr == segment->getData())
    {
        return AddSegmentStatus::ERROR;
    }

    auto requestIter{m_requestMap.find(requestId)};
    if (requestIter != m_requestMap.end())
    {
        return requestIter->second.addSegment(segment);
    }

    return AddSegmentStatus::ERROR;
}

const IMediaPipeline::MediaSegmentVector &ActiveRequests::getSegments(std::uint32_t requestId) const
{
    auto requestIter{m_requestMap.find(requestId)};
    if (requestIter != m_requestMap.end())
    {
        return requestIter->second.getSegments();
    }

    throw std::runtime_error("No segments for request id " + std::to_string(requestId));
}
} // namespace firebolt::rialto::server
