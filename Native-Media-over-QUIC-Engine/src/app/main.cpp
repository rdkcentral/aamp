// nmqe-player: native MoQ player CLI. Tunes the reference publication (or a
// capture), plays it, reports status, and stops on signal or after --duration.

#include "common/Config.h"
#include "common/Log.h"
#include "common/Telemetry.h"
#include "media/api/MediaPipeline.h"
#include "player/Player.h"
#include "replay/Capture.h"
#include "transport/ObjectSource.h"

#include <atomic>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <type_traits>

namespace {

std::atomic<bool> gInterrupted{false};

void usage()
{
	fprintf(stderr,
	        "usage: nmqe-player [options]\n"
	        "  --relay URL               MOQT relay, https:// (WebTransport) or moqt:// (raw QUIC);\n"
	        "                            default: $NMQE_RELAY_URL\n"
	        "  --moqt-version V          e.g. moq-transport-22 (default)\n"
	        "  --namespace NS            broadcast namespace (default: mediakind-test)\n"
	        "  --catalog-track NAME      catalog track (default: catalog)\n"
	        "  --video-track NAME        override catalog video selection\n"
	        "  --audio-track NAME        override catalog audio selection\n"
	        "  --no-audio | --no-video   disable a medium\n"
	        "  --pipeline gst|null       media backend (default: gst)\n"
	        "  --platform FILE           platform profile JSON (config/platform/*.json)\n"
	        "  --replay FILE             play a capture instead of the live relay\n"
	        "  --replay-fast             replay without the captured pacing\n"
	        "  --capture FILE            record received Objects for replay\n"
	        "  --duration SEC            stop after SEC seconds (default: until Ctrl-C)\n"
	        "  --status-ms MS            status interval (default: 1000)\n"
	        "  --telemetry FILE|-        JSON Lines output (default: stdout)\n"
	        "  --trace                   per-object/per-frame telemetry\n"
	        "  --log LEVEL               trace|debug|info|warn|error|off (default: info)\n"
	        "  --transport-log FILTER    MOQT library tracing filter (e.g. info)\n"
	        "  --queue-max-bytes N       per-track Object queue cap\n"
	        "  --queue-max-ms N          per-track demuxed queue cap\n"
	        "  --max-delay-ms N          transport superseded-group budget\n"
	        "  --catch-up-threshold-ms N live-edge distance recorded as behind\n"
	        "  --stable-window-ms N      window for stable playback / live edge\n");
}

bool parse(int argc, char **argv, nmqe::PlayerConfig &config)
{
	for (int i = 1; i < argc; ++i)
	{
		const std::string arg = argv[i];
		auto value = [&](std::string &out) {
			if (i + 1 >= argc) return false;
			out = argv[++i];
			return true;
		};
		auto number = [&](auto &out) {
			std::string text;
			if (!value(text)) return false;
			char *end = nullptr;
			const long long parsed = strtoll(text.c_str(), &end, 10);
			if (!end || *end) return false;
			out = static_cast<std::remove_reference_t<decltype(out)>>(parsed);
			return true;
		};
		bool ok = true;
		if (arg == "--help" || arg == "-h") { usage(); exit(0); }
		else if (arg == "--relay") ok = value(config.relayUrl);
		else if (arg == "--moqt-version") ok = value(config.moqtVersion);
		else if (arg == "--namespace") ok = value(config.ns);
		else if (arg == "--catalog-track") ok = value(config.catalogTrack);
		else if (arg == "--video-track") ok = value(config.videoTrack);
		else if (arg == "--audio-track") ok = value(config.audioTrack);
		else if (arg == "--no-audio") config.audioEnabled = false;
		else if (arg == "--no-video") config.videoEnabled = false;
		else if (arg == "--pipeline") ok = value(config.pipeline);
		else if (arg == "--platform")
		{
			std::string path, error;
			ok = value(path) && nmqe::loadPlatformProfile(path, config.platform, error);
			if (!ok) fprintf(stderr, "platform profile: %s\n", error.c_str());
		}
		else if (arg == "--replay") ok = value(config.replayPath);
		else if (arg == "--replay-fast") config.replayRealTime = false;
		else if (arg == "--capture") ok = value(config.capturePath);
		else if (arg == "--duration") ok = number(config.runDurationSec);
		else if (arg == "--status-ms") ok = number(config.statusIntervalMs);
		else if (arg == "--telemetry") ok = value(config.telemetryPath);
		else if (arg == "--trace") config.trace = true;
		else if (arg == "--log") ok = value(config.logLevel);
		else if (arg == "--transport-log") ok = value(config.transportLog);
		else if (arg == "--queue-max-bytes") ok = number(config.queueMaxBytes);
		else if (arg == "--queue-max-ms") ok = number(config.queueMaxDurationMs);
		else if (arg == "--max-delay-ms") ok = number(config.transportMaxDelayMs);
		else if (arg == "--catch-up-threshold-ms") ok = number(config.catchUpThresholdMs);
		else if (arg == "--stable-window-ms") ok = number(config.stableWindowMs);
		else
		{
			fprintf(stderr, "unknown option: %s\n", arg.c_str());
			ok = false;
		}
		if (!ok)
		{
			fprintf(stderr, "invalid value for %s\n", arg.c_str());
			return false;
		}
	}
	if (config.pipeline != "gst" && config.pipeline != "null")
	{
		fprintf(stderr, "--pipeline must be gst or null\n");
		return false;
	}
	if (config.relayUrl.empty())
	{
		if (const char *env = getenv("NMQE_RELAY_URL")) config.relayUrl = env;
	}
	if (config.replayPath.empty() && config.relayUrl.empty())
	{
		fprintf(stderr, "no relay: pass --relay URL or set NMQE_RELAY_URL\n");
		return false;
	}
	return true;
}

int run(int argc, char **argv, void *)
{
	nmqe::PlayerConfig config;
	if (!parse(argc, argv, config))
	{
		usage();
		return 2;
	}
	nmqe::log::Level level;
	if (!nmqe::log::parseLevel(config.logLevel, level))
	{
		fprintf(stderr, "invalid --log level %s\n", config.logLevel.c_str());
		return 2;
	}
	nmqe::log::setLevel(level);

	auto &telemetry = nmqe::Telemetry::instance();
	if (!telemetry.open(config.telemetryPath, nmqe::Telemetry::makeRunId(), config.trace))
	{
		fprintf(stderr, "cannot open telemetry output %s\n", config.telemetryPath.c_str());
		return 2;
	}

	std::unique_ptr<nmqe::ObjectSource> source;
	if (!config.replayPath.empty())
	{
		source = std::make_unique<nmqe::ReplayObjectSource>(config.replayPath, config.replayRealTime);
	}
	else
	{
		nmqe::LiveSourceSettings live;
		live.url = config.relayUrl;
		live.version = config.moqtVersion;
		live.ns = config.ns;
		live.libraryLog = config.transportLog;
		source = nmqe::createLiveObjectSource(live);
	}

	std::shared_ptr<nmqe::CaptureWriter> capture;
	if (!config.capturePath.empty())
	{
		capture = std::make_shared<nmqe::CaptureWriter>();
		if (!capture->open(config.capturePath))
		{
			fprintf(stderr, "cannot open capture %s\n", config.capturePath.c_str());
			return 2;
		}
		source = std::make_unique<nmqe::RecordingObjectSource>(std::move(source), capture);
	}

	LOG_INFO("app", "run %s: %s", telemetry.runId().c_str(), source->describe().c_str());
	auto player = nmqe::Player::create(config, std::move(source));
	player->tune();

	auto ended = player->ended();
	while (!gInterrupted && ended.wait_for(std::chrono::milliseconds(100)) != std::future_status::ready)
	{
	}
	if (gInterrupted) LOG_INFO("app", "interrupted");
	player->stop();
	const nmqe::Player::State state = ended.wait_for(std::chrono::seconds(0)) == std::future_status::ready ? ended.get() : nmqe::Player::State::Stopped;
	player.reset();
	if (capture) capture->close();
	telemetry.close();
	return state == nmqe::Player::State::Failed ? 1 : 0;
}

} // namespace

int main(int argc, char **argv)
{
	std::signal(SIGINT, [](int) { gInterrupted = true; });
	std::signal(SIGTERM, [](int) { gInterrupted = true; });
	const int status = nmqe::media::runPlatformMain(argc, argv, &run, nullptr);
	// gst_macos_main does not return control to a run loop that would exit on its own.
	exit(status);
}
