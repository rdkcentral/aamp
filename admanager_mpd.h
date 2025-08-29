/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2018 RDK Management
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
 * @file admanager_mpd.h
 * @brief Client side DAI manger for MPEG DASH
 */

#ifndef ADMANAGER_MPD_H_
#define ADMANAGER_MPD_H_

#include "AdManagerBase.h"
#include <string>
#include <condition_variable>
#include "libdash/INode.h"
#include "libdash/IDASHManager.h"
#include "libdash/xml/Node.h"
#include "libdash/IMPD.h"
#include "AampMPDParseHelper.h"

using namespace dash;
using namespace std;
using namespace dash::mpd;

/**
 * @class CDAIObjectMPD
 *
 * @brief Client Side DAI object implementation for DASH
 */
class CDAIObjectMPD: public CDAIObject
{
	class PrivateCDAIObjectMPD* mPrivObj;    /**< Private instance of Client Side DAI object for DASH */
public:

	/**
	 * @fn CDAIObjectMPD
	 *
	 * @param[in] aamp - Pointer to PrivateInstanceAAMP
	 */
	CDAIObjectMPD(PrivateInstanceAAMP* aamp);

	/**
	 * @fn ~CDAIObjectMPD
	 */
	virtual ~CDAIObjectMPD();

	/**
	* @brief CDAIObject Copy Constructor
	*/
	CDAIObjectMPD(const CDAIObjectMPD&) = delete;

	/**
	* @brief CDAIObject assignment operator overloading
	*/
	CDAIObjectMPD& operator= (const CDAIObjectMPD&) = delete;

	/**
	 * @brief Getter for the PrivateCDAIObjectMPD instance
	 *
	 * @return PrivateCDAIObjectMPD object pointer
	 */
	PrivateCDAIObjectMPD* GetPrivateCDAIObjectMPD()
	{
		return mPrivObj;
	}

	/**
	 * @fn SetAlternateContents 
	 *
	 * @param[in] periodId - Adbreak's unique identifier; the first period id
	 * @param[in] adId - Individual Ad's id
	 * @param[in] url - Ad URL
	 * @param[in] startMS - Ad start time in milliseconds
	 * @param[in] breakdur - Adbreak's duration in MS
	 */
	virtual void SetAlternateContents(const std::string &periodId, const std::string &adId, const std::string &url, uint64_t startMS=0, uint32_t breakdur=0) override;
};


/**
 * @brief Events to the Ad manager's state machine
 */
enum class AdEvent
{
	INIT,                       /**< Playback initialization */
	BASE_OFFSET_CHANGE,         /**< Base period's offset change */
	AD_FINISHED,                /**< Ad playback finished */
	AD_FAILED,                  /**< Ad playback failed */
	PERIOD_CHANGE,              /**< Period changed */
	DEFAULT = PERIOD_CHANGE     /**< Default event */
};

#define OFFSET_ALIGN_FACTOR 2000 /**< Observed minor slacks in the ad durations. Align factor used to place the ads correctly. */

/**
 * @struct AdNode
 * @brief Individual Ad's meta info
 */
struct AdNode {
	bool         invalid;          /**< Failed to playback failed once, no need to attempt later */
	bool         placed;           /**< Ad completely placed on the period */
	bool         resolved;         /**< Ad resolved status */
	std::string  adId;             /**< Ad's identifier */
	std::string  url;              /**< Ad's manifest URL */
	uint64_t     duration;         /**< Duration of the Ad in milliseconds*/
	std::string  basePeriodId;     /**< Id of the base period at the beginning of the Ad */
	int          basePeriodOffset; /**< Offset of the base period at the beginning of the Ad in milliseconds */
	MPD*         mpd;              /**< Pointer to the MPD object */

	/**
	* @brief AdNode default constructor
	*/
	AdNode() : invalid(false), placed(false), resolved(false), adId(), url(), duration(0), basePeriodId(), basePeriodOffset(0), mpd(nullptr)
	{

	}

	/**
	* @brief AdNode constructor
	*
	* @param[in] invalid - Is Ad valid
	* @param[in] placed - Is Ad completed placed over underlying period
	* @param[in] resolved - Is Ad resolved
	* @param[in] adId - Ad identifier
	* @param[in] url - Ad's manifest URL
	* @param[in] duration - Duration of the Ad
	* @param[in] basePeriodId - Base period id of the Ad
	* @param[in] basePeriodOffset - Base period offset of the Ad
	* @param[in] mpd - Pointer to the MPD object
	*/
	AdNode(bool invalid, bool placed, bool resolved, std::string adId, std::string url, uint64_t duration,
		std::string basePeriodId, int basePeriodOffset, MPD* mpd)
		: invalid(invalid), placed(placed), resolved(resolved), adId(adId), url(url), duration(duration), basePeriodId(basePeriodId),
		basePeriodOffset(basePeriodOffset), mpd(mpd)
	{

	}

