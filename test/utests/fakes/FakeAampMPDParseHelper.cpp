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


/**
 *   @fn AampMPDParseHelper
 *   @brief Default Constructor
 */
AampMPDParseHelper::AampMPDParseHelper() 	: mMPDInstance(NULL),mIsLiveManifest(false),mMinUpdateDurationMs(0),
	mIsFogMPD(false),
	mAvailabilityStartTime(0.0),mSegmentDurationSeconds(0),mTSBDepth(0.0),
	mPresentationOffsetDelay(0.0),mMediaPresentationDuration(0),
	mMyObjectMutex(),mPeriodEncryptionMap(),mNumberOfPeriods(0),mPeriodEmptyMap()
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
 * @brief Get content protection from represetation/adaptation field
 * @retval content protections if present. Else NULL.
 */
vector<IDescriptor*> AampMPDParseHelper::GetContentProtection(const IAdaptationSet *adaptationSet)
{
	//Priority for representation.If the content protection not available in the representation, go with adaptation set
	return std::vector<IDescriptor*>();
}

bool AampMPDParseHelper::IsPeriodEncrypted(int iPeriodIndex)
{
	return 0;
}


/**
 * @brief Check if Period is empty or not
 * @retval Return true on empty Period
 */
bool AampMPDParseHelper::IsEmptyPeriod(int iPeriodIndex, bool checkIframe) 
{
	return 0;
}

/**
 * @brief Check if Period is empty or not
 * @retval Return true on empty Period
 */
bool AampMPDParseHelper::IsEmptyAdaptation(IAdaptationSet *adaptationSet)
{
	return 0;
}

/**
 * @brief Check if adaptation set is iframe track
 * @param adaptationSet Pointer to adaptainSet
 * @retval true if iframe track
 */
bool AampMPDParseHelper::IsIframeTrack(IAdaptationSet *adaptationSet)
{
	return false;
}
