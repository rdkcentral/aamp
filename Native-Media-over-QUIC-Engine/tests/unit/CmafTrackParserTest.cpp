#include "catalog/Catalog.h"
#include "mediaformat/CmafTrackParser.h"
#include "replay/Capture.h"

#include <gtest/gtest.h>

#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <sstream>

using namespace nmqe;

namespace {

Catalog referenceCatalog()
{
	std::ifstream in(std::string(NMQE_FIXTURES) + "/catalog/mediakind-test-20261120.json", std::ios::binary);
	std::stringstream text;
	text << in.rdbuf();
	const std::string json = text.str();
	std::string error;
	auto catalog = Catalog::parse(reinterpret_cast<const uint8_t *>(json.data()), json.size(), error);
	EXPECT_TRUE(catalog) << error;
	return catalog.value_or(Catalog());
}

/** Collects every Object of a capture, per track, without pacing. */
class Collector : public ObjectSourceListener
{
public:
	void onSourceEvent(const SourceEvent &event) override
	{
		if (event.kind != SourceEvent::Kind::Closed) return;
		std::lock_guard<std::mutex> lock(mutex);
		done = true;
		cv.notify_all();
	}
	void onObject(uint64_t, ReceivedObject object) override
	{
		std::lock_guard<std::mutex> lock(mutex);
		objects[object.id.track].push_back(std::move(object));
	}
	std::mutex mutex;
	std::condition_variable cv;
	bool done = false;
	std::map<std::string, std::vector<ReceivedObject>> objects;
};

const std::string kCapture = std::string(NMQE_FIXTURES) + "/captures/reference.capture";

std::vector<uint8_t> prftBox(uint8_t version, uint64_t ntp, uint64_t media)
{
	std::vector<uint8_t> box;
	auto u32 = [&](uint32_t v) {
		for (int s = 24; s >= 0; s -= 8) box.push_back(static_cast<uint8_t>(v >> s));
	};
	u32(version ? 32 : 28);
	for (char c : std::string("prft")) box.push_back(static_cast<uint8_t>(c));
	u32(static_cast<uint32_t>(version) << 24 | 0x000008);
	u32(1);
	u32(static_cast<uint32_t>(ntp >> 32));
	u32(static_cast<uint32_t>(ntp));
	if (version) u32(static_cast<uint32_t>(media >> 32));
	u32(static_cast<uint32_t>(media));
	return box;
}

} // namespace

TEST(CmafTrackParser, VideoHeaderFromCatalog)
{
	const Catalog catalog = referenceCatalog();
	const CatalogTrack *video = catalog.select("video", "");
	ASSERT_NE(video, nullptr);
	CmafTrackParser parser(video->name, media::MediaSourceType::VIDEO);
	std::string error;
	ASSERT_TRUE(parser.setInit(video->initData, error)) << error;
	EXPECT_EQ(parser.source().mimeType, "video/h264");
	EXPECT_EQ(parser.source().streamFormat, media::StreamFormat::AVC);
	EXPECT_EQ(parser.source().width, 1280u);
	EXPECT_EQ(parser.source().height, 720u);
	EXPECT_EQ(parser.timescale(), 90000u);
	ASSERT_FALSE(parser.source().codecData.empty());
	EXPECT_EQ(parser.source().codecData.front(), 1); // AVCDecoderConfigurationRecord version
}

TEST(CmafTrackParser, AudioHeaderFromCatalog)
{
	const Catalog catalog = referenceCatalog();
	const CatalogTrack *audio = catalog.select("audio", "");
	ASSERT_NE(audio, nullptr);
	CmafTrackParser parser(audio->name, media::MediaSourceType::AUDIO);
	std::string error;
	ASSERT_TRUE(parser.setInit(audio->initData, error)) << error;
	EXPECT_EQ(parser.source().mimeType, "audio/mp4");
	EXPECT_EQ(parser.source().sampleRate, 48000u);
	EXPECT_EQ(parser.source().numberOfChannels, 2u);
	ASSERT_GE(parser.source().codecData.size(), 2u);
	EXPECT_EQ(parser.source().codecData.front() >> 3, 2); // AudioSpecificConfig audioObjectType 2 = AAC-LC
}

