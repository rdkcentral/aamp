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
#include "main_aamp.h"
#include "AampLogManager.h"
#include "AampCCManager.h"

AampCCManagerBase* AampCCManager::mInstance = nullptr;
int AampCCManagerBase::Init(void *handle)
{
	return 0;
}
void AampCCManagerBase::RestoreCC()
{
}
void AampCCManagerBase::Release(int iID)
{
}
bool AampCCManagerBase::IsOOBCCRenderingSupported()
{
	return false;
}
int AampCCManagerBase::SetStatus(bool enable)
{ 
	return 0;
};
int AampCCManagerBase::SetStyle(const std::string &options)
{
	return 0;
};
int AampCCManagerBase::SetTrack(const std::string &track, const CCFormat format)
{
	return 0; 
};
void AampCCManagerBase::SetTrickplayStatus(bool enable)
{
};
void AampCCManagerBase::SetParentalControlStatus(bool locked)
{
};

void AampCCManagerBase::StartRendering()
{
};
void AampCCManagerBase::StopRendering()
{
};

int AampCCManagerBase::SetDigitalChannel(unsigned int id)
{
       return 0;
};
int AampCCManagerBase::SetAnalogChannel(unsigned int id)
{
       return 0;
};

void AampCCManager::DestroyInstance()
{
	delete mInstance;
}

AampCCManagerBase *AampCCManager::GetInstance()
{
	mInstance = new AampFakeCCManager();
        return mInstance;
}

