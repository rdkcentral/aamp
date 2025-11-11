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

#include "DataReaderFactory.h"
#include "DataReaderV1.h"
#include "DataReaderV2.h"
#include "ShmCommon.h"
#include "ShmUtils.h"

namespace
{
uint32_t readLEUint32(const uint8_t *buffer)
{
    uint32_t value = buffer[3] << 24 | buffer[2] << 16 | buffer[1] << 8 | buffer[0];
    return value;
}
} // namespace

namespace firebolt::rialto::server
{
std::shared_ptr<IDataReader> DataReaderFactory::createDataReader(const MediaSourceType &mediaSourceType,
                                                                 std::uint8_t *buffer, std::uint32_t dataOffset,
                                                                 std::uint32_t numFrames) const
{
    // Version is always first 4 bytes of data
    std::uint8_t *metadata = buffer + dataOffset;
    std::uint32_t version = readLEUint32(metadata);
    if (1 == version)
    {
        std::uint32_t metadataOffsetWithoutVersion = dataOffset + common::VERSION_SIZE_BYTES;
        return std::make_shared<DataReaderV1>(mediaSourceType, buffer, metadataOffsetWithoutVersion, numFrames);
    }
    if (2 == version)
    {
        std::uint32_t v2DataOffset = dataOffset + getMaxMetadataBytes();
        return std::make_shared<DataReaderV2>(mediaSourceType, buffer, v2DataOffset, numFrames);
    }
    return nullptr;
}
} // namespace firebolt::rialto::server
