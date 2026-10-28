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
 * @file AampVodStitcherTrimPeriodTests.cpp
 * @brief L1 tests for TrimPeriod(), which splits a main-content period at a VOD
 *        ad insertion point.
 *
 * Contract
 * --------
 * TrimPeriod(body, startSec, endSec) keeps the segments covering [startSec, endSec),
 * with each edge snapped up to the first segment boundary at or after it:
 *   - the content slice before a splice ends at the first boundary >= splice
 *     (a segment straddling the splice is kept, nothing after it);
 *   - the slice after the splice starts at that same boundary;
 *   so every segment is emitted exactly once across the slices (no extra
 *   main content before the ad, and no repeated content after it).
 *
 * The timelines mirror a field asset (timescale 48000, 1.92s segments, mid-roll
 * at 1189.2s): video has a boundary exactly at 1189.2s, audio straddles it
 * (boundary at 1189.216s).
 */

#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "AampVodStitcher.h"

namespace
{
constexpr uint64_t kTimescale = 48000;

// 618 x 1.92s (-> 1186.56s), then 2.64s (-> 1189.2s), 1.2s (-> 1190.4s), 4 x 1.92s (-> 1198.08s)
const char *kVideoTimeline =
	"<S t=\"0\" d=\"92160\" r=\"617\"/><S d=\"126720\"/><S d=\"57600\"/><S d=\"92160\" r=\"3\"/>";
// 618 x 1.92s (-> 1186.56s), then 2.656s (-> 1189.216s), 1.184s (-> 1190.4s), 4 x 1.92s (-> 1198.08s)
const char *kAudioTimeline =
	"<S t=\"0\" d=\"92160\" r=\"617\"/><S d=\"127488\"/><S d=\"56832\"/><S d=\"92160\" r=\"3\"/>";

constexpr size_t kSegmentCount = 624;
constexpr size_t kVideo = 0;
constexpr size_t kAudio = 1;

uint64_t Ticks(double sec)
{
	return (uint64_t)std::llround(sec * (double)kTimescale);
}

struct Track
{
	uint64_t pto = 0;
	long startNumber = 0;
	std::vector<std::pair<uint64_t, uint64_t>> segments; // (t, d)

