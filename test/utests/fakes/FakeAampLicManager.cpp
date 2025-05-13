
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
#include "PlayerUtils.h"
MockAampLicenseManager *g_mockAampLicenseManager = nullptr;
AAMPTuneFailure MapDrmToAampTuneFailure(DrmTuneFailure drmError)
{
    switch (drmError)
    {
        case MW_DRM_INIT_FAILED:            return AAMP_TUNE_DRM_INIT_FAILED;
        case MW_DRM_DATA_BIND_FAILED:       return AAMP_TUNE_DRM_DATA_BIND_FAILED;
        case MW_DRM_SESSIONID_EMPTY:        return AAMP_TUNE_DRM_SESSIONID_EMPTY;
        case MW_DRM_CHALLENGE_FAILED:       return AAMP_TUNE_DRM_CHALLENGE_FAILED;
        case MW_INVALID_DRM_KEY:            return AAMP_TUNE_INVALID_DRM_KEY;
        case MW_CORRUPT_DRM_DATA:           return AAMP_TUNE_CORRUPT_DRM_DATA;
        case MW_CORRUPT_DRM_METADATA:       return AAMP_TUNE_CORRUPT_DRM_METADATA;
        case MW_DRM_DECRYPT_FAILED:         return AAMP_TUNE_DRM_DECRYPT_FAILED;
        case MW_DRM_UNSUPPORTED:            return AAMP_TUNE_DRM_UNSUPPORTED;
        case MW_DRM_SELF_ABORT:             return AAMP_TUNE_DRM_SELF_ABORT;
        case MW_DRM_KEY_UPDATE_FAILED:      return AAMP_TUNE_DRM_KEY_UPDATE_FAILED;
        case MW_FAILED_TO_GET_KEYID:        return AAMP_TUNE_FAILED_TO_GET_KEYID;
        case MW_UNTRACKED_DRM_ERROR:        return AAMP_TUNE_UNTRACKED_DRM_ERROR;
        default:                            return AAMP_TUNE_UNTRACKED_DRM_ERROR;
    }
}

AampDRMLicenseManager::AampDRMLicenseManager(int, PrivateInstanceAAMP*)
{
}

AampDRMLicenseManager::~AampDRMLicenseManager()
{
}

void AampDRMLicenseManager::renewLicense(std::shared_ptr<DrmHelper>, void*, PrivateInstanceAAMP*)
{
}

void AampDRMLicenseManager::setPlaybackSpeedState(bool , double, bool, double, int, double, bool)
{
}

void AampDRMLicenseManager::hideWatermarkOnDetach()
{
}

void AampDRMLicenseManager::setVideoMute(bool live, double currentLatency, bool livepoint , double liveOffsetMs,bool isVideoOnMute, double positionMs)
{
}

void AampDRMLicenseManager::setVideoWindowSize(int width, int height)
{
	if (g_mockAampLicenseManager)
	{
		g_mockAampLicenseManager->setVideoWindowSize(width, height);
	}
}

void AampDRMLicenseManager::Stop()
{
}

void AampDRMLicenseManager::UpdateMaxDRMSessions(int)
{
}

void AampDRMLicenseManager::setLicenseRequestAbort(bool)
{
}

		

void AampDRMLicenseManager::SetLicenseFetcher(AampLicenseFetcher *fetcherInstance)
{
}

bool AampDRMLicenseManager::QueueContentProtection(DrmHelperPtr drmHelper, std::string periodId, uint32_t adapIdx, AampMediaType type, bool isVssPeriod)
{
	return false;
}

void AampDRMLicenseManager::QueueProtectionEvent(DrmHelperPtr drmHelper, std::string periodId, uint32_t adapIdx, AampMediaType type)
{
}

void AampDRMLicenseManager::clearDrmSession(bool forceClearSession)
{
}

void AampDRMLicenseManager::clearFailedKeyIds()
{
}

void AampDRMLicenseManager::setSessionMgrState(SessionMgrState state)
{
}

void AampDRMLicenseManager::SetSendErrorOnFailure(bool sendErrorOnFailure)
{
}

void AampDRMLicenseManager::SetCommonKeyDuration(int keyDuration)
{
}

void AampDRMLicenseManager::notifyCleanup()
{
}
DrmSession* AampDRMLicenseManager::createDrmSession(char const*, MediaFormat, unsigned char const*, unsigned short, int, DrmCallbacks*, std::shared_ptr<DrmMetaDataEvent>, unsigned char const*, bool)
{
}
SessionMgrState AampDRMLicenseManager::getSessionMgrState()
{
 return SessionMgrState::eSESSIONMGR_INACTIVE;
}
