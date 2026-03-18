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

/**
 * @file AampTsbDataManager.h
 * @brief TSB Handler for AAMP
 */

#ifndef __AAMP_TSB_DATA_MANAGER_H__
#define __AAMP_TSB_DATA_MANAGER_H__

#include <iostream>
#include <cmath>
#include <memory>
#include <map>
#include <exception>
#include <mutex>
#include <utility>
#include "StreamAbstractionAAMP.h"
#include "ABRManager.h" // For BitsPerSecond
#include "AampTime.h"

#define TSB_DATA_DEBUG_ENABLED 0 /** Enable debug log on development/debug */

struct TSBWriteData
{
	std::string url;
	std::shared_ptr<CachedFragment> cachedFragment;
	double pts;
	std::string periodId;
};

/**
 * @class Parent class
 * @brief Abstracted parent class
 */
class TsbSegment
{
protected:
	/* data */
	std::string url; /**< URL of the fragment  init or media*/
	AampMediaType mediaType; /**< Type of the fragment*/
	AampTime absolutePositionS; /**< absolute position of the current fragment, in seconds since 1970 */
	std::string periodId; /**< period Id of the fragment*/

	TsbSegment(std::string link, AampMediaType media, AampTime absolutePositionS, std::string prId) : url(std::move(link)), mediaType(media), absolutePositionS(absolutePositionS), periodId(std::move(prId)){}

public:
	/**
	 *   @fn GetUrl
	 *   @return string url
	 */
	std::string GetUrl() { return url; }

	/**
	 *   @fn GetMediaType
	 *   @return AampMediaType media type
	 */
	AampMediaType GetMediaType() { return mediaType; }

	/**
	 *   @fn GetPeriodId
	 *   @return API to get periodId
	 */
	std::string& GetPeriodId() { return periodId; }

	/**
	 * @fn GetAbsolutePosition
	 *
	 * @return absolute position of the current fragment, in seconds since 1970
	 */
	AampTime GetAbsolutePosition() const { return absolutePositionS; }
};

/**
 * @class TsbInitData
 * @brief Prototype to Store the fragment and initFragment information  aka meta data
 */
class TsbInitData : public TsbSegment
{
private:
	/* data */
	StreamInfo fragStreamInfo; /**< Fragment stream info such as bandwidth, resolution and framerate*/
	int profileIndex;
	unsigned long users; /**< No of fragments using this init data*/

public:
	/**
	 *   @fn incrementUser
	 *   @brief Increment count of fragments used
	 */
	void incrementUser() { users++; };
	/**
	 *   @fn decrementUser
	 *   @brief Decrement count of fragments used
	 */
	void decrementUser() { users--; };
	/**
	 *   @fn constructor
	 *   @param[in] url - Segment URL as string
	 *   @param[in] media - Segment type as AampMediaType
	 *   @param[in] absolutePositionS - absolute position of the current fragment, in seconds since 1970
	 *   @param[in] streamInfo - fragment stream info
	 *   @param[in] prId - Period Id of the fragment
	 *   @param[in] profileIdx - ABR profile index
	 *   @return void
	 */
	TsbInitData(std::string url, AampMediaType media, AampTime absolutePositionS, const StreamInfo &streamInfo, std::string prId, int profileIdx)
		: TsbSegment(std::move(url), media, absolutePositionS, std::move(prId)), fragStreamInfo(streamInfo), users(0), profileIndex(profileIdx)
	{
	}

	/**
	 *  @brief destructor of init data
	 */
	~TsbInitData()
	{
	}

	/**
	 * @fn GetBandWidth
	 * @brief Get the bandwidth information
	 * @return Bandwidth information in bits per second
	 */
	BitsPerSecond GetBandWidth() { return fragStreamInfo.bandwidthBitsPerSecond; }

	/**
	 * @fn GetCacheFragStreamInfo
	 * @brief Get the cached fragment stream info
	 * @return Fragment stream info
	 */
	const StreamInfo& GetCacheFragStreamInfo() { return fragStreamInfo; }

	/**
	 * @fn GetProfileIndex
	 * @brief Get ABR profile index
	 * @return ABR profile index
	 */
	int GetProfileIndex() { return profileIndex; }

	/**
	 * @brief Get the fragments used count
	 * @fn GetUsers
	 * @return fragments used count as unsigned long
	 */
	unsigned long GetUsers() { return users; }
};

