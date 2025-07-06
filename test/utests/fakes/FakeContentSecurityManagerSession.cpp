/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2025 RDK Management
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
 * limitations under the License.m
*/

/**
 * @file SecureContentSession.cpp
 * @brief Class impl for SecureContentSession
 */

#include "SecureContentSession.h"

std::shared_ptr<SecureContentSession::SessionManager> SecureContentSession::SessionManager::getInstance(int64_t sessionID, std::size_t inputSummaryHash)
{
	std::shared_ptr<SessionManager> returnValue = nullptr;
	return returnValue;
}

SecureContentSession::SessionManager::~SessionManager()
{
}
void SecureContentSession::SessionManager::setInputSummaryHash(std::size_t inputSummaryHash)
{
}


SecureContentSession::SessionManager::SessionManager(int64_t sessionID, std::size_t inputSummaryHash)
{};

SecureContentSession::SecureContentSession(int64_t sessionID, std::size_t inputSummaryHash)
{};

int64_t SecureContentSession::getSessionID(void) const
{
	int64_t ID = PLAYER_SECMGR_INVALID_SESSION_ID;
	return ID;
}

std::size_t SecureContentSession::getInputSummaryHash()
{
	std::size_t hash=0;
	return hash;
}