	uint64_t Start() const { return segments.front().first; }
	uint64_t End() const { return segments.back().first + segments.back().second; }
};

std::string Attr(const std::string &element, const std::string &name)
{
	const std::string needle = " " + name + "=\"";
	size_t pos = element.find(needle);
	if (pos == std::string::npos)
		return {};
	pos += needle.size();
	return element.substr(pos, element.find('"', pos) - pos);
}

/**
 * @brief Parse every SegmentTemplate/SegmentTimeline in a period body, expanding r= repeats.
 */
std::vector<Track> ParseTracks(const std::string &body)
{
	std::vector<Track> tracks;
	size_t pos = 0;
	while ((pos = body.find("<SegmentTemplate ", pos)) != std::string::npos)
	{
		const size_t tagEnd = body.find('>', pos);
		const std::string tag = body.substr(pos, tagEnd - pos + 1);
		Track track;
		const std::string pto = Attr(tag, "presentationTimeOffset");
		const std::string startNumber = Attr(tag, "startNumber");
		track.pto = pto.empty() ? 0 : std::stoull(pto);
		track.startNumber = startNumber.empty() ? 1 : std::stol(startNumber);

		const size_t tlStart = body.find("<SegmentTimeline>", tagEnd);
		const size_t tlEnd = body.find("</SegmentTimeline>", tlStart);
		uint64_t t = 0;
		size_t s = tlStart;
		while ((s = body.find("<S ", s)) != std::string::npos && s < tlEnd)
		{
			const size_t sEnd = body.find('>', s);
			const std::string entry = body.substr(s, sEnd - s + 1);
			const std::string tStr = Attr(entry, "t");
			const std::string rStr = Attr(entry, "r");
			if (!tStr.empty())
				t = std::stoull(tStr);
			const uint64_t d = std::stoull(Attr(entry, "d"));
			const int r = rStr.empty() ? 0 : std::stoi(rStr);
			for (int i = 0; i <= r; i++, t += d)
				track.segments.emplace_back(t, d);
			s = sEnd;
		}
		tracks.push_back(track);
		pos = tagEnd;
	}
	return tracks;
}

std::string AdaptationSet(const char *type, const std::string &templateAttrs, const char *timeline)
{
	return std::string("<AdaptationSet contentType=\"") + type + "\" mimeType=\"" + type + "/mp4\">\n"
		   "<SegmentTemplate timescale=\"48000\" " + templateAttrs + "media=\"" + type + "-$Number$.mp4\" initialization=\"" + type + "-init.mp4\">\n"
		   "<SegmentTimeline>\n" + timeline + "\n</SegmentTimeline>\n"
		   "</SegmentTemplate>\n"
		   "<Representation id=\"" + type + "\" bandwidth=\"1000\"/>\n"
		   "</AdaptationSet>\n";
}

/**
 * @brief Main-content period body: video with an explicit startNumber, audio relying on the default of 1.
 */
std::string MainPeriodBody()
{
	return AdaptationSet("video", "startNumber=\"1\" ", kVideoTimeline) + AdaptationSet("audio", "", kAudioTimeline);
}

std::vector<Track> Trim(double startSec, double endSec)
{
	std::vector<Track> tracks = ParseTracks(TrimPeriod(MainPeriodBody(), startSec, endSec));
	EXPECT_EQ(tracks.size(), 2u);
	for (const Track &track : tracks)
		EXPECT_FALSE(track.segments.empty());
	return tracks;
}
} // namespace

/**
 * @brief Sanity check of the fixture: the input timelines expand as documented.
 */
TEST(AampVodStitcherTrimPeriodTests, Fixture_TimelinesExpandAsDocumented)
{
	std::vector<Track> tracks = ParseTracks(MainPeriodBody());
	ASSERT_EQ(tracks.size(), 2u);
	EXPECT_EQ(tracks[kVideo].segments.size(), kSegmentCount);
	EXPECT_EQ(tracks[kAudio].segments.size(), kSegmentCount);
	EXPECT_EQ(tracks[kVideo].segments[619].first, Ticks(1189.2));
	EXPECT_EQ(tracks[kAudio].segments[619].first, Ticks(1189.216));
	EXPECT_EQ(tracks[kVideo].End(), Ticks(1198.08));
	EXPECT_EQ(tracks[kAudio].End(), Ticks(1198.08));
}

/**
 * @brief No trim window (as used for ad periods) keeps every segment.
 */
TEST(AampVodStitcherTrimPeriodTests, NoWindow_KeepsWholeTimeline)
{
	std::vector<Track> tracks = Trim(0.0, 0.0);
	ASSERT_EQ(tracks.size(), 2u);
	for (const Track &track : tracks)
	{
		EXPECT_EQ(track.segments.size(), kSegmentCount);
		EXPECT_EQ(track.Start(), 0u);
		EXPECT_EQ(track.End(), Ticks(1198.08));
		EXPECT_EQ(track.startNumber, 1);
		EXPECT_EQ(track.pto, 0u);
	}
}

/**
 * @brief Regression: with the splice exactly on a segment boundary, the
 *        pre-splice slice must end at the splice, not carry the following segment
 *        (which played 1.2s of post-ad content before the ad).
 */
TEST(AampVodStitcherTrimPeriodTests, PreSplice_BoundaryAligned_EndsExactlyAtSplice)
{
	std::vector<Track> tracks = Trim(0.0, 1189.2);
	ASSERT_EQ(tracks.size(), 2u);
	const Track &video = tracks[kVideo];
	EXPECT_EQ(video.segments.size(), 619u);
	EXPECT_EQ(video.End(), Ticks(1189.2));
	EXPECT_EQ(video.Start(), 0u);
	EXPECT_EQ(video.startNumber, 1);
	EXPECT_EQ(video.pto, 0u);
}

