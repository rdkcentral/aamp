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
 * @file admanager_mpd.cpp
 * @brief Client side DAI manger for MPEG DASH
 */

#include "admanager_mpd.h"
#include "AampUtils.h"
#include "fragmentcollector_mpd.h"
#include <inttypes.h>

#include <algorithm>

/**
 * @brief CDAIObjectMPD Constructor
 */
CDAIObjectMPD::CDAIObjectMPD(PrivateInstanceAAMP* aamp): CDAIObject(aamp), mPrivObj(new PrivateCDAIObjectMPD(aamp))
{

}

/**
 * @brief CDAIObjectMPD destructor.
 */
CDAIObjectMPD::~CDAIObjectMPD()
{
	SAFE_DELETE(mPrivObj);
}

/**
 * @brief Setting the alternate contents' (Ads/blackouts) URL
 *
 */
void CDAIObjectMPD::SetAlternateContents(const std::string &periodId, const std::string &adId, const std::string &url,  uint64_t startMS, uint32_t breakdur)
{
	mPrivObj->SetAlternateContents(periodId, adId, url, startMS, breakdur);
}


/**
 * @brief PrivateCDAIObjectMPD constructor
 */
PrivateCDAIObjectMPD::PrivateCDAIObjectMPD(PrivateInstanceAAMP* aamp) : mAamp(aamp),mDaiMtx(), mIsFogTSB(false), mAdBreaks(), mPeriodMap(), mCurPlayingBreakId(), mAdObjThreadID(), mCurAds(nullptr),
					mCurAdIdx(-1), mContentSeekOffset(0), mAdState(AdState::OUTSIDE_ADBREAK),mPlacementObj(), mAdFulfillObj(),mAdObjThreadStarted(false),mImmediateNextAdbreakAvailable(false),currentAdPeriodClosed(false),mAdtoInsertInNextBreakVec(),
					mAdBrkVecMtx(), mAdFulfillMtx(), mAdFulfillCV(), mAdFulfillQ(), mExitFulfillAdLoop(false), mAdPlacementMtx(), mAdPlacementCV()
{
	StartFulfillAdLoop();
	mAamp->CurlInit(eCURLINSTANCE_DAI,1,mAamp->GetNetworkProxy());
}

/**
 * @brief PrivateCDAIObjectMPD destructor
 */
PrivateCDAIObjectMPD::~PrivateCDAIObjectMPD()
{
	AbortWaitForNextAdResolved();
	StopFulfillAdLoop();
	mAamp->CurlTerm(eCURLINSTANCE_DAI);
}

/**
 * @brief Method to insert period into period map
 */
void PrivateCDAIObjectMPD::InsertToPeriodMap(IPeriod * period)
{
	const std::string &prdId = period->GetId();
	if(!isPeriodExist(prdId))
	{
		mPeriodMap[prdId] = Period2AdData();
	}
}

/**
 * @brief Method to check the existence of period in the period map
 */
bool PrivateCDAIObjectMPD::isPeriodExist(const std::string &periodId)
{
	return (mPeriodMap.end() != mPeriodMap.find(periodId))?true:false;
}

/**
 * @brief Method to check the existence of Adbreak object in the AdbreakObject map
 */
bool PrivateCDAIObjectMPD::isAdBreakObjectExist(const std::string &adBrkId)
{
	return (mAdBreaks.end() != mAdBreaks.find(adBrkId))?true:false;
}

/**
 * @brief Method to remove expired periods from the period map
 */
void PrivateCDAIObjectMPD::PrunePeriodMaps(std::vector<std::string> &newPeriodIds)
{
	//Erase all adbreaks other than new adbreaks
	std::lock_guard<std::mutex> lock( mDaiMtx );
	for (auto it = mAdBreaks.begin(); it != mAdBreaks.end();)
	{
		/* We should not remove the adbreakObj that is currently getting placed (probably due to a bug in PlaceAds)
		 * and the adbreakObj which is currently playing whose mpd object will be in use
		 */
		if ((mPlacementObj.pendingAdbrkId != it->first) && (mCurPlayingBreakId != it->first) &&
				(newPeriodIds.end() == std::find(newPeriodIds.begin(), newPeriodIds.end(), it->first)))
		{
			// Erase the ad break object if it is not in the new period list
			auto &adBrkObj = *it;
			AAMPLOG_INFO("[CDAI] Removing the period[%s] from mAdBreaks.",adBrkObj.first.c_str());
			auto adNodes = adBrkObj.second.ads;
			if (adNodes)
			{
				for(AdNode &ad: *adNodes)
				{
					SAFE_DELETE(ad.mpd);
				}
			}
			ErasefrmAdBrklist(it->first);
			it = mAdBreaks.erase(it);
		}
		else
		{
			++it;
		}
	}

	//Erase all periods other than new periods
	for (auto it = mPeriodMap.begin(); it != mPeriodMap.end();)
	{
		if (newPeriodIds.end() == std::find(newPeriodIds.begin(), newPeriodIds.end(), it->first))
		{
			it = mPeriodMap.erase(it);
		}
		else
		{
			++it;
		}
	}
}

/**
 * @brief Method to reset the state of the CDAI state machine
 */
void PrivateCDAIObjectMPD::ResetState()
{
	 //TODO: Vinod, maybe we can move these playback state variables to PrivateStreamAbstractionMPD
	 mIsFogTSB = false;
	 mCurPlayingBreakId = "";
	 mCurAds = nullptr;
	 std::lock_guard<std::mutex> lock(mDaiMtx);
	 mCurAdIdx = -1;
	 mContentSeekOffset = 0;
	 mAdState = AdState::OUTSIDE_ADBREAK;
}

/**
 * @brief Method to clear the maps in the CDAI object
 */
void PrivateCDAIObjectMPD::ClearMaps()
{
	std::unordered_map<std::string, AdBreakObject> tmpMap;
	std::swap(mAdBreaks,tmpMap);
	for(auto &adBrkObj: tmpMap)
	{
		auto adNodes = adBrkObj.second.ads;
		if (adNodes)
		{
			for(AdNode &ad: *adNodes)
			{
				SAFE_DELETE(ad.mpd);
			}
		}
	}

	mPeriodMap.clear();
}

/**
 * @brief Method to create a bidirectional between the ads and the underlying content periods
 */
