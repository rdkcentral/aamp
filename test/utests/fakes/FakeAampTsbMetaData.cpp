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

#include "MockAampTsbMetaData.h"

MockAampTsbMetaData* g_mockAampTsbMetaData = nullptr;

// Constructor for AampTsbMetaData
AampTsbMetaData::AampTsbMetaData(const AampTime& position)
	: mPosition(position), mOrderAdded(0)
{
}

AampTime AampTsbMetaData::GetPosition() const
{
	AampTime position;
	if (g_mockAampTsbMetaData)
	{
		position = g_mockAampTsbMetaData->GetPosition();
	}
	return position;
}

void AampTsbMetaData::SetPosition(const AampTime& position)
{
	if (g_mockAampTsbMetaData)
	{
		g_mockAampTsbMetaData->SetPosition(position);
	}
}

uint32_t AampTsbMetaData::GetOrderAdded() const
{
	uint32_t ret = 0;
	if (g_mockAampTsbMetaData)
	{
		ret = g_mockAampTsbMetaData->GetOrderAdded();
	}
	return ret;
}

void AampTsbMetaData::SetOrderAdded(uint32_t orderAdded)
{
	if (g_mockAampTsbMetaData)
	{
		g_mockAampTsbMetaData->SetOrderAdded(orderAdded);
	}
}
