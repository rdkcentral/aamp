#pragma once

#include <cstdint>
#include <string>

namespace nmqe {

/**
 * Platform profile: candidate GStreamer elements and buffering properties for
 * one platform. Loaded from config/platform/<name>.json and logged with every
 * run. Element names are opaque strings above src/media/gst/.
 */
struct PlatformProfile
{
	std::string name = "macos";
	std::string videoDecoder;          // empty = let decodebin choose
	std::string audioDecoder;          // empty = let decodebin choose
	std::string videoSink = "autovideosink";
	std::string audioSink = "autoaudiosink";
	bool sinkSync = true;
	uint64_t appsrcMaxBytes = 4 * 1024 * 1024;
	int64_t sinkProcessingDeadlineMs = 20;
	int64_t pipelineLatencyMs = 0;     // 0 = GStreamer default
};

struct PlayerConfig
{
	// Endpoint and publication (see docs/integration-contract.md).
	std::string relayUrl;              // --relay or NMQE_RELAY_URL; required for live playback
	std::string moqtVersion = "moq-transport-22";
	std::string ns = "mediakind-test";
	std::string catalogTrack = "catalog";
	std::string videoTrack;            // empty = first video track in the catalog
	std::string audioTrack;            // empty = first audio track in the catalog

	// Object source.
	std::string replayPath;            // non-empty = replay a capture instead of the live relay
	std::string capturePath;           // non-empty = record received Objects
	bool replayRealtime = true;        // replay with the captured timing

	// Media pipeline: "gst" or "null".
	std::string pipeline = "gst";
	bool audioEnabled = true;
	bool videoEnabled = true;

	// Transport subscription parameters.
	uint32_t transportMaxDelayMs = 1000;

	// Bounded per-track Object queues (engineering defaults; tune from measurement).
	uint64_t queueMaxBytes = 16 * 1024 * 1024;
	int64_t queueMaxDurationMs = 6000;

	// Live-edge policy.
	int64_t targetLatencyMs = -1;      // -1 = catalog targetLatency, else this
	int64_t catchUpThresholdMs = 3000; // live-edge distance recorded as "behind" (no catch-up action yet)
	int64_t stableWindowMs = 4000;     // stall-free presentation for stable playback; live-edge floor window (>= 2 fragments)

	// Run control and observability.
	int64_t runDurationSec = 0;        // 0 = until stopped
	int64_t statusIntervalMs = 1000;
	std::string telemetryPath = "-";
	bool trace = false;
	std::string logLevel = "info";
	std::string transportLog;          // tracing filter passed to the MOQT library, empty = off

	PlatformProfile platform;

	/** Serializes the effective configuration for the run record. */
	std::string toJson() const;
};

/** Loads a platform profile JSON file into `profile`; returns false with `error` on failure. */
bool loadPlatformProfile(const std::string &path, PlatformProfile &profile, std::string &error);

} // namespace nmqe