void  PrivateCDAIObjectMPD::PlaceAds(dash::mpd::IMPD *mpd)
{
	AampMPDParseHelper *adMPDParseHelper = nullptr;
	adMPDParseHelper  = new AampMPDParseHelper();
	adMPDParseHelper->Initialize(mpd);
	//Populate the map to specify the period boundaries
	if(mpd && (-1 != mPlacementObj.curAdIdx) && "" != mPlacementObj.pendingAdbrkId && isAdBreakObjectExist(mPlacementObj.pendingAdbrkId) && !mAdBreaks[mPlacementObj.pendingAdbrkId].mSrcPeriodOffsetGTthreshold) //Some Ad is still waiting for the placement
	{
		AdBreakObject &abObj = mAdBreaks[mPlacementObj.pendingAdbrkId];
		vector<IPeriod *> periods = mpd->GetPeriods();
		if(!abObj.adjustEndPeriodOffset) // not all ads are placed
		{
			bool openPrdFound = false;
			std::string prevOpenperiodId = mPlacementObj.openPeriodId;

			for(int iter = 0; iter < periods.size(); iter++)
			{
				if(abObj.adjustEndPeriodOffset)
				{
					// placement done no need to run for loop now
					break;
				}
				IPeriod* period = periods.at(iter);
				std::string periodId = period->GetId();
				//We need to check, open period is available in the manifest. Else, something wrong
				//While processing the current source period with DAI advertisement we saw multiple
				//open periods in the manifest.
				//So we need to make sure that the player processes only the very next open period
				//even it has multiple periods after the current ad period.
				if(mPlacementObj.openPeriodId == periodId)
				{
					openPrdFound = true;
					if(0 != (prevOpenperiodId.compare(mPlacementObj.openPeriodId)))
					{
						mPeriodMap[mPlacementObj.openPeriodId].filled = true;
						mPlacementObj.curEndNumber = 0;
					}
					prevOpenperiodId =mPlacementObj.openPeriodId;
				}
				else if(openPrdFound)
				{
					if(!currentAdPeriodClosed)
					{
						for(int iter = 0; iter < periods.size(); iter++)
						{
							if(mPlacementObj.openPeriodId == periodId)
							{
								// TODO:[VKB] Need to revisit this logic.
								period = periods.at(iter);
								periodId = period->GetId();
							}
						}
					}
					else if (currentAdPeriodClosed && adMPDParseHelper->aamp_GetPeriodDuration(iter, 0) > 0)
					{
						currentAdPeriodClosed = false;
						if(0 != (prevOpenperiodId.compare(mPlacementObj.openPeriodId)))
						{
							AAMPLOG_INFO("[CDAI]Previous openPeriod ended. New period(%s) in the adbreak will be the new open period",periodId.c_str());
							//Previous openPeriod ended. New period in the adbreak will be the new open period
							mPeriodMap[mPlacementObj.openPeriodId].filled = true;
							mPlacementObj.openPeriodId = periodId;
							prevOpenperiodId = periodId;
							mPlacementObj.curEndNumber = 0;
						}
					}
					else
					{
						// Empty period may come early; excluding them
						continue;
					}
				}

				if(openPrdFound && -1 != mPlacementObj.curAdIdx && (mPlacementObj.openPeriodId == periodId))
				{
					double periodDelta = adMPDParseHelper->GetPeriodNewContentDurationMs(period, mPlacementObj.curEndNumber);
					double currperioddur = adMPDParseHelper->aamp_GetPeriodDuration(iter, 0); 
					double nextperioddur = -1;
					if((iter+1) < periods.size())
					{
						nextperioddur = adMPDParseHelper->aamp_GetPeriodDuration(iter+1, 0);
					}
					Period2AdData& p2AdData = mPeriodMap[periodId];

					if("" == p2AdData.adBreakId)
					{
						//New period opened
						p2AdData.adBreakId = mPlacementObj.pendingAdbrkId;
						p2AdData.offset2Ad[0] = AdOnPeriod{mPlacementObj.curAdIdx,mPlacementObj.adNextOffset};
					}
					p2AdData.duration += periodDelta;
					double diffInDurationMs = currperioddur - p2AdData.duration;
					if(diffInDurationMs > 0)
					{
						AAMPLOG_WARN("[CDAI] Resetting p2AdData.duration!! periodId:%s diff:%lf periodDuration:%f p2AdData.duration:%" PRIu64 ,
								periodId.c_str(), diffInDurationMs, currperioddur, p2AdData.duration);
						periodDelta += diffInDurationMs;
						p2AdData.duration += diffInDurationMs;
					}
					AAMPLOG_INFO("periodDelta = %f p2AdData.duration = [%" PRIu64 "] mPlacementObj.adNextOffset = %u periodId = %s",periodDelta,p2AdData.duration,mPlacementObj.adNextOffset, periodId.c_str());
					bool isSrcdurnotequalstoaddur = false;
					if ((periodDelta == 0) && (nextperioddur > 0))
					{
						IPeriod* nextPeriod = periods.at(iter+1);
						if (nextPeriod)
						{
							// Next period was not available earlier when the adIdx was incremented, now the next period is present
							// Move onto next period to be placed
							if (mPlacementObj.waitForNextPeriod)
							{
								// Confirm the current ad is completely placed otherwise log an error. AD should be completely placed at this point.
								if ((abObj.ads->at(mPlacementObj.curAdIdx).duration - mPlacementObj.adNextOffset) != 0)
								{
									AAMPLOG_ERR("[CDAI] Error while placing ad[id:%s dur:%" PRIu64 "], remaining:%d to place, but skipping to next ad",
										abObj.ads->at(mPlacementObj.curAdIdx).adId.c_str(), abObj.ads->at(mPlacementObj.curAdIdx).duration,
										static_cast<int>(abObj.ads->at(mPlacementObj.curAdIdx).duration - mPlacementObj.adNextOffset));
								}
								// Mark the current ad as placed.
								abObj.ads->at(mPlacementObj.curAdIdx).placed = true;
								// Player ready to  process next period
								currentAdPeriodClosed = true;
								// Getting the next valid ad to get placed by iterating through the ad break
								if (true == GetNextAdInBreakToPlace())
								{
									AdNode &currAd = abObj.ads->at(mPlacementObj.curAdIdx);
									const std::string &nextPeriodId = nextPeriod->GetId();
									UpdateNextPeriodAdPlacement(nextPeriod, 0);
									if("" == currAd.basePeriodId)
									{
										//Next ad started placing from the beginning
										currAd.basePeriodId = nextPeriodId;
										currAd.basePeriodOffset = 0;
										AAMPLOG_INFO("[CDAI]currAd.basePeriodId:%s, currAd.basePeriodOffset:%d", currAd.basePeriodId.c_str(), currAd.basePeriodOffset);
										// offset2Ad is already updated above
									}
									// We have already moved into new period. Logging split period marker now.
									abObj.mSplitPeriod = true;
								}
								else
								{
									AAMPLOG_ERR("[CDAI] Reached the end of ads[size:%zu] in adbreak[%s], not expected", abObj.ads->size(), mPlacementObj.pendingAdbrkId.c_str());
								}
								continue;
							}
							AAMPLOG_INFO("nextPeriod:%s nextperioddur:%lf currperioddur:%lf adDuration:%" PRIu64 "", nextPeriod->GetId().c_str(), nextperioddur, currperioddur, abObj.ads->at(mPlacementObj.curAdIdx).duration);
							// Ad duration remaining to be placed in this break
							// adStartOffset signifies some portion of the current ad is already placed in a previous period
							double adDurationToPlaceInBreak = GetRemainingAdDurationInBreak(mPlacementObj.pendingAdbrkId, mPlacementObj.curAdIdx, mPlacementObj.adStartOffset);
							// This is the duration that is available in the current period, after deducting already placed ads if any.
							// If that duration not reaching the adDurationToPlaceInBreak, then its a split period case
							double periodDurationAvailable = (currperioddur - abObj.ads->at(mPlacementObj.curAdIdx).basePeriodOffset);
							if((nextperioddur > 0) && ((periodDurationAvailable >= 0) && (periodDurationAvailable <= adDurationToPlaceInBreak)))
							{
								AAMPLOG_INFO("nextperioddur = %f currperioddur = %f curAd.duration = [%" PRIu64 "] periodDurationAvailable:%lf adDurationToPlaceInBreak:%lf",
									nextperioddur,currperioddur,abObj.ads->at(mPlacementObj.curAdIdx).duration, periodDurationAvailable, adDurationToPlaceInBreak);
								isSrcdurnotequalstoaddur = true;
								// An ad exceeding the current period duration by more than 2 seconds is considered a split period
								// Source period duration should be more than tiny period to be treated as split period
								// If the tiny period just happens to be within a split period, then split period marker will be set which is expected as of now
								if ((currperioddur > THRESHOLD_TOIGNORE_TINYPERIOD) && ((periodDurationAvailable + OFFSET_ALIGN_FACTOR) < adDurationToPlaceInBreak))
								{
									abObj.mSplitPeriod = true;
								}
							}
						}
					}
					while(periodDelta > 0 || isSrcdurnotequalstoaddur)
					{
						AdNode &curAd = abObj.ads->at(mPlacementObj.curAdIdx);
						AAMPLOG_INFO("curAd.duration = [%" PRIu64 "]",curAd.duration);
						if(periodDelta < (curAd.duration - mPlacementObj.adNextOffset))
						{
							mPlacementObj.adNextOffset += periodDelta;
							if(isSrcdurnotequalstoaddur)
							{ // check if the current source period duration < current period ad duration and it is lest than offset factor
								AAMPLOG_INFO("nextperiod : %s with valid duration  available",periods.at(iter+1)->GetId().c_str());
								AAMPLOG_INFO("currperioddur : [%f] curAd.duration : %" PRIu64 " periodDelta : %f mPlacementObj.adNextOffset:%u diff : %" PRIu64 ,
									currperioddur, curAd.duration, periodDelta, mPlacementObj.adNextOffset, (curAd.duration - mPlacementObj.adNextOffset));

								currentAdPeriodClosed = true;//Player ready to  process next period
								// This is a split period case, so we need to update nextperiod as the open period, so that we can continue placement
								if(abObj.mSplitPeriod)
								{
									IPeriod* nextPeriod = periods.at(iter+1);
									UpdateNextPeriodAdPlacement(nextPeriod, mPlacementObj.adNextOffset);
								}
								else
								{
									// this is the case where ad is greater than source period but within OFFSET_ALIGN_FACTOR, so place ad
									curAd.placed = true;
									//Place the end markers of adbreak
									setAdMarkers(p2AdData.duration,periodDelta);
									if (p2AdData.duration > THRESHOLD_TOIGNORE_TINYPERIOD)
									{
										AAMPLOG_INFO("[CDAI] Source Ad duration is less than CDAI Ad. Mark as placed.end period:%s end period offset:%" PRIu64 " adjustEndPeriodOffset:%d",
											periodId.c_str(), abObj.endPeriodOffset, abObj.adjustEndPeriodOffset);
									}
									else
									{
										AAMPLOG_WARN("[CDAI] Detected tiny period[id:%s dur:%" PRIu64 "] with ad[numads:%zu adsdur:%" PRIu32 "], not expected, setting ads to invalid",
											periodId.c_str(), p2AdData.duration, abObj.ads->size(), abObj.adsDuration);
										// Mark all the ads in the break as invalid, so that we don't play this break anymore
										// This will not be called if the first period is not a split period
										for (int idx = 0; idx < abObj.ads->size(); idx++)
										{
											// We need to delete the ad entries once we fix FetcherLoop to pick tiny period
											abObj.ads->at(idx).placed = true;
											abObj.ads->at(idx).invalid = true;
										}
									}
								}
								break;
							}
							else
							{
								periodDelta = 0;
							}
						}
						//if we check period delta > OFFSET_ALIGN_FACTOR(previous logic), Player won't mark  the ad as completely placed if delta is less  than OFFSET_ALIGN_FACTOR(2000ms).This is a
						//corner case and player may fail to switch to next period
						//Player should mark the ad as placed if delta is greater than or equal to  the difference of  the ad duration and offset.
						else if((mPlacementObj.curAdIdx < (abObj.ads->size()-1))    //If it is not the last Ad, we can start placement immediately.
								|| periodDelta >= curAd.duration - mPlacementObj.adNextOffset)              //current ad completely placed.
						{
							int64_t remainingAdDuration = (curAd.duration - mPlacementObj.adNextOffset);
							if (remainingAdDuration >= 0)
							{
								// Adjust the params for the current ad
								mPlacementObj.adNextOffset += remainingAdDuration;
								periodDelta -= remainingAdDuration;
							}
							else
							{
								AAMPLOG_ERR("[CDAI] remainingAdDuration[%" PRId64 "] is -ve, not expected, adDuration:%" PRIu64 " nextOffset:%" PRIu32 ,
									remainingAdDuration, curAd.duration, mPlacementObj.adNextOffset);
							}
							isSrcdurnotequalstoaddur = false;
							// If another ad exists in this break
							if(mPlacementObj.curAdIdx+1 < abObj.ads->size())
							{
								// This case is added as part of split period. If periodDelta reaches zero, after the current ad is placed
								// it could be a period end or not and this is not clear at this moment. So we will delay setting the placed flag
								// If periodDelta is greater than zero, we can definitely start placing next ad.
								if (periodDelta > 0)
								{
									// Current Ad completely placed. But more space available in the current period for next Ad
									// Once marked as placed, we need to update the basePeriodOffset of next ad, otherwise fragmentTime calculation will go wrong
									curAd.placed = true;
									// Getting the next valid ad to get placed by iterating through the max ad size
									if (true == GetNextAdInBreakToPlace())
									{
										AdNode &nextAd = abObj.ads->at(mPlacementObj.curAdIdx);
										if("" == nextAd.basePeriodId)
										{
											// Next ad started placing
											nextAd.basePeriodId = periodId;
											nextAd.basePeriodOffset = p2AdData.duration - periodDelta;
											AAMPLOG_INFO("[CDAI]nextAd.basePeriodId:%s, nextAd.basePeriodOffset:%d", nextAd.basePeriodId.c_str(), nextAd.basePeriodOffset);
											int offsetKey = nextAd.basePeriodOffset;
											offsetKey = offsetKey - (offsetKey%OFFSET_ALIGN_FACTOR);
											// At offsetKey of the period, new Ad starts placing
											p2AdData.offset2Ad[offsetKey] = AdOnPeriod{mPlacementObj.curAdIdx,0};
										}
									}
								}
								else if (periodDelta == 0)
								{
									// Next period is not available. Update flag to wait
									// If the current period updates in the next iteration we will be able to start placing the next ad
									// If the next period is available or updated in the next iteration, we need to change open period to next period
									mPlacementObj.waitForNextPeriod = true;
								}
							}
							else
							{
								curAd.placed = true;
								currentAdPeriodClosed = true;//Player ready to  process next period
								mPlacementObj.curAdIdx++; // basically a no-op
								mPlacementObj.adNextOffset = 0; //basically a no-op
								// No ads left to place. lets mark the adbreak as complete
								setAdMarkers(p2AdData.duration,periodDelta);
								AAMPLOG_INFO("[CDAI] Current Ad completely placed.end period:%s end period offset:%" PRIu64 " adjustEndPeriodOffset:%d",periodId.c_str(),abObj.endPeriodOffset,abObj.adjustEndPeriodOffset);
								break;
							}
						}
						else
						{
							//No more ads to place & No sufficient space to finalize. Wait for next period/next mpd refresh.
							break;
						}
					}
				}
			}
		}
		if(abObj.adjustEndPeriodOffset) // make endPeriodOffset adjustment 
		{
			bool endPeriodFound = false;
			int iter =0;

			for(iter = 0; iter < periods.size(); iter++)
			{
				auto period = periods.at(iter);
				const std::string &periodId = period->GetId();
				//We need to check, end period is available in the manifest. Else, something wrong
				if(abObj.endPeriodId == periodId)
				{
					endPeriodFound = true;
					break;
				}
			}
			if(false == endPeriodFound) // something wrong keep the end-period positions same and proceed.
			{
				abObj.adjustEndPeriodOffset = false;
				abObj.mAdBreakPlaced = true;
				AAMPLOG_WARN("[CDAI] Couldn't adjust offset [endPeriodNotFound] ");
			}
			else
			{
				//Inserted Ads finishes in < 4 seconds of new period (inside the adbreak) : Play-head goes to the period’s beginning.
				if(abObj.endPeriodOffset < 2*OFFSET_ALIGN_FACTOR)
				{
					abObj.adjustEndPeriodOffset = false; // done with Adjustment
					abObj.endPeriodOffset = 0;//Aligning the last period
					abObj.mAdBreakPlaced = true;
					mPeriodMap[abObj.endPeriodId] = Period2AdData(); //Resetting the period with small out-lier.
					AAMPLOG_INFO("[CDAI] Adjusted endperiodOffset");
				}
				else
				{
					// get current period duration
					uint64_t currPeriodDuration = adMPDParseHelper->aamp_GetPeriodDuration(iter, 0);

					// Are we too close to current period end?
					//--> Inserted Ads finishes < 2 seconds behind new period : Channel play-back starts from new period.
					int diff = (int)(currPeriodDuration - abObj.endPeriodOffset);
					// if diff is negative or < OFFSET_ALIGN_FACTOR we have to wait for it to catch up
					// and either period will end with diff < OFFSET_ALIGN_FACTOR then adjust to next period start
					// or diff will be more than OFFSET_ALIGN_FACTOR then don't do any adjustment
					if (diff <  OFFSET_ALIGN_FACTOR)
					{
						//check if next period available
						iter++;
						if( iter < periods.size() && adMPDParseHelper->aamp_GetPeriodDuration(iter, 0) > 0)
						{
							auto nextPeriod = periods.at(iter);
							abObj.adjustEndPeriodOffset = false; // done with Adjustment
							abObj.endPeriodOffset = 0;//Aligning to next period start
							abObj.endPeriodId = nextPeriod->GetId();
							abObj.mAdBreakPlaced = true;
							AAMPLOG_INFO("[CDAI] diff [%d] close to period end [%" PRIu64 "],Aligning to next-period:%s", 
														diff,currPeriodDuration,abObj.endPeriodId.c_str());
						}
						else
						{
							AAMPLOG_INFO("[CDAI] diff [%d] close to period end [%" PRIu64 "],but next period not available,waiting", 
														diff,currPeriodDuration);
						}
					}// --> Inserted Ads finishes >= 2 seconds behind new period : Channel playback starts from that position in the current period.
					// OR //--> Inserted Ads finishes in >= 4 seconds of new period (inside the adbreak) : Channel playback starts from that position in the period.
					else
					{
						AAMPLOG_INFO("[CDAI] diff [%d] NOT close to period end, period:%s duration[%" PRIu64 "]", diff, mPlacementObj.pendingAdbrkId.c_str(), currPeriodDuration);

						abObj.adjustEndPeriodOffset = false; // done with Adjustment
						abObj.mAdBreakPlaced = true; // adbrk duration not equal to src period duration continue to play source period remaining duration
						abObj.mSrcPeriodOffsetGTthreshold = true;
					}
				}
			}

			if(!abObj.adjustEndPeriodOffset) // placed all ads now print the placement data and set mPlacementObj.curAdIdx = -1;
			{
				mPlacementObj.curAdIdx = -1;
				if (abObj.mSplitPeriod)
				{
					std::stringstream splitStr;
					for(auto it = mPeriodMap.begin();it != mPeriodMap.end();it++)
					{
						if(it->second.adBreakId == mPlacementObj.pendingAdbrkId)
						{
							splitStr<<" BasePeriod["<<it->first<<" -  "<<it->second.duration<<"]";
						}
					}
					for(int k=0;k<abObj.ads->size();k++)
					{
						splitStr<<" CDAIPeriod["<<abObj.ads->at(k).adId<<" - "<<abObj.ads->at(k).duration<<"]";
					}
					AAMPLOG_MIL("[CDAI] Detected split period.%s", splitStr.str().c_str());
				}
				//Printing the placement positions
				std::stringstream ss;
				ss<<"{AdbreakId: "<<mPlacementObj.pendingAdbrkId;
				ss<<", duration: "<<abObj.adsDuration;
				ss<<", endPeriodId: "<<abObj.endPeriodId;
				ss<<", endPeriodOffset: "<<abObj.endPeriodOffset;
				ss<<", #Ads: "<<abObj.ads->size() << ",[";
				for(int k=0;k<abObj.ads->size();k++)
				{
					AdNode &ad = abObj.ads->at(k);
					ss<<"\n{AdIdx:"<<k <<",AdId:"<<ad.adId<<",duration:"<<ad.duration<<",basePeriodId:"<<ad.basePeriodId<<", basePeriodOffset:"<<ad.basePeriodOffset<<"},";
				}
				ss<<"],\nUnderlyingPeriods:[ ";
				for(auto it = mPeriodMap.begin();it != mPeriodMap.end();it++)
				{
					if(it->second.adBreakId == mPlacementObj.pendingAdbrkId)
					{
						ss<<"\n{PeriodId:"<<it->first<<", duration:"<<it->second.duration;
						for(auto pit = it->second.offset2Ad.begin(); pit != it->second.offset2Ad.end() ;pit++)
						{
							ss<<", offset["<<pit->first<<"]=> Ad["<<pit->second.adIdx<<"@"<<pit->second.adStartOffset<<"]";
						}
					}
				}
				ss<<"]}";
				AAMPLOG_WARN("[CDAI] Placement Done: %s.",  ss.str().c_str());

			}
		}
		if(-1 == mPlacementObj.curAdIdx)
		{
			if(mAdtoInsertInNextBreakVec.empty())
			{
				mPlacementObj.pendingAdbrkId = "";
				mPlacementObj.openPeriodId = "";
				mPlacementObj.curEndNumber = 0;
				mPlacementObj.adNextOffset = 0;
				mImmediateNextAdbreakAvailable = false;
			}
			else
			{
				//Current ad break finished and the next ad break is available.
				//So need to call onAdEvent again from fetcher loop
				if(!mAdtoInsertInNextBreakVec.empty())
				{
					mPlacementObj = setPlacementObj(mPlacementObj.pendingAdbrkId,abObj.endPeriodId);
				}
                AAMPLOG_INFO("[CDAI]  num of adbrks avail: %zu ",mAdtoInsertInNextBreakVec.size());

				mImmediateNextAdbreakAvailable = true;
			}
		}
	}
	SAFE_DELETE(adMPDParseHelper);
}

