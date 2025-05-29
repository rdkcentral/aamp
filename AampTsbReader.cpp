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
	: mAamp(aamp), mDataMgr(std::move(dataMgr)), mMediaType(mediaType), mInitialized_(false), mStartPosition(0.0),
	  mUpcomingFragmentPosition(0.0), mCurrentRate(AAMP_NORMAL_PLAY_RATE), mTsbSessionId(std::move(sessionId)), mEosReached(false), mTrackEnabled(false),
	  mFirstPTS(0.0), mCurrentBandwidth(0.0), mNewInitWaiting(false), mActiveTuneType(eTUNETYPE_NEW_NORMAL),
	  mEosCVWait(), mEosMutex(), mIsEndFragmentInjected(false), mLastInitFragmentData(nullptr), mIsNextFragmentDisc(false), mIsPeriodBoundary(false)
{
	AAMPLOG_INFO("[%s] Constructor", GetMediaTypeName(mMediaType));
}

/**
 * @fn AampTsbReader Destructor
 *
 * @return None
 */
AampTsbReader::~AampTsbReader()
{
	AAMPLOG_INFO("[%s] Destructor", GetMediaTypeName(mMediaType));
	Term();
}

/**
 * @fn AampTsbReader Init function
 *
 * @param[in,out] startPosSec - Start absolute position, seconds since 1970; in: requested, out: selected
 * @param[in] rate - Playback rate
 * @param[in] tuneType - Type of tune
 * @param[in] other - Optional other TSB reader
 *
 * @return AAMPStatusType
 */
