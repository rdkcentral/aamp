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
 * @file abrsim.cpp
 * @brief Standalone ABR Heuristics Simulator
 * 
 * Purpose: Test AAMP's adaptive bitrate algorithms in faster-than-real-time
 * without requiring actual stream downloads or playback.
 * 
 * Features:
 * - Simulates multi-hour stream playback in seconds
 * - Uses realistic network simulation based on NetTrace personas
 * - Models DASH manifest video profile ladders
 * - Integrates with AAMP's ABR manager from abr/
 * - Generates detailed reports on bitrate changes and rebuffering events
 * - Focus on video segment downloads (ignores manifest/audio for simplicity)
 * 
 * Build:
 *   Simple (placeholder ABR):
 *     g++ -std=c++17 -O2 -o abrsim abrsim.cpp
 *   
 *   Full (real AAMP ABR):
 *     g++ -std=c++17 -O2 -DUSE_REAL_ABR -I../abr -I.. -o abrsim abrsim.cpp \
 *         AbrSimAdapter.cpp ../abr/abr.cpp ../abr/HarmonicEwmaEstimator.cpp \
 *         ../abr/RollingMedianOutlierEstimator.cpp
 * 
 * Usage:
 *   ./abrsim --manifest profiles.json --persona network.json \
 *            --duration 7200 --out report.csv
 */

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <vector>
#include <memory>
#include <cstring>

// Conditionally include real ABR adapter
#ifdef USE_REAL_ABR
#include "AbrSimAdapter.h"
#endif

// =============================================================================
// Network Persona (from simnet)
// =============================================================================

struct NetworkCharacteristics {
	double base_rtt_ms = 175.0;
	double rtt_jitter_ms = 20.0;
	double ttfb_spike_p = 0.01;
	double ttfb_spike_ms = 120.0;
	
	double mean_thr_mbps = 200.0;
	double thr_sigma_ln = 0.80;
	double thr_rho = 0.15;
	
	int bursts_per_segment = 8;
	double burst_bytes_cv = 0.40;
	
	double cadence_ms = 175.0;
	double cadence_jitter_ms = 45.0;
	double flush_jitter_ms = 6.0;
	double late_chunk_p = 0.01;
	double late_chunk_extra_ms = 120.0;
	
	double p_conn_reuse = 0.95;
	double new_conn_penalty_ms = 170.0;
	
	double capacity_drop_p = 0.0;
	double capacity_drop_factor = 0.6;
	double rtt_inflation_ms = 0.0;
};

// =============================================================================
// Network Scenario Support
// =============================================================================

struct NetworkStage {
	std::string personaFile;
	double durationS;
	std::string description;
};

struct NetworkScenario {
	std::string description;
	std::vector<NetworkStage> stages;
	
	double getTotalDuration() const {
		double total = 0.0;
		for (const auto& stage : stages) {
			total += stage.durationS;
		}
		return total;
	}
};

// =============================================================================
// Video Profile Ladder (DASH manifest abstraction)
// =============================================================================

struct VideoProfile {
	int index;
	int64_t bitrateBps;      // Bitrate in bits/second
	int width;
	int height;
	double avgSegmentBytes;  // Average segment size in bytes
	double segmentSizeStdDev; // Variation in segment size
	
	VideoProfile(int idx, int64_t bps, int w, int h, double avgBytes, double stdDev = 0.0)
	: index(idx), bitrateBps(bps), width(w), height(h), 
	  avgSegmentBytes(avgBytes), segmentSizeStdDev(stdDev) {}
};

struct VideoProfileLadder {
	std::vector<VideoProfile> profiles;
	double segmentDurationS = 2.0; // Typical segment duration
	
	void addProfile(int idx, int64_t bitrateBps, int width, int height, 
	                double avgSegmentBytes = 0.0, double stdDev = 0.0) {
		// If avgSegmentBytes not specified, calculate from bitrate
		if (avgSegmentBytes == 0.0) {
			avgSegmentBytes = (bitrateBps * segmentDurationS) / 8.0;
		}
		// Default stdDev to 10% of average if not specified
		if (stdDev == 0.0) {
			stdDev = avgSegmentBytes * 0.10;
		}
		profiles.emplace_back(idx, bitrateBps, width, height, avgSegmentBytes, stdDev);
	}
	
	int getLowestProfileIndex() const {
		if (profiles.empty()) return -1;
		return std::min_element(profiles.begin(), profiles.end(),
			[](const VideoProfile& a, const VideoProfile& b) {
				return a.bitrateBps < b.bitrateBps;
			})->index;
	}
	
	int getHighestProfileIndex() const {
		if (profiles.empty()) return -1;
		return std::max_element(profiles.begin(), profiles.end(),
			[](const VideoProfile& a, const VideoProfile& b) {
				return a.bitrateBps < b.bitrateBps;
			})->index;
	}
	
	const VideoProfile* getProfile(int index) const {
		for (const auto& p : profiles) {
			if (p.index == index) return &p;
		}
		return nullptr;
	}
};

// =============================================================================
// Network Simulator (simplified from simnet)
// =============================================================================

class NetworkSimulator {
public:
	NetworkSimulator(const NetworkCharacteristics& nc, uint64_t seed = 0)
	: mChar(nc), mRng(seed ? seed : std::random_device{}()), 
	  mConnReused(false), mThrStateLn(0.0) {
		// Convert mean throughput from Mbps to ln(bytes/s)
		double meanBps = nc.mean_thr_mbps * 1e6 / 8.0;
		mMeanThrLn = std::log(meanBps);
		mThrStateLn = mMeanThrLn;  // Start at mean
	}
	
