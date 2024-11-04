/*
* If not stated otherwise in this file or this component's license file the
* following copyright and licenses apply:
*
* Copyright 2023 RDK Management
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
#include "MockIsoBmffHelper.h"

MockIsoBmffHelper* g_mockIsoBmffHelper = nullptr;

bool IsoBmffHelper::ConvertToKeyFrame(AampGrowableBuffer &buffer)
{
    return true;
}

bool IsoBmffHelper::RestampPts(AampGrowableBuffer &buffer, int64_t ptsOffset, std::string const &url, const char* trackName, uint32_t timeScale)
{
	if (g_mockIsoBmffHelper)
	{
		return g_mockIsoBmffHelper->RestampPts(buffer, ptsOffset, url, trackName, timeScale);
	}

    return true;
}

bool IsoBmffHelper::SetTimescale(AampGrowableBuffer &buffer, uint32_t timeScale)
{
	if (g_mockIsoBmffHelper)
	{
		return g_mockIsoBmffHelper->SetTimescale(buffer, timeScale);
	}

    return true;
}

bool IsoBmffHelper::SetPtsAndDuration(AampGrowableBuffer &buffer, uint64_t pts, uint64_t duration)
{
	if (g_mockIsoBmffHelper)
	{
		return g_mockIsoBmffHelper->SetPtsAndDuration(buffer, pts, duration);
	}

    return true;
}
