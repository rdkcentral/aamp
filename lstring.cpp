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
 * @brief Implementation file for lightweight trivially‑copiable string utilities.
 *
 * This file contains the out‑of‑line definition of lstring::atof().  
 * All other lstring methods remain header‑only for performance and inlining.
 *
 * @note atof() is implemented here (instead of in lstring.hpp) to avoid pulling
 *       in heavy dependencies through AampLogManager.h. Including that header
 *       inside lstring.hpp would force every consumer of lstring.hpp
 *       (notably fragmentcollector_hls.h) to indirectly include large modules
 *       such as curl, increasing compile‑time coupling and reducing the
 *       lightweight, reusable nature of the lstring abstraction.
 *
 * @note lstring represents a simple (ptr, length) view similar to
 *       std::basic_string_view from <string_view>.
 */

#include <cctype>
#include <stdexcept>
#include "lstring.hpp"
#include "AampLogManager.h"

/**
 * @brief Converts a character buffer to a floating‑point value with robust,
 *        fault‑tolerant parsing tailored for HLS playlist fields.
 *
 * This function is designed for scenarios where malformed or unexpected input
 * is common (e.g., EXTINF durations in HLS playlists) and strict failure is
 * undesirable. It extracts a best‑effort numeric value while still providing
 * strong diagnostics for debugging malformed playlist data.
 *
 * Compared to std::atof() or std::strtod(), this parser:
 *   - Accepts partial numbers and stops cleanly at the first non‑numeric
 *     character (e.g., "3.25e-5" → 3.25, "12.5abc" → 12.5).
 *   - Rejects malformed formats that may indicate real authoring errors,
 *     such as multiple decimal points ("12.34.56") or a sign with no digits
 *     ("+", "-", "+  ").
 *   - Handles leading whitespace and an optional sign.
 *   - Avoids undefined behavior by safely casting characters before calling
 *     std::isspace() or std::isdigit().
 *   - Logs detailed error messages—including the failure reason, offending
 *     character, and input buffer—without throwing exceptions to the caller.
 *
 * @return The parsed floating‑point value, or a best‑effort approximation if
 *         the input is partially valid.
 */
double lstring::atof() const
{
    try
    {
        if (ptr == nullptr)
            throw std::runtime_error("null pointer input");

        if (len == 0)
            throw std::runtime_error("empty string at index 0");

        size_t i = 0;

        // Skip leading whitespace
        while (i < len && std::isspace(static_cast<unsigned char>(ptr[i])))
            i++;

        // If we reached the end, the string was only whitespace
        if (i >= len)
            throw std::runtime_error("empty string at index 0");

        // Optional sign
        int sign = 1;
        if (ptr[i] == '-' || ptr[i] == '+')
        {
            if (ptr[i] == '-')
                sign = -1;
            i++;
        }

        // First meaningful character must be digit or '.'
        if (i >= len)
        {
            throw std::runtime_error(
                std::string("unexpected end of string while expecting digit or '.' at index ") + std::to_string(i)
            );
        }

        // After optional sign, the first character MUST be digit or '.'
        if (!(std::isdigit(static_cast<unsigned char>(ptr[i])) || ptr[i] == '.'))
        {
            throw std::runtime_error( std::string("unexpected character '") + ptr[i] + "' at index " + std::to_string(i) );
        }

        long long ival = 0;     // integer part
        long long frac = 0;     // fractional part
        long long fracDiv = 1;  // divisor for fractional digits
        bool afterDecimal = false;

        // Main parsing loop
        for (; i < len; i++)
        {
            char c = ptr[i];

            if (c >= '0' && c <= '9')
            {
                if (!afterDecimal)
                    ival = ival * 10 + (c - '0');
                else
                {
                    frac = frac * 10 + (c - '0');
                    fracDiv *= 10;
                }
            }
            else if (c == '.')
            {
                if (afterDecimal)
                {
                    throw std::runtime_error( "multiple decimal points at index " + std::to_string(i) );
                }
                afterDecimal = true;
            }
            else
            {
                // Stop parsing on any non-numeric character
                break;
            }
        }

        double result = (double)ival + (double)frac / (double)fracDiv;
        return sign * result;
    }
    catch (const std::exception& e)
    {
        if (ptr != nullptr && len > 0)
        {
            AAMPLOG_ERR( "exception %s | input: \"%.*s\" len: %zu", e.what(), (int)len, ptr, (size_t)len );
        }
        else
        {
            AAMPLOG_ERR( "unknown exception | len: %zu", (size_t)len );
        }
        return 0.0;
    }
    catch (...)
    {
        if (ptr != nullptr && len > 0)
        {
            AAMPLOG_ERR( "unknown exception | input: \"%.*s\" len: %zu", (int)len, ptr, (size_t)len );
        }
        else
        {
            AAMPLOG_ERR( "unknown exception | len: %zu", (size_t)len );
        }
        return 0.0;
    }
}