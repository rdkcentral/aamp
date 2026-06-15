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
 * @file RialtoMediaKeySystem.cpp
 * @brief Implementation of RialtoMediaKeySystem and inner MediaKeysClient.
 */

#include "RialtoMediaKeySystem.h"
#include "PlayerLogManager.h"

// ---------------------------------------------------------------------------
// RialtoMediaKeySystem::MediaKeysClient
// ---------------------------------------------------------------------------

void RialtoMediaKeySystem::MediaKeysClient::deregisterSession(int32_t keySessionId)
{
	std::lock_guard<std::mutex> lock(mutex);
	callbacks.erase(keySessionId);
	sessions.erase(keySessionId);
	MW_LOG_INFO("RialtoMediaKeySystem::MediaKeysClient: deregistered session %d", keySessionId);
}

void RialtoMediaKeySystem::MediaKeysClient::onLicenseRequest(
	int32_t keySessionId,
	const std::vector<unsigned char>& licenseRequestMessage,
	const std::string& url)
{
	MW_LOG_INFO("RialtoMediaKeySystem::MediaKeysClient: onLicenseRequest session=%d url=%s size=%zu",
	            keySessionId, url.c_str(), licenseRequestMessage.size());

	std::lock_guard<std::mutex> lock(mutex);
	auto it = callbacks.find(keySessionId);
	if (it != callbacks.end() && it->second.onChallenge)
	{
		it->second.onChallenge(url.c_str(),
		                       licenseRequestMessage.data(),
		                       static_cast<uint16_t>(licenseRequestMessage.size()));
	}
	else
	{
		MW_LOG_WARN("RialtoMediaKeySystem::MediaKeysClient: onLicenseRequest for session %d has no handler",
		            keySessionId);
	}
}

void RialtoMediaKeySystem::MediaKeysClient::onLicenseRenewal(
	int32_t keySessionId,
	const std::vector<unsigned char>& licenseRenewalMessage)
{
	MW_LOG_INFO("RialtoMediaKeySystem::MediaKeysClient: onLicenseRenewal session=%d size=%zu",
	            keySessionId, licenseRenewalMessage.size());

	std::lock_guard<std::mutex> lock(mutex);
	auto it = callbacks.find(keySessionId);
	if (it != callbacks.end() && it->second.onLicenseRenewal)
	{
		it->second.onLicenseRenewal(licenseRenewalMessage.data(),
		                            licenseRenewalMessage.size());
	}
	else
	{
		MW_LOG_WARN("RialtoMediaKeySystem::MediaKeysClient: onLicenseRenewal for session %d has no handler",
		            keySessionId);
	}
}

void RialtoMediaKeySystem::MediaKeysClient::onKeyStatusesChanged(
	int32_t keySessionId,
	const firebolt::rialto::KeyStatusVector& keyStatuses)
{
	MW_LOG_INFO("RialtoMediaKeySystem::MediaKeysClient: onKeyStatusesChanged session=%d count=%zu",
	            keySessionId, keyStatuses.size());

	std::lock_guard<std::mutex> lock(mutex);

	auto cbIt = callbacks.find(keySessionId);
	auto sessIt = sessions.find(keySessionId);

	for (const auto& [keyId, status] : keyStatuses)
	{
		if (PlayerLogManager::isLogLevelAllowed(mLOGLEVEL_TRACE))
		{
			MW_LOG_TRACE("RialtoMediaKeySystem::MediaKeysClient: "
			             "key status=%d keyIdSize=%zu",
			             static_cast<int>(status), keyId.size());
			DumpBinaryBlob(keyId.data(), keyId.size());
		}

		// Update the session's own key status cache.
		if (sessIt != sessions.end())
		{
			sessIt->second->updateKeyStatus(keyId, status);
		}

		// Fire the per-key update callback.
		if (cbIt != callbacks.end() && cbIt->second.onKeyUpdate)
		{
			cbIt->second.onKeyUpdate(keyId.data(),
			                         static_cast<uint8_t>(keyId.size()));
		}
	}

	// Signal that all key statuses for this update have been delivered.
	if (cbIt != callbacks.end() && cbIt->second.onKeysUpdated)
	{
		cbIt->second.onKeysUpdated();
	}
}

// ---------------------------------------------------------------------------
// RialtoMediaKeySystem
// ---------------------------------------------------------------------------

