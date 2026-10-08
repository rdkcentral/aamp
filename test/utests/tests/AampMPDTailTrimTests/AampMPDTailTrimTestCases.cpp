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

#include <gtest/gtest.h>
#include "AampMPDDownloader.h"
#include "AampMPDTailTrim.h"
#include "AampDefine.h"
#include "AampConfig.h"

AampConfig *gpGlobalConfig{nullptr};

/**
 * @brief Parses a manifest through the production path with the trim disabled, so a test can trim it explicitly.
 */
static ManifestDownloadResponsePtr ParseManifest(const std::string &manifest)
{
	ManifestDownloadResponsePtr response = MakeSharedManifestDownloadResponsePtr();
	response->mMPDDownloadResponse->mDownloadData.assign(manifest.begin(), manifest.end());
	response->mMPDDownloadResponse->sEffectiveUrl = "http://host/asset/manifest.mpd";
	response->parseMPD();
	return response;
}

/**
 * @brief Builds a two-track MPD (audio then video, each at AdaptationSet level) for Period-tail trimming tests.
 *        p1 starts at PT10S; p0's @duration comes from period0Attrs and may be omitted.
 */
static std::string TailTrimManifest(const std::string &period0Attrs, const std::string &audioTimeline,
									const std::string &videoTimeline = "<S t=\"0\" d=\"2000\" r=\"4\" />",
									const std::string &templateAttrs = "")
{
	auto adaptationSet = [&](const char *contentType, const std::string &timeline)
	{
		return std::string("<AdaptationSet contentType=\"") + contentType + "\" mimeType=\"" + contentType + "/mp4\">"
			   "<SegmentTemplate timescale=\"1000\" " + templateAttrs + " initialization=\"init.mp4\" media=\"$Time$.m4s\">"
			   "<SegmentTimeline>" + timeline + "</SegmentTimeline></SegmentTemplate>"
			   "<Representation id=\"1\" bandwidth=\"1000\" codecs=\"x\"/></AdaptationSet>";
	};
	return "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
		   "<MPD xmlns=\"urn:mpeg:dash:schema:mpd:2011\" type=\"static\" mediaPresentationDuration=\"PT20S\">"
		   "<Period id=\"p0\" start=\"PT0S\" " + period0Attrs + ">" + adaptationSet("audio", audioTimeline) + adaptationSet("video", videoTimeline) + "</Period>"
		   "<Period id=\"p1\" start=\"PT10S\">" + adaptationSet("audio", "<S t=\"0\" d=\"2000\" r=\"4\" />") + "</Period></MPD>";
}

static std::vector<ITimeline *> &TimelinesOf(dash::mpd::IMPD *mpd, int periodIndex, int adaptationSetIndex)
{
	return mpd->GetPeriods().at(periodIndex)->GetAdaptationSets().at(adaptationSetIndex)->GetSegmentTemplate()->GetSegmentTimeline()->GetTimelines();
}

static std::vector<DroppedSegment> TrimTail(dash::mpd::IMPD *mpd)
{
	return TrimPeriodTailSegments(mpd, AAMP_DASH_PERIOD_TAIL_START_TOLERANCE_SEC, AAMP_DASH_PERIOD_TAIL_MIN_OVERHANG_SEC);
}

/**
 * @brief An ad-splicer overhang (starts 0.2s before the Period end, runs 3s past it) is removed from the
 *        audio timeline; the video timeline, which fits the Period, is untouched.
 */
TEST(AampMPDTailTrimTests, TrimPeriodTail_SplicerOverhang_DropsOverhangOnly)
{
	ManifestDownloadResponsePtr response = ParseManifest(TailTrimManifest("duration=\"PT10S\"",
		"<S t=\"0\" d=\"2000\" r=\"3\" /><S t=\"8000\" d=\"1800\" /><S t=\"9800\" d=\"3200\" />"));
	dash::mpd::IMPD *mpd = response->mMPDInstance.get();
	ASSERT_NE(mpd, nullptr);

	auto &audio = TimelinesOf(mpd, 0, 0);
	ASSERT_EQ(audio.size(), 3u);

	TrimTail(mpd);

	ASSERT_EQ(audio.size(), 2u);
	EXPECT_EQ(audio.back()->GetStartTime(), 8000u);
	auto &video = TimelinesOf(mpd, 0, 1);
	ASSERT_EQ(video.size(), 1u);
	EXPECT_EQ(video.front()->GetRepeatCount(), 4u);
}