	// Simulate downloading a segment and return download metrics
	struct DownloadResult {
		double durationMs;       // Total download time in milliseconds
		double throughputBps;    // Measured throughput in bits/second
		bool hadStall;           // Whether download encountered a stall
		int numBursts;           // Number of bursts during download
	};
	
	DownloadResult simulateDownload(size_t segmentBytes) {
		DownloadResult result{};
		
		// Connection setup delay (if new connection)
		double setupDelayMs = 0.0;
		std::bernoulli_distribution connReuseDist(mChar.p_conn_reuse);
		mConnReused = connReuseDist(mRng);
		if (!mConnReused) {
			setupDelayMs = mChar.new_conn_penalty_ms;
		}
		
		// TTFB: base RTT + jitter + occasional spike
		std::normal_distribution<double> rttDist(mChar.base_rtt_ms, mChar.rtt_jitter_ms);
		double ttfbMs = std::max(1.0, rttDist(mRng));
		
		std::bernoulli_distribution spikeDist(mChar.ttfb_spike_p);
		if (spikeDist(mRng)) {
			ttfbMs += mChar.ttfb_spike_ms;
		}
		
		// Burst structure
		int numBursts = mChar.bursts_per_segment;
		result.numBursts = numBursts;
		
		// Distribute segment bytes across bursts with variation
		std::vector<double> burstSizes = distributeBytesAcrossBursts(segmentBytes, numBursts);
		
		// Simulate data transfer with pacing
		double transferTimeMs = 0.0;
		double totalBytesTransferred = 0.0;
		
		for (int b = 0; b < numBursts; ++b) {
			// Update throughput state (AR(1) lognormal)
			updateThroughputState();
			double burstThrBps = std::exp(mThrStateLn);
			
			// Burst transmission time
			double burstBytes = burstSizes[b];
			double burstTimeMs = (burstBytes / burstThrBps) * 1000.0;
			
			// Add flush jitter
			std::normal_distribution<double> flushDist(0.0, mChar.flush_jitter_ms);
			burstTimeMs += std::abs(flushDist(mRng));
			
			transferTimeMs += burstTimeMs;
			totalBytesTransferred += burstBytes;
			
			// Inter-burst gap (cadence)
			if (b < numBursts - 1) {
				std::normal_distribution<double> cadenceDist(
					mChar.cadence_ms, mChar.cadence_jitter_ms);
				double gapMs = std::max(0.0, cadenceDist(mRng));
				
				// Occasional late chunk
				std::bernoulli_distribution lateDist(mChar.late_chunk_p);
				if (lateDist(mRng)) {
					gapMs += mChar.late_chunk_extra_ms;
					result.hadStall = true;
				}
				
				transferTimeMs += gapMs;
			}
		}
		
		result.durationMs = setupDelayMs + ttfbMs + transferTimeMs;
		result.throughputBps = (totalBytesTransferred * 8000.0) / result.durationMs;
		
		return result;
	}
	
private:
	NetworkCharacteristics mChar;
	std::mt19937_64 mRng;
	bool mConnReused;
	double mMeanThrLn;   // ln(mean throughput in bytes/s)
	double mThrStateLn;  // ln(current throughput in bytes/s)
	
	void updateThroughputState() {
		std::normal_distribution<double> innov(0.0, mChar.thr_sigma_ln);
		// AR(1) process centered around mean
		mThrStateLn = mMeanThrLn * (1.0 - mChar.thr_rho) + mChar.thr_rho * mThrStateLn + innov(mRng);
	}
	
	std::vector<double> distributeBytesAcrossBursts(size_t totalBytes, int numBursts) {
		std::vector<double> sizes(numBursts);
		
		// Generate weights with variation
		std::gamma_distribution<double> gammaDist(
			1.0 / (mChar.burst_bytes_cv * mChar.burst_bytes_cv),
			mChar.burst_bytes_cv * mChar.burst_bytes_cv);
		
		double sumWeights = 0.0;
		for (int i = 0; i < numBursts; ++i) {
			sizes[i] = gammaDist(mRng);
			sumWeights += sizes[i];
		}
		
		// Normalize to sum to totalBytes
		for (int i = 0; i < numBursts; ++i) {
			sizes[i] = (sizes[i] / sumWeights) * totalBytes;
		}
		
		return sizes;
	}
};

// =============================================================================
// Playback Buffer Model
// =============================================================================

class PlaybackBuffer {
public:
	PlaybackBuffer(double targetBufferS = 30.0, double minBufferS = 2.0, bool isLive = false, double maxBufferS = 0.0)
	: mTargetBufferS(targetBufferS), mMinBufferS(minBufferS), 
	  mMaxBufferS(maxBufferS > 0.0 ? maxBufferS : targetBufferS),
	  mIsLive(isLive),
	  mCurrentBufferS(0.0), mIsRebuffering(false), 
	  mTotalRebufferEvents(0), mTotalRebufferTimeS(0.0),
	  mMaxLatencyS(0.0), mMinLatencyS(999999.0), mTotalLatencyS(0.0), mLatencySamples(0) {}
	
	// Add a downloaded segment to buffer
	void addSegment(double segmentDurationS) {
		mCurrentBufferS += segmentDurationS;
		
		// For live streaming, cap buffer at max allowed
		if (mIsLive && mCurrentBufferS > mMaxBufferS) {
			mCurrentBufferS = mMaxBufferS;
		}
	}
	
	// Simulate playback consuming buffer during download
	void consumeBuffer(double elapsedTimeS) {
		mCurrentBufferS -= elapsedTimeS;
		
		// Check for rebuffering
		if (mCurrentBufferS < 0.0) {
			if (!mIsRebuffering) {
				startRebuffering();
			}
			mTotalRebufferTimeS += (-mCurrentBufferS);
			mCurrentBufferS = 0.0;
		}
	}
	