/**
 * @brief Updates ad placement details for the next period in the MPD.
 * @param[in] nextPeriod next period pointer
 * @param[in] adStartOffset Starting offset for the ad in the next period.
 */
void PrivateCDAIObjectMPD::UpdateNextPeriodAdPlacement(IPeriod* nextPeriod, uint32_t adStartOffset)
{
	if (nextPeriod)
	{
		const std::string &nextPeriodId = nextPeriod->GetId();
		// If adbreak object exist for next period in split period with valid ads duration, log an error. This is not expected
		if (isAdBreakObjectExist(nextPeriodId) && mAdBreaks[nextPeriodId].adsDuration > 0)
		{
			// Lock the mutex, so we can delete this entry
			std::lock_guard<std::mutex> lock(mDaiMtx);
			const auto& adBreakObj = mAdBreaks[nextPeriodId];
			AAMPLOG_ERR("[CDAI] Detected ads for next period[id:%s, breakdur:%" PRIu32 ", numads:%zu] in split periods, not expected",
				nextPeriodId.c_str(), adBreakObj.brkDuration, adBreakObj.ads->size());
			{
				std::stringstream ss;
				for(int k=0;k<adBreakObj.ads->size();k++)
				{
					AdNode &ad = adBreakObj.ads->at(k);
					ss<<"{AdIdx:"<<k <<",AdId:"<<ad.adId<<",duration:"<<ad.duration<<",basePeriodId:"<<ad.basePeriodId<<", basePeriodOffset:"<<ad.basePeriodOffset<<"},";
					AAMPLOG_ERR("[CDAI] Removing ad break %s", ss.str().c_str());
				}
			}
			mAdBreaks.erase(nextPeriodId);
		}
		InsertToPeriodMap(nextPeriod);
		// Update the period2AdData entries for nextPeriod
		Period2AdData& nextP2AdData = mPeriodMap[nextPeriodId];
		nextP2AdData.adBreakId = mPlacementObj.pendingAdbrkId;
		nextP2AdData.offset2Ad[0] = AdOnPeriod{mPlacementObj.curAdIdx,mPlacementObj.adNextOffset};

		// Update the placement object to nextPeriod
		mPlacementObj.openPeriodId = nextPeriod->GetId();
		mPlacementObj.curEndNumber = 0;
		mPlacementObj.adStartOffset = adStartOffset;
	}
}
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
int PrivateCDAIObjectMPD::CheckForAdStart(const float &rate, bool init, const std::string &periodId, double offSet, std::string &breakId, double &adOffset)
{
	int adIdx = -1;
	auto pit = mPeriodMap.find(periodId);
	Period2AdData &curP2Ad = pit->second;
	if(mPeriodMap.end() != pit && !(pit->second.adBreakId.empty()))
	{
		//mBasePeriodId belongs to an Adbreak. Now we need to see whether any Ad is placed in the offset.
		// This condition may hit if the player is in TSB and if there is an abreak to place at live point. Possible?
		if(mPlacementObj.pendingAdbrkId != curP2Ad.adBreakId)
		{
			AAMPLOG_INFO("[CDAI] PlacementObj open adbreak(%s) and current period's(%s) adbreak(%s) not equal ... may be BUG ",
				mPlacementObj.pendingAdbrkId.c_str(), periodId.c_str(), curP2Ad.adBreakId.c_str());
		}
		if(isAdBreakObjectExist(curP2Ad.adBreakId))
		{
			breakId = curP2Ad.adBreakId;
			AdBreakObject &abObj = mAdBreaks[breakId];
			// seamLess is a faster way to iterate through the offset2Ad map. In this case, the offSet will be exactly or closer to the adStartOffset
			// For discrete playback, we need to iterate through the offset2Ad map to find the right ad
			bool seamLess = init?false:(AAMP_NORMAL_PLAY_RATE == rate);
			if(seamLess)
			{
				int floorKey = (int)(offSet * 1000);
				floorKey = floorKey - (floorKey%OFFSET_ALIGN_FACTOR);
				auto adIt = curP2Ad.offset2Ad.find(floorKey);
				if(curP2Ad.offset2Ad.end() == adIt)
				{
					//Need in cases like the current offset=29.5sec, next adAdSart=30.0sec
					int ceilKey = floorKey + OFFSET_ALIGN_FACTOR;
					adIt = curP2Ad.offset2Ad.find(ceilKey);
				}

				if((curP2Ad.offset2Ad.end() != adIt) && (0 == adIt->second.adStartOffset))
				{
					//Considering only Ad start
					adIdx = adIt->second.adIdx;
					adOffset = 0;
				}
			}
			else	//Discrete playback
			{
				uint64_t key = (uint64_t)(offSet * 1000);
				uint64_t start = 0;
				uint64_t end = curP2Ad.duration;
				if(periodId ==  abObj.endPeriodId)
				{
					// This adbreak was placed completely, so endPeriodOffset can be used which will be more accurate than curP2Ad.duration
					end = abObj.endPeriodOffset;	//No need to look beyond the adbreakEnd
				}
				if( (key >= start) &&
				    (key < end) || ( (rate < AAMP_RATE_PAUSE) && (key == end))
				  )
				{
					//Yes, Key is in Adbreak. Find which Ad.
					for(auto it = curP2Ad.offset2Ad.begin(); it != curP2Ad.offset2Ad.end(); it++)
					{
						if(key >= it->first)
						{
							adIdx = it->second.adIdx;
							adOffset = (double)(((key - it->first)/1000) + (it->second.adStartOffset/1000));
						}
						else
						{
							break;
						}
					}
				}
			}

			if(rate >= AAMP_NORMAL_PLAY_RATE && (-1 == adIdx) && (abObj.endPeriodId == periodId) && (uint64_t)(offSet*1000) >= abObj.endPeriodOffset)
			{
				breakId = "";	//AdState should not stick to IN_ADBREAK after Adbreak ends.
			}
		}
	}
    //reset the placementObj to current playing AdBreakId if it is not placed
    if((adIdx != -1 && !breakId.empty()) && (mPlacementObj.pendingAdbrkId != curP2Ad.adBreakId))
    {
		AAMPLOG_INFO("[CDAI] PlacementObj pendingAdbrkId(%s) and current period's(%s) adbreak(%s) not equal",
				mPlacementObj.pendingAdbrkId.c_str(), periodId.c_str(), curP2Ad.adBreakId.c_str());
	    AdBreakObject &abObj = mAdBreaks[curP2Ad.adBreakId];
	    AdNode &curAd = abObj.ads->at(adIdx);
	    if(!curAd.placed)
	    {
		    for(auto placementObj: mAdtoInsertInNextBreakVec)
            {
                if(curP2Ad.adBreakId == placementObj.pendingAdbrkId)
                {
                    mPlacementObj = placementObj;
                    AAMPLOG_INFO("[CDAI] change in pending AdBrkId to (%s) ",mPlacementObj.pendingAdbrkId.c_str());
                    break;
                }
            }
        }
    }
	return adIdx;
}

