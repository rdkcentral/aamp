/*
* If not stated otherwise in this file or this component's license file the
* following copyright and licenses apply:
*
* Copyright 2022 RDK Management
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

#ifndef AAMP_MOCK_AAMP_MPD_PARSE_H
#define AAMP_MOCK_AAMP_MPD_PARSE_H

#include <gmock/gmock.h>
#include "AampMPDParseHelper.h"

class MockAampMPDParseHelper
{
public:
	MOCK_METHOD(double, GetFirstSegmentScaledStartTime, (IPeriod * period, AampMediaType type));
	MOCK_METHOD(double, GetPeriodDuration, (int periodIndex, uint64_t mLastPlaylistDownloadTimeMs, bool checkIFrame, bool IsUninterruptedTSB));
	MOCK_METHOD(void, GetStartAndDurationFromTimeline, (IPeriod * period, int representationIdx, int adaptationSetIdx, AampTime &scaledStartTime, AampTime &duration));
};

extern MockAampMPDParseHelper *g_mockAampMPDParseHelper;

#endif /* AAMP_MOCK_AAMP_MPD_PARSE_H */
