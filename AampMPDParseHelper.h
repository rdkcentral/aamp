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
* @file AampMPDParseHelper.h
* @brief Helper Class for MPD Parsing
**************************************/

#ifndef __AAMP_MPD_PARSE_HELPER_H__
#define __AAMP_MPD_PARSE_HELPER_H__

#include <stdint.h>
#include <unordered_map>
#include <vector>
#include <iterator>
#include <algorithm>
#include <map>
#include <string>
#include <mutex>
#include <queue>
#include <sys/time.h>
#include <iostream>
#include <string>
#include <curl/curl.h>
#include <chrono>
#include <condition_variable> // std::condition_variable, std::cv_status
#include <memory>
#include <string>
#include <stdint.h>
#include "libdash/IMPD.h"
#include "libdash/INode.h"
#include "libdash/IDASHManager.h"
#include "libdash/IProducerReferenceTime.h"
#include "libdash/xml/Node.h"
#include "libdash/helpers/Time.h"
#include "libdash/xml/DOMParser.h"
#include <libxml/xmlreader.h>
#include "AampDefine.h"

using namespace dash;
using namespace dash::mpd;
using namespace dash::xml;
using namespace dash::helpers;

using namespace std;

typedef std::map<int, bool> PeriodEncryptedMap;
typedef std::map<std::pair<int,bool>, bool> PeriodEmptyMap;

/**
 * @class PeriodElement
 * @brief Consists Adaptation Set and representation-specific parts
 */
class PeriodElement
{ //  Common (Adaptation Set) and representation-specific parts
private:
	const IRepresentation *pRepresentation; // primary (representation)
	const IAdaptationSet *pAdaptationSet; // secondary (adaptation set)
	
public:
	PeriodElement(const PeriodElement &other) = delete;
	PeriodElement& operator=(const PeriodElement& other) = delete;
	
	PeriodElement(const IAdaptationSet *adaptationSet, const IRepresentation *representation ):
	pAdaptationSet(NULL),pRepresentation(NULL)
	{
		pRepresentation = representation;
		pAdaptationSet = adaptationSet;
	}
	~PeriodElement()
	{
	}
	
	std::string GetMimeType()
	{
		std::string mimeType;
		if( pAdaptationSet ) mimeType = pAdaptationSet->GetMimeType();
		if( mimeType.empty() && pRepresentation ) mimeType = pRepresentation->GetMimeType();
		return mimeType;
	}
};//PerioidElement

/**
 * @class SegmentTemplates
 * @brief Handles operation and information on segment template from manifest
 */
class SegmentTemplates
{ //  SegmentTemplate can be split info common (Adaptation Set) and representation-specific parts
private:
	const ISegmentTemplate *segmentTemplate1; // primary (representation)
	const ISegmentTemplate *segmentTemplate2; // secondary (adaptation set)
	
public:
	SegmentTemplates(const SegmentTemplates &other) = delete;
	SegmentTemplates& operator=(const SegmentTemplates& other) = delete;

	SegmentTemplates( const ISegmentTemplate *representation, const ISegmentTemplate *adaptationSet ) : segmentTemplate1(0),segmentTemplate2(0)
	{
		segmentTemplate1 = representation;
		segmentTemplate2 = adaptationSet;
	}
	~SegmentTemplates()
	{
	}

	bool HasSegmentTemplate()
	{
		return segmentTemplate1 || segmentTemplate2;
	}
	
	std::string Getmedia()
	{
		std::string media;
		if( segmentTemplate1 ) media = segmentTemplate1->Getmedia();
		if( media.empty() && segmentTemplate2 ) media = segmentTemplate2->Getmedia();
		return media;
	}
	
	const ISegmentTimeline *GetSegmentTimeline()
	{
		const ISegmentTimeline *segmentTimeline = NULL;
		if( segmentTemplate1 ) segmentTimeline = segmentTemplate1->GetSegmentTimeline();
		if( !segmentTimeline && segmentTemplate2 ) segmentTimeline = segmentTemplate2->GetSegmentTimeline();
		return segmentTimeline;
	}
	
	uint32_t GetTimescale()
	{
		uint32_t timeScale = 0;
		if( segmentTemplate1 ) timeScale = segmentTemplate1->GetTimescale();
		// if timescale missing in template ,GetTimeScale returns 1
		if((timeScale==1 || timeScale==0) && segmentTemplate2 ) timeScale = segmentTemplate2->GetTimescale();
		return timeScale;
	}

	uint32_t GetDuration()
	{
		uint32_t duration = 0;
		if( segmentTemplate1 ) duration = segmentTemplate1->GetDuration();
		if( duration==0 && segmentTemplate2 ) duration = segmentTemplate2->GetDuration();
		return duration;
	}
	
	long GetStartNumber()
	{
		long startNumber = 0;
		if( segmentTemplate1 ) startNumber = segmentTemplate1->GetStartNumber();
		if( startNumber==0 && segmentTemplate2 ) startNumber = segmentTemplate2->GetStartNumber();
		return startNumber;
	}

	uint64_t GetPresentationTimeOffset()
	{
		uint64_t presentationOffset = 0;
		if(segmentTemplate1 ) presentationOffset = segmentTemplate1->GetPresentationTimeOffset();
		if( presentationOffset==0 && segmentTemplate2) presentationOffset = segmentTemplate2->GetPresentationTimeOffset();
		return presentationOffset;
	}
	
	std::string Getinitialization()
	{
		std::string initialization;
		if( segmentTemplate1 ) initialization = segmentTemplate1->Getinitialization();
		if( initialization.empty() && segmentTemplate2 ) initialization = segmentTemplate2->Getinitialization();
		return initialization;
	}
}; // SegmentTemplates


