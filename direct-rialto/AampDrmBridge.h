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
 * @file AampDrmBridge.h
 * @brief Production concrete implementation of IDrmBridge for AampRialtoPlayer.
 *
 * Drives DRM session creation through AAMP's existing DrmSessionManager /
 * OCDM stack and returns the Rialto media key session ID (mks_id) so that
 * AampRialtoPlayer can stamp it onto every encrypted MediaSegment without
 * any GStreamer dependency.
 *
 * @note createSession() blocks until the full OCDM license acquisition
 *       flow completes (session creation → challenge → license server →
 *       key update).  It must not be called on a latency-critical thread.
 *       In normal AAMP operation it is called from the fragment-collector
 *       thread, which is acceptable.
 */

#pragma once

#include "IDrmBridge.h"

class PrivateInstanceAAMP;

/**
 * @class AampDrmBridge
 * @brief Concrete IDrmBridge that delegates to AAMP's DrmSessionManager.
 *
 * One instance is owned by AampRialtoPlayer for its lifetime.
 */
class AampDrmBridge : public IDrmBridge
{
public:
	/**
	 * @brief Construct the bridge.
	 * @param[in] aamp  Owning AAMP instance; must outlive this object.
	 */
	explicit AampDrmBridge(PrivateInstanceAAMP *aamp);

	~AampDrmBridge() override = default;

	AampDrmBridge(const AampDrmBridge &) = delete;
	AampDrmBridge &operator=(const AampDrmBridge &) = delete;

	/**
	 * @brief Create a DRM session via AAMP's DrmSessionManager and return
	 *        the resulting Rialto media key session ID.
	 *
	 * Internally calls DrmSessionManager::createDrmSession() which drives
	 * the full OCDM license acquisition flow.  On success the mks_id
	 * stored inside the OCDMSessionAdapter is returned; on failure -1 is
	 * returned.
	 *
	 * @param[in] systemId   DRM system UUID string.
	 * @param[in] initData   PSSH init data blob.
	 * @param[in] len        Byte length of @p initData.
	 * @param[in] type       Media type (video / audio).
	 *
	 * @return Rialto mks_id (>= 0) on success, -1 on failure.
	 */
	int32_t createSession(
		const char    *systemId,
		const void    *initData,
		size_t         len,
		AampMediaType  type) override;

	/**
	 * @brief Release all DRM sessions via AAMP's DrmSessionManager.
	 */
	void clearSessions() override;

private:
	PrivateInstanceAAMP *m_aamp; ///< Non-owning pointer; lifetime guaranteed by AampRialtoPlayer
};
