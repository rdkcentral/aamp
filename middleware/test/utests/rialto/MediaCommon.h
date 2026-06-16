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
 * @file MediaCommon.h
 * @brief Stub Rialto MediaCommon types for unit tests.
 *
 * Provides the minimal subset of firebolt::rialto types needed to compile
 * the Rialto DRM classes in the test environment without the real Rialto SDK.
 */

#ifndef FIREBOLT_RIALTO_MEDIA_COMMON_H_
#define FIREBOLT_RIALTO_MEDIA_COMMON_H_

#include <cstdint>
#include <utility>
#include <vector>

namespace firebolt::rialto
{

constexpr int32_t kInvalidSessionId{-1};

enum class MediaKeyErrorStatus
{
	OK,
	FAIL,
	BAD_SESSION_ID,
	NOT_SUPPORTED,
	INVALID_STATE,
	INTERFACE_NOT_IMPLEMENTED,
	BUFFER_TOO_SMALL
};

enum class KeySessionType
{
	UNKNOWN,
	TEMPORARY,
	PERSISTENT_LICENCE,
	PERSISTENT_RELEASE_MESSAGE
};

enum class InitDataType
{
	UNKNOWN,
	CENC,
	KEY_IDS,
	WEBM,
	DRMHEADER
};

enum class KeyStatus
{
	USABLE,
	EXPIRED,
	OUTPUT_RESTRICTED,
	PENDING,
	INTERNAL_ERROR,
	RELEASED
};

typedef std::vector<std::pair<std::vector<unsigned char>, KeyStatus>> KeyStatusVector;

enum class LimitedDurationLicense
{
	NOT_SPECIFIED,
	ENABLED,
	DISABLED
};

} // namespace firebolt::rialto

#endif // FIREBOLT_RIALTO_MEDIA_COMMON_H_
