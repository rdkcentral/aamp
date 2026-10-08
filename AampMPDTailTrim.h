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
* @file AampMPDTailTrim.h
* @brief Removes overrunning Period-tail segments from a parsed MPD
**************************************/

#ifndef __AAMP_MPD_TAIL_TRIM_H__
#define __AAMP_MPD_TAIL_TRIM_H__

#include <cstdint>
#include <mutex>
#include <set>
#include <string>
#include <tuple>
#include <vector>
#include "libdash/IMPD.h"

/**
 * @struct DroppedSegment
 * @brief A timeline segment removed by TrimPeriodTailSegments
 */
struct DroppedSegment
{
	std::string periodId;
	uint64_t startTicks;
	uint32_t durationTicks;
	uint32_t timeScale;
	uint64_t presentationTimeOffset;
	double startSec;		/**< relative to the Period start */
	double endSec;			/**< relative to the Period start */
	double periodEndSec;	/**< Period @duration */
};

/**
 * @fn TrimPeriodTailSegments
 * @brief Remove trailing SegmentTimeline segments that start within startToleranceSec of a Period end
 *        given by its @duration, and finish more than minOverhangSec after it (e.g. an ad-splicer overhang).
 *        Periods without @duration are left alone. The MPD is edited in place; call before AampMPDParseHelper::Initialize().
 * @param[in,out] mpd parsed MPD
 * @param[in] startToleranceSec start window before the Period end
 * @param[in] minOverhangSec minimum time the segment must run past the Period end
 * @retval the segments removed, for the caller to report
 */
std::vector<DroppedSegment> TrimPeriodTailSegments(dash::mpd::IMPD *mpd, double startToleranceSec, double minOverhangSec);

/**
 * @fn LogDroppedSegment
 * @brief Warn about a dropped segment, giving its position and the Period end it overran
 */
void LogDroppedSegment(const DroppedSegment &segment);

/**
 * @class TailDropTracker
 * @brief Remembers the segments dropped by the previous manifest parse so that each drop is reported once.
 *        Each Update() replaces the remembered set, so a segment that stops being dropped
 *        (e.g. its Period left the manifest) is reported again if it returns.
 */
class TailDropTracker
{
public:
	/**
	 * @brief Remember this parse's drops
	 * @param[in] dropped segments dropped by the current parse; call on every parse, including when empty
	 * @retval the segments that the previous parse did not also drop
	 */
	std::vector<DroppedSegment> Update(const std::vector<DroppedSegment> &dropped);

private:
	using Key = std::tuple<std::string, uint64_t, uint32_t, uint32_t>;

	std::mutex mMutex;
	std::set<Key> mPrevious{};
};

#endif