/**
 * @class TsbFragmentData
 * @brief Linked list with data related to the fragment and initFragment stored in AAMP TSB
 */
class TsbFragmentData : public TsbSegment
{
private:
	AampTime duration; /**< duration of the current fragment*/
	AampTime mPTS; /**< PTS of the current fragment in seconds before applying PTS offset, i.e. ISO BMFF baseMediaDecodeTime / timescale */
	bool isDiscontinuous;  /**< the current fragment is discontinuous*/
	std::shared_ptr<TsbInitData> initFragData; /**< init Fragment of the current fragment*/
	uint32_t timeScale; /**< timescale of the current fragment */
	AampTime PTSOffset; /**< PTS offset of the current fragment */

	/* data */
public:
	std::shared_ptr<TsbFragmentData> next; /**< Link list next node for easy access*/
	std::shared_ptr<TsbFragmentData> prev; /**< Link list previous node for easy access*/
	/**
	 *   @fn constructor
	 *   @param[in] url - Segment URL as string
	 *   @param[in] media - Segment type as AampMediaType
	 *   @param[in] absolutePositionS - absolute position of the current fragment, in seconds since 1970
	 *   @param[in] duration - duration of the current fragment
	 *   @param[in] pts - PTS of the current fragment in seconds before applying PTS offset, i.e. ISO BMFF baseMediaDecodeTime / timescale
	 *   @param[in] disc - discontinuity flag
	 *   @param[in] prId - Period Id of the fragment
	 *   @param[in] initData - Pointer to initData
	 *   @param[in] timeScale - timescale of the current fragment
	 *   @param[in] PTSOffset - PTS offset of the current fragment
	 */
	TsbFragmentData(std::string url, AampMediaType media, AampTime absolutePositionS, AampTime duration, AampTime pts, bool disc,
		std::string prId, std::shared_ptr<TsbInitData> initData, uint32_t timeScale, AampTime PTSOffset)
		: TsbSegment(std::move(url), media, absolutePositionS, std::move(prId)), duration(duration), mPTS(pts), isDiscontinuous(disc), initFragData(std::move(initData)),
		timeScale(timeScale), PTSOffset(PTSOffset)
	{
	}

	/**
	 *  @fn Destructor
	 */
	~TsbFragmentData()
	{
	}

	/**
	 *   @fn GetInitFragData
	 *   @return return initFragment shared pointer associated with it
	 */
	std::shared_ptr<TsbInitData> GetInitFragData() const { return initFragData; }

	/**
	 * @fn GetPTS
	 *
	 * @return Query the PST of fragment
	 */
	AampTime GetPTS() const { return mPTS; }

	/**
	 * @fn GetDuration
	 *
	 * @return duration of the fragment as double
	 */
	AampTime GetDuration() const { return duration; }

	/**
	 * @fn IsDiscontinuous
	 *
	 * @return whether is it is discontinuous fragment
	 */
	bool IsDiscontinuous() const { return isDiscontinuous; }

	/**
	 * @fn GetTimeScale
	 *
	 * @return timescale of the fragment
	 */
	uint32_t GetTimeScale() const { return timeScale; }

	/**
	 * @fn GetPTSOffset
	 *
	 * @return PTS offset of the fragment
	 */
	AampTime GetPTSOffset() const { return PTSOffset; }
};

typedef std::shared_ptr<TsbFragmentData> TsbFragmentDataPtr;
typedef std::shared_ptr<TsbInitData> TsbInitDataPtr;

/**
 * @class AampTsbDataManager
 * @brief Handle the TSB meta Data informations;
 */
class AampTsbDataManager
{
private:
	/**
	 *  @brief Internal API to compare data in the sorted Map
	 */
	struct PositionComparator
	{
		bool operator()(const double &a, const double &b) const
		{
			return a < b;
		}
	};

	std::mutex mTsbDataMutex;
	std::map<double, std::shared_ptr<TsbFragmentData>, PositionComparator> mTsbFragmentData;
	std::list<std::shared_ptr<TsbInitData>> mTsbInitData;
	std::shared_ptr<TsbInitData> mCurrentInitData;
	std::shared_ptr<TsbFragmentData> mCurrHead;
	double mRelativePos;

public:
	/**
	 * @brief Construct a new Aamp Tsb Data Manager object
	 */
	AampTsbDataManager() : mTsbDataMutex(), mTsbFragmentData(), mTsbInitData(), mCurrentInitData(nullptr), mCurrHead(nullptr), mRelativePos(0.0)
	{
		AAMPLOG_INFO("Constructor");
	}

