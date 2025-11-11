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

#ifndef FIREBOLT_RIALTO_SERVER_SHM_UTILS_H_
#define FIREBOLT_RIALTO_SERVER_SHM_UTILS_H_

#include "ShmCommon.h"
#include <cstdint>

namespace firebolt::rialto::server
{
constexpr std::uint32_t kPrerollNumFrames{3};
constexpr std::uint32_t kMaxFrames{24};
constexpr std::uint32_t getMaxMetadataBytes()
{
    // The Rialto Server must size the metadata regions to be at least the following size:
    // 4 bytes version + max_frames_to_request * (maximum metadata struct size for stream type &
    //                                            supported metadata format versions)

    // Metadata V2 contains version only, so maximum metadata size is size of MetadataV1
    std::uint32_t maxMetadataStructSize = common::METADATA_V1_SIZE_PER_FRAME_BYTES;
    return common::VERSION_SIZE_BYTES + kMaxFrames * maxMetadataStructSize;
}
} // namespace firebolt::rialto::server

#endif // FIREBOLT_RIALTO_SERVER_SHM_UTILS_H_