AAMPStatusType AampTsbReader::Init(double &startPosSec, float rate, TuneType tuneType, std::shared_ptr<AampTsbReader> other)
{
	AAMPStatusType ret = eAAMPSTATUS_OK;
	if (!mInitialized_)
	{
		if (startPosSec >= 0)
		{
			if (mDataMgr)
			{
				TsbFragmentDataPtr firstFragment = mDataMgr->GetFirstFragment();
				TsbFragmentDataPtr lastFragment = mDataMgr->GetLastFragment();
				double requestedPosition = 0.0;
				mActiveTuneType = tuneType;
				if (!(firstFragment && lastFragment))
				{
					// No fragments available
					AAMPLOG_WARN("[%s] TSB is empty", GetMediaTypeName(mMediaType));
					mTrackEnabled = false;
				}
				else
				{
					if (lastFragment->GetAbsolutePosition() < startPosSec)
					{
						// Handle seek out of range
						AAMPLOG_WARN("[%s] Seeking to the TSB End: %lfs (Requested:%lfs), Range:(%lfs-%lfs) tunetype:%d", GetMediaTypeName(mMediaType), lastFragment->GetAbsolutePosition().inSeconds(), startPosSec, firstFragment->GetAbsolutePosition().inSeconds(), lastFragment->GetAbsolutePosition().inSeconds(), mActiveTuneType);
						requestedPosition = lastFragment->GetAbsolutePosition().inSeconds();
					}
					else
					{
						// Adjust the position according to the stored values.
						requestedPosition = startPosSec;
					}
					TsbFragmentDataPtr firstFragmentToFetch = mDataMgr->GetNearestFragment(requestedPosition);
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
						mStartPosition = firstFragmentToFetch->GetAbsolutePosition().inSeconds();
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
						mFirstPTS = firstFragmentToFetch->GetPTS().inSeconds();
						AAMPLOG_INFO("[%s] startPosition:%lfs rate:%f pts:%lfs Range:(%lfs-%lfs)", GetMediaTypeName(mMediaType), mStartPosition, mCurrentRate, mFirstPTS, firstFragment->GetAbsolutePosition().inSeconds(), lastFragment->GetAbsolutePosition().inSeconds());
						mInitialized_ = true;
						startPosSec = firstFragmentToFetch->GetAbsolutePosition().inSeconds();
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
			AAMPLOG_ERR("[%s] Negative position requested %fs", GetMediaTypeName(mMediaType), startPosSec);
			ret = eAAMPSTATUS_SEEK_RANGE_ERROR;
			mInitialized_ = false;
		}
	}
	return ret;
}

/**
 * @fn FindNext - function to find the next fragment from TSB
 *
 * @param[in] offset - Offset from last read fragment
 *
 * @return Pointer to the next fragment data
 */
TsbFragmentDataPtr AampTsbReader::FindNext(AampTime offset)
{
	if (!mInitialized_)
	{
		AAMPLOG_ERR("TsbReader[%s] not initialized", GetMediaTypeName(mMediaType));
		return nullptr;
	}

	double position = mUpcomingFragmentPosition;
	if (mCurrentRate >= 0)
	{
		position += offset.inSeconds();
	}
	else
	{
		position -= offset.inSeconds();
	}

	bool eos;
	TsbFragmentDataPtr ret = mDataMgr->GetFragment(position, eos);
	if (!ret)
	{
		AAMPLOG_TRACE("[%s]Retrying fragment to fetch at position: %lf", GetMediaTypeName(mMediaType), position);
		double correctedPosition = position - FLOATING_POINT_EPSILON;
		ret = mDataMgr->GetNearestFragment(correctedPosition);
		if (!ret)
		{
			// Return a nullptr if fragment not found
			AAMPLOG_ERR("[%s]Fragment null", GetMediaTypeName(mMediaType));
			return ret;
		}
		// Error Handling
		// We are skipping one fragment if not found based on the rate
		if (mCurrentRate > 0 && (ret->GetAbsolutePosition() + FLOATING_POINT_EPSILON) < position)
		{
			// Forward rate
			// Nearest fragment behind position calculated based of last fragment info
			// The fragment currently active in 'ret' should be the last injected fragment.
			// Therefore, retrieve the next fragment from the linked list if it exists, otherwise, return nullptr.
			ret = ret->next;
		}
		else if (mCurrentRate < 0 && (ret->GetAbsolutePosition() - FLOATING_POINT_EPSILON) > position)
		{
			// Rewinding
			// Nearest fragment ahead of position calculated based of last fragment info
			// The fragment currently active in 'ret' should be the last injected fragment.
			// Therefore, retrieve the previous fragment from the linked list if it exists, otherwise, return nullptr.
			ret = ret->prev;
		}
		if (!ret)
		{
			AAMPLOG_INFO("[%s] Retry is also failing mCurrentRate %f, returning nullptr ", GetMediaTypeName(mMediaType), mCurrentRate);
			if (AAMP_NORMAL_PLAY_RATE != mCurrentRate)
			{
				// Culling segment, but EOS never marked
				mEosReached = true;
			}
			return ret;
		}
	}
	AAMPLOG_INFO("[%s] Returning fragment: absPos %lfs pts %lfs period %s timeScale %u ptsOffset %fs url %s",
		GetMediaTypeName(mMediaType), ret->GetAbsolutePosition().inSeconds(), ret->GetPTS().inSeconds(), ret->GetPeriodId().c_str(), ret->GetTimeScale(), ret->GetPTSOffsetSec().inSeconds(), ret->GetUrl().c_str());

	return ret;
}

/**
 * @fn ReadNext - function to update the last read file from TSB
 *
 * @param[in] nextFragmentData - Next fragment data obtained previously from FindNext
 */
void AampTsbReader::ReadNext(TsbFragmentDataPtr nextFragmentData)
{
	if (nextFragmentData != nullptr)
	{
		// Mark EOS, for forward iteration check next fragment, for reverse check prev
		mEosReached = (mCurrentRate >= 0) ? (nextFragmentData->next == nullptr) : (nextFragmentData->prev == nullptr);
		// Compliment this state with last init header push status
		if (mActiveTuneType == eTUNETYPE_SEEKTOLIVE)
		{
			mEosReached &= !mNewInitWaiting;
		}
		// Determine if the next fragment is discontinuous.
		// For forward iteration, examine the discontinuity marker in the next fragment.
		// For reverse iteration, inspect the discontinuity marker in the current fragment,
		//		indicating that the upcoming iteration will transition to a different period.
		if (mCurrentRate >= 0)
		{
			mIsNextFragmentDisc = nextFragmentData->IsDiscontinuous();
		}
		else
		{
			mIsNextFragmentDisc = (nextFragmentData->next && nextFragmentData->next->IsDiscontinuous());
		}

		if (!IsFirstDownload())
		{
			CheckPeriodBoundary(nextFragmentData);
		}
		mUpcomingFragmentPosition += (mCurrentRate >= 0) ? nextFragmentData->GetDuration() : -nextFragmentData->GetDuration();
		AAMPLOG_INFO("[%s] Fragment: absPos %lfs next %lfs eos %d initWaiting %d mIsNextFragmentDisc %d mIsPeriodBoundary %d",
			GetMediaTypeName(mMediaType), nextFragmentData->GetAbsolutePosition().inSeconds(), mUpcomingFragmentPosition, mEosReached, mNewInitWaiting, mIsNextFragmentDisc, mIsPeriodBoundary);
	}
}

/**
 * @fn CheckPeriodBoundary
 *
 * @param[in] currFragment - Current fragment
 */
void AampTsbReader::CheckPeriodBoundary(TsbFragmentDataPtr currFragment)
{
	mIsPeriodBoundary = false;

	TsbFragmentDataPtr adjFragment = (mCurrentRate >= 0) ? currFragment->prev : currFragment->next;
	if (adjFragment)
	{
		mIsPeriodBoundary = (currFragment->GetPeriodId() != adjFragment->GetPeriodId());
	}

	if (mIsPeriodBoundary && (AAMP_NORMAL_PLAY_RATE == mCurrentRate))
	{
		AampTime nextPTSCal = (adjFragment->GetPTS()) + ((mCurrentRate >= 0) ? adjFragment->GetDuration() : -adjFragment->GetDuration());
		if (nextPTSCal != currFragment->GetPTS())
		{
			mFirstPTS = currFragment->GetPTS().inSeconds();
			AAMPLOG_INFO("Discontinuity detected at PTS position %lf", mFirstPTS);
		}
	}
}

/**
 * @fn Term  - function to clear TsbReader states
 */
void AampTsbReader::Term()
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
	mLastInitFragmentData = nullptr;
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

	/**
	 * @fn IsFirstDownload
	 * @return True if first download
	 */
	bool AampTsbReader::IsFirstDownload()
	{
		return (mStartPosition == mUpcomingFragmentPosition);
	}

	/**
	 * @fn GetPlaybackRate
	 * @return Playback rate
	 */
	float AampTsbReader::GetPlaybackRate()
	{
		return mCurrentRate;
	}