/**
 * @brief Expected effect of the Period-tail trim on the audio timeline of a two-track Period.
 */
struct TrimCase
{
	const char *name;
	std::string period0Attrs;
	std::string audioTimeline;
	std::string videoTimeline;
	std::string templateAttrs;
	size_t expectedAudioEntries;
	uint32_t expectedLastAudioRepeat;
};

static const char *kVideoFitsPeriod = "<S t=\"0\" d=\"2000\" r=\"4\" />";

class TrimPeriodTailCasesTest : public ::testing::TestWithParam<TrimCase>
{
};

/**
 * @brief The audio timeline has the expected number of entries and last-entry repeat count after the trim.
 */
TEST_P(TrimPeriodTailCasesTest, TrimPeriodTail_AudioTimeline_MatchesExpectedEntries)
{
	const TrimCase &c = GetParam();
	ManifestDownloadResponsePtr response = ParseManifest(TailTrimManifest(c.period0Attrs, c.audioTimeline, c.videoTimeline, c.templateAttrs));
	dash::mpd::IMPD *mpd = response->mMPDInstance.get();
	ASSERT_NE(mpd, nullptr);

	TrimTail(mpd);
	auto &audio = TimelinesOf(mpd, 0, 0);
	ASSERT_EQ(audio.size(), c.expectedAudioEntries);
	EXPECT_EQ(audio.back()->GetRepeatCount(), c.expectedLastAudioRepeat);
}

INSTANTIATE_TEST_SUITE_P(TrimPeriodTail, TrimPeriodTailCasesTest, ::testing::Values(
	// A valid tiny final segment (starts 0.2s before the end, finishes only 0.1s after) is kept
	TrimCase{"KeepsTinyValidFinalSegment", "duration=\"PT10S\"",
		"<S t=\"0\" d=\"2000\" r=\"3\" /><S t=\"8000\" d=\"1800\" /><S t=\"9800\" d=\"300\" />", kVideoFitsPeriod, "", 3, 0},
	// Only the repeat that starts at the Period end is removed from a repeated <S>
	TrimCase{"DropsOneRepeatOfRepeatedEntry", "duration=\"PT10S\"",
		"<S t=\"0\" d=\"2000\" r=\"5\" />", kVideoFitsPeriod, "", 1, 4},
	// The explicit t after a gap decides the start, not the accumulated durations
	TrimCase{"UsesExplicitStartAfterGap", "duration=\"PT10S\"",
		"<S t=\"0\" d=\"2000\" r=\"3\" /><S t=\"9800\" d=\"3200\" />", kVideoFitsPeriod, "", 1, 3},
	// An end inferred from the next Period's @start is not used
	TrimCase{"NextPeriodStartAloneIsUntouched", "",
		"<S t=\"0\" d=\"2000\" r=\"3\" /><S t=\"8000\" d=\"1800\" /><S t=\"9800\" d=\"3200\" />", kVideoFitsPeriod, "", 3, 0},
	// A segment that overruns the Period but starts before the tolerance window is kept
	TrimCase{"KeepsOverrunStartingBeforeTolerance", "duration=\"PT10S\"",
		"<S t=\"0\" d=\"2000\" r=\"3\" /><S t=\"8000\" d=\"2500\" />", kVideoFitsPeriod, "", 2, 0},
	// Starts are measured from presentationTimeOffset
	TrimCase{"PresentationTimeOffsetRespected", "duration=\"PT10S\"",
		"<S t=\"5000\" d=\"2000\" r=\"3\" /><S t=\"13000\" d=\"1800\" /><S t=\"14800\" d=\"3200\" />", "<S t=\"5000\" d=\"2000\" r=\"4\" />",
		"presentationTimeOffset=\"5000\"", 2, 0},
	// The timeline is never emptied, even if its only segment qualifies
	TrimCase{"NeverEmptiesTimeline", "duration=\"PT10S\"",
		"<S t=\"9800\" d=\"3200\" />", kVideoFitsPeriod, "", 1, 0},
	// Several trailing segments that qualify are all removed
	TrimCase{"DropsMultipleTrailingSegments", "duration=\"PT10S\"",
		"<S t=\"0\" d=\"2000\" r=\"3\" /><S t=\"9800\" d=\"3200\" /><S t=\"13000\" d=\"3200\" />", kVideoFitsPeriod, "", 1, 3}),
	[](const ::testing::TestParamInfo<TrimCase> &info) { return std::string(info.param.name); });

