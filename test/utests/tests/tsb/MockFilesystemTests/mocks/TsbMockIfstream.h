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

#ifndef __TSB_MOCK_IFSTREAM__
#define __TSB_MOCK_IFSTREAM__

#include <gmock/gmock.h>
#include <ios>

class TsbMockIfstream
{
public:
	MOCK_METHOD(void, open, (const std::string&, std::ios_base::openmode));
	MOCK_METHOD(bool, fail, (), (const));
	MOCK_METHOD(void, read, (char*, std::streamsize));
	MOCK_METHOD(void, close, ());
};

extern TsbMockIfstream* g_mockIfstream;

#endif // __TSB_MOCK_IFSTREAM__
