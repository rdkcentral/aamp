/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2024 RDK Management
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

#include <thread>
#include "TsbLog.h"

using TSB::LogLevel;

void TSB::Log::FormatMetadata(std::ostringstream& logStream, LogLevel level, const char* func,
							  const char* file, const int line)
{
	logStream << "[TSB]";
	switch (level)
	{
		case LogLevel::ERROR:
			logStream << "[ERROR]";
			break;
		case LogLevel::MIL:
			logStream << "[MIL]";
			break;
		case LogLevel::WARN:
			logStream << "[WARN]";
			break;
		case LogLevel::TRACE:
			logStream << "[TRACE]";
			break;
	}
	logStream << "[" << std::this_thread::get_id() << "]";
	logStream << "[" << func << "]";
	logStream << "[" << file << ":" << line << "]";
}

std::ostream& TSB::Log::operator<<(std::ostream& logStream, const std::filesystem::path& path)
{
	return logStream << path.string();
}
