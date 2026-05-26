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
 * @file AampTextTransform.cpp
 * @brief TTML display-offset detection for the direct-Rialto subtitle path.
 *
 * Implements the same LINEAR_OFFSET detection heuristic as
 * gstvipertransform::before_transform() without any GStreamer or AAMP
 * dependencies.
 */

#include "AampTextTransform.h"

#include <cmath>
#include <cstdio>
#include <cstring>

// ---------------------------------------------------------------------------
// parseTimeToMs
// ---------------------------------------------------------------------------

/**
 * TTML clock values appear as H:MM:SS.mmm (hours may have multiple digits).
 * sscanf with "%d:%d:%f" handles the common broadcast format; other TTML
 * time expressions (tick values, frame counts) are not used in practice by
 * the linear content that requires LINEAR_OFFSET correction.
 */
int64_t AampTextTransform::parseTimeToMs(const std::string &timeStr)
{
	int   hours = 0;
	int   mins  = 0;
	float secs  = 0.f;

	if (std::sscanf(timeStr.c_str(), "%d:%d:%f", &hours, &mins, &secs) != 3)
	{
		return 0;
	}

	return (static_cast<int64_t>(hours) * 3600LL + static_cast<int64_t>(mins) * 60LL)
		* 1000LL
		+ static_cast<int64_t>(secs * 1000.f);
}

// ---------------------------------------------------------------------------
// findFirstBeginMs
// ---------------------------------------------------------------------------

bool AampTextTransform::findFirstBeginMs(const std::string &ttml, int64_t &outMs)
{
	const std::string tag{"begin=\""};
	const auto pos = ttml.find(tag);
	if (pos == std::string::npos)
	{
		return false;
	}

	const auto valStart = pos + tag.size();
	const auto valEnd   = ttml.find('"', valStart);
	if (valEnd == std::string::npos)
	{
		return false;
	}

	outMs = parseTimeToMs(ttml.substr(valStart, valEnd - valStart));
	return true;
}

// ---------------------------------------------------------------------------
// compute
// ---------------------------------------------------------------------------

int64_t AampTextTransform::compute(
	const uint8_t *data, size_t size,
	int64_t ptsMs, int64_t aampOffsetMs, int64_t durationMs)
{
	if (m_contentType == ContentType::PASSTHROUGH)
	{
		return 0LL;
	}

	// Build a string from the raw payload.
	// For Harmonic UHD buffers that concatenate multiple XML documents,
	// truncate to the first document before searching for begin=.  This
	// matches vipertransform's behaviour of inspecting only the first
	// document to derive the offset.
	std::string ttml(reinterpret_cast<const char *>(data), size);
	const auto secondXml = ttml.find("<?xml", 5);
	if (secondXml != std::string::npos)
	{
		ttml.resize(secondXml);
	}

	int64_t firstBeginMs = 0;
	if (!findFirstBeginMs(ttml, firstBeginMs))
	{
		// Empty / header-only segment — reuse last known offset.
		return (m_contentType == ContentType::LINEAR_OFFSET)
		       ? m_linearBeginOffsetMs
		       : 0LL;
	}

	// Offset between the TTML timestamp of the first cue and the
	// container PTS — positive when TTML uses absolute wall-clock values.
	const int64_t offsetFromPtsMs = firstBeginMs - ptsMs;

	if (m_contentType == ContentType::UNKNOWN)
	{
		if (std::abs(offsetFromPtsMs - aampOffsetMs) > durationMs)
		{
			// Large mismatch: TTML timestamps are absolute.  Use the
			// TTML-derived offset to align cues with the audio/video PTS.
			m_contentType          = ContentType::LINEAR_OFFSET;
			m_linearBeginOffsetMs  = offsetFromPtsMs;
		}
		else if (aampOffsetMs != 0)
		{
			// Small mismatch but AAMP already knows the period-start
			// correction — use it.
			m_contentType          = ContentType::LINEAR_OFFSET;
			m_linearBeginOffsetMs  = aampOffsetMs;
		}
		else
		{
			// TTML cue times line up with the container PTS.
			m_contentType = ContentType::PASSTHROUGH;
			return 0LL;
		}
	}
	else if (m_contentType == ContentType::LINEAR_OFFSET)
	{
		// Refine the stored offset each fragment, matching vipertransform:
		// prefer the TTML-derived value when the gap vs AAMP's offset
		// exceeds one segment duration.
		m_linearBeginOffsetMs =
			(std::abs(offsetFromPtsMs - aampOffsetMs) > durationMs)
			? offsetFromPtsMs
			: aampOffsetMs;
	}

	return m_linearBeginOffsetMs;
}

// ---------------------------------------------------------------------------
// reset
// ---------------------------------------------------------------------------

void AampTextTransform::reset()
{
	m_contentType          = ContentType::UNKNOWN;
	m_linearBeginOffsetMs  = 0;
}