	// Start rebuffering
	void startRebuffering() {
		if (!mIsRebuffering) {
			mIsRebuffering = true;
			mTotalRebufferEvents++;
		}
	}
	
	// Add rebuffer time during download
	void addRebufferTime(double timeS) {
		mTotalRebufferTimeS += timeS;
	}
	
	// End rebuffering when new segment arrives
	void endRebuffering() {
		mIsRebuffering = false;
	}
	
	// Track latency from live edge (for live streaming)
	void recordLatency(double latencyS) {
		if (latencyS > mMaxLatencyS) mMaxLatencyS = latencyS;
		if (latencyS < mMinLatencyS) mMinLatencyS = latencyS;
		mTotalLatencyS += latencyS;
		mLatencySamples++;
	}
	
	double getCurrentBuffer() const { return mCurrentBufferS; }
	bool isRebuffering() const { return mIsRebuffering; }
	bool needsSegment() const { return mCurrentBufferS < mTargetBufferS; }
	bool isHealthy() const { return mCurrentBufferS >= mMinBufferS; }
	double getMinBuffer() const { return mMinBufferS; }
	double getTargetBuffer() const { return mTargetBufferS; }
	
	int getTotalRebufferEvents() const { return mTotalRebufferEvents; }
	double getTotalRebufferTime() const { return mTotalRebufferTimeS; }
	
	double getMaxLatency() const { return mMaxLatencyS; }
	double getMinLatency() const { return mLatencySamples > 0 ? mMinLatencyS : 0.0; }
	double getAvgLatency() const { return mLatencySamples > 0 ? mTotalLatencyS / mLatencySamples : 0.0; }
	
private:
	double mTargetBufferS;
	double mMinBufferS;
	double mMaxBufferS;       // For live: cap on buffer (distance from live edge)
	bool mIsLive;
	double mCurrentBufferS;
	bool mIsRebuffering;
	int mTotalRebufferEvents;
	double mTotalRebufferTimeS;
	
	// Live streaming latency tracking
	double mMaxLatencyS;
	double mMinLatencyS;
	double mTotalLatencyS;
	int mLatencySamples;
};

// =============================================================================
// ABR Simulation Event Log
// =============================================================================

struct SimulationEvent {
	enum Type {
		SEGMENT_DOWNLOAD,
		PROFILE_CHANGE,
		REBUFFER_START,
		REBUFFER_END
	};
	
	double timeS;
	Type type;
	int profileIndex;
	double downloadTimeMs;
	double throughputBps;
	double bufferLevelS;
	std::string description;
};

class EventLogger {
public:
	void log(const SimulationEvent& event) {
		mEvents.push_back(event);
	}
	
	void writeCSV(const std::string& filename) const {
		std::ofstream ofs(filename);
		if (!ofs) {
			std::cerr << "Failed to open " << filename << std::endl;
			return;
		}
		
		ofs << "time_s,event_type,profile_idx,download_ms,throughput_bps,buffer_s,description\n";
		
		for (const auto& e : mEvents) {
			ofs << std::fixed << std::setprecision(3) << e.timeS << ',';
			
			switch (e.type) {
				case SimulationEvent::SEGMENT_DOWNLOAD: ofs << "download"; break;
				case SimulationEvent::PROFILE_CHANGE: ofs << "profile_change"; break;
				case SimulationEvent::REBUFFER_START: ofs << "rebuffer_start"; break;
				case SimulationEvent::REBUFFER_END: ofs << "rebuffer_end"; break;
			}
			
			ofs << ',' << e.profileIndex 
			    << ',' << std::setprecision(2) << e.downloadTimeMs
			    << ',' << std::setprecision(0) << e.throughputBps
			    << ',' << std::setprecision(2) << e.bufferLevelS
			    << ',' << e.description << '\n';
		}
		
		ofs.close();
		std::cout << "Wrote event log to " << filename << std::endl;
	}
	
	void printSummary(double totalDurationS) const {
		std::cout << "\n=== Simulation Summary ===\n";
		std::cout << "Total duration: " << std::fixed << std::setprecision(1) 
		          << totalDurationS << " seconds\n";
		std::cout << "Total events: " << mEvents.size() << "\n";
		
		int downloads = 0, profileChanges = 0, rebuffers = 0;
		for (const auto& e : mEvents) {
			switch (e.type) {
				case SimulationEvent::SEGMENT_DOWNLOAD: downloads++; break;
				case SimulationEvent::PROFILE_CHANGE: profileChanges++; break;
				case SimulationEvent::REBUFFER_START: rebuffers++; break;
				default: break;
			}
		}
		
		std::cout << "Segment downloads: " << downloads << "\n";
		std::cout << "Profile changes: " << profileChanges << "\n";
		std::cout << "Rebuffer events: " << rebuffers << "\n";
	}
	
	// Get raw events for multi-stage scenario combining
	const std::vector<SimulationEvent>& getEvents() const {
		return mEvents;
	}
	
private:
	std::vector<SimulationEvent> mEvents;
};

// =============================================================================
// JSON Parsing Utilities (minimal implementation)
// =============================================================================

static bool FindNumber(const std::string& s, const std::string& key, double &out) {
	const std::string kq = "\"" + key + "\"";
	size_t kp = s.find(kq);
	if (kp == std::string::npos) return false;
	size_t colon = s.find(':', kp + kq.size());
	if (colon == std::string::npos) return false;
	size_t start = s.find_first_of("-0123456789", colon + 1);
	if (start == std::string::npos) return false;
	size_t end = s.find_first_not_of("0123456789.eE+-", start);
	try { out = std::stod(s.substr(start, end - start)); }
	catch (...) { return false; }
	return true;
}

