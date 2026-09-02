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

#include "MockAampTsbAdReservationMetaData.h"

std::shared_ptr<MockAampTsbAdReservationMetaData> g_mockAampTsbAdReservationMetaData{};

// Constructor for AampTsbAdReservationMetaData
AampTsbAdReservationMetaData::AampTsbAdReservationMetaData(
	EventType eventType, const AampTime& adPosition,
	std::string adBreakId, uint64_t periodPosition,
	std::string reason)
	: AampTsbAdMetaData(AdType::RESERVATION, eventType, adPosition),
	  mAdBreakId(std::move(adBreakId)),
	  mPeriodPosition(periodPosition),
	  mReason(std::move(reason))
{
}

void AampTsbAdReservationMetaData::Dump(const std::string &message) const
{
	if (g_mockAampTsbAdReservationMetaData)
	{
		g_mockAampTsbAdReservationMetaData->Dump(message);
	}
}

void AampTsbAdReservationMetaData::SendEvent(PrivateInstanceAAMP* aamp) const
{
	if (g_mockAampTsbAdReservationMetaData)
	{
		g_mockAampTsbAdReservationMetaData->SendEvent(aamp);
	}
}
