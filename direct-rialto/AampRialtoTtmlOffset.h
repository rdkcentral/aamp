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
 * @file AampRialtoTtmlOffset.h
 * @brief Detects whether TTML cue times need a display offset, and how much.
 */

#ifndef AAMP_RIALTO_TTML_OFFSET_H
#define AAMP_RIALTO_TTML_OFFSET_H

#include <cstddef>
#include <cstdint>
#include <string>

/**
 * @class AampRialtoTtmlOffset
 * @brief Display-offset detection for one TTML subtitle stream, mirroring
 *        gstvipertransform.
 *
 * The first document with a begin= time decides the mode: PASSTHROUGH when
 * cue times match the container PTS, LINEAR_OFFSET when they are absolute
 * (wall-clock) or AAMP already supplies a period offset.  In LINEAR_OFFSET
 * the offset is refined on every document.
 *
 * Not thread-safe: used only by the subtitle injector thread, and reset by
 * Configure() while injection is stopped.
 */
class AampRialtoTtmlOffset
{
public:
	/// Forget the detected mode, for a new stream.
	void Reset();

	/**
	 * @brief Display offset (ms) for one TTML document.
	 * @param data          The document bytes (not NUL-terminated).
	 * @param size          Number of bytes in @p data.
	 * @param ptsMs         Container PTS of the document.
	 * @param durationMs    Container duration of the document.
	 * @param aampOffsetMs  Period offset AAMP supplied for the fragment.
	 */
	int64_t Refine(const char *data, size_t size, int64_t ptsMs,
		int64_t durationMs, int64_t aampOffsetMs);

	/// TTML clock value H:MM:SS[.fff] in ms; 0 if it does not parse.
	static int64_t ParseTimeToMs(const std::string &timeStr);

	/// First begin="..." value in @p ttml, in ms.
	/// @return false for empty or header-only documents.
	static bool FindFirstBeginMs(const std::string &ttml, int64_t &outMs);

private:
	enum class ContentType
	{
		UNKNOWN,       ///< Not yet determined.
		PASSTHROUGH,   ///< TTML timestamps align with container PTS.
		LINEAR_OFFSET  ///< Absolute TTML timestamps require correction.
	};

	ContentType m_contentType{ContentType::UNKNOWN};
	int64_t m_linearOffsetMs{0};
};

#endif /* AAMP_RIALTO_TTML_OFFSET_H */
