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

/**
 * @file AampCMCDCollector.cpp
 * @brief Class to collect the CMCD Data
 */

#include "AampCMCDCollector.h"
#include "StreamAbstractionAAMP.h"



/**
 * @brief AampCMCDCollector - Constructor
 *
 */
AampCMCDCollector::AampCMCDCollector() : bCMCDEnabled(false),mTraceId(""),
							mCMCDStreamData(),myMutex()
{

}

/**
 * @brief AampCMCDCollector - Destructor
 *
 */
AampCMCDCollector::~AampCMCDCollector()
{
	// Free the memory if allocated
	if(mCMCDStreamData.size())
	{
		for(StreamTypeCMCDIter it=mCMCDStreamData.begin() ; it!=mCMCDStreamData.end() ; it++)
		{
			SAFE_DELETE(it->second);
		}
		mCMCDStreamData.clear();
	}
}

/**
 * @brief Initialize the CMCDCollector , create storage for metrics
 *
 * @return None
 */
void AampCMCDCollector::Initialize(bool enableDisable , std::string &traceId) 
{
	std::lock_guard<std::mutex> lock (myMutex);
	bCMCDEnabled = enableDisable;
	if(enableDisable)
	{
		if(traceId == "unknown")
		{
			uuid_t uuid;
			uuid_generate(uuid);
			char sid[MAX_SESSION_ID_LENGTH];
			uuid_unparse_lower(uuid, sid);
			traceId = sid;
		}
		mTraceId = traceId;
		AAMPLOG_WARN("CMCD Enabled. TraceId:%s", mTraceId.c_str());
		// Create metric handlers for each stream type
		// Add it to table
		CMCDHeaders *pCMCDMetrics = NULL;
		// for Manifest
		pCMCDMetrics = new ManifestCMCDHeaders();
		pCMCDMetrics->SetSessionId(mTraceId);
		pCMCDMetrics->SetMediaType("MANIFEST");
		delete mCMCDStreamData[eMEDIATYPE_MANIFEST];
		mCMCDStreamData[eMEDIATYPE_MANIFEST] = pCMCDMetrics;
		// for Video
		pCMCDMetrics = new VideoCMCDHeaders();
		pCMCDMetrics->SetSessionId(mTraceId);
		pCMCDMetrics->SetMediaType("VIDEO");
		delete mCMCDStreamData[eMEDIATYPE_VIDEO];
		mCMCDStreamData[eMEDIATYPE_VIDEO] = pCMCDMetrics;
		// for Video Init
		pCMCDMetrics = new VideoCMCDHeaders();
		pCMCDMetrics->SetSessionId(mTraceId);
		pCMCDMetrics->SetMediaType("INIT_VIDEO");
		delete mCMCDStreamData[eMEDIATYPE_INIT_VIDEO];
		mCMCDStreamData[eMEDIATYPE_INIT_VIDEO] = pCMCDMetrics;
		// for Video Iframe
		pCMCDMetrics = new VideoCMCDHeaders();
		pCMCDMetrics->SetSessionId(mTraceId);
		pCMCDMetrics->SetMediaType("VIDEO");
		delete mCMCDStreamData[eMEDIATYPE_IFRAME];
		mCMCDStreamData[eMEDIATYPE_IFRAME] = pCMCDMetrics;
		// for Audio
		pCMCDMetrics = new AudioCMCDHeaders();
		pCMCDMetrics->SetSessionId(mTraceId);
		pCMCDMetrics->SetMediaType("AUDIO");
		delete mCMCDStreamData[eMEDIATYPE_AUDIO];
		mCMCDStreamData[eMEDIATYPE_AUDIO] = pCMCDMetrics;
		// for Audio Init
		pCMCDMetrics = new AudioCMCDHeaders();
		pCMCDMetrics->SetSessionId(mTraceId);
		pCMCDMetrics->SetMediaType("INIT_AUDIO");
		delete mCMCDStreamData[eMEDIATYPE_INIT_AUDIO];
		mCMCDStreamData[eMEDIATYPE_INIT_AUDIO] = pCMCDMetrics;
		// for Aux Audio
		pCMCDMetrics = new AudioCMCDHeaders();
		pCMCDMetrics->SetSessionId(mTraceId);
		pCMCDMetrics->SetMediaType("AUXAUDIO");
		delete mCMCDStreamData[eMEDIATYPE_AUX_AUDIO];
		mCMCDStreamData[eMEDIATYPE_AUX_AUDIO] = pCMCDMetrics;
		// for Aux Audio Init
		pCMCDMetrics = new AudioCMCDHeaders();
		pCMCDMetrics->SetSessionId(mTraceId);
		pCMCDMetrics->SetMediaType("INIT_AUDIO");
		delete mCMCDStreamData[eMEDIATYPE_INIT_AUX_AUDIO];
		mCMCDStreamData[eMEDIATYPE_INIT_AUX_AUDIO] = pCMCDMetrics;
		// for Subtitle
		pCMCDMetrics = new SubtitleCMCDHeaders();
		pCMCDMetrics->SetSessionId(mTraceId);
		pCMCDMetrics->SetMediaType("SUBTITLE");
		delete mCMCDStreamData[eMEDIATYPE_SUBTITLE];
		mCMCDStreamData[eMEDIATYPE_SUBTITLE] = pCMCDMetrics;
		// for Subtitle Init
		pCMCDMetrics = new SubtitleCMCDHeaders();
		pCMCDMetrics->SetSessionId(mTraceId);
		pCMCDMetrics->SetMediaType("SUBTITLE");
		delete mCMCDStreamData[eMEDIATYPE_INIT_SUBTITLE];
		mCMCDStreamData[eMEDIATYPE_INIT_SUBTITLE] = pCMCDMetrics;

		// for Video Playlist
		pCMCDMetrics = new ManifestCMCDHeaders();
		pCMCDMetrics->SetSessionId(mTraceId);
		pCMCDMetrics->SetMediaType("PLAYLIST_VIDEO");
		delete mCMCDStreamData[eMEDIATYPE_PLAYLIST_VIDEO];
		mCMCDStreamData[eMEDIATYPE_PLAYLIST_VIDEO] = pCMCDMetrics;

		// for Audio Playlist
		pCMCDMetrics = new ManifestCMCDHeaders();
		pCMCDMetrics->SetSessionId(mTraceId);
		pCMCDMetrics->SetMediaType("PLAYLIST_AUDIO");
		delete mCMCDStreamData[eMEDIATYPE_PLAYLIST_AUDIO];
		mCMCDStreamData[eMEDIATYPE_PLAYLIST_AUDIO] = pCMCDMetrics;

		// for Subtitle Playlist
		pCMCDMetrics = new ManifestCMCDHeaders();
		pCMCDMetrics->SetSessionId(mTraceId);
		pCMCDMetrics->SetMediaType("PLAYLIST_SUBTITLE");
		delete mCMCDStreamData[eMEDIATYPE_PLAYLIST_SUBTITLE];
		mCMCDStreamData[eMEDIATYPE_PLAYLIST_SUBTITLE] = pCMCDMetrics;
	}
}



