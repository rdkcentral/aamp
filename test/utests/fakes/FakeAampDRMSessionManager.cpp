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

#include "DRMSessionManager.h"
#include "DrmHelper.h"
#include "MockAampDRMSessionManager.h"
MockDRMSessionManager *g_mockDRMSessionManager = nullptr;

DRMSessionManager::DRMSessionManager(int, void*)
{
}

DRMSessionManager::~DRMSessionManager()
{
}


void DRMSessionManager::setPlaybackSpeedState(bool , double , bool  , double ,int , double , bool )
{
}

void DRMSessionManager::hideWatermarkOnDetach()
{
}

void DRMSessionManager::setVideoMute(bool , double , bool , double ,bool , double )
{
}

void DRMSessionManager::setVideoWindowSize(int width, int height)
{
	if (g_mockDRMSessionManager)
	{
		g_mockDRMSessionManager->setVideoWindowSize(width, height);
	}
}


void DRMSessionManager::UpdateMaxDRMSessions(int)
{
}


DrmSession * DRMSessionManager::createDrmSession(int& err,
		const char* systemId, MediaFormat mediaFormat, const unsigned char * initDataPtr,
		uint16_t initDataLen, int streamType, 
		DrmCallbacks* aamp, void *ptr , const unsigned char* contentMetadataPtr,
		bool isPrimarySession)
		{
			return nullptr;
		}
		
SessionMgrState DRMSessionManager::getSessionMgrState()
{
	return SessionMgrState::eSESSIONMGR_INACTIVE;
}
#if 0
void DRMSessionManager::SetLicenseFetcher(AampLicenseFetcher *fetcherInstance)
{
}

bool DRMSessionManager::QueueContentProtection(DrmHelperPtr drmHelper, std::string periodId, uint32_t adapIdx, AampMediaType type, bool isVssPeriod)
{
	return false;
}

void DRMSessionManager::QueueProtectionEvent(DrmHelperPtr drmHelper, std::string periodId, uint32_t adapIdx, AampMediaType type)
{
}

void DRMSessionManager::clearDrmSession(bool forceClearSession)
{
}

void DRMSessionManager::clearFailedKeyIds()
{
}

void DRMSessionManager::setSessionMgrState(SessionMgrState state)
{
}

void DRMSessionManager::SetSendErrorOnFailure(bool sendErrorOnFailure)
{
}

void DRMSessionManager::SetCommonKeyDuration(int keyDuration)
{
}

void DRMSessionManager::notifyCleanup()
{
}
#endif
