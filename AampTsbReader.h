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
 * @file AampTsbReader.h
 * @brief AampTsbReader for AAMP
 */

#ifndef AAMP_TSBREADER_H
#define AAMP_TSBREADER_H

#include "AampTsbDataManager.h"
#include "AampMediaType.h"
#include "AampTime.h"

/**
 * @class AampTsbReader
 * @brief AampTsbReader Class defn
 */
class AampTsbReader
{
public:
	/**
	 * @fn AampTsbReader Constructor
	 *
	 * @return None
	 */
	AampTsbReader(PrivateInstanceAAMP *aamp, std::shared_ptr<AampTsbDataManager> dataMgr, AampMediaType mediaType, std::string sessionId);

	/**
	 * @fn AampTsbReader Destructor
	 *
	 * @return None
	 */
	~AampTsbReader();

	/**
	 * @fn AampTsbReader Init function
	 * @brief Initialize TSB reader
	 *
	 * @param[in,out] startPosSec - Start absolute position, seconds since 1970; in: requested, out: selected
	 * @param[in] rate - Playback rate
	 * @param[in] tuneType - Type of tune
	 * @param[in] other - Optional other TSB reader
	 *
	 * @return AAMPStatusType
	 */
	AAMPStatusType Init(double &startPosSec, float rate, TuneType tuneType, std::shared_ptr<AampTsbReader> other=nullptr);

	/**
	 * @fn FindNext - function to find the next fragment from TSB
	 *
	 * @param[in] offset - Offset from last read fragment
	 *
	 * @return Pointer to the next fragment data
	 */
	TsbFragmentDataPtr FindNext();

	/**
	 * @fn ReadNext - function to update the last read file from TSB
	 *
	 * @param[in] nextFragmentData - Next fragment data obtained previously with FindNext
	 */
	void ReadNext(TsbFragmentDataPtr nextFragmentData);

	/**
	 * @fn GetStartPosition
	 *
	 * @return AampTime - Start position
	 */
	AampTime GetStartPosition();

	/**
	 * @fn Flush  - function to clear the TSB storage
	 *
	 * @return None
	 */
	void Term();

	/**
	 * @fn IsEos  - function to get EOS status
	 *
	 * @return bool - EOS
	 */
	bool IsEos() { return mEosReached; }

	/**
	 * @fn Reset EOS
	 */
	void ResetEos() { mEosReached = false; }

	/**
	 * @fn Set Unprocessed init header flag
	 *
	 * @param value
	 */
	void SetNewInitWaiting(bool value) { mNewInitWaiting = value; }

	/**
	 * @fn IsFirstDownload
	 *
	 * @return bool - true if first download
	 */
	bool IsFirstDownload();

	/**
	 * @fn TrackEnabled
	 *
	 * @return bool - true if enabled
	 */
	bool TrackEnabled() { return !IsEos() && mTrackEnabled; }

	/**
	 * @fn GetFirstPTS
	 *
	 * @return double - First PTS
	 */
  
	double GetFirstPTS();

	/**
	 * @fn GetFirstPTSOffset
	 *
	 * @return AampTime - First PTS Offset
	 */
	AampTime GetFirstPTSOffset();

	/**
	 * @fn GetMediaType
	 *
	 * @return mMediaType - Media type
	 */
	AampMediaType GetMediaType() { return mMediaType; }

	/**
	 * @fn GetPlaybackRate
	 *
	 * @return float - Playback rate
	 */
	float GetPlaybackRate();

	/**
	 * @fn IsDiscontinuous
	 *
	 * @return bool - Is discontinuous or not
	 */
	bool IsDiscontinuous() { return mIsNextFragmentDisc; }

	/**
	 * @fn IsPeriodBoundary
	 *
	 * @return bool - Is PeriodId changed or not
	 */
	bool IsPeriodBoundary() { return mIsPeriodBoundary; }

	/**
	 * @fn CheckForWaitIfReaderDone  - fn to wait for reader to inject end fragment
	 */
	void CheckForWaitIfReaderDone();

	/**
	 * @brief AbortCheckForWaitIfReaderDone - fn to set the reader end fragment injected
	 */
	void AbortCheckForWaitIfReaderDone();

	/**
	 * @fn IsEndFragmentInjected - Is the end fragment injected
	 *
	 * @return bool - true if end fragment injected
	 */
	bool IsEndFragmentInjected() { return mIsEndFragmentInjected.load(); }

	/**
	 * @fn SetEndFragmentInjected- Set the end fragment injected
	 */
	void SetEndFragmentInjected() { mIsEndFragmentInjected.store(true); }

private:

	bool mInitialized_;

	AampTime mStartPosition;
	float mCurrentRate;
	std::string mTsbSessionId;
	AampMediaType mMediaType;
	AampTime mFirstPTS;
	AampTime mFirstPTSOffset;
	bool mNewInitWaiting;
	TuneType mActiveTuneType;
	bool mIsNextFragmentDisc;
	bool mIsPeriodBoundary;
	std::atomic<bool> mIsEndFragmentInjected;
	std::mutex mEosMutex;					/**< EOS mutex for conditional, used for syncing live downloader and reader*/
	std::condition_variable mEosCVWait;	/**< Conditional variable for signaling wait*/
	TsbFragmentDataPtr mCurrentFragment;	/**< Current fragment pointer for list navigation*/

protected:
	AampTime mUpcomingFragmentPosition;
	/**
	 * @fn CheckPeriodBoundary
	 * 
	 * @param[in] currFragment - Current fragment
	 */
	void CheckPeriodBoundary(TsbFragmentDataPtr currFragment);

public:
	PrivateInstanceAAMP *mAamp;
	bool mEosReached;
	bool mTrackEnabled;
	std::shared_ptr<AampTsbDataManager> mDataMgr;
	double mCurrentBandwidth;
	TsbInitDataPtr mLastInitFragmentData;
};

#endif // AAMP_TSBREADER_H
