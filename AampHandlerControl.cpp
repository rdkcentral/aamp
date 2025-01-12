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
#include "AampHandlerControl.h"

AampHandlerControl::ScopeHelper&  AampHandlerControl::ScopeHelper::operator=(AampHandlerControl::ScopeHelper&& other)
{
	if(mpController)
	{
		mpController->handlerEnd();
	}
	mpController = other.mpController;
	other.mpController = nullptr;
	return *this;
}

void AampHandlerControl::handlerEnd()
{
	std::lock_guard<std::mutex> guard(mSync);
	mInstanceCount--;
	if(mInstanceCount<0)
	{
		mInstanceCount=0;
		AAMPLOG_ERR("instanceCount<0");
	}
	mDoneCond.notify_one();
}

bool AampHandlerControl::isEnabled() const
{
	std::lock_guard<std::mutex> guard(mSync);
	return mEnabled;
}

AampHandlerControl::ScopeHelper AampHandlerControl::getScopeHelper()
{
	std::lock_guard<std::mutex> guard(mSync);
	mInstanceCount++;
	return AampHandlerControl::ScopeHelper(this);
}

bool AampHandlerControl::waitForDone(int MaximumDelayMilliseconds, std::string name)
{
	const std::chrono::steady_clock::time_point end =
//	const std::chrono::_V2::steady_clock::time_point end =
	std::chrono::milliseconds{MaximumDelayMilliseconds} + std::chrono::steady_clock::now();

	disable();

	std::unique_lock<std::mutex> lock(mSync);
	while(mInstanceCount && (std::chrono::steady_clock::now()<end))
	{
		mDoneCond.wait_until(lock, end);
	}

	if(mInstanceCount)
	{
		AAMPLOG_ERR("AAMPGstPlayer: %d instance%s of %s running", 
		mInstanceCount, mInstanceCount?"s":"", name.c_str());
		return false;
	}
	else
	{
		AAMPLOG_MIL("AAMPGstPlayer: all instances of %s completed.", name.c_str());
		return true;
	}
}