/**
 * @brief With the splice inside a segment, the pre-splice slice keeps only the straddling
 *        segment and nothing after it.
 */
TEST(AampVodStitcherTrimPeriodTests, PreSplice_MidSegment_KeepsOnlyStraddlingSegment)
{
	std::vector<Track> tracks = Trim(0.0, 1189.2);
	ASSERT_EQ(tracks.size(), 2u);
	const Track &audio = tracks[kAudio];
	EXPECT_EQ(audio.segments.size(), 619u);
	EXPECT_LT(audio.segments.back().first, Ticks(1189.2));
	EXPECT_EQ(audio.End(), Ticks(1189.216));
}

/**
 * @brief The post-splice slice starts at the first boundary at or after the splice, with
 *        startNumber and presentationTimeOffset rebased to that segment.
 */
TEST(AampVodStitcherTrimPeriodTests, PostSplice_StartsAtFirstBoundaryAtOrAfterSplice)
{
	std::vector<Track> tracks = Trim(1189.2, 0.0);
	ASSERT_EQ(tracks.size(), 2u);
	EXPECT_EQ(tracks[kVideo].Start(), Ticks(1189.2));
	EXPECT_EQ(tracks[kVideo].pto, Ticks(1189.2));
	EXPECT_EQ(tracks[kAudio].Start(), Ticks(1189.216));
	EXPECT_EQ(tracks[kAudio].pto, Ticks(1189.216));
	for (const Track &track : tracks)
	{
		EXPECT_EQ(track.segments.size(), 5u);
		EXPECT_EQ(track.startNumber, 620);
		EXPECT_EQ(track.End(), Ticks(1198.08));
	}
}

/**
 * @brief Splices within a fraction of a tick of a boundary, including the value produced
 *        by the stitched-timeline arithmetic seen in the field (1329.275 - 140.075), snap
 *        to that boundary rather than to the next one.
 */
TEST(AampVodStitcherTrimPeriodTests, SubTickJitter_SnapsToBoundary)
{
	const double tick = 1.0 / (double)kTimescale;
	for (double splice : {1329.275 - 140.075, 1189.2 - 1e-9, 1189.2 + 1e-9, 1189.2 - 0.4 * tick, 1189.2 + 0.4 * tick})
	{
		SCOPED_TRACE(testing::Message() << "splice=" << std::to_string(splice));
		std::vector<Track> pre = Trim(0.0, splice);
		std::vector<Track> post = Trim(splice, 0.0);
		ASSERT_EQ(pre.size(), 2u);
		ASSERT_EQ(post.size(), 2u);
		EXPECT_EQ(pre[kVideo].segments.size(), 619u);
		EXPECT_EQ(pre[kVideo].End(), Ticks(1189.2));
		EXPECT_EQ(post[kVideo].Start(), Ticks(1189.2));
	}
}

/**
 * @brief A splice clearly past a boundary is mid-segment: the content between the boundary
 *        and the splice belongs before the ad, so the straddling segment is kept pre-splice.
 */
TEST(AampVodStitcherTrimPeriodTests, JustPastBoundary_KeepsStraddlingSegment)
{
	const double splice = 1189.2 + 1.5 / (double)kTimescale;
	std::vector<Track> pre = Trim(0.0, splice);
	std::vector<Track> post = Trim(splice, 0.0);
	ASSERT_EQ(pre.size(), 2u);
	ASSERT_EQ(post.size(), 2u);
	EXPECT_EQ(pre[kVideo].segments.size(), 620u);
	EXPECT_EQ(pre[kVideo].End(), Ticks(1190.4));
	EXPECT_EQ(post[kVideo].Start(), Ticks(1190.4));
}

/**
 * @brief A splice inside the first segment keeps just that segment before the ad.
 */
