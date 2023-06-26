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
