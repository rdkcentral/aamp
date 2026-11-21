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

#include "ReferenceLog.h"

#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace rialto_reference
{

bool verboseLogging()
{
	static const bool verbose = [] {
		const char *value = std::getenv("RIALTO_REF_VERBOSE");
		return value && std::strcmp(value, "1") == 0;
	}();
	return verbose;
}

void logLine(const char *function, int line, const char *fmt, ...)
{
	const auto ms =
		std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
	char prefix[160];
	std::snprintf(prefix, sizeof(prefix), "%lld.%03lld: [RialtoRef][%s][%d] ", static_cast<long long>(ms / 1000),
	              static_cast<long long>(ms % 1000), function, line);

	va_list args;
	va_start(args, fmt);
	char body[1024];
	std::vsnprintf(body, sizeof(body), fmt, args);
	va_end(args);

	// One write per line so it does not interleave with the host's logging.
	std::string out = std::string(prefix) + body + "\n";
	std::fputs(out.c_str(), stderr);
}

} // namespace rialto_reference