/**
 * @brief CMCDSetNextObjectRequest Store the next segment uri for stream type
 *
 * @return None
 */
void AampCMCDCollector::CMCDSetNextObjectRequest(std::string url,long CMCDBandwidth,AampMediaType mediaT)
{
	std::lock_guard<std::mutex> lock (myMutex);
	if(bCMCDEnabled)
	{
		StreamTypeCMCDIter it=mCMCDStreamData.find(mediaT);
		if(it != mCMCDStreamData.end())
		{
			CMCDHeaders *pCMCDMetrics = it->second;
			pCMCDMetrics->SetBitrate((int)(CMCDBandwidth/1000));
			pCMCDMetrics->SetNextUrl(url);
		}
	}
}


/**
 * @brief convertHexa to convert decimal to hexadecimal
 *
 * @return hexadecimal
 */
std::string AampCMCDCollector::convertHexa(long long number)
{
	std::string hexa;
	// loop till number>0
	while (number)
	{
		int rem = number % 16;
		// when rem is less than 10 then store 0-9
		// else store A - F
		if (rem < 10)
		   hexa.push_back(rem + '0');
		else
		   hexa.push_back(rem - 10 + 'A');
		number = number / 16;
	}
	std::reverse(hexa.begin(), hexa.end());
	return hexa;
}
/**
 * @brief CMCDGetHeaders Get the CMCD headers to add in download request
 *
 * @return None
 */
void AampCMCDCollector::CMCDGetHeaders(AampMediaType mediaType , std::vector<std::string> &customHeader)
{
	std::lock_guard<std::mutex> lock (myMutex);
	if(bCMCDEnabled)
	{
		// To find the execution time of CMCD Header packing during download operation
		std::unordered_map<std::string, std::vector<std::string>> CMCDCustomHeaders;
		StreamTypeCMCDIter it=mCMCDStreamData.find(mediaType);
		CMCDHeaders *pCMCDMetrics=NULL;
		if(it != mCMCDStreamData.end())
		{
			pCMCDMetrics = it->second;
			pCMCDMetrics->BuildCMCDCustomHeaders(CMCDCustomHeaders);
		}
		else
		{
			AAMPLOG_INFO("[CMCD][%d]Couldn't find the filetype to Get metrics",mediaType);
			return;
		}
		std::string headerValue;
		for (std::unordered_map<std::string, std::vector<std::string>>::iterator it = CMCDCustomHeaders.begin();it != CMCDCustomHeaders.end(); it++)
		{
			headerValue.clear();
			headerValue.append(it->first);
			headerValue.append(" ");
			headerValue.append(it->second.at(0));
			customHeader.push_back(headerValue);
			AAMPLOG_TRACE("[CMCD][%d]Header :%s",mediaType,headerValue.c_str());
		}
	}
}


