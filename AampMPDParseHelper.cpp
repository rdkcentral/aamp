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
				mMyObjectMutex(),mPeriodEncryptionMap(),mNumberOfPeriods(0),mPeriodEmptyMap(),mLiveTimeFragmentSync(false),mHasServerUtcTime(false),mUpperBoundaryPeriod(0),mLowerBoundaryPeriod(0),mMPDPeriodDetails(),mDeltaTime(0.0)
{
}

/**
*   @fn AampMPDParseHelper
*   @brief Destructor
*/
AampMPDParseHelper::~AampMPDParseHelper()
{
	Clear();
}

/**
*  @fn AampMPDParseHelper
*  @brief Copy Constructor
*/
AampMPDParseHelper::AampMPDParseHelper(const AampMPDParseHelper& cachedMPD) : mIsLiveManifest(cachedMPD.mIsLiveManifest), mIsFogMPD(cachedMPD.mIsFogMPD),
					mMinUpdateDurationMs(cachedMPD.mMinUpdateDurationMs), mAvailabilityStartTime(cachedMPD.mAvailabilityStartTime),
					   mSegmentDurationSeconds(cachedMPD.mSegmentDurationSeconds), mTSBDepth(cachedMPD.mTSBDepth),
					   mPresentationOffsetDelay(cachedMPD.mPresentationOffsetDelay), mMediaPresentationDuration(cachedMPD.mMediaPresentationDuration),
					   mMyObjectMutex(), mNumberOfPeriods(cachedMPD.mNumberOfPeriods) , mPeriodEncryptionMap(cachedMPD.mPeriodEncryptionMap),
					   mPeriodEmptyMap(cachedMPD.mPeriodEmptyMap) , mMPDInstance(NULL),mLiveTimeFragmentSync(cachedMPD.mLiveTimeFragmentSync),mHasServerUtcTime(cachedMPD.mHasServerUtcTime),mUpperBoundaryPeriod(cachedMPD.mUpperBoundaryPeriod),mLowerBoundaryPeriod(cachedMPD.mLowerBoundaryPeriod),mMPDPeriodDetails(cachedMPD.mMPDPeriodDetails),mDeltaTime(cachedMPD.mDeltaTime)

{
}

/**
*   @fn Clear
*   @brief reset all values
*/
void AampMPDParseHelper::Initialize(dash::mpd::IMPD *instance)
{
	Clear();
	std::unique_lock<std::mutex> lck(mMyObjectMutex);
	if(instance != NULL)
	{
		mMPDInstance    =   instance;
		parseMPD();
	}
}

/**
*   @fn Clear
*   @brief reset all values
*/
void AampMPDParseHelper::Clear()
{
	std::unique_lock<std::mutex> lck(mMyObjectMutex);
	mMPDInstance    =   NULL;
	mIsLiveManifest =   false;
	mIsFogMPD       =   false;
	mMinUpdateDurationMs    =   0;
	mAvailabilityStartTime  =   0.0;
	mSegmentDurationSeconds =   0;
	mTSBDepth       =   0.0;
	mPresentationOffsetDelay    =   0.0;
	mMediaPresentationDuration     =   0;
	mNumberOfPeriods	=	0;
	mPeriodEncryptionMap.clear();
	mPeriodEmptyMap.clear();
	mLiveTimeFragmentSync = false;
	mHasServerUtcTime = false;
	mUpperBoundaryPeriod = 0;
	mLowerBoundaryPeriod = 0;
	mDeltaTime = 0.0;
	mMPDPeriodDetails.clear();

}

/**
*   @fn parseMPD
*   @brief function to parse the MPD
*/
void AampMPDParseHelper::parseMPD()
{
	mIsLiveManifest   =       !(mMPDInstance->GetType() == "static");

	std::string tempStr = mMPDInstance->GetMinimumUpdatePeriod();
	if(!tempStr.empty())
	{
		mMinUpdateDurationMs  = ParseISO8601Duration( tempStr.c_str() );
	}
	else
	{
		mMinUpdateDurationMs = DEFAULT_INTERVAL_BETWEEN_MPD_UPDATES_MS;
	}

	tempStr = mMPDInstance->GetAvailabilityStarttime();
	if(!tempStr.empty())
	{
		mAvailabilityStartTime = (double)ISO8601DateTimeToUTCSeconds(tempStr.c_str());
	}

	tempStr = mMPDInstance->GetTimeShiftBufferDepth();
	uint64_t timeshiftBufferDepthMS = 0;
	if(!tempStr.empty())
	{
		timeshiftBufferDepthMS = ParseISO8601Duration( tempStr.c_str() );
	}

	tempStr = mMPDInstance->GetMaxSegmentDuration();
	if(!tempStr.empty())
	{
		mSegmentDurationSeconds = ParseISO8601Duration( tempStr.c_str() )/1000;
	}

	if(timeshiftBufferDepthMS)
	{
		mTSBDepth = (double)timeshiftBufferDepthMS / 1000;
		// Add valid check for minimum size requirement here
		if(mTSBDepth < ( 4 * (double)mSegmentDurationSeconds))
		{
			mTSBDepth = ( 4 * (double)mSegmentDurationSeconds);
		}
	}

	tempStr = mMPDInstance->GetSuggestedPresentationDelay();
	uint64_t presentationDelay = 0;
	if(!tempStr.empty())
	{
		presentationDelay = ParseISO8601Duration( tempStr.c_str() );
	}
	if(presentationDelay)
	{
		mPresentationOffsetDelay = (double)presentationDelay / 1000;
	}
	else
	{
		tempStr = mMPDInstance->GetMinBufferTime();
		uint64_t minimumBufferTime = 0;
		if(!tempStr.empty())
		{
			minimumBufferTime = ParseISO8601Duration( tempStr.c_str() );
		}
		if(minimumBufferTime)
		{
			mPresentationOffsetDelay = 	(double)minimumBufferTime / 1000;
		}
		else
		{
			mPresentationOffsetDelay = 2.0;
		}
	}

	tempStr =  mMPDInstance->GetMediaPresentationDuration();
	if(!tempStr.empty())
	{
		mMediaPresentationDuration = ParseISO8601Duration( tempStr.c_str());
	}

	std::map<std::string, std::string> mpdAttributes = mMPDInstance->GetRawAttributes();
	if(mpdAttributes.find("fogtsb") != mpdAttributes.end())
	{
		mIsFogMPD = true;
	}

	mNumberOfPeriods = (int)mMPDInstance->GetPeriods().size();
}

/**
 * @fn to Update the upper and lower boundary periods
 * @param IsTrickMode A flag indicating whether playback is in trick mode or not
 */
void AampMPDParseHelper::UpdateBoundaryPeriod(bool IsTrickMode)
{
	mUpperBoundaryPeriod = mNumberOfPeriods - 1;
	mLowerBoundaryPeriod = 0;
	// Calculate lower boundary of playable periods, discard empty periods at the start
	for(int periodIter = 0; periodIter < mNumberOfPeriods; periodIter++)
	{
		if(IsEmptyPeriod(periodIter, IsTrickMode))
		{
			mLowerBoundaryPeriod++;
			continue;
		}
		break;
	}
	// Calculate upper boundary of playable periods, discard empty periods at the end
	for(int periodIter = mNumberOfPeriods-1; periodIter >= 0; periodIter--)
	{
		if(IsEmptyPeriod(periodIter, IsTrickMode))
		{
			mUpperBoundaryPeriod--;
			continue;
		}
		break;
	}	
}
/**
* @brief Get content protection from representation/adaptation field
* @retval content protections if present. Else NULL.
*/
vector<IDescriptor*> AampMPDParseHelper::GetContentProtection(const IAdaptationSet *adaptationSet)
{
	//Priority for representation.If the content protection not available in the representation, go with adaptation set
	if(adaptationSet->GetRepresentation().size() > 0)
	{
		for(int index=0; index < adaptationSet->GetRepresentation().size() ; index++ )
		{
			IRepresentation* representation = adaptationSet->GetRepresentation().at(index);
			if( representation->GetContentProtection().size() > 0 )
			{
				return( representation->GetContentProtection() );
			}
		}
	}
	return (adaptationSet->GetContentProtection());
}