/**
 * @brief Checking to see if the position in a period corresponds to an end of Ad playback or not
 */
bool PrivateCDAIObjectMPD::CheckForAdTerminate(double currOffset)
{
	if (currOffset > 0)
	{
		uint64_t fragOffset = (uint64_t)(currOffset * 1000);
		if (mCurAds && (mCurAdIdx < mCurAds->size()))
		{
			if (fragOffset >= (mCurAds->at(mCurAdIdx).duration + OFFSET_ALIGN_FACTOR))
			{
				//Current Ad is playing beyond the AdBreak + OFFSET_ALIGN_FACTOR
				return true;
			}
		}
	}

	return false;

}

/**
 * @brief Checking to see if a period has Adbreak
 */
bool PrivateCDAIObjectMPD::isPeriodInAdbreak(const std::string &periodId)
{
	return !(mPeriodMap[periodId].adBreakId.empty());
}

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
MPD* PrivateCDAIObjectMPD::GetAdMPD(std::string &manifestUrl, bool &finalManifest, int &http_error, double &downloadTime, bool tryFog)
{
	MPD* adMpd = NULL;
	AampGrowableBuffer manifest("adMPD_CDN");
	bool gotManifest = false;
	std::string effectiveUrl;
	gotManifest = mAamp->GetFile(manifestUrl, eMEDIATYPE_MANIFEST, &manifest, effectiveUrl, &http_error, &downloadTime, NULL, eCURLINSTANCE_DAI);
	if (gotManifest)
	{
		AAMPLOG_TRACE("PrivateCDAIObjectMPD:: manifest download success");
	}
	else if (mAamp->DownloadsAreEnabled())
	{
		AAMPLOG_ERR("PrivateCDAIObjectMPD:: manifest download failed");
	}

	if (gotManifest)
	{
		if(mAamp->mConfig->IsConfigSet(eAAMPConfig_PlayAdFromCDN) && mAamp->mTsbType == "cloud")
		{
			finalManifest = false;
		}
		else
		{
			finalManifest = true;
		}
		xmlTextReaderPtr reader = xmlReaderForMemory( manifest.GetPtr(), (int) manifest.GetLen(), NULL, NULL, 0);
		if(tryFog && !mAamp->mConfig->IsConfigSet(eAAMPConfig_PlayAdFromCDN) && reader && mIsFogTSB)	//Main content from FOG. Ad is expected from FOG.
		{
			std::string channelUrl = mAamp->GetManifestUrl();	//TODO: Get FOG URL from channel URL
			std::string encodedUrl;
			UrlEncode(effectiveUrl, encodedUrl);
			int ipend = 0;
			for(int slashcnt=0; ipend < channelUrl.length(); ipend++)
			{
				if(channelUrl[ipend] == '/')
				{
					slashcnt++;
					if(slashcnt >= 3)
					{
						break;
					}
				}
			}

			effectiveUrl.assign(channelUrl.c_str(), 0, ipend);
			effectiveUrl.append("/adrec?clientId=FOG_AAMP&recordedUrl=");
			effectiveUrl.append(encodedUrl.c_str());

			AampGrowableBuffer fogManifest("adMPD_FOG");
			http_error = 0;
			mAamp->GetFile(effectiveUrl, eMEDIATYPE_MANIFEST, &fogManifest, effectiveUrl, &http_error, &downloadTime, NULL, eCURLINSTANCE_DAI);
			if(200 == http_error || 204 == http_error)
			{
				manifestUrl = effectiveUrl;
				if(200 == http_error)
				{
					//FOG already has the manifest. Releasing the one from CDN and using FOG's
					xmlFreeTextReader(reader);
					reader = xmlReaderForMemory(fogManifest.GetPtr(), (int) fogManifest.GetLen(), NULL, NULL, 0);
					manifest.Free();
					manifest.Replace(&fogManifest);
				}
				else
				{
					finalManifest = false;
				}
			}

			if(fogManifest.GetPtr())
			{
				fogManifest.Free();
			}
		}
		if (reader != NULL)
		{
			if (xmlTextReaderRead(reader))
			{
				Node* root = MPDProcessNode(&reader, manifestUrl, true);
				if (NULL != root)
				{
					std::vector<Node*> children = root->GetSubNodes();
					for (size_t i = 0; i < children.size(); i++)
					{
						Node* child = children.at(i);
						const std::string& name = child->GetName();
						AAMPLOG_INFO("PrivateCDAIObjectMPD:: child->name %s", name.c_str());
						if (name == "Period")
						{
							AAMPLOG_INFO("PrivateCDAIObjectMPD:: found period");
							std::vector<Node *> children = child->GetSubNodes();
							bool hasBaseUrl = false;
							for (size_t i = 0; i < children.size(); i++)
							{
								if (children.at(i)->GetName() == "BaseURL")
								{
									hasBaseUrl = true;
								}
							}
							if (!hasBaseUrl)
							{
								// BaseUrl not found in the period. Get it from the root and put it in the period
								children = root->GetSubNodes();
								for (size_t i = 0; i < children.size(); i++)
								{
									if (children.at(i)->GetName() == "BaseURL")
									{
										Node* baseUrl = new Node(*children.at(i));
										child->AddSubNode(baseUrl);
										hasBaseUrl = true;
										break;
									}
								}
							}
							if (!hasBaseUrl)
							{
								std::string baseUrlStr = Path::GetDirectoryPath(manifestUrl);
								Node* baseUrl = new Node();
								baseUrl->SetName("BaseURL");
								baseUrl->SetType(Text);
								baseUrl->SetText(baseUrlStr);
								AAMPLOG_INFO("PrivateCDAIObjectMPD:: manual adding BaseURL Node [%p] text %s",
								         baseUrl, baseUrl->GetText().c_str());
								child->AddSubNode(baseUrl);
							}
							break;
						}
					}
					adMpd = root->ToMPD();
					SAFE_DELETE(root);
				}
				else
				{
					AAMPLOG_WARN("Could not create root node");
				}
			}
			else
			{
				AAMPLOG_ERR("xmlTextReaderRead failed");
			}
			xmlFreeTextReader(reader);
		}
		else
		{
			AAMPLOG_ERR("xmlReaderForMemory failed");
		}

		if (AampLogManager::isLogLevelAllowed(eLOGLEVEL_TRACE))
		{ // use printf to avoid 2048 char syslog limitation
			manifest.AppendNulTerminator(); // make safe for cstring operations
			printf("***Ad manifest***:\n\n%s\n", manifest.GetPtr() );
		}
		manifest.Free();
	}
	else
	{
		AAMPLOG_ERR("[CDAI]: Error on manifest fetch");
	}
	return adMpd;
}

