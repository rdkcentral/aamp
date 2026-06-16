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

#ifndef RialtoMediaKeySession_h
#define RialtoMediaKeySession_h

/**
 * @file RialtoMediaKeySession.h
 * @brief A single Rialto DRM key session.
 *
 * Wraps a firebolt::rialto::IMediaKeys session identified by keySessionId.
 * Delegates update/close/destruct operations to IMediaKeys and caches per-key
 * status from onKeyStatusesChanged callbacks.
 */

#include "IMediaKeys.h"

#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <vector>

/**
 * @class RialtoMediaKeySession
 * @brief Represents a single Rialto DRM session.
 *
 * Decryption is performed server-side by the Rialto pipeline — this class
 * only handles key management (update, close, status queries).
 */
class RialtoMediaKeySession
{
public:
	/**
	 * @brief Construct a session wrapper.
	 *
	 * @param keySessionId  Session ID assigned by IMediaKeys::createKeySession().
	 * @param mediaKeys     Reference to the owning system's IMediaKeys instance.
	 *                      Must outlive this object.
	 * @param deregister    Callback invoked from destruct() to remove this
	 *                      session from the system's client maps.
	 */
	RialtoMediaKeySession(int32_t keySessionId,
	                      firebolt::rialto::IMediaKeys& mediaKeys,
	                      std::function<void(int32_t)> deregister);

	~RialtoMediaKeySession() = default;

	RialtoMediaKeySession(const RialtoMediaKeySession&) = delete;
	RialtoMediaKeySession& operator=(const RialtoMediaKeySession&) = delete;

	/**
	 * @brief Deliver a license response to the Rialto server.
	 * @return true on success, false on failure.
	 */
	bool update(const uint8_t* keyMessage, uint16_t keyMessageLength);

	/**
	 * @brief Close the key session on the Rialto server.
	 * @return true on success, false on failure.
	 */
	bool close();

	/**
	 * @brief Release all resources and deregister from the system.
	 * @return true on success, false on failure.
	 */
	bool destruct();

	/**
	 * @brief Query whether a specific key is usable.
	 *
	 * Checks the local cache updated by updateKeyStatus().
	 *
	 * @param keyId     Key ID bytes.
	 * @param keyIdSize Length of keyId.
	 * @return true if the key status is USABLE.
	 */
	bool isKeyUsable(const uint8_t* keyId, uint8_t keyIdSize) const;

	/**
	 * @brief Check if a key has OUTPUT_RESTRICTED status.
	 *
	 * @param keyId     Key ID bytes.
	 * @param keyIdSize Length of keyId.
	 * @return true if the key status is OUTPUT_RESTRICTED.
	 */
	bool isKeyOutputRestricted(const uint8_t* keyId, uint8_t keyIdSize) const;

	/**
	 * @brief Get the Rialto media key session ID.
	 */
	int32_t getMediaKeySessionId() const { return m_keySessionId; }

	/**
	 * @brief Update the cached status for a key.
	 *
	 * Called by RialtoMediaKeySystem's MediaKeysClient when
	 * onKeyStatusesChanged is received from the Rialto server.
	 */
	void updateKeyStatus(const std::vector<uint8_t>& keyId,
	                     firebolt::rialto::KeyStatus status);

private:
	int32_t                          m_keySessionId;
	firebolt::rialto::IMediaKeys&    m_mediaKeys;
	std::function<void(int32_t)>     m_deregister;

	/// Per-key status cache, updated from onKeyStatusesChanged callbacks.
	std::map<std::vector<uint8_t>, firebolt::rialto::KeyStatus> m_keyStatuses;
	mutable std::mutex m_keyStatusMutex;
};

#endif // RialtoMediaKeySession_h