bool AampMPDParseHelper::IsPeriodEncrypted(int iPeriodIndex)
{
	bool retVal = false;
	if(iPeriodIndex >= mNumberOfPeriods || iPeriodIndex < 0)
	{
		AAMPLOG_WARN("Invalid PeriodIndex given %d",iPeriodIndex);
		return false;
	}
	
	// check in the queue if already stored for data 
	if(mPeriodEncryptionMap.find(iPeriodIndex) != mPeriodEncryptionMap.end())
	{
		retVal =  mPeriodEncryptionMap[iPeriodIndex];
	}
	else
	{
		vector<IPeriod *> periods = mMPDInstance->GetPeriods();
		IPeriod *period	=	periods.at(iPeriodIndex);
		
		if(period != NULL)
		{
			size_t numAdaptationSets = period->GetAdaptationSets().size();
			for(unsigned iAdaptationSet = 0; iAdaptationSet < numAdaptationSets; iAdaptationSet++)
			{
				const IAdaptationSet *adaptationSet = period->GetAdaptationSets().at(iAdaptationSet);
				if(adaptationSet != NULL)
				{				
					if(0 != GetContentProtection(adaptationSet).size())
					{
						mPeriodEncryptionMap[iPeriodIndex] = true;
						retVal = true;
						break;
					}				
				}
			}
		}
	}
	return retVal;
}


/**
 * @brief Check if Period is empty or not
 * @retval Return true on empty Period
 */
bool AampMPDParseHelper::IsEmptyPeriod(int iPeriodIndex, bool checkIframe) 
{
	bool isEmptyPeriod = true;		
	if(iPeriodIndex >= mNumberOfPeriods || iPeriodIndex < 0)
	{
		AAMPLOG_WARN("Invalid PeriodIndex given %d",iPeriodIndex);
		return isEmptyPeriod;
	}

	// check in the queue if already stored for data 
	std::pair<int,bool> key = std::make_pair(iPeriodIndex, checkIframe);
	if(mPeriodEmptyMap.find(key) != mPeriodEmptyMap.end())
	{
		isEmptyPeriod =  mPeriodEmptyMap[key];
		//AAMPLOG_WARN("From Cache Period %d value:%d",iPeriodIndex,isEmptyPeriod);
	}
	else
	{
		vector<IPeriod *> periods = mMPDInstance->GetPeriods();
		IPeriod *period	=	periods.at(iPeriodIndex);		
		if(period != NULL)
		{
			const std::vector<IAdaptationSet *> adaptationSets = period->GetAdaptationSets();
			size_t numAdaptationSets = period->GetAdaptationSets().size();
			for (int iAdaptationSet = 0; iAdaptationSet < numAdaptationSets; iAdaptationSet++)
			{
				IAdaptationSet *adaptationSet = period->GetAdaptationSets().at(iAdaptationSet);

				//if (rate != AAMP_NORMAL_PLAY_RATE)

				if(checkIframe)
				{
					if (IsIframeTrack(adaptationSet))
					{
						isEmptyPeriod = false;						
						break;
					}
				}
				else
				{
					isEmptyPeriod = IsEmptyAdaptation(adaptationSet);
					if(!isEmptyPeriod)
					{
						// Not to loop thru all Adaptations if one found.
						break;
					}
				}
			}
			mPeriodEmptyMap.insert({key, isEmptyPeriod});
		}

	}

	return isEmptyPeriod;
}

/**
 * @brief Check if Period is empty or not
 * @retval Return true on empty Period
 */
bool AampMPDParseHelper::IsEmptyAdaptation(IAdaptationSet *adaptationSet)
{
	bool isEmptyAdaptation = true;
	bool isFogPeriod	=	mIsFogMPD;
	IRepresentation *representation = NULL;
	ISegmentTemplate *segmentTemplate = adaptationSet->GetSegmentTemplate();
	if (segmentTemplate)
	{
		if(!isFogPeriod || (0 != segmentTemplate->GetDuration()))
		{
			isEmptyAdaptation = false;
		}
	}
	else
	{
		if(adaptationSet->GetRepresentation().size() > 0)
		{
			//Get first representation in the adaptation set
			representation = adaptationSet->GetRepresentation().at(0);
		}
		if(representation)
		{
			segmentTemplate = representation->GetSegmentTemplate();
			if(segmentTemplate)
			{
				if(!isFogPeriod || (0 != segmentTemplate->GetDuration()))
				{
					isEmptyAdaptation = false;
				}
			}
			else
			{
				ISegmentList *segmentList = representation->GetSegmentList();
				if(segmentList)
				{
					isEmptyAdaptation = false;
				}
				else
				{
					ISegmentBase *segmentBase = representation->GetSegmentBase();
					if(segmentBase)
					{
						isEmptyAdaptation = false;
					}
				}
			}
		}
	}
	return isEmptyAdaptation;
}

/**
 * @brief Check if adaptation set is iframe track
 * @param adaptationSet Pointer to adaptationSet
 * @retval true if iframe track
 */
bool AampMPDParseHelper::IsIframeTrack(IAdaptationSet *adaptationSet)
{
	const std::vector<INode *> subnodes = adaptationSet->GetAdditionalSubNodes();
	for (unsigned i = 0; i < subnodes.size(); i++)
	{
		INode *xml = subnodes[i];
		if(xml != NULL)
		{
			if (xml->GetName() == "EssentialProperty")
			{
				if (xml->HasAttribute("schemeIdUri"))
				{
					const std::string& schemeUri = xml->GetAttributeValue("schemeIdUri");
					if (schemeUri == "http://dashif.org/guidelines/trickmode")
					{
						return true;
					}
					else
					{
						AAMPLOG_WARN("skipping schemeUri %s", schemeUri.c_str());
					}
				}
			}
			else
			{
				AAMPLOG_TRACE("skipping name %s", xml->GetName().c_str());
			}
		}
		else
		{
			AAMPLOG_WARN("xml is null");  //CID:81118 - Null Returns
		}
	}
	return false;
}


/**
 * @brief Check if adaptation set is of a given media type
 * @param adaptationSet adaptation set
 * @param mediaType media type
 * @retval true if adaptation set is of the given media type
 */

bool AampMPDParseHelper::IsContentType(const IAdaptationSet *adaptationSet, AampMediaType mediaType )
{
	const char *name = GetMediaTypeName(mediaType);
	if (strcmp(name, "UNKNOWN") != 0)
	{
		if (adaptationSet->GetContentType() == name)
		{
			return true;
		}
		else if (adaptationSet->GetContentType() == "muxed")
		{
			AAMPLOG_WARN("excluding muxed content");
		}
		else
		{
			PeriodElement periodElement(adaptationSet, NULL);
			if (IsCompatibleMimeType(periodElement.GetMimeType(), mediaType) )
			{
				return true;
			}
			const std::vector<IRepresentation *> &representation = adaptationSet->GetRepresentation();
			for (int i = 0; i < representation.size(); i++)
			{
				const IRepresentation * rep = representation.at(i);
				PeriodElement periodElement(adaptationSet, rep);
				if (IsCompatibleMimeType(periodElement.GetMimeType(), mediaType) )
				{
					return true;
				}
			}

			const std::vector<IContentComponent *>contentComponent = adaptationSet->GetContentComponent();
			for( int i = 0; i < contentComponent.size(); i++)
			{
				if (contentComponent.at(i)->GetContentType() == name)
				{
					return true;
				}
			}
		}
	}
	else
	{
		AAMPLOG_WARN("name is null");  //CID:86093 - Null Returns
	}
	return false;
}