/**
 * @brief A negative repeat count (repeat to the end of the Period) is unsupported: nothing is edited or reported.
 */
TEST(AampMPDTailTrimTests, TrimPeriodTail_RepeatToEnd_UntouchedAndNothingReported)
{
	ManifestDownloadResponsePtr response = ParseManifest(TailTrimManifest("duration=\"PT10S\"", "<S t=\"0\" d=\"2000\" r=\"-1\" />"));
	dash::mpd::IMPD *mpd = response->mMPDInstance.get();
	ASSERT_NE(mpd, nullptr);

	auto &audio = TimelinesOf(mpd, 0, 0);
	ASSERT_EQ(audio.size(), 1u);
	const uint32_t repeatBefore = audio.front()->GetRepeatCount();

	const std::vector<DroppedSegment> dropped = TrimTail(mpd);

	EXPECT_TRUE(dropped.empty());
	ASSERT_EQ(audio.size(), 1u);
	EXPECT_EQ(audio.front()->GetRepeatCount(), repeatBefore);
}

static const char *kRepresentationLevelTimelineManifest =
	"<?xml version=\"1.0\" encoding=\"utf-8\"?>"
	"<MPD xmlns=\"urn:mpeg:dash:schema:mpd:2011\" type=\"static\" mediaPresentationDuration=\"PT10S\">"
	"<Period id=\"p0\" start=\"PT0S\" duration=\"PT10S\"><AdaptationSet contentType=\"video\" mimeType=\"video/mp4\">"
	"<Representation id=\"1\" bandwidth=\"1000\" codecs=\"x\"><SegmentTemplate timescale=\"1000\" initialization=\"a.mp4\" media=\"$Time$.m4s\">"
	"<SegmentTimeline><S t=\"0\" d=\"2000\" r=\"3\" /><S t=\"9800\" d=\"3200\" /></SegmentTimeline></SegmentTemplate></Representation>"
	"<Representation id=\"2\" bandwidth=\"2000\" codecs=\"x\"><SegmentTemplate timescale=\"1000\" initialization=\"b.mp4\" media=\"$Time$.m4s\">"
	"<SegmentTimeline><S t=\"0\" d=\"2000\" r=\"3\" /><S t=\"9800\" d=\"3200\" /></SegmentTimeline></SegmentTemplate></Representation>"
	"</AdaptationSet></Period></MPD>";

static const char *kSharedAdaptationSetTimelineManifest =
	"<?xml version=\"1.0\" encoding=\"utf-8\"?>"
	"<MPD xmlns=\"urn:mpeg:dash:schema:mpd:2011\" type=\"static\" mediaPresentationDuration=\"PT10S\">"
	"<Period id=\"p0\" start=\"PT0S\" duration=\"PT10S\"><AdaptationSet contentType=\"video\" mimeType=\"video/mp4\">"
	"<SegmentTemplate timescale=\"1000\" initialization=\"a.mp4\" media=\"$Time$.m4s\">"
	"<SegmentTimeline><S t=\"0\" d=\"2000\" r=\"3\" /><S t=\"9800\" d=\"3200\" /></SegmentTimeline></SegmentTemplate>"
	"<Representation id=\"1\" bandwidth=\"1000\" codecs=\"x\"/><Representation id=\"2\" bandwidth=\"2000\" codecs=\"x\"/>"
	"</AdaptationSet></Period></MPD>";

/**
 * @brief A SegmentTimeline on each Representation is trimmed separately.
 */
TEST(AampMPDTailTrimTests, TrimPeriodTail_RepresentationLevelTimelines_EachTrimmed)
{
	ManifestDownloadResponsePtr response = ParseManifest(kRepresentationLevelTimelineManifest);
	dash::mpd::IMPD *mpd = response->mMPDInstance.get();
	ASSERT_NE(mpd, nullptr);

	const auto &representations = mpd->GetPeriods().at(0)->GetAdaptationSets().at(0)->GetRepresentation();
	ASSERT_EQ(representations.size(), 2u);
	EXPECT_EQ(representations.at(0)->GetSegmentTemplate()->GetSegmentTimeline()->GetTimelines().size(), 2u);

	EXPECT_EQ(TrimTail(mpd).size(), 2u);

	for (const auto *representation : representations)
	{
		EXPECT_EQ(representation->GetSegmentTemplate()->GetSegmentTimeline()->GetTimelines().size(), 1u);
	}
}

