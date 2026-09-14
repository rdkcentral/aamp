/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 *   Copyright 2022 RDK Management
 *
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
 */

/**
 * @file CMCDSerializer.h
 * @brief Centralized CTA-5004 §3 CMCD serialization primitives.
 *
 * Provides a pure, dependency-free serialization layer that converts structured
 * CMCDEntry records into conformant CMCD header value strings. Deliberately
 * separate from the CMCDHeaders base class so the serializer is not subject to
 * the no-op Fake override used in the aampmetrics test suite.
 */

#ifndef CMCDSerializer_h
#define CMCDSerializer_h

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

/**
 * @brief CMCD header group, mapping each group to its CTA-5004 header name.
 *
 * Values correspond to the four standard CMCD request headers:
 *   Object  -> "CMCD-Object:"
 *   Request -> "CMCD-Request:"
 *   Session -> "CMCD-Session:"
 *   Status  -> "CMCD-Status:"
 */
enum class CMCDGroup
{
    Object,  ///< CMCD-Object header group (br, ot, tb, d)
    Request, ///< CMCD-Request header group (bl, dl, mtp, nor, nrr, su)
    Session, ///< CMCD-Session header group (cid, pr, sf, sid, st, v)
    Status   ///< CMCD-Status header group (bs, rtp)
};

/**
 * @brief A single CMCD key-value entry with type information for correct serialization.
 *
 * Subclasses fill a std::vector<CMCDEntry> and pass it to SerializeToCMCDMap.
 * The serializer sorts entries alphabetically by key within each group, then
 * applies the appropriate encoding per the type flags:
 *
 * - isInteger: applies RoundToNearest100 to the integer in value; entry is
 *   omitted entirely if the rounded result is zero (per CTA-5004 §3 optional-key rule).
 * - isQuotedString: wraps value in double-quotes with interior escape of '"' and '\\'.
 * - isBoolToken: emits the bare key when value == "1"; omits the entry otherwise.
 * - (all false, default): emits a bare key=value token (used for ot, sf, st, etc.).
 */
struct CMCDEntry
{
    std::string key;            ///< CMCD key name, e.g. "br", "sid", "com.comcast-dns"
    std::string value;          ///< Pre-formatted value string; serializer encodes/rounds as needed
    CMCDGroup   group{CMCDGroup::Object}; ///< Header group this entry belongs to
    bool isInteger{false};      ///< true -> apply RoundToNearest100; omit if result is 0
    bool isQuotedString{false}; ///< true -> wrap value in double-quotes with backslash escaping
    bool isBoolToken{false};    ///< true -> emit bare key when value=="1", omit when value!="1"
};

/**
 * @brief Round an integer value to the nearest 100 (half-up).
 *
 * Implements the CTA-5004 §3 rounding rule for numeric CMCD keys (br, tb, bl).
 * Values that round to zero should be treated as "unavailable" and omitted by
 * the caller (SerializeToCMCDMap enforces this for integer entries automatically).
 *
 * Negative values and zero are treated as "unavailable" and return 0 immediately,
 * preventing the INT_MIN+50 signed-overflow that the naive formula would produce.
 *
 * @param value Integer value (kbps or ms). Negative values are treated as unavailable.
 * @return Value rounded to the nearest 100 using round-half-up semantics.
 *         Returns 0 for any input in the range (-inf, 50).
 */
int RoundToNearest100(int value);

/**
 * @brief Wrap a string value in CTA-5004 quoted-string syntax.
 *
 * Adds enclosing double-quotes and backslash-escapes any interior '"' or '\\'
 * characters, as required by CTA-5004 §3 for quoted-string token types
 * (sid, nor, nrr).
 *
 * @param value The raw string value to wrap.
 * @return The quoted and escaped string, e.g. "\"uuid-123\"" or "\"a\\\"b\"".
 */
std::string QuoteString(std::string_view value);

/**
 * @brief Map a CMCDGroup enum value to its CTA-5004 header key string.
 *
 * Returns the header key string with a trailing ':' so it is compatible with
 * the AampCMCDCollector::CMCDGetHeaders assembly loop, which produces:
 *   it->first + " " + it->second.at(0)  ->  "CMCD-Session: sid=\"...\""
 *
 * @param group The CMCD header group.
 * @return Header key string with trailing colon, e.g. "CMCD-Object:".
 */
std::string CMCDGroupToHeaderKey(CMCDGroup group);

/**
 * @brief Serialize a collection of CMCDEntry records into a CMCD header map.
 *
 * For each header group represented in entries, produces a single
 * comma-delimited value string sorted alphabetically by key (CTA-5004 §3 SER-04)
 * and stores it as the first (and only) element of the vector at the group's
 * header key in out.
 *
 * If a group key already exists in out (e.g. "CMCD-Session:" seeded by the base
 * class before a subclass calls this function), the new tokens are merged with
 * the pre-existing tokens, and the combined set is re-sorted alphabetically by
 * CMCD key so the final string remains spec-compliant. This allows multiple
 * sequential calls that each contribute entries to the same group.
 *
 * Encoding rules applied per entry type:
 *   - isInteger: rounds via RoundToNearest100; entries rounding to 0 are omitted.
 *   - isQuotedString: wraps the value in double-quotes via QuoteString.
 *   - isBoolToken: emits bare key when value=="1"; omits the entry otherwise.
 *   - default (all false): emits key=value as a bare token (e.g. ot=v).
 *
 * Groups absent from entries produce no entry in out.
 *
 * @param entries Vector of CMCDEntry records to serialize (may be in any order).
 * @param out     Output map keyed by header name (e.g. "CMCD-Object:") with a
 *                single-element vector containing the joined value string at [0].
 *                Matches the container consumed by AampCMCDCollector::CMCDGetHeaders.
 *                Pre-existing entries for a group are merged (not replaced).
 */
void SerializeToCMCDMap(const std::vector<CMCDEntry>& entries,
                        std::unordered_map<std::string, std::vector<std::string>>& out);

#endif /* CMCDSerializer_h */
