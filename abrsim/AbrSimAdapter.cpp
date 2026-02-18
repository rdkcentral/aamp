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
 * @file AbrSimAdapter.cpp
 * @brief Implementation of ABR simulation adapter
 */

#include "AbrSimAdapter.h"
#include "../abr/abr.h"
#include <algorithm>

namespace abrsim {

// ============================================================================
// Helper functions to convert between simulation and AAMP types
// ============================================================================

namespace {

/**
 * @brief Convert sim download metrics to AAMP DownloadMetrics
 */
DownloadMetrics toAampMetrics(const SimDownloadMetrics& simMetrics) {
	DownloadMetrics metrics{};
	metrics.m_size_download_bytes = simMetrics.sizeBytes;
	metrics.m_total_time_seconds = simMetrics.totalTimeSeconds;
	metrics.m_time_to_first_byte_seconds = simMetrics.timeToFirstByteSeconds;
	return metrics;
}

/**
 * @brief Convert sim profile to AAMP ProfileInfo
 */
ABRManager::ProfileInfo toAampProfile(const SimProfileInfo& simProfile) {
	ABRManager::ProfileInfo profile{};
	profile.isIframeTrack = simProfile.isIframeTrack;
	profile.bandwidthBitsPerSecond = simProfile.bitrateBps;
	profile.width = simProfile.width;
	profile.height = simProfile.height;
	profile.userData = simProfile.index;
	return profile;
}

} // anonymous namespace

// ============================================================================
// AbrSimAdapter Implementation
// ============================================================================

AbrSimAdapter::AbrSimAdapter()
	: mAbrManager(std::make_unique<ABRManager>())
	, mInitialized(false)
	, mNetworkConsistencyCount(DEFAULT_ABR_NW_CONSISTENCY_COUNT)
{
	// Use Harmonic EWMA by default (smoother for simulation)
	mAbrManager->SelectBandwidthEstimationAlgorithm(
		BANDWIDTH_ESTIMATION_ALGORITHM_HARMONIC_EWMA);
}

AbrSimAdapter::~AbrSimAdapter() = default;

// Profile management

void AbrSimAdapter::addProfile(const SimProfileInfo& profile) {
	ABRManager::ProfileInfo aampProfile = toAampProfile(profile);
	mAbrManager->addProfile(aampProfile);
	mInitialized = true;
}

void AbrSimAdapter::clearProfiles() {
	mAbrManager->clearProfiles();
	mInitialized = false;
}

int AbrSimAdapter::getProfileCount() const {
	return mAbrManager->getProfileCount();
}

// Bandwidth estimation

void AbrSimAdapter::reportDownload(const SimDownloadMetrics& metrics, bool isLowLatency) {
	// Calculate bandwidth from download metrics
	if (metrics.totalTimeSeconds > 0.0) {
		BitsPerSecond downloadBps = 
			static_cast<BitsPerSecond>((metrics.sizeBytes * 8.0) / metrics.totalTimeSeconds);
		
		// Report to AAMP's bandwidth estimator
		DownloadMetrics aampMetrics = toAampMetrics(metrics);
		mAbrManager->ReportDownloadComplete(downloadBps, isLowLatency, aampMetrics);
	}
}

BitsPerSecond AbrSimAdapter::getCurrentBandwidth() const {
	return mAbrManager->GetCurrentlyAvailableBandwidth();
}

BitsPerSecond AbrSimAdapter::getNetworkBandwidth() const {
	return mAbrManager->GetNetworkBandwidth();
}

// ABR decision-making

int AbrSimAdapter::getInitialProfile(bool chooseMedium) {
	if (!mInitialized) {
		return 0;
	}
	return mAbrManager->getInitialProfileIndex(chooseMedium);
}

int AbrSimAdapter::makeAbrDecision(int currentProfile, const AbrDecisionContext& context) {
	if (!mInitialized) {
		return currentProfile;
	}
	
	BitsPerSecond currentBandwidth = getCurrentBandwidth();
	BitsPerSecond networkBandwidth = getNetworkBandwidth();
	
	// Use AAMP's sophisticated ABR logic that considers:
	// - Current bandwidth estimates
	// - Network bandwidth trends
	// - Buffer state (via network consistency counter adjustment)
	// - Ramping strategy
	
	// Adjust network consistency based on buffer health
	int adjustedConsistency = mNetworkConsistencyCount;
	
	// If buffer is critically low, be more aggressive in ramping down
	if (context.currentBufferSeconds < context.minBufferSeconds) {
		adjustedConsistency = std::max(1, mNetworkConsistencyCount / 2);
	}
	// If buffer is very healthy, be more willing to ramp up
	else if (context.currentBufferSeconds > context.targetBufferSeconds * 1.5) {
		adjustedConsistency = std::max(1, mNetworkConsistencyCount / 2);
	}
	// If rebuffering, immediately ramp down
	else if (context.isRebuffering) {
		adjustedConsistency = 1;
	}
	
	// For live streaming, also consider latency
	if (context.isLive) {
		// If falling behind live edge, prefer lower bitrates
		if (context.currentLatencySeconds > context.targetBufferSeconds * 1.2) {
			adjustedConsistency = std::max(1, mNetworkConsistencyCount / 2);
		}
	}
	
	// Get ABR recommendation from AAMP's algorithm
	int recommendedProfile = mAbrManager->getProfileIndexByBitrateRampUpOrDown(
		currentProfile, currentBandwidth, networkBandwidth, adjustedConsistency);
	
	return recommendedProfile;
}

int AbrSimAdapter::getRampedDownProfile(int currentProfile) {
	if (!mInitialized) {
		return currentProfile;
	}
	return mAbrManager->getRampedDownProfileIndex(currentProfile);
}

int AbrSimAdapter::getRampedUpProfile(int currentProfile) {
	if (!mInitialized) {
		return currentProfile;
	}
	return mAbrManager->getRampedUpProfileIndex(currentProfile);
}

bool AbrSimAdapter::isLowestProfile(int profileIndex) const {
	if (!mInitialized) {
		return true;
	}
	return mAbrManager->isProfileIndexBitrateLowest(profileIndex);
}

BitsPerSecond AbrSimAdapter::getProfileBitrate(int profileIndex) const {
	if (!mInitialized) {
		return 0;
	}
	return mAbrManager->getBandwidthOfProfile(profileIndex);
}

// Configuration

void AbrSimAdapter::setDefaultInitBitrate(BitsPerSecond bitrate) {
	mAbrManager->setDefaultInitBitrate(bitrate);
}

void AbrSimAdapter::configureAbrParameters(int minBuffer, int maxBuffer, int nwConsistency) {
	mNetworkConsistencyCount = nwConsistency;
	
	// Note: Some ABR configuration may require access to AampConfig
	// For simulation purposes, we track key parameters locally
}

void AbrSimAdapter::selectBandwidthEstimationAlgorithm(int algorithmType) {
	if (algorithmType >= 0 && algorithmType < BANDWIDTH_ESTIMATION_ALGORITHM_MAX) {
		mAbrManager->SelectBandwidthEstimationAlgorithm(
			static_cast<BandwidthEstimationAlgorithm>(algorithmType));
	}
}

} // namespace abrsim
