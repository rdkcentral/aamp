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
 * @file AampTsbDataManager.cpp
 * @brief Tsb Data handler for AAMP
 */
#include "AampTsbDataManager.h"
#include <mutex>
#include <chrono>
/**
 * Debug class to find time taken by each API; Enable only with debug flag
 */
class DebugTimeData
{
	std::string apiName;
	std::chrono::steady_clock::time_point creationTime;

public:
	DebugTimeData(std::string api) : apiName(std::move(api))
	{
		creationTime = std::chrono::steady_clock::now();
	}

	~DebugTimeData()
	{
		auto endTime = std::chrono::steady_clock::now();
		std::chrono::duration<double> duration = endTime - creationTime;
		AAMPLOG_MIL("API: %s Taken time: %.02lf ", apiName.c_str(), duration.count() * 1000);
	}
};

#if TSB_DATA_DEBUG_ENABLED
#define TSB_DM_TIME_DATA() DebugTimeData timeData(__FUNCTION__);
#else
#define TSB_DM_TIME_DATA()
#endif

/**
 *   @fn GetNearestFragment
 *   @brief Get the nearest fragment for the position
 *   @param[in] position - Absolute position of the fragment, in seconds since 1970
 *   @return pointer to the nearest fragment data
 */
TsbFragmentDataPtr AampTsbDataManager::GetNearestFragment(double position)
{
	TSB_DM_TIME_DATA();
	TsbFragmentDataPtr fragmentData = nullptr;
	try
	{
		std::lock_guard<std::mutex> lock(mTsbDataMutex);
		if(!mTsbFragmentData.empty())
		{
			do
			{
				auto lower = mTsbFragmentData.lower_bound(position); // Find the first element not less than position
				if (lower == mTsbFragmentData.begin())				 // If target is less than the first key
				{
					fragmentData = lower->second;
					break;
				}

				auto prev = std::prev(lower);		 // Get the previous element
				if (lower == mTsbFragmentData.end()) // If position is greater than the last key
				{
					fragmentData = prev->second;
					break;
				}

				// Determine which key is closer to the position
				if (lower->first - position < position - prev->first)
				{
					fragmentData = lower->second;
					break;
				}
				else
				{
					fragmentData = prev->second;
					break;
				}
			} while (0);
		}
	}
	catch (const std::exception &e)
	{
		AAMPLOG_WARN("Exception caught while removing data from front: %s ", e.what());
	}

	return fragmentData;
}

/**
 *  @brief RemoveFragment - remove fragment from the top
 */
TsbFragmentDataPtr AampTsbDataManager::RemoveFragment(bool &deleteInit)
{
	TSB_DM_TIME_DATA();
	TsbFragmentDataPtr deletedFragment = nullptr;
	try
	{
		std::lock_guard<std::mutex> lock(mTsbDataMutex);
		if (!mTsbFragmentData.empty())
		{
			auto it = mTsbFragmentData.begin();
			deletedFragment = it->second;
			TsbInitDataPtr initData = deletedFragment->GetInitFragData();
			initData->decrementUser();
			if (initData->GetUsers() <= 0)
			{
				AAMPLOG_INFO("Removing Init fragment of BW( %" BITSPERSECOND_FORMAT ")", initData->GetBandWidth());
				deleteInit = true;
				mTsbInitData.remove(initData);
			}
			if (deletedFragment->next)
			{
				deletedFragment->next->prev = nullptr;
			}
			AAMPLOG_INFO("Remove fragment");
			mTsbFragmentData.erase(it);
		}
	}
	catch (const std::exception &e)
	{
		AAMPLOG_WARN("Exception caught while removing data from front: %s ", e.what());
	}

	return deletedFragment;
}

/**
 *   @fn RemoveFragments
 *   @brief Remove all fragments until the given position
 *   @param[in] position - Absolute position, in seconds since 1970, to remove segment until
 *   @return shared pointer List of TSB fragments removed
 */