TEST(AampVodStitcherTrimPeriodTests, SpliceInsideFirstSegment_KeepsOnlyFirstSegment)
{
	std::vector<Track> pre = Trim(0.0, 1.0);
	std::vector<Track> post = Trim(1.0, 0.0);
	ASSERT_EQ(pre.size(), 2u);
	ASSERT_EQ(post.size(), 2u);
	for (size_t i : {kVideo, kAudio})
	{
		EXPECT_EQ(pre[i].segments.size(), 1u);
		EXPECT_EQ(pre[i].End(), Ticks(1.92));
		EXPECT_EQ(post[i].Start(), Ticks(1.92));
		EXPECT_EQ(post[i].startNumber, 2);
	}
}

/**
 * @brief A splice past the end of the timeline keeps every segment.
 */
TEST(AampVodStitcherTrimPeriodTests, SpliceBeyondEnd_KeepsWholeTimeline)
{
	std::vector<Track> tracks = Trim(0.0, 2000.0);
	ASSERT_EQ(tracks.size(), 2u);
	for (const Track &track : tracks)
	{
		EXPECT_EQ(track.segments.size(), kSegmentCount);
		EXPECT_EQ(track.End(), Ticks(1198.08));
	}
}

/**
 * @brief For splices on and between boundaries, the slices before and after the splice meet
 *        exactly: no gap, no overlap, and contiguous segment numbering.
 */
TEST(AampVodStitcherTrimPeriodTests, PreAndPostSplice_AreContiguous)
{
	for (double splice : {1.0, 1185.0, 1186.56, 1189.2, 1189.21, 1190.0, 1193.0})
	{
		SCOPED_TRACE(testing::Message() << "splice=" << splice);
		std::vector<Track> pre = Trim(0.0, splice);
		std::vector<Track> post = Trim(splice, 0.0);
		ASSERT_EQ(pre.size(), 2u);
		ASSERT_EQ(post.size(), 2u);
		for (size_t i : {kVideo, kAudio})
		{
			SCOPED_TRACE(i == kVideo ? "video" : "audio");
			EXPECT_EQ(pre[i].End(), post[i].Start());
			EXPECT_EQ(pre[i].segments.size() + post[i].segments.size(), kSegmentCount);
			EXPECT_EQ(post[i].startNumber, pre[i].startNumber + (long)pre[i].segments.size());
		}
	}
}

/**
 * @brief Splitting one period at two insertion points emits every original segment exactly
 *        once across the three content slices, in order.
 */
TEST(AampVodStitcherTrimPeriodTests, TwoSplices_EverySegmentEmittedExactlyOnce)
{
	const std::vector<Track> original = ParseTracks(MainPeriodBody());
	std::vector<Track> first = Trim(0.0, 1189.2);
	std::vector<Track> middle = Trim(1189.2, 1194.0);
	std::vector<Track> last = Trim(1194.0, 0.0);
	ASSERT_EQ(original.size(), 2u);
	ASSERT_EQ(first.size(), 2u);
	ASSERT_EQ(middle.size(), 2u);
	ASSERT_EQ(last.size(), 2u);
	for (size_t i : {kVideo, kAudio})
	{
		SCOPED_TRACE(i == kVideo ? "video" : "audio");
		std::vector<std::pair<uint64_t, uint64_t>> joined = first[i].segments;
		joined.insert(joined.end(), middle[i].segments.begin(), middle[i].segments.end());
		joined.insert(joined.end(), last[i].segments.begin(), last[i].segments.end());
		EXPECT_EQ(joined, original[i].segments);
		EXPECT_EQ(middle[i].startNumber, 1 + (long)first[i].segments.size());
		EXPECT_EQ(last[i].startNumber, middle[i].startNumber + (long)middle[i].segments.size());
		EXPECT_EQ(middle[i].pto, middle[i].Start());
		EXPECT_EQ(last[i].pto, last[i].Start());
	}
}
