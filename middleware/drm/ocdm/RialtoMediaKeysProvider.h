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

#ifndef RialtoMediaKeysProvider_h
#define RialtoMediaKeysProvider_h

/**
 * @file RialtoMediaKeysProvider.h
 * @brief Concrete IOpenCDM / IOpenCDMSession implementation that delegates
 *        to firebolt::rialto::IMediaKeys.
 *
 * Used when eAAMPConfig_useRialtoDirect is enabled.  decrypt() and
 * decryptGst() are no-op stubs — decryption is performed server-side by
 * the Rialto pipeline.
 *
 * TODO: [SHARED-INFRA] This file lives in middleware/drm/ocdm/ (shared AAMP
 *       infrastructure) but depends on Rialto headers.  Build-system changes
 *       in middleware/CMakeLists.txt are needed to add the Rialto include path
 *       and link the Rialto client library when USE_RIALTO_DIRECT is enabled.
 *       Per direct-rialto.instructions.md, this must be reviewed as a
 *       separate, explicit shared-infrastructure change.
 */

#include "IOpenCDM.h"

#include "IMediaKeys.h"
#include "IMediaKeysClient.h"

#include <map>
#include <memory>
#include <mutex>
#include <vector>

/**
 * @class RialtoOpenCDMSession
 * @brief IOpenCDMSession backed by a firebolt::rialto::IMediaKeys session.
 *
 * Holds the Rialto keySessionId and delegates key-management operations to
 * the owning RialtoMediaKeysProvider.  decrypt() and decryptGst() are no-op
 * stubs.
 */
class RialtoOpenCDMSession final : public IOpenCDMSession
{
public:
	/**
	 * @param keySessionId  Session ID assigned by IMediaKeys::createKeySession().
	 * @param mediaKeys     Reference to the owning provider's IMediaKeys instance.
	 *                      Must outlive this object.
	 */
	/**
	 * @param deregister  Callback invoked from destruct() to remove this
	 *                    session's entries from the provider's client maps.
	 *                    Captured as a weak_ptr so it is safe if the provider
	 *                    is destroyed before destruct() is called.
	 */
	RialtoOpenCDMSession(int32_t keySessionId,
	                     firebolt::rialto::IMediaKeys& mediaKeys,
	                     std::function<void(int32_t)> deregister);

	~RialtoOpenCDMSession() override = default;

	RialtoOpenCDMSession(const RialtoOpenCDMSession&) = delete;
	RialtoOpenCDMSession& operator=(const RialtoOpenCDMSession&) = delete;

	/**
	 * @brief Returns USABLE if the key is currently usable, INTERNAL_ERROR otherwise.
	 *
	 * Key status is derived from the last onKeyStatusesChanged callback received
	 * for this session.  The status is stored at the provider level and looked up
	 * here by key ID.
	 */
	KeyStatus getStatus(const uint8_t* keyId, uint8_t keyIdSize) override;

	OpenCDMError update(const uint8_t* keyMessage, uint16_t keyMessageLength) override;
	OpenCDMError close() override;
	OpenCDMError destruct() override;

	/**
	 * @brief Returns the Rialto keySessionId directly.
	 *
	 * No OCDM API call is required — the ID is stored from the
	 * IMediaKeys::createKeySession() return value.
	 */
	int32_t getMediaKeySessionId() override;

	/**
	 * @brief No-op stub — decryption is performed server-side.
	 */
	bool hasGstDecryptBuffer() override;

	/**
	 * @brief No-op stub — decryption is performed server-side by Rialto.
	 */
	OpenCDMError decrypt(uint8_t* data,
	                     uint32_t dataSize,
	                     EncryptionScheme encScheme,
	                     EncryptionPattern pattern,
	                     const uint8_t* iv,
	                     uint16_t ivSize,
	                     const uint8_t* keyId,
	                     uint16_t keyIdSize,
	                     uint32_t initWithLast15) override;

	/**
	 * @brief No-op stub — decryption is performed server-side by Rialto.
	 */
	OpenCDMError decryptGst(GstBuffer* buffer, GstCaps* caps) override;

	/**
	 * @brief No-op stub — decryption is performed server-side by Rialto.
	 */
	OpenCDMError decryptGstLegacy(GstBuffer* buffer,
	                               GstBuffer* subSamples,
	                               uint32_t subSampleCount,
	                               GstBuffer* iv,
	                               GstBuffer* keyId,
	                               uint32_t initWithLast15) override;

	/**
	 * @brief Update the stored key status for a given key ID.
	 *
	 * Called by RialtoMediaKeysProvider::MediaKeysClient when
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

/**
 * @class RialtoMediaKeysProvider
 * @brief IOpenCDM implementation that delegates to firebolt::rialto::IMediaKeys.
 *
 * Owns one IMediaKeys instance for the lifetime of the provider.  An inner
 * MediaKeysClient object (shared_ptr) implements IMediaKeysClient and routes
 * Rialto callbacks into the registered OpenCDMSessionCallbackSet functions.
 */
class RialtoMediaKeysProvider final : public IOpenCDM
{
public:
	/**
	 * @brief Construct and create the Rialto IMediaKeys system.
	 *
	 * @param keySystem  DRM key system string (e.g. Widevine UUID).
	 * @param factory    IMediaKeysFactory to use.  Defaults to
	 *                   IMediaKeysFactory::createFactory() for production;
	 *                   inject a mock in tests.
	 */
	explicit RialtoMediaKeysProvider(
		const std::string& keySystem,
		std::shared_ptr<firebolt::rialto::IMediaKeysFactory> factory =
			firebolt::rialto::IMediaKeysFactory::createFactory());

	~RialtoMediaKeysProvider() override = default;

	RialtoMediaKeysProvider(const RialtoMediaKeysProvider&) = delete;
	RialtoMediaKeysProvider& operator=(const RialtoMediaKeysProvider&) = delete;

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
	/**
	 * @brief Inner IMediaKeysClient that routes events to the per-session
	 *        callback sets registered in m_sessionCallbacks.
	 *
	 * Held as a shared_ptr so it can be passed as a weak_ptr to
	 * IMediaKeys::createKeySession().
	 */
	struct MediaKeysClient final : public firebolt::rialto::IMediaKeysClient
	{
		/// Maps Rialto keySessionId → callbacks registered at constructSession.
		std::map<int32_t, OpenCDMSessionCallbackSet> callbacks;
		/// Maps Rialto keySessionId → the owning RialtoOpenCDMSession.
		std::map<int32_t, RialtoOpenCDMSession*> sessions;
		std::mutex mutex;

		/// Remove all entries for keySessionId from both maps.
		void deregisterSession(int32_t keySessionId);

		void onLicenseRequest(int32_t keySessionId,
		                      const std::vector<unsigned char>& licenseRequestMessage,
		                      const std::string& url) override;

		void onLicenseRenewal(int32_t keySessionId,
		                      const std::vector<unsigned char>& licenseRenewalMessage) override;

		void onKeyStatusesChanged(int32_t keySessionId,
		                          const firebolt::rialto::KeyStatusVector& keyStatuses) override;
	};

	std::unique_ptr<firebolt::rialto::IMediaKeys> m_mediaKeys;
	std::shared_ptr<MediaKeysClient>              m_client;
};

#endif // RialtoMediaKeysProvider_h
