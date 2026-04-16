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

#ifndef IOpenCDM_h
#define IOpenCDM_h

/**
 * @file IOpenCDM.h
 * @brief Abstraction over the OpenCDM C library API.
 *
 * Replaces direct calls to the opencdm_* C functions with virtual method calls
 * so that two concrete implementations can be selected at runtime:
 *
 *  - OpenCDMProvider      : delegates to the real OCDM C library.
 *  - RialtoMediaKeysProvider : delegates to firebolt::rialto::IMediaKeys.
 *
 * The correct implementation is chosen by OpenCDMProviderFactory based on the
 * eAAMPConfig_useRialtoDirect config flag.
 *
 * TODO: [SHARED-INFRA] This file lives in middleware/drm/ocdm/ which is shared
 *       AAMP infrastructure.  Per direct-rialto.instructions.md, changes to
 *       shared files must be reviewed independently from direct-rialto work.
 */

#include "open_cdm.h"
#include "open_cdm_adapter.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

// Forward-declare GStreamer opaque pointer types so this header does not drag
// in all of GStreamer.  Concrete implementations that actually call GStreamer
// APIs include the real GStreamer headers in their .cpp files.
struct _GstBuffer;
typedef struct _GstBuffer GstBuffer;
struct _GstCaps;
typedef struct _GstCaps GstCaps;

/**
 * @struct OpenCDMSessionCallbackSet
 * @brief Replaces the raw C OpenCDMSessionCallbacks struct.
 *
 * Using std::function members allows clean lambda captures without the
 * void* userData cookie required by the C callback API.
 */
struct OpenCDMSessionCallbackSet
{
	/// Fired when a license challenge is ready to be sent to the license server.
	std::function<void(const char* url, const uint8_t* challenge, uint16_t size)>
		onChallenge;

	/// Fired when the status of a single key changes.
	std::function<void(const uint8_t* key, uint8_t keySize)>
		onKeyUpdate;

	/// Fired once after all key status changes for an update have been delivered.
	std::function<void()>
		onKeysUpdated;

	/// Fired when a license renewal message is received from the DRM server.
	/// Only fired on the Rialto Direct path via IMediaKeysClient::onLicenseRenewal.
	/// The OCDM path delivers renewals through onChallenge with message type "1".
	std::function<void(const uint8_t* message, size_t size)>
		onLicenseRenewal;

	/// The C-level callback struct passed to opencdm_construct_session.
	/// Stored here (on the heap) so the pointer passed to the OCDM library
	/// remains valid for the lifetime of the session.
	/// Populated and used exclusively by OpenCDMProvider::constructSession.
	OpenCDMSessionCallbacks cCallbacks{};
};

/**
 * @class IOpenCDMSession
 * @brief Abstracts a single DRM key session.
 *
 * Concrete implementations:
 *  - OpenCDMSessionProvider : wraps a raw OpenCDMSession* from the OCDM library.
 *  - RialtoOpenCDMSession   : delegates to firebolt::rialto::IMediaKeys.
 *                              decrypt() and decryptGst() are no-op stubs —
 *                              decryption is performed server-side by the Rialto
 *                              pipeline and does not go through this interface.
 */
class IOpenCDMSession
{
public:
	virtual ~IOpenCDMSession() = default;

	/**
	 * @brief Query the status of a specific key.
	 */
	virtual KeyStatus getStatus(const uint8_t* keyId, uint8_t keyIdSize) = 0;

	/**
	 * @brief Deliver a license response to the DRM system.
	 */
	virtual OpenCDMError update(const uint8_t* keyMessage,
	                            uint16_t keyMessageLength) = 0;

	/**
	 * @brief Signal that the session should be closed.
	 */
	virtual OpenCDMError close() = 0;

	/**
	 * @brief Release all resources held by this session.
	 */
	virtual OpenCDMError destruct() = 0;

