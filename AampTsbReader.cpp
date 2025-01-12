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
 * @file AampTsbReader.cpp
 * @brief AampTsbReader for AAMP
 */

#include "AampTsbReader.h"
#include "AampConfig.h"
#include "StreamAbstractionAAMP.h"
#include <iostream>

/**
 * @fn AampTsbReader Constructor
 *
 * @return None
 */
AampTsbReader::AampTsbReader(PrivateInstanceAAMP *aamp, std::shared_ptr<AampTsbDataManager> dataMgr, AampMediaType mediaType, std::string sessionId)
	: mAamp(aamp), mDataMgr(dataMgr), mMediaType(mediaType), mInitialized_(false), mStartPosition(0.0),
	  mUpcomingFragmentPosition(0.0), mCurrentRate(AAMP_NORMAL_PLAY_RATE), mTsbSessionId(sessionId), mEosReached(false), mTrackEnabled(false),
	  mFirstPTS(0.0), mCurrentBandwidth(0.0), mNewInitWaiting(false), mActiveTuneType(eTUNETYPE_NEW_NORMAL),
	  mEosCVWait(), mEosMutex(), mIsEndFragmentInjected(false)
{
}

/**
 * @fn AampTsbReader Destructor
 *
 * @return None
 */
AampTsbReader::~AampTsbReader()
{
	DeInit();
}

/**
 * @fn AampTsbReader Init function
 * @param[out] - start position
 * @param[in] - current rate
 * @param[in] - tuneType
 * @param[in] - video track reader
 * @return AampStatusType
 */
AAMPStatusType AampTsbReader::Init(double &startPos, float rate, TuneType tuneType, std::shared_ptr<AampTsbReader> other)
{
	AAMPStatusType ret = eAAMPSTATUS_OK;
	if (!mInitialized_)
	{
		if (startPos >= 0)
		{
			if (mDataMgr)
			{
				TsbFragmentDataPtr firstFragment = mDataMgr->GetFirstFragment();
				TsbFragmentDataPtr lastFragment = mDataMgr->GetLastFragment();
				double currentFragmentPosition = 0.0;
				mActiveTuneType = tuneType;
				if (!(firstFragment && lastFragment))
				{
					// No fragments available
					AAMPLOG_INFO("[%s] TSB is empty", GetMediaTypeName(mMediaType));
					mTrackEnabled = false;
				}
				else
				{
					if ((lastFragment->GetRelativePosition() - firstFragment->GetRelativePosition()) < startPos)
					{
						// Handle seek out of range
						AAMPLOG_WARN("[%s] Seeking to the TSB End : %lf (Requested:%lf), Position:%lf, Range:(%lf-%lf) tunetype:%d", GetMediaTypeName(mMediaType), lastFragment->GetRelativePosition(), startPos, lastFragment->GetPosition(), firstFragment->GetRelativePosition(), lastFragment->GetRelativePosition(), mActiveTuneType);
						currentFragmentPosition = lastFragment->GetPosition();
					}
					else
					{
						// Adjust the position according to the stored values.
						currentFragmentPosition = firstFragment->GetPosition() + startPos;
					}
					TsbFragmentDataPtr firstFragmentToFetch = mDataMgr->GetNearestFragment(currentFragmentPosition);
					if (eMEDIATYPE_VIDEO != mMediaType)
					{
						if (other)
						{
							// TODO : Jump to set of video selected period fragments first and then iterate
							double vPTS = other->GetFirstPTS();
							while (firstFragmentToFetch && firstFragmentToFetch->GetPTS() > vPTS)
							{
								if (nullptr == firstFragmentToFetch->prev)
								{
									break; // Break if no previous fragment exists
								}
								if (firstFragmentToFetch->GetPeriodId() != firstFragmentToFetch->prev->GetPeriodId())
								{
									break; // Break if at period boundary
								}
								firstFragmentToFetch = firstFragmentToFetch->prev;
							}
						}
					}
					if (nullptr != firstFragmentToFetch)
					{
						mStartPosition = firstFragmentToFetch->GetPosition();
						// Assign upcoming position as start position
						mUpcomingFragmentPosition = mStartPosition;
						mCurrentRate = rate;
						if (rate != AAMP_NORMAL_PLAY_RATE && eMEDIATYPE_VIDEO != mMediaType)
						{
							// Disable all other tracks except video for trickplay
							mTrackEnabled = false;
						}
						else
						{
							mTrackEnabled = true;
						}
						// Save First PTS
						mFirstPTS = firstFragmentToFetch->GetPTS();
						AAMPLOG_INFO("[%s] startPosition:%lf relative:%lf rate:%f pts:%lf Window:(%lf..%lf)", GetMediaTypeName(mMediaType), mStartPosition, firstFragmentToFetch->GetRelativePosition(), mCurrentRate, mFirstPTS, firstFragment->GetRelativePosition(), lastFragment->GetRelativePosition());
						mInitialized_ = true;
						startPos = firstFragmentToFetch->GetPosition();
					}
					else
					{
						AAMPLOG_ERR("[%s] FirstFragmentToFetch is null", GetMediaTypeName(mMediaType));
						// TODO : Commented this one because of TrackEnabled() dependency
						//			This should be done in a same way as StreamSelection does
						//			Otherwise we will get init failure from disabled tracks
						// ret = eAAMPSTATUS_SEEK_RANGE_ERROR;
					}
				}
			}
			else
			{
				AAMPLOG_INFO("No data manager found for mediatype[%s]", GetMediaTypeName(mMediaType));
				ret = eAAMPSTATUS_INVALID_PLAYLIST_ERROR;
			}
		}
		else
		{
			ret = eAAMPSTATUS_SEEK_RANGE_ERROR;
			mInitialized_ = false;
		}
	}
	return ret;
}

