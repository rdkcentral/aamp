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

#include "MockAampTsbAdPlacementMetaData.h"

MockAampTsbAdPlacementMetaData* g_mockAampTsbAdPlacementMetaData = nullptr;

// Constructor for AampTsbAdPlacementMetaData
AampTsbAdPlacementMetaData::AampTsbAdPlacementMetaData(
	EventType eventType, const AampTime& adPosition, uint32_t duration,
	std::string adId, uint32_t relativePosition, uint32_t offset)
	: AampTsbAdMetaData(AdType::PLACEMENT, eventType, adPosition),
	  mAdId(std::move(adId)),
	  mRelativePosition(relativePosition),
	  mOffset(offset),
	  mDuration(duration)
{
}

void AampTsbAdPlacementMetaData::Dump(const std::string &message) const
{
	if (g_mockAampTsbAdPlacementMetaData)
	{
		g_mockAampTsbAdPlacementMetaData->Dump(message);
	}
}

void AampTsbAdPlacementMetaData::SendEvent(PrivateInstanceAAMP* aamp) const
{
	if (g_mockAampTsbAdPlacementMetaData)
	{
		g_mockAampTsbAdPlacementMetaData->SendEvent(aamp);
	}
}
