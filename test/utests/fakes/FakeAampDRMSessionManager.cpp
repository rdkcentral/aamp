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

#include "AampDRMSessionManager.h"
#include "MockAampDRMSessionManager.h"

MockAampDRMSessionManager *g_mockAampDRMSessionManager = nullptr;

AampDRMSessionManager::AampDRMSessionManager(int, PrivateInstanceAAMP*)
{
}

AampDRMSessionManager::~AampDRMSessionManager()
{
}

void AampDRMSessionManager::renewLicense(std::shared_ptr<AampDrmHelper>, void*, PrivateInstanceAAMP*)
{
}

void AampDRMSessionManager::setPlaybackSpeedState(int, double, bool)
{
}

void AampDRMSessionManager::hideWatermarkOnDetach()
{
}

void AampDRMSessionManager::setVideoMute(bool, double)
{
}

void AampDRMSessionManager::setVideoWindowSize(int width, int height)
{
	if (g_mockAampDRMSessionManager)
	{
		g_mockAampDRMSessionManager->setVideoWindowSize(width, height);
	}
}

void AampDRMSessionManager::Stop()
{
}

void AampDRMSessionManager::UpdateMaxDRMSessions(int)
{
}

void AampDRMSessionManager::setLicenseRequestAbort(bool)
{
}
