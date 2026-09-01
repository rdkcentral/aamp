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
 * @file RialtoMediaKeySessionAdapter.cpp
 * @brief DrmSession adapter implementation for the Rialto Direct path.
 */

#include "RialtoMediaKeySessionAdapter.h"
#include "AampLogManager.h"
#include "AampUtils.h"

#include <algorithm>
#include <chrono>
#include <set>

#define LICENSE_RENEWAL_MESSAGE_TYPE "1"

RialtoMediaKeySessionAdapter::RialtoMediaKeySessionAdapter(
	DrmHelperPtr drmHelper,
	std::unique_ptr<RialtoMediaKeySystem> system,
	DrmCallbacks* callbacks) :
	DrmSession(drmHelper->ocdmSystemId())
	, m_system(move(system))
	, m_session(nullptr)
	, m_drmHelper(drmHelper)
	, m_drmCallbacks(callbacks)
	, m_eKeyState(KEY_INIT)
	, m_mutex()
	, m_challenge()
	, m_destUrl()
	, m_usableKeys()
	, m_usableKeysMutex()
	, m_eventMutex()
	, m_challengeReady()
	, m_keyStatusReady()
	, m_stateChanged()
	, m_challengeReceived(false)
	, m_keyStatusReceived(false)
	, m_timeBeforeCallback(0)
{
	AAMPLOG_INFO("RialtoMediaKeySessionAdapter: created for keySystem=%s",
	            m_keySystem.c_str());
}

RialtoMediaKeySessionAdapter::~RialtoMediaKeySessionAdapter()
{
	AAMPLOG_INFO("RialtoMediaKeySessionAdapter: destructor keySystem=%s", m_keySystem.c_str());
	clearDecryptContext();
}

void RialtoMediaKeySessionAdapter::generateDRMSession(
	const uint8_t* f_pbInitData,
	uint32_t f_cbInitData,
	std::string& customData)
{
	AAMPLOG_INFO("RialtoMediaKeySessionAdapter::generateDRMSession initDataSize=%u", f_cbInitData);

	std::lock_guard<std::mutex> guard(m_mutex);

	if (!m_system || !m_system->isValid())
	{
		AAMPLOG_ERR("RialtoMediaKeySessionAdapter::generateDRMSession: no valid RialtoMediaKeySystem");
		m_eKeyState = KEY_ERROR;
		m_stateChanged.notify_all();
		return;
	}

	m_timeBeforeCallback = aamp_GetCurrentTimeMS();

	// Wire callbacks to route Rialto events into this adapter.
	RialtoSessionCallbacks callbacks;

	callbacks.onChallenge = [this](const char* destUrl,
	                               const uint8_t* challenge,
	                               uint16_t challengeSize)
	{
		long long elapsed = aamp_GetCurrentTimeMS() - m_timeBeforeCallback;
		AAMPLOG_INFO("RialtoMediaKeySessionAdapter: onChallenge received, elapsed=%lld ms size=%u destUrl=%s",
		            elapsed, challengeSize, destUrl);

		const std::string challengeData(reinterpret_cast<const char*>(challenge), challengeSize);
		const std::string delimiter(":Type:");
		const size_t delimiterPos = challengeData.find(delimiter);
		const std::string messageType = challengeData.substr(0, delimiterPos);
		const std::set<std::string> individualisationTypes = {"individualization-request", "3"};

		if ((delimiterPos != std::string::npos) && (individualisationTypes.count(messageType) > 0))
		{
			AAMPLOG_INFO("RialtoMediaKeySessionAdapter: onChallenge individualisation type=%s",
			            messageType.c_str());
			if (m_drmCallbacks)
			{
				m_drmCallbacks->Individualization(
					challengeData.substr(delimiterPos + delimiter.length()));
			}
			return;
		}

		std::lock_guard<std::mutex> lock(m_eventMutex);
		m_challenge.assign(challengeData);
		m_destUrl.assign(destUrl);
		m_challengeReceived = true;
		m_challengeReady.notify_all();
	};

	callbacks.onKeyUpdate = [this](const uint8_t* key, uint8_t keySize)
	{
		AAMPLOG_INFO("RialtoMediaKeySessionAdapter: onKeyUpdate keySize=%u", keySize);

		if (key != nullptr && keySize > 0)
		{
			if (AampLogManager::isLogLevelAllowed(eLOGLEVEL_TRACE))
			{
				AAMPLOG_TRACE("RialtoMediaKeySessionAdapter: onKeyUpdate keyId:");
				DumpBlob(key, keySize);
			}
			std::vector<uint8_t> keyData(key, key + keySize);
			{
				std::lock_guard<std::mutex> lock(m_usableKeysMutex);
				if (find(m_usableKeys.begin(), m_usableKeys.end(), keyData) == m_usableKeys.end())
				{
					AAMPLOG_TRACE("RialtoMediaKeySessionAdapter: new usable key added");
					m_usableKeys.push_back(keyData);
				}
			}
		}
	};

	callbacks.onKeysUpdated = [this]()
	{
		AAMPLOG_INFO("RialtoMediaKeySessionAdapter: onKeysUpdated");

		std::lock_guard<std::mutex> lock(m_eventMutex);
		m_keyStatusReceived = true;
		m_keyStatusReady.notify_all();
	};

	callbacks.onLicenseRenewal = [this](const uint8_t* /*message*/, size_t /*size*/)
	{
		AAMPLOG_INFO("RialtoMediaKeySessionAdapter: onLicenseRenewal");
		if (m_drmCallbacks)
		{
			m_drmCallbacks->LicenseRenewal(m_drmHelper, static_cast<DrmSession*>(this));
		}
	};

	m_session = m_system->createSession("cenc", f_pbInitData, f_cbInitData, callbacks);
	if (!m_session)
	{
		AAMPLOG_ERR("createSession failed");
		m_eKeyState = KEY_ERROR_SESSION_CREATE_FAILED;
		m_stateChanged.notify_all();
	}
	else
	{
		AAMPLOG_INFO("session created, keySessionId=%d", m_session->getMediaKeySessionId());
	}
}

