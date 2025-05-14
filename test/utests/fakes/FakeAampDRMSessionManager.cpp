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

#include "DrmHelper.h"
#include "AampDRMSessionManager.h"
#include "MockAampDRMSessionManager.h"

MockAampDRMSessionManager *g_mockAampDRMSessionManager = nullptr;

AampDRMSessionManager::AampDRMSessionManager(int, PrivateInstanceAAMP*)
{
}

AampDRMSessionManager::~AampDRMSessionManager()
{
}

void AampDRMSessionManager::renewLicense(DrmHelperPtr, void*, PrivateInstanceAAMP*)
{
}

void AampDRMSessionManager::setPlaybackSpeedState(int, double, bool)
{
}

void AampDRMSessionManager::hideWatermarkOnDetach()
{
}

void AampDRMSessionManager::setVideoMute(bool, double)
{
}

void AampDRMSessionManager::setVideoWindowSize(int width, int height)
{
	if (g_mockAampDRMSessionManager)
	{
		g_mockAampDRMSessionManager->setVideoWindowSize(width, height);
	}
}

void AampDRMSessionManager::Stop()
{
}

void AampDRMSessionManager::UpdateMaxDRMSessions(int)
{
}

void AampDRMSessionManager::setLicenseRequestAbort(bool)
{
}

DrmSession * AampDRMSessionManager::createDrmSession(
		const char* systemId, MediaFormat mediaFormat, const unsigned char * initDataPtr,
		uint16_t initDataLen, AampMediaType streamType,
		PrivateInstanceAAMP* aamp, DrmMetaDataEventPtr e, const unsigned char* contentMetadataPtr,
		bool isPrimarySession)
		{
			return nullptr;
		}
		
SessionMgrState AampDRMSessionManager::getSessionMgrState()
{
	return SessionMgrState::eSESSIONMGR_INACTIVE;
}

void AampDRMSessionManager::SetLicenseFetcher(AampLicenseFetcher *fetcherInstance)
{
}

bool AampDRMSessionManager::QueueContentProtection(DrmHelperPtr drmHelper, std::string periodId, uint32_t adapIdx, AampMediaType type, bool isVssPeriod)
{
	return false;
}

void AampDRMSessionManager::QueueProtectionEvent(DrmHelperPtr drmHelper, std::string periodId, uint32_t adapIdx, AampMediaType type)
{
}

void AampDRMSessionManager::clearDrmSession(bool forceClearSession)
{
}

void AampDRMSessionManager::clearFailedKeyIds()
{
}

void AampDRMSessionManager::setSessionMgrState(SessionMgrState state)
{
}

void AampDRMSessionManager::SetSendErrorOnFailure(bool sendErrorOnFailure)
{
}

void AampDRMSessionManager::SetCommonKeyDuration(int keyDuration)
{
}

void AampDRMSessionManager::notifyCleanup()
{
}