static bool FindInt(const std::string& s, const std::string& key, int &out) {
	double d;
	if (!FindNumber(s, key, d)) return false;
	out = static_cast<int>(d);
	return true;
}

static bool LoadPersona(const std::string& filename, NetworkCharacteristics& nc) {
	std::ifstream ifs(filename);
	if (!ifs) {
		std::cerr << "Failed to open persona file: " << filename << std::endl;
		return false;
	}
	
	std::stringstream buf;
	buf << ifs.rdbuf();
	std::string json = buf.str();
	
	FindNumber(json, "base_rtt_ms", nc.base_rtt_ms);
	FindNumber(json, "rtt_jitter_ms", nc.rtt_jitter_ms);
	FindNumber(json, "ttfb_spike_p", nc.ttfb_spike_p);
	FindNumber(json, "ttfb_spike_ms", nc.ttfb_spike_ms);
	FindNumber(json, "mean_thr_mbps", nc.mean_thr_mbps);
	FindNumber(json, "thr_sigma_ln", nc.thr_sigma_ln);
	FindNumber(json, "thr_rho", nc.thr_rho);
	FindInt(json, "bursts_per_segment", nc.bursts_per_segment);
	FindNumber(json, "burst_bytes_cv", nc.burst_bytes_cv);
	FindNumber(json, "cadence_ms", nc.cadence_ms);
	FindNumber(json, "cadence_jitter_ms", nc.cadence_jitter_ms);
	FindNumber(json, "flush_jitter_ms", nc.flush_jitter_ms);
	FindNumber(json, "late_chunk_p", nc.late_chunk_p);
	FindNumber(json, "late_chunk_extra_ms", nc.late_chunk_extra_ms);
	FindNumber(json, "p_conn_reuse", nc.p_conn_reuse);
	FindNumber(json, "new_conn_penalty_ms", nc.new_conn_penalty_ms);
	
	return true;
}

static bool LoadScenario(const std::string& filename, NetworkScenario& scenario) {
	std::ifstream ifs(filename);
	if (!ifs) {
		std::cerr << "Failed to open scenario file: " << filename << std::endl;
		return false;
	}
	
	std::stringstream buf;
	buf << ifs.rdbuf();
	std::string json = buf.str();
	
	// Find description
	size_t descPos = json.find("\"description\"");
	if (descPos != std::string::npos) {
		size_t colonPos = json.find(':', descPos);
		size_t quoteStart = json.find('"', colonPos);
		size_t quoteEnd = json.find('"', quoteStart + 1);
		if (quoteStart != std::string::npos && quoteEnd != std::string::npos) {
			scenario.description = json.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
		}
	}
	
	// Find stages array
	size_t stagesPos = json.find("\"stages\"");
	if (stagesPos == std::string::npos) {
		std::cerr << "No 'stages' array found in scenario file" << std::endl;
		return false;
	}
	
	size_t arrayStart = json.find('[', stagesPos);
	size_t arrayEnd = json.find(']', arrayStart);
	if (arrayStart == std::string::npos || arrayEnd == std::string::npos) {
		std::cerr << "Malformed stages array" << std::endl;
		return false;
	}
	
	// Parse each stage object
	size_t pos = arrayStart + 1;
	while (pos < arrayEnd) {
		size_t objStart = json.find('{', pos);
		if (objStart >= arrayEnd) break;
		
		size_t objEnd = json.find('}', objStart);
		if (objEnd >= arrayEnd) break;
		
		std::string stageJson = json.substr(objStart, objEnd - objStart + 1);
		
		NetworkStage stage;
		
		// Extract persona
		size_t personaPos = stageJson.find("\"persona\"");
		if (personaPos != std::string::npos) {
			size_t colonPos = stageJson.find(':', personaPos);
			size_t quoteStart = stageJson.find('"', colonPos);
			size_t quoteEnd = stageJson.find('"', quoteStart + 1);
			if (quoteStart != std::string::npos && quoteEnd != std::string::npos) {
				stage.personaFile = stageJson.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
			}
		}
		
		// Extract duration
		FindNumber(stageJson, "duration", stage.durationS);
		
		// Extract description (optional)
		size_t descPos = stageJson.find("\"description\"");
		if (descPos != std::string::npos) {
			size_t colonPos = stageJson.find(':', descPos);
			size_t quoteStart = stageJson.find('"', colonPos);
			size_t quoteEnd = stageJson.find('"', quoteStart + 1);
			if (quoteStart != std::string::npos && quoteEnd != std::string::npos) {
				stage.description = stageJson.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
			}
		}
		
		if (!stage.personaFile.empty() && stage.durationS > 0) {
			scenario.stages.push_back(stage);
		}
		
		pos = objEnd + 1;
	}
	
	return !scenario.stages.empty();
}

// =============================================================================
// Main Simulation Engine
// =============================================================================

