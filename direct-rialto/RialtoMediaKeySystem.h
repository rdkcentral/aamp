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

#ifndef RialtoMediaKeySystem_h
#define RialtoMediaKeySystem_h

/**
 * @file RialtoMediaKeySystem.h
 * @brief Rialto DRM key system — owns IMediaKeys and creates sessions.
 *
 * This is the entry point for Rialto-based DRM. It wraps the Rialto
 * IMediaKeys instance and an inner MediaKeysClient that routes Rialto
 * callbacks to the appropriate session/adapter callbacks.
 */

#include "RialtoMediaKeySession.h"

#include "IMediaKeys.h"
#include "IMediaKeysClient.h"

#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

/**
 * @struct RialtoSessionCallbacks
 * @brief Callback set for Rialto DRM session events.
 *
 * Using std::function allows clean lambda captures without void* userData.
 */
struct RialtoSessionCallbacks
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

	/// Fired when a license renewal message is received.
	std::function<void(const uint8_t* message, size_t size)>
		onLicenseRenewal;
};

/**
 * @class RialtoMediaKeySystem
 * @brief Owns one IMediaKeys instance and creates RialtoMediaKeySessions.
 *
 * The inner MediaKeysClient implements IMediaKeysClient and routes Rialto
 * callbacks into the registered RialtoSessionCallbacks for each session.
 */
class RialtoMediaKeySystem
{
public:
	/**
	 * @brief Construct and create the Rialto IMediaKeys system.
	 *
	 * @param keySystem  DRM key system string (e.g. "com.widevine.alpha").
	 * @param factory    IMediaKeysFactory to use. Defaults to the production
	 *                   factory; inject a mock for testing.
	 */
	explicit RialtoMediaKeySystem(
		const std::string& keySystem,
		std::shared_ptr<firebolt::rialto::IMediaKeysFactory> factory =
			firebolt::rialto::IMediaKeysFactory::createFactory());

	~RialtoMediaKeySystem() = default;

	RialtoMediaKeySystem(const RialtoMediaKeySystem&) = delete;
	RialtoMediaKeySystem& operator=(const RialtoMediaKeySystem&) = delete;

	/**
	 * @brief Returns true if the IMediaKeys system was created successfully.
	 */
	bool isValid() const { return m_mediaKeys != nullptr; }

	/**
	 * @brief Create a new DRM session.
	 *
	 * Calls IMediaKeys::createKeySession() and generateRequest(), registers
	 * the session callbacks for routing.
	 *
	 * @param initDataType  Init data format string ("cenc", "webm", "keyids").
	 * @param initData      PSSH init data blob.
	 * @param initDataSize  Byte length of initData.
	 * @param callbacks     Callback set for challenge and key-update events.
	 *
	 * @return Owning pointer to the session, or nullptr on failure.
	 */
	std::unique_ptr<RialtoMediaKeySession> createSession(
		const std::string& initDataType,
		const uint8_t* initData,
		uint32_t initDataSize,
		const RialtoSessionCallbacks& callbacks);

private:
	/**
	 * @brief Inner IMediaKeysClient that routes events to per-session callbacks.
	 */
	struct MediaKeysClient final : public firebolt::rialto::IMediaKeysClient
	{
		/// Maps Rialto keySessionId → registered callbacks.
		std::map<int32_t, RialtoSessionCallbacks> callbacks;
		/// Maps Rialto keySessionId → the owning RialtoMediaKeySession.
		std::map<int32_t, RialtoMediaKeySession*> sessions;
		std::mutex mutex;

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

#endif // RialtoMediaKeySystem_h
