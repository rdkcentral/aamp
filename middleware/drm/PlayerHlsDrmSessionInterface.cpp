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
 * limitations under the License.
*/


/**
 * @file PlayerHlsDrmSessionInterface.cpp
 * @brief Interface for HLS DRM 
 */

#include "PlayerHlsDrmSessionInterface.h"
#ifdef USE_OPENCDM_ADAPTER
#include "HlsDrmSessionManager.h"
#endif

PlayerHlsDrmSessionInterface* pInstance = nullptr;

/**
 * @brief Constructor
 */
PlayerHlsDrmSessionInterface::PlayerHlsDrmSessionInterface()
{
#ifdef USE_OPENCDM_ADAPTER
	m_pHlsDrmSessionManager = &HlsDrmSessionManager::getInstance();
#else
	m_pHlsDrmSessionManager = new FakeHlsDrmSessionManager();
#endif
}

/**
 * @brief getInstance Get DRM instance
 *        Get an instance of the Hls DRM Session Manager
 */
PlayerHlsDrmSessionInterface* PlayerHlsDrmSessionInterface::getInstance()
{
	if(pInstance == nullptr)
	{
		pInstance = new PlayerHlsDrmSessionInterface();
	}
	return pInstance;
}

/**
 * @brief Check stream is DRM supported
 */
bool PlayerHlsDrmSessionInterface::isDrmSupported(const struct DrmInfo& drmInfo) const
{
	return m_pHlsDrmSessionManager->isDrmSupported(drmInfo);
}

/**
 * @brief createSession create session for DRM
 */
std::shared_ptr<HlsDrmBase> PlayerHlsDrmSessionInterface::createSession(const struct DrmInfo& drmInfo, int  streamTypeIn)
{
	return m_pHlsDrmSessionManager->createSession(drmInfo, streamTypeIn);
}

/**
 * @brief Registers GetAccessKey callback from application
 */
void PlayerHlsDrmSessionInterface::RegisterGetHlsDrmSessionCb(const GetHlsDrmSessionCallback Callback)
{
	return m_pHlsDrmSessionManager->RegisterGetHlsDrmSessionCb(Callback);
}