std::list<TsbFragmentDataPtr> AampTsbDataManager::RemoveFragments(double position)
{
	TSB_DM_TIME_DATA();
	std::list<TsbFragmentDataPtr> deletedFragments;
	try
	{
		std::lock_guard<std::mutex> lock(mTsbDataMutex);
		if (!mTsbFragmentData.empty())
		{
			AAMPLOG_INFO("Remove fragments");
			auto it = mTsbFragmentData.begin();
			while (it != mTsbFragmentData.end())
			{
				if (it->first < position)
				{
					TsbInitDataPtr initData = it->second->GetInitFragData();
					initData->decrementUser();
					if (initData->GetUsers() <= 0)
					{
						AAMPLOG_INFO("Removing Init fragment of BW( %" BITSPERSECOND_FORMAT ") since no more cached fragment using it", initData->GetBandWidth());
						mTsbInitData.remove(initData);
					}
					deletedFragments.push_back(it->second);
					it = mTsbFragmentData.erase(it);
				}
				else
				{
					/** Sorted Map; so can break here*/
					break;
				}
			}
		}
	}
	catch (const std::exception &e)
	{
		AAMPLOG_WARN("Exception caught while removing data based on position: %s ", e.what());
	}

	return deletedFragments;
}

/**
 *   @fn IsFragmentPresent
 *   @brief Check for any fragment availability at the given position
 *   @param[in] position - Absolute position of the fragment, in seconds since 1970
 *   @return true if present
 */
bool AampTsbDataManager::IsFragmentPresent(double position)
{
	TSB_DM_TIME_DATA();
	bool present = false;
	try
	{
		std::lock_guard<std::mutex> lock(mTsbDataMutex);
		if (!mTsbFragmentData.empty())
		{
			auto fst = mTsbFragmentData.begin();
			auto lst = --mTsbFragmentData.end();
			if ((fst->first <= position) && (lst->first >= position))
			{
				present = true;
			}
		}
	}
	catch (const std::exception &e)
	{
		AAMPLOG_WARN("Exception caught while checking fragment Data: %s ", e.what());
	}
	return present;
}

/**
 *   @fn GetFragment
 *   @brief Get fragment for the position
 *   @param[in] position - Exact absolute position of the fragment, in seconds since 1970
 *   @param[out] eos - Flag to identify the End of stream
 *   @return pointer to Fragment data and TsbFragmentData
 */
TsbFragmentDataPtr AampTsbDataManager::GetFragment(double position, bool &eos)
{
	TSB_DM_TIME_DATA();
	eos = false;
	TsbFragmentDataPtr fragment = nullptr;
	std::lock_guard<std::mutex> lock(mTsbDataMutex);
	if (!mTsbFragmentData.empty())
	{
		auto segment = mTsbFragmentData.find(position);
		if (segment != mTsbFragmentData.end())
		{ /** Check whether it is last fragment or not */
			if (segment == (std::prev(mTsbFragmentData.end())))
			{
				/** Mark EOS*/
				eos = true;
			}
			fragment = segment->second;
		}
	}
	return fragment;
}

/**
 *   @fn GetFirstFragmentPosition
 *   @return Absolute position of the first fragment, in seconds since 1970
 */
double AampTsbDataManager::GetFirstFragmentPosition()
{
	TSB_DM_TIME_DATA();
	double pos = 0.0;
	std::lock_guard<std::mutex> lock(mTsbDataMutex);
	if (!mTsbFragmentData.empty())
	{
		pos = mTsbFragmentData.begin()->second->GetAbsolutePosition().inSeconds();
	}
	return pos;
}

/**
 *   @fn GetFirstFragment
 *   @return return the first fragment in the list
 */
TsbFragmentDataPtr AampTsbDataManager::GetFirstFragment()
{
	TSB_DM_TIME_DATA();
	TsbFragmentDataPtr ret = nullptr;
	std::lock_guard<std::mutex> lock(mTsbDataMutex);
	if (!mTsbFragmentData.empty())
	{
		ret = mTsbFragmentData.begin()->second;
	}
	return ret;
}