/**
 * @brief Method for fulfilling the Ad
 *
 * @return bool - true if the Ad is fulfilled successfully
 */
bool PrivateCDAIObjectMPD::FulFillAdObject()
{
	bool ret = true;
	AampMPDParseHelper adMPDParseHelper;
	bool adStatus = false;
	uint64_t startMS = 0;
	uint32_t durationMs = 0;
	bool finalManifest = false;
	std::lock_guard<std::mutex> lock( mDaiMtx );
	int http_error = 0;
	double downloadTime = 0;
	MPD *ad = GetAdMPD(mAdFulfillObj.url, finalManifest, http_error, downloadTime, true);
	if(ad)
	{
		adMPDParseHelper.Initialize(ad);
		auto periodId = mAdFulfillObj.periodId;
		if(ad->GetPeriods().size() && isAdBreakObjectExist(periodId))	// Ad has periods && ensuring that the adbreak still exists
		{
			auto &adbreakObj = mAdBreaks[periodId];
			AdNodeVectorPtr adBreakAssets = adbreakObj.ads;
			durationMs = (uint32_t)adMPDParseHelper.GetDurationFromRepresentation();

			startMS = adbreakObj.adsDuration;
			uint32_t availSpace = (uint32_t)(adbreakObj.brkDuration - startMS);
			if(availSpace < durationMs)
			{
				AAMPLOG_WARN("Adbreak's available space[%u] < Ad's Duration[%u]. Trimming the Ad.",  availSpace, durationMs);
				durationMs = availSpace;
			}
			adbreakObj.adsDuration += durationMs;

			// Add offset to mPeriodMap
			if(isPeriodExist(periodId) && mPeriodMap[periodId].offset2Ad.empty())
			{
				//First Ad placement is doing now.
				mPeriodMap[periodId].offset2Ad[0] = AdOnPeriod{0,0};
			}
			// Add entry to mAdtoInsertInNextBreakVec
			if(!HasDaiAd(periodId))
			{
				//If current ad index is -1 (that is no ads are pushed into the map yet), current ad placement can take place from here itself.
				//Otherwise, the Player need to wait until the current ad placement is done.
				if(mPlacementObj.curAdIdx == -1 )
				{
					mPlacementObj.pendingAdbrkId = periodId;
					mPlacementObj.openPeriodId = periodId;	//May not be available Now.
					mPlacementObj.curEndNumber = 0;
					mPlacementObj.curAdIdx = 0;
					mPlacementObj.adNextOffset = 0;
					mPlacementObj.adStartOffset = 0;
					mPlacementObj.waitForNextPeriod = false;
					mAdtoInsertInNextBreakVec.push_back(mPlacementObj);
					AAMPLOG_WARN("Next available DAI Ad break = %s",mPlacementObj.pendingAdbrkId.c_str());
				}
				else
				{
					// Add to an array of DAI ad's for B2B substitution
					mAdtoInsertInNextBreakVec.emplace_back(periodId, periodId, 0, 0, 0, 0, false);
				}
			}
			if(!finalManifest)
			{
				AAMPLOG_INFO("Final manifest to be downloaded from the FOG later. Deleting the manifest got from CDN.");
				SAFE_DELETE(ad);
			}
			if (!adBreakAssets->empty())
			{
				// Find the right AdNode from adBreakAssets based on mAdFulfillObj.adId
				for (int iter=0; iter<adBreakAssets->size(); iter++)
				{
					AdNode &node = adBreakAssets->at(iter);
					if (node.adId == mAdFulfillObj.adId)
					{
						node.mpd = ad;
						node.duration = durationMs;
						if (iter == 0)
						{
							node.basePeriodId = periodId;
							node.basePeriodOffset = 0;
						}
						else
						{
							// For subsequent ads in an adbreak, basePeriodId and basePeriodOffset will be filled on placement
							node.basePeriodId ="";
							node.basePeriodOffset = -1;
						}
						node.url = mAdFulfillObj.url;
						node.resolved = true;
						break;
					}
				}
			}
			else
			{
				// Handle the case where the vector is empty if necessary
				// For example, you might want to push the new node if the vector is empty
				AAMPLOG_WARN("AdBreakAssets is empty. Adding new Ad, May be a BUG in fulfill queue.");
				adBreakAssets->emplace_back(AdNode{false, false, true, mAdFulfillObj.adId, mAdFulfillObj.url, durationMs, periodId, 0, ad});
			}
			AAMPLOG_WARN("New Ad successfully for periodId : %s added[Id=%s, url=%s, durationMs=%" PRIu32 "].",periodId.c_str(),mAdFulfillObj.adId.c_str(),mAdFulfillObj.url.c_str(), durationMs);
			adStatus = true;
		}
		else
		{
			AAMPLOG_WARN("AdBreakId[%s] not existing. Dropping the Ad.", periodId.c_str());
			SAFE_DELETE(ad);
		}
	}
	else
	{
		if(CURLE_ABORTED_BY_CALLBACK == http_error)
		{
			AAMPLOG_WARN("Ad MPD[%s] download aborted.", mAdFulfillObj.url.c_str());
			ret = false;
		}
		else
		{
			// Check if the ad break object exists for the given period ID
			if(isAdBreakObjectExist(mAdFulfillObj.periodId))
			{
				// Retrieve the ad break object
				auto &adbreakObj = mAdBreaks[mAdFulfillObj.periodId];
				// Ensure the ad break object and its ads vector are valid
				if(adbreakObj.ads)
				{
					for (int iter=0; iter<adbreakObj.ads->size(); iter++)
					{
						AdNode &node = adbreakObj.ads->at(iter);
						// Check if the ad ID matches the one we are looking for
						if (node.adId == mAdFulfillObj.adId)
						{
							// Mark the ad node as resolved and invalid
							node.resolved = true;
							node.invalid = true;
							break;
						}
					}
				}
			}
			AAMPLOG_ERR("Failed to get Ad MPD[%s].", mAdFulfillObj.url.c_str());
		}
	}
	// Send the resolved event
	if(ret)
	{
		// Send the resolved event to the player
		AbortWaitForNextAdResolved();
		mAamp->SendAdResolvedEvent(mAdFulfillObj.adId, adStatus, startMS, durationMs);
	}
	return ret;
}

