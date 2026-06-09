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
 * @file AampErrorInjector.cpp
 * @brief Error injection framework implementation
 */

#include "AampErrorInjector.h"
#include "priv_aamp.h"
#include "AampConfig.h"
#include "AampDefine.h"

AampErrorInjector::AampErrorInjector(PrivateInstanceAAMP* aamp)
	: mAamp(aamp)
	, mErrorCode(0)
	, mSubCode(0)
	, mPositionSec(0.0)
	, mErrorInjected(false)
{
}

AampErrorInjector::~AampErrorInjector()
{
}

void AampErrorInjector::LoadConfig()
{
	std::lock_guard<std::mutex> lock(mMutex);

	// Load error code (AAMPTuneFailure enum value; -1 = disabled)
	mErrorCode = mAamp->mConfig->GetConfigValue(eAAMPConfig_ErrorInjectionCode);

	// Load sub-code (HTTP error code for download errors)
	mSubCode = mAamp->mConfig->GetConfigValue(eAAMPConfig_ErrorInjectionSubCode);

	// Load position (0.0 = at init, non-zero = at position)
	mPositionSec = mAamp->mConfig->GetConfigValue(eAAMPConfig_ErrorInjectionPositionSec);

	if (IsEnabled()) {
		AAMPLOG_WARN("Error Injection ENABLED - ErrorCode: %d, SubCode: %d, Position: %.2f sec",
					 mErrorCode, mSubCode, mPositionSec);
	}
}

void AampErrorInjector::Reset()
{
	std::lock_guard<std::mutex> lock(mMutex);
	mErrorInjected = false;
}

bool AampErrorInjector::ShouldInjectAtInit() const
{
	if (!IsEnabled() || mErrorInjected) {
		return false;
	}

	// Inject at init if:
	// 1. Error code is an init error (AAMP_TUNE_INIT_FAILED to AAMP_TUNE_INIT_FAILED_TRACK_SYNC_ERROR), OR
	// 2. Position is 0.0 (inject any error at initialization)
	return ((mErrorCode >= AAMP_TUNE_INIT_FAILED && mErrorCode <= AAMP_TUNE_INIT_FAILED_TRACK_SYNC_ERROR) || (mPositionSec == 0.0));
}

bool AampErrorInjector::ShouldInjectAtPosition(double currentPositionSec)
{
	std::lock_guard<std::mutex> lock(mMutex);

	if (!IsEnabled() || mErrorInjected) {
		return false;
	}

	// Only inject at position if:
	// 1. Position is non-zero, AND
	// 2. Error code is NOT an init error (i.e., runtime/download/DRM error), AND
	// 3. Current position has reached target position
	if (mPositionSec > 0.0 &&
		(mErrorCode > AAMP_TUNE_INIT_FAILED_TRACK_SYNC_ERROR)) {

		// Use tolerance of 0.5 sec to avoid missing exact position
		if (currentPositionSec >= mPositionSec &&
			currentPositionSec < (mPositionSec + 0.5)) {
			return true;
		}
	}

	return false;
}