	/**
	* @brief AdNode Copy Constructor
	*
	* @param[in] adNode - Reference to the source AdNode
	*/
	AdNode(const AdNode& adNode)
		: invalid(adNode.invalid), placed(adNode.placed), resolved(adNode.resolved), adId(adNode.adId),
		url(adNode.url), duration(adNode.duration), basePeriodId(adNode.basePeriodId),
		basePeriodOffset(adNode.basePeriodOffset), mpd(adNode.mpd)
	{
	}

	/**
	* @brief AdNode assignment operator overloading
	*/
	AdNode& operator=(const AdNode&) = delete;
};

typedef std::shared_ptr<std::vector<AdNode>> AdNodeVectorPtr;

/**
 * @struct AdBreakObject
 *
 * @brief AdBreak's metadata object
 */
struct AdBreakObject{
	uint32_t                             brkDuration;     /**< Adbreak's duration in milliseconds*/
	AdNodeVectorPtr                      ads;             /**< Ads in the Adbreak in sequential order */
	std::string                          endPeriodId;     /**< Base period's id after the adbreak playback */
	uint64_t                             endPeriodOffset; /**< Base period's offset after the adbreak playback in milliseconds*/
	uint32_t                             adsDuration;     /**< Ads' duration in the Adbreak in milliseconds*/
	bool                                 adjustEndPeriodOffset;     /**< endPeriodOffset needs be re-adjusted or not */
	bool                                 mAdBreakPlaced;  /**< flag marks if the adbreak is completely placed */
	bool                                 mAdFailed;       /** Current Ad playback failed flag */
	bool                                 mSplitPeriod;    /**< To identify whether the ad is split period ad or not */
	bool                                 invalid;         /**< flag marks if the adbreak is invalid or not */
	bool                                 resolved;       /**< flag marks if the adbreak is resolved or not */
	AampTime                             mAbsoluteAdBreakStartTime; /**< Period start time */
	/**
	* @brief AdBreakObject default constructor
	*/
	AdBreakObject()
		: brkDuration(0), ads(), endPeriodId(), endPeriodOffset(0), adsDuration(0), adjustEndPeriodOffset(false),
		mAdBreakPlaced(false), mAdFailed(false), mSplitPeriod(false), invalid(false), resolved(false), mAbsoluteAdBreakStartTime(0.0)
	{
	}

	/**
	* @brief AdBreakObject constructor
	*
	* @param[in] _duration - Adbreak's duration in milliseconds
	* @param[in] _ads - Ads in the Adbreak
	* @param[in] _endPeriodId - Base period's id after the adbreak playback
	* @param[in] _endPeriodOffset - Base period's offset after the adbreak playback in milliseconds
	* @param[in] _adsDuration - Ads' duration in the Adbreak in milliseconds
	*/
	AdBreakObject(uint32_t _duration, AdNodeVectorPtr _ads, std::string _endPeriodId,
		uint64_t _endPeriodOffset, uint32_t _adsDuration)
		: brkDuration(_duration), ads(_ads), endPeriodId(_endPeriodId), endPeriodOffset(_endPeriodOffset),
		adsDuration(_adsDuration), adjustEndPeriodOffset(false), mAdBreakPlaced(false), mAdFailed(false), mSplitPeriod(false), invalid(false), resolved(false), mAbsoluteAdBreakStartTime(0.0)
	{
	}
};

/**
 * @struct AdOnPeriod
 *
 * @brief Individual Ad's object placed over the period
 */
struct AdOnPeriod
{
	int32_t  adIdx;           /**< Ad's idx (of vector) */
	uint32_t adStartOffset;   /**< Ad's start offset in milliseconds*/

    /**
     * @brief AdOnPeriod constructor
     */
    AdOnPeriod() : adIdx(-1), adStartOffset(0)
    {
    }

    /**
     * @brief AdOnPeriod constructor
     *
     * @param[in] _adIdx - Ad's idx
     * @param[in] _adStartOffset - Ad's start offset
     */
    AdOnPeriod(int32_t _adIdx, uint32_t _adStartOffset)
        : adIdx(_adIdx), adStartOffset(_adStartOffset)
    {
    }
};

/**
 * @struct Period2AdData
 * @brief Meta info corresponding to each period.
 */
