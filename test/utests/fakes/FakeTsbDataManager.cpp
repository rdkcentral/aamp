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
 * @file AampTSBSessionManager.cpp
 * @brief TSBSession Manager for Aamp
 **************************************/


#include "AampTsbDataManager.h"
#include "MockTSBDataManager.h"

MockTSBDataManager *g_mockTSBDataManager = nullptr;

std::shared_ptr<TsbFragmentData> AampTsbDataManager::GetFragment(double position, bool &eos)
{
	if (g_mockTSBDataManager)
	{
		return g_mockTSBDataManager->GetFragment(position,eos);
	}
	else
	{
		return nullptr;
	}
}

bool AampTsbDataManager::AddInitFragment(std::string &url, AampMediaType media, const StreamInfo &streamInfo, std::string &periodId, double absPosition, int profileIdx)
{
	return false;
}

bool AampTsbDataManager::AddFragment(TSBWriteData &writeData, AampMediaType media, bool discont)
{
	return false;
}

std::shared_ptr<TsbFragmentData> AampTsbDataManager::RemoveFragment(bool &initDeleted)
{
	if (g_mockTSBDataManager)
	{
		return g_mockTSBDataManager->RemoveFragment(initDeleted);
	}
	else
	{
		return nullptr;
	}
}

void AampTsbDataManager::Flush()
{
}

double AampTsbDataManager::GetFirstFragmentPosition()
{
	return 0.0;
}

/**
 *   @fn GetLastFragmentPosition
 *   @return return the position of last fragment inn the list
 */
double AampTsbDataManager::GetLastFragmentPosition()
{
	return 0.0;
}

std::shared_ptr<TsbFragmentData> AampTsbDataManager::GetNearestFragment(double position)
{
	if (g_mockTSBDataManager)
	{
		return g_mockTSBDataManager->GetNearestFragment(position);
	}
	else
	{
		return nullptr;
	}
}

TsbFragmentDataPtr AampTsbDataManager::GetFirstFragment()
{
	if (g_mockTSBDataManager)
	{
		return g_mockTSBDataManager->GetFirstFragment();
	}
	else
	{
		return nullptr;
	}
}

TsbFragmentDataPtr AampTsbDataManager::GetLastFragment()
{
	if (g_mockTSBDataManager)
	{
		return g_mockTSBDataManager->GetLastFragment();
	}
	else
	{
		return nullptr;
	}
}