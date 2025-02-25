
/*
* If not stated otherwise in this file or this component's license file the
* following copyright and licenses apply:
*
* Copyright 2023 RDK Management
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

#include "AampDRMLicPreFetcher.h"

AampLicensePreFetcher::AampLicensePreFetcher(PrivateInstanceAAMP *aamp)
{

}

AampLicensePreFetcher::~AampLicensePreFetcher()
{

}

bool AampLicensePreFetcher::Init()
{
	return false;
}

bool KeyIsQueued(LicensePreFetchObjectPtr &fetchObject)
{
	return false;
}

bool AampLicensePreFetcher::QueueContentProtection(DrmHelperPtr drmHelper, std::string periodId, uint32_t adapIdx, AampMediaType type, bool isVssPeriod)
{
	return false;
}

bool AampLicensePreFetcher::Term()
{
	return false;
}

void AampLicensePreFetcher::PreFetchThread()
{

}

void AampLicensePreFetcher::VssPreFetchThread()
{

}

void AampLicensePreFetcher::NotifyDrmFailure(LicensePreFetchObjectPtr fetchObj, DrmMetaDataEventPtr event)
{

}

bool AampLicensePreFetcher::CreateDRMSession(LicensePreFetchObjectPtr fetchObj)
{
	return false;
}