/**
 * @class AampMPDParseHelper
 * @brief Handles manifest parsing and providing helper functions for fragment collector
 */
class AampMPDParseHelper
{
public :
	/**
	*   @fn AampMPDParseHelper
	*   @brief Default Constructor
	*/
	AampMPDParseHelper();
	/**
	*   @fn ~AampMPDParseHelper
	*   @brief  Destructor
	*/
	~AampMPDParseHelper();

	/**
	 *  @ AampMPDParseHelper
	 *  @brief Copy Constructor
	 */
	AampMPDParseHelper(const AampMPDParseHelper& cachedMPD);

	/**
	*   @fn Initialize
	*   @brief  Initialize the parser with MPD instance 
	* 	@param[in] instance - MPD instance to parse
 	* 	@retval None
	*/
	void Initialize(dash::mpd::IMPD *instance);
	/**
	*   @fn Clear
	*   @brief  Clear the parsed values in the helper 
 	* 	@retval None
	*/
	void Clear();
	/**
	*   @fn IsLiveManifest
	*   @brief  Returns if Manifest is Live Stream or not 
 	* 	@retval bool . True if Live , False if VOD
	*/
	bool IsLiveManifest() { return mIsLiveManifest;}
	/**
	*   @fn GetMinUpdateDurationMs
	*   @brief  Returns MinUpdateDuration from the manifest  
 	* 	@retval uint64_t Minimum Update Duration
	*/
	uint64_t GetMinUpdateDurationMs() { return mMinUpdateDurationMs;}
	/**
	*   @fn GetAvailabilityStartTime
	*   @brief  Returns AvailabilityStartTime from the manifest  
 	* 	@retval double . AvailabilityStartTime
	*/	
	double GetAvailabilityStartTime() { return mAvailabilityStartTime;}
	/**
	*   @fn GetSegmentDurationSeconds
	*   @brief  Returns SegmentDuration from the manifest  
 	* 	@retval uint64_t . SegmentDuration
	*/
	uint64_t GetSegmentDurationSeconds() { return mSegmentDurationSeconds;}
	/**
	*   @fn GetTSBDepth
	*   @brief  Returns TSBDepth from the manifest  
 	* 	@retval double . TSB Depth
	*/
	double GetTSBDepth() { return mTSBDepth;}
	/**
	*   @fn GetPresentationOffsetDelay
	*   @brief  Returns PresentationOffsetDelay from the manifest  
 	* 	@retval double . OffsetDelay
	*/
	double GetPresentationOffsetDelay() { return mPresentationOffsetDelay;}
	/**
	*   @fn GetMinUpdaGetMediaPresentationDurationteDurationMs
	*   @brief  Returns mediaPresentationDuration from the manifest  
 	* 	@retval uint64_t . duration
	*/
	uint64_t GetMediaPresentationDuration()  {  return mMediaPresentationDuration;}
	/**
	*   @fn GetNumberOfPeriods
	*   @brief  Returns Number of Periods from the manifest  
 	* 	@retval int  
	*/
	int GetNumberOfPeriods() { return mNumberOfPeriods;}
	/**
	*   @fn IsFogMPD
	*   @brief  Returns Check if the manifest is from Fog  
 	* 	@retval bool . True if Fog , False if not Fog MPD
	*/
	bool IsFogMPD() { return mIsFogMPD;}
	/**
	 * @fn IsPeriodEncrypted
	 * @param[in] period - current period
	 * @brief check if current period is encrypted
	 * @retval true on success
	 */
	bool IsPeriodEncrypted(int iPeriodIndex);
	/**
	 * @fn GetContentProtection
	 * @param[In] adaptation set and media type
	 */	
	std::vector<IDescriptor*> GetContentProtection(const IAdaptationSet *adaptationSet);
	/**
	 * @fn IsEmptyPeriod
	 * @param period period to check whether it is empty
	 * @param checkIframe check only for Iframe Adaptation	 
	 * @retval Return true on empty Period
	 */
	bool IsEmptyPeriod(int iPeriodIndex, bool checkIframe);
	/**
	 * @fn IsEmptyAdaptation
	 * @param Adaptation Adaptationto check whether it is empty	 
	 */
	bool IsEmptyAdaptation(IAdaptationSet *adaptationSet);
	/**
	* @brief Check if adaptation set is iframe track
	* @param adaptationSet Pointer to adaptainSet
	* @retval true if iframe track
	*/
	bool IsIframeTrack(IAdaptationSet *adaptationSet);

private:

	/**
	*	@fn parseMPD
	*	@brief Function to parse the manifest downloaded
	*/
	void parseMPD();
private:
	/* Flag to indicate Live Manifest */
	bool mIsLiveManifest;
	/* Flag to indicate if Fog Manifest or not */
	bool mIsFogMPD;
	/* storage for Minimum Update Duration in mSec*/
	uint64_t mMinUpdateDurationMs;
	/* storage for Availability Start Time */
	double mAvailabilityStartTime;
	/* storage for Segment Duration in seconds */
	uint64_t mSegmentDurationSeconds;
	/* storage of TSB Depth */
	double mTSBDepth;
	/* storage for Presentation Offset Delay */
	double mPresentationOffsetDelay;
	/* storage for media Presentation duration */
	uint64_t mMediaPresentationDuration;
	/* lib dash mpd instance */
	dash::mpd::IMPD* mMPDInstance;
	/* Mutex to protect between to public API access*/
	std::mutex mMyObjectMutex;
	/* Storage for Period count in manifest */
	int mNumberOfPeriods;
	/* Container to store Period Encryption details for MPD*/
	PeriodEncryptedMap	mPeriodEncryptionMap;
	/* Containter to store Period Empty map for MPD */
	PeriodEmptyMap		mPeriodEmptyMap;
};


#endif