/**
 * @fn ReadNext - function to read file from TSB
 *
 * @return pointer of TsbFragmentData
 */
std::shared_ptr<TsbFragmentData> AampTsbReader::ReadNext()
{
	if (!mInitialized_)
	{
		AAMPLOG_ERR("TsbReader[%s] not initialized", GetMediaTypeName(mMediaType));
		return nullptr;
	}
	/**< Reset while reading new fragment*/
	mIsPeriodBoundary = false;
	TsbFragmentDataPtr ret = mDataMgr->GetFragment(mUpcomingFragmentPosition, mEosReached);
	if (!ret)
	{
		AAMPLOG_TRACE("[%s]Retrying fragment to fetch at position: %lf, EOS:%d", GetMediaTypeName(mMediaType), mUpcomingFragmentPosition, mEosReached);
		double correctedPosition = mUpcomingFragmentPosition - FLOATING_POINT_EPSILON;
		ret = mDataMgr->GetNearestFragment(correctedPosition);
		if (!ret)
		{
			// Return a nullptr if fragment not found
			AAMPLOG_ERR("[%s]Fragment null", GetMediaTypeName(mMediaType));
			return ret;
		}
		// Error Handling
		// We are skipping one fragment if not found based on the rate
		if (mCurrentRate > 0 && (ret->GetPosition() + FLOATING_POINT_EPSILON) < mUpcomingFragmentPosition)
		{
			// Forward rate
			// Nearest fragment behind mUpcomingFragmentPosition calculated based of last fragment info
			// The fragment currently active in 'ret' should be the last injected fragment.
			// Therefore, retrieve the next fragment from the linked list if it exists, otherwise, return nullptr.
			ret = ret->next;
		}
		else if (mCurrentRate < 0 && (ret->GetPosition() - FLOATING_POINT_EPSILON) > mUpcomingFragmentPosition)
		{
			// Rewinding
			// Nearest fragment ahead of mUpcomingFragmentPosition calculated based of last fragment info
			// The fragment currently active in 'ret' should be the last injected fragment.
			// Therefore, retrieve the previous fragment from the linked list if it exists, otherwise, return nullptr.
			ret = ret->prev;
		}
		if (!ret)
		{
			AAMPLOG_TRACE("[%s] Retry is also failing, returning nullptr", GetMediaTypeName(mMediaType));
			if (AAMP_NORMAL_PLAY_RATE != mCurrentRate)
			{
				// Culling segment, but EOS never marked
				mEosReached = true;
			}
			return ret;
		}
	}
	// Mark EOS, for forward iteration check next fragment, for reverse check prev
	mEosReached = (mCurrentRate >= 0) ? (ret->next == nullptr) : (ret->prev == nullptr);
	// Compliment this state with last init header push status
	if (mActiveTuneType == eTUNETYPE_SEEKTOLIVE)
	{
		mEosReached &= !mNewInitWaiting;
	}
	// Determine if the next fragment is discontinuous.
	// For forward iteration, examine the discontinuity marker in the next fragment.
	// For reverse iteration, inspect the discontinuity marker in the current fragment,
	//		indicating that the upcoming iteration will transition to a different period.
	mIsNextFragmentDisc = (mCurrentRate >= 0) ? (ret ? ret->IsDiscontinuous() : false) : (ret->next ? ret->next->IsDiscontinuous() : false);
	if (AAMP_NORMAL_PLAY_RATE == mCurrentRate)
	{
		DetectDiscontinuity(ret);
	}
	mUpcomingFragmentPosition += (mCurrentRate >= 0) ? ret->GetDuration() : -ret->GetDuration();
	AAMPLOG_INFO("[%s] Returning fragment Pos:%lf PTS %lf relativePos:%lf Next:%lf EOS(%d), initWaiting(%d) discontinuity(%d) mIsPeriodBoundary(%d) period(%s) Url: %s",
				 GetMediaTypeName(mMediaType), ret->GetPosition(), ret->GetPTS(), ret->GetRelativePosition(), mUpcomingFragmentPosition, mEosReached, mNewInitWaiting, mIsNextFragmentDisc, mIsPeriodBoundary, ret->GetPeriodId().c_str(), ret->GetUrl().c_str());

	return ret;
}

