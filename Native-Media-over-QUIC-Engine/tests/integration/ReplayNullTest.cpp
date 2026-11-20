// Replay -> Player -> Null Media Pipeline: the captured reference publication
// tunes, joins at a random-access point, plays without stalls and produces the
// tune and live-edge telemetry the comparison relies on.

#include "common/Telemetry.h"
#include "player/Player.h"
#include "replay/Capture.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

using namespace nmqe;

namespace {
const std::string kCapture = std::string(NMQE_FIXTURES) + "/captures/reference.capture";

std::vector<std::string> lines(const std::string &path)
{
	std::ifstream in(path);
	std::vector<std::string> out;
	for (std::string line; std::getline(in, line);) out.push_back(line);
	return out;
}

bool contains(const std::vector<std::string> &all, const std::string &needle)
{
	return std::any_of(all.begin(), all.end(), [&](const std::string &line) { return line.find(needle) != std::string::npos; });
}
} // namespace

TEST(ReplayNull, PlaysReferenceCapture)
{
	if (!std::filesystem::exists(kCapture)) GTEST_SKIP() << "no reference capture at " << kCapture;
	const std::string telemetryPath = (std::filesystem::temp_directory_path() / "nmqe-replay-null.jsonl").string();
	std::filesystem::remove(telemetryPath);
	ASSERT_TRUE(Telemetry::instance().open(telemetryPath, "test", false));

	PlayerConfig config;
	config.pipeline = "null";
	config.runDurationSec = 9;
	config.replayPath = kCapture;
	auto player = Player::create(config, std::make_unique<ReplayObjectSource>(kCapture, true));
	player->tune();
	auto ended = player->ended();
	ASSERT_EQ(ended.wait_for(std::chrono::seconds(20)), std::future_status::ready);
	player->stop();
	EXPECT_EQ(ended.get(), Player::State::Stopped);
	player.reset();
	Telemetry::instance().close();

	const auto all = lines(telemetryPath);
	for (const char *milestone : {"catalog_received", "init_video", "init_audio", "join_video", "join_audio", "play_requested",
	                              "first_frame", "stable_playback"})
	{
		EXPECT_TRUE(contains(all, std::string("\"milestone\":\"") + milestone + "\"")) << milestone;
	}
	EXPECT_FALSE(contains(all, "\"ev\":\"parse_error\""));
	EXPECT_FALSE(contains(all, "\"cat\":\"stall\""));
	EXPECT_TRUE(contains(all, "\"cat\":\"summary\""));
	EXPECT_TRUE(contains(all, "\"stalls\":0"));
}
