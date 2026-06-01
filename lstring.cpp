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
 * @file lstring.cpp
 * @brief Implementation file for lightweight string-view utilities.
 *
 * This file contains the out-of-line definition of lstring::atof().
 * All other lstring methods remain header-only for performance and inlining.
 *
 * @note atof() is implemented here (instead of in lstring.hpp) to avoid pulling
 *       in heavy dependencies through AampLogManager.h. Including that header
 *       inside lstring.hpp would force every consumer of lstring.hpp to
 *       indirectly include large modules, increasing compile-time coupling and
 *       reducing the lightweight, reusable nature of the lstring abstraction.
 *
 * @note lstring represents a simple (ptr, length) view similar to
 *       std::basic_string_view from <string_view>.
 */

#include <cctype>
#include <climits>

#include "lstring.hpp"
#include "AampLogManager.h"

/**
 * @brief Converts the view to a floating-point value with robust, fault-tolerant
 *        parsing tailored for HLS playlist fields.
 *
 * This parser is designed for scenarios where malformed or unexpected input is
 * common (e.g., EXTINF durations in HLS playlists) and strict failure is
 * undesirable. It extracts a best-effort numeric value and stops cleanly at the
 * first non-numeric character.
 *
 * Parsing behavior:
 *   - Skips leading whitespace.
 *   - Accepts an optional leading '+' or '-' sign.
 *   - Parses digits and at most one '.' decimal point.
 *   - Requires at least one digit somewhere (either before or after '.').
 *   - Stops at the first character that is not a digit or '.'.
 *   - If a second '.' is encountered, parsing stops (best-effort) rather than
 *     failing the entire conversion.
 *   - Does not parse exponent notation (e.g. "3.25e-5" parses as 3.25 and stops
 *     at 'e').
 *
 * Safety / robustness:
 *   - Never reads past @c len.
 *   - Avoids undefined behavior from signed integer overflow by accumulating
 *     directly into @c double.
 *   - Uses safe casting when calling std::isspace() / std::isdigit().
 *   - Logs a diagnostic message on failure (no digits found, null/empty input).
 *
 * @return Parsed floating-point value on success; 0.0 on failure.
 */
double lstring::atof() const
{
	if (ptr == nullptr || len == 0)
	{
		AAMPLOG_ERR("lstring::atof invalid input | ptr=%p len=%zu", ptr, (size_t)len);
		return 0.0;
	}

	size_t i = 0;

	// Skip leading whitespace
	while (i < len && std::isspace(static_cast<unsigned char>(ptr[i])))
	{
		i++;
	}

	// Whitespace-only input
	if (i >= len)
	{
		AAMPLOG_ERR("lstring::atof whitespace-only input | len=%zu", (size_t)len);
		return 0.0;
	}

	// Optional sign
	double sign = 1.0;
	if (ptr[i] == '+' || ptr[i] == '-')
	{
		if (ptr[i] == '-')
		{
			sign = -1.0;
		}
		i++;
	}

	// Parse digits and optional decimal point. Require at least one digit.
	bool sawDigit = false;
	bool sawDot   = false;

	double value     = 0.0;
	double fracScale = 0.1;

	for (; i < len; i++)
	{
		char c = ptr[i];

		if (c >= '0' && c <= '9')
		{
			sawDigit = true;
			int d = c - '0';

			if (!sawDot)
			{
				// Accumulate integer part (in double; no signed-integer overflow UB)
				value = value * 10.0 + (double)d;
			}
			else
			{
				// Accumulate fractional part
				value += (double)d * fracScale;
				fracScale *= 0.1;
			}
		}
		else if (c == '.')
		{
			if (sawDot)
			{
				// Best-effort: stop on second decimal point rather than failing
				break;
			}
			sawDot = true;
		}
		else
		{
			// Stop parsing on any other character
			break;
		}
	}

	if (!sawDigit)
	{
		// Log a bounded preview of the input (does not require null termination)
		int n = (len > (size_t)INT_MAX) ? INT_MAX : (int)len;
		AAMPLOG_ERR("lstring::atof no digits | input:\"%.*s\" len:%zu", n, ptr, (size_t)len);
		return 0.0;
	}

	return sign * value;
}
