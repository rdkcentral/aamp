/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2025 RDK Management
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
 * @file AampDownloadInfo.hpp
 * @brief Download information for AAMP fragment downloads
 */

#ifndef AAMP_DOWNLOAD_INFO_HPP
#define AAMP_DOWNLOAD_INFO_HPP

#include <string>
#include <map>
#include <vector>
#include <cstdint>
#include <curl/curl.h>
#include "AampConstants.h"
#include "AampCurlDefine.h"
#include "AampMediaType.h"
#include "AampUtils.h"
#include "AampConfig.h"
#include "AampTime.h"
#include "main_aamp.h"

struct URLInfo
{
	std::string url;   /**< URL of the fragment */
	std::string range; /**< Byte range of the fragment in the format "<start>-<end>" i.e. "0-511" for first 512 bytes from url. Empty string if no explicit range (downloads whole media segment) */

	/**
	 * @brief Default constructor
	 */
	URLInfo()
		: url(""),
		  range("")
	{
	}

	/**
	 * @brief Parameterized constructor
	 * @param url URL of the fragment
	 * @param range Byte range of the fragment
	 */
	URLInfo(const std::string &url, const std::string &range)
		: url(url),
		  range(range)
	{
	}

	/**
	 * @brief Parameterized constructor
	 * @param url URL of the fragment
	 */
	URLInfo(const std::string &url)
		: url(url),
		  range("")
	{
	}
};

typedef std::map<uint32_t, URLInfo> URLBitrateMap;

/**
 * @struct DownloadInfo
 * @brief Stores information for downloading a fragment
 */
struct DownloadInfo
{
	AampMediaType mediaType;	   /**< Media type of the fragment */
	AampCurlInstance curlInstance; /**< Curl instance to be used for download */
	double fragmentDurationSec;	   /**< Duration of the fragment in seconds */
	double absolutePosition;	   /**< Absolute position of the fragment in seconds as per manifest file. For live it will be in epoch time and for VOD, it will be resolved based on the position in period */
	std::string range;			   /**< Byte range of the fragment in the format "<start>-<end>" i.e. "0-511" for first 512 bytes from url. Empty string if no explicit range (downloads whole media segment) */
	int fragmentIndex;			   /**< Index of the byte range in the fragment */
	uint64_t fragmentOffset;	   /**< Offset of the fragment in byte range based stream */
	bool isInitSegment;			   /**< Flag indicating if the fragment is an initialization segment */
	bool isDiscontinuity;		   /**< Flag indicating if the fragment is discontinuous */
	bool isPlayingAd;			   /**< Flag indicating if an ad is playing */
	bool failoverContentSegment;   /**< Flag indicating if the FCS content matched */
	double pts;					   /**< Scaled PTS value from the fragment */
	uint64_t fragmentNumber;	   /**< Fragment number, incremented with each new segment in track, corresponds to $Number& in segment template */
	uint32_t timeScale;			   /**< Fragment Time scale, divide fragment time or duration by timeScale to convert to seconds */
	std::string url;			   /**< URL of the fragment */
	uint32_t bandwidth;			   /**< Bandwidth of the fragment at the time of job submission */
	AampTime ptsOffset;			   /**< Period specific PTS offset used for restamping */
	URLBitrateMap urlList;		   /**< List of all possible URLs with their respective bitrates */

	/**
	 * @brief Default constructor
	 */
	DownloadInfo()
		: mediaType(eMEDIATYPE_DEFAULT),
		  curlInstance(eCURLINSTANCE_MAX),
		  fragmentDurationSec(0),
		  absolutePosition(0),
		  range(""),
		  fragmentIndex(-1),
		  fragmentOffset(0),
		  isInitSegment(false),
		  isDiscontinuity(false),
		  isPlayingAd(false),
		  failoverContentSegment(false),
		  url(""),
		  pts(0),
		  fragmentNumber(0),
		  timeScale(1),
		  bandwidth(0),
		  ptsOffset(0),
		  urlList()
	{
	}

	/**
	 * @brief Parameterized constructor
	 * @param mediaType Media type of the fragment
	 * @param curlInstance Curl instance to be used for download
	 * @param absolutePosition Absolute position of the fragment in seconds
	 * @param fragmentDurationSec Duration of the fragment in seconds
	 * @param range Range of the fragment
	 * @param fragmentIndex Index of the byte range in the fragment
	 * @param fragmentOffset Offset of the fragment in byte range based stream
	 * @param isInitSegment Flag indicating if the fragment is an initialization segment
	 * @param isDiscontinuity Flag indicating if the fragment is discontinuous
	 * @param isPlayingAd Flag indicating if an ad is playing
	 * @param failoverContentSegment Flag indicating if the FCS content
	 * @param pts Scale PTS
	 * @param fragmentNumber Fragment number
	 * @param timeScale Time scale
	 * @param bandwidth Bandwidth of the fragment
	 * @param ptsOffset PTS offset
	 * @param urlList List of all possible URLs with their respective bitrates
	 */
	DownloadInfo(AampMediaType mediaType, AampCurlInstance curlInstance, double absolutePosition, double fragmentDurationSec, std::string range, int fragmentIndex, uint64_t fragmentOffset, bool isInitSegment, bool isDiscontinuity, bool isPlayingAd, bool failoverContentSegment, double pts, uint64_t fragmentNumber, uint32_t timeScale, uint32_t bandwidth, AampTime ptsOffset, URLBitrateMap urlList)
		: mediaType(mediaType),
		  curlInstance(curlInstance),
		  absolutePosition(absolutePosition),
		  fragmentDurationSec(fragmentDurationSec),
		  range(range),
		  fragmentIndex(fragmentIndex),
		  fragmentOffset(fragmentOffset),
		  isInitSegment(isInitSegment),
		  isDiscontinuity(isDiscontinuity),
		  isPlayingAd(isPlayingAd),
		  failoverContentSegment(failoverContentSegment),
		  pts(pts),
		  fragmentNumber(fragmentNumber),
		  timeScale(timeScale),
		  bandwidth(bandwidth),
		  ptsOffset(ptsOffset),
		  urlList(urlList),
		  url("")
	{
	}
};

typedef std::shared_ptr<DownloadInfo> DownloadInfoPtr;

#endif /* AAMP_DOWNLOAD_INFO_HPP */
