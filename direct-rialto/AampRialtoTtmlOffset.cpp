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
#include "AampLogManager.h"

#include <cctype>
#include <cinttypes>
#include <cstdlib>
#include <limits>

void AampRialtoTtmlOffset::Reset()
{
	m_contentType = ContentType::UNKNOWN;
	m_linearOffsetMs = 0;
}

namespace
{
/// Read decimal digits at @p pos into @p value; false on none or overflow.
bool ReadDigits(const std::string &str, size_t &pos, int64_t maxValue,
	int64_t &value, size_t &count)
{
	value = 0;
	count = 0;
	while (pos < str.size() && std::isdigit(static_cast<unsigned char>(str[pos])))
	{
		const int digit = str[pos] - '0';
		if (value > (maxValue - digit) / 10)
		{
			return false;
		}
		value = value * 10 + digit;
		++pos;
		++count;
	}
	return count > 0;
}
}

std::optional<int64_t> AampRialtoTtmlOffset::ParseTimeToMs(
	const std::string &timeStr)
{
	// hours:MM:SS[.fraction | :frames[.subframes]] (TTML clock-time).  Hours
	// are unbounded in TTML; the cap only keeps the result in int64_t.
	constexpr int64_t kMaxHours = std::numeric_limits<int64_t>::max() / 3'600'000 - 1;
	size_t pos = 0;
	size_t count = 0;
	int64_t hours = 0;
	int64_t mins = 0;
	int64_t secs = 0;
	if (!ReadDigits(timeStr, pos, kMaxHours, hours, count) ||
	    pos >= timeStr.size() || timeStr[pos++] != ':' ||
	    !ReadDigits(timeStr, pos, 99, mins, count) || count != 2 || mins > 59 ||
	    pos >= timeStr.size() || timeStr[pos++] != ':' ||
	    !ReadDigits(timeStr, pos, 99, secs, count) || count != 2 || secs > 60)
	{
		return std::nullopt;
	}

	int64_t ms = 0;
	if (pos < timeStr.size() && timeStr[pos] == '.')
	{
		// Milliseconds from the first three digits, rounded by the fourth.
		const size_t start = ++pos;
		while (pos < timeStr.size() &&
		       std::isdigit(static_cast<unsigned char>(timeStr[pos])))
		{
			const size_t index = pos - start;
			const int digit = timeStr[pos] - '0';
			if (index < 3)
			{
				ms = ms * 10 + digit;
			}
			else if (index == 3 && digit >= 5)
			{
				++ms;
			}
			++pos;
		}
		if (pos == start)
		{
			return std::nullopt;
		}
		for (size_t index = pos - start; index < 3; ++index)
		{
			ms *= 10;
		}
	}
	else if (pos < timeStr.size() && timeStr[pos] == ':')
	{
		// Frames (and sub-frames) need the frame rate; ignored, as before.
		int64_t frames = 0;
		++pos;
		if (!ReadDigits(timeStr, pos, 999'999, frames, count))
		{
			return std::nullopt;
		}
		if (pos < timeStr.size() && timeStr[pos] == '.' &&
		    !ReadDigits(timeStr, ++pos, 999'999, frames, count))
		{
			return std::nullopt;
		}
	}
	if (pos != timeStr.size())
	{
		return std::nullopt;
	}
	return (hours * 3600 + mins * 60 + secs) * 1000 + ms;
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

	const std::string value = ttml.substr(valStart, valEnd - valStart);
	const auto ms = ParseTimeToMs(value);
	if (!ms)
	{
		AAMPLOG_WARN("findFirstBeginMs: ignoring unparsable begin=\"%.32s\"",
			value.c_str());
		return false;
	}
	outMs = *ms;
	AAMPLOG_DEBUG("findFirstBeginMs: found begin=\"%s\" → %" PRId64 " ms",
		value.c_str(), outMs);
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
