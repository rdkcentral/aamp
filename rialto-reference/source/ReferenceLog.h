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

#ifndef RIALTO_REFERENCE_LOG_H
#define RIALTO_REFERENCE_LOG_H

namespace rialto_reference
{

void logLine(const char *function, int line, const char *fmt, ...) __attribute__((format(printf, 3, 4)));
bool verboseLogging();

} // namespace rialto_reference

// Lifecycle events, always printed to stderr with a "[RialtoRef]" prefix.
#define RREF_LOG(fmt, ...) ::rialto_reference::logLine(__func__, __LINE__, fmt, ##__VA_ARGS__)

// Per-request detail, printed only when RIALTO_REF_VERBOSE=1.
#define RREF_VERBOSE(fmt, ...)                                                                                         \
	do                                                                                                                 \
	{                                                                                                                  \
		if (::rialto_reference::verboseLogging())                                                                      \
			::rialto_reference::logLine(__func__, __LINE__, fmt, ##__VA_ARGS__);                                       \
	} while (0)

#endif // RIALTO_REFERENCE_LOG_H
