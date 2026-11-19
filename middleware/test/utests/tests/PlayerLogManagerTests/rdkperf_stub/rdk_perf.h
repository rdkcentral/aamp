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

#ifndef RDK_PERF_STUB_H
#define RDK_PERF_STUB_H

#include <string>
#include <vector>

inline std::vector<std::string>& RDKPerfStubEvents()
{
	static std::vector<std::string> events;
	return events;
}

class RDKPerf
{
public:
	explicit RDKPerf(const char* name) : mName(name)
	{
		RDKPerfStubEvents().push_back("start:" + mName);
	}
	~RDKPerf()
	{
		RDKPerfStubEvents().push_back("stop:" + mName);
	}
	RDKPerf(const RDKPerf&) = delete;
	RDKPerf& operator=(const RDKPerf&) = delete;

private:
	std::string mName;
};

#endif
