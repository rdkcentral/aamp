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
 * TrimPeriod(body, startSec, endSec) keeps the segments covering [startSec, endSec).
 * Times are period-relative, mapped onto each timeline via its presentationTimeOffset.
 * Each edge snaps to the nearest boundary of the reference (video) track within
 * AAMP_VOD_SPLICE_SNAP_TOLERANCE_SEC, otherwise up to its next boundary, and every
 * track then splits at its own boundary nearest that common time, so audio and
 * video keep their relative timing.  A window that snaps to nothing yields an
 * empty result (no content period is emitted for it):
 *   - the content slice before a splice ends at that boundary (a segment
 *     straddling the splice beyond the window is kept, nothing after it);
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
#include "AampDefine.h"

namespace
{
constexpr uint64_t kTimescale = 48000;
constexpr double kSnapSec = AAMP_VOD_SPLICE_SNAP_TOLERANCE_SEC;

// 618 x 1.92s (-> 1186.56s), then 2.64s (-> 1189.2s), 1.2s (-> 1190.4s), 4 x 1.92s (-> 1198.08s)
std::string VideoTimeline(uint64_t t0 = 0)
{
	return "<S t=\"" + std::to_string(t0) + "\" d=\"92160\" r=\"617\"/><S d=\"126720\"/><S d=\"57600\"/><S d=\"92160\" r=\"3\"/>";
}
// 618 x 1.92s (-> 1186.56s), then 2.656s (-> 1189.216s), 1.184s (-> 1190.4s), 4 x 1.92s (-> 1198.08s)
std::string AudioTimeline(uint64_t t0 = 0)
{
	return "<S t=\"" + std::to_string(t0) + "\" d=\"92160\" r=\"617\"/><S d=\"127488\"/><S d=\"56832\"/><S d=\"92160\" r=\"3\"/>";
}

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

std::string AdaptationSet(const char *type, const std::string &templateAttrs, const std::string &timeline,
						  uint64_t timescale = kTimescale)
{
	return std::string("<AdaptationSet contentType=\"") + type + "\" mimeType=\"" + type + "/mp4\">\n"
		   "<SegmentTemplate timescale=\"" + std::to_string(timescale) + "\" " + templateAttrs + "media=\"" + type + "-$Number$.mp4\" initialization=\"" + type + "-init.mp4\">\n"
		   "<SegmentTimeline>\n" + timeline + "\n</SegmentTimeline>\n"
		   "</SegmentTemplate>\n"
		   "<Representation id=\"" + type + "\" bandwidth=\"1000\"/>\n"
		   "</AdaptationSet>\n";
}

/**
 * @brief Main-content period body: video with an explicit startNumber, audio relying on the default of 1.
 *        With a non-zero offset a track's timeline starts at t = presentationTimeOffset.
 */
std::string MainPeriodBody(uint64_t videoPto = 0, uint64_t audioPto = 0)
{
	auto ptoAttr = [](uint64_t pto) { return pto ? "presentationTimeOffset=\"" + std::to_string(pto) + "\" " : std::string(); };
	return AdaptationSet("video", "startNumber=\"1\" " + ptoAttr(videoPto), VideoTimeline(videoPto)) +
		   AdaptationSet("audio", ptoAttr(audioPto), AudioTimeline(audioPto));
}

std::vector<Track> TrimBody(const std::string &body, double startSec, double endSec, size_t expectedTracks = 2)
{
	std::vector<Track> tracks = ParseTracks(TrimPeriod(body, startSec, endSec));
	EXPECT_EQ(tracks.size(), expectedTracks);
	for (const Track &track : tracks)
		EXPECT_FALSE(track.segments.empty());
	return tracks;
}

std::vector<Track> Trim(double startSec, double endSec)
{
	return TrimBody(MainPeriodBody(), startSec, endSec);
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
 * @brief A splice shortly after a boundary (within the snap window) snaps back to it rather
 *        than playing most of the next segment before the ad.
 */
TEST(AampVodStitcherTrimPeriodTests, SnapWindow_JustAfterBoundary_SnapsBackToBoundary)
{
	for (double offset : {1.5 / (double)kTimescale, 0.01, 0.05, 0.9 * kSnapSec})
	{
		const double splice = 1189.2 + offset;
		SCOPED_TRACE(testing::Message() << "splice=" << std::to_string(splice));
		std::vector<Track> pre = Trim(0.0, splice);
		std::vector<Track> post = Trim(splice, 0.0);
		ASSERT_EQ(pre.size(), 2u);
		ASSERT_EQ(post.size(), 2u);
		EXPECT_EQ(pre[kVideo].segments.size(), 619u);
		EXPECT_EQ(pre[kVideo].End(), Ticks(1189.2));
		EXPECT_EQ(post[kVideo].Start(), Ticks(1189.2));
		EXPECT_EQ(pre[kAudio].End(), Ticks(1189.216));
		EXPECT_EQ(post[kAudio].Start(), Ticks(1189.216));
	}
}

/**
 * @brief A splice shortly before a boundary (within the snap window) snaps up to it.
 */
TEST(AampVodStitcherTrimPeriodTests, SnapWindow_JustBeforeBoundary_SnapsUpToBoundary)
{
	for (double offset : {0.01, 0.05, 0.9 * kSnapSec})
	{
		const double splice = 1189.2 - offset;
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
 * @brief A splice past a boundary by more than the snap window is mid-segment: the content
 *        between the boundary and the splice belongs before the ad, so the straddling
 *        segment is kept pre-splice.
 */
TEST(AampVodStitcherTrimPeriodTests, BeyondSnapWindow_KeepsStraddlingSegment)
{
	const double splice = 1189.2 + 1.5 * kSnapSec;
	std::vector<Track> pre = Trim(0.0, splice);
	std::vector<Track> post = Trim(splice, 0.0);
	ASSERT_EQ(pre.size(), 2u);
	ASSERT_EQ(post.size(), 2u);
	EXPECT_EQ(pre[kVideo].segments.size(), 620u);
	for (size_t i : {kVideo, kAudio})
	{
		EXPECT_EQ(pre[i].End(), Ticks(1190.4));
		EXPECT_EQ(post[i].Start(), Ticks(1190.4));
	}
}

/**
 * @brief With boundaries on both sides of the splice inside the snap window, the nearer one wins.
 */
TEST(AampVodStitcherTrimPeriodTests, SnapWindow_PrefersNearestBoundary)
{
	// timescale 1000: boundaries at 0, 1.000, 1.060, 2.060s
	const std::string body = AdaptationSet("video", "", "<S t=\"0\" d=\"1000\"/><S d=\"60\"/><S d=\"1000\"/>", 1000);
	ASSERT_LT(0.04, kSnapSec);

	std::vector<Track> nearerPrev = TrimBody(body, 0.0, 1.02, 1);
	ASSERT_EQ(nearerPrev.size(), 1u);
	EXPECT_EQ(nearerPrev[0].End(), 1000u);

	std::vector<Track> nearerNext = TrimBody(body, 0.0, 1.04, 1);
	ASSERT_EQ(nearerNext.size(), 1u);
	EXPECT_EQ(nearerNext[0].End(), 1060u);
}

/**
 * @brief Timelines whose t values start at a non-zero presentationTimeOffset (each track with
 *        its own offset) split at the same period-relative positions as zero-based ones, and
 *        each slice's offset is rebased to its first segment.
 */
TEST(AampVodStitcherTrimPeriodTests, NonZeroPresentationTimeOffset_SplitsAtSamePositions)
{
	const uint64_t videoPto = 100000ULL * kTimescale;
	const uint64_t audioPto = videoPto + 7ULL * kTimescale;
	const std::string body = MainPeriodBody(videoPto, audioPto);
	const uint64_t pto[] = {videoPto, audioPto};

	std::vector<Track> pre = TrimBody(body, 0.0, 1189.2);
	std::vector<Track> post = TrimBody(body, 1189.2, 0.0);
	ASSERT_EQ(pre.size(), 2u);
	ASSERT_EQ(post.size(), 2u);
	EXPECT_EQ(pre[kVideo].End(), videoPto + Ticks(1189.2));
	EXPECT_EQ(pre[kAudio].End(), audioPto + Ticks(1189.216));
	for (size_t i : {kVideo, kAudio})
	{
		SCOPED_TRACE(i == kVideo ? "video" : "audio");
		EXPECT_EQ(pre[i].segments.size(), 619u);
		EXPECT_EQ(pre[i].Start(), pto[i]);
		EXPECT_EQ(pre[i].pto, pto[i]);
		EXPECT_EQ(pre[i].startNumber, 1);
		EXPECT_EQ(post[i].Start(), pre[i].End());
		EXPECT_EQ(post[i].pto, post[i].Start());
		EXPECT_EQ(post[i].segments.size(), 5u);
		EXPECT_EQ(post[i].startNumber, 620);
	}

	std::vector<Track> middle = TrimBody(body, 1189.2, 1194.0);
	ASSERT_EQ(middle.size(), 2u);
	for (size_t i : {kVideo, kAudio})
	{
		EXPECT_EQ(middle[i].segments.size(), 3u);
		EXPECT_EQ(middle[i].End(), pto[i] + Ticks(1194.24));
	}
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

/**
 * @brief Near the snap threshold, where only audio has a boundary within the window
 *        (1189.31s: video 1189.2s is 110ms back, audio 1189.216s is 94ms back), both tracks
 *        must still split at the same content position as the video track.
 */
TEST(AampVodStitcherTrimPeriodTests, CoordinatedSnap_NearThreshold_TracksSplitTogether)
{
	const double splice = 1189.31;
	std::vector<Track> pre = Trim(0.0, splice);
	std::vector<Track> post = Trim(splice, 0.0);
	ASSERT_EQ(pre.size(), 2u);
	ASSERT_EQ(post.size(), 2u);
	for (size_t i : {kVideo, kAudio})
	{
		SCOPED_TRACE(i == kVideo ? "video" : "audio");
		EXPECT_EQ(pre[i].segments.size(), 620u);
		EXPECT_EQ(pre[i].End(), Ticks(1190.4));
		EXPECT_EQ(post[i].Start(), Ticks(1190.4));
		EXPECT_EQ(post[i].pto, Ticks(1190.4));
	}
}

/**
 * @brief Across a sweep of splice points around the boundaries, the post-splice audio and
 *        video resume within the snap window of each other (never a whole segment apart),
 *        and each track's slices stay contiguous.
 */
TEST(AampVodStitcherTrimPeriodTests, CoordinatedSnap_Sweep_AudioVideoResumeTogether)
{
	for (int ms = 1188900; ms <= 1190600; ms += 10)
	{
		const double splice = ms / 1000.0;
		SCOPED_TRACE(testing::Message() << "splice=" << std::to_string(splice));
		std::vector<Track> pre = Trim(0.0, splice);
		std::vector<Track> post = Trim(splice, 0.0);
		ASSERT_EQ(pre.size(), 2u);
		ASSERT_EQ(post.size(), 2u);
		const double videoResume = (double)post[kVideo].pto / (double)kTimescale;
		const double audioResume = (double)post[kAudio].pto / (double)kTimescale;
		EXPECT_LE(std::fabs(videoResume - audioResume), kSnapSec);
		for (size_t i : {kVideo, kAudio})
			EXPECT_EQ(pre[i].End(), post[i].Start());
	}
}

/**
 * @brief The reference track is the video track even when audio is listed first; a period
 *        without video uses its first timeline.
 */
TEST(AampVodStitcherTrimPeriodTests, SnapVodSpliceTime_PrefersVideoTrack)
{
	const std::string audioFirst = AdaptationSet("audio", "", AudioTimeline()) + AdaptationSet("video", "", VideoTimeline());
	EXPECT_NEAR(SnapVodSpliceTime(audioFirst, 1189.25), 1189.2, 1e-9);
	EXPECT_NEAR(SnapVodSpliceTime(audioFirst, 1189.31), 1190.4, 1e-9);

	const std::string audioOnly = AdaptationSet("audio", "", AudioTimeline());
	EXPECT_NEAR(SnapVodSpliceTime(audioOnly, 1189.25), 1189.216, 1e-9);
}

/**
 * @brief Video declaring its MIME type only on a Representation after the SegmentTemplate,
 *        listed after audio, is still the reference track.
 */
TEST(AampVodStitcherTrimPeriodTests, SnapVodSpliceTime_VideoMimeTypeOnRepresentation)
{
	const std::string body = AdaptationSet("audio", "", AudioTimeline()) +
		"<AdaptationSet>\n"
		"<SegmentTemplate timescale=\"48000\" media=\"video-$Number$.mp4\" initialization=\"video-init.mp4\">\n"
		"<SegmentTimeline>\n" + VideoTimeline() + "\n</SegmentTimeline>\n"
		"</SegmentTemplate>\n"
		"<Representation id=\"video\" mimeType=\"video/mp4\" bandwidth=\"1000\"/>\n"
		"</AdaptationSet>\n";
	EXPECT_NEAR(SnapVodSpliceTime(body, 1189.25), 1189.2, 1e-9);
	EXPECT_NEAR(SnapVodSpliceTime(body, 1189.31), 1190.4, 1e-9);
}

/**
 * @brief A trick-play (I-frame) video AdaptationSet listed first does not drive snapping.
 */
TEST(AampVodStitcherTrimPeriodTests, SnapVodSpliceTime_IgnoresTrickPlayVideo)
{
	const std::string body =
		"<AdaptationSet contentType=\"video\" mimeType=\"video/mp4\">\n"
		"<EssentialProperty schemeIdUri=\"http://dashif.org/guidelines/trickmode\" value=\"1\"/>\n"
		"<SegmentTemplate timescale=\"48000\" media=\"iframe-$Number$.mp4\" initialization=\"iframe-init.mp4\">\n"
		"<SegmentTimeline>\n" + AudioTimeline() + "\n</SegmentTimeline>\n"
		"</SegmentTemplate>\n"
		"<Representation id=\"iframe\" bandwidth=\"100\"/>\n"
		"</AdaptationSet>\n" +
		AdaptationSet("video", "", VideoTimeline());
	EXPECT_NEAR(SnapVodSpliceTime(body, 1189.25), 1189.2, 1e-9);
}

/**
 * @brief The resolved splice point is capped at the Period duration: with a 10s Period whose
 *        two 6s segments overrun its end, a postroll at 10s (or a splice snapping past the
 *        end) stays at 10s instead of moving to 12s, while the content slice still carries
 *        the overrunning segment and no remainder is left.
 */
TEST(AampVodStitcherTrimPeriodTests, ResolveVodSplicePoint_CappedAtPeriodDuration)
{
	const std::string body = AdaptationSet("video", "", "<S t=\"0\" d=\"6000\" r=\"1\"/>", 1000);
	const double periodDur = 10.0;

	EXPECT_NEAR(SnapVodSpliceTime(body, 10.0), 12.0, 1e-9);
	EXPECT_NEAR(ResolveVodSplicePoint(body, periodDur, 0.0, 10.0), 10.0, 1e-9);
	EXPECT_NEAR(ResolveVodSplicePoint(body, periodDur, 0.0, 9.95), 10.0, 1e-9);
	EXPECT_NEAR(ResolveVodSplicePoint(body, periodDur, 0.0, 5.95), 6.0, 1e-9);

	std::vector<Track> pre = TrimBody(body, 0.0, periodDur, 1);
	ASSERT_EQ(pre.size(), 1u);
	EXPECT_EQ(pre[0].segments.size(), 2u);
	EXPECT_TRUE(TrimPeriod(body, periodDur, 0.0).empty());
}

/**
 * @brief A postroll at the end of a 10s Period whose short last segment overruns it (boundaries
 *        at 9.950s and 10.150s) stays at the Period end instead of snapping back to 9.950s, and
 *        the content slice before it (trimmed to the end of the timeline) keeps the straddling
 *        segment, so no content is left to play after the postroll.  A midroll just before the
 *        end still snaps to the last boundary.
 */
TEST(AampVodStitcherTrimPeriodTests, ResolveVodSplicePoint_PostrollWithShortOverrunningLastSegment)
{
	const std::string body = AdaptationSet("video", "", "<S t=\"0\" d=\"1990\" r=\"4\"/><S d=\"200\"/>", 1000);
	const double periodDur = 10.0;

	EXPECT_NEAR(SnapVodSpliceTime(body, periodDur), 9.95, 1e-9);
	EXPECT_NEAR(ResolveVodSplicePoint(body, periodDur, 0.0, periodDur), periodDur, 1e-9);
	EXPECT_NEAR(ResolveVodSplicePoint(body, periodDur, 9.95, periodDur), periodDur, 1e-9);
	EXPECT_NEAR(ResolveVodSplicePoint(body, periodDur, 0.0, 9.96), 9.95, 1e-9);

	std::vector<Track> pre = TrimBody(body, 0.0, 0.0, 1);
	ASSERT_EQ(pre.size(), 1u);
	EXPECT_EQ(pre[0].segments.size(), 6u);
	EXPECT_EQ(pre[0].End(), 10150u);

	std::vector<Track> tail = TrimBody(body, 9.95, 0.0, 1);
	ASSERT_EQ(tail.size(), 1u);
	ASSERT_EQ(tail[0].segments.size(), 1u);
	EXPECT_EQ(tail[0].Start(), 9950u);
	EXPECT_EQ(tail[0].End(), 10150u);
}

/**
 * @brief The resolved splice point never moves before the content already emitted.
 */
TEST(AampVodStitcherTrimPeriodTests, ResolveVodSplicePoint_NeverBeforeCursor)
{
	const std::string body = MainPeriodBody();
	EXPECT_NEAR(ResolveVodSplicePoint(body, 1198.08, 1189.2, 1189.15), 1189.2, 1e-9);
	EXPECT_NEAR(ResolveVodSplicePoint(body, 1198.08, 1190.4, 1189.25), 1190.4, 1e-9);
}

/**
 * @brief A splice just after content start snaps to boundary 0: the pre-splice window is
 *        empty (no content period), and the post-splice slice starts at the first segment,
 *        so that segment is not emitted twice.
 */
TEST(AampVodStitcherTrimPeriodTests, NearZeroSplice_EmptyPreSlice_FirstSegmentNotDuplicated)
{
	for (double splice : {1e-6, 0.05, 0.9 * kSnapSec})
	{
		SCOPED_TRACE(testing::Message() << "splice=" << std::to_string(splice));
		EXPECT_NEAR(SnapVodSpliceTime(MainPeriodBody(), splice), 0.0, 1e-9);
		EXPECT_TRUE(TrimPeriod(MainPeriodBody(), 0.0, splice).empty());

		std::vector<Track> post = Trim(splice, 0.0);
		ASSERT_EQ(post.size(), 2u);
		for (const Track &track : post)
		{
			EXPECT_EQ(track.segments.size(), kSegmentCount);
			EXPECT_EQ(track.Start(), 0u);
			EXPECT_EQ(track.startNumber, 1);
		}
	}
}

/**
 * @brief A window starting at (or snapping to) the end of the timeline is empty, so no
 *        remainder period repeats the last segment after a postroll.
 */
TEST(AampVodStitcherTrimPeriodTests, SpliceAtTimelineEnd_EmptyRemainder)
{
	for (double splice : {1198.08, 1198.08 - 0.9 * kSnapSec, 2000.0})
	{
		SCOPED_TRACE(testing::Message() << "splice=" << std::to_string(splice));
		EXPECT_TRUE(TrimPeriod(MainPeriodBody(), splice, 0.0).empty());

		std::vector<Track> pre = Trim(0.0, splice);
		ASSERT_EQ(pre.size(), 2u);
		for (const Track &track : pre)
			EXPECT_EQ(track.segments.size(), kSegmentCount);
	}
}

/**
 * @brief Two insertion points that snap to the same boundary leave an empty content window
 *        between them.
 */
TEST(AampVodStitcherTrimPeriodTests, TwoSplicesSnappingToSameBoundary_EmptyMiddleSlice)
{
	EXPECT_TRUE(TrimPeriod(MainPeriodBody(), 1189.15, 1189.25).empty());
}

namespace
{
std::string WrapMpd(const std::string &periodBody, const std::string &periodAttrs = "")
{
	return "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
		   "<MPD xmlns=\"urn:mpeg:dash:schema:mpd:2011\" type=\"static\" mediaPresentationDuration=\"PT1198.08S\">\n"
		   "<Period id=\"p0\" start=\"PT0S\"" + periodAttrs + ">\n" + periodBody + "</Period>\n</MPD>\n";
}

std::string ReplaceFirst(std::string text, const std::string &from, const std::string &to)
{
	size_t pos = text.find(from);
	EXPECT_NE(pos, std::string::npos) << "fixture does not contain " << from;
	if (pos != std::string::npos)
		text.replace(pos, from.size(), to);
	return text;
}
} // namespace

/**
 * @brief A well-formed MPD can be stitched; an MPD without any Period cannot.
 */
TEST(AampVodStitcherTrimPeriodTests, CanStitchVodMpd_WellFormed)
{
	EXPECT_TRUE(CanStitchVodMpd(WrapMpd(MainPeriodBody())));
	EXPECT_TRUE(CanStitchVodMpd(WrapMpd(MainPeriodBody(100000ULL * kTimescale, 100007ULL * kTimescale))));
	EXPECT_FALSE(CanStitchVodMpd("<MPD type=\"static\" mediaPresentationDuration=\"PT10S\"></MPD>"));
	EXPECT_FALSE(CanStitchVodMpd(""));
}

/**
 * @brief Malformed numbers anywhere the stitcher parses them make the MPD rejected for stitching
 *        instead of escaping as an exception.
 */
TEST(AampVodStitcherTrimPeriodTests, CanStitchVodMpd_MalformedNumbers_ReturnsFalse)
{
	const std::string body = MainPeriodBody();
	const struct
	{
		const char *what;
		std::string mpd;
	} cases[] = {
		{"timescale not a number", WrapMpd(ReplaceFirst(body, "timescale=\"48000\"", "timescale=\"abc\""))},
		{"timescale zero", WrapMpd(ReplaceFirst(body, "timescale=\"48000\"", "timescale=\"0\""))},
		{"startNumber not a number", WrapMpd(ReplaceFirst(body, "startNumber=\"1\"", "startNumber=\"oops\""))},
		{"presentationTimeOffset not a number",
		 WrapMpd(ReplaceFirst(body, "startNumber=\"1\" ", "startNumber=\"1\" presentationTimeOffset=\"oops\" "))},
		{"S@t out of range", WrapMpd(ReplaceFirst(body, "t=\"0\"", "t=\"99999999999999999999999\""))},
		{"S@d not a number", WrapMpd(ReplaceFirst(body, "d=\"92160\"", "d=\"x\""))},
		{"S@r not a number", WrapMpd(ReplaceFirst(body, "r=\"617\"", "r=\"x\""))},
		{"Period@duration malformed", WrapMpd(body, " duration=\"PT.S\"")},
	};
	for (const auto &c : cases)
	{
		SCOPED_TRACE(c.what);
		EXPECT_NO_THROW(EXPECT_FALSE(CanStitchVodMpd(c.mpd)));
	}
}

/**
 * @brief Numeric attributes must be complete unsigned integers: trailing text and signs are
 *        rejected rather than partially parsed or wrapped around.
 */
TEST(AampVodStitcherTrimPeriodTests, CanStitchVodMpd_PartialOrSignedNumbers_ReturnsFalse)
{
	const std::string body = MainPeriodBody();
	const std::string withPto = ReplaceFirst(body, "startNumber=\"1\" ", "startNumber=\"1\" presentationTimeOffset=\"PTO\" ");
	const struct
	{
		const char *what;
		std::string mpd;
	} cases[] = {
		{"timescale trailing text", WrapMpd(ReplaceFirst(body, "timescale=\"48000\"", "timescale=\"48000junk\""))},
		{"timescale negative", WrapMpd(ReplaceFirst(body, "timescale=\"48000\"", "timescale=\"-1\""))},
		{"timescale plus sign", WrapMpd(ReplaceFirst(body, "timescale=\"48000\"", "timescale=\"+48000\""))},
		{"startNumber negative", WrapMpd(ReplaceFirst(body, "startNumber=\"1\"", "startNumber=\"-1\""))},
		{"startNumber trailing text", WrapMpd(ReplaceFirst(body, "startNumber=\"1\"", "startNumber=\"1x\""))},
		{"startNumber beyond int", WrapMpd(ReplaceFirst(body, "startNumber=\"1\"", "startNumber=\"4294967296\""))},
		{"presentationTimeOffset negative", WrapMpd(ReplaceFirst(withPto, "PTO", "-48000"))},
		{"presentationTimeOffset trailing text", WrapMpd(ReplaceFirst(withPto, "PTO", "0x"))},
		{"S@t negative", WrapMpd(ReplaceFirst(body, "t=\"0\"", "t=\"-1\""))},
		{"S@d trailing text", WrapMpd(ReplaceFirst(body, "d=\"92160\"", "d=\"92160junk\""))},
		{"S@d negative", WrapMpd(ReplaceFirst(body, "d=\"92160\"", "d=\"-92160\""))},
		{"S@d zero", WrapMpd(ReplaceFirst(body, "d=\"92160\"", "d=\"0\""))},
		{"S@r trailing text", WrapMpd(ReplaceFirst(body, "r=\"617\"", "r=\"617junk\""))},
		{"S@r negative", WrapMpd(ReplaceFirst(body, "r=\"617\"", "r=\"-2\""))},
		{"S@r -1 unsupported", WrapMpd(ReplaceFirst(body, "r=\"617\"", "r=\"-1\""))},
	};
	for (const auto &c : cases)
	{
		SCOPED_TRACE(c.what);
		EXPECT_NO_THROW(EXPECT_FALSE(CanStitchVodMpd(c.mpd)));
	}
}

/**
 * @brief Period and MPD durations must be complete ISO 8601 durations; a malformed one is
 *        rejected rather than ignored in favour of the SegmentTimeline span.
 */
TEST(AampVodStitcherTrimPeriodTests, CanStitchVodMpd_MalformedDurations_ReturnsFalse)
{
	const std::string body = MainPeriodBody();
	for (const char *duration : {"garbage", "1198.08", "PT", "P", "P1DT", "PT-5S", "-PT5S", "PT10Sjunk",
								 "PT5S10M", "PT1.5M", "PT5.S", "PT.5S", "P1Y", "P1M", "P1.5D", "PT1H1H"})
	{
		SCOPED_TRACE(duration);
		EXPECT_NO_THROW(EXPECT_FALSE(CanStitchVodMpd(WrapMpd(body, std::string(" duration=\"") + duration + "\""))));
		EXPECT_NO_THROW(EXPECT_FALSE(CanStitchVodMpd(ReplaceFirst(WrapMpd(body, " duration=\"PT1198.08S\""),
			"mediaPresentationDuration=\"PT1198.08S\"", std::string("mediaPresentationDuration=\"") + duration + "\""))));
	}
}

/**
 * @brief Well-formed ISO 8601 durations in their various spellings are accepted.
 */
TEST(AampVodStitcherTrimPeriodTests, CanStitchVodMpd_WellFormedDurations)
{
	const std::string body = MainPeriodBody();
	for (const char *duration : {"PT1198.08S", "PT19M58.08S", "PT0H19M58.080S", "P0Y0M0DT0H19M58.08S", "P0D",
								 "PT0S", "P1DT1H", "PT1H"})
	{
		SCOPED_TRACE(duration);
		EXPECT_TRUE(CanStitchVodMpd(WrapMpd(body, std::string(" duration=\"") + duration + "\"")));
	}
}

/**
 * @brief The lower-level helpers report malformed numbers by throwing; the stitcher catches
 *        them (see CanStitchVodMpd and BuildStitchedVodManifest).
 */
TEST(AampVodStitcherTrimPeriodTests, TrimPeriod_MalformedNumber_Throws)
{
	const std::string body = ReplaceFirst(MainPeriodBody(), "d=\"92160\"", "d=\"x\"");
	EXPECT_ANY_THROW(TrimPeriod(body, 0.0, 1189.2));
	EXPECT_ANY_THROW(SnapVodSpliceTime(body, 1189.2));
}
