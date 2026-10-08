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

/**************************************
* @file AampMPDTailTrim.cpp
* @brief Removes overrunning Period-tail segments from a parsed MPD
**************************************/

#include "AampMPDTailTrim.h"
#include "AampMPDParseHelper.h"
#include "AampUtils.h"
#include "AampLogManager.h"
#include <algorithm>
#include <memory>
#include <optional>

using dash::mpd::IAdaptationSet;
using dash::mpd::IPeriod;
using dash::mpd::IRepresentation;
using dash::mpd::ISegmentTemplate;
using dash::mpd::ISegmentTimeline;
using dash::mpd::ITimeline;
using dash::mpd::Timeline;

namespace
{
/**
 * @brief <S> repeat counts above this are unsupported; r="-1" parses to a huge value
 */
constexpr uint32_t kMaxTimelineRepeatCount = 100000;

/**
 * @brief One <S> of a SegmentTimeline, validated and with its start resolved
 */
struct TimelineEntry
{
	uint64_t startTicks;
	uint32_t durationTicks;
	uint32_t count;		/**< segments in the entry: repeat count + 1 */
	Timeline *editable;
};

/**
 * @brief A SegmentTimeline with the timescale and presentationTimeOffset that apply to it
 */
struct PeriodTimeline
{
	const ISegmentTimeline *timeline;
	uint32_t timeScale;
	uint64_t presentationTimeOffset;
};

/**
 * @brief A segment's start and end, in seconds relative to the Period start
 */
struct SegmentSpan
{
	uint64_t startTicks;
	double startSec;
	double endSec;
};

/**
 * @brief A segment is dropped if it starts no earlier than startToleranceSec before the Period end
 *        and ends more than minOverhangSec after it.
 */
struct TailDropRule
{
	std::string periodId;
	double periodDurationSec;
	double startToleranceSec;
	double minOverhangSec;

	bool IsCandidate(const SegmentSpan &span) const
	{
		return span.startSec >= (periodDurationSec - startToleranceSec) && span.endSec > (periodDurationSec + minOverhangSec);
	}
};

SegmentSpan SpanOf(const PeriodTimeline &track, const TimelineEntry &entry, uint32_t segmentIndex)
{
	const uint64_t startTicks = entry.startTicks + static_cast<uint64_t>(segmentIndex) * entry.durationTicks;
	const double startSec = (static_cast<double>(startTicks) - static_cast<double>(track.presentationTimeOffset)) / track.timeScale;
	return {startTicks, startSec, startSec + static_cast<double>(entry.durationTicks) / track.timeScale};
}

/**
 * @brief Resolves each start: explicit t, else the end of the previous <S> (libdash reports an absent t as 0).
 * @retval nullopt if any entry is unsupported (zero duration, a repeat count such as r="-1", or not an editable Timeline),
 *         so nothing is edited unless every entry is supported
 */
std::optional<std::vector<TimelineEntry>> ReadTimelineEntries(const std::vector<ITimeline *> &timelines)
{
	std::vector<TimelineEntry> entries;
	entries.reserve(timelines.size());
	uint64_t nextStart = 0;
	for (ITimeline *timeline : timelines)
	{
		Timeline *editable = dynamic_cast<Timeline *>(timeline);
		if (!editable || editable->GetDuration() == 0 || editable->GetRepeatCount() > kMaxTimelineRepeatCount)
		{
			return std::nullopt;
		}
		const uint64_t start = (editable->GetStartTime() != 0) ? editable->GetStartTime() : nextStart;
		const uint32_t count = editable->GetRepeatCount() + 1;
		entries.push_back({start, editable->GetDuration(), count, editable});
		nextStart = start + static_cast<uint64_t>(editable->GetDuration()) * count;
	}
	return entries;
}

/**
 * @brief Period end from @duration only; an end inferred from the next Period's @start can appear after a segment
 *        was already accepted. Not GetPeriodDuration(): that applies the head-cull start delta and needs Initialize().
 * @retval nullopt if @duration is absent or not positive
 */
std::optional<double> PublishedPeriodDurationSec(const IPeriod *period)
{
	if (period->GetDuration().empty())
	{
		return std::nullopt;
	}
	const double durationSec = ParseISO8601Duration(period->GetDuration().c_str()) / 1000.0;
	if (durationSec <= 0.0)
	{
		return std::nullopt;
	}
	return durationSec;
}

/**
 * @brief Segments of an entry to keep: all but the trailing run that the rule drops
 */
uint32_t SegmentsToKeep(const TimelineEntry &entry, const PeriodTimeline &track, const TailDropRule &rule)
{
	uint32_t keep = entry.count;
	while (keep > 0 && rule.IsCandidate(SpanOf(track, entry, keep - 1)))
	{
		keep--;
	}
	return keep;
}

/**
 * @brief Describes segment segmentIndex of entry, in Period-relative seconds
 */
DroppedSegment MakeDroppedSegment(const PeriodTimeline &track, const TailDropRule &rule, const TimelineEntry &entry, uint32_t segmentIndex)
{
	const SegmentSpan span = SpanOf(track, entry, segmentIndex);
	return {rule.periodId, span.startTicks, entry.durationTicks, track.timeScale, track.presentationTimeOffset, span.startSec, span.endSec, rule.periodDurationSec};
}

/**
 * @brief Remove trailing segments of one SegmentTimeline that match the rule, appending each to dropped. Never empties the timeline.
 */
void TrimTimelineTail(const PeriodTimeline &track, const TailDropRule &rule, std::vector<DroppedSegment> &dropped)
{
	// libdash hands back a mutable vector from this const accessor; SegmentTimeline deletes its entries on destruction
	std::vector<ITimeline *> &timelines = track.timeline->GetTimelines();
	const std::optional<std::vector<TimelineEntry>> entries = ReadTimelineEntries(timelines);
	if (!entries)
	{
		return;
	}

	for (size_t index = entries->size(); index > 0; index--)
	{
		const TimelineEntry &entry = (*entries)[index - 1];
		uint32_t keep = SegmentsToKeep(entry, track, rule);
		if (keep == 0 && index == 1)
		{
			keep = 1;
		}
		if (keep == entry.count)
		{
			break;
		}
		for (uint32_t segmentIndex = keep; segmentIndex < entry.count; segmentIndex++)
		{
			dropped.push_back(MakeDroppedSegment(track, rule, entry, segmentIndex));
		}
		if (keep > 0)
		{
			entry.editable->SetRepeatCount(keep - 1);
			break;
		}
		timelines.erase(timelines.begin() + (index - 1));
		const std::unique_ptr<Timeline> removed(entry.editable); // erased above, so SegmentTimeline will not delete it
	}
}

/**
 * @brief The distinct SegmentTimelines of a Period; a timeline shared by several Representations is listed once
 */
std::vector<PeriodTimeline> CollectPeriodTimelines(IPeriod *period)
{
	std::vector<PeriodTimeline> result;
	std::set<const ISegmentTimeline *> visited;
	for (IAdaptationSet *adaptationSet : period->GetAdaptationSets())
	{
		const ISegmentTemplate *adaptationSetTemplate = adaptationSet->GetSegmentTemplate();
		const std::vector<IRepresentation *> &representations = adaptationSet->GetRepresentation();
		// An AdaptationSet without Representations still carries its own template
		const size_t templateCount = std::max<size_t>(representations.size(), 1);
		for (size_t i = 0; i < templateCount; i++)
		{
			const ISegmentTemplate *representationTemplate = representations.empty() ? nullptr : representations[i]->GetSegmentTemplate();
			SegmentTemplates segmentTemplates(representationTemplate, adaptationSetTemplate);
			const ISegmentTimeline *segmentTimeline = segmentTemplates.HasSegmentTemplate() ? segmentTemplates.GetSegmentTimeline() : nullptr;
			const uint32_t timeScale = segmentTemplates.GetTimescale();
			if (segmentTimeline && timeScale != 0 && visited.insert(segmentTimeline).second)
			{
				result.push_back({segmentTimeline, timeScale, segmentTemplates.GetPresentationTimeOffset()});
			}
		}
	}
	return result;
}
} // namespace