/**
 * @brief Get start time of current period
 * @retval current period's start time
 */
double AampMPDParseHelper::GetPeriodStartTime(int periodIndex,uint64_t mLastPlaylistDownloadTimeMs)
{
	auto it = std::find_if(mMPDPeriodDetails.begin(), mMPDPeriodDetails.end(),
			[periodIndex](const PeriodInfo& period) {
			return period.periodIndex == periodIndex;
			});

	if (it != mMPDPeriodDetails.end()) {
		// Found a matching PeriodInfo object, return its startTime.
		return it->periodStartTime;
	}
	else
	{
		double periodStart = 0;
		double  periodStartMs = 0;
		if( periodIndex<0 )
		{
			AAMPLOG_WARN( "periodIndex<0" );
		}
		else if(mMPDInstance != NULL )
		{
			if(periodIndex < mNumberOfPeriods)
			{
				string startTimeStr = mMPDInstance->GetPeriods().at(periodIndex)->GetStart();
				if(!startTimeStr.empty())
				{
					double deltaInStartTime = aamp_GetPeriodStartTimeDeltaRelativeToPTSOffset(mMPDInstance->GetPeriods().at(periodIndex)) * 1000;
					periodStartMs = ParseISO8601Duration(startTimeStr.c_str()) + deltaInStartTime;
					periodStart = (periodStartMs / 1000) + mAvailabilityStartTime;
					if(mNumberOfPeriods == 1 && periodIndex == 0 && mIsLiveManifest && !mIsFogMPD && (periodStart == mAvailabilityStartTime) && deltaInStartTime == 0)
					{
						// Temp hack to avoid running below if condition code for segment timeline , Due to this periodStart is getting changed for Cloud TSB or Hot Cloud DVR with segment timeline, which is not required.
						bool bHasSegmentTimeline = aamp_HasSegmentTimeline(mMPDInstance->GetPeriods().at(periodIndex));
						if( false == bHasSegmentTimeline ) // only for segment template
						{
							// segmentTemplate without timeline having period start "PT0S".
							if(!mLiveTimeFragmentSync)
							{
								mLiveTimeFragmentSync = true;
							}
							
							double duration = (aamp_GetPeriodDuration(periodIndex, mLastPlaylistDownloadTimeMs) / 1000);
							double liveTime = (double)mLastPlaylistDownloadTimeMs / 1000.0;
							if(mHasServerUtcTime)
							{
								liveTime+=mDeltaTime;
							}
							if(mAvailabilityStartTime < (liveTime - duration))
							{
								periodStart =  liveTime - duration;
							}
						}
					}

					AAMPLOG_INFO("StreamAbstractionAAMP_MPD: - MPD periodIndex %d AvailStartTime %f periodStart %f %s", periodIndex, mAvailabilityStartTime, periodStart,startTimeStr.c_str());
				}
				// As per spec:If the @start attribute is absent, but the previous Period element contains a @duration attribute .The start time of the new Period PeriodStart is the sum of the start time of the previous Period PeriodStart and the value of the attribute @duration of the previous Period
				else if (periodIndex > 0 && !mMPDInstance->GetPeriods().at(periodIndex-1)->GetDuration().empty())
				{
					string durationStr = mMPDInstance->GetPeriods().at(periodIndex -1)->GetDuration();
					double previousPeriodStart = GetPeriodStartTime(periodIndex - 1,mLastPlaylistDownloadTimeMs); 
					double durationTotal = ParseISO8601Duration(durationStr.c_str());
					periodStart = previousPeriodStart + (durationTotal / 1000);
				}
				else
				{
					double durationTotal = 0;
					for(int idx=0;idx < periodIndex; idx++)
					{
						//Calculates the total duration by summing the durations of individual periods up to 'periodIndex
						durationTotal += aamp_GetPeriodDuration(idx, mLastPlaylistDownloadTimeMs);
					}
					periodStart =  ((double)durationTotal / (double)1000);
					if(mIsLiveManifest && (periodStart > 0))
					{
						periodStart += mAvailabilityStartTime;
					}

					AAMPLOG_INFO("StreamAbstractionAAMP_MPD: - MPD periodIndex %d periodStart %f", periodIndex, periodStart);
				}
			}
		}
		else
		{
			AAMPLOG_WARN("mpd is null");  //CID:83436 Null Returns
		}


		return periodStart;
	}
}

/*
 * @brief Get end time of current period
 * @retval current period's end time
 */
double AampMPDParseHelper::GetPeriodEndTime(int periodIndex, uint64_t mLastPlaylistDownloadTimeMs, bool checkIFrame, bool IsUninterruptedTSB)
{
	auto it = std::find_if(mMPDPeriodDetails.begin(), mMPDPeriodDetails.end(),
			[periodIndex](const PeriodInfo& period) {
			return period.periodIndex == periodIndex;
			});

	if (it != mMPDPeriodDetails.end()) {
		// Found a matching PeriodInfo object, return its ENdTime.
		return it->periodEndTime;
	}
	else
	{
		double periodStartMs = 0;
		double periodDurationMs = 0;
		double periodEndTime = 0;
		IPeriod *period = NULL;
		if(mMPDInstance != NULL)
		{
			if(periodIndex < mNumberOfPeriods)
			{
				period = mMPDInstance->GetPeriods().at(periodIndex);
			}
		}
		else
		{
			AAMPLOG_WARN("mpd is null");  //CID:80459 , 80529- Null returns
		}
		if(period != NULL)
		{
			//For any Period in the MPD except for the last one, the PeriodEnd is obtained as the value of the PeriodStart of the next Period
			if(periodIndex != mNumberOfPeriods-1)
			{
				periodEndTime = GetPeriodStartTime(periodIndex+1,mLastPlaylistDownloadTimeMs);
				if(periodEndTime !=0 )
				{
					return periodEndTime;
				}
			}
			
			string startTimeStr = period->GetStart();
                        periodDurationMs = GetPeriodDuration(periodIndex,mLastPlaylistDownloadTimeMs,checkIFrame,IsUninterruptedTSB);
			if((mMPDInstance->GetAvailabilityStarttime().empty()) && !(mMPDInstance->GetType() == "static"))
			{
				AAMPLOG_WARN("availabilityStartTime required to calculate period duration not present in MPD");
			}
			else if(0 == periodDurationMs)
			{
				AAMPLOG_WARN("Could not get valid period duration to calculate end time");
			}
			else
			{
				if(startTimeStr.empty() || mLiveTimeFragmentSync)
				{
					AAMPLOG_INFO("Period startTime is not present in MPD, so calculating start time with previous period durations");
					if(mIsLiveManifest)
                                        {
						periodStartMs = GetPeriodStartTime(periodIndex,mLastPlaylistDownloadTimeMs) * 1000 - (mAvailabilityStartTime * 1000);
					}
					else
					{
						 periodStartMs = GetPeriodStartTime(periodIndex,mLastPlaylistDownloadTimeMs) * 1000;
					}
				}
				else
				{
					periodStartMs = ParseISO8601Duration(startTimeStr.c_str()) + (aamp_GetPeriodStartTimeDeltaRelativeToPTSOffset(period)* 1000);
                                }
				periodEndTime = ((double)(periodStartMs + periodDurationMs) /1000);
				if(mIsLiveManifest)
				{
					periodEndTime +=  mAvailabilityStartTime;
				}
			}
			AAMPLOG_INFO("StreamAbstractionAAMP_MPD: MPD periodIndex:%d periodEndTime %f", periodIndex, periodEndTime);
		}
		else
		{
			AAMPLOG_WARN("period is null");  //CID:85519- Null returns
		}
		return periodEndTime;
	}
}
/**
 *   @brief  Get difference between first segment start time and presentation offset from period
 *   @retval start time delta in seconds
 */
