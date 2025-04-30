
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

#include "MockAampLicManager.h"
#include "priv_aamp.h"
MockAampLicenseManager *g_mockAampLicenseManager = nullptr;

AampLicenseManager::AampLicenseManager(int, PrivateInstanceAAMP*)
{
}

AampLicenseManager::~AampLicenseManager()
{
}

void AampLicenseManager::renewLicense(std::shared_ptr<DrmHelper>, void*, PrivateInstanceAAMP*)
{
}

DrmData * AampLicenseManager::getLicenseSec(const LicenseRequest &licenseRequest, std::shared_ptr<DrmHelper> drmHelper,
		const ChallengeInfo& challengeInfo, void* aampI, int32_t *httpCode, int32_t *httpExtStatusCode, DrmMetaDataEventPtr eventHandle)
{
	return nullptr;
}

void AampLicenseManager::setPlaybackSpeedState(bool , double, bool, double, int, double, bool)
{
}

void AampLicenseManager::hideWatermarkOnDetach()
{
}

void AampLicenseManager::setVideoMute(bool live, double currentLatency, bool livepoint , double liveOffsetMs,bool isVideoOnMute, double positionMs)
{
}

void AampLicenseManager::setVideoWindowSize(int width, int height)
{
	if (g_mockAampLicenseManager)
	{
		g_mockAampLicenseManager->setVideoWindowSize(width, height);
	}
}

void AampLicenseManager::Stop()
{
}

void AampLicenseManager::UpdateMaxDRMSessions(int)
{
}

void AampLicenseManager::setLicenseRequestAbort(bool)
{
}

		

void AampLicenseManager::SetLicenseFetcher(AampLicenseFetcher *fetcherInstance)
{
}

bool AampLicenseManager::QueueContentProtection(DrmHelperPtr drmHelper, std::string periodId, uint32_t adapIdx, AampMediaType type, bool isVssPeriod)
{
	return false;
}

void AampLicenseManager::QueueProtectionEvent(DrmHelperPtr drmHelper, std::string periodId, uint32_t adapIdx, AampMediaType type)
{
}

void AampLicenseManager::clearDrmSession(bool forceClearSession)
{
}

void AampLicenseManager::clearFailedKeyIds()
{
}

void AampLicenseManager::setSessionMgrState(SessionMgrState state)
{
}

void AampLicenseManager::SetSendErrorOnFailure(bool sendErrorOnFailure)
{
}

void AampLicenseManager::SetCommonKeyDuration(int keyDuration)
{
}

void AampLicenseManager::notifyCleanup()
{
}
DrmSession* AampLicenseManager::createDrmSession(char const*, MediaFormat, unsigned char const*, unsigned short, int, DrmCallbacks*, std::shared_ptr<DrmMetaDataEvent>, unsigned char const*, bool)
{
}
SessionMgrState AampLicenseManager::getSessionMgrState()
{
 return SessionMgrState::eSESSIONMGR_INACTIVE;
}
