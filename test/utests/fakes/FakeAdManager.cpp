/*
* If not stated otherwise in this file or this component's license file the
* following copyright and licenses apply:
*
* Copyright 2022 RDK Management
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

#include "admanager_mpd.h"
#include "MockAdManager.h"

MockPrivateCDAIObjectMPD *g_MockPrivateCDAIObjectMPD = nullptr;

CDAIObjectMPD::CDAIObjectMPD(PrivateInstanceAAMP* aamp): CDAIObject(aamp), mPrivObj(new PrivateCDAIObjectMPD(aamp))
{
}

CDAIObjectMPD::~CDAIObjectMPD()
{
	SAFE_DELETE(mPrivObj);
}

void CDAIObjectMPD::SetAlternateContents(const std::string &adBreakId, const std::string &adId, const std::string &url, uint64_t startMS, uint32_t breakdur)
{
	if(g_MockPrivateCDAIObjectMPD)
    {
		g_MockPrivateCDAIObjectMPD->SetAlternateContents(adBreakId, adId, url);
    }
}

PrivateCDAIObjectMPD::PrivateCDAIObjectMPD(PrivateInstanceAAMP* aamp) : mAamp(aamp),mDaiMtx(), mIsFogTSB(false), mAdBreaks(), mPeriodMap(), mCurPlayingBreakId(), mAdObjThreadID(), mCurAds(nullptr),
					mCurAdIdx(-1), mContentSeekOffset(0), mAdState(AdState::OUTSIDE_ADBREAK),mPlacementObj(), mAdFulfillObj(),mAdObjThreadStarted(false),mAdtoInsertInNextBreakVec(),mAdBrkVecMtx()
{
}

/**
 * @brief PrivateCDAIObjectMPD destructor
 */
PrivateCDAIObjectMPD::~PrivateCDAIObjectMPD()
{
}

MPD* PrivateCDAIObjectMPD::GetAdMPD(std::string &url, bool &finalManifest, int &http_error, double &downloadTime, AAMPCDAIError &errorCode, bool tryFog)
{
	return NULL;
}

void PrivateCDAIObjectMPD::PlaceAds(AampMPDParseHelperPtr adMPDParseHelper)
{
}

void PrivateCDAIObjectMPD::InsertToPeriodMap(IPeriod *period)
{
}

bool PrivateCDAIObjectMPD::CheckForAdTerminate(double fragmentTime)
{
	return false;
}

int PrivateCDAIObjectMPD::CheckForAdStart(const float &rate, bool init, const std::string &periodId, double offSet, std::string &breakId, double &adOffset)
{
	if(g_MockPrivateCDAIObjectMPD != nullptr)
	{
		return g_MockPrivateCDAIObjectMPD->CheckForAdStart(rate, init, periodId, offSet, breakId, adOffset);
	}
	return 0;
}

bool PrivateCDAIObjectMPD::isPeriodExist(const std::string &periodId)
{
	return false;
}

bool PrivateCDAIObjectMPD::isAdBreakObjectExist(const std::string &adBrkId)
{
	if(g_MockPrivateCDAIObjectMPD != nullptr)
	{
		return g_MockPrivateCDAIObjectMPD->isAdBreakObjectExist(adBrkId);
	}
	return false;
}

void PrivateCDAIObjectMPD::PrunePeriodMaps(std::vector<std::string> &newPeriodIds)
{
}

void PrivateCDAIObjectMPD::ResetState()
{
}

void PrivateCDAIObjectMPD::RemovePlacementObj(const std::string adBrkId)
{

}

bool PrivateCDAIObjectMPD::HasDaiAd(const std::string periodId)
{
	return false;
}

void PrivateCDAIObjectMPD::NotifyAdLoopWait()
{
}

bool PrivateCDAIObjectMPD::WaitForNextAdResolved(int timeoutMs)
{
	if(g_MockPrivateCDAIObjectMPD != nullptr)
	{
		return g_MockPrivateCDAIObjectMPD->WaitForNextAdResolved(timeoutMs);
	}
	return true;
}

void PrivateCDAIObjectMPD::AbortWaitForNextAdResolved()
{
}


bool PrivateCDAIObjectMPD::WaitForNextAdResolved(int timeoutMs, std::string periodId)
{
	if(g_MockPrivateCDAIObjectMPD != nullptr)
	{
		return g_MockPrivateCDAIObjectMPD->WaitForNextAdResolved(timeoutMs, periodId);
	}
	return true;
}

PlacementObj PrivateCDAIObjectMPD::UpdatePlacementObj(const std::string adBrkId, const std::string endPeriodId)
{
	PlacementObj obj;
	return obj;
}

void PrivateCDAIObjectMPD::ValidateAdManifest(AampMPDParseHelper& adMPDParseHelper, AAMPCDAIError &adErrorCode)
{
}

void PrivateCDAIObjectMPD::InsertToPlacementQueue(const std::string& periodId)
{
}