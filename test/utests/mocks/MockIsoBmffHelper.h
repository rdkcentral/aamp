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

#ifndef AAMP_MOCK_ISOBMFF_HELPER_H
#define AAMP_MOCK_ISOBMFF_HELPER_H

#include <gmock/gmock.h>
#include "isobmff/isobmffhelper.h"

class MockIsoBmffHelper
{
public:
	MOCK_METHOD(bool, SetTimescale, (AampGrowableBuffer &, uint32_t));
	MOCK_METHOD(bool, SetPtsAndDuration, (AampGrowableBuffer &, uint64_t, uint64_t));
	MOCK_METHOD(bool, RestampPts, (AampGrowableBuffer &, int64_t, const std::string&, const char*, uint32_t));
	MOCK_METHOD(bool, ClearMediaHeaderDuration, (AampGrowableBuffer &));
};

extern MockIsoBmffHelper *g_mockIsoBmffHelper;

#endif /* AAMP_MOCK_ISOBMFF_HELPER_H */