class ABRSimulator {
public:
	ABRSimulator(const VideoProfileLadder& ladder, 
	             const NetworkCharacteristics& netChar,
	             bool isLive = true,
	             double targetLatencyS = 6.0,
	             double maxBufferS = 20.0,
	             uint64_t seed = 0)
	: mLadder(ladder), mNetSim(netChar, seed), 
	  mBuffer(isLive ? targetLatencyS : maxBufferS, 2.0, true, isLive ? targetLatencyS : maxBufferS),
	  mIsLive(isLive), mTargetLatencyS(targetLatencyS), mMaxBufferS(maxBufferS),
	  mLiveEdgeS(0.0), mCurrentSegmentNum(0),
	  mCurrentProfile(0), mSimTimeS(0.0), mPlaybackTimeS(0.0), mRealClockS(0.0),
	  mRng(seed ? seed : std::random_device{}()) {
		
#ifdef USE_REAL_ABR
		// Initialize real AAMP ABR
		mAbrAdapter = std::make_unique<abrsim::AbrSimAdapter>();
		
		// Add profiles to ABR manager
		for (const auto& profile : ladder.profiles) {
			abrsim::SimProfileInfo simProfile{};
			simProfile.index = profile.index;
			simProfile.bitrateBps = profile.bitrateBps;
			simProfile.width = profile.width;
			simProfile.height = profile.height;
			simProfile.isIframeTrack = false;
			mAbrAdapter->addProfile(simProfile);
		}
		
		// Configure ABR parameters
		mAbrAdapter->configureAbrParameters(
			2,   // minBuffer (seconds)
			isLive ? static_cast<int>(targetLatencyS) : static_cast<int>(maxBufferS),  // maxBuffer
			3    // network consistency count
		);
		
		// Select bandwidth estimation algorithm (use Harmonic EWMA for smoother results)
		mAbrAdapter->selectBandwidthEstimationAlgorithm(1); // 1 = Harmonic EWMA
		
		// Get initial profile from ABR
		mCurrentProfile = mAbrAdapter->getInitialProfile(false);
		std::cout << "Using AAMP's real ABR algorithm\n";
#else
		// Start with mid-range profile
		mCurrentProfile = ladder.profiles.size() / 2;
		std::cout << "Using simple placeholder ABR algorithm\n";
#endif
	}
	