struct Period2AdData {
	bool                        filled;      /**< Period filled with ads or not */
	std::string                 adBreakId;   /**< Parent Adbreak */
	uint64_t                    duration;    /**< Period's Duration in milliseconds */
	std::map<int, AdOnPeriod>   offset2Ad;   /**< Mapping period's offset in milliseconds to individual Ads */

	/**
	* @brief Period2AdData constructor
	*/
	Period2AdData() : filled(false), adBreakId(), duration(0), offset2Ad()
	{
	}

	Period2AdData(bool _filled, std::string _adBreakId, uint64_t _duration, std::map<int, AdOnPeriod> _offset2Ad)
		: filled(_filled), adBreakId(_adBreakId), duration(_duration), offset2Ad(_offset2Ad)
	{
	}
};

/**
 * @struct AdFulfillObj
 *
 * @brief Temporary object representing currently fulfilling ad (given by setAlternateContent).
 */
struct AdFulfillObj {
	std::string periodId;      /**< Currently fulfilling adbreak id */
	std::string adId;          /**< Currently placing Ad id */
	std::string url;           /**< Current Ad's URL */

	/**
	* @brief AdFulfillObj constructor
	*/
	AdFulfillObj() : periodId(), adId(), url()
	{
	}

	/**
	* @brief AdFulfillObj constructor
	*/
	AdFulfillObj(std::string _periodId, std::string _adId, std::string _url)
		: periodId(_periodId), adId(_adId), url(_url)
	{
	}
};

/**
 * @struct PlacementObj
 *
 * @brief Currently placing Ad's object
 */
struct PlacementObj {
	std::string pendingAdbrkId;         /**< Only one Adbreak will be pending for replacement */
	std::string openPeriodId;           /**< The period in the adbreak that is progressing */
	uint64_t    curEndNumber;           /**< Current periods last fragment number */
	int         curAdIdx;               /**< Currently placing ad, during MPD progression */
	uint32_t    adNextOffset;           /**< Current Ad's offset to be placed in the next iteration of PlaceAds in milliseconds*/
	uint32_t    adStartOffset;          /**< Current Ad's start offset in milliseconds, this is the position from where ad is getting placed in current period*/
	bool        waitForNextPeriod;      /**< Flag denotes if we are waiting for the next period to be available in the current placement*/

	/**
	* @brief PlacementObj constructor
	*/
	PlacementObj() : pendingAdbrkId(), openPeriodId(), curEndNumber(0), curAdIdx(-1), adNextOffset(0), adStartOffset(0), waitForNextPeriod(false)
	{

	}

	/**
	* @brief PlacementObj parameterized constructor
	* @param pendingAdbrkId The pending adbreak ID
	* @param openPeriodId The open period ID
	* @param curEndNumber The current period's last fragment number
	* @param curAdIdx The index of the currently placing ad during MPD progression
	* @param adNextOffset The current ad's offset to be placed in the next iteration of PlaceAds in milliseconds
	* @param adStartOffset The current ad's start offset in milliseconds
	*/
	PlacementObj(const std::string& pendingAdbrkId, const std::string& openPeriodId, uint64_t curEndNumber,
		int curAdIdx, uint32_t adNextOffset, uint32_t adStartOffset, bool waitForNextPeriod)
			: pendingAdbrkId(pendingAdbrkId), openPeriodId(openPeriodId), curEndNumber(curEndNumber),
			curAdIdx(curAdIdx), adNextOffset(adNextOffset), adStartOffset(adStartOffset), waitForNextPeriod(waitForNextPeriod)
	{

	}
};


/**
 * @class PrivateCDAIObjectMPD
 *
 * @brief Private Client Side DAI object for DASH
 */
