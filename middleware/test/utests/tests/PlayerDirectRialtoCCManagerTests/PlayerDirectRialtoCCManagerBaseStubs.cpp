/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2026 RDK Management
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

#include "PlayerCCManager.h"

int PlayerCCManagerBase::Init(void *handle)
{
	(void) handle;
	return 0;
}

void PlayerCCManagerBase::RestoreCC(bool shouldRestoreCC)
{
	if (!mEnabled && shouldRestoreCC)
	{
		mEnabled = shouldRestoreCC;
	}
}

bool PlayerCCManagerBase::IsOOBCCRenderingSupported()
{
	return false;
}

int PlayerCCManagerBase::SetStatus(bool enable)
{
	mEnabled = enable;
	return 0;
}

int PlayerCCManagerBase::SetStyle(const std::string &options)
{
	mOptions = options;
	return 0;
}

int PlayerCCManagerBase::SetTrack(const std::string &track,
	const CCFormat format)
{
	mTrack = track;
	mTrackFormat = format;
	return 0;
}

void PlayerCCManagerBase::SetTrickplayStatus(bool enable)
{
	mTrickplayStarted = enable;
}

void PlayerCCManagerBase::SetParentalControlStatus(bool locked)
{
	mParentalCtrlLocked = locked;
}

void PlayerCCManagerBase::ResetState()
{
	mOptions.clear();
	mTrack.clear();
	mTrackFormat = eCLOSEDCAPTION_FORMAT_DEFAULT;
	mLastTextTracks.clear();
	mEnabled = false;
	mTrickplayStarted = false;
	mParentalCtrlLocked = false;
}