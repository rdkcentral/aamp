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

#ifndef MOCK_PLAYER_UTILS_H
#define MOCK_PLAYER_UTILS_H

#include <gmock/gmock.h>
class MockPlayerUtils
{
public:

        MOCK_METHOD(long long, GetCurrentTimeMS, ());
};

extern MockPlayerUtils *g_mockPlayerUtils;
#endif