	/**
	 * @brief Returns true if the session supports the GStreamer buffer/caps
	 *        decrypt API.
	 *
	 * When true, callers should prefer decryptGst(); otherwise decryptGstLegacy()
	 * must be used.  The Rialto implementation always returns false.
	 */
	virtual bool hasGstDecryptBuffer() = 0;

	/**
	 * @brief Return the Rialto media key session ID.
	 *
	 * For the OCDM library path (OpenCDMSessionProvider) this returns -1 as
	 * the OCDM library does not expose an equivalent concept.
	 * For the Rialto path (RialtoOpenCDMSession) this returns the
	 * keySessionId assigned by firebolt::rialto::IMediaKeys::createKeySession().
	 */
	virtual int32_t getMediaKeySessionId() = 0;

	/**
	 * @brief Decrypt a raw buffer (non-GStreamer path).
	 *
	 * @note The Rialto implementation is a no-op stub that returns ERROR_NONE.
	 *       Decryption for the Rialto path is performed server-side.
	 */
	virtual OpenCDMError decrypt(uint8_t* data,
	                             uint32_t dataSize,
	                             EncryptionScheme encScheme,
	                             EncryptionPattern pattern,
	                             const uint8_t* iv,
	                             uint16_t ivSize,
	                             const uint8_t* keyId,
	                             uint16_t keyIdSize,
	                             uint32_t initWithLast15) = 0;

	/**
	 * @brief Decrypt a GStreamer buffer using the buffer/caps API.
	 *
	 * Used by OCDMGSTSessionAdapter when opencdm_gstreamer_session_decrypt_buffer
	 * is available (dlsym).
	 *
	 * @note The Rialto implementation is a no-op stub that returns ERROR_NONE.
	 */
	virtual OpenCDMError decryptGst(GstBuffer* buffer, GstCaps* caps) = 0;

	/**
	 * @brief Decrypt a GStreamer buffer using the legacy sub-sample API.
	 *
	 * Used by OCDMGSTSessionAdapter as a fallback when
	 * opencdm_gstreamer_session_decrypt_buffer is not available.
	 *
	 * @note The Rialto implementation is a no-op stub that returns ERROR_NONE.
	 */
	virtual OpenCDMError decryptGstLegacy(GstBuffer* buffer,
	                                      GstBuffer* subSamples,
	                                      uint32_t subSampleCount,
	                                      GstBuffer* iv,
	                                      GstBuffer* keyId,
	                                      uint32_t initWithLast15) = 0;
};

/**
 * @class IOpenCDM
 * @brief Abstracts the DRM system object (one per key system).
 *
 * Concrete implementations:
 *  - OpenCDMProvider         : wraps OpenCDMSystem / opencdm_create_system().
 *  - RialtoMediaKeysProvider : wraps firebolt::rialto::IMediaKeys.
 */
class IOpenCDM
{
public:
	virtual ~IOpenCDM() = default;

	/**
	 * @brief Construct a new DRM session.
	 *
	 * @param[in] keySystem       DRM system UUID string (e.g. Widevine UUID).
	 * @param[in] licenseType     License type (e.g. LicenseType::Temporary).
	 * @param[in] initDataType    Init data format string (e.g. "cenc").
	 * @param[in] initData        PSSH init data blob.
	 * @param[in] initDataSize    Byte length of initData.
	 * @param[in] customData      Optional custom data (may be nullptr).
	 * @param[in] customDataSize  Byte length of customData.
	 * @param[in] callbacks       Callback set for challenge and key-update events.
	 *
	 * @return Owning pointer to the new session, or nullptr on failure.
	 */
	virtual std::unique_ptr<IOpenCDMSession> constructSession(
		const std::string&              keySystem,
		LicenseType                     licenseType,
		const std::string&              initDataType,
		const uint8_t*                  initData,
		uint32_t                        initDataSize,
		const uint8_t*                  customData,
		uint16_t                        customDataSize,
		const OpenCDMSessionCallbackSet& callbacks) = 0;
};

#endif // IOpenCDM_h