double AampMPDParseHelper::aamp_GetPeriodStartTimeDeltaRelativeToPTSOffset(IPeriod * period)
{
	double duration = 0;

	const std::vector<IAdaptationSet *> adaptationSets = period->GetAdaptationSets();
	const ISegmentTemplate *representation = NULL;
	const ISegmentTemplate *adaptationSet = NULL;
	if( adaptationSets.size() > 0 )
	{
		IAdaptationSet * firstAdaptation = NULL;
		for (auto &adaptationSet : period->GetAdaptationSets())
		{
			//Check for video adaptation
			if (!IsContentType(adaptationSet, eMEDIATYPE_VIDEO))
			{
				continue;
			}
			firstAdaptation = adaptationSet;
		}

		if(firstAdaptation != NULL)
		{
			adaptationSet = firstAdaptation->GetSegmentTemplate();
			const std::vector<IRepresentation *> representations = firstAdaptation->GetRepresentation();
			if (representations.size() > 0)
			{
				representation = representations.at(0)->GetSegmentTemplate();
			}
		}

		SegmentTemplates segmentTemplates(representation,adaptationSet);

		if (segmentTemplates.HasSegmentTemplate())
		{
			const ISegmentTimeline *segmentTimeline = segmentTemplates.GetSegmentTimeline();
			if (segmentTimeline)
			{
				uint32_t timeScale = segmentTemplates.GetTimescale();
				uint64_t presentationTimeOffset = segmentTemplates.GetPresentationTimeOffset();
				//AAMPLOG_TRACE("tscale: %" PRIu32 " offset : %" PRIu64 "", timeScale, presentationTimeOffset);
				std::vector<ITimeline *>&timelines = segmentTimeline->GetTimelines();
				if(timelines.size() > 0)
				{
					ITimeline *timeline = timelines.at(0);
					uint64_t deltaBwFirstSegmentAndOffset = 0;
					if(timeline != NULL)
					{
						uint64_t timelineStart = timeline->GetStartTime();
						if(timelineStart > presentationTimeOffset)
						{
							deltaBwFirstSegmentAndOffset = timelineStart - presentationTimeOffset;
						}
						duration = (double) deltaBwFirstSegmentAndOffset / timeScale;
						//AAMPLOG_TRACE("timeline start : %" PRIu64 " offset delta : %lf", timelineStart,duration);
					}
					AAMPLOG_TRACE("offset delta : %lf",  duration);
				}
			}
		}
	}
	return duration;
}

/**
 * @brief  A helper function to  check if period has segment timeline for video track
 * @param period period of segment
 * @return True if period has segment timeline for video otherwise false
 */
bool AampMPDParseHelper::aamp_HasSegmentTimeline(IPeriod * period)
{
    bool bRetValue = false;

    const std::vector<IAdaptationSet *> adaptationSets = period->GetAdaptationSets();
    const ISegmentTemplate *representation = NULL;
    const ISegmentTemplate *adaptationSet = NULL;
    if( adaptationSets.size() > 0 )
    {
        IAdaptationSet * firstAdaptation = NULL;
        for (auto &adaptationSet : period->GetAdaptationSets())
        {
            //Check for video adaptation
            if (!IsContentType(adaptationSet, eMEDIATYPE_VIDEO))
            {
                continue;
            }
            firstAdaptation = adaptationSet;
        }

        if(firstAdaptation != NULL)
        {
            adaptationSet = firstAdaptation->GetSegmentTemplate();
            const std::vector<IRepresentation *> representations = firstAdaptation->GetRepresentation();
            if (representations.size() > 0)
            {
                representation = representations.at(0)->GetSegmentTemplate();
            }
        }

        SegmentTemplates segmentTemplates(representation,adaptationSet);

        if (segmentTemplates.HasSegmentTemplate())
        {
            const ISegmentTimeline *segmentTimeline = segmentTemplates.GetSegmentTimeline();
            if (segmentTimeline)
            {
                bRetValue = true;
            }
        }
    }
    return bRetValue;
}

/**
 * @brief Get duration of current period
 * @retval current period's duration
 */
double AampMPDParseHelper::GetPeriodDuration(int periodIndex,uint64_t mLastPlaylistDownloadTimeMs, bool checkIFrame, bool IsUninterruptedTSB)
{
	auto it = std::find_if(mMPDPeriodDetails.begin(), mMPDPeriodDetails.end(),
                        [periodIndex](const PeriodInfo& period) {
                        return period.periodIndex == periodIndex;
                        });

        if (it != mMPDPeriodDetails.end()) {
                // Found a matching PeriodInfo object, return its Duration
                return it->duration;
        }
	else
	{
		double periodDuration = 0;
		double  periodDurationMs = 0;
		bool liveTimeFragmentSync = false;
		if(mMPDInstance != NULL)
		{
			if(periodIndex < mNumberOfPeriods)
			{
				const std::string & durationStr = mMPDInstance->GetPeriods().at(periodIndex)->GetDuration();
				//Check for single period.
				if (1 == mNumberOfPeriods && !mIsLiveManifest)
				{
					//Priority for MediaPresentationDuration if it is Single period VOD asset
					if(mMediaPresentationDuration != 0 )
					{
						periodDurationMs = mMediaPresentationDuration;
						AAMPLOG_WARN("period duration based on mMediaPresentationDuration =%f",periodDurationMs );
						return mMediaPresentationDuration;
					}
					//Next priority for duration tag
					else if(!durationStr.empty() )
					{
						periodDurationMs = ParseISO8601Duration(durationStr.c_str());
						AAMPLOG_WARN("period duration based on duration field =%f",periodDurationMs );
						return periodDurationMs;
					}
				}
				// If it's not the last period or it is but both the duration and the mediaPresentationDuration are empty,
				// calculate the duration from other manifest properties:
				// 1. As the difference between the start of this period and the next
				// 2. From the adaptation sets of the manifest, if the period is not empty

				string startTimeStr = mMPDInstance->GetPeriods().at(periodIndex)->GetStart();
				if(!durationStr.empty())
				{
					periodDurationMs = ParseISO8601Duration(durationStr.c_str());
					double liveTime = (double)mLastPlaylistDownloadTimeMs / 1000.0;
					//To find liveTimeFragmentSync
					if(!startTimeStr.empty())
					{
						double deltaInStartTime = aamp_GetPeriodStartTimeDeltaRelativeToPTSOffset(mMPDInstance->GetPeriods().at(periodIndex)) * 1000;
						double periodStartMs = ParseISO8601Duration(startTimeStr.c_str()) + deltaInStartTime;
						double	periodStart = (periodStartMs / 1000) + mAvailabilityStartTime;
						if(mNumberOfPeriods == 1 && periodIndex == 0 && mIsLiveManifest && !mIsFogMPD && (periodStart == mAvailabilityStartTime) && deltaInStartTime == 0)
						{
							// segmentTemplate without timeline having period start "PT0S".
							if(!liveTimeFragmentSync)
							{
								liveTimeFragmentSync = true;
							}
						}
					}
					if(!mIsFogMPD && mIsLiveManifest && liveTimeFragmentSync && (mAvailabilityStartTime > (liveTime - (periodDurationMs/1000))))
					{
						periodDurationMs = (liveTime - mAvailabilityStartTime) * 1000;
					}
					periodDuration = periodDurationMs / 1000.0;
					AAMPLOG_INFO("StreamAbstractionAAMP_MPD: MPD periodIndex:%d periodDuration %f", periodIndex, periodDuration);
				}
				else
				{
					if(mNumberOfPeriods == 1 && periodIndex == 0)
					{
						if(!mMPDInstance->GetMediaPresentationDuration().empty())
						{
							periodDurationMs = mMediaPresentationDuration;
						}
						else
						{
							periodDurationMs = aamp_GetPeriodDuration(periodIndex, mLastPlaylistDownloadTimeMs);
						}
						periodDuration = (periodDurationMs / 1000.0);
						AAMPLOG_INFO("StreamAbstractionAAMP_MPD: [MediaPresentation] - MPD periodIndex:%d periodDuration %f", periodIndex, periodDuration);
					}
					else
					{
						string curStartStr = mMPDInstance->GetPeriods().at(periodIndex)->GetStart();
						string nextStartStr = "";
						if(periodIndex+1 < mNumberOfPeriods)
						{
							nextStartStr = mMPDInstance->GetPeriods().at(periodIndex+1)->GetStart();
						}
						if(!curStartStr.empty() && (!nextStartStr.empty()) && !IsUninterruptedTSB)
						{
							double  curPeriodStartMs = 0;
							double  nextPeriodStartMs = 0;
							curPeriodStartMs = ParseISO8601Duration(curStartStr.c_str()) + (aamp_GetPeriodStartTimeDeltaRelativeToPTSOffset(mMPDInstance->GetPeriods().at(periodIndex)) * 1000);
							nextPeriodStartMs = ParseISO8601Duration(nextStartStr.c_str()) + (aamp_GetPeriodStartTimeDeltaRelativeToPTSOffset(mMPDInstance->GetPeriods().at(periodIndex+1)) * 1000);
							periodDurationMs = nextPeriodStartMs - curPeriodStartMs;
							periodDuration = (periodDurationMs / 1000.0);
							if(periodDuration != 0.0f)
								AAMPLOG_INFO("StreamAbstractionAAMP_MPD: [StartTime based] - MPD periodIndex:%d periodDuration %f", periodIndex, periodDuration);
						}
						else
						{
							if(IsEmptyPeriod(periodIndex, checkIFrame))
							{
								// Final empty period, return duration as 0 incase if GetPeriodDuration is called for this.
								periodDurationMs = 0;
								periodDuration = 0;
							}
							else
							{
								periodDurationMs = aamp_GetPeriodDuration(periodIndex, mLastPlaylistDownloadTimeMs);
								periodDuration = (periodDurationMs / 1000.0);
							}
							AAMPLOG_INFO("StreamAbstractionAAMP_MPD: [Segments based] - MPD periodIndex:%d periodDuration %f", periodIndex, periodDuration);
						}
					}
				}
			}
		}
		else
		{
			AAMPLOG_WARN("mpd is null");  //CID:83436 Null Returns
		}
		return periodDurationMs;
	}
}