/**
 * @fn SetAlternateContents
 *
 * @param[in] periodId - Adbreak's unique identifier.
 * @param[in] adId - Individual Ad's id
 * @param[in] url - Ad URL
 * @param[in] startMS - Ad start time in milliseconds
 * @param[in] breakdur - Adbreak's duration in MS
 */
void PrivateCDAIObjectMPD::SetAlternateContents(const std::string &periodId, const std::string &adId, const std::string &url,  uint64_t startMS, uint32_t breakdur)
{
	if("" == adId || "" == url)
	{
		std::lock_guard<std::mutex> lock(mDaiMtx);
		//Putting a place holder
		if(!(isAdBreakObjectExist(periodId)))
		{
			auto adBreakAssets = std::make_shared<std::vector<AdNode>>();
			mAdBreaks.emplace(periodId, AdBreakObject{breakdur, adBreakAssets, "", 0, 0});	//Fix the duration after getting the Ad
			Period2AdData &pData = mPeriodMap[periodId];
			pData.adBreakId = periodId;
		}
	}
	else
	{
		bool adCached = false;
		if(isAdBreakObjectExist(periodId))
		{
			auto &adbreakObj = mAdBreaks[periodId];
			if(adbreakObj.brkDuration <= adbreakObj.adsDuration)
			{
				AAMPLOG_WARN("No more space left in the Adbreak. Rejecting the promise.");
			}
			else
			{
				//Cache the Ad to be placed later
				CacheAdData(periodId, adId, url);
				adCached = true;
			}
		}
		// Reject the promise as ad couldn't be resolved
		if(!adCached)
		{
			mAamp->SendAdResolvedEvent(adId, false, 0, 0);
		}
	}
}

