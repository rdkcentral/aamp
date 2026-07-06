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
 * fetched sequentially before stitching (see implementation notes) so that
 * tune latency stays bounded without unsafe parallel GetFile() usage.
 * Supported segment addressing: SegmentTemplate + SegmentTimeline only.
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