/**
 *   @brief  Get Period Duration
 *   @retval period duration in milliseconds
 */
double AampMPDParseHelper::aamp_GetPeriodDuration(int periodIndex, uint64_t mpdDownloadTime)
{
	double durationMs = 0;
	vector<IPeriod *> periods = mMPDInstance->GetPeriods();
	IPeriod *period	=	periods.at(periodIndex);
	
	std::string tempString = period->GetDuration();
	if(!tempString.empty())
	{
		durationMs = ParseISO8601Duration( tempString.c_str());
	}
	//Calculate duration from @mediaPresentationDuration for a single period VOD stream having empty @duration.This is added as a fix for voot stream seekposition timestamp issue.
	if(0 == durationMs && !mIsLiveManifest && mNumberOfPeriods == 1)
	{
		if(!mMPDInstance->GetMediaPresentationDuration().empty())
		{
			durationMs = mMediaPresentationDuration;
		}
		else
		{
			AAMPLOG_WARN("mediaPresentationDuration missing in period %s", period->GetId().c_str());
		}

	}
	if(0 == durationMs)
	{
		const std::vector<IAdaptationSet *> adaptationSets = period->GetAdaptationSets();
		const ISegmentTemplate *representation = NULL;
		const ISegmentTemplate *adaptationSet = NULL;
		if (adaptationSets.size() > 0)
		{
			IAdaptationSet * firstAdaptation = NULL;
			for (auto &adaptationSet : period->GetAdaptationSets())
			{
				//Check for video adaptation
				if (!IsContentType(adaptationSet, eMEDIATYPE_VIDEO))
				{
					continue;
				}
				firstAdaptation = adaptationSet;
			}
			if(firstAdaptation != NULL)
			{
				adaptationSet = firstAdaptation->GetSegmentTemplate();
				const std::vector<IRepresentation *> representations = firstAdaptation->GetRepresentation();
				if (representations.size() > 0)
				{
					representation = representations.at(0)->GetSegmentTemplate();
				}

				SegmentTemplates segmentTemplates(representation,adaptationSet);

				if( segmentTemplates.HasSegmentTemplate() )
				{
					const ISegmentTimeline *segmentTimeline = segmentTemplates.GetSegmentTimeline();
					uint32_t timeScale = segmentTemplates.GetTimescale();
					uint64_t presentationTimeOffset = segmentTemplates.GetPresentationTimeOffset();
					//Calculate period duration by adding up the segment durations in timeline
					if (segmentTimeline)
					{
						std::vector<ITimeline *>&timelines = segmentTimeline->GetTimelines();
						if(!timelines.empty())
						{
							int timeLineIndex = 0;
							uint64_t timelineStartTime = timelines.at(timeLineIndex)->GetStartTime();
							while (timeLineIndex < timelines.size())
							{
								ITimeline *timeline = timelines.at(timeLineIndex);
								uint32_t repeatCount = timeline->GetRepeatCount();
								double timelineDurationMs = ComputeFragmentDuration(timeline->GetDuration(),timeScale) * 1000;
								durationMs += ((repeatCount + 1) * timelineDurationMs);
								AAMPLOG_TRACE("timeLineIndex[%d] size [%zu] updated durationMs[%lf]", timeLineIndex, timelines.size(), durationMs);
								timeLineIndex++;
							}
							if (presentationTimeOffset > timelineStartTime)
							{
								durationMs -= ((double)((presentationTimeOffset - timelineStartTime) * 1000) / (double) timeScale);
								AAMPLOG_TRACE("presentationTimeOffset:%" PRIu64 " timelineStartTime:%" PRIu64 " updated durationMs[%lf]", presentationTimeOffset, timelineStartTime, durationMs);
							}
						}
					}
					else
					{
						std::string periodStartStr = period->GetStart();
						if(!periodStartStr.empty())
						{
							//If it's last period find period duration using mpd download time
							//and minimumUpdatePeriod
							if(!mMPDInstance->GetMediaPresentationDuration().empty() && !mIsLiveManifest && periodIndex == (periods.size() - 1))
							{
								double periodStart = 0;
								double totalDuration = 0;
								periodStart = ParseISO8601Duration( periodStartStr.c_str() );
								totalDuration = mMediaPresentationDuration;
								durationMs = totalDuration - periodStart;
							}
							else if(periodIndex == (periods.size() - 1))
							{
								std::string availabilityStartStr = mMPDInstance->GetAvailabilityStarttime();
								std::string publishTimeStr;
								auto attributesMap = mMPDInstance->GetRawAttributes();
								if(attributesMap.find("publishTime") != attributesMap.end())
								{
									publishTimeStr = attributesMap["publishTime"];
								}

								if(!publishTimeStr.empty() && (publishTimeStr.compare(availabilityStartStr) != 0))
								{
									mpdDownloadTime = (uint64_t)ISO8601DateTimeToUTCSeconds(publishTimeStr.c_str()) * 1000;
								}

								if(0 == mpdDownloadTime)
								{
									AAMPLOG_WARN("mpdDownloadTime required to calculate period duration not provided");
								}
								else if(mMPDInstance->GetMinimumUpdatePeriod().empty())
								{
									AAMPLOG_WARN("minimumUpdatePeriod required to calculate period duration not present in MPD");
								}
								else if(mMPDInstance->GetAvailabilityStarttime().empty())
								{
									AAMPLOG_WARN("availabilityStartTime required to calculate period duration not present in MPD");
								}
								else
								{
									double periodStart = 0;
									periodStart = ParseISO8601Duration( periodStartStr.c_str() );
									double periodEndTime = mpdDownloadTime + mMinUpdateDurationMs;
									double periodStartTime = (mAvailabilityStartTime * 1000) + periodStart;
									std::string tsbDepth = mMPDInstance->GetTimeShiftBufferDepth();
									AAMPLOG_INFO("periodStart=%lf availabilityStartTime=%lf minUpdatePeriod=%" PRIu64 " mpdDownloadTime=%" PRIu64 " tsbDepth:%s",
												 periodStart, mAvailabilityStartTime, mMinUpdateDurationMs, mpdDownloadTime, tsbDepth.c_str());
									if(periodStartTime == (mAvailabilityStartTime * 1000))
									{
										// period starting from availability start time
										if(!tsbDepth.empty())
										{
											durationMs = ParseISO8601Duration(tsbDepth.c_str());
										}
										//If MPD@timeShiftBufferDepth is not present, the period duration is should be based on the MPD@availabilityStartTime; and should not result in a value of 0. 
										else
										{
											durationMs = mpdDownloadTime - (mAvailabilityStartTime * 1000);
										}
										if((mpdDownloadTime - durationMs) < mAvailabilityStartTime && !tsbDepth.empty())
										{
											durationMs = mpdDownloadTime - (mAvailabilityStartTime * 1000);
										}
									}
									else
									{
										durationMs = periodEndTime - periodStartTime;
									}

									if(durationMs <= 0)
									{
										AAMPLOG_WARN("Invalid period duration periodStartTime %lf periodEndTime %lf durationMs %lf", periodStartTime, periodEndTime, durationMs);
										durationMs = 0;
									}
								}
							}
							//We can calculate period duration by subtracting startime from next period start time.
							else
							{
								std::string nextPeriodStartStr = periods.at(periodIndex + 1)->GetStart();
								if(!nextPeriodStartStr.empty())
								{
									double periodStart = 0;
									double nextPeriodStart = 0;
									periodStart = ParseISO8601Duration( periodStartStr.c_str() );
									nextPeriodStart = ParseISO8601Duration( nextPeriodStartStr.c_str() );
									durationMs = nextPeriodStart - periodStart;
									if(durationMs <= 0)
									{
										AAMPLOG_WARN("Invalid period duration periodStartTime %lf nextPeriodStart %lf durationMs %lf", periodStart, nextPeriodStart, durationMs);
										durationMs = 0;
									}
								}
								else
								{
									AAMPLOG_WARN("Next period startTime missing periodIndex %d", periodIndex);
								}
							}
						}
						else
						{
							AAMPLOG_WARN("Start time and duration missing in period %s", period->GetId().c_str());
						}
					}
				}
				else
				{
					const std::vector<IRepresentation *> representations = firstAdaptation->GetRepresentation();
					if (representations.size() > 0)
					{
						ISegmentList *segmentList = representations.at(0)->GetSegmentList();
						if (segmentList)
						{
							const std::vector<ISegmentURL*> segmentURLs = segmentList->GetSegmentURLs();
							if(!segmentURLs.empty())
							{
								durationMs += ComputeFragmentDuration( segmentList->GetDuration(), segmentList->GetTimescale()) * 1000;
							}
							else
							{
								AAMPLOG_WARN("segmentURLs  is null");  //CID:82729 - Null Returns
							}
						}
						else
						{
							AAMPLOG_ERR("not-yet-supported mpd format");
						}
					}
				}
			}
			else
			{
				AAMPLOG_WARN("firstAdaptation is null");  //CID:84261 - Null Returns
			}
		}
	}
	return durationMs;
}

