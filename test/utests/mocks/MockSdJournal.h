/*
* If not stated otherwise in this file or this component's license file the
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

#ifndef AAMP_MOCK_SDJOURNAL_H
#define AAMP_MOCK_SDJOURNAL_H

#include <gmock/gmock.h>


class MockSdJournal
{
public:
	/* Mock without location information used instead of sd_journal_print_with_location() to simplify the mock interface and
	avoid complicating refactoring of the code (i.e. having to update the microtests if a line number changes). */
	MOCK_METHOD(int, sd_journal_print_mock, (int priority, const char *buffer));
};

extern MockSdJournal *g_mockSdJournal;

#endif /* AAMP_MOCK_SDJOURNAL_H */
