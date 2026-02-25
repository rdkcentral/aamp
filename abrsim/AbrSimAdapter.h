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
 * @file AbrSimAdapter.h
 * @brief Adapter to integrate AAMP's ABRManager into abrsim
 * 
 * This adapter bridges the gap between abrsim's simulation environment
 * and AAMP's real ABR algorithms, allowing us to test and refine
 * ABR heuristics in faster-than-real-time.
 */

#ifndef ABR_SIM_ADAPTER_H
#define ABR_SIM_ADAPTER_H

#include <memory>
#include <cstdint>
#include <cstddef>

// Forward declarations to avoid heavy AAMP dependencies
class ABRManager;

namespace abrsim {

/**
 * @brief Minimal types for simulation (avoids AAMP headers)
 * Note: Using 'long' to match AAMP's BitsPerSecond typedef from AampMediaType.h
 * This ensures compatibility across platforms where long may be 32-bit.
 */
using BitsPerSecond = long;

/**
 * @brief Download metrics for bandwidth estimation
 */
struct SimDownloadMetrics {
	std::size_t sizeBytes;
	double totalTimeSeconds;
	double timeToFirstByteSeconds;
};

/**
 * @brief Video profile information
 */
struct SimProfileInfo {
	int index;
	BitsPerSecond bitrateBps;
	int width;
	int height;
	bool isIframeTrack;
	
	SimProfileInfo() 
		: index(0), bitrateBps(0), width(0), height(0), isIframeTrack(false) {}
		
	SimProfileInfo(int idx, BitsPerSecond bps, int w, int h, bool iframe = false)
		: index(idx), bitrateBps(bps), width(w), height(h), isIframeTrack(iframe) {}
};

/**
 * @brief ABR decision context for simulation
 */
struct AbrDecisionContext {
	double currentBufferSeconds;
	double targetBufferSeconds;
	double minBufferSeconds;
	bool isLive;
	double currentLatencySeconds;  // For live streaming
	bool isRebuffering;
	int segmentNumber;
};

/**
 * @brief Adapter class integrating AAMP's ABRManager into abrsim
 * 
 * This class wraps AAMP's ABRManager and provides a simplified interface
 * suitable for the simulation environment. It handles:
 * - Profile management
 * - Bandwidth estimation updates
 * - ABR decision-making based on simulation state
 * - Configuration of ABR parameters
 */
class AbrSimAdapter {
public:
	/**
	 * @brief Construct adapter with default configuration
	 */
	AbrSimAdapter();
	
	/**
	 * @brief Destructor
	 */
	~AbrSimAdapter();
	
	// Profile management
	
	/**
	 * @brief Add a video profile to the ABR ladder
	 * @param profile Profile information
	 */
	void addProfile(const SimProfileInfo& profile);
	
	/**
	 * @brief Clear all profiles
	 */
	void clearProfiles();
	
	/**
	 * @brief Get the number of profiles
	 * @return Profile count
	 */
	int getProfileCount() const;
	
	// Bandwidth estimation
	
	/**
	 * @brief Report download completion for bandwidth estimation
	 * @param metrics Download metrics
	 * @param isLowLatency Whether this is low-latency mode
	 */
	void reportDownload(const SimDownloadMetrics& metrics, bool isLowLatency = false);
	
	/**
	 * @brief Get current estimated bandwidth
	 * @return Bandwidth in bits per second
	 */
	BitsPerSecond getCurrentBandwidth() const;
	
	/**
	 * @brief Get network bandwidth estimate
	 * @return Network bandwidth in bits per second
	 */
	BitsPerSecond getNetworkBandwidth() const;
	
	// ABR decision-making
	
	/**
	 * @brief Get initial profile for playback start
	 * @param chooseMedium If true, choose medium profile; otherwise use bandwidth-based selection
	 * @return Profile index
	 */
	int getInitialProfile(bool chooseMedium = false);
	
	/**
	 * @brief Make ABR decision based on current state
	 * @param currentProfile Current profile index
	 * @param context Current playback context
	 * @return Recommended profile index
	 */
	int makeAbrDecision(int currentProfile, const AbrDecisionContext& context);
	
	/**
	 * @brief Get profile one step down from current
	 * @param currentProfile Current profile index
	 * @return Lower profile index
	 */
	int getRampedDownProfile(int currentProfile);
	
	/**
	 * @brief Get profile one step up from current
	 * @param currentProfile Current profile index
	 * @return Higher profile index
	 */
	int getRampedUpProfile(int currentProfile);
	
	/**
	 * @brief Check if current profile is the lowest
	 * @param profileIndex Profile to check
	 * @return True if lowest profile
	 */
	bool isLowestProfile(int profileIndex) const;
	
	/**
	 * @brief Get bitrate of a profile
	 * @param profileIndex Profile index
	 * @return Bitrate in bits per second
	 */
	BitsPerSecond getProfileBitrate(int profileIndex) const;
	
	// Configuration
	
	/**
	 * @brief Set default initial bitrate
	 * @param bitrate Default bitrate in bits per second
	 */
	void setDefaultInitBitrate(BitsPerSecond bitrate);
	
	/**
	 * @brief Configure ABR parameters
	 * @param minBuffer Minimum buffer for rampdown (seconds)
	 * @param maxBuffer Maximum buffer for rampup (seconds)
	 * @param nwConsistency Network consistency count
	 */
	void configureAbrParameters(int minBuffer, int maxBuffer, int nwConsistency);
	
	/**
	 * @brief Select bandwidth estimation algorithm
	 * @param algorithmType 0=RollingMedian, 1=HarmonicEWMA
	 */
	void selectBandwidthEstimationAlgorithm(int algorithmType);

private:
	std::unique_ptr<ABRManager> mAbrManager;
	bool mInitialized;
	int mNetworkConsistencyCount;
};

} // namespace abrsim

#endif // ABR_SIM_ADAPTER_H
