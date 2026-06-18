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

#ifndef RialtoMediaKeySessionAdapter_h
#define RialtoMediaKeySessionAdapter_h

/**
 * @file RialtoMediaKeySessionAdapter.h
 * @brief DrmSession adapter for the Rialto Direct path.
 *
 * Integrates RialtoMediaKeySystem/RialtoMediaKeySession with the
 * DrmSessionManager by implementing the DrmSession interface.
 * Decryption is server-side (no-op). Key management flows through
 * the Rialto IMediaKeys API.
 */

#include "DrmSession.h"
#include "DrmHelper.h"
#include "DrmCallbacks.h"
#include "RialtoMediaKeySystem.h"
#include "RialtoMediaKeySession.h"

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

/**
 * @class RialtoMediaKeySessionAdapter
 * @brief DrmSession implementation for the Rialto Direct DRM path.
 *
 * Follows the same lifecycle as OCDMSessionAdapter:
 *   generateDRMSession → generateKeyRequest → processDRMKey
 *
 * Internally delegates to RialtoMediaKeySystem/RialtoMediaKeySession.
 * decrypt() methods are no-ops — decryption is handled server-side
 * by the Rialto pipeline.
 */
class RialtoMediaKeySessionAdapter : public DrmSession
{
public:
	/**
	 * @brief Construct the adapter.
	 *
	 * @param drmHelper   DRM helper for this session.
	 * @param system      Rialto key system (takes ownership).
	 * @param callbacks   Optional DRM lifecycle callbacks.
	 */
	RialtoMediaKeySessionAdapter(DrmHelperPtr drmHelper,
	                             std::unique_ptr<RialtoMediaKeySystem> system,
	                             DrmCallbacks* callbacks = nullptr);

	~RialtoMediaKeySessionAdapter() override;

	RialtoMediaKeySessionAdapter(const RialtoMediaKeySessionAdapter&) = delete;
	RialtoMediaKeySessionAdapter& operator=(const RialtoMediaKeySessionAdapter&) = delete;

	void generateDRMSession(const uint8_t* f_pbInitData,
	                        uint32_t f_cbInitData,
	                        std::string& customData) override;

	DrmData* generateKeyRequest(string& destinationURL, uint32_t timeout) override;

	int processDRMKey(DrmData* key, uint32_t timeout) override;

	KeyState getState() override;

	bool waitForState(KeyState state, const uint32_t timeout) override;

	void clearDecryptContext() override;

	int decrypt(GstBuffer* keyIDBuffer, GstBuffer* ivBuffer,
	            GstBuffer* buffer, unsigned subSampleCount,
	            GstBuffer* subSamplesBuffer, GstCaps* caps = nullptr) override;

	int decrypt(const uint8_t* f_pbIV, uint32_t f_cbIV,
	            const uint8_t* payloadData, uint32_t payloadDataSize,
	            uint8_t** ppOpaqueData) override;

	int32_t getMediaKeySessionId() const override;

	std::vector<std::vector<uint8_t>> getUsableKeys() const override;

private:
	/// Owned Rialto key system.
	std::unique_ptr<RialtoMediaKeySystem> m_system;

	/// Active session — created in generateDRMSession().
	std::unique_ptr<RialtoMediaKeySession> m_session;

	DrmHelperPtr m_drmHelper;
	DrmCallbacks* m_drmCallbacks;

	std::atomic<KeyState> m_eKeyState;
	std::mutex m_mutex;

	/// Challenge received from Rialto onLicenseRequest callback.
	std::string m_challenge;
	std::string m_destUrl;

	/// Usable key IDs.
	std::vector<std::vector<uint8_t>> m_usableKeys;
	mutable std::mutex m_usableKeysMutex;

	/// Event signalling for challenge/key-ready synchronisation.
	std::mutex m_eventMutex;
	std::condition_variable m_challengeReady;
	std::condition_variable m_keyStatusReady;
	std::condition_variable m_stateChanged;
	bool m_challengeReceived;
	bool m_keyStatusReceived;

	/// Timing for diagnostics.
	long long m_timeBeforeCallback;
};

#endif // RialtoMediaKeySessionAdapter_h
