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
 * @file CMCDHeaders.h
 * @brief CMCDHeaders values
 */

#ifndef CMCDHeaders_h
#define CMCDHeaders_h

#include <string>
#include <unordered_map>
#include <mutex>
#include <vector>

const std::string CMCDSession = "sid=";
const std::string CMCDObject = "ot=";
const std::string CMCDDns = "com.comcast-dns=";
const std::string CMCDFirstByte = "com.comcast-fb=";
const std::string CMCDLastByte = "com.comcast-lb=";
const std::string CMCDBITRATE = "br=";
const std::string CMCDTOPBITRATE = "tb=";
const std::string CMCDBUFFERSTARVATION = "bs";
const std::string CMCDBUFFERLENGTH = "bl=";
const std::string CMCDNEXTURL = "nor=";
const std::string CMCDNEXTRANGEREQUEST = "nrr=";

/**
 * @class   CMCDHeaders
 * @brief   CMCDHeaders Context
 */
class CMCDHeaders
{
protected:
	std::string sessionId;
	std::string mediaType;
	int firstByte;
	int lastByte;
	int dnsLookUptime;
	int bitrate;
	int topBitrate;
	int bufferLength;
	bool bufferStarvation;
	std::string nextUrl;
	std::string mNextRange;

public:
	CMCDHeaders() : sessionId(""), mediaType(""), firstByte(0), lastByte(0), dnsLookUptime(0), bufferStarvation(false), bitrate(0), topBitrate(0), bufferLength(0), nextUrl(""),mNextRange("") {}
	virtual void SetNetworkMetrics(const int &startTransferTime,const int &totalTime,const int &dnsLookUpTime);
	virtual void GetNetworkMetrics(int &startTransferTime, int &totalTime, int &dnsLookUpTime);
	virtual void SetSessionId(const std::string &sid);
	virtual std::string GetSessionId();
	virtual void SetMediaType(const std::string &mediaTypeName);
	virtual void SetBitrate(const int &Bandwidth);
	virtual void SetTopBitrate(const int &Bandwidth);
	virtual void SetBufferLength(const int &bufferlength);
	virtual void SetBufferStarvation(const bool &bufferstarvation);
	virtual std::string GetMediaType();
	virtual void SetNextUrl(const std::string &url);
	virtual void BuildCMCDCustomHeaders(std::unordered_map<std::string, std::vector<std::string>> &mCMCDCustomHeaders);
	virtual void SetNextRange(const std::string &nextrange);
	virtual ~CMCDHeaders() {};
};

#endif