DrmData* RialtoMediaKeySessionAdapter::generateKeyRequest(
	std::string& destinationURL, uint32_t timeout)
{
	DrmData* result = nullptr;
	m_eKeyState = KEY_ERROR;

	{
		std::unique_lock<std::mutex> lock(m_eventMutex);
		if (!m_challengeReceived)
		{
			AAMPLOG_INFO("blocking wait for challenge timeout=%u ms", timeout);
			auto waitResult = m_challengeReady.wait_for(
				lock, std::chrono::milliseconds(timeout),
				[this]() { return m_challengeReceived; });
			if (!waitResult)
			{
				AAMPLOG_WARN("timed out waiting for challenge");
				return nullptr;
			}
			else
			{
				AAMPLOG_INFO("challenge received");
			}
		}
	}

	std::string challenge;
	std::string destUrl;
	{
		std::lock_guard<std::mutex> lock(m_eventMutex);
		challenge = m_challenge;
		destUrl = m_destUrl;
	}

	if (!challenge.empty())
	{
		// Strip type prefix if present (same format as OCDM: "Type:payload")
		const std::string delimiter(":Type:");
		std::string challengeData = challenge;
		std::string requestType(challengeData.substr(0, challengeData.find(delimiter)));
		if (!requestType.empty() && requestType.size() != challengeData.size())
		{
			challengeData.erase(0, challengeData.find(delimiter) + delimiter.length());
		}

		result = new DrmData(challengeData.c_str(), challengeData.length());
		destinationURL.assign(destUrl);
		AAMPLOG_INFO("RialtoMediaKeySessionAdapter::generateKeyRequest: destUrl=%s",
		            destinationURL.c_str());
		m_eKeyState = KEY_PENDING;
	}
	else
	{
		AAMPLOG_WARN("RialtoMediaKeySessionAdapter::generateKeyRequest: empty challenge");
	}

	return result;
}

