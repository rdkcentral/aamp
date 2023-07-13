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
					   mPeriodEmptyMap(cachedMPD.mPeriodEmptyMap) , mMPDInstance(NULL)
{
	//AAMPLOG_INFO("%s\n",__FUNCTION__);
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
* @brief Get content protection from represetation/adaptation field
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
			//Get first representation in the adapatation set
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
 * @param adaptationSet Pointer to adaptainSet
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

bool AampMPDParseHelper::IsContentType(const IAdaptationSet *adaptationSet, MediaType mediaType )
{
	//FN_TRACE_F_MPD( __FUNCTION__ );
	const char *name = getMediaTypeName(mediaType);
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
 * @brief Get duration of current period
 * @retval current period's duration
 */
double AampMPDParseHelper::GetPeriodDuration(int periodIndex,uint64_t mLastPlaylistDownloadTimeMs, bool checkIFrame, bool IsUninterruptedTSB)
{
	double periodDuration = 0;
	double  periodDurationMs = 0;
	bool liveTimeFragmentSync = false;
	if(mMPDInstance != NULL)
	{
		const bool is_last_period = periodIndex == mNumberOfPeriods-1;

		if(periodIndex < mNumberOfPeriods)
		{
			const std::string & durationStr = mMPDInstance->GetPeriods().at(periodIndex)->GetDuration();

			// If it's the last period either use (in this strict order!) the media presentation duration or the period's duration.
			if (is_last_period)
			{
				if(mMediaPresentationDuration != 0)
				{
					periodDurationMs = mMediaPresentationDuration;
					return periodDurationMs;
				}
				if(!durationStr.empty())
				{
					periodDurationMs = ParseISO8601Duration(durationStr.c_str());
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

/**
 *   @brief  Get Period Duration
 *   @retval period duration in milli seconds
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
	//DELIA-45784 Calculate duration from @mediaPresentationDuration for a single period VOD stream having empty @duration.This is added as a fix for voot stream seekposition timestamp issue.
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
			IAdaptationSet * firstAdaptation;
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
					//Calculate period duration by adding up the segment durations in timeline
					if (segmentTimeline)
					{
						std::vector<ITimeline *>&timelines = segmentTimeline->GetTimelines();
						int timeLineIndex = 0;
						while (timeLineIndex < timelines.size())
						{
							ITimeline *timeline = timelines.at(timeLineIndex);
							uint32_t repeatCount = timeline->GetRepeatCount();
							double timelineDurationMs = ComputeFragmentDuration(timeline->GetDuration(),timeScale) * 1000;
							durationMs += ((repeatCount + 1) * timelineDurationMs);
							AAMPLOG_TRACE("timeLineIndex[%d] size [%lu] updated durationMs[%lf]", timeLineIndex, timelines.size(), durationMs);
							timeLineIndex++;
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
									double periodStartTime = mAvailabilityStartTime + periodStart;
									std::string tsbDepth = mMPDInstance->GetTimeShiftBufferDepth();
									AAMPLOG_INFO("periodStart %lf availabilityStartTime %lf minUpdatePeriod %lu mpdDownloadTime %" PRIu64 " tsbDepth:%s", periodStart, mAvailabilityStartTime, mMinUpdateDurationMs, mpdDownloadTime, tsbDepth.c_str());
									if(periodStartTime == mAvailabilityStartTime)
									{

										// period starting from availability start time
										if(!tsbDepth.empty())
										{
											durationMs = ParseISO8601Duration(tsbDepth.c_str());
										}
										//If MPD@timeShiftBufferDepth is not present, the period duration is should be based on the MPD@availabilityStartTime; and should not result in a value of 0. 
										else
										{
											durationMs = mAvailabilityStartTime;
										}
										if((mpdDownloadTime - durationMs) < mAvailabilityStartTime && !tsbDepth.empty())
										{
											durationMs = mpdDownloadTime - mAvailabilityStartTime;
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