	void run(double durationS) {
		if (mIsLive) {
			std::cout << "Starting LIVE ABR simulation for " << durationS << " seconds...\n";
			std::cout << "Target latency: " << mTargetLatencyS << "s from live edge\n";
		} else {
			std::cout << "Starting VOD ABR simulation for " << durationS << " seconds...\n";
			std::cout << "Max buffer: " << mMaxBufferS << "s\n";
		}
		
		auto startTime = std::chrono::steady_clock::now();
		
			int segmentCount = 0;
		
		// For live streaming: calculate how many initial segments needed for target latency
		int initialSegmentsForLatency = 0;
		if (mIsLive) {
			initialSegmentsForLatency = static_cast<int>(std::ceil(mTargetLatencyS / mLadder.segmentDurationS));
			// Live edge starts ahead by target latency
			mLiveEdgeS = mTargetLatencyS;
			std::cout << "Initial segments immediately available: " << initialSegmentsForLatency 
			          << " (" << (initialSegmentsForLatency * mLadder.segmentDurationS) << "s of content)\n";
		}
		
		while (mPlaybackTimeS < durationS) {
			// For live streaming, update live edge and track latency
			if (mIsLive) {
				// Live edge advances at real-time rate (simulation time)
				mLiveEdgeS = mTargetLatencyS + mSimTimeS;
				
				// Current latency = distance behind live edge
				double currentLatencyS = mLiveEdgeS - mPlaybackTimeS;
				mBuffer.recordLatency(currentLatencyS);
				
				// Check if next segment is available yet (after initial buffering period)
				if (mCurrentSegmentNum >= initialSegmentsForLatency) {
					// Normal live segment availability - segment N available when live edge reaches it
					double segmentAvailableTime = (mCurrentSegmentNum - initialSegmentsForLatency) * mLadder.segmentDurationS;
					if (mSimTimeS < segmentAvailableTime) {
						// Wait for next segment to become available
						double tickTime = 0.05;
						if (!mBuffer.isRebuffering()) {
							mBuffer.consumeBuffer(tickTime);
							mPlaybackTimeS += tickTime;
						}
						mSimTimeS += tickTime;
						continue;
					}
				}
			}
			
			// Check if buffer is too low - need to stall playback
		// Don't count initial buffering (segment 0) as a rebuffer event
		if (mBuffer.getCurrentBuffer() < 0.001 && mCurrentSegmentNum > 0) {
				if (!mBuffer.isRebuffering()) {
					SimulationEvent rebufferEvent{};
					rebufferEvent.timeS = mPlaybackTimeS;
					rebufferEvent.type = SimulationEvent::REBUFFER_START;
					rebufferEvent.profileIndex = mCurrentProfile;
					rebufferEvent.bufferLevelS = 0.0;
					rebufferEvent.description = "Buffer underrun";
					mLogger.log(rebufferEvent);
					mBuffer.startRebuffering();
				}
			}
			
		// Decide if we need to download
		bool shouldDownload = mBuffer.needsSegment();
		
		if (!shouldDownload) {
			// No download needed, just advance playback
			double tickTime = 0.1;
			if (!mBuffer.isRebuffering()) {
				mBuffer.consumeBuffer(tickTime);
				mPlaybackTimeS += tickTime;
			}
			mSimTimeS += tickTime;
			continue;
		}
		
		// Download next segment
		const VideoProfile* profile = mLadder.getProfile(mCurrentProfile);
		if (!profile) {
			std::cerr << "Invalid profile index: " << mCurrentProfile << std::endl;
			break;
		}
		
		// Generate segment size with variation
		std::normal_distribution<double> sizeDist(
			profile->avgSegmentBytes, profile->segmentSizeStdDev);
		double segmentBytes = std::max(1000.0, sizeDist(mRng));
		
		// Simulate download
		auto result = mNetSim.simulateDownload(static_cast<size_t>(segmentBytes));
		double downloadTimeS = result.durationMs / 1000.0;
		
		// During download, playback continues (buffer consumed)
		// Only consume if not rebuffering
		if (!mBuffer.isRebuffering()) {
			mBuffer.consumeBuffer(downloadTimeS);
			mPlaybackTimeS += downloadTimeS;
		} else {
			// Rebuffering - playback stalled, track rebuffer time
			mBuffer.addRebufferTime(downloadTimeS);
		}
			
		// Log buffer BEFORE segment injection (after download consumption)
		SimulationEvent bufferBeforeEvent{};
		bufferBeforeEvent.timeS = mSimTimeS + downloadTimeS;
		bufferBeforeEvent.type = SimulationEvent::SEGMENT_DOWNLOAD;
		bufferBeforeEvent.profileIndex = mCurrentProfile;
		bufferBeforeEvent.downloadTimeMs = 0.0;
		bufferBeforeEvent.throughputBps = 0.0;
		bufferBeforeEvent.bufferLevelS = mBuffer.getCurrentBuffer();
		bufferBeforeEvent.description = "Before segment injection";
		mLogger.log(bufferBeforeEvent);
		
		// Download completes - add segment to buffer
		mBuffer.addSegment(mLadder.segmentDurationS);
		
		// Log buffer AFTER segment injection (show the jump)
		SimulationEvent bufferAfterEvent{};
		bufferAfterEvent.timeS = mSimTimeS + downloadTimeS;
		bufferAfterEvent.type = SimulationEvent::SEGMENT_DOWNLOAD;
		bufferAfterEvent.profileIndex = mCurrentProfile;
		bufferAfterEvent.downloadTimeMs = 0.0;
		bufferAfterEvent.throughputBps = 0.0;
		bufferAfterEvent.bufferLevelS = mBuffer.getCurrentBuffer();
		bufferAfterEvent.description = "After segment injection (+" + std::to_string(mLadder.segmentDurationS) + "s)";
		mLogger.log(bufferAfterEvent);
		if (mBuffer.isRebuffering() && mBuffer.getCurrentBuffer() > mBuffer.getMinBuffer()) {
			SimulationEvent resumeEvent{};
			resumeEvent.timeS = mPlaybackTimeS;
			resumeEvent.type = SimulationEvent::REBUFFER_END;
			resumeEvent.profileIndex = mCurrentProfile;
			resumeEvent.bufferLevelS = mBuffer.getCurrentBuffer();
			resumeEvent.description = "Playback resumed";
			mLogger.log(resumeEvent);
			mBuffer.endRebuffering();
		}
		
		// Simulation time advances by download duration
		mSimTimeS += downloadTimeS;
		mCurrentSegmentNum++;
		
		// Log download event
		SimulationEvent downloadEvent{};
		downloadEvent.timeS = mSimTimeS;
		downloadEvent.type = SimulationEvent::SEGMENT_DOWNLOAD;
		downloadEvent.profileIndex = mCurrentProfile;
		downloadEvent.downloadTimeMs = result.durationMs;
		downloadEvent.throughputBps = result.throughputBps;
		downloadEvent.bufferLevelS = mBuffer.getCurrentBuffer();
		if (mIsLive) {
			double latency = mLiveEdgeS - mPlaybackTimeS;
			downloadEvent.description = "Profile " + std::to_string(profile->bitrateBps / 1000) + 
			                            " kbps, Latency: " + std::to_string(static_cast<int>(latency)) + "s";
		} else {
			downloadEvent.description = "Profile " + std::to_string(profile->bitrateBps / 1000) + " kbps";
		}
		mLogger.log(downloadEvent);
		
		// ABR decision
		int newProfile = makeABRDecision(result, profile);
		if (newProfile != mCurrentProfile) {
			SimulationEvent profileEvent{};
			profileEvent.timeS = mPlaybackTimeS;
			profileEvent.type = SimulationEvent::PROFILE_CHANGE;
			profileEvent.profileIndex = newProfile;
			profileEvent.bufferLevelS = mBuffer.getCurrentBuffer();
			profileEvent.description = std::to_string(profile->bitrateBps / 1000) + 
			                           " -> " + std::to_string(mLadder.getProfile(newProfile)->bitrateBps / 1000) + " kbps";
			mLogger.log(profileEvent);
			mCurrentProfile = newProfile;
		}
		
		segmentCount++;
			
			// Sanity check
			if (segmentCount > 100000) {
				std::cerr << "Warning: Simulation exceeded 100k segments, stopping\n";
				break;
			}
		}
		
		auto endTime = std::chrono::steady_clock::now();
		mRealClockS = std::chrono::duration<double>(endTime - startTime).count();
		
		std::cout << "\nSimulation completed in " << std::fixed << std::setprecision(3) 
		          << mRealClockS << " seconds (real time)\n";
		std::cout << "Simulated " << std::setprecision(1) << mPlaybackTimeS 
		          << " seconds of playback\n";
		std::cout << "Segments downloaded: " << segmentCount << "\n";
		std::cout << "Speed-up factor: " << std::setprecision(1) 
		          << (mRealClockS > 0 ? (mPlaybackTimeS / mRealClockS) : 0.0) << "x\n";
	}
	