/**
 * @fn setPlacementObj
 * @brief Function to update the PlacementObj with the new available DAI ad
 * @param[in] adBrkId : currentPlaying DAI AdId
 * @param[in] endPeriodId : nextperiod to play(after DAI playback)
 * @return new PlacementObj to be placed
 */
PlacementObj PrivateCDAIObjectMPD::setPlacementObj(std::string adBrkId,std::string endPeriodId)
{
	PlacementObj nxtPlacementObj = PlacementObj();
	mAdBrkVecMtx.lock();
	if(adBrkId == endPeriodId)
	{
		// If adBrkId and endPeriodId are same, then the current ad break is completely placed but the period is not filled.
		// So we are searching for any other ad breaks with the same period id to place next.
		AAMPLOG_INFO("[CDAI] Period %s playback remained endPeriodId : %s",adBrkId.c_str(),endPeriodId.c_str());
		if(mAdtoInsertInNextBreakVec.size() > 1)
		{
			for(auto itr = mAdtoInsertInNextBreakVec.begin();itr != mAdtoInsertInNextBreakVec.end();itr++)
			{
				if(adBrkId == itr->pendingAdbrkId)
				{
					if(++itr != mAdtoInsertInNextBreakVec.end())
					{
						// Move to the next placementObj in the vector
						nxtPlacementObj = (*itr);
						AAMPLOG_INFO("[CDAI] PeriodId [%s] has source content next placementObj [%s]",adBrkId.c_str(),nxtPlacementObj.pendingAdbrkId.c_str());
					}
					break;
				}
			}
		}
		else if (mAdtoInsertInNextBreakVec.size() == 1 && mAdtoInsertInNextBreakVec[0].pendingAdbrkId != adBrkId)
		{
			nxtPlacementObj = mAdtoInsertInNextBreakVec[0];
			AAMPLOG_INFO("[CDAI]  PeriodId [%s] has source content next placementObj [%s]  first element of adbrkVec",adBrkId.c_str(),nxtPlacementObj.pendingAdbrkId.c_str());
		}
	}
	else
	{
		// If adBrkId and endPeriodId are different, then the current ad break is completely placed and the period is filled.
		// So we are searching if the next period has DAI ad to place or not.
		int adBrkIdx = -1;
		for(int i = 0;i < mAdtoInsertInNextBreakVec.size();i++)
		{
			if( mAdtoInsertInNextBreakVec[i].pendingAdbrkId ==  endPeriodId)
			{
				//DAI Ad available,point nxtPlacementObj to the endPeriod
				nxtPlacementObj = mAdtoInsertInNextBreakVec[i]; 
				AAMPLOG_INFO("[CDAI] Placed AdBrkId = %s ",nxtPlacementObj.pendingAdbrkId.c_str());
				break;
			}
			else if(mAdtoInsertInNextBreakVec[i].pendingAdbrkId == adBrkId)  // get the current PlaceAd Break Index
			{
				adBrkIdx = i;
			}
		}
		// We didn't find any DAI ad in the next period (nxtPlacementObj.curAdIdx == -1). So we are checking if there is a next available DAI ad to place.
		if(nxtPlacementObj.curAdIdx == -1 && (adBrkIdx != -1 && ++adBrkIdx < mAdtoInsertInNextBreakVec.size()))
		{
			AAMPLOG_INFO("[CDAI] endPeriodId[%s] is not an DAI AD assign the placementObj with next available CDAI Ad[%s]",endPeriodId.c_str(),mAdtoInsertInNextBreakVec[adBrkIdx].pendingAdbrkId.c_str());
			nxtPlacementObj = mAdtoInsertInNextBreakVec[adBrkIdx];
		}
	}
	AAMPLOG_INFO("[CDAI] Placed AdBrkId = %s curAdIx = %d",nxtPlacementObj.pendingAdbrkId.c_str(),nxtPlacementObj.curAdIdx);
	mAdBrkVecMtx.unlock();
	return nxtPlacementObj;
}

/**
 * @fn ErasefrmAdBrklist
 * @brief Function to erase the current PlayingAdBrkId from next AdBreak Vector
 * @param[in] adBrkId Ad break id to be erased
 */
void PrivateCDAIObjectMPD::ErasefrmAdBrklist(const std::string adBrkId)
{
	mAdBrkVecMtx.lock();
	for(auto it = mAdtoInsertInNextBreakVec.begin();it != mAdtoInsertInNextBreakVec.end();)
	{
		if(it->pendingAdbrkId == adBrkId)
		{
			AAMPLOG_WARN("Period Id : %s state : %s processed remove from the DAI adbrk list ",it->pendingAdbrkId.c_str(),ADSTATE_STR[static_cast<int>(mAdState)]);
			it = mAdtoInsertInNextBreakVec.erase(it);
			break;
		}
		else
		{
			++it;
		}
	}
	mAdBrkVecMtx.unlock();
}

/**
 * @fn HasDaiAd
 * @brief Function Verify if the current period has DAI Ad
 * @param[in] periodId Base period ID
 * @return true if DAI Ad is present
 */
bool PrivateCDAIObjectMPD::HasDaiAd(const std::string periodId)
{
	bool adFound = false;
	mAdBrkVecMtx.lock();
	if( !mAdtoInsertInNextBreakVec.empty() )
	{
		for(auto it = mAdtoInsertInNextBreakVec.begin();it != mAdtoInsertInNextBreakVec.end(); )
		{
			if(it->pendingAdbrkId == periodId)
			{
				adFound = true;
				break;
			}
			it++;
		}
	}
	mAdBrkVecMtx.unlock();
	return adFound;
}

/**
 * @fn setAdMarkers
 * @brief Update ad markers for the current ad break being placed
 * @param[in] p2AdDataduration Duration of the ad break
 * @param[in] periodDelta Period delta
 */
void PrivateCDAIObjectMPD::setAdMarkers(uint64_t p2AdDataduration,double periodDelta)
{
	AdBreakObject &abObj = mAdBreaks[mPlacementObj.pendingAdbrkId];
	abObj.endPeriodOffset = p2AdDataduration - periodDelta;
	abObj.endPeriodId = mPlacementObj.openPeriodId; //if it is the exact period boundary, end period will be the next one
	abObj.adjustEndPeriodOffset = true; // marked for later adjustment
}

/**
 * @fn FulfillAdLoop
 */
