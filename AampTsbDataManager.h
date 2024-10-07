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
#include "priv_aamp.h"
#include "StreamAbstractionAAMP.h"
#include "ABRManager.h" // For BitsPerSecond

#define TSB_DATA_DEBUG_ENABLED 0 /** Enable debug log on development/debug */

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
	std::string periodId; /**< period Id of the fragment*/

	TsbSegment(std::string link, AampMediaType media, std::string prId) : url(link), mediaType(media), periodId(prId){}

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
};

/**
 * @class TsbInitData
 * @brief Prototype to Store the fragment and initFragment infromation  aka meta data
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
	 *   @return Increment count of fragments used
	 */
	void incrementUser() { users++; };
	/**
	 *   @fn decrementUser
	 *   @return decriment count of fragments used
	 */
	void decrementUser() { users--; };
	/**
	 *   @fn controctor
	 *   @param[in] url - Segment URL as string
	 *   @param[in] media - Segment type as AampMediaType
	 *   @param[in] streamInfo - fragment stream info
	 *   @param[in] prId - Period Id of the fragment
	 *   @param[in] profileIdx - ABR profile index
	 *   @return void
	 */
	TsbInitData(std::string url, AampMediaType media, const StreamInfo &streamInfo, std::string prId, int profileIdx) : TsbSegment(url, media, prId), fragStreamInfo(streamInfo), users(0), profileIndex(profileIdx)
	{
	}

	/**
	 *  @brief Destroctor of init data
	 */
	~TsbInitData()
	{
	}

	/**
	 * @fn GetBandWidth
	 * @return double bandwidth information
	 */
	BitsPerSecond GetBandWidth() { return fragStreamInfo.bandwidthBitsPerSecond; }

	/**
	 * @fn GetCacheFragStreamInfo
	 * @return StreamInfo Fragment stream info
	 */
	const StreamInfo& GetCacheFragStreamInfo() { return fragStreamInfo; }

	/**
	 * @fn GetProfileIndex
	 * @return int ABR profile index
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
 * @brief Prototype to Store the fragment and initFragment infromation
 */
class TsbFragmentData : public TsbSegment
{
private:
	double position; /**< position of the current fragment*/
	double duration; /**< duration of the current fragment*/
	double mPTS; /**< PTS of the current fragment*/
	bool isDiscontinuous;  /**< the current fragment is discontinuous*/
	double relativePosition; /**< Relative position from start*/
	std::shared_ptr<TsbInitData> initFragData; /**< init Fragment of the current fragment*/

	/* data */
public:
	std::shared_ptr<TsbFragmentData> next; /**< Link list next node for easy access*/
	std::shared_ptr<TsbFragmentData> prev; /**< Link list previous node for easy access*/
	/**
	 *   @fn controctor
	 *   @param[in] url - Segment URL as string
	 *   @param[in] media - Segment type as AampMediaType
	 *   @param[in] position - position as double
	 *   @param[in] duration - duration as double
	 *   @param[in] relativePos - Relative position
	 *   @param[in] prId - Period Id of the fragment
	 *   @param[in] initData - Pointer to initData
	 *   @return void
	 */
	TsbFragmentData(std::string url, AampMediaType media, double position, double duration, double pts, bool disc, double relativePos, std::string prId, std::shared_ptr<TsbInitData> initData) : TsbSegment(url, media, prId), position(position), duration(duration), mPTS(pts), isDiscontinuous(disc), initFragData(initData), relativePosition(relativePos)
	{
	}

	/**
	 *  @brief Internal API to insert data
	 */
	~TsbFragmentData()
	{
	}

	/**
	 *   @fn GetInitFragData
	 *   @return retunr initFragment shared pointer associated with it
	 */
	std::shared_ptr<TsbInitData> GetInitFragData() { return initFragData; }

	/**
	 * @brief GetPosition
	 *
	 * @return position of the fragment as double
	 */
	double GetPosition() { return position; }

