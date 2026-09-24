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
 * @file IDrmBridge.h
 * @brief Interface for DRM session management within AampRialtoPlayer.
 *
 * Abstracts creation of DRM sessions and retrieval of the Rialto media key
 * session ID (mks_id) so that AampRialtoPlayer depends on an interface rather
 * than a concrete OCDM/DrmSessionManager implementation.  This enables full
 * mocking in unit tests without any GStreamer dependency.
 *
 * The concrete production implementation is responsible for driving the full
 * OCDM license flow (challenge → license server → key ready) and returning the
 * Rialto session ID assigned by the server during opencdm_construct_session().
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include "AampMediaType.h"

/**
 * @class IDrmBridge
 * @brief Pure interface for DRM session lifecycle inside AampRialtoPlayer.
 *
 * Follows the Dependency Inversion Principle: AampRialtoPlayer depends on this
 * abstraction; concrete OCDM/Rialto details live in a separate class.
 */
class IDrmBridge
{
public:
	/**
	 * @brief Virtual destructor.
	 */
	virtual ~IDrmBridge() = default;

	/**
	 * @brief Create a DRM session for the given protection event.
	 *
	 * Implementations are expected to drive the full license acquisition flow
	 * (generate request → fetch license → update session) before returning.
	 * The returned value is the Rialto media key session ID (mks_id) that must
	 * be stamped onto every encrypted MediaSegment for this media type.
	 *
	 * @param[in] systemId   DRM system UUID string (e.g. Widevine UUID).
	 * @param[in] initData   PSSH init data blob.
	 * @param[in] len        Byte length of @p initData.
	 * @param[in] type       Media type the protection event applies to.
	 *
	 * @return Rialto media key session ID (>= 0) on success, -1 on failure.
	 */
	virtual int32_t createSession(
		const char     *systemId,
		const void     *initData,
		size_t          len,
		AampMediaType   type) = 0;

	/**
	 * @brief Release all DRM sessions created by this bridge.
	 *
	 * Called from ClearProtectionEvent().  Implementations should release any
	 * held OCDM sessions and reset internal state.
	 */
	virtual void clearSessions() = 0;
};
