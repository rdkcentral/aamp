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

#ifndef __TSB_FAKE_OFSTREAM__
#define __TSB_FAKE_OFSTREAM__

#include <ios>

#include "TsbFakeBasicFilebuf.h"

namespace TSB
{

namespace FS
{

class ofstream
{
public:
	static constexpr std::ios_base::openmode binary = static_cast<std::ios_base::openmode>(1);

	basic_filebuf* rdbuf() const;
	void open(const std::string&, std::ios_base::openmode);
	bool fail() const;
	bool bad() const;
	void clear(std::ios_base::iostate state = std::ios_base::goodbit);
	void write(const char*, std::streamsize);
	void close();
};

} // namespace FS

} // namespace TSB

#endif // __TSB_FAKE_OFSTREAM__
