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
CDAIObjectMPD::CDAIObjectMPD(AampLogManager *logObj, PrivateInstanceAAMP* aamp): CDAIObject(logObj, aamp), mPrivObj(new PrivateCDAIObjectMPD(logObj, aamp))
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
PrivateCDAIObjectMPD::PrivateCDAIObjectMPD(AampLogManager* logObj, PrivateInstanceAAMP* aamp) : mLogObj(logObj),mAamp(aamp),mDaiMtx(), mIsFogTSB(false), mAdBreaks(), mPeriodMap(), mCurPlayingBreakId(), mAdObjThreadID(), mAdFailed(false), mCurAds(nullptr),
					mCurAdIdx(-1), mContentSeekOffset(0), mAdState(AdState::OUTSIDE_ADBREAK),mPlacementObj(), mAdFulfillObj(),mAdObjThreadStarted(false),mImmediateNextAdbreakAvailable(false),currentAdPeriodClosed(false),mAdtoInsertInNextBreakVec(),mWaitForManifestUpdateFlag(false)
{
	mAamp->CurlInit(eCURLINSTANCE_DAI,1,mAamp->GetNetworkProxy());
}

/**
 * @brief PrivateCDAIObjectMPD destructor
 */
PrivateCDAIObjectMPD::~PrivateCDAIObjectMPD()
{
	if(mAdObjThreadStarted)
	{
		mAdObjThreadID.join();
		mAdObjThreadStarted = false;
	}
	mWaitForManifestUpdateFlag = false;
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
inline bool PrivateCDAIObjectMPD::isAdBreakObjectExist(const std::string &adBrkId)
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
	for (auto it = mAdBreaks.begin(); it != mAdBreaks.end();) {
		if ((mPlacementObj.pendingAdbrkId != it->first) && (mCurPlayingBreakId != it->first) &&//We should not remove the pending/playing adbreakObj
				(newPeriodIds.end() == std::find(newPeriodIds.begin(), newPeriodIds.end(), it->first))) {
			auto &adBrkObj = *it;
			AAMPLOG_INFO("[CDAI] Removing the period[%s] from mAdBreaks.",adBrkObj.first.c_str());
			auto adNodes = adBrkObj.second.ads;
			for(AdNode &ad: *adNodes)
			{
				SAFE_DELETE(ad.mpd);
			}
			ErasefrmAdBrklist(it->first);
			it = mAdBreaks.erase(it);
		} else {
			++it;
		}
	}

	//Erase all periods other than new periods
	for (auto it = mPeriodMap.begin(); it != mPeriodMap.end();) {
		if (newPeriodIds.end() == std::find(newPeriodIds.begin(), newPeriodIds.end(), it->first)) {
			it = mPeriodMap.erase(it);
		} else {
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
	 mAdFailed = false;
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
		for(AdNode &ad: *adNodes)
		{
			SAFE_DELETE(ad.mpd);
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
	if(mpd && (-1 != mPlacementObj.curAdIdx) && "" != mPlacementObj.pendingAdbrkId && isAdBreakObjectExist(mPlacementObj.pendingAdbrkId)) //Some Ad is still waiting for the placement
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
				//open periods in the sky manifest.
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
						continue;		//Empty period may come early; excluding them
					}
				}

				if(openPrdFound && -1 != mPlacementObj.curAdIdx && (mPlacementObj.openPeriodId == periodId))
				{
					double periodDelta = aamp_GetPeriodNewContentDuration(mpd, period, mPlacementObj.curEndNumber);
					Period2AdData& p2AdData = mPeriodMap[periodId];

					if("" == p2AdData.adBreakId)
					{
						//New period opened
						p2AdData.adBreakId = mPlacementObj.pendingAdbrkId;
						p2AdData.offset2Ad[0] = AdOnPeriod{mPlacementObj.curAdIdx,mPlacementObj.adNextOffset};
					}

					p2AdData.duration += periodDelta;

					while(periodDelta > 0)
					{
						AdNode &curAd = abObj.ads->at(mPlacementObj.curAdIdx);
						if("" == curAd.basePeriodId)
						{
							//Next ad started placing
							curAd.basePeriodId = periodId;
							curAd.basePeriodOffset = p2AdData.duration - periodDelta;
							int offsetKey = curAd.basePeriodOffset;
							offsetKey = offsetKey - (offsetKey%OFFSET_ALIGN_FACTOR);
							p2AdData.offset2Ad[offsetKey] = AdOnPeriod{mPlacementObj.curAdIdx,0};	//At offsetKey of the period, new Ad starts
						}
						if(periodDelta < (curAd.duration - mPlacementObj.adNextOffset))
						{
							mPlacementObj.adNextOffset += periodDelta;
							periodDelta = 0;
						}
						//if we check period delta > OFFSET_ALIGN_FACTOR(previous logic), Player won't mark  the ad as completely placed if delta is less  than OFFSET_ALIGN_FACTOR(2000ms).This is a
						//corner case and player may fail to switch to next period
						//Player should mark the ad as placed if delta is greater than or equal to  the difference of  the ad duration and offset.
						else if((mPlacementObj.curAdIdx < (abObj.ads->size()-1))    //If it is not the last Ad, we can start placement immediately.
								|| periodDelta >= curAd.duration - mPlacementObj.adNextOffset)              //current ad completely placed.
						{
							//Current Ad completely placed. But more space available in the current period for next Ad
							curAd.placed = true;
							currentAdPeriodClosed = true;//Player ready to  process next period
							periodDelta -= (curAd.duration - mPlacementObj.adNextOffset);
							mPlacementObj.curAdIdx++;
							if(mPlacementObj.curAdIdx < abObj.ads->size())
							{
								mPlacementObj.adNextOffset = 0; //New Ad's offset
								AAMPLOG_INFO("[CDAI] New ad offset is setting to 0");
							}
							else
							{
								//Place the end markers of adbreak
								abObj.endPeriodId = periodId;	//If it is the exact period boundary, end period will be the next one
								abObj.endPeriodOffset = p2AdData.duration - periodDelta;
								abObj.adjustEndPeriodOffset = true; // marked for later adjustment
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
				AAMPLOG_WARN("[CDAI] Couldn't adjust offset [endPeriodNotFound] ");
			}
			else
			{
				//Inserted Ads finishes in < 4 seconds of new period (inside the adbreak) : Play-head goes to the period’s beginning.
				if(abObj.endPeriodOffset < 2*OFFSET_ALIGN_FACTOR)
				{
					abObj.adjustEndPeriodOffset = false; // done with Adjustment
					abObj.endPeriodOffset = 0;//Aligning the last period
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
					if(diff <  OFFSET_ALIGN_FACTOR)
					{
						//check if next period available
						iter++;
						if( iter < periods.size() )
						{
							auto nextPeriod = periods.at(iter);
							abObj.adjustEndPeriodOffset = false; // done with Adjustment
							abObj.endPeriodOffset = 0;//Aligning to next period start
							abObj.endPeriodId = nextPeriod->GetId();
							mPeriodMap[abObj.endPeriodId] = Period2AdData();
							mWaitForManifestUpdateFlag = false;
							AAMPLOG_INFO("[CDAI] [%d] close to period end [%" PRIu64 "],Aligning to next-period:%s", 
														diff,currPeriodDuration,abObj.endPeriodId.c_str());
						}
						else
						{
							mWaitForManifestUpdateFlag = true; //adbrk duration matches the source period duration wait for the manifest update to conitnue playback
							AAMPLOG_INFO("[CDAI] [%d] close to period end [%" PRIu64 "],but next period not available,waiting", 
														diff,currPeriodDuration);
						}
					}// --> Inserted Ads finishes >= 2 seconds behind new period : Channel playback starts from that position in the current period.
					// OR //--> Inserted Ads finishes in >= 4 seconds of new period (inside the adbreak) : Channel playback starts from that position in the period.
					else
					{
						AAMPLOG_INFO("[CDAI] [%d] NOT close to period end", diff);
						abObj.adjustEndPeriodOffset = false; // done with Adjustment
						mWaitForManifestUpdateFlag = false; // adbrk duration not equal to src period duration continue to play source period remaining duration
					}
				}
			}
			if(!abObj.adjustEndPeriodOffset) // placed all ads now print the placement data and set mPlacementObj.curAdIdx = -1;
			{
				mPlacementObj.curAdIdx = -1;
				mWaitForManifestUpdateFlag = false;
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
				//Current ad break finshed and the next ad break is avaialable.
				//So need to call onAdEvent again from fetcher loop
				if(!mAdtoInsertInNextBreakVec.empty())
				{
					mPlacementObj = setPlacementObj(mPlacementObj.pendingAdbrkId,abObj.endPeriodId);
				}
				AAMPLOG_INFO("[CDAI]  num of adbrks avail: %d ",mAdtoInsertInNextBreakVec.size());

				mImmediateNextAdbreakAvailable = true;
			}
			mWaitForManifestUpdateFlag = false;
		}
	}
	SAFE_DELETE(adMPDParseHelper);
}

/**
 * @brief Checking to see if a period is the begining of the Adbreak or not.
 *
 * @return Ad index, if the period has an ad over it. Else -1
 */
int PrivateCDAIObjectMPD::CheckForAdStart(const float &rate, bool init, const std::string &periodId, double offSet, std::string &breakId, double &adOffset)
{
	int adIdx = -1;
	auto pit = mPeriodMap.find(periodId);
	if(mPeriodMap.end() != pit && !(pit->second.adBreakId.empty()))
	{
		//mBasePeriodId belongs to an Adbreak. Now we need to see whether any Ad is placed in the offset.
		Period2AdData &curP2Ad = pit->second;
		if(isAdBreakObjectExist(curP2Ad.adBreakId))
		{
			breakId = curP2Ad.adBreakId;
			AdBreakObject &abObj = mAdBreaks[breakId];
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
					end = abObj.endPeriodOffset;	//No need to look beyond the adbreakEnd
				}
				if(key >= start && key <= end)
				{
					//Yes, Key is in Adbreak. Find which Ad.
					for(auto it = curP2Ad.offset2Ad.begin(); it != curP2Ad.offset2Ad.end(); it++)
					{
						if(key >= it->first)
						{
							adIdx = it->second.adIdx;
							adOffset = (double)((key - it->first)/1000);
						}
						else
						{
							break;
						}
					}
				}
			}

			if(rate >= AAMP_NORMAL_PLAY_RATE && -1 == adIdx && abObj.endPeriodId == periodId && (uint64_t)(offSet*1000) >= abObj.endPeriodOffset)
			{
				breakId = "";	//AdState should not stick to IN_ADBREAK after Adbreak ends.
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
	uint64_t fragOffset = (uint64_t)(currOffset * 1000);
	if(fragOffset >= (mCurAds->at(mCurAdIdx).duration + OFFSET_ALIGN_FACTOR))
	{
		//Current Ad is playing beyond the AdBreak + OFFSET_ALIGN_FACTOR
		return true;
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
 * @brief Method for downloading and parsing Ad's MPD
 *
 * @return Pointer to the MPD object
 */
MPD* PrivateCDAIObjectMPD::GetAdMPD(std::string &manifestUrl, bool &finalManifest, bool tryFog)
{
	MPD* adMpd = NULL;
	bool gotManifest = false;
	int http_error = 0;
	double downloadTime = 0;
	std::string effectiveUrl;
	//create MPD instance for ad manifest download
	AampMPDDownloader *mAdDownloaderInstance = nullptr;
	ManifestDownloadResponsePtr mAdManifestDnldRespPtr = std::make_shared<ManifestDownloadResponse> ();	
	std::shared_ptr<ManifestDownloadConfig> inpData    = std::make_shared<ManifestDownloadConfig> ();
	mAdDownloaderInstance 	= new AampMPDDownloader();
	//initialize the downloadconfig 
	inpData->mTuneUrl 	= manifestUrl;
	inpData->mDnldConfig->iStartTimeout = mAamp->mConfig->GetConfigValue(eAAMPConfig_CurlDownloadStartTimeout);
        inpData->mDnldConfig->iStallTimeout = mAamp->mConfig->GetConfigValue(eAAMPConfig_CurlStallTimeout);
        inpData->mDnldConfig->iLowBWTimeout = 0;
	inpData->mDnldConfig->iDownloadTimeout = mAamp->mConfig->GetConfigValue(eAAMPConfig_ManifestTimeout);
	inpData->mDnldConfig->bNeedDownloadMetrics = true;
	inpData->mDnldConfig->pCurl                  =       CurlStore::GetCurlStoreInstance(mAamp).GetCurlHandle(mAamp, manifestUrl, eCURLINSTANCE_DAI);
	inpData->mDnldConfig->userAgentString = mAamp->mConfig->GetUserAgentString();
	inpData->mDnldConfig->proxyName       = mAamp->mConfig->GetConfigValue(eAAMPConfig_NetworkProxy);
	mAdDownloaderInstance->Initialize(inpData, mLogObj);
	mAdDownloaderInstance->Start();
	mAdManifestDnldRespPtr = mAdDownloaderInstance->GetManifest(true, mAamp->mManifestTimeoutMs);

	http_error		=	mAdManifestDnldRespPtr->mMPDDownloadResponse->iHttpRetValue;
	gotManifest		=	(mAdManifestDnldRespPtr->mMPDStatus == AAMPStatusType::eAAMPSTATUS_OK);
	downloadTime	=	mAdManifestDnldRespPtr->mMPDDownloadResponse->downloadCompleteMetrics.total;
	effectiveUrl = mAdManifestDnldRespPtr->mMPDDownloadResponse->sEffectiveUrl;
	if (gotManifest)
	{
		AAMPLOG_TRACE("PrivateCDAIObjectMPD:: manifest download success");
	}
	else if (mAamp->DownloadsAreEnabled())
	{
		AAMPLOG_ERR("PrivateCDAIObjectMPD:: manifest download failed");
	}
	std::string manifest 	=	mAdManifestDnldRespPtr->mMPDDownloadResponse->getString();

	if (gotManifest)
	{
		finalManifest = true;
		xmlTextReaderPtr reader = xmlReaderForMemory( (char *)manifest.c_str(), (int) manifest.length(), NULL, NULL, 0);
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
			http_error = 0;
		
			mAdManifestDnldRespPtr.reset(); // To Clear the previous downloadresponse value and set to nullptr 

			//Initialize with new config data 
			inpData->mTuneUrl       	= effectiveUrl;
			inpData->mDnldConfig->bNeedDownloadMetrics 	= true;
			inpData->mDnldConfig->pCurl			=	CurlStore::GetCurlStoreInstance(mAamp).GetCurlHandle(mAamp, effectiveUrl, eCURLINSTANCE_DAI);
			mAdDownloaderInstance->Initialize(inpData, mLogObj);
			mAdDownloaderInstance->Start();

			mAdManifestDnldRespPtr   = mAdDownloaderInstance->GetManifest(true, mAamp->mManifestTimeoutMs);
			http_error               = mAdManifestDnldRespPtr->mMPDDownloadResponse->iHttpRetValue;
			effectiveUrl = mAdManifestDnldRespPtr->mMPDDownloadResponse->sEffectiveUrl;
			std::string fogManifest 	=	mAdManifestDnldRespPtr->mMPDDownloadResponse->getString();
			if(200 == http_error || 204 == http_error)
			{
				manifestUrl = effectiveUrl;
				if(200 == http_error)
				{
					//FOG already has the manifest. Releasing the one from CDN and using FOG's
					xmlFreeTextReader(reader);
					reader = xmlReaderForMemory(fogManifest.c_str(), (int) fogManifest.length(), NULL, NULL, 0);
					manifest = fogManifest;
				}
				else
				{
					finalManifest = false;
				}
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

		if (gpGlobalConfig->logging.trace)
		{
			AAMPLOG_WARN("Ad manifest: %s", manifest.c_str());
		}
	}
	else
	{
		AAMPLOG_ERR("[CDAI]: Error on manifest fetch");
	}
	mAdDownloaderInstance->Release();
	SAFE_DELETE(mAdDownloaderInstance);
	return adMpd;
}

/**
 * @brief Method for fullfilling the Ad
 */
void PrivateCDAIObjectMPD::FulFillAdObject()
{
	bool adStatus = false;
	uint64_t startMS = 0;
	uint32_t durationMs = 0;
	bool finalManifest = false;
	std::lock_guard<std::mutex> lock( mDaiMtx );
	MPD *ad = GetAdMPD(mAdFulfillObj.url, finalManifest, true);
	if(ad)
	{
		auto periodId = mAdFulfillObj.periodId;
		if(ad->GetPeriods().size() && isAdBreakObjectExist(periodId))	// Ad has periods && ensuring that the adbreak still exists
		{
			auto &adbreakObj = mAdBreaks[periodId];
			std::shared_ptr<std::vector<AdNode>> adBreakAssets = adbreakObj.ads;
			durationMs = (uint32_t)aamp_GetDurationFromRepresentation(ad);

			startMS = adbreakObj.adsDuration;
			uint32_t availSpace = (uint32_t)(adbreakObj.brkDuration - startMS);
			if(availSpace < durationMs)
			{
				AAMPLOG_WARN("Adbreak's available space[%u] < Ad's Duration[%u]. Trimming the Ad.",  availSpace, durationMs);
				durationMs = availSpace;
			}
			adbreakObj.adsDuration += durationMs;

			std::string bPeriodId = "";		//BasePeriodId will be filled on placement
			int bOffset = -1;				//BaseOffset will be filled on placement
			if(0 == adBreakAssets->size())
			{
				//First Ad placement is doing now.
				PlacementObj mAdtoInsertInNextBreak; //maintain an array of DAI ad's for B2B substitution
				if(isPeriodExist(periodId))
				{
					mPeriodMap[periodId].offset2Ad[0] = AdOnPeriod{0,0};
				}
				//If current ad index is -1 (that is no ads are pushed into the map yet), current ad placement can take place from here itself.
				//Otherwise, the Player need to wait until the current ad placement is done.

				if(mPlacementObj.curAdIdx == -1 )
				{
					mPlacementObj.pendingAdbrkId = periodId;
					mPlacementObj.openPeriodId = periodId;	//May not be available Now.
					mPlacementObj.curEndNumber = 0;
					mPlacementObj.curAdIdx = 0;
					mPlacementObj.adNextOffset = 0;
					mAdtoInsertInNextBreakVec.push_back(mPlacementObj);
				}
				else
				{
					mAdtoInsertInNextBreak.pendingAdbrkId = periodId;
					mAdtoInsertInNextBreak.openPeriodId = periodId;	//May not be available Now.
					mAdtoInsertInNextBreak.curEndNumber = 0;
					mAdtoInsertInNextBreak.curAdIdx = 0;
					mAdtoInsertInNextBreak.adNextOffset = 0;
					mAdtoInsertInNextBreakVec.push_back(mAdtoInsertInNextBreak);
				}
				bPeriodId = periodId;
				bOffset = 0;
			}
			if(!finalManifest)
			{
				AAMPLOG_INFO("Final manifest to be downloaded from the FOG later. Deleting the manifest got from CDN.");
				SAFE_DELETE(ad);
			}
			adBreakAssets->emplace_back(AdNode{false, false, mAdFulfillObj.adId, mAdFulfillObj.url, durationMs, bPeriodId, bOffset, ad});
			AAMPLOG_WARN("New Ad successfully periodId : %s added[Id=%s, url=%s].", mAdFulfillObj.adId.c_str(),mAdFulfillObj.url.c_str(),periodId.c_str());

			adStatus = true;
		}
		else
		{
			AAMPLOG_WARN("AdBreadkId[%s] not existing. Dropping the Ad.", periodId.c_str());
			SAFE_DELETE(ad);
		}
	}
	else
	{
		AAMPLOG_ERR("Failed to get Ad MPD[%s].", mAdFulfillObj.url.c_str());
	}
	mAamp->SendAdResolvedEvent(mAdFulfillObj.adId, adStatus, startMS, durationMs);
}

/**
 * @brief Setting the alternate contents' (Ads/blackouts) URL
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
		if(mAdObjThreadStarted)
		{
			//Clearing the previous thread
			mAdObjThreadID.join();
			mAdObjThreadStarted = false;
		}
		if(isAdBreakObjectExist(periodId))
		{
			auto &adbreakObj = mAdBreaks[periodId];
			int ret = 0;
			if(adbreakObj.brkDuration <= adbreakObj.adsDuration)
			{
				AAMPLOG_WARN("No more space left in the Adbreak. Rejecting the promise.");
				ret = -1;
			}
			else
			{
				mAdFulfillObj.periodId = periodId;
				mAdFulfillObj.adId = adId;
				mAdFulfillObj.url = url;
				try
				{
						mAdObjThreadID = std::thread(&PrivateCDAIObjectMPD::FulFillAdObject, this);
		    			AAMPLOG_INFO("Thread created (FulFillAdObject) [%lu]", GetPrintableThreadID(mAdObjThreadID));
						mAdObjThreadStarted = true;
				}
				catch(std::exception &e)
				{
		    			AAMPLOG_ERR(" thread create(FulFillAdObject) failed. Rejecting promise. : %s", e.what());
		    			ret = -1;
				}
			}
			if(ret != 0)
			{
				mAamp->SendAdResolvedEvent(mAdFulfillObj.adId, false, 0, 0);
			}
		}
	}
}

PlacementObj PrivateCDAIObjectMPD::setPlacementObj(std::string adBrkId,std::string endPeriodId)
{
	std::vector<PlacementObj>::iterator itr;
	PlacementObj nxtPlacementObj;
	mAdBrkVecMtx.lock();
	if(adBrkId == endPeriodId)
	{
		AAMPLOG_INFO("[CDAI] Period %s playback remained endPeriodId : %s",adBrkId.c_str(),endPeriodId.c_str());
		if(mAdtoInsertInNextBreakVec.size() > 1 )
		{
			for(itr = mAdtoInsertInNextBreakVec.begin();itr != mAdtoInsertInNextBreakVec.end();itr++)
			{
				if(adBrkId == itr->pendingAdbrkId)
				{
					if(++itr != mAdtoInsertInNextBreakVec.end())
					{
						nxtPlacementObj = (*itr);
						AAMPLOG_INFO("[CDAI] PeriodId [%s] has source content next playbacklacementObj [%s]  ",adBrkId.c_str(),nxtPlacementObj.pendingAdbrkId.c_str());
						break;
					}
				}
			}
		}
		else
		{
			nxtPlacementObj = mAdtoInsertInNextBreakVec[0];
			AAMPLOG_INFO("[CDAI]  PeriodId [%s] has source content next playbacklacementObj [%s]  first element of adbrkVec",adBrkId.c_str(),nxtPlacementObj.pendingAdbrkId.c_str());
		}
	}
	else
	{
		for(itr = mAdtoInsertInNextBreakVec.begin();itr != mAdtoInsertInNextBreakVec.end();itr++)
		{
			if(itr->pendingAdbrkId ==  endPeriodId)
			{
				nxtPlacementObj = (*itr);
				AAMPLOG_INFO("[CDAI] Placed AdBrkId = %s ",nxtPlacementObj.pendingAdbrkId.c_str());
				break;
			}
		}
	}
	AAMPLOG_INFO("[CDAI] Placed AdBrkId = %s ",nxtPlacementObj.pendingAdbrkId.c_str());
	mAdBrkVecMtx.unlock();
	return nxtPlacementObj;
}

/**
 * @fn ErasefrmAdBrklist
 * @brief Function to erase the current PlayingAdBrkId
 * from next AdBreak Vector
 * @param[in] mpd
 * @retval true if adstate changes.
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
* @param[in] string basperiodId
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
