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

/**************************************
 * @file AampMPDParseHelper.cpp
 * @brief Helper Class for MPD Parsing
 **************************************/

#include "AampMPDParseHelper.h"
#include "AampUtils.h"
#include "AampLogManager.h"
#include "MockAampMPDParseHelper.h"

MockAampMPDParseHelper *g_mockAampMPDParseHelper;
/**
 *   @fn AampMPDParseHelper
 *   @brief Default Constructor
 */
AampMPDParseHelper::AampMPDParseHelper() 	: mMPDInstance(NULL),mIsLiveManifest(false),mMinUpdateDurationMs(0),
	mIsFogMPD(false),
	mAvailabilityStartTime(0.0),mSegmentDurationSeconds(0),mTSBDepth(0.0),
	mPresentationOffsetDelay(0.0),mMediaPresentationDuration(0),
	mMyObjectMutex(),mPeriodEncryptionMap(),mNumberOfPeriods(0),mPeriodEmptyMap(),mLiveTimeFragmentSync(false),mHasServerUtcTime(false),mUpperBoundaryPeriod(0),mLowerBoundaryPeriod(0),mMPDPeriodDetails()
{

}

/**
 *   @fn AampMPDParseHelper
 *   @brief Destructor
 */
AampMPDParseHelper::~AampMPDParseHelper()
{


}

/**
 *   @fn Clear
 *   @brief reset all values
 */
void AampMPDParseHelper::Initialize(dash::mpd::IMPD *instance)
{
}

/**
 *   @fn Clear
 *   @brief reset all values
 */
void AampMPDParseHelper::Clear()
{
}


/**
 * @brief Get content protection from representation/adaptation field
 * @retval content protections if present. Else NULL.
 */
vector<IDescriptor*> AampMPDParseHelper::GetContentProtection(const IAdaptationSet *adaptationSet)
{
	//Priority for representation.If the content protection not available in the representation, go with adaptation set
	return std::vector<IDescriptor*>();
}

bool AampMPDParseHelper::IsPeriodEncrypted(int iPeriodIndex)
{
	return false;
}


/**
 * @brief Check if Period is empty or not
 * @retval Return true on empty Period
 */
bool AampMPDParseHelper::IsEmptyPeriod(int iPeriodIndex, bool checkIframe) 
{
	return false;
}

/**
 * @brief Check if Period is empty or not
 * @retval Return true on empty Period
 */
bool AampMPDParseHelper::IsEmptyAdaptation(IAdaptationSet *adaptationSet)
{
	return false;
}

/**
 * @brief Check if adaptation set is iframe track
 * @param adaptationSet Pointer to adaptationSet
 * @retval true if iframe track
 */
bool AampMPDParseHelper::IsIframeTrack(IAdaptationSet *adaptationSet)
{
	return false;
}
/**
 *   @brief  Get Period Duration
 *   @retval period duration in milliseconds
 */
double AampMPDParseHelper::aamp_GetPeriodDuration(int periodIndex, uint64_t mpdDownloadTime)
{
	return 0.0;
}

/**
 * @brief Check if adaptation set is of a given media type
 * @retval true if adaptation set is of the given media type
 */
bool IsContentType(const IAdaptationSet *adaptationSet, AampMediaType mediaType )
{
	return false;
}

/**
 * @fn GetPeriodDuration
 * @param mpd : pointer manifest
 * @param periodIndex Index of the current period
 */
double AampMPDParseHelper::GetPeriodDuration(int periodIndex, uint64_t mLastPlaylistDownloadTimeMs, bool checkIFrame, bool IsUninterruptedTSB)
{
	double x = 0.0;
	if (g_mockAampMPDParseHelper)
	{
		x = g_mockAampMPDParseHelper->GetPeriodDuration(periodIndex, mLastPlaylistDownloadTimeMs, checkIFrame, IsUninterruptedTSB);
	}
	return x;
}

/**
 * @fn aamp_GetPeriodStartTimeDeltaRelativeToPTSOffset
 * @param period period of segment
 */
double AampMPDParseHelper::aamp_GetPeriodStartTimeDeltaRelativeToPTSOffset(IPeriod * period)
{
	return 0.0;
}

/**
 * @fn to Update the upper and lower boundary periods
 * @param IsTrickMode A flag indicating whether playback is in trick mode or not
 */
void AampMPDParseHelper::UpdateBoundaryPeriod(bool IsTrickMode)
{
}

/**
 * @brief Get start time of current period
 * @retval current period's start time
 */
double AampMPDParseHelper::GetPeriodStartTime(int periodIndex,uint64_t mLastPlaylistDownloadTimeMs)
{
	return 0.0;
}

/**
 * @fn getPeriodIdx
 * @brief Function to get base period index from mpd
 * @param[in] periodId.
 * @retval period index.
 */
int AampMPDParseHelper::getPeriodIdx(const std::string &periodId)
{
	return 0;
}

/**
 * @brief  GetFirstSegment start time from period
 * @param  period
 * @param  type media type
 * @retval start time
 */
double AampMPDParseHelper::GetFirstSegmentScaledStartTime(IPeriod *period, AampMediaType type)
{
	double x = 0.0;
	if (g_mockAampMPDParseHelper)
	{
		x = g_mockAampMPDParseHelper->GetFirstSegmentScaledStartTime(period, type);
	}
	return x;
}

uint64_t AampMPDParseHelper::GetDurationFromRepresentation()
{
	return 0;
}

bool AampMPDParseHelper::IsContentType(const IAdaptationSet *adaptationSet, AampMediaType mediaType )
{
	return false;
}

vector<Representation *> AampMPDParseHelper::GetBitrateInfoFromCustomMpd( const IAdaptationSet *adaptationSet)
{
	return vector<Representation*>();
}

double AampMPDParseHelper::GetPeriodEndTime(int periodIndex, uint64_t mLastPlaylistDownloadTimeMs, bool checkIFrame, bool IsUninterruptedTSB)
{
	return 0.0;
}

uint32_t AampMPDParseHelper::GetPeriodSegmentTimeScale(IPeriod * period)
{
	return 0;
}

uint64_t AampMPDParseHelper::GetFirstSegmentStartTime(IPeriod * period)
{
	return 0;
}

void AampMPDParseHelper::GetStartAndDurationFromTimeline(IPeriod * period, int representationIdx, int adaptationSetIdx, AampTime &scaledStartTime, AampTime &duration)
{
	if (g_mockAampMPDParseHelper)
	{
		g_mockAampMPDParseHelper->GetStartAndDurationFromTimeline(period, representationIdx, adaptationSetIdx, scaledStartTime, duration);
	}
}

double AampMPDParseHelper::GetPeriodNewContentDurationMs(IPeriod * period, uint64_t &curEndNumber)
{
	return 0;
}
