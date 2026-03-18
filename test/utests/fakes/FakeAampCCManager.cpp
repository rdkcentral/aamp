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
#include <string>
#include <vector>
#include "priv_aamp.h"
#include "AampLogManager.h"
#include "PlayerCCManager.h"
#include "MockPlayerCCManager.h"

std::shared_ptr<MockPlayerCCManager> g_mockPlayerCCManager{};

// Custom fake class that properly calls the mock
class TestPlayerCCManager : public PlayerCCManagerBase
{
public:
	int SetStatus(bool enable) override
	{
		int result = 0;
		if (g_mockPlayerCCManager)
		{
			result = g_mockPlayerCCManager->SetStatus(enable);
		}
		return result;
	}

	void Release(int iID) override {}
	void StartRendering() override {}
	void StopRendering() override {}
	int SetDigitalChannel(unsigned int id) override { return 0; }
	int SetAnalogChannel(unsigned int id) override { return 0; }

	int SetTrack(const std::string &track, const CCFormat format) override
	{
		int result = 0;
		if (g_mockPlayerCCManager)
		{
			result = g_mockPlayerCCManager->SetTrack(track, format);
		}
		return result;
	}
};

PlayerCCManagerBase* PlayerCCManager::mInstance = nullptr;
int PlayerCCManagerBase::Init(void *handle)
{
	return 0;
}
void PlayerCCManagerBase::RestoreCC()
{
}
void PlayerCCManagerBase::Release(int iID)
{
}
bool PlayerCCManagerBase::IsOOBCCRenderingSupported()
{
	return false;
}
int PlayerCCManagerBase::SetStatus(bool enable)
{
	return 0;
}
int PlayerCCManagerBase::SetStyle(const std::string &options)
{
	return 0;
}
int PlayerCCManagerBase::SetTrack(const std::string &track, const CCFormat format)
{
	return 0;
}
void PlayerCCManagerBase::SetTrickplayStatus(bool enable)
{
}
void PlayerCCManagerBase::SetParentalControlStatus(bool locked)
{
}

void PlayerCCManagerBase::StartRendering()
{
}
void PlayerCCManagerBase::StopRendering()
{
}
void PlayerCCManagerBase::ResetState()
{
}

int PlayerCCManagerBase::SetDigitalChannel(unsigned int id)
{
	return 0;
}
int PlayerCCManagerBase::SetAnalogChannel(unsigned int id)
{
	return 0;
}

void PlayerCCManager::DestroyInstance()
{
	delete mInstance;
}

void PlayerCCManager::SetRialto(bool state)
{
}

PlayerCCManagerBase *PlayerCCManager::GetInstance()
{
	if (!mInstance)
	{
		mInstance = new TestPlayerCCManager();
	}
	return mInstance;
}
