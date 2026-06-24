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
 * @file IDrmSession.h
 * @brief Pure virtual interface for DRM sessions.
 *
 * Callers that create or hold DRM sessions should depend on this interface,
 * not on the concrete DrmSession base class. This keeps the DRM session
 * lifecycle contract decoupled from middleware-specific implementation
 * details (GStreamer buffers, ContentSecurityManagerSession, etc.).
 */

#ifndef IDrmSession_h
#define IDrmSession_h

#include <cstdint>
#include <string>
#include <vector>
#include "DrmUtils.h"
#include "ContentSecurityManagerSession.h"

// Forward declarations for GStreamer types used in the GstBuffer decrypt overload.
// Including gst headers here would add a GStreamer dependency to the interface;
// forward declarations are sufficient for pointer/reference parameters.
typedef struct _GstBuffer GstBuffer;
typedef struct _GstCaps   GstCaps;

/**
 * @brief HDCP compliance check failure error code.
 */
#define HDCP_COMPLIANCE_CHECK_FAILURE 4327

/**
 * @brief HDCP output protection failure error code.
 */
#define HDCP_OUTPUT_PROTECTION_FAILURE 4427

#define PLAYREADY_KEY_SYSTEM_STRING "com.microsoft.playready"
#define WIDEVINE_KEY_SYSTEM_STRING "com.widevine.alpha"
#define CLEAR_KEY_SYSTEM_STRING "org.w3.clearkey"
#define VERIMATRIX_KEY_SYSTEM_STRING "com.verimatrix.ott"

/**
 * @enum KeyState
 * @brief DRM session states.
 *
 * Defined here so that IDrmSession is self-contained without pulling in
 * the full DrmSession.h.
 */
typedef enum
{
	KEY_INIT = 0,                        /**< Has been initialized */
	KEY_PENDING = 1,                     /**< Has a key message pending to be processed */
	KEY_READY = 2,                       /**< Has a usable key */
	KEY_ERROR = 3,                       /**< Has an error */
	KEY_CLOSED = 4,                      /**< Has been closed */
	KEY_ERROR_EMPTY_SESSION_ID = 5,      /**< Has Empty DRM session id */
	KEY_ERROR_SESSION_CREATE_FAILED = 6  /**< Session creation failed (OCDM) */
} KeyState;

/**
 * @class IDrmSession
 * @brief Pure virtual interface for a DRM session.
 *
 * Concrete implementations live in the middleware (DrmSession /
 * OCDMSessionAdapter / ClearKeySession) and in direct-rialto
 * (RialtoMediaKeySessionAdapter).  All factory and manager code should
 * hold IDrmSession* rather than DrmSession* so that direct-rialto
 * implementations carry no middleware dependency.
 */
class IDrmSession
{
public:
	virtual ~IDrmSession() = default;

	IDrmSession(const IDrmSession&) = delete;
	IDrmSession& operator=(const IDrmSession&) = delete;

	/**
	 * @brief Create DRM session with given init data.
	 */
	virtual void generateDRMSession(const uint8_t* f_pbInitData,
	                                uint32_t f_cbInitData,
	                                std::string& customData) = 0;

	/**
	 * @brief Generate key request from DRM session.
	 *        Caller is responsible for freeing the returned DrmData.
	 */
	virtual DrmData* generateKeyRequest(std::string& destinationURL,
	                                    uint32_t timeout) = 0;

	/**
	 * @brief Deliver a license key to the DRM session.
	 */
	virtual int processDRMKey(DrmData* key, uint32_t timeout) = 0;

	/**
	 * @brief Get the current state of the DRM session.
	 */
	virtual KeyState getState() = 0;

	/**
	 * @brief Wait for the session to reach the given state.
	 * @return true if the state was reached before timeout.
	 */
	virtual bool waitForState(KeyState state, uint32_t timeout) = 0;

	/**
	 * @brief Clear the current session context so new init data can be bound.
	 */
	virtual void clearDecryptContext() = 0;

	/**
	 * @brief Return a snapshot of the current usable key IDs.
	 */
	virtual std::vector<std::vector<uint8_t>> getUsableKeys() const { return {}; }

	/**
	 * @brief Return the Rialto media key session ID, or -1 if not applicable.
	 */
	virtual int32_t getMediaKeySessionId() const { return -1; }

	/**
	 * @brief Return the DRM system UUID string for this session.
	 */
	virtual std::string getKeySystem() = 0;

	/**
	 * @brief Enable or disable output protection for this session.
	 */
	virtual void setOutputProtection(bool bValue) = 0;

		/**
	 * @brief Decrypt a block of encrypted data (non-GStreamer path).
	 *
	 * @param[in]  f_pbIV           Initialisation vector.
	 * @param[in]  f_cbIV           Length of IV in bytes.
	 * @param[in]  payloadData      Encrypted data buffer.
	 * @param[in]  payloadDataSize  Length of encrypted data.
	 * @param[out] ppOpaqueData     Output opaque data pointer (may be null).
	 * @return 0 on success, non-zero on failure.
	 */
	virtual int decrypt(const uint8_t *f_pbIV, uint32_t f_cbIV,
		const uint8_t *payloadData, uint32_t payloadDataSize,
		uint8_t **ppOpaqueData)  { return 0; }

	/**
	 * @brief Decrypt a GStreamer buffer (OCDM/GStreamer path).
	 *
	 * @param[in] keyIDBuffer      Key ID GstBuffer.
	 * @param[in] ivBuffer         IV GstBuffer.
	 * @param[in] buffer           Encrypted data GstBuffer.
	 * @param[in] subSampleCount   Number of sub-samples.
	 * @param[in] subSamplesBuffer Sub-sample mapping GstBuffer.
	 * @param[in] caps             Sink caps of the media being decrypted.
	 * @return 0 on success, non-zero on failure.
	 */
	virtual int decrypt(GstBuffer* /*keyIDBuffer*/, GstBuffer* /*ivBuffer*/,
		GstBuffer* /*buffer*/, unsigned /*subSampleCount*/,
		GstBuffer* /*subSamplesBuffer*/, GstCaps* /*caps*/ = nullptr) { return 0; }

    /**
     * @brief Store the DRM key ID for this session.
     *        No-op for session types that do not use key-ID tracking.
     */
    virtual void setKeyId(const std::vector<uint8_t>& /*keyId*/) {}

    /**
     * @brief Associate a ContentSecurityManager session with this DRM session.
	 *        No-op for session types that do not use ContentSecurityManager.
	 */
	virtual void setSecManagerSession(ContentSecurityManagerSession /*session*/) = 0;

	/**
	 * @brief Return the associated ContentSecurityManager session.
	 *        Returns a default (invalid) session for types that do not use it.
	 */
	virtual ContentSecurityManagerSession getSecManagerSession() const = 0;

protected:
	IDrmSession() = default;
};

#endif // IDrmSession_h
