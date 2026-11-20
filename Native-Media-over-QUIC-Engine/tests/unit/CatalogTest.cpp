#include "catalog/Catalog.h"

#include <gtest/gtest.h>

#include <fstream>
#include <sstream>

using nmqe::Catalog;

namespace {
std::string readFixture(const std::string &name)
{
	std::ifstream in(std::string(NMQE_FIXTURES) + "/" + name, std::ios::binary);
	std::stringstream text;
	text << in.rdbuf();
	return text.str();
}

std::optional<Catalog> parse(const std::string &text, std::string &error)
{
	return Catalog::parse(reinterpret_cast<const uint8_t *>(text.data()), text.size(), error);
}
} // namespace

TEST(Catalog, ParsesReferencePublication)
{
	std::string error;
	auto catalog = parse(readFixture("catalog/mediakind-test-20261120.json"), error);
	ASSERT_TRUE(catalog) << error;
	EXPECT_EQ(catalog->version, 1);
	EXPECT_FALSE(catalog->isDelta);
	ASSERT_EQ(catalog->tracks.size(), 4u);

	const auto *video = catalog->select("video", "");
	ASSERT_NE(video, nullptr);
	EXPECT_EQ(video->name, "video0.m4s");
	EXPECT_EQ(video->codec, "avc1.4d4020");
	EXPECT_EQ(video->width, 1280);
	EXPECT_EQ(video->height, 720);
	EXPECT_TRUE(video->isLive);
	EXPECT_EQ(video->targetLatencyMs, 500);
	EXPECT_EQ(video->renderGroup, 1);
	ASSERT_GE(video->initData.size(), 8u);
	EXPECT_EQ(std::string(video->initData.begin() + 4, video->initData.begin() + 8), "ftyp");
	EXPECT_TRUE(video->unknownFields.empty());

	const auto *audio = catalog->select("audio", "");
	ASSERT_NE(audio, nullptr);
	EXPECT_EQ(audio->name, "audio0.m4s");
	EXPECT_EQ(audio->samplerate, 48000);
	EXPECT_EQ(audio->altGroup, 1);
	EXPECT_EQ(audio->channelConfig, "2");

	EXPECT_EQ(catalog->select("audio", "audio1.m4s")->name, "audio1.m4s");
	const auto *timeline = catalog->find("sap-timeline");
	ASSERT_NE(timeline, nullptr);
	EXPECT_FALSE(timeline->isMedia());
}

TEST(Catalog, ReportsUnknownFields)
{
	std::string error;
	auto catalog = parse(R"({"version":1,"generatedAt":5,"tracks":[{"name":"v","packaging":"cmaf","role":"video","lang":"en"}]})", error);
	ASSERT_TRUE(catalog) << error;
	EXPECT_EQ(catalog->unknownFields, std::vector<std::string>({"generatedAt"}));
	EXPECT_EQ(catalog->tracks.front().unknownFields, std::vector<std::string>({"lang"}));
}

TEST(Catalog, FlagsDeltaUpdates)
{
	std::string error;
	auto catalog = parse(R"({"version":1,"deltaUpdate":true,"addTracks":[]})", error);
	ASSERT_TRUE(catalog);
	EXPECT_TRUE(catalog->isDelta);
	EXPECT_FALSE(error.empty());
}

TEST(Catalog, RejectsMalformedDocuments)
{
	std::string error;
	EXPECT_FALSE(parse("not json", error));
	EXPECT_FALSE(parse(R"({"version":1})", error));
	EXPECT_FALSE(parse(R"({"version":1,"tracks":[{"role":"video"}]})", error));
	EXPECT_FALSE(parse(R"({"version":1,"tracks":[{"name":"v","initData":"%%%"}]})", error));
}