RialtoMediaKeySystem::RialtoMediaKeySystem(
	const std::string& keySystem,
	std::shared_ptr<firebolt::rialto::IMediaKeysFactory> factory)
	: m_mediaKeys(nullptr)
	, m_client(std::make_shared<MediaKeysClient>())
{
	MW_LOG_INFO("RialtoMediaKeySystem: creating for keySystem=%s", keySystem.c_str());

	if (!factory)
	{
		MW_LOG_ERR("RialtoMediaKeySystem: null IMediaKeysFactory");
		return;
	}

	m_mediaKeys = factory->createMediaKeys(keySystem);
	if (!m_mediaKeys)
	{
		MW_LOG_ERR("RialtoMediaKeySystem: createMediaKeys(%s) failed", keySystem.c_str());
	}
	else
	{
		MW_LOG_INFO("RialtoMediaKeySystem: IMediaKeys created successfully for %s", keySystem.c_str());
	}
}

std::unique_ptr<RialtoMediaKeySession> RialtoMediaKeySystem::createSession(
	const std::string& initDataType,
	const uint8_t* initData,
	uint32_t initDataSize,
	const RialtoSessionCallbacks& callbacks)
{
	if (!m_mediaKeys)
	{
		MW_LOG_ERR("RialtoMediaKeySystem::createSession: no IMediaKeys instance");
		return nullptr;
	}

	MW_LOG_INFO("RialtoMediaKeySystem::createSession: initDataType=%s initDataSize=%u",
	            initDataType.c_str(), initDataSize);

	int32_t keySessionId = firebolt::rialto::kInvalidSessionId;
	firebolt::rialto::MediaKeyErrorStatus status =
		m_mediaKeys->createKeySession(firebolt::rialto::KeySessionType::TEMPORARY,
		                              std::weak_ptr<firebolt::rialto::IMediaKeysClient>(m_client),
		                              keySessionId);

	if (status != firebolt::rialto::MediaKeyErrorStatus::OK ||
	    keySessionId == firebolt::rialto::kInvalidSessionId)
	{
		MW_LOG_ERR("RialtoMediaKeySystem::createSession: createKeySession failed, status=%d",
		           static_cast<int>(status));
		return nullptr;
	}

	MW_LOG_INFO("RialtoMediaKeySystem::createSession: keySessionId=%d", keySessionId);

	// Create session object with a deregister lambda that safely removes
	// it from the client maps. Using weak_ptr ensures safety if the system
	// is destroyed before the session's destruct() is called.
	auto deregister = [weakClient = std::weak_ptr<MediaKeysClient>(m_client)](int32_t id)
	{
		if (auto client = weakClient.lock())
		{
			client->deregisterSession(id);
		}
	};

	auto session = std::make_unique<RialtoMediaKeySession>(
		keySessionId, *m_mediaKeys, std::move(deregister));

	// Register in client maps for callback routing.
	{
		std::lock_guard<std::mutex> lock(m_client->mutex);
		m_client->callbacks[keySessionId] = callbacks;
		m_client->sessions[keySessionId]  = session.get();
	}

	// Map init data type string to Rialto InitDataType enum.
	firebolt::rialto::InitDataType rialtoInitDataType = firebolt::rialto::InitDataType::CENC;
	if (initDataType == "webm")
	{
		rialtoInitDataType = firebolt::rialto::InitDataType::WEBM;
	}
	else if (initDataType == "keyids")
	{
		rialtoInitDataType = firebolt::rialto::InitDataType::KEY_IDS;
	}

	const std::vector<uint8_t> initDataVec(initData, initData + initDataSize);
	status = m_mediaKeys->generateRequest(keySessionId, rialtoInitDataType, initDataVec);
	if (status != firebolt::rialto::MediaKeyErrorStatus::OK)
	{
		MW_LOG_ERR("RialtoMediaKeySystem::createSession: generateRequest failed, status=%d keySessionId=%d",
		           static_cast<int>(status), keySessionId);
		// Clean up the session since generateRequest failed.
		m_client->deregisterSession(keySessionId);
		m_mediaKeys->releaseKeySession(keySessionId);
		return nullptr;
	}

	MW_LOG_INFO("RialtoMediaKeySystem::createSession: generateRequest succeeded, keySessionId=%d",
	            keySessionId);
	return session;
}