int RialtoMediaKeySessionAdapter::processDRMKey(DrmData* key, uint32_t timeout)
{
	AAMPLOG_INFO("RialtoMediaKeySessionAdapter::processDRMKey timeout=%u", timeout);

	int retValue = -1;

	if (!m_session)
	{
		AAMPLOG_ERR("RialtoMediaKeySessionAdapter::processDRMKey: no active session");
		m_eKeyState = KEY_ERROR;
		m_stateChanged.notify_all();
		return retValue;
	}

	if (key)
	{
		const uint8_t* keyMessage = reinterpret_cast<const uint8_t*>(key->getData().c_str());
		const uint16_t keyMsgLength = static_cast<uint16_t>(key->getDataLength());

		AAMPLOG_INFO("RialtoMediaKeySessionAdapter::processDRMKey: calling update, length=%u",
		            keyMsgLength);

		if (!m_session->update(keyMessage, keyMsgLength))
		{
			AAMPLOG_ERR("RialtoMediaKeySessionAdapter::processDRMKey: update failed");
			m_eKeyState = KEY_ERROR;
			m_stateChanged.notify_all();
			return retValue;
		}
	}
	else
	{
		AAMPLOG_INFO("RialtoMediaKeySessionAdapter::processDRMKey: NULL key, assuming external acquisition");
	}

	// Wait for key status callback
	{
		std::unique_lock<std::mutex> lock(m_eventMutex);
		if (!m_keyStatusReceived)
		{
			AAMPLOG_INFO("blocking wait for key status timeout=%u ms", timeout);
			auto waitResult = m_keyStatusReady.wait_for(
				lock, std::chrono::milliseconds(timeout),
				[this]() { return m_keyStatusReceived; });
			if (!waitResult)
			{
				AAMPLOG_WARN("timed out waiting for key status");
			}
			else
			{
				AAMPLOG_INFO("key status received");
			}
		}
	}

	// Check if the key is usable by querying the session's cached status.
	// Use the key from usableKeys if available.
	bool keyUsable = false;
	{
		std::lock_guard<std::mutex> lock(m_usableKeysMutex);
		for (const auto& keyData : m_usableKeys)
		{
			if (m_session->isKeyUsable(keyData.data(), static_cast<uint8_t>(keyData.size())))
			{
				keyUsable = true;
				break;
			}
			if (m_session->isKeyOutputRestricted(keyData.data(), static_cast<uint8_t>(keyData.size())))
			{
				AAMPLOG_WARN("RialtoMediaKeySessionAdapter::processDRMKey: output restricted");
				m_eKeyState = KEY_ERROR;
				retValue = HDCP_OUTPUT_PROTECTION_FAILURE;
				m_stateChanged.notify_all();
				return retValue;
			}
		}
	}

	if (keyUsable)
	{
		AAMPLOG_INFO("RialtoMediaKeySessionAdapter::processDRMKey: key usable");
		m_eKeyState = KEY_READY;
		retValue = 0;
	}
	else
	{
		AAMPLOG_WARN("RialtoMediaKeySessionAdapter::processDRMKey: key not usable");
		m_eKeyState = KEY_ERROR;
	}

	m_stateChanged.notify_all();
	return retValue;
}

KeyState RialtoMediaKeySessionAdapter::getState()
{
	return m_eKeyState;
}

bool RialtoMediaKeySessionAdapter::waitForState(KeyState state, const uint32_t timeout)
{
	if (m_eKeyState == state)
	{
		return true;
	}

	std::unique_lock<std::mutex> lock(m_eventMutex);
	auto waitResult = m_stateChanged.wait_for(
		lock, std::chrono::milliseconds(timeout),
		[this, state]() { return m_eKeyState == state; });

	return waitResult;
}

void RialtoMediaKeySessionAdapter::clearDecryptContext()
{
	AAMPLOG_INFO("RialtoMediaKeySessionAdapter::clearDecryptContext");

	std::lock_guard<std::mutex> guard(m_mutex);

	if (m_session)
	{
		m_session->close();
		m_session->destruct();
		m_session.reset();
	}

	{
		std::lock_guard<std::mutex> keyLock(m_usableKeysMutex);
		m_usableKeys.clear();
	}

	{
		std::lock_guard<std::mutex> lock(m_eventMutex);
		m_challengeReceived = false;
		m_keyStatusReceived = false;
	}

	m_eKeyState = KEY_INIT;
	m_stateChanged.notify_all();
}

int32_t RialtoMediaKeySessionAdapter::getMediaKeySessionId() const
{
	if (m_session)
	{
		return m_session->getMediaKeySessionId();
	}
	return -1;
}

std::vector<std::vector<uint8_t>> RialtoMediaKeySessionAdapter::getUsableKeys() const
{
	std::lock_guard<std::mutex> lock(m_usableKeysMutex);
	return m_usableKeys;
}
