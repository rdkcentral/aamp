/*
* If not stated otherwise in this file or this component's license file the
* following copyright and licenses apply:
*
* Copyright 2024 RDK Management
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

#include "DrmSessionManager.h"
#include "DrmHelper.h"
#include "MockAampDRMSessionManager.h"
MockDRMSessionManager *g_mockDRMSessionManager = nullptr;

DrmSessionManager::DrmSessionManager(int, void*)
{
}

DrmSessionManager::~DrmSessionManager()
{
}


void DrmSessionManager::setPlaybackSpeedState(bool , double , bool  , double ,int , double , bool )
{
}

void DrmSessionManager::hideWatermarkOnDetach()
{
}

void DrmSessionManager::setVideoMute(bool , double , bool , double ,bool , double )
{
}

void DrmSessionManager::setVideoWindowSize(int width, int height)
{
	if (g_mockDRMSessionManager)
	{
		g_mockDRMSessionManager->setVideoWindowSize(width, height);
	}
}


void DrmSessionManager::UpdateMaxDRMSessions(int)
{
}


DrmSession * DrmSessionManager::createDrmSession(int& err,
		const char* systemId, MediaFormat mediaFormat, const unsigned char * initDataPtr,
		uint16_t initDataLen, int streamType, 
		DrmCallbacks* aamp, void *ptr , const unsigned char* contentMetadataPtr,
		bool isPrimarySession)
		{
			return nullptr;
		}
		
SessionMgrState DrmSessionManager::getSessionMgrState()
{
	return SessionMgrState::eSESSIONMGR_INACTIVE;
}
#if 0
void DrmSessionManager::SetLicenseFetcher(AampLicenseFetcher *fetcherInstance)
{
}

bool DrmSessionManager::QueueContentProtection(DrmHelperPtr drmHelper, std::string periodId, uint32_t adapIdx, AampMediaType type, bool isVssPeriod)
{
	return false;
}

void DrmSessionManager::QueueProtectionEvent(DrmHelperPtr drmHelper, std::string periodId, uint32_t adapIdx, AampMediaType type)
{
}

void DrmSessionManager::clearDrmSession(bool forceClearSession)
{
}

void DrmSessionManager::clearFailedKeyIds()
{
}

void DrmSessionManager::setSessionMgrState(SessionMgrState state)
{
}

void DrmSessionManager::SetSendErrorOnFailure(bool sendErrorOnFailure)
{
}

void DrmSessionManager::SetCommonKeyDuration(int keyDuration)
{
}

void DrmSessionManager::notifyCleanup()
{
}
#endif