	void generateReport(const std::string& outfile) {
		mLogger.writeCSV(outfile);
		mLogger.printSummary(mPlaybackTimeS);
		
		// Print profile ladder
		std::cout << "\nProfile Ladder:\n";
		for (const auto& profile : mLadder.profiles) {
			std::cout << "  [" << profile.index << "] " 
			          << profile.width << "x" << profile.height
			          << " @ " << (profile.bitrateBps / 1000) << " kbps\n";
		}
		
		std::cout << "\nBuffer Statistics:\n";
		std::cout << "  Rebuffer events: " << mBuffer.getTotalRebufferEvents() << "\n";
		std::cout << "  Total rebuffer time: " << std::fixed << std::setprecision(2)
		          << mBuffer.getTotalRebufferTime() << " seconds\n";
		std::cout << "  Final buffer level: " << std::fixed << std::setprecision(2) 
		          << mBuffer.getCurrentBuffer() << " seconds\n";
		
		if (mIsLive) {
			std::cout << "\nLive Streaming Latency:\n";
			std::cout << "  Target latency: " << std::fixed << std::setprecision(2) 
			          << mTargetLatencyS << " seconds\n";
			std::cout << "  Average latency: " << std::fixed << std::setprecision(2) 
			          << mBuffer.getAvgLatency() << " seconds\n";
			std::cout << "  Min latency: " << std::fixed << std::setprecision(2) 
			          << mBuffer.getMinLatency() << " seconds\n";
			std::cout << "  Max latency: " << std::fixed << std::setprecision(2) 
			          << mBuffer.getMaxLatency() << " seconds\n";
			double drift = mBuffer.getAvgLatency() - mTargetLatencyS;
			std::cout << "  Latency drift: " << std::fixed << std::setprecision(2) 
			          << (drift >= 0 ? "+" : "") << drift << " seconds ";
			if (std::abs(drift) < 1.0) {
				std::cout << "(good)";
			} else if (std::abs(drift) < 3.0) {
				std::cout << "(acceptable)";
			} else {
				std::cout << "(poor)";
			}
			std::cout << "\n";
		}
	}
	
	// Get raw results for multi-stage scenario combining
	const std::vector<SimulationEvent>& getResults() const {
		return mLogger.getEvents();
	}
	
private:
	const VideoProfileLadder& mLadder;
	NetworkSimulator mNetSim;
	PlaybackBuffer mBuffer;
	EventLogger mLogger;
	
	// Live streaming state
	bool mIsLive;
	double mTargetLatencyS;   // Target distance from live edge
	double mMaxBufferS;       // Max buffer cap (VOD mode)
	double mLiveEdgeS;        // Current live edge position
	int mCurrentSegmentNum;   // Segment number being downloaded
	
	int mCurrentProfile;
	double mSimTimeS;         // Current simulation time
	double mPlaybackTimeS;    // Playback position (continuous)
	double mRealClockS;
	std::mt19937_64 mRng;
	
#ifdef USE_REAL_ABR
	std::unique_ptr<abrsim::AbrSimAdapter> mAbrAdapter;
#endif
	
	// Simplified ABR decision logic (placeholder for real ABRManager integration)
	int makeABRDecision(const NetworkSimulator::DownloadResult& result, 
	                    const VideoProfile* currentProfile) {
#ifdef USE_REAL_ABR
		// Use real AAMP ABR algorithm
		if (mAbrAdapter) {
			abrsim::AbrDecisionContext context{};
			context.currentBufferSeconds = mBuffer.getCurrentBuffer();
			context.targetBufferSeconds = mIsLive ? mTargetLatencyS : mMaxBufferS;
			context.minBufferSeconds = 2.0;
			context.isLive = mIsLive;
			context.currentLatencySeconds = mIsLive ? 
				(mLiveEdgeS - (mSimTimeS - mBuffer.getCurrentBuffer())) : 0.0;
			context.isRebuffering = mBuffer.isRebuffering();
			context.segmentNumber = mCurrentSegmentNum;
			
			// Report download metrics to bandwidth estimator
			abrsim::SimDownloadMetrics metrics{};
			metrics.sizeBytes = static_cast<size_t>(currentProfile->avgSegmentBytes);
			metrics.totalTimeSeconds = result.durationMs / 1000.0;
			metrics.timeToFirstByteSeconds = 0.1; // Approximate TTFB
			mAbrAdapter->reportDownload(metrics, mIsLive);
			
			// Get ABR decision
			int decision = mAbrAdapter->makeAbrDecision(mCurrentProfile, context);
			return decision;
		}
#endif
		
		// Fallback: Simple heuristic if real ABR not available
		const double safetyMargin = 1.3; // Need 30% headroom
		double requiredBps = currentProfile->bitrateBps * safetyMargin;
		
		// Check buffer health
		if (mBuffer.getCurrentBuffer() < 5.0 && result.throughputBps < requiredBps) {
			// Ramp down if buffer is low and throughput insufficient
			return getRampedDownProfile(mCurrentProfile);
		} else if (mBuffer.getCurrentBuffer() > 15.0 && result.throughputBps > requiredBps * 1.5) {
			// Ramp up if buffer is healthy and throughput is abundant
			return getRampedUpProfile(mCurrentProfile);
		}
		
		return mCurrentProfile;
	}
	
	int getRampedDownProfile(int current) {
		for (int i = current - 1; i >= 0; --i) {
			if (mLadder.getProfile(i)) return i;
		}
		return current;
	}
	
	int getRampedUpProfile(int current) {
		for (size_t i = current + 1; i < mLadder.profiles.size(); ++i) {
			if (mLadder.getProfile(i)) return i;
		}
		return current;
	}
};

// =============================================================================
// Main Entry Point
// =============================================================================

void printUsage(const char* progName) {
	std::cout << "Usage: " << progName << " [options]\n"
	          << "Options:\n"
	          << "  --persona <file>      Network persona JSON file\n"
	          << "  --scenario <file>     Network scenario JSON file (multi-stage simulation)\n"
	          << "  --duration <secs>     Simulation duration in seconds (default: 3600)\n"
	          << "  --out <file>          Output CSV filename (default: abrsim.csv)\n"
	          << "  --seed <n>            Random seed (default: random)\n"
	          << "  --live                Enable live streaming mode (default: VOD)\n"
	          << "  --target-latency <s>  Target latency from live edge in seconds (default: 8.0)\n"
	          << "  --max-buffer <s>      Max buffer size in seconds for VOD mode (default: 20.0)\n"
	          << "  --help                Show this help\n"
	          << "\nExamples:\n"
	          << "  VOD:      " << progName << " --persona network.json --max-buffer 20 --duration 7200\n"
	          << "  Live:     " << progName << " --persona network.json --live --target-latency 8 --duration 3600\n"
	          << "  Scenario: " << progName << " --scenario degradation.json --duration 140\n";
}

