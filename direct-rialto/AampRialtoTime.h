/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2026 RDK Management
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

/**
 * @file AampRialtoTime.h
 * @brief Time unit constants and seconds conversions shared by direct-Rialto.
 */

#ifndef AAMP_RIALTO_TIME_H
#define AAMP_RIALTO_TIME_H

#include <cmath>
#include <cstdint>

/// Nanoseconds per second.
constexpr int64_t kNsPerSecond = 1'000'000'000LL;

/// Nanoseconds per millisecond.
constexpr int64_t kNsPerMs = 1'000'000LL;

/// Seconds to nanoseconds, rounded to nearest (truncation turns 1.001 s into
/// 1000999999 ns).
inline int64_t SecondsToNs(double seconds)
{
	return std::llround(seconds * static_cast<double>(kNsPerSecond));
}

/// Seconds to milliseconds, rounded to nearest.
inline int64_t SecondsToMs(double seconds)
{
	return std::llround(seconds * 1000.0);
}

#endif // AAMP_RIALTO_TIME_H
