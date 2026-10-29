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
 * @file AampVodStitcher.h
 * @brief Client-side VOD CDAI manifest stitcher.
 *
 * Produces a single multi-period static DASH MPD by splicing ad-break periods
 * into the main content MPD at registered insertion points. Ad MPDs are
 * fetched in parallel using per-break threads (each with a dedicated curl
 * handle) so that tune latency is minimised when multiple breaks are
 * registered. Supported segment addressing: SegmentTemplate + SegmentTimeline only.
 */

#pragma once

#include <string>

class PrivateInstanceAAMP;
class PrivateCDAIObjectMPD;

/**
 * @brief Build a stitched VOD MPD from main content + resolved ad breaks.
 *
 * Called once at tune time inside FetchDashManifest(), after the main MPD has
 * been downloaded but before it is parsed.  Returns an empty string when there
 * are no registered VOD ad breaks or when stitching is not applicable (live
 * stream, CDAI disabled, etc.).
 *
 * @param aamp        AAMP private instance (used for HTTP downloads).
 * @param mainMpdText Raw XML text of the downloaded main content MPD.
 * @param mainMpdUrl  Effective URL of the main MPD (used to resolve BaseURLs).
 * @param cdaiObj     CDAI private object carrying registered break metadata and
 *                    resolved ad URLs (populated by SetAlternateContents before
 *                    tune()).
 * @return Stitched MPD XML string, or empty string on error / no-op.
 */
std::string BuildStitchedVodManifest(
	PrivateInstanceAAMP    *aamp,
	const std::string      &mainMpdText,
	const std::string      &mainMpdUrl,
	PrivateCDAIObjectMPD   *cdaiObj);

/**
 * @brief Check that an MPD can be stitched: it has at least one Period, and every
 *        number the stitcher parses from it (Period/MPD durations, timescale,
 *        presentationTimeOffset, startNumber, SegmentTimeline t/d/r) is well formed.
 *        BuildStitchedVodManifest() returns the original MPD if the main MPD fails
 *        this check, and plays through (skips) any ad whose MPD fails it.
 *        Never throws.  Exposed for L1 unit tests.
 */
bool CanStitchVodMpd(const std::string &mpdText);

/**
 * @brief Snap a period-relative splice time to a segment boundary of the
 *        period's reference track (the first non-trick-play video track with a
 *        SegmentTimeline, else the first track with one): the nearest boundary
 *        within AAMP_VOD_SPLICE_SNAP_TOLERANCE_SEC, otherwise the next one.
 *        Times are mapped onto the timeline via its presentationTimeOffset.
 *        Exposed for L1 unit tests.
 *
 * @param periodBody  Inner XML of the <Period> element.
 * @param sec         Splice time relative to the period.
 * @return Snapped period-relative time; sec unchanged if the period has no SegmentTimeline.
 */
double SnapVodSpliceTime(const std::string &periodBody, double sec);

/**
 * @brief Resolve the period-relative splice point used for the stitched output:
 *        SnapVodSpliceTime(), kept at or after the current cursor (so splice
 *        points never move backwards) and capped at the Period duration (a
 *        last segment overrunning the Period end must not delay the ad or
 *        extend the presentation).  Exposed for L1 unit tests.
 *
 * @param periodBody  Inner XML of the <Period> element.
 * @param periodDur   Declared duration of the Period in seconds.
 * @param cursor      Period-relative end of the content already emitted.
 * @param sec         Requested period-relative insertion point.
 * @return Splice point in [cursor, periodDur].
 */
double ResolveVodSplicePoint(const std::string &periodBody, double periodDur, double cursor, double sec);

/**
 * @brief Trim a period's SegmentTemplate/SegmentTimeline blocks to the window
 *        [startSec, endSec).  Both edges are first snapped with
 *        SnapVodSpliceTime() so every track splits around the same content
 *        position; each track then uses its own boundary nearest that time
 *        (within AAMP_VOD_SPLICE_SNAP_TOLERANCE_SEC), otherwise the next one.
 *        Exposed for L1 unit tests.
 *
 * @param periodBody  Inner XML of the <Period> element.
 * @param startSec    Window start relative to the period (0 = from the beginning).
 * @param endSec      Window end relative to the period (0 = to the end).
 * @return Trimmed period body XML, or an empty string if the snapped window
 *         contains no segments (the caller should then emit no period).
 * @throws std::invalid_argument / std::out_of_range for malformed numeric
 *         attributes (see CanStitchVodMpd()); SnapVodSpliceTime() and
 *         ResolveVodSplicePoint() likewise.
 */
std::string TrimPeriod(const std::string &periodBody, double startSec, double endSec);