/**
 * @fn getPeriodIdx
 * @brief Function to get base period index from mpd
 * @param[in] periodId.
 * @retval period index.
 */
int AampMPDParseHelper::getPeriodIdx(const std::string &periodId)
{
	vector<IPeriod *> periods = mMPDInstance->GetPeriods();
	int periodIter = mNumberOfPeriods-1;
	int PeriodIdx = -1;
	while(periodIter >= 0)
	{
		if(periodId == periods.at(periodIter)->GetId())
		{
			PeriodIdx = periodIter;
			break;
		}
		periodIter--;
	}
	return PeriodIdx;
}


std::vector<Representation *>  AampMPDParseHelper::GetBitrateInfoFromCustomMpd( const IAdaptationSet *adaptationSet)
{
	vector<Representation *> representations;
	std::vector<xml::INode *> subNodes = adaptationSet->GetAdditionalSubNodes();
	for(int i = 0; i < subNodes.size(); i ++)
	{
		xml::INode * node = subNodes.at(i);
		if(node != NULL)
		{
			if(node->GetName() == "AvailableBitrates")
			{
				std::vector<xml::INode *> reprNodes = node->GetNodes();
				for(int reprIter = 0; reprIter < reprNodes.size(); reprIter++)
				{
					xml::INode * reprNode = reprNodes.at(reprIter);
					if(reprNode != NULL)
					{
						if(reprNode->GetName() == "Representation")
						{
							dash::mpd::Representation * repr = new dash::mpd::Representation();
							if(reprNode->HasAttribute("bandwidth"))
							{
								repr->SetBandwidth( (uint32_t)stol(reprNode->GetAttributeValue("bandwidth")));
							}
							if(reprNode->HasAttribute("height"))
							{
								repr->SetHeight( (uint32_t)stol(reprNode->GetAttributeValue("height")));
							}
							if(reprNode->HasAttribute("width"))
							{
								repr->SetWidth( (uint32_t)stol(reprNode->GetAttributeValue("width")));
							}
							representations.push_back(repr);
						}
					}
					else
					{
						AAMPLOG_WARN("reprNode  is null");  //CID:85171 - Null Returns
					}
				}
				break;
			}
		}
		else
		{
			AAMPLOG_WARN("node is null");  //CID:81731 - Null Returns
		}

	}
	 return representations;
}

/**
 *   @brief  GetFirstSegment start time from period
 *   @param  period
 *   @param  type media type
 *   @retval start time
 */
double AampMPDParseHelper::GetFirstSegmentScaledStartTime(IPeriod * period, AampMediaType type)
{
	double scaledStartTime = -1;
	const std::vector<IAdaptationSet *> adaptationSets = period->GetAdaptationSets();

	const ISegmentTemplate *representation = NULL;
	const ISegmentTemplate *adaptationSet = NULL;
	if( adaptationSets.size() > 0 )
	{
		IAdaptationSet * firstAdaptation = adaptationSets.at(0);
		if (type != eMEDIATYPE_DEFAULT)
		{
			for (auto adaptation : adaptationSets)
			{
				if (IsContentType(adaptation, type))
				{
					firstAdaptation = adaptation;
					break;
				}
			}
		}
		if(firstAdaptation != NULL)
		{
			adaptationSet = firstAdaptation->GetSegmentTemplate();
			const std::vector<IRepresentation *> representations = firstAdaptation->GetRepresentation();
			if (representations.size() > 0)
			{
				representation = representations.at(0)->GetSegmentTemplate();
			}
		}
	}
	SegmentTemplates segmentTemplates(representation,adaptationSet);

	if (segmentTemplates.HasSegmentTemplate())
	{
		uint64_t startTime = 0;
		const ISegmentTimeline *segmentTimeline = segmentTemplates.GetSegmentTimeline();
		if (segmentTimeline)
		{
			std::vector<ITimeline *>&timelines = segmentTimeline->GetTimelines();
			if(timelines.size() > 0)
			{
				startTime = timelines.at(0)->GetStartTime();
			}
			uint64_t presentationTimeOffset = segmentTemplates.GetPresentationTimeOffset();
			if(presentationTimeOffset > startTime)
			{
				AAMPLOG_WARN("StreamAbstractionAAMP_MPD: Presentation Time Offset %" PRIu64 " ahead of segment start %" PRIu64 ", Set PTO as start time", presentationTimeOffset, startTime);
				startTime = presentationTimeOffset;
			}
			scaledStartTime = ((double) startTime / (double)segmentTemplates.GetTimescale());
		}
		else
		{
			startTime = segmentTemplates.GetPresentationTimeOffset();
			if (startTime > 0)
			{
				scaledStartTime = ((double) startTime / (double)segmentTemplates.GetTimescale());
			}
		}
	}
	return scaledStartTime;
}


