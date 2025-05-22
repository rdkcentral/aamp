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
#include <inttypes.h>
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
#include "AampUtils.h"
#include "AampMPDUtils.h"
#include "AampTime.h"

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
};//PeriodElementElement

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
	 *  @ AampMPDParseHelper
	 *  @brief Copy assignment operator
	 */
	AampMPDParseHelper& operator=(const AampMPDParseHelper&) = delete;
	
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
	*   @fn GetMediaPresentationDuration
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
	 * @param Adaptation Adaptation to check whether it is empty	 
	 */
	bool IsEmptyAdaptation(IAdaptationSet *adaptationSet);
	/**
	* @brief Check if adaptation set is iframe track
	* @param adaptationSet Pointer to adaptationSet
	* @retval true if iframe track
	*/
	bool IsIframeTrack(IAdaptationSet *adaptationSet);

	/**
	 *   @brief  Get Period Duration
	 *   @retval period duration in milliseconds
	 */
	double aamp_GetPeriodDuration(int periodIndex, uint64_t mpdDownloadTime);
	
	/**
 	  * @brief Check if adaptation set is of a given media type
	  * @retval true if adaptation set is of the given media type
	  */
	bool IsContentType(const IAdaptationSet *adaptationSet, AampMediaType mediaType );
	
	/**
	 * @fn GetPeriodDuration
	 * @param mpd : pointer manifest
	 * @param periodIndex Index of the current period
	 */
	double GetPeriodDuration(int periodIndex, uint64_t mLastPlaylistDownloadTimeMs, bool checkIFrame, bool IsUninterruptedTSB);

	/**
	 * @fn aamp_GetPeriodStartTimeDeltaRelativeToPTSOffset
	 * @param period period of segment
	 */
	double aamp_GetPeriodStartTimeDeltaRelativeToPTSOffset(IPeriod * period);

	/**
	 * @brief Get start time of current period
	 * @retval current period's start time
	 */
	double GetPeriodStartTime(int periodIndex,uint64_t mLastPlaylistDownloadTimeMs);

	/**
	 * @brief Get LiveTime Fragment Sync status.
	 * @return LiveTime Fragment Sync status.
	 */
	bool GetLiveTimeFragmentSync() {return mLiveTimeFragmentSync;}

	/**
	 * @brief SetHasServerUtcTime 
	 * @param True - if UTCTiming element is available in the manifest else false
	 */
	void SetHasServerUtcTime(bool hasServerUtcTime) { mHasServerUtcTime = hasServerUtcTime; }

	/**
	 * @fn SetLocalTimeDelta in Seconds
	 * @params deltaTime - time difference between localUtctime and currentTime
	 */
	void SetLocalTimeDelta(double deltaTime) { mDeltaTime = deltaTime;}

	/**
	 * @brief Get end time of current period
	 * @retval current period's end time
	 */
	double GetPeriodEndTime(int periodIndex,  uint64_t mLastPlaylistDownloadTimeMs, bool checkIFrame, bool IsUninterruptedTSB);

	/**
	 * @fn UpdateBoundaryPeriod - to  Calculate Upper and lower boundary of playable periods
	 * @params - Is trickplay mode
	 */
	void UpdateBoundaryPeriod(bool IsTrickMode);

	/**
	 * @fn getPeriodIdx
	 * @brief Function to get base period index from mpd
	 * @param[in] periodId.
	 * @retval period index.
	 */
	int getPeriodIdx(const std::string &periodId);

	/**
	 * @brief Extract bitrate info from custom mpd
	 * @note Caller function should delete the vector elements after use.
	 * @param adaptationSet : Adaptation from which bitrate info is to be extracted
	 * @param[out] representations : Representation vector gets updated with Available bit rates.
	 */
	vector<Representation *> GetBitrateInfoFromCustomMpd( const IAdaptationSet *adaptationSet);

	int mUpperBoundaryPeriod;	// Last playable period index
	int mLowerBoundaryPeriod;	// First playable period index

	/**
	 * @brief Set the MPD period details.
	 * This function allows you to set the MPD period details using the provided vector of PeriodInfo objects.
	 * @param currMPDDetails A vector containing the period details to be set.
	 */
	void SetMPDPeriodDetails(const std::vector<PeriodInfo> currMPDDetails){mMPDPeriodDetails =  currMPDDetails;}

	/**
	 * @brief  GetFirstSegment start time from period
	 * @param  period
	 * @param  type media type
	 * @retval start time
	 */
	double GetFirstSegmentScaledStartTime(IPeriod * period, AampMediaType type);

	/**
	 * @brief Get duration though representation iteration
	 * @retval duration in milliseconds
	 */
	uint64_t GetDurationFromRepresentation();

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
	double GetPeriodNewContentDurationMs(IPeriod * period, uint64_t &curEndNumber);

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
	uint32_t GetPeriodSegmentTimeScale(IPeriod * period);

	/**
	 * @brief Retrieves the start time of the first segment in the given period.
	 *
	 * @param[in] period The period for which to retrieve the start time.
	 * @return The start time of the first segment in the period.
	 */
	uint64_t GetFirstSegmentStartTime(IPeriod * period);

	/**
	 * @brief  Get start time and duration from the current timeline
	 * @param[in]   period for current period
	 * @param[in]   representationIdx being used in current period
	 * @param[in]   adaptationSetIdx being used in current period
	 * @param[out]  scaledStartTime (seconds) of selected timeline returned
	 * @param[out]  duration (seconds) of selected timeline returned
	 */
	void GetStartAndDurationFromTimeline(IPeriod * period, int representationIdx, int adaptationSetIdx, AampTime &scaledStartTime, AampTime &duration);

    /**
     * @brief  A helper function to  check if period has segment timeline for video track
     * @param period period of segment
     * @return True if period has segment timeline for video otherwise false
     */
    bool aamp_HasSegmentTimeline(IPeriod * period);

	/**
	 * @brief Get the MPD instance.
	 *
	 * @return const dash::mpd::IMPD* A pointer to the MPD instance.
	 */
	const dash::mpd::IMPD* getMPD() const { return mMPDInstance; }
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
	/* Container to store Period Empty map for MPD */
	PeriodEmptyMap		mPeriodEmptyMap;
	
	bool mLiveTimeFragmentSync;
	/*To check whether UTCTiming element is available in the manifest */
	bool mHasServerUtcTime;
	/* storage for time difference between LocalUtcTime and  currentTime */
	double mDeltaTime;
	/* Vector to store the current MPD period details */
	std::vector<PeriodInfo> mMPDPeriodDetails;
};

typedef std::shared_ptr<AampMPDParseHelper> AampMPDParseHelperPtr;

#endif
