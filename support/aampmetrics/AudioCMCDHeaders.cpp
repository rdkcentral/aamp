/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 *   Copyright 2022 RDK Management
 *
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
 */

/**
 * @file AudioCMCDHeaders.cpp
 * @brief AudioCMCDHeaders values formatting
 */

#include "AudioCMCDHeaders.h"
using namespace std;

/**
 * @brief   BuildCMCDCustomHeaders
 * @param   map which collects formatted CMCD headers
 */
void AudioCMCDHeaders::BuildCMCDCustomHeaders(std::unordered_map<std::string, std::vector<std::string>> &mCMCDCustomHeaders)
{
	//For audio sessionid,object type,currentvideobitrate,maximum videobitrate,bufferlength are send as a part of CMCD Headers
	CMCDHeaders::BuildCMCDCustomHeaders(mCMCDCustomHeaders); 
	std::string headerName;
	std::vector<std::string> headerValue;
	std::string delimiter = ",";
	if(mediaType == "INIT_AUDIO")
	{
		headerName="i";
	}
	else
	{
		headerName="a";
	}
	if(bufferStarvation)
	{
		headerValue.push_back(CMCDBUFFERSTARVATION);
		mCMCDCustomHeaders["CMCD-Status:"] = headerValue;
	}
	headerValue.clear();
 	headerValue.push_back(CMCDBITRATE+std::to_string(bitrate)+delimiter+CMCDObject+headerName+delimiter+CMCDTOPBITRATE+std::to_string(topBitrate));
 	mCMCDCustomHeaders["CMCD-Object:"] = headerValue;
	headerValue.clear();
	if(dnsLookUptime > 0)
	{
		headerValue.push_back(CMCDBUFFERLENGTH+std::to_string(bufferLength)+delimiter+CMCDNEXTURL+nextUrl+delimiter+CMCDDns+std::to_string(dnsLookUptime)+delimiter+CMCDFirstByte+std::to_string(firstByte)+delimiter+CMCDLastByte+std::to_string(lastByte));
	}
	else if(!mNextRange.empty())
        {
                headerValue.push_back(CMCDBUFFERLENGTH+std::to_string(bufferLength)+delimiter+CMCDNEXTRANGEREQUEST+mNextRange+delimiter+CMCDFirstByte+std::to_string(firstByte)+delimiter+CMCDLastByte+std::to_string(lastByte));
        }
	else
	{
		headerValue.push_back(CMCDBUFFERLENGTH+std::to_string(bufferLength)+delimiter+CMCDNEXTURL+nextUrl+delimiter+CMCDFirstByte+std::to_string(firstByte)+delimiter+CMCDLastByte+std::to_string(lastByte));
	}
	mCMCDCustomHeaders["CMCD-Request:"] = headerValue;
}