/**
 *   @fn GetLastFragment
 *   @return return the last fragment in the list
 */
TsbFragmentDataPtr AampTsbDataManager::GetLastFragment()
{
	TSB_DM_TIME_DATA();
	TsbFragmentDataPtr ret = nullptr;
	std::lock_guard<std::mutex> lock(mTsbDataMutex);
	if (!mTsbFragmentData.empty())
	{
		ret = (std::prev(mTsbFragmentData.end()))->second;
	}
	return ret;
}

/**
 *   @fn GetLastFragmentPosition
 *   @return Absolute position of the last fragment, in seconds since 1970
 */
double AampTsbDataManager::GetLastFragmentPosition()
{
	TSB_DM_TIME_DATA();
	double pos = 0.0;
	std::lock_guard<std::mutex> lock(mTsbDataMutex);
	if (!mTsbFragmentData.empty())
	{
		pos = (std::prev(mTsbFragmentData.end()))->second->GetAbsolutePosition().inSeconds();
	}
	return pos;
}

/**
 *  @brief  AddInitFragment - add Init fragment to TSB data
 */
bool AampTsbDataManager::AddInitFragment(std::string &url, AampMediaType media, const StreamInfo &streamInfo, std::string &periodId, double absPosition, int profileIndex)
{
	TSB_DM_TIME_DATA();
	bool ret = false;

	try
	{
		std::lock_guard<std::mutex> lock(mTsbDataMutex);
		AAMPLOG_INFO("[%s] Adding Init Data: position %.02lfs bandwidth %" BITSPERSECOND_FORMAT "  periodId:%s wt: %d ht: %d fr: %.02lf Url: '%s'",
					GetMediaTypeName(media), absPosition, streamInfo.bandwidthBitsPerSecond, periodId.c_str(),
					streamInfo.resolution.width, streamInfo.resolution.height, streamInfo.resolution.framerate, url.c_str());
		TsbInitDataPtr newInitFragData = std::make_shared<TsbInitData>(url, media, AampTime(absPosition), streamInfo, periodId, profileIndex);
		mTsbInitData.push_back(newInitFragData);
		mCurrentInitData = std::move(newInitFragData);
		ret = true;
	}
	catch (const std::exception &e)
	{
		AAMPLOG_WARN("Exception caught while inserting init data: %s ", e.what());
	}

	return ret;
}

/**
 *  @brief  AddFragment - add Fragment to TSB data
 */
bool AampTsbDataManager::AddFragment(TSBWriteData &writeData, AampMediaType media, bool discont)
{
	TSB_DM_TIME_DATA();
	bool ret = false;
	std::string url {writeData.url};
	double position {writeData.cachedFragment->absPosition};
	double duration {writeData.cachedFragment->duration};
	double pts {writeData.pts};
	std::string periodId {writeData.periodId};
	uint32_t timeScale {writeData.cachedFragment->timeScale};
	double PTSOffsetSec {writeData.cachedFragment->PTSOffsetSec};
	try
	{
		std::lock_guard<std::mutex> lock(mTsbDataMutex);
		if (mCurrentInitData == nullptr)
		{
			AAMPLOG_WARN("Inserting fragment at %.02lf but init header information is missing !!!", position);
			return ret;
		}
		AAMPLOG_INFO("[%s] Adding fragment data: position %.02lfs duration %.02lfs pts %.02lfs bandwidth %" BITSPERSECOND_FORMAT " discontinuous %d periodId %s timeScale %u ptsOffset %fs fragmentUrl '%s' initHeaderUrl '%s'",
					 GetMediaTypeName(media), position, duration, pts, mCurrentInitData->GetBandWidth(), discont, periodId.c_str(), timeScale, PTSOffsetSec,
					 url.c_str(), mCurrentInitData->GetUrl().c_str());
		mCurrentInitData->incrementUser();
		TsbFragmentDataPtr fragmentData = std::make_shared<TsbFragmentData>(url, media, AampTime(position), duration, pts, discont, periodId, mCurrentInitData, timeScale, PTSOffsetSec);
		if (mCurrHead != nullptr)
		{
			fragmentData->prev = mCurrHead;
			mCurrHead->next = fragmentData;
		}
		mCurrHead = std::move(fragmentData);
		mTsbFragmentData[position] = mCurrHead;
		ret = true;
	}
	catch (const std::exception &e)
	{
		AAMPLOG_WARN("Exception caught while inserting fragments at pos %.02lf : %s ", position, e.what());
	}
	return ret;
}

