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
 * @file AampCMCDSerializer.h
 * @brief Serialization of CMCD (CTA-5004) key-value entries into HTTP request header lines.
 */

#pragma once

#include <string>
#include <vector>

namespace AampCMCD
{

/**
 * @enum HeaderGroup
 * @brief CTA-5004 request header a CMCD key is transported in.
 */
enum class HeaderGroup
{
	eOBJECT,   ///< "CMCD-Object:" - keys describing the requested media object
	eREQUEST,  ///< "CMCD-Request:" - keys describing this particular request
	eSESSION,  ///< "CMCD-Session:" - keys that are constant across the session
	eSTATUS    ///< "CMCD-Status:" - keys describing current client state
};

/**
 * @enum ValueKind
 * @brief How an Entry's value is encoded into its serialized token.
 */
enum class ValueKind
{
	ePLAIN,    ///< emit key=value with the value verbatim
	eBOOLEAN   ///< emit the bare key when value is "1"; omit the token otherwise
};

/**
 * @struct Entry
 * @brief A single CMCD key with its value, transport group and encoding.
 */
struct Entry
{
	std::string key;                          ///< CMCD key name, e.g. "br" or "com.comcast-dns"
	std::string value;                        ///< Value string; interpretation depends on kind
	HeaderGroup group{HeaderGroup::eOBJECT};  ///< Header the key is transported in
	ValueKind kind{ValueKind::ePLAIN};        ///< Encoding applied when serializing
};

/**
 * @brief Map a header group to its HTTP request header name.
 *
 * @param group Header group.
 * @return Header name with trailing colon, e.g. "CMCD-Object:".
 */
std::string HeaderName(HeaderGroup group);

/**
 * @brief Serialize entries into complete CMCD header lines.
 *
 * Tokens are joined with ',' within each group in the order given (no
 * reordering), and one "<header-name> <tokens>" line is produced per group
 * that serialized at least one token. Groups are emitted in the fixed order
 * Object, Request, Session, Status.
 *
 * @param entries Entries to serialize, in emission order.
 * @return Header lines ready to attach to a request, e.g. "CMCD-Object: br=2500,ot=v".
 */
std::vector<std::string> SerializeHeaders(const std::vector<Entry> &entries);

} // namespace AampCMCD
