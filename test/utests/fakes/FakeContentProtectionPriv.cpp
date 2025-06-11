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

#include "ContentProtectionPriv.h"
#include <string.h>
#include "_base64.h"
#include <uuid/uuid.h>

ContentProtectionBase* ContentProtection::mInstance = nullptr;

bool ContentProtection::ContentProtectionEnabled()
{
	return false;
}

bool ContentProtectionBase::AcquireLicense(std::string clientId, std::string appId, const char* licenseUrl, const char* moneyTraceMetdata[][2],
		const char* accessAttributes[][2], const char* contentMetadata, size_t contentMetadataLen,
		const char* licenseRequest, size_t licenseRequestLen, const char* keySystemId,
		const char* mediaUsage, const char* accessToken, size_t accessTokenLen,
		ContentProtectionSession &session,
		char** licenseResponse, size_t* licenseResponseLength,
		int32_t* statusCode, int32_t* reasonCode, int32_t* businessStatus, bool isVideoMuted, int sleepTime) 
{
	return false;
}

bool ContentProtectionBase::UpdateSessionState(int64_t sessionId, bool active) 
{
	return false;
}

bool ContentProtectionBase::setPlaybackSpeedState(int64_t sessionId, float speed, int32_t position) 
{
	return false;
}

bool ContentProtectionBase::getSchedulerStatus() 
{
	return false;
}
ContentProtectionBase *ContentProtection::GetInstance()
{
	mInstance = new FakeContentProtection();
	return mInstance;
}

void ContentProtection::DestroyInstance()
{
	delete mInstance;
}

void ContentProtectionBase::setWatermarkSessionEvent_CB(const std::function<void(uint32_t, uint32_t, const std::string&)>& callback)
{
}

int64_t ContentProtectionSession::getSessionID(void) const
{
	return -1;
}

std::size_t ContentProtectionSession::getInputSummaryHash()
{
	return 0;
}

ContentProtectionBase::ContentProtectionBase()
{
}

ContentProtectionBase::~ContentProtectionBase()
{
}
