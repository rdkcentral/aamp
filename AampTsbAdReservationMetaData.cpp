/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2025 RDK Management
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

#include <inttypes.h>
#include "AampTsbAdReservationMetaData.h"
#include "AampLogManager.h"
#include "priv_aamp.h"

// AampTsbAdReservationMetaData implementation
AampTsbAdReservationMetaData::AampTsbAdReservationMetaData(
	EventType eventType, const AampTime& adPosition, std::string adBreakId, uint64_t periodPosition)
	: AampTsbAdMetaData(AdType::RESERVATION, eventType, adPosition),
	  mAdBreakId(std::move(adBreakId)),
	  mPeriodPosition(periodPosition)
{
}

void AampTsbAdReservationMetaData::Dump(const std::string &message) const
{
	AAMPLOG_INFO("%sAampTsbAdReservationMetaData: Position=%" PRIu64 "ms, EventType=%d, AdBreakId=%s, PeriodPosition=%" PRIu64,
				 message.c_str(), mPosition.milliseconds(), static_cast<int>(mEventType), mAdBreakId.c_str(), mPeriodPosition);
}

void AampTsbAdReservationMetaData::SendEvent(PrivateInstanceAAMP* aamp) const
{
	if (aamp)
	{
		switch (mEventType)
		{
			case EventType::START:
				aamp->SendAdReservationEvent(AAMP_EVENT_AD_RESERVATION_START, mAdBreakId, mPeriodPosition, mPosition.milliseconds());
				break;
			case EventType::END:
				aamp->SendAdReservationEvent(AAMP_EVENT_AD_RESERVATION_END, mAdBreakId, mPeriodPosition, mPosition.milliseconds());
				break;
			default:
				AAMPLOG_WARN("Unknown reservation event type: %d", static_cast<int>(mEventType));
				break;
		}
	}
	else
	{
		AAMPLOG_ERR("Invalid AAMP instance pointer");
	}
}
