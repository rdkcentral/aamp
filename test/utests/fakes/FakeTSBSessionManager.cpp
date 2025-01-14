/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2024 RDK Management
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
 * @file AampTSBSessionManager.cpp
 * @brief TSBSession Manager for Aamp
 **************************************/


#include "AampTSBSessionManager.h"
#include "AampLogManager.h"
#include "MockTSBSessionManager.h"

MockTSBSessionManager *g_mockTSBSessionManager = nullptr;
/**
 * @fn AampTSBSessionManager Constructor
 *
 * @return None
 */
AampTSBSessionManager::AampTSBSessionManager(PrivateInstanceAAMP* aamp)	:
        mInitialized_(true), mStopThread_(false), mAamp(aamp), mTSBStore(nullptr),
        mActiveTuneType(eTUNETYPE_NEW_NORMAL), mLastVideoPos(AAMP_PAUSE_POSITION_INVALID_POSITION),
		mCulledDuration(0.0), mStoreEndPosition(0.0), mLiveEndPosition(0.0), mTsbMaxDiskStorage(0),
		mTsbMinFreePercentage(0)
{
}

/**
 * @fn AampTSBSessionManager Destructor
 *
 * @return None
 */
AampTSBSessionManager::~AampTSBSessionManager()
{
}
/**
 * @fn AampTSBSessionManager Init function
 *
 * @return None
 */
void AampTSBSessionManager::Init()
{
    if (g_mockTSBSessionManager)
    {
        g_mockTSBSessionManager->Init();
    }
}
/**
 * @fn Write - function to Enqueues data for writing
 *
 * @return None
 */
void AampTSBSessionManager::EnqueueWrite(std::string url,std::shared_ptr<CachedFragment> cachedFragment, std::string periodId)
{
}
/**
 * @fn Flush  - function to clear the TSB storage
 *
 * @return None
 */	
void AampTSBSessionManager::Flush()
{
}

AAMPStatusType AampTSBSessionManager::InvokeTsbReaders(double &startPosSec, float rate, TuneType tuneType)
{
    return eAAMPSTATUS_OK;
}

std::shared_ptr<AampTsbReader> AampTSBSessionManager::GetTsbReader(AampMediaType)
{
    return nullptr;
}

void AampTSBSessionManager::ProcessWriteQueue()
{

}

double AampTSBSessionManager::CullSegments()
{
    return 0.0;
}

bool AampTSBSessionManager::PushNextTsbFragment(MediaStreamContext *pMediaStreamContext)
{
    return false;
}

void AampTSBSessionManager::UpdateProgress(double manifestDuration, double manifestCulledSecondsFromStart)
{
}

void AampTSBSessionManager::SkipFragment(std::shared_ptr<AampTsbReader> &reader, TsbFragmentDataPtr& nextFragmentData)
{
}

void AampTSBSessionManager::InitializeTsbReaders()
{
}

BitsPerSecond AampTSBSessionManager::GetVideoBitrate()
{
    return 0;
}
double AampTSBSessionManager::GetManifestEndDelta()
{
    return 0;
}