/**
 * @brief API to dump the data with meta details for debug
 *
 */
bool AampTsbDataManager::DumpData()
{
	TSB_DM_TIME_DATA();
	bool ret = false;
	try
	{
		std::lock_guard<std::mutex> lock(mTsbDataMutex);
		if (!mTsbFragmentData.empty())
		{
			auto it = mTsbFragmentData.begin();
			while (it != mTsbFragmentData.end())
			{
				TsbFragmentDataPtr fragmentData = it->second;
				TsbInitDataPtr initdata = fragmentData->GetInitFragData();
				AAMPLOG_INFO("Fragment Meta Data: { Media [%d] absPosition : %.02lf duration: %.02lf PTS : %.02lf bandwidth: %" BITSPERSECOND_FORMAT " discontinuous: %d fragmentUrl: '%s' initHeaderUrl: '%s' }",
							 fragmentData->GetMediaType(), fragmentData->GetAbsolutePosition().inSeconds(), fragmentData->GetDuration().inSeconds(), fragmentData->GetPTS().inSeconds(),
							 initdata->GetBandWidth(), fragmentData->IsDiscontinuous(), fragmentData->GetUrl().c_str(), initdata->GetUrl().c_str());
			}
		}
		ret = true;
	}
	catch (const std::exception &e)
	{
		AAMPLOG_WARN("Exception caught while accessing fragments : %s ", e.what());
	}
	return ret;
}

/**
 * @brief API to dump the data with meta details for debug
 *
 */
void AampTsbDataManager::Flush()
{
	TSB_DM_TIME_DATA();
	AAMPLOG_INFO("Flush AAMP TSB data");
	try
	{
		std::lock_guard<std::mutex> lock(mTsbDataMutex);
		if (!mTsbFragmentData.empty())
		{
			mTsbFragmentData.clear();
		}
		if (!mTsbInitData.empty())
		{
			mTsbInitData.clear();
		}
		mCurrentInitData = nullptr;
	}
	catch (const std::exception &e)
	{
		AAMPLOG_WARN("Exception caught while flushing fragments : %s ", e.what());
	}
}

/**
 *   @fn GetNextDiscFragment
 *   @brief API to get next discontinuous fragment in the list. If not found, will return nullptr.
 *   @param[in] position - Absolute position, in seconds since 1970, for querying the discontinuous fragment
 *   @param[in] backwordSerach - Search direction from the position to discontinuous fragment, default forward
 *   @return TsbFragmentData shared object to fragment data
 */
TsbFragmentDataPtr AampTsbDataManager::GetNextDiscFragment(double position, bool backwardSearch)
{
	TsbFragmentDataPtr fragment = nullptr;
	try
	{
		std::lock_guard<std::mutex> lock(mTsbDataMutex);
		auto segment = mTsbFragmentData.lower_bound(position);
		if (!backwardSearch)
		{
			while( segment != mTsbFragmentData.end())
			{
				if (segment->second->IsDiscontinuous())
				{
					fragment =  segment->second;
					break;
				}
				++segment;
			}
		}
		else
		{
			while (segment != mTsbFragmentData.begin())
			{
				if (segment->second->IsDiscontinuous())
				{
					fragment =  segment->second;
					break;
				}
				--segment;
			}
		}
	}
	catch (const std::exception &e)
	{
		AAMPLOG_WARN("Exception caught while getting discontinuous fragments : %s ", e.what());
	}
	return fragment;
}

/**
 * EOF
 */