void PrivateCDAIObjectMPD::FulfillAdLoop()
{
	AAMPLOG_INFO("Enter");
	// Start tread
	do
	{
		std::unique_lock<std::mutex> lock(mAdFulfillMtx);
		// Wait for the condition variable to be notified
		// It goes into wait state if the queue is empty or if the downloads are disabled
		mAdFulfillCV.wait(lock, [this] {
			return (mAamp->DownloadsAreEnabled() && !mAdFulfillQ.empty()) || mExitFulfillAdLoop;});
		AAMPLOG_INFO("AdFulfillQ size[%zu]", mAdFulfillQ.size());
		// Check if the queue is not empty and downloads are enabled
		if(!mAdFulfillQ.empty() && mAamp->DownloadsAreEnabled() && !mExitFulfillAdLoop)
		{
			AdFulfillObj adFulfillObj = mAdFulfillQ.front();
			lock.unlock();
			mAdFulfillObj = adFulfillObj;
			AAMPLOG_INFO("Fulfilling Ad[%s] with URL[%s]", mAdFulfillObj.adId.c_str(), mAdFulfillObj.url.c_str());
			if(FulFillAdObject())
			{
				// Remove the fulfilled Ad from the queue,
				// if the Ad is successfully placed
				// otherwise, it will be retried in the next iteration
				mAdFulfillQ.pop();
			}
		}
	}
	while(!mExitFulfillAdLoop);
	AAMPLOG_INFO("Exit");
}

/**
 * @fn StartFulfillAdLoop
 */
void PrivateCDAIObjectMPD::StartFulfillAdLoop()
{
	if(!mAdObjThreadStarted)
	{
		mAdObjThreadStarted = true;
		mAdObjThreadID = std::thread(&PrivateCDAIObjectMPD::FulfillAdLoop, this);
		AAMPLOG_INFO("Thread created mAdObjThreadID[%zx]", GetPrintableThreadID(mAdObjThreadID));
	}
}

/**
 * @fn StopFulfillAdLoop
 */
void PrivateCDAIObjectMPD::StopFulfillAdLoop()
{
	if(mAdObjThreadStarted)
	{
		mExitFulfillAdLoop = true;
		NotifyAdLoopWait();
		mAdObjThreadID.join();
		mAdObjThreadStarted = false;
	}
}

/**
 * @fn NotifyAdLoopWait
 */
void PrivateCDAIObjectMPD::NotifyAdLoopWait()
{
	{
		std::lock_guard<std::mutex> lock(mAdFulfillMtx);
		AAMPLOG_INFO("Aborting fulfill ad loop wait.");
	}
	mAdFulfillCV.notify_one();
}

/**
 * @fn CacheAdData
 * @brief Function to cache the Ad data to be placed later
 * @param[in] periodId Base period ID
 * @param[in] adId Ad ID
 * @param[in] url Ad URL
 */
void PrivateCDAIObjectMPD::CacheAdData(const std::string &periodId, const std::string &adId, const std::string &url)
{
	bool notify = false;
	{
		std::lock_guard<std::mutex> lock(mAdFulfillMtx);
		if(isAdBreakObjectExist(periodId))
		{
			mAdBreaks[periodId].ads->emplace_back(AdNode{false, false, false, adId, url, 0, "", 0, NULL});
			mAdFulfillQ.push(AdFulfillObj(periodId, adId, url));
			notify = true;
		}
		else
		{
			AAMPLOG_WARN("[CDAI] AdBreakId[%s] not existing. Dropping the Ad.", periodId.c_str());
		}
	}
	if(notify)
	{
		NotifyAdLoopWait();
	}
}

/**
 * @fn WaitForNextAdResolved for ad fulfillment
 * @brief Wait for the next ad placement to complete with a timeout
 * @param[in] timeoutMs Timeout value in milliseconds
 * @return true if the ad placement completed within the timeout, false otherwise
 */
bool PrivateCDAIObjectMPD::WaitForNextAdResolved(int timeoutMs)
{
	std::unique_lock<std::mutex> lock(mAdPlacementMtx);
	bool completed = false;
	if (!mAdFulfillObj.periodId.empty() && isAdBreakObjectExist(mAdFulfillObj.periodId))
	{
		auto& ads = this->mAdBreaks[mAdFulfillObj.periodId].ads;
		auto adId = mAdFulfillObj.adId;
		auto it = std::find_if(ads->begin(), ads->end(), [adId](const AdNode& node) {
			return node.adId == adId;
		});
		if (it != ads->end())
		{
			if (!it->resolved)
			{
				AAMPLOG_INFO("Waiting for next ad placement to complete with timeout %d ms.", timeoutMs);
				completed = mAdPlacementCV.wait_for(lock, std::chrono::milliseconds(timeoutMs), [it] {
					return it->resolved;
				});
			}
			else
			{
				completed = true;
			}
		}
	}
	AAMPLOG_INFO("Received notification for next ad placement.");
	return completed;
}

/**
 * @fn WaitForNextAdResolved (with periodId parameter for initial ad placement)
 * @brief Wait for the next ad placement to complete with a timeout
 * @param[in] timeoutMs Timeout value in milliseconds
 * @return true if the ad placement completed within the timeout, false otherwise
 */
bool PrivateCDAIObjectMPD::WaitForNextAdResolved(int timeoutMs, std::string periodId)
{
	std::unique_lock<std::mutex> lock(mAdPlacementMtx);
	bool completed = false;
	AAMPLOG_INFO("Waiting for next ad placement in %s to complete with timeout %d ms.", periodId.c_str(), timeoutMs);
	if (mAdPlacementCV.wait_for(lock, std::chrono::milliseconds(timeoutMs)) == std::cv_status::no_timeout)
	{
		completed = true;
	}
	else
	{
		AAMPLOG_INFO("Timed out waiting for next ad placement.");
		if(isAdBreakObjectExist(periodId))
		{
			// Mark the ad break as invalid
			mAdBreaks[periodId].invalid = true;
		}
	}
	return completed;
}

/**
 * @fn AbortWaitForNextAdResolved
 */
void PrivateCDAIObjectMPD::AbortWaitForNextAdResolved()
{
	{
		std::lock_guard<std::mutex> lock(mAdPlacementMtx);
		AAMPLOG_INFO("Aborting wait for next ad placement.");
	}
	mAdPlacementCV.notify_one();
}

/**
 * @brief Get the ad duration of remaining ads to be placed in an adbreak
 * @param[in] breakId - adbreak id
 * @param[in] adIdx - current ad index
 * @param[in] startOffset - start offset of current ad
 */
uint64_t PrivateCDAIObjectMPD::GetRemainingAdDurationInBreak(const std::string &breakId, int adIdx, uint32_t startOffset)
{
	uint64_t duration = 0;
	if (!breakId.empty())
	{
		AdBreakObject &abObj = mAdBreaks[breakId];
		for (int idx = adIdx; idx < abObj.ads->size(); idx++)
		{
			duration += abObj.ads->at(idx).duration;
		}
		if (duration > startOffset)
		{
			duration -= startOffset;
		}
	}
	return duration;
}

/**
 * @brief Getting the next valid ad in the break to be placed
 * @return true if the next ad is available, false otherwise
 */
bool PrivateCDAIObjectMPD::GetNextAdInBreakToPlace()
{
	AdBreakObject &abObj = mAdBreaks[mPlacementObj.pendingAdbrkId];
	bool ret = false;
	// Move to next ad and reset adNextOffset
	mPlacementObj.curAdIdx++;
	while (mPlacementObj.curAdIdx < abObj.ads->size())
	{
		if (abObj.ads->at(mPlacementObj.curAdIdx).invalid)
		{
			AAMPLOG_INFO("[CDAI] Current ad index to be placed[%d] is invalid", mPlacementObj.curAdIdx);
			mPlacementObj.curAdIdx++;
		}
		else
		{
			AAMPLOG_INFO("[CDAI] Current ad index to be placed[%d] is valid", mPlacementObj.curAdIdx);
			// Valid ad is found so return true
			ret = true;
			break;
		}
	}
	// New Ad's offset
	mPlacementObj.adNextOffset = 0;
	return ret;
}
