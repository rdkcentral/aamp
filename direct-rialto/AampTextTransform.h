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
 * @file AampTextTransform.h
 * @brief TTML display-offset detection for the direct-Rialto subtitle path.
 *
 * Mirrors the offset-correction logic of gstvipertransform for broadcast
 * linear TTML subtitles where cue timestamps are absolute wall-clock values
 * rather than relative to the segment PTS.
 *
 * On the first non-empty TTML buffer the class detects whether a
 * LINEAR_OFFSET (content timestamps are far ahead of the container PTS) or
 * PASSTHROUGH (timestamps align with PTS) mode applies.  Subsequent calls
 * refine the stored offset using the same heuristic as vipertransform.
 *
 * WebVTT content always passes through unchanged — callers are responsible
 * for only invoking compute() for TTML payloads.
 */

#ifndef AAMP_TEXT_TRANSFORM_H
#define AAMP_TEXT_TRANSFORM_H

#include <cstddef>
#include <cstdint>
#include <string>

/**
 * @class AampTextTransform
 * @brief Stateful TTML display-offset corrector.
 *
 * Usage:
 * @code
 *   AampTextTransform t;
 *   // For each TTML data fragment (raw or demuxed from MP4):
 *   int64_t offsetMs = t.compute(data, size, ptsMs, aampOffsetMs, durationMs);
 *   segment->setDisplayOffset(static_cast<uint64_t>(offsetMs * 1'000'000LL));
 *   // On seek / channel-change / flush:
 *   t.reset();
 * @endcode
 */
class AampTextTransform
{
public:
	/**
	 * @brief Content-type state detected from the first non-empty segment.
	 */
	enum class ContentType
	{
		UNKNOWN,       ///< Not yet determined.
		PASSTHROUGH,   ///< TTML timestamps align with container PTS.
		LINEAR_OFFSET  ///< Absolute TTML timestamps require a correction.
	};

	/**
	 * @brief Compute the display offset (ms) for one TTML sample.
	 *
	 * Detects LINEAR_OFFSET vs PASSTHROUGH on the first call, then
	 * refines the stored offset on subsequent calls following the same
	 * heuristic as gstvipertransform::before_transform().
	 *
	 * For Harmonic UHD buffers containing multiple concatenated XML
	 * documents only the first document is inspected, consistent with
	 * vipertransform behaviour.
	 *
	 * @param data          Raw bytes of the TTML payload.
	 * @param size          Byte count.
	 * @param ptsMs         Presentation timestamp of the sample (ms).
	 * @param aampOffsetMs  Display offset from AAMP fragment metadata
	 *                      (fragmentPTSoffset × 1000).  Zero when AAMP has
	 *                      not applied any period-start correction.
	 * @param durationMs    Sample duration (ms) — used as the threshold
	 *                      that distinguishes LINEAR_OFFSET from PASSTHROUGH.
	 * @return Refined display offset in milliseconds.
	 *         Returns 0 when PASSTHROUGH applies.
	 */
	int64_t compute(const uint8_t *data, size_t size,
	                int64_t ptsMs, int64_t aampOffsetMs,
	                int64_t durationMs);

	/**
	 * @brief Reset detection state.
	 *
	 * Must be called on seek, discontinuity, or channel change so that
	 * content type is re-detected from the new stream.
	 */
	void reset();

	/** @brief Current detection state (diagnostics / unit tests). */
	ContentType contentType() const { return m_contentType; }

private:
	ContentType m_contentType{ContentType::UNKNOWN};
	int64_t     m_linearBeginOffsetMs{0};

	/**
	 * @brief Convert a TTML clock-value string (H:MM:SS[.fff]) to
	 *        milliseconds.
	 */
	static int64_t parseTimeToMs(const std::string &timeStr);

	/**
	 * @brief Locate the first begin="…" attribute in @p ttml and
	 *        return its value in milliseconds via @p outMs.
	 * @return true if found; false if the document has no begin attribute
	 *         (empty / header-only segment).
	 */
	static bool findFirstBeginMs(const std::string &ttml, int64_t &outMs);
};

#endif /* AAMP_TEXT_TRANSFORM_H */