/**
 * @brief Get duration though representation iteration
 * @retval duration in milliseconds
 */
uint64_t AampMPDParseHelper::GetDurationFromRepresentation()
{
	uint64_t durationMs = 0;
	if(mMPDInstance == NULL) {
		AAMPLOG_WARN("mpd is null");  //CID:82158 - Null Returns
		return durationMs;
	}
	size_t numPeriods = mMPDInstance->GetPeriods().size();

	for (unsigned iPeriod = 0; iPeriod < numPeriods; iPeriod++)
	{
		IPeriod *period = NULL;
		if(mMPDInstance != NULL)
		{
			period = mMPDInstance->GetPeriods().at(iPeriod);
		}
		else
		{
			AAMPLOG_WARN("mpd is null");  //CID:82158 - Null Returns
		}
		
		if(period != NULL)
		{
			const std::vector<IAdaptationSet *> adaptationSets = period->GetAdaptationSets();
			if (adaptationSets.size() > 0)
			{
				IAdaptationSet * firstAdaptation = adaptationSets.at(0);
				for (auto &adaptationSet : period->GetAdaptationSets())
				{
					//Check for first video adaptation
					if (IsContentType(adaptationSet, eMEDIATYPE_VIDEO))
					{
						firstAdaptation = adaptationSet;
						break;
					}
				}
				ISegmentTemplate *AdapSegmentTemplate = NULL;
				ISegmentTemplate *RepSegmentTemplate = NULL;
				if (firstAdaptation == NULL)
				{
					AAMPLOG_WARN("firstAdaptation is null");  //CID:82158 - Null Returns
					return durationMs;
				}
				AdapSegmentTemplate = firstAdaptation->GetSegmentTemplate();
				const std::vector<IRepresentation *> representations = firstAdaptation->GetRepresentation();
				if (representations.size() > 0)
				{
					RepSegmentTemplate  = representations.at(0)->GetSegmentTemplate();
				}
				SegmentTemplates segmentTemplates(RepSegmentTemplate,AdapSegmentTemplate);
				if (segmentTemplates.HasSegmentTemplate())
				{
					std::string media = segmentTemplates.Getmedia();
					if(!media.empty())
					{
						const ISegmentTimeline *segmentTimeline = segmentTemplates.GetSegmentTimeline();
						if (segmentTimeline)
						{
							std::vector<ITimeline *>&timelines = segmentTimeline->GetTimelines();
							uint32_t timeScale = segmentTemplates.GetTimescale();
							int timeLineIndex = 0;
							while (timeLineIndex < timelines.size())
							{
								ITimeline *timeline = timelines.at(timeLineIndex);
								uint32_t repeatCount = timeline->GetRepeatCount();
								uint64_t timelineDurationMs = ComputeFragmentDuration(timeline->GetDuration(),timeScale) * 1000;
								durationMs += ((repeatCount + 1) * timelineDurationMs);
								AAMPLOG_TRACE("period[%d] timeLineIndex[%d] size [%zu] updated durationMs[%" PRIu64 "]", iPeriod, timeLineIndex, timelines.size(), durationMs);
								timeLineIndex++;
							}
						}
					}
					else
					{
						AAMPLOG_WARN("media is null");  //CID:83185 - Null Returns
					}
				}
				else
				{
					const std::vector<IRepresentation *> representations = firstAdaptation->GetRepresentation();
					if (representations.size() > 0)
					{
						ISegmentList *segmentList = representations.at(0)->GetSegmentList();
						if (segmentList)
						{
							const std::vector<ISegmentURL*> segmentURLs = segmentList->GetSegmentURLs();
							if(segmentURLs.empty())
							{
								AAMPLOG_WARN("segmentURLs is null");  //CID:82113 - Null Returns
							}
							durationMs += ComputeFragmentDuration(segmentList->GetDuration(), segmentList->GetTimescale()) * 1000;
						}
						else
						{
							AAMPLOG_ERR("not-yet-supported mpd format");
						}
					}
				}
			}
		}
		else
		{
			AAMPLOG_WARN("period is null");  //CID:81482 - Null Returns
		}
	}
	return durationMs;
}


/**
 * @brief Calculates the duration of new content in a period.
 *
 * This function takes an IPeriod object and calculates the duration of new content
 * within that period. It considers various factors such as the duration of the period,
 * the type of MPD (Media Presentation Description), and the segment templates.
 *
 * @param[in] period The IPeriod object representing the period.
 * @param[out] curEndNumber A reference to a uint64_t variable that will store the current end number.
 * @return The duration of new content in milliseconds.
 */
double AampMPDParseHelper::GetPeriodNewContentDurationMs(IPeriod * period, uint64_t &curEndNumber)
{
	double durationMs = 0;
	bool found = false;
	std::string tempString = period->GetDuration();
	if(!tempString.empty())
	{
		durationMs = ParseISO8601Duration( tempString.c_str());
		found = true;
		AAMPLOG_INFO("periodDuration %f", durationMs);
	}
	size_t numPeriods = mMPDInstance->GetPeriods().size();
	if(0 == durationMs && mMPDInstance->GetType() == "static" && numPeriods == 1)
	{
		std::string durationStr =  mMPDInstance->GetMediaPresentationDuration();
		if(!durationStr.empty())
		{
			durationMs = ParseISO8601Duration( durationStr.c_str());
			found = true;
			AAMPLOG_INFO("mediaPresentationDuration based periodDuration %f", durationMs);
		}
		else
		{
			AAMPLOG_INFO("mediaPresentationDuration missing in period %s", period->GetId().c_str());
		}
	}
	const std::vector<IAdaptationSet *> adaptationSets = period->GetAdaptationSets();
	const ISegmentTemplate *representation = NULL;
	const ISegmentTemplate *adaptationSet = NULL;
	if( adaptationSets.size() > 0 )
	{
		IAdaptationSet * firstAdaptation = adaptationSets.at(0);
		for (auto &adaptationSet : period->GetAdaptationSets())
		{
			//Check for first video adaptation
			if (IsContentType(adaptationSet, eMEDIATYPE_VIDEO))
			{
				firstAdaptation = adaptationSet;
				break;
			}
		}
		if(firstAdaptation != NULL)
		{
			adaptationSet = firstAdaptation->GetSegmentTemplate();
			const std::vector<IRepresentation *> representations = firstAdaptation->GetRepresentation();
			if (representations.size() > 0)
			{
				representation = representations.at(0)->GetSegmentTemplate();
			}
		}

		SegmentTemplates segmentTemplates(representation,adaptationSet);

		if (segmentTemplates.HasSegmentTemplate())
		{
			const ISegmentTimeline *segmentTimeline = segmentTemplates.GetSegmentTimeline();
			if (segmentTimeline)
			{
				uint32_t timeScale = segmentTemplates.GetTimescale();
				std::vector<ITimeline *>&timelines = segmentTimeline->GetTimelines();
				uint64_t startNumber = segmentTemplates.GetStartNumber();
				int timeLineIndex = 0;
				while (timeLineIndex < timelines.size())
				{
					ITimeline *timeline = timelines.at(timeLineIndex);
					uint32_t segmentCount = timeline->GetRepeatCount() + 1;
					double timelineDurationMs = ComputeFragmentDuration(timeline->GetDuration(),timeScale) * 1000;
					if(found)
					{
						curEndNumber = startNumber + segmentCount - 1;
						startNumber = curEndNumber++;
					}
					else
					{
						for(int i=0;i<segmentCount;i++)
						{
							if(startNumber > curEndNumber)
							{
								durationMs += timelineDurationMs;
								curEndNumber = startNumber;
							}
							startNumber++;
						}
					}
					timeLineIndex++;
				}
			}
		}
	}
	return durationMs;
}