	/**
	 * @brief Destroy the Aamp Tsb Data Manager object
	 */
	~AampTsbDataManager()
	{
		AAMPLOG_INFO("Destructor");
	}

	/**
	 *   @fn GetNearestFragment
	 *   @brief Get the nearest fragment for the position
	 *   @param[in] position - Absolute position of the fragment, in seconds since 1970
	 *   @return pointer to the nearest fragment data
	 */
	std::shared_ptr<TsbFragmentData> GetNearestFragment(double position);

	/**
	 *   @fn GetFragment
	 *   @brief Get fragment for the position
	 *   @param[in] position - Exact absolute position of the fragment, in seconds since 1970
	 *   @param[out] eos - Flag to identify the End of stream
	 *   @return pointer to Fragment data and TsbFragmentData
	 */
	std::shared_ptr<TsbFragmentData> GetFragment(double position, bool &eos);

	/**
	 *   @fn GetFirstFragmentPosition
	 *   @return Absolute position of the first fragment, in seconds since 1970
	 */
	double GetFirstFragmentPosition();

	/**
	 *   @fn GetLastFragmentPosition
	 *   @return Absolute position of the last fragment, in seconds since 1970
	 */
	double GetLastFragmentPosition();

	/**
	 *   @fn GetFirstFragment
	 *   @return return the first fragment in the list
	 */
	TsbFragmentDataPtr GetFirstFragment();

	/**
	 *   @fn GetLastFragment
	 *   @return return the last fragment in the list
	 */
	TsbFragmentDataPtr GetLastFragment();

	/**
	 *   @fn RemoveFragment
	 *   @param[in,out] deleteInit - True if init segment was removed as well
	 *   @return Shared pointer to removed fragment
	 */
	std::shared_ptr<TsbFragmentData> RemoveFragment(bool &deleteInit);

	/**
	 *   @fn RemoveFragments
	 *   @brief Remove all fragments until the given position
	 *   @param[in] position - Absolute position, in seconds since 1970, to remove segment until
	 *   @return shared pointer List of TSB fragments removed
	 */
	std::list<std::shared_ptr<TsbFragmentData>> RemoveFragments(double position);

	/**
	 *   @fn AddInitFragment
	 *   @param[in] url - Segment URL as string
	 *   @param[in] media - Segment type as AampMediaType
	 *   @param[in] streamInfo - fragment stream info
	 *   @param[in] periodId - Period Id of this fragment
	 *   @param[in] absPosition - Abs position of this fragment
	 *   @param[in] profileIdx - ABR profile index
	 *   @return true if no exception
	 */
	bool AddInitFragment(std::string &url, AampMediaType media, const StreamInfo &streamInfo, std::string &periodId, double absPosition, int profileIdx = 0);

	/**
	 *   @fn AddFragment
	 *   @brief  AddFragment - add Fragment to TSB data
	 *   @param[in] writeData - Segment data
	 *   @param[in] media - Segment type as AampMediaType
	 *   @param[in] discont - discontinuity flag
	 *   @return true if no exception
	 */
	bool AddFragment(TSBWriteData &writeData, AampMediaType media, bool discont);

	/**
	 *   @fn IsFragmentPresent
	 *   @brief Check for any fragment availability at the given position
	 *   @param[in] position - Absolute position of the fragment, in seconds since 1970
	 *   @return true if present
	 */
	bool IsFragmentPresent(double position);

	/**
	 *  @fn Flush
	 * @return none
	 */
	void Flush();

	/**
	 *   @fn GetNextDiscFragment
	 *   @brief API to get next discontinuous fragment in the list. If not found, will return nullptr.
	 *   @param[in] position - Absolute position, in seconds since 1970, for querying the discontinuous fragment
	 *   @param[in] backwordSearch - Search direction from the position to discontinuous fragment, default forward
	 *   @return TsbFragmentData shared object to fragment data
	 */
	std::shared_ptr<TsbFragmentData> GetNextDiscFragment(double position, bool backwordSearch = false);

	/**
	 *   @fn DumpData
	 *   @return true if no exception
	 */
	bool DumpData();
};

/**EOF*/

#endif
