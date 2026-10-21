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
 * @file AampRialtoTtmlOffset.cpp
 * @brief TTML display-offset detection.
 */

#include "AampRialtoTtmlOffset.h"
#include "AampRialtoTime.h"
#include "AampLogManager.h"

#include <cinttypes>
#include <cstdio>
#include <cstdlib>

void AampRialtoTtmlOffset::Reset()
{
	m_contentType = ContentType::UNKNOWN;
	m_linearOffsetMs = 0;
}

int64_t AampRialtoTtmlOffset::ParseTimeToMs(const std::string &timeStr)
{
	int   hours = 0;
	int   mins  = 0;
	double secs = 0.0;

	if (std::sscanf(timeStr.c_str(), "%d:%d:%lf", &hours, &mins, &secs) != 3)
	{
		return 0;
	}

	return (static_cast<int64_t>(hours) * 3600LL +
	        static_cast<int64_t>(mins)  * 60LL)
	       * 1000LL
	       + SecondsToMs(secs);
}

bool AampRialtoTtmlOffset::FindFirstBeginMs(
	const std::string &ttml, int64_t &outMs)
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

	outMs = ParseTimeToMs(ttml.substr(valStart, valEnd - valStart));
	AAMPLOG_DEBUG("findFirstBeginMs: found begin=\"%s\" → %" PRId64 " ms",
		ttml.substr(valStart, valEnd - valStart).c_str(), outMs);
	return true;
}

int64_t AampRialtoTtmlOffset::Refine(const char *data, size_t size,
	int64_t ptsMs, int64_t durationMs, int64_t aampOffsetMs)
{
	if (m_contentType == ContentType::PASSTHROUGH)
	{
		AAMPLOG_TRACE("refineDisplayOffset: PASSTHROUGH → 0");
		return 0LL;
	}

	// Harmonic UHD buffers concatenate several XML documents; only the first
	// is used, as gstvipertransform does.
	std::string ttml(data, size);
	const auto secondXml = ttml.find("<?xml", 5);
	if (secondXml != std::string::npos)
	{
		AAMPLOG_DEBUG("refineDisplayOffset: truncating Harmonic UHD "
			"multi-doc buffer at offset %zu", secondXml);
		ttml.resize(secondXml);
	}

	int64_t firstBeginMs = 0;
	if (!FindFirstBeginMs(ttml, firstBeginMs))
	{
		// Empty / header-only segment — reuse last known offset.
		const int64_t result =
			(m_contentType == ContentType::LINEAR_OFFSET)
			? m_linearOffsetMs
			: 0LL;
		AAMPLOG_DEBUG("refineDisplayOffset: no begin= found, "
			"reusing stored offset %" PRId64 " ms", result);
		return result;
	}

	const int64_t offsetFromPtsMs = firstBeginMs - ptsMs;
	AAMPLOG_DEBUG("refineDisplayOffset: firstBeginMs=%" PRId64
		" ptsMs=%" PRId64 " offsetFromPtsMs=%" PRId64
		" aampOffsetMs=%" PRId64 " durationMs=%" PRId64,
		firstBeginMs, ptsMs, offsetFromPtsMs,
		aampOffsetMs, durationMs);

	if (m_contentType == ContentType::UNKNOWN)
	{
		if (std::abs(offsetFromPtsMs - aampOffsetMs) > durationMs)
		{
			// Large mismatch: TTML timestamps are absolute wall-clock.
			m_contentType = ContentType::LINEAR_OFFSET;
			m_linearOffsetMs = offsetFromPtsMs;
			AAMPLOG_INFO("refineDisplayOffset: detected LINEAR_OFFSET "
				"from TTML, offset=%" PRId64 " ms", m_linearOffsetMs);
		}
		else if (aampOffsetMs != 0)
		{
			// AAMP already knows the period-start correction.
			m_contentType = ContentType::LINEAR_OFFSET;
			m_linearOffsetMs = aampOffsetMs;
			AAMPLOG_INFO("refineDisplayOffset: detected LINEAR_OFFSET "
				"from AAMP offset, offset=%" PRId64 " ms", m_linearOffsetMs);
		}
		else
		{
			// TTML cue times align with container PTS.
			m_contentType = ContentType::PASSTHROUGH;
			AAMPLOG_INFO("refineDisplayOffset: detected PASSTHROUGH");
			return 0LL;
		}
	}
	else
	{
		const int64_t prev = m_linearOffsetMs;
		m_linearOffsetMs =
			(std::abs(offsetFromPtsMs - aampOffsetMs) > durationMs)
			? offsetFromPtsMs
			: aampOffsetMs;
		if (m_linearOffsetMs != prev)
		{
			AAMPLOG_DEBUG("refineDisplayOffset: refined offset "
				"%" PRId64 " → %" PRId64 " ms", prev, m_linearOffsetMs);
		}
	}

	AAMPLOG_TRACE("refineDisplayOffset: post displayOffsetMs=%" PRId64,
		m_linearOffsetMs);
	return m_linearOffsetMs;
}