TEST(CmafTrackParser, RejectsFragmentWithoutHeader)
{
	CmafTrackParser parser("video0.m4s", media::MediaSourceType::VIDEO);
	CmafFragment fragment;
	std::string error;
	auto junk = std::make_shared<const ByteBuffer>(ByteBuffer{0, 0, 0, 8, 'm', 'o', 'o', 'f'});
	EXPECT_FALSE(parser.parseFragment(junk, fragment, error));
}

TEST(CmafTrackParser, ParsesProducerReferenceTime)
{
	// 2026-11-20T00:00:00Z as NTP seconds, plus half a second of fraction.
	const uint64_t ntpSeconds = 1795046400ULL + 2208988800ULL;
	const uint64_t ntp = (ntpSeconds << 32) | 0x80000000ULL;
	for (uint8_t version : {0, 1})
	{
		auto box = prftBox(version, ntp, 123456);
		auto prft = CmafTrackParser::findPrft(box.data(), box.size());
		ASSERT_TRUE(prft);
		EXPECT_EQ(prft->referenceTrackId, 1u);
		EXPECT_EQ(prft->mediaTime, 123456u);
		EXPECT_EQ(prft->flags, 8u);
		EXPECT_EQ(prft->wallMs(), 1795046400500LL);
	}
	EXPECT_FALSE(CmafTrackParser::findPrft(nullptr, 0));
}

TEST(CmafTrackParser, ReferenceFragmentsHaveContinuousTiming)
{
	if (!std::filesystem::exists(kCapture)) GTEST_SKIP() << "no reference capture at " << kCapture;
	Collector collector;
	ReplayObjectSource replay(kCapture, false);
	ASSERT_TRUE(replay.start(&collector));
	for (const char *track : {"video0.m4s", "audio0.m4s"})
	{
		TrackSubscription subscription;
		subscription.track = track;
		replay.subscribe(subscription);
	}
	{
		std::unique_lock<std::mutex> lock(collector.mutex);
		collector.cv.wait_for(lock, std::chrono::seconds(10), [&] { return collector.done; });
	}
	replay.stop();

	const Catalog catalog = referenceCatalog();
	for (const auto &[name, type] : {std::pair{"video0.m4s", media::MediaSourceType::VIDEO}, std::pair{"audio0.m4s", media::MediaSourceType::AUDIO}})
	{
		const auto &objects = collector.objects[name];
		ASSERT_GE(objects.size(), 3u) << name;
		CmafTrackParser parser(name, type);
		std::string error;
		ASSERT_TRUE(parser.setInit(catalog.find(name)->initData, error)) << error;
		int64_t previousEnd = 0;
		uint64_t previousGroup = 0;
		for (const auto &object : objects)
		{
			CmafFragment fragment;
			ASSERT_TRUE(parser.parseFragment(object.payload, fragment, error)) << name << " group " << object.id.group << ": " << error;
			ASSERT_FALSE(fragment.segments.empty());
			// Each Group is one CMAF fragment that starts at a random-access point.
			EXPECT_EQ(object.id.object, 0u);
			EXPECT_TRUE(fragment.startsWithKeyFrame) << name << " group " << object.id.group;
			if (previousGroup)
			{
				EXPECT_EQ(object.id.group, previousGroup + 1) << name;
				// Decode timeline is contiguous across fragments (to within rounding of one tick).
				EXPECT_NEAR(static_cast<double>(fragment.segments.front().decodeTimeStampNs), static_cast<double>(previousEnd), 1e5) << name;
			}
			const auto &last = fragment.segments.back();
			previousEnd = last.decodeTimeStampNs + last.durationNs;
			previousGroup = object.id.group;
			for (const auto &au : fragment.segments)
			{
				EXPECT_GT(au.size, 0u);
				EXPECT_GT(au.durationNs, 0);
			}
		}
	}
}
