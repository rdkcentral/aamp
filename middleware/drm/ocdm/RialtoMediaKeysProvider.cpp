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
 * @file RialtoMediaKeysProvider.cpp
 * @brief Concrete IOpenCDM / IOpenCDMSession implementation for the Rialto
 *        Direct path.  All firebolt::rialto::IMediaKeys calls are confined
 *        to this translation unit.
 */

#include "RialtoMediaKeysProvider.h"
#include "PlayerLogManager.h"

#include <algorithm>

// ---------------------------------------------------------------------------
// Helper: map Rialto KeyStatus → OCDM KeyStatus
// ---------------------------------------------------------------------------

namespace
{

KeyStatus toOcdmKeyStatus(firebolt::rialto::KeyStatus s)
{
	switch (s)
	{
	case firebolt::rialto::KeyStatus::USABLE:
		return KeyStatus::Usable;
	case firebolt::rialto::KeyStatus::OUTPUT_RESTRICTED:
		return KeyStatus::OutputRestricted;
	default:
		return KeyStatus::InternalError;
	}
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// RialtoOpenCDMSession
// ---------------------------------------------------------------------------

RialtoOpenCDMSession::RialtoOpenCDMSession(int32_t keySessionId,
                                           firebolt::rialto::IMediaKeys& mediaKeys,
                                           std::function<void(int32_t)> deregister)
	: m_keySessionId(keySessionId)
	, m_mediaKeys(mediaKeys)
	, m_deregister(std::move(deregister))
{
}

KeyStatus RialtoOpenCDMSession::getStatus(const uint8_t* keyId, uint8_t keyIdSize)
{
	std::lock_guard<std::mutex> lock(m_keyStatusMutex);
	const std::vector<uint8_t> key(keyId, keyId + keyIdSize);
	auto it = m_keyStatuses.find(key);
	if (it != m_keyStatuses.end())
	{
		return toOcdmKeyStatus(it->second);
	}
	return KeyStatus::InternalError;
}

OpenCDMError RialtoOpenCDMSession::update(const uint8_t* keyMessage, uint16_t keyMessageLength)
{
	const std::vector<uint8_t> responseData(keyMessage, keyMessage + keyMessageLength);
	firebolt::rialto::MediaKeyErrorStatus status =
		m_mediaKeys.updateSession(m_keySessionId, responseData);
	return (status == firebolt::rialto::MediaKeyErrorStatus::OK)
		? ERROR_NONE : ERROR_FAIL;
}

OpenCDMError RialtoOpenCDMSession::close()
{
	firebolt::rialto::MediaKeyErrorStatus status =
		m_mediaKeys.closeKeySession(m_keySessionId);
	return (status == firebolt::rialto::MediaKeyErrorStatus::OK)
		? ERROR_NONE : ERROR_FAIL;
}

OpenCDMError RialtoOpenCDMSession::destruct()
{
	firebolt::rialto::MediaKeyErrorStatus status =
		m_mediaKeys.releaseKeySession(m_keySessionId);
	// Remove this session's entries from the provider's client maps.
	if (m_deregister)
	{
		m_deregister(m_keySessionId);
	}
	return (status == firebolt::rialto::MediaKeyErrorStatus::OK)
		? ERROR_NONE : ERROR_FAIL;
}

int32_t RialtoOpenCDMSession::getMediaKeySessionId()
{
	return m_keySessionId;
}

bool RialtoOpenCDMSession::hasGstDecryptBuffer()
{
	// Rialto decryption is server-side; GStreamer decrypt is never used.
	return false;
}

OpenCDMError RialtoOpenCDMSession::decrypt(uint8_t*, uint32_t,
                                            EncryptionScheme, EncryptionPattern,
                                            const uint8_t*, uint16_t,
                                            const uint8_t*, uint16_t,
                                            uint32_t)
{
	// No-op stub — decryption is performed server-side by the Rialto pipeline.
	return ERROR_NONE;
}

OpenCDMError RialtoOpenCDMSession::decryptGst(GstBuffer*, GstCaps*)
{
	// No-op stub — decryption is performed server-side by the Rialto pipeline.
	return ERROR_NONE;
}

OpenCDMError RialtoOpenCDMSession::decryptGstLegacy(GstBuffer*, GstBuffer*,
                                                     uint32_t, GstBuffer*,
                                                     GstBuffer*, uint32_t)
{
	// No-op stub — decryption is performed server-side by the Rialto pipeline.
	return ERROR_NONE;
}

void RialtoOpenCDMSession::updateKeyStatus(const std::vector<uint8_t>& keyId,
                                            firebolt::rialto::KeyStatus status)
{
	std::lock_guard<std::mutex> lock(m_keyStatusMutex);
	m_keyStatuses[keyId] = status;
}

// ---------------------------------------------------------------------------
// RialtoMediaKeysProvider::MediaKeysClient
// ---------------------------------------------------------------------------

void RialtoMediaKeysProvider::MediaKeysClient::onLicenseRequest(
	int32_t keySessionId,
	const std::vector<unsigned char>& licenseRequestMessage,
	const std::string& url)
{
	std::lock_guard<std::mutex> lock(mutex);
	auto it = callbacks.find(keySessionId);
	if (it != callbacks.end() && it->second.onChallenge)
	{
		it->second.onChallenge(url.c_str(),
		                       licenseRequestMessage.data(),
		                       static_cast<uint16_t>(licenseRequestMessage.size()));
	}
}

void RialtoMediaKeysProvider::MediaKeysClient::deregisterSession(int32_t keySessionId)
{
	std::lock_guard<std::mutex> lock(mutex);
	callbacks.erase(keySessionId);
	sessions.erase(keySessionId);
	MW_LOG_INFO("RialtoMediaKeysProvider: deregistered session %d", keySessionId);
}

void RialtoMediaKeysProvider::MediaKeysClient::onLicenseRenewal(
	int32_t keySessionId,
	const std::vector<unsigned char>& licenseRenewalMessage)
{
	std::lock_guard<std::mutex> lock(mutex);
	auto it = callbacks.find(keySessionId);
	if (it != callbacks.end() && it->second.onLicenseRenewal)
	{
		it->second.onLicenseRenewal(licenseRenewalMessage.data(),
		                            licenseRenewalMessage.size());
	}
	else
	{
		MW_LOG_WARN("RialtoMediaKeysProvider: onLicenseRenewal for session %d has no handler",
		            keySessionId);
	}
}

void RialtoMediaKeysProvider::MediaKeysClient::onKeyStatusesChanged(
	int32_t keySessionId,
	const firebolt::rialto::KeyStatusVector& keyStatuses)
{
	std::lock_guard<std::mutex> lock(mutex);

	auto cbIt = callbacks.find(keySessionId);
	auto sessIt = sessions.find(keySessionId);

	for (const auto& [keyId, status] : keyStatuses)
	{
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
// RialtoMediaKeysProvider
// ---------------------------------------------------------------------------

RialtoMediaKeysProvider::RialtoMediaKeysProvider(
	const std::string& keySystem,
	std::shared_ptr<firebolt::rialto::IMediaKeysFactory> factory)
	: m_mediaKeys(nullptr)
	, m_client(std::make_shared<MediaKeysClient>())
{
	if (!factory)
	{
		MW_LOG_ERR("RialtoMediaKeysProvider: null IMediaKeysFactory");
		return;
	}

	m_mediaKeys = factory->createMediaKeys(keySystem);
	if (!m_mediaKeys)
	{
		MW_LOG_ERR("RialtoMediaKeysProvider: createMediaKeys(%s) failed", keySystem.c_str());
	}
}

std::unique_ptr<IOpenCDMSession> RialtoMediaKeysProvider::constructSession(
	const std::string&              /*keySystem*/,
	LicenseType                     /*licenseType*/,
	const std::string&              initDataType,
	const uint8_t*                  initData,
	uint32_t                        initDataSize,
	const uint8_t*                  /*customData*/,
	uint16_t                        /*customDataSize*/,
	const OpenCDMSessionCallbackSet& callbacks)
{
	if (!m_mediaKeys)
	{
		MW_LOG_ERR("RialtoMediaKeysProvider::constructSession: no IMediaKeys instance");
		return nullptr;
	}

	int32_t keySessionId = firebolt::rialto::kInvalidSessionId;
	firebolt::rialto::MediaKeyErrorStatus status =
		m_mediaKeys->createKeySession(firebolt::rialto::KeySessionType::TEMPORARY,
		                              std::weak_ptr<firebolt::rialto::IMediaKeysClient>(m_client),
		                              keySessionId);

	if (status != firebolt::rialto::MediaKeyErrorStatus::OK ||
	    keySessionId == firebolt::rialto::kInvalidSessionId)
	{
		MW_LOG_ERR("RialtoMediaKeysProvider::constructSession: createKeySession failed, status=%d",
		           static_cast<int>(status));
		return nullptr;
	}

	MW_LOG_INFO("RialtoMediaKeysProvider::constructSession: keySessionId=%d", keySessionId);

	// Create session object and register it in the client for callback routing.
	// The deregister lambda captures m_client as a weak_ptr so it is safe even
	// if the provider is destroyed before the session's destruct() is called.
	auto deregister = [weakClient = std::weak_ptr<MediaKeysClient>(m_client)](int32_t id)
	{
		if (auto client = weakClient.lock())
		{
			client->deregisterSession(id);
		}
	};
	auto session = std::make_unique<RialtoOpenCDMSession>(
		keySessionId, *m_mediaKeys, std::move(deregister));
	{
		std::lock_guard<std::mutex> lock(m_client->mutex);
		m_client->callbacks[keySessionId] = callbacks;
		m_client->sessions[keySessionId]  = session.get();
	}

	// Map "cenc" init data type string to Rialto InitDataType.
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
		MW_LOG_ERR("RialtoMediaKeysProvider::constructSession: generateRequest failed, status=%d",
		           static_cast<int>(status));
		// Clean up the session registration.
		{
			std::lock_guard<std::mutex> lock(m_client->mutex);
			m_client->callbacks.erase(keySessionId);
			m_client->sessions.erase(keySessionId);
		}
		m_mediaKeys->releaseKeySession(keySessionId);
		return nullptr;
	}

	return session;
}