/**
 * @brief Retrieves the time scale of the period segment.
 *
 * This function takes an IPeriod object as input and returns the time scale of the period segment.
 * The time scale is obtained by checking the segment templates of the first video adaptation set in the period.
 * If a segment template is found, the time scale is retrieved from it.
 *
 * @param period[in] The IPeriod object representing the period.
 * @return The time scale of the period segment. If no segment template is found, the time scale will be 0.
 */
uint32_t AampMPDParseHelper::GetPeriodSegmentTimeScale(IPeriod * period)
{
	uint32_t timeScale = 0;
	const std::vector<IAdaptationSet *> adaptationSets = period->GetAdaptationSets();

	const ISegmentTemplate *representation = NULL;
	const ISegmentTemplate *adaptationSet = NULL;
	if( adaptationSets.size() > 0 )
	{
		IAdaptationSet * firstAdaptation = adaptationSets.at(0);
		for (auto &adaptationSet : period->GetAdaptationSets())
		{
			//Check for first video adaptation
			if (IsContentType(adaptationSet, eMEDIATYPE_VIDEO))
			{
				firstAdaptation = adaptationSet;
				break;
			}
		}
		if(firstAdaptation != NULL)
		{
			adaptationSet = firstAdaptation->GetSegmentTemplate();
			const std::vector<IRepresentation *> representations = firstAdaptation->GetRepresentation();
			if (representations.size() > 0)
			{
				representation = representations.at(0)->GetSegmentTemplate();
			}
		}
	}
	SegmentTemplates segmentTemplates(representation,adaptationSet);

	if( segmentTemplates.HasSegmentTemplate() )
	{
		timeScale = segmentTemplates.GetTimescale();
	}
	return timeScale;
}

/**
 * @brief Retrieves the start time of the first segment in the given period.
 *
 * @param[in] period The period for which to retrieve the start time.
 * @return The start time of the first segment in the period.
 */
uint64_t AampMPDParseHelper::GetFirstSegmentStartTime(IPeriod * period)
{
	uint64_t startTime = 0;
	const std::vector<IAdaptationSet *> adaptationSets = period->GetAdaptationSets();

	const ISegmentTemplate *representation = NULL;
	const ISegmentTemplate *adaptationSet = NULL;
	if( adaptationSets.size() > 0 )
	{
		IAdaptationSet * firstAdaptation = adaptationSets.at(0);
		for (auto &adaptationSet : period->GetAdaptationSets())
		{
			//Check for first video adaptation
			if (IsContentType(adaptationSet, eMEDIATYPE_VIDEO))
			{
				firstAdaptation = adaptationSet;
				break;
			}
		}
		if(firstAdaptation != NULL)
		{
			adaptationSet = firstAdaptation->GetSegmentTemplate();
			const std::vector<IRepresentation *> representations = firstAdaptation->GetRepresentation();
			if (representations.size() > 0)
			{
				representation = representations.at(0)->GetSegmentTemplate();
			}
		}
	}
	SegmentTemplates segmentTemplates(representation,adaptationSet);
	
	if( segmentTemplates.HasSegmentTemplate() )
	{
		const ISegmentTimeline *segmentTimeline = segmentTemplates.GetSegmentTimeline();
		if (segmentTimeline)
		{
			std::vector<ITimeline *>&timelines = segmentTimeline->GetTimelines();
			if(timelines.size() > 0)
			{
				startTime = timelines.at(0)->GetStartTime();
			}
			uint64_t presentationTimeOffset = segmentTemplates.GetPresentationTimeOffset();
			if(presentationTimeOffset > startTime)
			{
				AAMPLOG_WARN("StreamAbstractionAAMP_MPD: Presentation Time Offset %" PRIu64 " ahead of segment start %" PRIu64 ", Set PTO as start time", presentationTimeOffset, startTime);
				startTime = presentationTimeOffset;
			}
		}
	}
	return startTime;
}


/**
 * @brief  Get start time and duration from the current timeline
 * @param[in]   period for current period
 * @param[in]   representationIdx being used in current period
 * @param[in]   adaptationSetIdx being used in current period
 * @param[out]  scaledStartTime (seconds) of selected timeline returned
 * @param[out]  duration (seconds) of selected timeline returned
 * @return void
 */
void AampMPDParseHelper::GetStartAndDurationFromTimeline(IPeriod * period, int representationIdx, int adaptationSetIdx, AampTime &scaledStartTime, AampTime &duration)
{

	duration = 0.0;
	const std::vector<IAdaptationSet *> adaptationSets = period->GetAdaptationSets();

	const ISegmentTemplate *representation = NULL;
	const ISegmentTemplate *adaptationSet = NULL;
	if (adaptationSetIdx < adaptationSets.size() && adaptationSetIdx >=0 )
	{
		IAdaptationSet *firstAdaptation = adaptationSets.at(adaptationSetIdx);

		adaptationSet = firstAdaptation->GetSegmentTemplate();
		const std::vector<IRepresentation *> representations = firstAdaptation->GetRepresentation();
		if (representationIdx < representations.size() && representationIdx >= 0)
		{
			representation = representations.at(representationIdx)->GetSegmentTemplate();
		}
	}
	SegmentTemplates segmentTemplates(representation, adaptationSet);

	if (segmentTemplates.HasSegmentTemplate())
	{
		const ISegmentTimeline *segmentTimeline = segmentTemplates.GetSegmentTimeline();
		uint32_t timeScale = segmentTemplates.GetTimescale();
		uint64_t startTime = 0;
		// Calculate period duration by adding up the segment durations in timeline
		if (segmentTimeline)
		{
			std::vector<ITimeline *> &timelines = segmentTimeline->GetTimelines();
			int timeLineIndex = 0;

			while (timeLineIndex < timelines.size())
			{
				ITimeline *timeline = timelines.at(timeLineIndex);
				uint32_t repeatCount = timeline->GetRepeatCount();
				double timelineDuration = ComputeFragmentDuration(timeline->GetDuration(), timeScale);
				duration += ((repeatCount + 1) * timelineDuration);
				AAMPLOG_TRACE("timeLineIndex[%d] size [%zu] updated duration[%lf]", timeLineIndex, timelines.size(), duration.inSeconds());
				timeLineIndex++;
			}

			if(timelines.size() > 0)
			{
				startTime = timelines.at(0)->GetStartTime();
				AAMPLOG_TRACE("startTime %" PRIu64 " timescale %u", startTime,timeScale);
			}
			uint64_t presentationTimeOffset = segmentTemplates.GetPresentationTimeOffset();
			if(presentationTimeOffset > startTime)
			{
				AAMPLOG_WARN("Presentation Time Offset %" PRIu64 " ahead of segment start %" PRIu64 ", Set PTO as start time", presentationTimeOffset, startTime);
				startTime = presentationTimeOffset;
				duration -= (double)(presentationTimeOffset - startTime) / (double)(timeScale);
			}
			scaledStartTime = ((double) startTime / (double)timeScale);
		}
		else
		{
			startTime = segmentTemplates.GetPresentationTimeOffset();
			if (startTime > 0)
			{
				scaledStartTime = ((double) startTime / (double)segmentTemplates.GetTimescale());
			}
			AAMPLOG_WARN("No timeline in this manifest");
		}
	}

}
