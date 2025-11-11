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

#ifndef FIREBOLT_RIALTO_COMMON_SHM_COMMON_H_
#define FIREBOLT_RIALTO_COMMON_SHM_COMMON_H_

#include <stdint.h>

namespace firebolt::rialto::common
{
/**
 * @brief Metadata v1 size per frame in bytes.
 */
const uint32_t VERSION_SIZE_BYTES = 4U;

/**
 * @brief Maximum bytes of extra data.
 */
const uint32_t MAX_EXTRA_DATA_SIZE = 32U;

/**
 * @brief Metadata v1 size per frame in bytes.
 */
const uint32_t METADATA_V1_SIZE_PER_FRAME_BYTES = 104U;
}; // namespace firebolt::rialto::common

#endif // FIREBOLT_RIALTO_COMMON_SHM_COMMON_H_
