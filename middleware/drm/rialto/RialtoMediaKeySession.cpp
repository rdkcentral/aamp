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
 * @file RialtoMediaKeySession.cpp
 * @brief Implementation of RialtoMediaKeySession.
 */

#include "RialtoMediaKeySession.h"
#include "PlayerLogManager.h"

RialtoMediaKeySession::RialtoMediaKeySession(
	int32_t keySessionId,
	firebolt::rialto::IMediaKeys& mediaKeys,
	std::function<void(int32_t)> deregister)
	: m_keySessionId(keySessionId)
	, m_mediaKeys(mediaKeys)
	, m_deregister(std::move(deregister))
{
	MW_LOG_INFO("RialtoMediaKeySession[%d]: created", m_keySessionId);
}

bool RialtoMediaKeySession::update(const uint8_t* keyMessage, uint16_t keyMessageLength)
{
	MW_LOG_INFO("RialtoMediaKeySession[%d]: update, length=%u", m_keySessionId, keyMessageLength);

	const std::vector<uint8_t> responseData(keyMessage, keyMessage + keyMessageLength);
	firebolt::rialto::MediaKeyErrorStatus status =
		m_mediaKeys.updateSession(m_keySessionId, responseData);

	if (status != firebolt::rialto::MediaKeyErrorStatus::OK)
	{
		MW_LOG_ERR("RialtoMediaKeySession[%d]: updateSession failed, status=%d",
		           m_keySessionId, static_cast<int>(status));
		return false;
	}

	MW_LOG_INFO("RialtoMediaKeySession[%d]: updateSession succeeded", m_keySessionId);
	return true;
}

bool RialtoMediaKeySession::close()
{
	MW_LOG_INFO("RialtoMediaKeySession[%d]: close", m_keySessionId);

	firebolt::rialto::MediaKeyErrorStatus status =
		m_mediaKeys.closeKeySession(m_keySessionId);

	if (status != firebolt::rialto::MediaKeyErrorStatus::OK)
	{
		MW_LOG_ERR("RialtoMediaKeySession[%d]: closeKeySession failed, status=%d",
		           m_keySessionId, static_cast<int>(status));
		return false;
	}

	MW_LOG_INFO("RialtoMediaKeySession[%d]: closeKeySession succeeded", m_keySessionId);
	return true;
}

bool RialtoMediaKeySession::destruct()
{
	MW_LOG_INFO("RialtoMediaKeySession[%d]: destruct", m_keySessionId);

	firebolt::rialto::MediaKeyErrorStatus status =
		m_mediaKeys.releaseKeySession(m_keySessionId);

	if (status != firebolt::rialto::MediaKeyErrorStatus::OK)
	{
		MW_LOG_ERR("RialtoMediaKeySession[%d]: releaseKeySession failed, status=%d",
		           m_keySessionId, static_cast<int>(status));
	}

	if (m_deregister)
	{
		m_deregister(m_keySessionId);
	}

	MW_LOG_INFO("RialtoMediaKeySession[%d]: destruct complete", m_keySessionId);
	return (status == firebolt::rialto::MediaKeyErrorStatus::OK);
}

bool RialtoMediaKeySession::isKeyUsable(const uint8_t* keyId, uint8_t keyIdSize) const
{
	std::lock_guard<std::mutex> lock(m_keyStatusMutex);
	const std::vector<uint8_t> key(keyId, keyId + keyIdSize);
	auto it = m_keyStatuses.find(key);
	if (it != m_keyStatuses.end())
	{
		return (it->second == firebolt::rialto::KeyStatus::USABLE);
	}
	MW_LOG_WARN("RialtoMediaKeySession[%d]: isKeyUsable - key not found", m_keySessionId);
	return false;
}

bool RialtoMediaKeySession::isKeyOutputRestricted(const uint8_t* keyId, uint8_t keyIdSize) const
{
	std::lock_guard<std::mutex> lock(m_keyStatusMutex);
	const std::vector<uint8_t> key(keyId, keyId + keyIdSize);
	auto it = m_keyStatuses.find(key);
	if (it != m_keyStatuses.end())
	{
		return (it->second == firebolt::rialto::KeyStatus::OUTPUT_RESTRICTED);
	}
	return false;
}

void RialtoMediaKeySession::updateKeyStatus(const std::vector<uint8_t>& keyId,
                                            firebolt::rialto::KeyStatus status)
{
	std::lock_guard<std::mutex> lock(m_keyStatusMutex);
	m_keyStatuses[keyId] = status;
	MW_LOG_INFO("RialtoMediaKeySession[%d]: key status updated to %d",
	            m_keySessionId, static_cast<int>(status));
}
