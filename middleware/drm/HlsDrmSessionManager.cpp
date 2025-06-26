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
 * @file HlsDrmSessionManager.cpp
 * @brief Operations for HLS DRM 
 */

#include "HlsDrmSessionManager.h"
#include "DrmSessionManager.h"
#include "DrmHelper.h"
#include "HlsOcdmBridge.h"
using namespace std;

/**
 * @brief getInstance Get DRM instance
 *        Get an instance of the Hls DRM Session Manager
 */
HlsDrmSessionManager& HlsDrmSessionManager::getInstance()
{
	static HlsDrmSessionManager instance;
	return instance;
}

/**
 * @brief Check stream is DRM supported
 */
bool HlsDrmSessionManager::isDrmSupported(const struct DrmInfo& drmInfo) const
{
	return DrmHelperEngine::getInstance().hasDRM(drmInfo);
}

/**
 * @brief createSession create session for DRM
 */
std::shared_ptr<HlsDrmBase> HlsDrmSessionManager::createSession(const struct DrmInfo& drmInfo, int  streamTypeIn)
{
	DrmMediaType streamType = (DrmMediaType)streamTypeIn;
	std::shared_ptr<HlsDrmBase> bridge = nullptr;
	DrmHelperPtr drmHelper = DrmHelperEngine::getInstance().createHelper(drmInfo);

	this->GetHlsDrmSessionCb(bridge, drmHelper ,mDrmSession, streamType);
	
	return bridge;
}