/**
 * @brief An AdaptationSet-level timeline shared by several Representations is trimmed.
 */
TEST(AampMPDTailTrimTests, TrimPeriodTail_SharedAdaptationSetTimeline_Trimmed)
{
	ManifestDownloadResponsePtr response = ParseManifest(kSharedAdaptationSetTimelineManifest);
	dash::mpd::IMPD *mpd = response->mMPDInstance.get();
	ASSERT_NE(mpd, nullptr);

	EXPECT_EQ(TimelinesOf(mpd, 0, 0).size(), 2u);
	EXPECT_EQ(TrimTail(mpd).size(), 1u);
	EXPECT_EQ(TimelinesOf(mpd, 0, 0).size(), 1u);
}

/**
 * @brief The trim describes the segment it removed so that the caller can report it.
 */
TEST(AampMPDTailTrimTests, TrimPeriodTail_OverrunningSegment_ReturnsItsDetails)
{
	ManifestDownloadResponsePtr response = ParseManifest(TailTrimManifest("duration=\"PT10S\"",
		"<S t=\"0\" d=\"2000\" r=\"3\" /><S t=\"8000\" d=\"1800\" /><S t=\"9800\" d=\"3200\" />"));
	dash::mpd::IMPD *mpd = response->mMPDInstance.get();
	ASSERT_NE(mpd, nullptr);

	const std::vector<DroppedSegment> dropped = TrimTail(mpd);

	ASSERT_EQ(dropped.size(), 1u);
	EXPECT_EQ(dropped[0].periodId, "p0");
	EXPECT_EQ(dropped[0].startTicks, 9800u);
	EXPECT_EQ(dropped[0].durationTicks, 3200u);
	EXPECT_EQ(dropped[0].timeScale, 1000u);
	EXPECT_DOUBLE_EQ(dropped[0].startSec, 9.8);
	EXPECT_DOUBLE_EQ(dropped[0].endSec, 13.0);
	EXPECT_DOUBLE_EQ(dropped[0].periodEndSec, 10.0);
}

static DroppedSegment MakeDropped(const char *periodId, uint64_t startTicks)
{
	return DroppedSegment{periodId, startTicks, 3200, 1000, 0, startTicks / 1000.0, startTicks / 1000.0 + 3.2, 10.0};
}

/**
 * @brief A segment is reported on the first parse that drops it and not again while later parses keep dropping it.
 */
TEST(TailDropTrackerTests, Update_RepeatedDrop_ReportedOnce)
{
	TailDropTracker tracker;
	EXPECT_EQ(tracker.Update({MakeDropped("p0", 9800)}).size(), 1u);
	EXPECT_TRUE(tracker.Update({MakeDropped("p0", 9800)}).empty());
	EXPECT_TRUE(tracker.Update({MakeDropped("p0", 9800)}).empty());
}

/**
 * @brief Only the new segment is reported when another is dropped alongside one already reported.
 */
TEST(TailDropTrackerTests, Update_NewDropAmongKnown_ReportsOnlyNew)
{
	TailDropTracker tracker;
	tracker.Update({MakeDropped("p0", 9800)});

	const std::vector<DroppedSegment> reported = tracker.Update({MakeDropped("p0", 9800), MakeDropped("p1", 9800)});

	ASSERT_EQ(reported.size(), 1u);
	EXPECT_EQ(reported[0].periodId, "p1");
}

/**
 * @brief A segment that stops being dropped (its Period left the manifest) is reported again if it returns.
 */
TEST(TailDropTrackerTests, Update_DropAbsentFromAParse_ReportedAgainOnReturn)
{
	TailDropTracker tracker;
	tracker.Update({MakeDropped("p0", 9800)});
	EXPECT_TRUE(tracker.Update({}).empty());
	EXPECT_EQ(tracker.Update({MakeDropped("p0", 9800)}).size(), 1u);
}