void AampErrorInjector::InjectError()
{
	std::lock_guard<std::mutex> lock(mMutex);

	if (mErrorInjected) {
		AAMPLOG_WARN("Error already injected, skipping duplicate injection");
		return;
	}

	AAMPLOG_ERR("========================================");
	AAMPLOG_ERR("INJECTING TEST ERROR");
	AAMPLOG_ERR("  Code: %d ", mErrorCode);
	AAMPLOG_ERR("  SubCode: %d", mSubCode);
	AAMPLOG_ERR("  Position: %.2f sec", mPositionSec);
	AAMPLOG_ERR("========================================");

	// Categorize and inject based on error type
	AAMPTuneFailure tuneFailure = static_cast<AAMPTuneFailure>(mErrorCode);

	// Category 1: INIT failures (0-6) - No subcode needed
	if (mErrorCode >= AAMP_TUNE_INIT_FAILED && mErrorCode <= AAMP_TUNE_INIT_FAILED_TRACK_SYNC_ERROR) {
		AAMPLOG_ERR("Category: INIT failure");
		InjectInitError();
	}
	// Category 2: Download failures with subcode (7-15) - HTTP error code in subcode
	else if (mErrorCode >= AAMP_TUNE_CONTENT_NOT_FOUND && mErrorCode <= AAMP_TUNE_DATA_TRANSFER_TIMEOUT) {
		AAMPLOG_ERR("Category: Download failure with HTTP subcode");
		InjectDownloadError(mSubCode);
	}
	// Category 3: DRM/Auth/Provisioning/HDCP failures (16-35) - No subcode needed
	else if (mErrorCode >= AAMP_TUNE_AUTHORIZATION_FAILURE && mErrorCode <= AAMP_TUNE_HDCP_COMPLIANCE_ERROR) {
		AAMPLOG_ERR("Category: DRM/Auth/HDCP failure");
		InjectDRMError();
	}
	// Category 4: Playback failures (36-42) - Stream/GST/MP4/Stall errors - No subcode needed
	else if (mErrorCode >= AAMP_TUNE_UNSUPPORTED_STREAM_TYPE && mErrorCode <= AAMP_TUNE_FAILURE_UNKNOWN) {
		AAMPLOG_ERR("Category: Playback failure");
		InjectPlaybackError();
	}
	else {
		AAMPLOG_ERR("Category: Unknown error code %d, treating as generic error", mErrorCode);
		mAamp->SendErrorEvent(tuneFailure);
	}

	mErrorInjected = true;
}

void AampErrorInjector::InjectInitError()
{
	AAMPLOG_ERR("Injecting INIT error: tuneFailure=%d", mErrorCode);

	mAamp->SendErrorEvent(static_cast<AAMPTuneFailure>(mErrorCode));
}

void AampErrorInjector::InjectDownloadError(int subCode)
{
	AAMPTuneFailure tuneFailure = (AAMPTuneFailure)mErrorCode;

	AAMPLOG_ERR("Injecting DOWNLOAD error: tuneFailure=%d", tuneFailure);

	mAamp->SendDownloadErrorEvent(tuneFailure, subCode);
}

void AampErrorInjector::InjectDRMError()
{
	AAMPTuneFailure tuneFailure;

	// Validate that it's a DRM/Auth/HDCP-related error code
	if (mErrorCode >= AAMP_TUNE_AUTHORIZATION_FAILURE &&
		mErrorCode <= AAMP_TUNE_HDCP_COMPLIANCE_ERROR) {
		tuneFailure = static_cast<AAMPTuneFailure>(mErrorCode);
	} else {
		// Default to generic DRM error if invalid
		AAMPLOG_WARN("Invalid DRM error code %d, using AAMP_TUNE_UNTRACKED_DRM_ERROR", mErrorCode);
		tuneFailure = AAMP_TUNE_UNTRACKED_DRM_ERROR;
	}

	AAMPLOG_ERR("Injecting DRM/Auth error: tuneFailure=%d", tuneFailure);

	char description[256];
	snprintf(description, sizeof(description),
			 "Test Error Injection: DRM/Auth Error %d", tuneFailure);

	mAamp->SendErrorEvent(tuneFailure, description, false);
}

void AampErrorInjector::InjectPlaybackError()
{
	AAMPTuneFailure tuneFailure;

	// Validate that it's a playback-related error code
	if (mErrorCode >= AAMP_TUNE_UNSUPPORTED_STREAM_TYPE &&
		mErrorCode <= AAMP_TUNE_FAILURE_UNKNOWN) {
		tuneFailure = static_cast<AAMPTuneFailure>(mErrorCode);
	} else {
		// Default to unknown failure if invalid
		AAMPLOG_WARN("Invalid playback error code %d, using AAMP_TUNE_FAILURE_UNKNOWN", mErrorCode);
		tuneFailure = AAMP_TUNE_FAILURE_UNKNOWN;
	}

	AAMPLOG_ERR("Injecting Playback error: tuneFailure=%d", tuneFailure);

	char description[256];
	snprintf(description, sizeof(description),
			 "Test Error Injection: Playback Error %d", tuneFailure);

	mAamp->SendErrorEvent(tuneFailure, description, false);
}