/**
 * @brief CMCDSetNetworkMetrics Set Network Metrics for CMCD
 *
 * @return None
 */
void AampCMCDCollector::CMCDSetNetworkMetrics(AampMediaType mediaType,  int startTransferTime, int totalTime, int dnsLookUpTime)
{
	std::lock_guard<std::mutex> lock (myMutex);
	if(bCMCDEnabled)
	{
		StreamTypeCMCDIter it=mCMCDStreamData.find(mediaType);
		if(it != mCMCDStreamData.end())
		{
			CMCDHeaders *pCMCDMetrics = it->second;
			pCMCDMetrics->SetNetworkMetrics(startTransferTime,totalTime,dnsLookUpTime);
		}
		else
		{
			AAMPLOG_INFO("[CMCD][%d]couldn't find the filetype to store metrics",mediaType);
		}
	}
}

/**
 * @brief Collect and send all key-value pairs for CMCD headers.
 */
void AampCMCDCollector::SetBitrates(AampMediaType mediaType,const std::vector<BitsPerSecond> bitrateList)
{
	std::lock_guard<std::mutex> lock (myMutex);
	if(bCMCDEnabled && bitrateList.size())
	{
		StreamTypeCMCDIter it=mCMCDStreamData.find(mediaType);
		if(it != mCMCDStreamData.end())
		{
			CMCDHeaders *pCMCDMetrics = it->second;
			long maxBitrate = *max_element(bitrateList.begin(), bitrateList.end());
			AAMPLOG_INFO("[CMCD][%d]Top Bitrate %ld",mediaType,maxBitrate);
			if(mediaType == eMEDIATYPE_VIDEO || mediaType == eMEDIATYPE_AUDIO)
			{
				pCMCDMetrics->SetTopBitrate( (int)(maxBitrate/1000) );
			}
		}
		else
		{
			AAMPLOG_INFO("[CMCD][%d]couldn't find the filetype to store metrics",mediaType);
		}
	}
}



/**
 * @brief Collect and send all key-value pairs for CMCD headers.
 */
void AampCMCDCollector::SetTrackData(AampMediaType mediaType,bool bufferRedStatus,int bufferedDuration,int currentBitrate, bool IsMuxed)
{
	if(bCMCDEnabled)
	{
		// This is internal function called from GetHeaders. No Mutex lock needed here
		StreamTypeCMCDIter it=mCMCDStreamData.find(mediaType);
		if(it == mCMCDStreamData.end())
		{
			return;
		}
		CMCDHeaders *pCMCDMetrics = it->second;
		if(mediaType == eMEDIATYPE_VIDEO || mediaType == eMEDIATYPE_INIT_VIDEO)
		{
			if(IsMuxed)
			{
				pCMCDMetrics->SetMediaType("MUXED");
			}
			pCMCDMetrics->SetBufferStarvation(bufferRedStatus);
			pCMCDMetrics->SetBitrate(currentBitrate);
			pCMCDMetrics->SetBufferLength(bufferedDuration);
		}
		else if(mediaType == eMEDIATYPE_AUDIO || mediaType == eMEDIATYPE_INIT_AUDIO)
		{
			pCMCDMetrics->SetBufferStarvation(bufferRedStatus);
			pCMCDMetrics->SetBufferLength(bufferedDuration);
		}
		// No data for Aux Audio now //////////////
	}
}

/**
 * @brief CMCD populate the nrr fields - next range request to fetch in case of Segment List MPD
 *
 * @return None
 */
void AampCMCDCollector::CMCDSetNextRangeRequest(std::string nextrange,long bandwidth,AampMediaType mediaType)
{
	std::lock_guard<std::mutex> lock (myMutex);
	if(bCMCDEnabled && (!nextrange.empty()))
	{
		StreamTypeCMCDIter it=mCMCDStreamData.find(mediaType);
		if(it != mCMCDStreamData.end())
		{
			CMCDHeaders *pCMCDMetrics = it->second;
			std::string CMCDNextRangeRequest;
			CMCDNextRangeRequest = nextrange;
			pCMCDMetrics->SetBitrate((int)(bandwidth/1000));
			pCMCDMetrics->SetNextRange(CMCDNextRangeRequest);
		}
	}
}