class PrivateCDAIObjectMPD
{
public:
	PrivateInstanceAAMP*                           mAamp;               /**< AAMP player's private instance */
	std::mutex                                     mDaiMtx;             /**< Mutex protecting DAI critical section */
	bool                                           mIsFogTSB;           /**< Channel playing from TSB or not */
	std::unordered_map<std::string, AdBreakObject> mAdBreaks;           /**< Periodid to adbreakobject map*/
	std::unordered_map<std::string, Period2AdData> mPeriodMap;          /**< periodId to Ad map */
	std::string                                    mCurPlayingBreakId;  /**< Currently playing Ad */
	std::thread                                    mAdObjThreadID;      /**< ThreadId of Ad fulfillment */
	bool                                           mAdObjThreadStarted; /**< Flag denotes if ad object thread is started */
	AdNodeVectorPtr                                mCurAds;             /**< Vector of ads from the current Adbreak */
	int                                            mCurAdIdx;           /**< Currently playing Ad index */
	AdFulfillObj                                   mAdFulfillObj;       /**< Temporary object for Ad fulfillment (to pass to the fulfillment thread) */
	PlacementObj                                   mPlacementObj;       /**< Temporary object for Ad placement over period */
	double                                         mContentSeekOffset;  /**< Seek offset after the Ad playback */
	AdState                                        mAdState;            /**< Current state of the CDAI state machine */
	bool                                           currentAdPeriodClosed;/**< The very next open period should be processed only when the flag is true*/
	std::vector<PlacementObj>                      mAdtoInsertInNextBreakVec;/**<Stores the PlacementObj yet to be placed*/
	std::mutex                                     mAdBrkVecMtx;        /**< Mutex protecting DAI critical section */
	std::mutex                                     mAdFulfillMtx;        /**< Mutex protecting Ad fulfillment */
	std::condition_variable                        mAdFulfillCV;         /**< Condition variable for AdBreak vector */
	std::queue<AdFulfillObj>                       mAdFulfillQ;            /**< Queue for Ad events */
	bool                                           mExitFulfillAdLoop;    /**< Flag to exit the Ad fulfillment loop */
	std::mutex                                     mAdPlacementMtx;       /**< Mutex protecting Ad placement */
	std::condition_variable                        mAdPlacementCV;        /**< Condition variable for Ad placement */

	/**
	 * @fn PrivateCDAIObjectMPD
	 *
	 * @param[in] aamp - Pointer to PrivateInstanceAAMP
	 */
	PrivateCDAIObjectMPD(PrivateInstanceAAMP* aamp);

	/**
	 * @fn ~PrivateCDAIObjectMPD
	 */
	~PrivateCDAIObjectMPD();

	/**
	* @brief PrivateCDAIObjectMPD copy constructor
	*/
	PrivateCDAIObjectMPD(const PrivateCDAIObjectMPD&) = delete;

	/**
	* @brief PrivateCDAIObjectMPD assignment operator
	*/
	PrivateCDAIObjectMPD& operator= (const PrivateCDAIObjectMPD&) = delete;

	/**
	 * @fn SetAlternateContents
	 *
	 * @param[in] periodId - Adbreak's unique identifier.
	 * @param[in] adId - Individual Ad's id
	 * @param[in] url - Ad URL
	 * @param[in] startMS - Ad start time in milliseconds
	 * @param[in] breakdur - Adbreak's duration in MS
	 */
	void SetAlternateContents(const std::string &periodId, const std::string &adId, const std::string &url,  uint64_t startMS, uint32_t breakdur=0);

	/**
	 * @fn FulFillAdObject
	 *
	 * @return bool true or false
	 */
	bool FulFillAdObject();

	/**
	 * @fn GetAdMPD
	 *
	 * @param[in]  url - Ad manifest's URL
	 * @param[out] finalManifest - Is final MPD or the final MPD should be downloaded later
	 * @param[out] http_error - http error code
	 * @param[out] downloadTime - Time taken to download the manifest
	 * @param[in]  tryFog - Attempt to download from FOG or not
	 * @return MPD* MPD instance
	 */
	MPD* GetAdMPD(std::string &url, bool &finalManifest, int &http_error, double &downloadTime, bool tryFog = false);

	/**
	 * @fn InsertToPeriodMap
	 *
	 * @param[in]  period - Pointer of the period to be inserted
	 */
	void InsertToPeriodMap(IPeriod *period);

	/**
	 * @fn isPeriodExist
	 *
	 * @param[in]  periodId - Period id to be checked.
	 * @return bool true or false
	 */
	bool isPeriodExist(const std::string &periodId);

	/**
	 * @brief Method to check the existence of Adbreak object in the AdbreakObject map
	 *
	 * @param[in]  adBrkId - Adbreak id to be checked.
	 * @return bool true or false
	 */
	bool isAdBreakObjectExist(const std::string &adBrkId);

	/**
	 * @fn PrunePeriodMaps
	 *
	 * @param[in]  newPeriodIds - Period ids from the latest manifest
	 */
	void PrunePeriodMaps(std::vector<std::string> &newPeriodIds);

	/**
	 * @fn ResetState
	 */
	void ResetState();

	/**
	 * @fn ClearMaps
	 */
	void ClearMaps();

	/**
	 * @brief Places ads using the provided AampMPDParseHelper object.
	 * This function is responsible for placing ads using the provided AampMPDParseHelper object.
	 *
	 * @param AampMPDParseHelperPtr shared_ptr to the AampMPDParseHelper object.
	 */
	void PlaceAds(AampMPDParseHelperPtr adMPDParseHelper);

