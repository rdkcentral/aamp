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

/**************************************
 * @file AampMPDPeriodInfo.h
 * @brief AAMP MPD PeriodInfo type
 **************************************/

#ifndef __AAMP_MPD_PERIOD_INFO_H__
#define __AAMP_MPD_PERIOD_INFO_H__

#include <string>
#include <sys/types.h>

/**
 * @struct PeriodInfo
 * @brief Stores details about available periods in mpd
 */

struct PeriodInfo
{
	std::string periodId;
	uint64_t startTime;
	uint32_t timeScale;
	double duration;
	int periodIndex;
	double periodStartTime;
	double periodEndTime;

	PeriodInfo()
		: periodId(""), startTime(0), duration(0.0), timeScale(0), periodIndex(-1),
		  periodStartTime(-1), periodEndTime(-1)
	{
	}
};

#endif /* __AAMP_MPD_PERIOD_INFO_H__ */