/**
 * @fn GetStartPosition
 *
 * @return startPosition
 */
void AampTsbReader::DetectDiscontinuity(TsbFragmentDataPtr currFragment)
{
	/**
	 * Current rate > 0 check the current Period Id is equal to next fragment period Id else current period Iid is equal to previous fragment
	 * period Id
	 */
	if (!IsFirstDownload())
	{
		mIsPeriodBoundary = (mCurrentRate >= 0) ? (currFragment->prev ? (currFragment->GetPeriodId() != currFragment->prev->GetPeriodId()) : false) : (currFragment->next ? (currFragment->GetPeriodId() != currFragment->next->GetPeriodId()) : false);
	}
	if (mIsPeriodBoundary)
	{
		// Countinious PTS check
		TsbFragmentDataPtr adjFragment = (mCurrentRate >= 0) ? currFragment->prev : currFragment->next;
		if (adjFragment)
		{
			double nextPTSCal = (adjFragment->GetPTS()) + ((mCurrentRate >= 0) ? adjFragment->GetDuration() : -adjFragment->GetDuration());
			if (nextPTSCal != currFragment->GetPTS())
			{
				mIsPeriodBoundary = true;
				mFirstPTS = currFragment->GetPTS();
				AAMPLOG_INFO("Discontinuity detected at PTS position %lf", mFirstPTS);
			}
			else
			{
				mIsPeriodBoundary = false;
			}
		}
	}
}

/**
 * @fn GetStartPosition
 *
 * @return startPosition
 */
double AampTsbReader::GetStartPosition()
{
	double ret = 0.0;
	if (mStartPosition >= 0)
	{
		// Take the relative start position based on data manager start
		TsbFragmentDataPtr firstFragmentToFetch = mDataMgr->GetNearestFragment((mStartPosition - FLOATING_POINT_EPSILON));
		if (firstFragmentToFetch)
		{
			ret = firstFragmentToFetch->GetRelativePosition();
			AAMPLOG_INFO("[%s] Data manager current start : %lf, returing start position as %lf", GetMediaTypeName(mMediaType), mDataMgr->GetFirstFragmentPosition(), ret);
		}
	}
	return ret;
}

/**
 * @fn DeInit  - function to clear TsbReader states
 */
void AampTsbReader::DeInit()
{
	mStartPosition = 0.0;
	mUpcomingFragmentPosition = 0.0;
	mCurrentRate = AAMP_NORMAL_PLAY_RATE;
	mInitialized_ = false;
	mEosReached = false;
	mTrackEnabled = false;
	mFirstPTS = 0.0;
	mCurrentBandwidth = 0.0;
	mActiveTuneType = eTUNETYPE_NEW_NORMAL;
	mIsPeriodBoundary = false;
	mIsEndFragmentInjected.store(false);
	AAMPLOG_INFO("mediaType : %s", GetMediaTypeName(mMediaType));
}

/**
 * @brief CheckForWaitIfReaderDone  - fn to wait for reader to inject end fragment
 */
void AampTsbReader::CheckForWaitIfReaderDone()
{
	std::unique_lock<std::mutex> lock(mEosMutex);
	if (!IsEndFragmentInjected())
	{
		AAMPLOG_INFO("[%s] Waiting for last fragment injection update", GetMediaTypeName(mMediaType));
		mEosCVWait.wait(lock, [this]
						{ return IsEndFragmentInjected(); });
	}
	AAMPLOG_INFO("[%s] Exiting", GetMediaTypeName(mMediaType));
}

/**
 * @brief AbortCheckForWaitIfReaderDone - fn to set the reader end fragment injected
 */
void AampTsbReader::AbortCheckForWaitIfReaderDone()
{
	std::unique_lock<std::mutex> lock(mEosMutex);
	if (!IsEndFragmentInjected())
	{
		SetEndFragmentInjected();
		mEosCVWait.notify_one();
	}
}