	/**
	 * @brief GetPTS
	 *
	 * @return Querry the PST of fragment
	 */
	double GetPTS() { return mPTS; }

	/**
	 * @brief GetRelativePosition
	 *
	 * @return Querry the relative position of fragment
	 */
	double GetRelativePosition() { return relativePosition; }

	/**
	 * @brief GetDuration
	 *
	 * @return duration of the fragment as double
	 */
	double GetDuration() { return duration; }

	/**
	 * @brief IsDiscontinuous
	 *
	 * @return whether is it is discontinuous fragment
	 */
	bool IsDiscontinuous() { return isDiscontinuous; }
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
	 *
	 */
	AampTsbDataManager() : mTsbDataMutex(), mTsbFragmentData(), mTsbInitData(), mCurrentInitData(nullptr), mCurrHead(nullptr), mRelativePos(0.0) {}

	/**
	 * @brief Destroy the Aamp Tsb Data Manager object
	 *
	 */
	~AampTsbDataManager() {}

	/**
	 *   @fn GetNearestFragment
	 *   @param[in] position - Nearest position as double
	 *   @return pointer to Fragment data and TsbFragmentData
	 */
	std::shared_ptr<TsbFragmentData> GetNearestFragment(double position);
	/**
	 *   @fn GetFragment
	 *   @param[in] position - Exact position as double
	 *   @param[out] eos - Flag to identify the End of stream
	 *   @return pointer to Fragment data and TsbFragmentData
	 */
	std::shared_ptr<TsbFragmentData> GetFragment(double position, bool &eos);

	/**
	 *   @fn GetFirstFragment
	 *   @return pointer to first Fragment data
	 */
	double GetFirstFragmentPosition();

	/**
	 *   @fn GetFirstFragment
	 *   @return pointer to first Fragment data
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
	 *   @return Shared pointer to removed fragment
	 */
	std::shared_ptr<TsbFragmentData> RemoveFragment();

	/**
	 *   @fn RemoveFragments
	 *   @param[in] position - position to  remove segment until
	 *   @return shared pointer List of TSB fragments removed
	 */
	std::list<std::shared_ptr<TsbFragmentData>> RemoveFragments(double position);

	/**
	 *   @fn AddInitFragment
	 *   @param[in] url - Segment URL as string
	 *   @param[in] media - Segment type as AampMediaType
	 *   @param[in] streamInfo - fragment stream info
	 *   @param[in] periodId - Period Id of this fragment
	 *   @param[in] profileIdx - ABR profile index
	 *   @return true if no exception
	 */
	bool AddInitFragment(std::string &url, AampMediaType media, const StreamInfo &streamInfo, std::string &periodId, int profileIdx = 0);

	/**
	 *   @fn AddFragment
	 *   @param[in] url - Segment URL as string
	 *   @param[in] media - Segment type as AampMediaType
	 *   @param[in] position - position as double
	 *   @param[in] duration - duration as double
	 *   @param[in] pts - PTS value of the fragment
	 *   @param[in] periodId - Period Id of this fragment
	 *   @return true if no exception
	 */
	bool AddFragment(std::string &url, AampMediaType media, double position, double duration, double pts, bool discont, std::string &periodId);

	/**
	 *   @fn IsFragmentPresent
	 *   @param[in] position - position as double
	 *   @return true if present
	 */
	bool IsFragmentPresent(double position);

	/**
	 *  @fn Flush
	 * @return none
	 */
	void Flush();

	/**
	 *   @fn controctor
	 *   @param[in] position - Position for qurrying the discontionious fragment
	 *   @param[in] backwordSerach - Search direction from the position to discontinuous fragment, default forward
	 *   @return TsbFragmentData shared object to fragment data
	 */
	std::shared_ptr<TsbFragmentData> GetNextDiscFragment(double position, bool backwordSerach = false);

	/**
	 *   @fn DumpData
	 *   @return true if no exception
	 */
	bool DumpData();
};

/**EOF*/

#endif
