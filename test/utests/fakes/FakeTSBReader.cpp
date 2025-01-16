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

/**************************************
 * @file AampTsbReader.cpp
 * @brief TSBSession Manager for Aamp
 **************************************/


#include "AampTsbReader.h"
#include "MockTSBReader.h"

MockTSBReader *g_mockTSBReader = nullptr;
/**
 * @fn AampTsbReader Constructor
 *
 * @return None
 */
AampTsbReader::AampTsbReader(PrivateInstanceAAMP *aamp, std::shared_ptr<AampTsbDataManager> dataMgr, AampMediaType mediaType, std::string sessionId)
	: mAamp(aamp), mDataMgr(dataMgr), mMediaType(mediaType), mInitialized_(false), mStartPosition(0.0),
		mUpcomingFragmentPosition(0.0), mCurrentRate(AAMP_NORMAL_PLAY_RATE), mTsbSessionId(sessionId), mEosReached(false), mTrackEnabled(false),
		mFirstPTS(0.0), mCurrentBandwidth(0.0), mNewInitWaiting(false), mActiveTuneType(eTUNETYPE_NEW_NORMAL)
{
}

/**
 * @fn AampTsbReader Destructor
 *
 * @return None
 */
AampTsbReader::~AampTsbReader()
{
}

double AampTsbReader::GetStartPosition()
{
	return 0;
}

void AampTsbReader::Term()
{

}

AAMPStatusType AampTsbReader::Init(double &startPos, float rate, TuneType tuneType, std::shared_ptr<AampTsbReader> other)
{
	return eAAMPSTATUS_OK;
}

std::shared_ptr<TsbFragmentData> AampTsbReader::ReadNext()
{
	return nullptr;
}

void AampTsbReader::DetectDiscontinuity(TsbFragmentDataPtr  currFragment)
{
}

void AampTsbReader::CheckForWaitIfReaderDone()
{
}

void AampTsbReader::AbortCheckForWaitIfReaderDone()
{
}