	/**
	 * @brief Updates ad placement details for the next period in the MPD.
	 * @param[in] nextPeriod next period pointer
	 * @param[in] adStartOffset Starting offset for the ad in the next period.
	 */
	void UpdateNextPeriodAdPlacement(IPeriod* nextPeriod, uint32_t adStartOffset);

	/**
	 * @fn CheckForAdStart
	 *
	 * @param[in]  rate - Playback rate
	 * @param[in]  periodId - Period id to be checked
	 * @param[in]  offSet - Period offset in seconds
	 * @param[out] breakId - Id of the Adbreak, if the period & offset falls in an Adbreak
	 * @param[out] adOffset - Offset of the Ad for that point of the period in seconds
	 * @return int Ad index, if the period has an ad over it. Else -1
	 */
	int CheckForAdStart(const float &rate, bool init, const std::string &periodId, double offSet, std::string &breakId, double &adOffset);

	/**
	 * @fn CheckForAdTerminate
	 *
	 * @param[in]  fragmentTime - Current offset in the period
	 *
	 * @return True or false
	 */
	bool CheckForAdTerminate(double fragmentTime);

	/**
	 * @fn isPeriodInAdbreak
	 *
	 * @param[in]  periodId - Period id
	 *
	 * @return True or false
	 */
	inline bool isPeriodInAdbreak(const std::string &periodId);

	/**
	 * @fn setPlacementObj
	 * @brief Function to update the PlacementObj with the new available DAI ad
	 * @param[in] adBrkId : currentPlaying DAI AdId
	 * @param[in] endPeriodId : nextperiod to play(after DAI playback)
	 * @return new PlacementObj to be placed
	 */
	PlacementObj setPlacementObj(const std::string adBrkId, const std::string endPeriodId);

	/**
	 * @fn RemovePlacementObj
	 * @brief Function to erase the PlacementObj matching the adBreakId from mAdtoInsertInNextBreakVec Vector
	 * @param[in] adBrkId Ad break id to be erased
	 */
	void RemovePlacementObj(const std::string adBrkId);

	/**
	 * @fn HasDaiAd
	 * @brief Function Verify if the current period has DAI Ad
	 * @param[in] periodId Base period ID
	 * @return true if DAI Ad is present
	 */
	bool HasDaiAd(const std::string periodId);

	/**
	 * @fn setAdMarkers
	 * @brief Update ad markers for the current ad break being placed
	 * @param[in] p2AdDataduration Duration of the ad break
	 * @param[in] periodDelta Period delta
	 */
	void setAdMarkers(uint64_t p2AdDataduration, int64_t periodDelta);

	/**
	 * @fn FulfillAdLoop
	 */
	void FulfillAdLoop();

	/**
	 * @fn NotifyAdLoopWait
	 */
	void NotifyAdLoopWait();

	/**
	 * @fn StartFulfillAdLoop
	 */
	void StartFulfillAdLoop();

	/**
	 * @fn StopFulfillAdLoop
	 */
	void StopFulfillAdLoop();

	/**
	 * @fn CacheAdData
	 * @param[in] periodId - Period id
	 * @param[in] adId - Ad id
	 * @param[in] url - Ad URL
	 */
	void CacheAdData(const std::string &periodId, const std::string &adId, const std::string &url);

	/**
	 * @fn WaitForNextAdResolved for ad fulfillment
	 * @brief Wait for the next ad placement to complete with a timeout
	 * @param[in] timeoutMs Timeout value in milliseconds
	 * @return true if the ad placement completed within the timeout, false otherwise
	 */
	bool WaitForNextAdResolved(int timeoutMs);

	/**
	 * @fn WaitForNextAdResolved (with periodId parameter for initial ad placement)
	 * @brief Wait for the next ad placement to complete with a timeout
	 * @param[in] timeoutMs Timeout value in milliseconds
	 * @return true if the ad placement completed within the timeout, false otherwise
	 */
	bool WaitForNextAdResolved(int timeoutMs, std::string periodId);

	/**
	 * @fn AbortWaitForNextAdResolved
	 */
	void AbortWaitForNextAdResolved();

	/**
	 * @brief Get the ad duration of remaining ads to be placed in an adbreak
	 * @param[in] breakId - adbreak id
	 * @param[in] adIdx - current ad index
	 * @param[in] startOffset - start offset of current ad
	 */
	uint64_t GetRemainingAdDurationInBreak(const std::string &breakId, int adIdx, uint32_t startOffset);

	/**
	 * @brief Getting the next valid ad in the break to be placed
	 * @return true if the next ad is available, false otherwise
	 */
	bool GetNextAdInBreakToPlace();
};

#endif /* ADMANAGER_MPD_H_ */
