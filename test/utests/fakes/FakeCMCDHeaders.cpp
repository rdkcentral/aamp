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
#include "CMCDHeaders.h"

void CMCDHeaders::SetNetworkMetrics(const int &startTransferTime,const int &totalTime,const int &dnsLookUpTime)
{
}

void CMCDHeaders::GetNetworkMetrics(int &startTransferTime, int &totalTime, int &dnsLookUpTime)
{
}

void CMCDHeaders::SetSessionId(const std::string &sid)
{
}

std::string CMCDHeaders::GetSessionId()
{
    return CMCDSession;
}

void CMCDHeaders::SetMediaType(const std::string &mediaTypeName)
{
    mediaType = mediaTypeName;
}

void CMCDHeaders::SetBitrate(const int &Bandwidth)
{
}

void CMCDHeaders::SetTopBitrate(const int &Bandwidth)
{
}

void CMCDHeaders::SetBufferLength(const int &bufferlength)
{
}

void CMCDHeaders::SetBufferStarvation(const bool &bufferStarvation)
{
}

std::string CMCDHeaders::GetMediaType()
{
    return mediaType;
}

void CMCDHeaders::SetNextUrl(const std::string &url)
{
}

void CMCDHeaders::BuildCMCDCustomHeaders(std::unordered_map<std::string, std::vector<std::string>> &mCMCDCustomHeaders)
{
}

