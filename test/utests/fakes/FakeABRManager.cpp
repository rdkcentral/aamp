/*
* If not stated otherwise in this file or this component's license file the
* following copyright and licenses apply:
*
* Copyright 2022 RDK Management
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

#include "ABRManager.h"

long ABRManager::mPersistBandwidth = 0;
long long ABRManager::mPersistBandwidthUpdatedTime = 0;

ABRManager::ABRManager()
{
}

int ABRManager::getProfileCount()
{
    return 0;
}

int ABRManager::getBestMatchedProfileIndexByBandWidth(int bandwidth)
{
    return 0;
}

int ABRManager::getMaxBandwidthProfile(const std::string& periodId)
{
    return 0;
}

long ABRManager::getBandwidthOfProfile(int profileIndex)
{
    return 0;
}

void ABRManager::clearProfiles()
{
    return;
}

void ABRManager::addProfile(ABRManager::ProfileInfo profile)
{
}

int ABRManager::getRampedDownProfileIndex(int currentProfileIndex, const std::string& periodId)
{
    return 0;
}

int ABRManager::getUserDataOfProfile(int currentProfileIndex)
{
    return 0;
}

void ABRManager::setDefaultInitBitrate(long defaultInitBitrate)
{
}

void ABRManager::updateProfile()
{
}

int ABRManager::getDesiredIframeProfile() const
{
    return 0;
}

int ABRManager::getInitialProfileIndex(bool chooseMediumProfile, const std::string& periodId)
{
    return 0;
}

int ABRManager::getLowestIframeProfile() const
{
    return 0;
}

int ABRManager::getProfileIndexByBitrateRampUpOrDown(int currentProfileIndex, long currentBandwidth, long networkBandwidth, int nwConsistencyCnt, const std::string& periodId)
{
    return 0;
}

int ABRManager::getRampedUpProfileIndex(int currentProfileIndex, const std::string& periodId)
{
    return 0;
}

bool ABRManager::isProfileIndexBitrateLowest(int currentProfileIndex, const std::string& periodId)
{
    return true;
}

void ABRManager::setDefaultIframeBitrate(long defaultIframeBitrate)
{
}

int ABRManager::removeProfiles(std::vector<BitsPerSecond> profileBPS, int currentProfileIndex, const std::string& periodId)
{
    return 0;
}
