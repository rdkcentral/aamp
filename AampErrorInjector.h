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
 * @file AampErrorInjector.h
 * @brief Error injection framework for testing upstream error handling
 */

#ifndef AAMP_ERROR_INJECTOR_H
#define AAMP_ERROR_INJECTOR_H

#include "AampEvent.h"
#include <mutex>

class PrivateInstanceAAMP;

/**
 * @class AampErrorInjector
 * @brief Manages error injection for testing purposes
 *
 * This class provides a framework to inject errors at specific points during
 * playback to test how upstream components handle various error conditions.
 *
 * Configuration Parameters:
 *   errorInjectionCode (int):
 *     -1 = Disabled (default)
 *     >= 0 = AAMPTuneFailure enum value to inject
 *
 *   Error Categories:
 *     Category 1: INIT failures (0-6) - No subcode needed
 *       0: AAMP_TUNE_INIT_FAILED
 *       1: AAMP_TUNE_INIT_FAILED_MANIFEST_DNLD_ERROR
 *       2: AAMP_TUNE_INIT_FAILED_MANIFEST_CONTENT_ERROR
 *       3: AAMP_TUNE_INIT_FAILED_MANIFEST_PARSE_ERROR
 *       4: AAMP_TUNE_INIT_FAILED_PLAYLIST_VIDEO_DNLD_ERROR
 *       5: AAMP_TUNE_INIT_FAILED_PLAYLIST_AUDIO_DNLD_ERROR
 *       6: AAMP_TUNE_INIT_FAILED_TRACK_SYNC_ERROR
 *
 *     Category 2: Download failures (7-15) - HTTP error code in subcode
 *       7: AAMP_TUNE_CONTENT_NOT_FOUND
 *       8: AAMP_TUNE_MANIFEST_REQ_FAILED
 *       9: AAMP_TUNE_FRAGMENT_DOWNLOAD_FAILURE
 *       10: AAMP_TUNE_INIT_FRAGMENT_DOWNLOAD_FAILURE
 *       11: AAMP_TUNE_INVALID_MANIFEST_FAILURE
 *       12: AAMP_TUNE_MP4_INIT_FRAGMENT_MISSING
 *       13: AAMP_TUNE_DNS_RESOLVE_TIMEOUT
 *       14: AAMP_TUNE_CURL_CONNECTION_TIMEOUT
 *       15: AAMP_TUNE_DATA_TRANSFER_TIMEOUT
 *
 *     Category 3: DRM/Auth/Provisioning/HDCP failures (16-35) - No subcode needed
 *       16: AAMP_TUNE_AUTHORIZATION_FAILURE
 *       17-33: DRM errors (AAMP_TUNE_UNTRACKED_DRM_ERROR to AAMP_TUNE_DRM_SESSION_CREATE_FAILED)
 *       34: AAMP_TUNE_DEVICE_NOT_PROVISIONED
 *       35: AAMP_TUNE_HDCP_COMPLIANCE_ERROR
 *
 *     Category 4: Playback failures (36-42) - No subcode needed
 *       36: AAMP_TUNE_UNSUPPORTED_STREAM_TYPE
 *       37: AAMP_TUNE_UNSUPPORTED_AUDIO_TYPE
 *       38: AAMP_TUNE_GST_PIPELINE_ERROR
 *       39: AAMP_TUNE_FAILED_PTS_ERROR
 *       40: AAMP_TUNE_MP4_DEMUX_ERROR
 *       41: AAMP_TUNE_PLAYBACK_STALLED
 *       42: AAMP_TUNE_FAILURE_UNKNOWN
 *
 *   errorInjectionSubCode (int):
 *     HTTP error code (e.g., 404, 500, 503) - Only used for Category 2 (Download failures)
 *     Default: 0 (no HTTP code)
 *
 *   errorInjectionPositionSec (float):
 *     0.0      = Inject at initialization/tune (default)
 *     Non-zero = Inject at specified playback position in seconds
 */
class AampErrorInjector {
public:

	/**
	 * @brief Constructor
	 * @param aamp Pointer to PrivateInstanceAAMP instance
	 */
	AampErrorInjector(PrivateInstanceAAMP* aamp);

	/**
	 * @brief Destructor
	 */
	~AampErrorInjector();

	/**
	 * @brief Load configuration from AampConfig
	 */
	void LoadConfig();

	/**
	 * @brief Check if error injection is enabled
	 * @return true if enabled (errorCode != 0)
	 */
	bool IsEnabled() const { return mErrorCode >= 0; }

	/**
	 * @brief Check if error should be injected at initialization
	 * @return true if should inject at init
	 */
	bool ShouldInjectAtInit() const;

	/**
	 * @brief Check if error should be injected at current playback position
	 * @param currentPositionSec Current playback position in seconds
	 * @return true if should inject at this position
	 */
	bool ShouldInjectAtPosition(double currentPositionSec);

	/**
	 * @brief Inject the configured error
	 */
	void InjectError();

	/**
	 * @brief Reset injection state (called on new tune)
	 */
	void Reset();

	/**
	 * @brief Get configured error code
	 * @return Error code (0 if disabled)
	 */
	int GetErrorCode() const { return mErrorCode; }

private:

	/**
	 * @brief Inject init error
	 */
	void InjectInitError();

	/**
	 * @brief Inject download/network error
	 * @param subCode HTTP or CURL error code
	 */
	void InjectDownloadError(int subCode);

	/**
	 * @brief Inject DRM error
	 */
	void InjectDRMError();

	/**
	 * @brief Inject playback error (stream/GST/MP4/stall errors)
	 */
	void InjectPlaybackError();

	PrivateInstanceAAMP* mAamp;
	int mErrorCode;                  // -1=disabled, >=0 = AAMPTuneFailure enum value
	int mSubCode;                    // HTTP error code for download errors (e.g., 404, 500)
	double mPositionSec;             // 0.0=at init, non-zero=at position
	bool mErrorInjected;             // Track if error already injected
	std::mutex mMutex;
};

#endif // AAMP_ERROR_INJECTOR_H