std::vector<DroppedSegment> TrimPeriodTailSegments(dash::mpd::IMPD *mpd, double startToleranceSec, double minOverhangSec)
{
	std::vector<DroppedSegment> dropped;
	if (!mpd)
	{
		return dropped;
	}

	for (IPeriod *period : mpd->GetPeriods())
	{
		const std::optional<double> periodDurationSec = PublishedPeriodDurationSec(period);
		if (!periodDurationSec)
		{
			continue;
		}

		const TailDropRule rule{period->GetId(), *periodDurationSec, startToleranceSec, minOverhangSec};
		for (const PeriodTimeline &track : CollectPeriodTimelines(period))
		{
			TrimTimelineTail(track, rule, dropped);
		}
	}
	return dropped;
}

void LogDroppedSegment(const DroppedSegment &segment)
{
	AAMPLOG_WARN("Period[%s] dropped tail segment t=%" PRIu64 " d=%" PRIu32 " (timescale %" PRIu32 ", pto %" PRIu64 "): starts %.3fs, ends %.3fs, Period end %.3fs",
				 segment.periodId.c_str(), segment.startTicks, segment.durationTicks, segment.timeScale, segment.presentationTimeOffset,
				 segment.startSec, segment.endSec, segment.periodEndSec);
}

std::vector<DroppedSegment> TailDropTracker::Update(const std::vector<DroppedSegment> &dropped)
{
	const auto keyOf = [](const DroppedSegment &segment)
	{
		return Key{segment.periodId, segment.startTicks, segment.durationTicks, segment.timeScale};
	};

	std::set<Key> current;
	for (const DroppedSegment &segment : dropped)
	{
		current.insert(keyOf(segment));
	}

	std::vector<DroppedSegment> unreported;
	std::lock_guard lock(mMutex);
	for (const DroppedSegment &segment : dropped)
	{
		if (mPrevious.find(keyOf(segment)) == mPrevious.end())
		{
			unreported.push_back(segment);
		}
	}
	mPrevious = std::move(current);
	return unreported;
}
