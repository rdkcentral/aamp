#include "Config.h"

#include "Telemetry.h"

#include <cjson/cJSON.h>

#include <fstream>
#include <memory>
#include <sstream>
#include <type_traits>

namespace nmqe {

std::string PlayerConfig::toJson() const
{
	Json profile;
	profile.add("name", platform.name)
		.add("video_decoder", platform.videoDecoder)
		.add("audio_decoder", platform.audioDecoder)
		.add("video_sink", platform.videoSink)
		.add("audio_sink", platform.audioSink)
		.add("sink_sync", platform.sinkSync)
		.add("appsrc_max_bytes", platform.appsrcMaxBytes)
		.add("sink_processing_deadline_ms", platform.sinkProcessingDeadlineMs)
		.add("pipeline_latency_ms", platform.pipelineLatencyMs);
	Json config;
	config.add("relay_url", relayUrl)
		.add("moqt_version", moqtVersion)
		.add("namespace", ns)
		.add("catalog_track", catalogTrack)
		.add("video_track", videoTrack)
		.add("audio_track", audioTrack)
		.add("replay_path", replayPath)
		.add("capture_path", capturePath)
		.add("replay_realtime", replayRealtime)
		.add("pipeline", pipeline)
		.add("audio_enabled", audioEnabled)
		.add("video_enabled", videoEnabled)
		.add("transport_max_delay_ms", static_cast<uint64_t>(transportMaxDelayMs))
		.add("queue_max_bytes", queueMaxBytes)
		.add("queue_max_duration_ms", queueMaxDurationMs)
		.add("target_latency_ms", targetLatencyMs)
		.add("catch_up_threshold_ms", catchUpThresholdMs)
		.add("stable_window_ms", stableWindowMs)
		.add("run_duration_sec", runDurationSec)
		.add("status_interval_ms", statusIntervalMs)
		.add("trace", trace)
		.raw("platform", profile.object());
	return config.object();
}

bool loadPlatformProfile(const std::string &path, PlatformProfile &profile, std::string &error)
{
	std::ifstream file(path);
	if (!file)
	{
		error = "cannot open " + path;
		return false;
	}
	std::stringstream text;
	text << file.rdbuf();
	std::unique_ptr<cJSON, decltype(&cJSON_Delete)> root(cJSON_Parse(text.str().c_str()), cJSON_Delete);
	if (!root || !cJSON_IsObject(root.get()))
	{
		error = "invalid JSON in " + path;
		return false;
	}
	auto str = [&](const char *key, std::string &out) {
		const cJSON *item = cJSON_GetObjectItemCaseSensitive(root.get(), key);
		if (cJSON_IsString(item)) out = item->valuestring;
	};
	auto num = [&](const char *key, auto &out) {
		const cJSON *item = cJSON_GetObjectItemCaseSensitive(root.get(), key);
		if (cJSON_IsNumber(item)) out = static_cast<std::remove_reference_t<decltype(out)>>(item->valuedouble);
	};
	str("name", profile.name);
	str("video_decoder", profile.videoDecoder);
	str("audio_decoder", profile.audioDecoder);
	str("video_sink", profile.videoSink);
	str("audio_sink", profile.audioSink);
	if (const cJSON *sync = cJSON_GetObjectItemCaseSensitive(root.get(), "sink_sync"); cJSON_IsBool(sync))
	{
		profile.sinkSync = cJSON_IsTrue(sync);
	}
	num("appsrc_max_bytes", profile.appsrcMaxBytes);
	num("sink_processing_deadline_ms", profile.sinkProcessingDeadlineMs);
	num("pipeline_latency_ms", profile.pipelineLatencyMs);
	return true;
}

} // namespace nmqe
