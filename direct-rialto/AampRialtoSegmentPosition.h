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
 * @file AampRialtoSegmentPosition.h
 * @brief Staged flush parameters and the current segment-start baseline.
 */

#ifndef AAMP_RIALTO_SEGMENT_POSITION_H
#define AAMP_RIALTO_SEGMENT_POSITION_H

#include "AampDefine.h"

#include <atomic>
#include <cstdint>

/**
 * @class AampRialtoSegmentPosition
 * @brief Owns where the current segment starts and what a flush has asked
 *        for but not yet committed.
 *
 * A flush cycle stages a position and rate, and later commits them: the
 * staged position becomes the segment-start baseline that
 * GetPositionMilliseconds() subtracts, and the staged rate becomes the
 * active playback rate.  Between those two points the staged values are
 * also what AttachSource() uses to place a newly attached Rialto source.
 *
 * The class additionally elects exactly one injector thread to drive a
 * deferred flush, for the content types where the position is only known
 * once the first sample of a new period has been demuxed.
 *
 * All state is atomic; no call blocks.
 */
class AampRialtoSegmentPosition
{
public:
	/// StagedPositionNs() when no flush has staged a position this session.
	static constexpr int64_t kNoStagedPosition = -1;

	/**
	 * @brief Record the position, rate and authority a Flush() committed to.
	 *
	 * @param[in] positionNs    Requested position in nanoseconds.
	 * @param[in] rate          Requested playback rate.
	 * @param[in] authoritative True when the caller's position is the real
	 *                          resume position rather than a placeholder.
	 *                          Stored unconditionally so a later
	 *                          non-authoritative flush cannot leave a stale
	 *                          true behind.
	 */
	void StageRequested(int64_t positionNs, int rate, bool authoritative);

	/// Position (ns) staged by the most recent Flush() this session, or
	/// kNoStagedPosition if none.
	int64_t StagedPositionNs() const;

	/// Rate staged by the most recent Flush().
	int StagedRate() const;

	/// Return whether the most recently staged position was authoritative,
	/// clearing the flag so it is answered only once.
	bool ConsumeAuthoritative();

	/// Re-open the deferred-flush election so the next elected sample may
	/// claim it.
	void ResetFlushClaim();

	/// Claim the right to drive the deferred Flush().  Returns true to the
	/// first caller only, until ResetFlushClaim() re-opens the election.
	bool ClaimDeferredFlush();

	/// Record where the current segment actually starts.  Called when a
	/// flush completes, or when the video source first attaches.
	void CommitBaseline(int64_t positionNs);

	/// Segment-start position (ns) for GetPositionMilliseconds().
	int64_t BaselineNs() const;

	/**
	 * @brief Discard the staged position and baseline at end of session.
	 *
	 * The staged rate, the authority flag and the deferred-flush claim are
	 * deliberately left alone: they are consumed by the next flush cycle
	 * or Configure(), not by session teardown.
	 */
	void ClearForNewSession();

private:
	std::atomic<int64_t> m_stagedPositionNs{kNoStagedPosition};
	std::atomic<int>     m_stagedRate{AAMP_NORMAL_PLAY_RATE};
	std::atomic<bool>    m_authoritative{false};
	std::atomic<bool>    m_flushClaimed{false};
	std::atomic<int64_t> m_baselineNs{0};
};

#endif // AAMP_RIALTO_SEGMENT_POSITION_H