int main(int argc, char* argv[]) {
	std::string personaFile;
	std::string scenarioFile;
	double durationS = 3600.0;
	std::string outFile = "abrsim.csv";
	uint64_t seed = 0;
	bool isLive = false;
	double targetLatencyS = 8.0;
	double maxBufferS = 20.0;
	
	// Parse command line arguments
	for (int i = 1; i < argc; ++i) {
		std::string arg = argv[i];
		if (arg == "--persona" && i + 1 < argc) {
			personaFile = argv[++i];
		} else if (arg == "--scenario" && i + 1 < argc) {
			scenarioFile = argv[++i];
		} else if (arg == "--duration" && i + 1 < argc) {
			durationS = std::stod(argv[++i]);
		} else if (arg == "--out" && i + 1 < argc) {
			outFile = argv[++i];
		} else if (arg == "--seed" && i + 1 < argc) {
			seed = std::stoull(argv[++i]);
		} else if (arg == "--live") {
			isLive = true;
		} else if (arg == "--target-latency" && i + 1 < argc) {
			targetLatencyS = std::stod(argv[++i]);
		} else if (arg == "--max-buffer" && i + 1 < argc) {
			maxBufferS = std::stod(argv[++i]);
		} else if (arg == "--help") {
			printUsage(argv[0]);
			return 0;
		} else {
			std::cerr << "Unknown option: " << arg << std::endl;
			printUsage(argv[0]);
			return 1;
		}
	}
	
	// Validate: must specify either persona OR scenario, not both
	if (personaFile.empty() && scenarioFile.empty()) {
		std::cerr << "Error: --persona or --scenario is required\n";
		printUsage(argv[0]);
		return 1;
	}
	
	if (!personaFile.empty() && !scenarioFile.empty()) {
		std::cerr << "Error: Cannot specify both --persona and --scenario\n";
		printUsage(argv[0]);
		return 1;
	}
	
	// Create typical DASH video profile ladder
	VideoProfileLadder ladder;
	ladder.segmentDurationS = 2.0;
	
	// Add profiles (typical HLS/DASH ladder)
	ladder.addProfile(0,  235000,   426,  240, 0, 0);  // 235 kbps
	ladder.addProfile(1,  375000,   640,  360, 0, 0);  // 375 kbps
	ladder.addProfile(2,  750000,   854,  480, 0, 0);  // 750 kbps
	ladder.addProfile(3, 1400000,  1280,  720, 0, 0);  // 1.4 Mbps
	ladder.addProfile(4, 2800000,  1920, 1080, 0, 0);  // 2.8 Mbps
	ladder.addProfile(5, 5000000,  1920, 1080, 0, 0);  // 5.0 Mbps
	ladder.addProfile(6, 8000000,  3840, 2160, 0, 0);  // 8.0 Mbps
	
	std::cout << "Created profile ladder with " << ladder.profiles.size() << " profiles\n";
	
	// Run simulation (either single persona or multi-stage scenario)
	if (!scenarioFile.empty()) {
		// Scenario mode: Load and run multi-stage simulation
		NetworkScenario scenario;
		if (!LoadScenario(scenarioFile, scenario)) {
			return 1;
		}
		
		std::cout << "Loaded scenario: " << scenario.description << " (" << scenario.stages.size() << " stages)\n";
		
		// Create combined event logger
		EventLogger combinedLogger;
		
		double elapsedTime = 0.0;
		int stageNum = 0;
		
		// Run each stage
		for (const auto& stage : scenario.stages) {
			stageNum++;
			std::cout << "\n=== Stage " << stageNum << "/" << scenario.stages.size() 
			          << ": " << stage.description << " (" << stage.durationS << "s) ===\n";
			
			// Load persona for this stage
			NetworkCharacteristics netChar;
			if (!LoadPersona(stage.personaFile, netChar)) {
				return 1;
			}
			
			std::cout << "  Network: " << netChar.mean_thr_mbps << " Mbps average\n";
			
			// Create simulator for this stage
			ABRSimulator stageSim(ladder, netChar, isLive, targetLatencyS, maxBufferS, seed);
			
			// Run this stage
			stageSim.run(stage.durationS);
			
			// Merge results into combined logger (with time offset)
			const auto& stageEvents = stageSim.getResults();
			for (auto event : stageEvents) {
				event.timeS += elapsedTime;  // Offset time for this stage
				combinedLogger.log(event);
			}
			
			elapsedTime += stage.durationS;
		}
		
		std::cout << "\n=== Scenario Complete ===\n";
		std::cout << "Total time: " << elapsedTime << "s\n";
		
		// Write combined results
		combinedLogger.writeCSV(outFile);
		combinedLogger.printSummary(elapsedTime);
		
	} else {
		// Single persona mode
		NetworkCharacteristics netChar;
		if (!LoadPersona(personaFile, netChar)) {
			return 1;
		}
		
		std::cout << "Loaded network persona: " << netChar.mean_thr_mbps << " Mbps average\n";
		
		ABRSimulator sim(ladder, netChar, isLive, targetLatencyS, maxBufferS, seed);
		sim.run(durationS);
		sim.generateReport(outFile);
	}
	
	return 0;
}
