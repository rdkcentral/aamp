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

#ifndef OpenCDMProvider_h
#define OpenCDMProvider_h

/**
 * @file OpenCDMProvider.h
 * @brief Concrete IOpenCDM / IOpenCDMSession implementation that delegates
 *        to the real OCDM C library.
 *
 * All opencdm_* C function calls are confined to this translation unit,
 * including the USE_THUNDER_OCDM_API_0_2 API variant guards.
 *
 * TODO: [SHARED-INFRA] This file lives in middleware/drm/ocdm/ (shared AAMP
 *       infrastructure).  Per direct-rialto.instructions.md, it must be
 *       reviewed as a separate change from direct-rialto work.
 */

#include "IOpenCDM.h"

#include <dlfcn.h>
#include <memory>

/**
 * @class OpenCDMSessionProvider
 * @brief Wraps a raw OpenCDMSession* from the OCDM C library.
 */
class OpenCDMSessionProvider final : public IOpenCDMSession
{
public:
	/**
	 * @brief Construct with an already-created OCDM session handle.
	 *
	 * The handle ownership is taken; close() + destruct() will release it.
	 * @param handle  Raw OCDM session pointer (must not be nullptr).
	 */
	/**
	 * @param handle    Raw OCDM session pointer — takes ownership.
	 * @param callbacks Heap-allocated callback set created by OpenCDMProvider;
	 *                  this class takes ownership and deletes it on destruct().
	 */
	explicit OpenCDMSessionProvider(OpenCDMSession* handle,
	                                OpenCDMSessionCallbackSet* callbacks);

	~OpenCDMSessionProvider() override = default;

	OpenCDMSessionProvider(const OpenCDMSessionProvider&) = delete;
	OpenCDMSessionProvider& operator=(const OpenCDMSessionProvider&) = delete;

	KeyStatus    getStatus(const uint8_t* keyId, uint8_t keyIdSize) override;
	OpenCDMError update(const uint8_t* keyMessage, uint16_t keyMessageLength) override;
	OpenCDMError close() override;
	OpenCDMError destruct() override;

	/**
	 * @brief Returns -1.
	 *
	 * The real OCDM library does not expose a Rialto media key session ID.
	 * The Rialto session ID is meaningful only when the Rialto path is
	 * active (RialtoOpenCDMSession).
	 */
	int32_t getMediaKeySessionId() override;

	/**
	 * @brief Returns true when opencdm_gstreamer_session_decrypt_buffer was
	 *        resolved via dlsym at construction time.
	 */
	bool hasGstDecryptBuffer() override;

	OpenCDMError decrypt(uint8_t* data,
	                     uint32_t dataSize,
	                     EncryptionScheme encScheme,
	                     EncryptionPattern pattern,
	                     const uint8_t* iv,
	                     uint16_t ivSize,
	                     const uint8_t* keyId,
	                     uint16_t keyIdSize,
	                     uint32_t initWithLast15) override;

	OpenCDMError decryptGst(GstBuffer* buffer, GstCaps* caps) override;

	OpenCDMError decryptGstLegacy(GstBuffer* buffer,
	                               GstBuffer* subSamples,
	                               uint32_t subSampleCount,
	                               GstBuffer* iv,
	                               GstBuffer* keyId,
	                               uint32_t initWithLast15) override;

private:
	OpenCDMSession*            m_handle;
	OpenCDMSessionCallbackSet* m_callbacks; ///< Owned; deleted in destruct().

	/// Function pointer for opencdm_gstreamer_session_decrypt_buffer,
	/// resolved via dlsym at construction time.
	OpenCDMError (*m_gstDecrypt)(OpenCDMSession*, GstBuffer*, GstCaps*);
};

/**
 * @class OpenCDMProvider
 * @brief Wraps an OpenCDMSystem handle and creates OpenCDMSessionProvider
 *        instances via opencdm_construct_session().
 */
class OpenCDMProvider final : public IOpenCDM
{
public:
	/**
	 * @brief Create and initialise the OCDM system for the given key system.
	 * @param keySystem  DRM UUID string (used with USE_THUNDER_OCDM_API_0_2).
	 */
	explicit OpenCDMProvider(const std::string& keySystem);
	~OpenCDMProvider() override;

	OpenCDMProvider(const OpenCDMProvider&) = delete;
	OpenCDMProvider& operator=(const OpenCDMProvider&) = delete;

	std::unique_ptr<IOpenCDMSession> constructSession(
		const std::string&              keySystem,
		LicenseType                     licenseType,
		const std::string&              initDataType,
		const uint8_t*                  initData,
		uint32_t                        initDataSize,
		const uint8_t*                  customData,
		uint16_t                        customDataSize,
		const OpenCDMSessionCallbackSet& callbacks) override;

private:
#ifdef USE_THUNDER_OCDM_API_0_2
	struct OpenCDMSystem* m_system;
#else
	struct OpenCDMAccessor* m_system;
#endif
};

#endif // OpenCDMProvider_h
