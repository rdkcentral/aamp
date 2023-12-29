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

#include <cstdlib>
#include <iostream>
#include <string>
#include <string.h>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "priv_aamp.h"
#include "AampProfiler.h"

#include "MockPrivateInstanceAAMP.h"
#include "main_aamp.h"
#include "AampConfig.h"
#include "MockAampConfig.h"
#include "MockAampGstPlayer.h"
#include "MockStreamAbstractionAAMP.h"
#include "fragmentcollector_mpd.h"


AampLogManager *mLogObj{nullptr};
AampConfig *gpGlobalConfig{nullptr};

class PrivAampTests : public ::testing::Test
{
    public:
    PrivateInstanceAAMP *p_aamp{nullptr};
protected:
	void SetUp() override
	{
		p_aamp =  new PrivateInstanceAAMP();
		mLogObj = new AampLogManager();
	}

	void TearDown() override
	{
        delete p_aamp;
		p_aamp = nullptr;

		delete mLogObj;
		mLogObj = nullptr;
	}
};

class PrivAampPrivTests : public ::testing::Test
{
protected:
    void SetUp() override
    {
    auto aamp = new PrivateInstanceAAMP();
    auto logmanager = new AampLogManager();

    auto config=new AampConfig();

    testp_aamp = new TestablePrivAamp(config);
    }

    void TearDown() override
    {
        delete testp_aamp;
    }
       class TestablePrivAamp : public PrivateInstanceAAMP
    {
public:
    TestablePrivAamp(AampConfig *config):PrivateInstanceAAMP(config)
    {
    }
    bool callIsWideVineKIDWorkaround(const std::string url)
    {
        return IsWideVineKIDWorkaround(url);
    }
    void callExtractServiceZone(std::string url)
    {
        ExtractServiceZone(url);
    }
    void callDeliverAdEvents(bool immediate)
    {
         DeliverAdEvents(true);
    }

    bool callDiscontinuitySeenInAllTracks()
    {
     return DiscontinuitySeenInAllTracks();
    }

    bool callDiscontinuitySeenInAnyTracks()
    {
        return DiscontinuitySeenInAnyTracks();
    }
    bool callHasSidecarData()
    {
        return HasSidecarData(); 
    }
    void callUpdatePTSOffsetFromTune(double value, bool is_set)
    {
    	UpdatePTSOffsetFromTune(value,is_set = false);
    }
	bool CallSetStateBufferingIfRequired()
	{
		 TestablePrivAamp::mPauseOnFirstVideoFrameDisp = false;
		 TestablePrivAamp::mFragmentCachingRequired = true;
		IsFragmentCachingRequired();
		return SetStateBufferingIfRequired();
	}
	void CallNotifyFirstBufferProcessed()
	{
		bool mFirstVideoFrameDisplayedEnabled = false;
		SetState(eSTATE_SEEKING);
		NotifyFirstBufferProcessed();
	}
	void CallNotifyFirstVideoFrameDisplayed()
	{
		TestablePrivAamp::mPauseOnFirstVideoFrameDisp = true;
		TuneHelper(eTUNETYPE_SEEKTOLIVE,true);
		SetState(eSTATE_PAUSED);
		NotifyFirstVideoFrameDisplayed();
	}
	void CallNotifyFirstVideoFrameDisplayed_1()
	{
		TestablePrivAamp::mPauseOnFirstVideoFrameDisp = true;
		TuneHelper(eTUNETYPE_SEEKTOLIVE,true);
		SetState(eSTATE_SEEKING);
		NotifyFirstVideoFrameDisplayed();
	}
	void CallGetContentTypString()
	{
		for (int i = ContentType_UNKNOWN; i < ContentType_MAX; i++)
		{
    		TestablePrivAamp::mContentType = static_cast<ContentType>(i);
			GetContentTypString();
		}
	}
	void CallProcessPendingDiscontinuity()
	{
		TestablePrivAamp::mDiscontinuityTuneOperationInProgress = true;
		bool result =  DiscontinuitySeenInAllTracks();
		ProcessPendingDiscontinuity();
	}
	void CallDiscontinuitySeenInAllTracks()
	{
		TestablePrivAamp::mDiscontinuityTuneOperationInProgress = false;
		TestablePrivAamp::mVideoFormat = FORMAT_MPEGTS;
		bool result = DiscontinuitySeenInAllTracks();
		ProcessPendingDiscontinuity();
	}
	void CallDiscontinuitySeenInAllTracks_1()
	{
		AampLogManager *mLogObj;
		double playlistSeekPos = seek_pos_seconds - culledSeconds;
		mpStreamAbstractionAAMP = new StreamAbstractionAAMP_MPD(TestablePrivAamp::mLogObj,this, playlistSeekPos, TestablePrivAamp::rate);
		TestablePrivAamp::mDiscontinuityTuneOperationInProgress = false;
		bool result = DiscontinuitySeenInAllTracks();
		ProcessPendingDiscontinuity();
	}
	void CallGetPlaybackStats()
	{
		TestablePrivAamp::mTimeAtTopProfile = 10;
		SendVideoEndEvent();
		std::string str = GetPlaybackStats();
	}
	void CallEnableContentRestrictions()
	{
		AampLogManager *mLogObj;
		double playlistSeekPos = seek_pos_seconds - culledSeconds;
		mpStreamAbstractionAAMP = new StreamAbstractionAAMP_MPD(TestablePrivAamp::mLogObj,this, playlistSeekPos, TestablePrivAamp::rate);
		EnableContentRestrictions();
	}
	void GetCurrentAudioTrackId_2()
    {
        double playlistSeekPos = seek_pos_seconds - culledSeconds;
        mpStreamAbstractionAAMP = new StreamAbstractionAAMP_MPD(TestablePrivAamp::mLogObj,this, playlistSeekPos, TestablePrivAamp::rate);
        GetCurrentAudioTrackId();
    }
};
    TestablePrivAamp *testp_aamp{nullptr};
};
TEST_F(PrivAampPrivTests, GetCurrentAudioTrackId_2)
{
    testp_aamp->GetCurrentAudioTrackId_2();
}
TEST_F(PrivAampPrivTests,GetMediaStreamContextTest_1)
{
	testp_aamp->CallEnableContentRestrictions();
}
TEST_F(PrivAampPrivTests,GetPeriodDurationTimeValueTest_1)
{
	testp_aamp->CallGetPlaybackStats();
}
TEST_F(PrivAampPrivTests,CallDiscontinuitySeenInAllTracks_1Test)
{
	testp_aamp->CallDiscontinuitySeenInAllTracks_1();
}
TEST_F(PrivAampPrivTests,CallDiscontinuitySeenInAllTracksTest12)
{
	testp_aamp->CallDiscontinuitySeenInAllTracks();
}
TEST_F(PrivAampPrivTests,ProcessPendingDiscontinuityTest11)
{
	 testp_aamp->CallProcessPendingDiscontinuity();
}
TEST_F(PrivAampPrivTests,GetContentTypStringTest)
{
	testp_aamp->CallGetContentTypString();
}
TEST_F(PrivAampPrivTests,IsWideVineKIDWorkaroundTest)
{
	bool enable;
	std::string url = "sampleString";
        enable = testp_aamp->callIsWideVineKIDWorkaround(url);
	EXPECT_FALSE(enable);
}

TEST_F(PrivAampPrivTests,ExtractServiceZoneTest)
{
	std::string url = "sampleString";
	testp_aamp->callExtractServiceZone(url);
	EXPECT_FALSE(testp_aamp->mTSBEnabled);
}

TEST_F(PrivAampPrivTests,ExtractServiceZoneTest_1)
{
	testp_aamp->mTSBEnabled = true;
	std::string url = "sampleString";
	testp_aamp->callExtractServiceZone(url);
	EXPECT_TRUE(testp_aamp->mTSBEnabled);
}

TEST_F(PrivAampPrivTests,ExtractServiceZoneTest_2)
{
	testp_aamp->mIsVSS = true;
	testp_aamp->mTSBEnabled = true;
	std::string url = "sampleString";
	testp_aamp->callExtractServiceZone(url);
	EXPECT_TRUE(testp_aamp->mTSBEnabled);
}

TEST_F(PrivAampPrivTests,DeliverAdEventsTest)
{
	testp_aamp->callDeliverAdEvents(true);
}

TEST_F(PrivAampPrivTests,DeliverAdEventsTest_1)
{
	testp_aamp->callDeliverAdEvents(false);
}

TEST_F(PrivAampPrivTests,DeliverAdEventsTest_2)
{
	AAMPEventObject event(AAMP_EVENT_AD_PLACEMENT_START);	
    testp_aamp->callDeliverAdEvents(true);
}

TEST_F(PrivAampPrivTests,DeliverAdEventsTest_3)
{
	AAMPEventObject event(AAMP_EVENT_AD_PLACEMENT_END);	
	testp_aamp->callDeliverAdEvents(true);
}

TEST_F(PrivAampPrivTests,DiscontinuitySeenInAllTracksTest1)
{
	//for true condition
	bool result = testp_aamp->callDiscontinuitySeenInAllTracks();

	EXPECT_TRUE(result);
}
TEST_F(PrivAampPrivTests,DiscontinuitySeenInAllTracksTest2)
{
	//for false condition
	testp_aamp->mVideoFormat = FORMAT_MPEGTS;
	bool result = testp_aamp->callDiscontinuitySeenInAllTracks();

	EXPECT_FALSE(result);
}

TEST_F(PrivAampPrivTests,DiscontinuitySeenInAnyTracksTest)
{
	testp_aamp->callDiscontinuitySeenInAnyTracks();
}

TEST_F(PrivAampPrivTests,HasSidecarDataTest)
{
    bool flag = testp_aamp->callHasSidecarData();
    EXPECT_FALSE(flag);
}

TEST_F(PrivAampPrivTests,UpdatePTSOffsetFromTuneTest)
{
    double value = 19.0988;
    bool is_set = true;
    testp_aamp->callUpdatePTSOffsetFromTune(value,is_set);
}

TEST_F(PrivAampPrivTests,UpdatePTSOffsetFromTuneTest_1)
{
    double value = 19.0988;
    bool is_set = false;
    testp_aamp->callUpdatePTSOffsetFromTune(value,is_set);
}


TEST_F(PrivAampTests, HandleSSLWriteCallbackTest)
{
	size_t val = p_aamp->HandleSSLWriteCallback(NULL,10,20,NULL);
	EXPECT_EQ(val,0);

	size_t val1 = p_aamp->HandleSSLWriteCallback(NULL,1000,2000,NULL);
	EXPECT_EQ(val,0);
}

TEST_F(PrivAampTests, RunPausePositionMonitoringTest)
{
	p_aamp->RunPausePositionMonitoring();

	EXPECT_NE(p_aamp->rate,1);
	EXPECT_FALSE(p_aamp->pipeline_paused);
}

TEST_F(PrivAampTests, StartPausePositionMonitoringTest1)
{
	p_aamp->StartPausePositionMonitoring(12347865293656786);

	p_aamp->StartPausePositionMonitoring(-10);

	p_aamp->StartPausePositionMonitoring(0);

	p_aamp->StartPausePositionMonitoring(1000);
	p_aamp->StartPausePositionMonitoring(100.0 * 1000);

	p_aamp->StartPausePositionMonitoring(200.0 * 1000);
	p_aamp->StartPausePositionMonitoring(-100.0 * 1000);

	p_aamp->StartPausePositionMonitoring(LLONG_MIN);
	p_aamp->StartPausePositionMonitoring(LLONG_MAX);
}

TEST_F(PrivAampTests, StopPausePositionMonitoringTest)
{
	p_aamp->StopPausePositionMonitoring("sampleString");
	EXPECT_EQ(p_aamp->mPausePositionMilliseconds,-1);
}

TEST_F(PrivAampTests,WaitForDiscontinuityProcessToCompleteTest)
{
	p_aamp->WaitForDiscontinuityProcessToComplete();
	p_aamp->UnblockWaitForDiscontinuityProcessToComplete();
}

TEST_F(PrivAampTests,CompleteDiscontinutyDataDeliverForPTSRestamp)
{
	p_aamp->CompleteDiscontinutyDataDeliverForPTSRestamp(eMEDIATYPE_VIDEO);
	p_aamp->CompleteDiscontinutyDataDeliverForPTSRestamp(eMEDIATYPE_AUDIO);
}

TEST_F(PrivAampTests,SetIsPeriodChangeMarkedTest)
{
	p_aamp->SetIsPeriodChangeMarked(true);
	bool flag = p_aamp->GetIsPeriodChangeMarked();
	p_aamp->SetIsPeriodChangeMarked(false);
	EXPECT_FALSE(p_aamp->GetIsPeriodChangeMarked());
}

TEST_F(PrivAampTests,SyncBeginTest)
{
	p_aamp->SyncBegin();
	p_aamp->SyncEnd();
}
TEST_F(PrivAampTests,GetVideoPTSTest)
{
	long long videoPTS = p_aamp->GetVideoPTS(TRUE);
	long long videoPTS1 = p_aamp->GetVideoPTS(FALSE);

	EXPECT_EQ(videoPTS,-1);
	EXPECT_EQ(videoPTS1,-1);
}

TEST_F(PrivAampTests,WakeupLatencyCheckTest)
{
	std::cout<<"@#$$$$#%^&%^^^*#$@"<<std::endl;
	p_aamp->WakeupLatencyCheck();
	p_aamp->TimedWaitForLatencyCheck(0);
	p_aamp->TimedWaitForLatencyCheck(10);
	p_aamp->TimedWaitForLatencyCheck(100);
	p_aamp->TimedWaitForLatencyCheck(500);


	p_aamp->TimedWaitForLatencyCheck(-10);
	EXPECT_FALSE(p_aamp->mAbortRateCorrection);

	p_aamp->TimedWaitForLatencyCheck(-12355);
	p_aamp->TimedWaitForLatencyCheck(1235);
	p_aamp->TimedWaitForLatencyCheck(12355);
	EXPECT_FALSE(p_aamp->mAbortRateCorrection);

	p_aamp->TimedWaitForLatencyCheck(-10);
	EXPECT_FALSE(p_aamp->mAbortRateCorrection);
}

TEST_F(PrivAampTests,StopRateCorrectionWokerthreadTest)
{
	p_aamp->StartRateCorrectionWokerthread();
	EXPECT_FALSE(p_aamp->mAbortRateCorrection);

	p_aamp->StopRateCorrectionWokerthread();
	EXPECT_FALSE(p_aamp->mAbortRateCorrection);
}

TEST_F(PrivAampTests,RateCorrectionWokerthreadTest1)
{
	p_aamp->RateCorrectionWokerthread();

	EXPECT_NE(p_aamp->mCorrectionRate,0);
	EXPECT_FALSE(p_aamp->mDisableRateCorrection);
}

TEST_F(PrivAampTests,ReportProgressTest1)
{
	//checking different boolean values
	p_aamp->ReportProgress(TRUE,TRUE);

	p_aamp->ReportAdProgress(TRUE);
}
TEST_F(PrivAampTests,ReportProgressTest2)
{
	//checking different boolean values
	p_aamp->ReportProgress(TRUE,FALSE);

	p_aamp->ReportAdProgress(FALSE);
}
TEST_F(PrivAampTests,ReportProgressTest3)
{
	//checking different boolean values
	p_aamp->ReportProgress(FALSE,TRUE);

	p_aamp->ReportAdProgress(TRUE);
}
TEST_F(PrivAampTests,ReportProgressTest4)
{
	//checking different boolean values
	p_aamp->ReportProgress(FALSE,FALSE);

	p_aamp->ReportAdProgress(FALSE);
}
TEST_F(PrivAampTests,ReportProgressTest5)
{
	bool sync = true; 
	bool beginningOfStream = true;
	p_aamp->SetState(eSTATE_SEEKING);
	p_aamp->ReportProgress(sync,beginningOfStream);
}
TEST_F(PrivAampTests,ReportProgressTest6)
{
	bool sync = true; 
	bool beginningOfStream = true;

	bool mDownloadsEnabled = true;
	p_aamp->SetState(eSTATE_PAUSED);

	p_aamp->ReportAdProgress(sync);

	p_aamp->ReportProgress(sync,beginningOfStream);
}

TEST_F(PrivAampTests,UpdateDurationTest)
{
	p_aamp->UpdateDuration(232.436);

	p_aamp->UpdateDuration(0);
	p_aamp->UpdateDuration(232436.232436);
	p_aamp->UpdateCullingState(232436.232436);

	p_aamp->UpdateDuration(232.1234567891234567890);
	p_aamp->UpdateDuration(232.12345678912345678901323254);
	p_aamp->UpdateDuration(232.12345678901234567890123456789);
	EXPECT_EQ(p_aamp->durationSeconds,232.12345678901234567890123456789);

	p_aamp->UpdateDuration(-10.00);
	EXPECT_EQ(p_aamp->durationSeconds,-10.00);
}

TEST_F(PrivAampTests,UpdateCullingStateTest)
{
	p_aamp->UpdateCullingState(0);
	p_aamp->UpdateCullingState(232.1234567891234567890);
	p_aamp->UpdateCullingState(232.123456789123456789035345);
	p_aamp->UpdateCullingState(232.12345678901234567890123456789);

	EXPECT_NE(p_aamp->culledSeconds,232.12345678901234567890123456789);
	p_aamp->UpdateCullingState(-10.00);
	EXPECT_NE(p_aamp->culledSeconds,-10.00);

	p_aamp->pipeline_paused=true;
	p_aamp->rate=4;
	p_aamp->mPausedBehavior = ePAUSED_BEHAVIOR_LIVE_IMMEDIATE;
	EXPECT_FALSE(p_aamp->mSeekFromPausedState);

	p_aamp->mAutoResumeTaskPending = false;
	EXPECT_FALSE(p_aamp->mAutoResumeTaskPending);
}

TEST_F(PrivAampTests,AddEventListenerTest)
{
	EventListener* eventListener;
	p_aamp->AddEventListener(AAMP_EVENT_CONTENT_PROTECTION_DATA_UPDATE,eventListener);
	p_aamp->RemoveEventListener(AAMP_EVENT_CONTENT_PROTECTION_DATA_UPDATE,eventListener);

	EXPECT_FALSE(p_aamp->IsEventListenerAvailable(AAMP_EVENT_CONTENT_PROTECTION_DATA_UPDATE));


	AAMPEventType eventType=AAMP_EVENT_HTTP_RESPONSE_HEADER;
	bool flag = p_aamp->IsEventListenerAvailable(eventType);
	EXPECT_FALSE(flag);


	flag = p_aamp->IsEventListenerAvailable(AAMP_EVENT_TUNED);
	EXPECT_FALSE(flag);

	flag = p_aamp->IsEventListenerAvailable(AAMP_EVENT_REPORT_ANOMALY);
	EXPECT_FALSE(flag);

	flag = p_aamp->IsEventListenerAvailable(AAMP_EVENT_BITRATE_CHANGED);
	EXPECT_FALSE(flag);

	flag = p_aamp->IsEventListenerAvailable(AAMP_MAX_NUM_EVENTS);
	EXPECT_FALSE(flag);
	
	flag = p_aamp->IsEventListenerAvailable((AAMPEventType(39)));
	EXPECT_FALSE(flag);

	flag = p_aamp->IsEventListenerAvailable((AAMPEventType(45)));
	EXPECT_FALSE(flag);

	flag = p_aamp->IsEventListenerAvailable((AAMPEventType(100)));
	EXPECT_FALSE(flag);

	flag = p_aamp->IsEventListenerAvailable((AAMPEventType(-1)));
	EXPECT_FALSE(flag);
}

TEST_F(PrivAampTests,SendDrmErrorEventTest)
{
	DrmMetaDataEventPtr event = std::make_shared<DrmMetaDataEvent>(AAMP_TUNE_FAILURE_UNKNOWN, "", 0, 0, true);
	p_aamp->SendDrmErrorEvent(event,true);
	p_aamp->SendDrmErrorEvent(event,false);
}

TEST_F(PrivAampTests,SendDrmErrorEventTest_1)
{
	DrmMetaDataEventPtr event = std::make_shared<DrmMetaDataEvent>(AAMP_TUNE_FAILURE_UNKNOWN, "", 0, 0, true);
	p_aamp->SendDrmErrorEvent(event,true);
}

TEST_F(PrivAampTests,SendDrmErrorEventTest_2)
{
	DrmMetaDataEventPtr event = std::make_shared<DrmMetaDataEvent>(AAMP_TUNE_FAILURE_UNKNOWN, "", 0, 0, true);
	p_aamp->SendDrmErrorEvent(event,true);
}

TEST_F(PrivAampTests,SendDownloadErrorEventTest1)
{
	p_aamp->SendDownloadErrorEvent(AAMP_TUNE_FAILED_PTS_ERROR,130);
	p_aamp->SendDownloadErrorEvent(AAMP_TUNE_FAILED_PTS_ERROR,133);
	
	p_aamp->SendDownloadErrorEvent((AAMPTuneFailure)38,130);
	p_aamp->SendDownloadErrorEvent(AAMP_TUNE_FAILED_PTS_ERROR,100);

	p_aamp->SendDownloadErrorEvent(AAMP_TUNE_FAILED_PTS_ERROR,404);
	p_aamp->SendDownloadErrorEvent(AAMP_TUNE_FAILED_PTS_ERROR,421);
}


TEST_F(PrivAampTests,SendAnomalyEventTest)
{
	p_aamp->SendAnomalyEvent(ANOMALY_ERROR,"error event");

	p_aamp->IsEventListenerAvailable(AAMP_EVENT_REPORT_ANOMALY);
	p_aamp->SendAnomalyEvent(ANOMALY_ERROR,"error event");
	
	p_aamp->IsEventListenerAvailable(AAMP_MAX_NUM_EVENTS);

	p_aamp->IsEventListenerAvailable(AAMP_EVENT_REPORT_ANOMALY);
	p_aamp->SendAnomalyEvent(ANOMALY_WARNING,"error event");

	p_aamp->IsEventListenerAvailable(AAMP_EVENT_REPORT_ANOMALY);
	p_aamp->SendAnomalyEvent(ANOMALY_TRACE,"error event");
}

TEST_F(PrivAampTests,UpdateRefreshPlaylistIntervalTest)
{
	p_aamp->UpdateRefreshPlaylistInterval(12.43265);

	p_aamp->UpdateRefreshPlaylistInterval(12.12f);
	p_aamp->UpdateRefreshPlaylistInterval(12.123456789999);
	p_aamp->UpdateRefreshPlaylistInterval(12.123456789123456789);
	p_aamp->UpdateRefreshPlaylistInterval(0);
	
	p_aamp->UpdateRefreshPlaylistInterval(-12.43265);
}

TEST_F(PrivAampTests,SendBufferChangeEventTest)
{
	p_aamp->SendBufferChangeEvent(true);
}

TEST_F(PrivAampTests,SendBufferChangeEventTest_1)
{
	p_aamp->SendBufferChangeEvent(false);
}

TEST_F(PrivAampTests,PausePipelineTest)
{
	EXPECT_TRUE(p_aamp->PausePipeline(true,true));
	EXPECT_TRUE(p_aamp->PausePipeline(true,false));
	EXPECT_TRUE(p_aamp->PausePipeline(false,true));
	EXPECT_TRUE(p_aamp->PausePipeline(false,false));

	EXPECT_FALSE(p_aamp->pipeline_paused);
}

TEST_F(PrivAampTests,SendErrorEventTest)
{
	p_aamp->SendErrorEvent(AAMP_TUNE_FAILURE_UNKNOWN,"DESCRIPTION",true,11,12,13,"responseString");

	EXPECT_EQ(p_aamp->rate,0);

	p_aamp->SendErrorEvent(AAMP_TUNE_PLAYBACK_STALLED,"DESCRIPTION",true,11,12,13,"responseString");
	p_aamp->SendErrorEvent(((AAMPTuneFailure)(-1)),"DESCRIPTION",true,11,12,13,"responseString");
	p_aamp->SendErrorEvent(((AAMPTuneFailure)(50)),"DESCRIPTION",true,11,12,13,"responseString");
	p_aamp->rate=4;
	p_aamp->SendErrorEvent(((AAMPTuneFailure)(0)),"DESCRIPTION",true,11,12,13,"responseString");
}

TEST_F(PrivAampTests,SendErrorEventTest_1)
{
	p_aamp->SetState(eSTATE_PREPARED);
	p_aamp->ReloadTSB();

	p_aamp->SetState(eSTATE_PREPARED);
	
	p_aamp->SendErrorEvent(AAMP_TUNE_PLAYBACK_STALLED, "UNKNOWNString");
	p_aamp->SendErrorEvent(AAMP_TUNE_FAILURE_UNKNOWN);

	p_aamp->SendErrorEvent(AAMP_TUNE_PLAYBACK_STALLED,NULL,true,-1,-1,-1,"UNKNOWNString");
}


TEST_F(PrivAampTests,LicenseRenewalTest)
{
    std::shared_ptr<AampDrmHelper> drmHelper;
    p_aamp->LicenseRenewal(drmHelper,NULL);
}

TEST_F(PrivAampTests,SendEventTest)
{
	AAMPEventPtr eventData;
	p_aamp->SendEvent(eventData,AAMP_EVENT_DEFAULT_MODE);
}

TEST_F(PrivAampTests,SendEventTest_1)
{
	AAMPEventPtr eventData;
	eventData = std::make_shared<AAMPEventObject>(AAMP_EVENT_TUNED);
	p_aamp->SendEvent(eventData,AAMP_EVENT_SYNC_MODE);

	eventData = std::make_shared<AAMPEventObject>(AAMP_EVENT_TUNED);
	p_aamp->SendEvent(eventData,AAMP_EVENT_ASYNC_MODE);

	eventData = std::make_shared<AAMPEventObject>(AAMP_MAX_NUM_EVENTS);
	p_aamp->SendEvent(eventData,AAMP_EVENT_SYNC_MODE);

	eventData = std::make_shared<AAMPEventObject>(AAMP_MAX_NUM_EVENTS);
	p_aamp->SendEvent(eventData,AAMP_EVENT_ASYNC_MODE);
}

TEST_F(PrivAampTests,NotifyBitRateChangeEventTest)
{
	p_aamp->IsEventListenerAvailable(AAMP_EVENT_BITRATE_CHANGED);
	p_aamp->NotifyBitRateChangeEvent(2,eAAMP_BITRATE_CHANGE_BY_ABR,10,12,10.00,15.00,false,eVIDEOSCAN_INTERLACED,30,15);
}

TEST_F(PrivAampTests,NotifyBitRateChangeEventTest_1)
{
	p_aamp->IsEventListenerAvailable(AAMP_EVENT_BITRATE_CHANGED);
	p_aamp->NotifyBitRateChangeEvent(4,eAAMP_BITRATE_CHANGE_BY_ABR,10,12,10.00,15.00,false,eVIDEOSCAN_PROGRESSIVE,30,15);
}

TEST_F(PrivAampTests,NotifyBitRateChangeEventTest_2)
{
	p_aamp->IsEventListenerAvailable(AAMP_EVENT_BITRATE_CHANGED);
	p_aamp->NotifyBitRateChangeEvent(2,eAAMP_BITRATE_CHANGE_BY_ABR,10,12,10.00,15.00,false,eVIDEOSCAN_UNKNOWN,30,15);
}

TEST_F(PrivAampTests,NotifyBitRateChangeEventTest_3)
{
	p_aamp->IsEventListenerAvailable(AAMP_EVENT_BITRATE_CHANGED);
	p_aamp->NotifyBitRateChangeEvent(0,eAAMP_BITRATE_CHANGE_BY_HDMIIN,10,12,10.00,0.00,false,eVIDEOSCAN_UNKNOWN,0,0);
}

TEST_F(PrivAampTests,NotifyBitRateChangeEventTest_4)
{
	p_aamp->IsEventListenerAvailable(AAMP_EVENT_BITRATE_CHANGED);
	p_aamp->NotifyBitRateChangeEvent(10,eAAMP_BITRATE_CHANGE_BY_HDMIIN,10,12,10.00,0.00,false,eVIDEOSCAN_UNKNOWN,100,50);
}

TEST_F(PrivAampTests,NotifySpeedChangedTest)
{
	p_aamp->NotifySpeedChanged(10.243,true);
	p_aamp->NotifySpeedChanged(10.243,false);
}

TEST_F(PrivAampTests,NotifySpeedChangedTest_1)
{
	p_aamp->NotifySpeedChanged(0,true);
	p_aamp->NotifySpeedChanged(1,true);
	p_aamp->NotifySpeedChanged(8,true);
	p_aamp->NotifySpeedChanged(64,true);
}

TEST_F(PrivAampTests,NotifySpeedChangedTest_2)
{
	p_aamp->NotifySpeedChanged(0,false);
	p_aamp->NotifySpeedChanged(1,false);
	p_aamp->NotifySpeedChanged(8,false);
	p_aamp->NotifySpeedChanged(64,false);
}

TEST_F(PrivAampTests,NotifySpeedChangedTest_3)
{
	p_aamp->NotifySpeedChanged(0.5,false);
	p_aamp->NotifySpeedChanged(-1,false);
	p_aamp->NotifySpeedChanged(-1.5,false);

	p_aamp->NotifySpeedChanged(-1,true);
}

TEST_F(PrivAampTests,SendDRMMetaDataTest)
{
	DrmMetaDataEventPtr drm = std::make_shared<DrmMetaDataEvent>(AAMP_TUNE_FAILURE_UNKNOWN, "", 0, 0, true);
	p_aamp->SendDRMMetaData(drm);
}

TEST_F(PrivAampTests,SendDRMMetaDataTest_1)
{
	DrmMetaDataEventPtr drm = std::make_shared<DrmMetaDataEvent>(AAMP_TUNE_INIT_FAILED, "", 0, 0, true);
	p_aamp->SendDRMMetaData(drm);
}
TEST_F(PrivAampTests,SendDRMMetaDataTest_2)
{
	DrmMetaDataEventPtr drm = std::make_shared<DrmMetaDataEvent>(AAMP_TUNE_FAILURE_UNKNOWN, "", 0, 0, false);
	p_aamp->SendDRMMetaData(drm);

	drm = std::make_shared<DrmMetaDataEvent>(AAMP_TUNE_LICENCE_REQUEST_FAILED, "", 0, 0, false);
	p_aamp->SendDRMMetaData(drm);
}

TEST_F(PrivAampTests,SendDrmErrorEventTest_3)
{
DrmMetaDataEventPtr event = std::make_shared<DrmMetaDataEvent>(AAMP_TUNE_FAILURE_UNKNOWN, "", 0, 0, true);
p_aamp->SendDrmErrorEvent(event,true);

event = std::make_shared<DrmMetaDataEvent>(AAMP_TUNE_FAILURE_UNKNOWN, "", 0, 0, false);
p_aamp->SendDrmErrorEvent(event,true);
}

TEST_F(PrivAampTests,IsDiscontinuityProcessPendingTest)
{
	EXPECT_FALSE(p_aamp->IsDiscontinuityProcessPending());
}

TEST_F(PrivAampTests,IsDiscontinuityProcessPendingTest_1)
{
	p_aamp->SetStreamFormat(FORMAT_INVALID,FORMAT_INVALID,FORMAT_INVALID);
	EXPECT_FALSE(p_aamp->IsDiscontinuityProcessPending());

	p_aamp->SetStreamFormat(FORMAT_UNKNOWN,FORMAT_UNKNOWN,FORMAT_UNKNOWN);
	EXPECT_FALSE(p_aamp->IsDiscontinuityProcessPending());

	p_aamp->SetStreamFormat(FORMAT_UNKNOWN,FORMAT_INVALID,FORMAT_INVALID);
	EXPECT_FALSE(p_aamp->IsDiscontinuityProcessPending());

	p_aamp->SetStreamFormat(FORMAT_INVALID,FORMAT_UNKNOWN,FORMAT_INVALID);
	EXPECT_FALSE(p_aamp->IsDiscontinuityProcessPending());

	p_aamp->SetStreamFormat(FORMAT_INVALID,FORMAT_UNKNOWN,FORMAT_UNKNOWN);
	EXPECT_FALSE(p_aamp->IsDiscontinuityProcessPending());
}

TEST_F(PrivAampTests,IsDiscontinuityProcessPendingTest_2)
{
	p_aamp->SetStreamFormat(FORMAT_VIDEO_ES_H264,FORMAT_AUDIO_ES_AC3,FORMAT_UNKNOWN);
	EXPECT_FALSE(p_aamp->IsDiscontinuityProcessPending());

	p_aamp->SetStreamFormat(FORMAT_VIDEO_ES_HEVC,FORMAT_AUDIO_ES_ATMOS,FORMAT_INVALID);
	EXPECT_FALSE(p_aamp->IsDiscontinuityProcessPending());

	p_aamp->SetStreamFormat(FORMAT_VIDEO_ES_MPEG2,FORMAT_AUDIO_ES_AAC,FORMAT_INVALID);
	EXPECT_FALSE(p_aamp->IsDiscontinuityProcessPending());
}

TEST_F(PrivAampTests,getLastInjectedPositionTest)
{
	double val = p_aamp->getLastInjectedPosition();
}

TEST_F(PrivAampTests,NotifyEOSReachedTest)
{
	p_aamp->rate=0;
	p_aamp->NotifyEOSReached();

	p_aamp->rate=8;
	p_aamp->NotifyEOSReached();

	p_aamp->IsDiscontinuityProcessPending();
	p_aamp->NotifyEOSReached();
}

TEST_F(PrivAampTests,NotifyEOSReachedTest_1)
{

	p_aamp->rate=-1;
	p_aamp->NotifyEOSReached();

	p_aamp->rate=1;
	p_aamp->NotifyEOSReached();
}

TEST_F(PrivAampTests,NotifyOnEnteringLiveTest1)
{
	bool flag = p_aamp->discardEnteringLiveEvt;
	p_aamp->NotifyOnEnteringLive();

	EXPECT_FALSE(flag);
}
TEST_F(PrivAampTests,NotifyOnEnteringLiveTest2)
{
	bool flag = p_aamp->discardEnteringLiveEvt = true;
	p_aamp->NotifyOnEnteringLive();
	EXPECT_TRUE(flag);
}

TEST_F(PrivAampTests,AdditionalTuneFailLogEntriesTest)
{
	p_aamp->AdditionalTuneFailLogEntries();
	bool flag = p_aamp->mbDownloadsBlocked;
	EXPECT_FALSE(flag);
}

TEST_F(PrivAampTests,AdditionalTuneFailLogEntriesTest_1)
{
	p_aamp->TuneHelper(eTUNETYPE_SEEKTOLIVE,true);
	p_aamp->AdditionalTuneFailLogEntries();
	bool flag = p_aamp->mbDownloadsBlocked;
	EXPECT_FALSE(flag);
}

TEST_F(PrivAampTests,TuneFailTest)
{
	p_aamp->TuneFail(true);
	p_aamp->TuneFail(false);
}

TEST_F(PrivAampTests,LogTuneCompleteTest)
{
	p_aamp->LogTuneComplete();
	p_aamp->LogFirstFrame();
}

TEST_F(PrivAampTests,LogTuneCompleteTest_1)
{
	p_aamp->SetContentType("LINEAR_TV");
	p_aamp->LogTuneComplete();
}

TEST_F(PrivAampTests,ResetProfileCacheTest)
{
	ProfileEventAAMP profiler;
	profiler.ProfileReset(PROFILE_BUCKET_INIT_VIDEO);
	profiler.ProfileReset(PROFILE_BUCKET_FRAGMENT_AUXILIARY);
	p_aamp->ResetProfileCache();
}

TEST_F(PrivAampTests,LogPlayerTest)
{
	p_aamp->LogPlayerPreBuffered();
	p_aamp->LogDrmInitComplete();

	ProfilerBucketType bucketType;
	p_aamp->LogDrmDecryptBegin(bucketType);
	p_aamp->LogDrmDecryptEnd(bucketType);
}

TEST_F(PrivAampTests,ActivatePlayerTest)
{
	p_aamp->ActivatePlayer();
}

TEST_F(PrivAampTests,StopDownloadsTest)
{
	p_aamp->StopDownloads();
	EXPECT_TRUE(p_aamp->mbDownloadsBlocked);
}

TEST_F(PrivAampTests,ResumeDownloadsTest)
{	
	p_aamp->ResumeDownloads();
	EXPECT_FALSE(p_aamp->mbDownloadsBlocked);
}

TEST_F(PrivAampTests,ResumeDownloadsTest_1)
{	
	EXPECT_FALSE(p_aamp->mbDownloadsBlocked);

	p_aamp->StopDownloads();
	EXPECT_TRUE(p_aamp->mbDownloadsBlocked);

	p_aamp->ResumeDownloads();
	EXPECT_FALSE(p_aamp->mbDownloadsBlocked);
}

TEST_F(PrivAampTests,TrackDownloadsTest)
{
	p_aamp->StopTrackDownloads(eMEDIATYPE_VIDEO);
	p_aamp->ResumeTrackDownloads(eMEDIATYPE_VIDEO);
}

TEST_F(PrivAampTests,BlockUntilGstreamerWantsDataTest)
{
	p_aamp->BlockUntilGstreamerWantsData(NULL,10,20);
	p_aamp->BlockUntilGstreamerWantsData(NULL,0,0);

	EXPECT_TRUE(p_aamp->mDownloadsEnabled);
	EXPECT_FALSE(p_aamp->mbDownloadsBlocked);
}

TEST_F(PrivAampTests,BlockUntilGstreamerWantsDataTest_1)
{
	p_aamp->BlockUntilGstreamerWantsData(NULL,-110,-220);
	p_aamp->BlockUntilGstreamerWantsData(NULL,0,0);

	EXPECT_FALSE(p_aamp->mbDownloadsBlocked);
}

TEST_F(PrivAampTests,CurlInitTest)
{
	p_aamp->CurlInit(eCURLINSTANCE_PLAYLISTPRECACHE, 1, "ProxyName");
}

TEST_F(PrivAampTests,CurlInitTest_1)
{
	std::string string;
	string = p_aamp->mConfig->GetUserAgentString();
	EXPECT_STRNE("testString",string.c_str());

	p_aamp->CurlInit(eCURLINSTANCE_PLAYLISTPRECACHE, 1, "");
}

TEST_F(PrivAampTests,StoreLanguageListTest)
{
	p_aamp->StoreLanguageList(std::set<std::string>());
	EXPECT_EQ(p_aamp->mMaxLanguageCount,0);
}

TEST_F(PrivAampTests,StoreLanguageListTest_1)
{
	std::set<std::string> list;
	list.insert("English");
	list.insert("spanish");
	list.insert("greece");
	p_aamp->StoreLanguageList(list);
	EXPECT_EQ(p_aamp->mMaxLanguageCount,3);
}

TEST_F(PrivAampTests,StoreLanguageListTest_2)
{
	std::set<std::string> list;
	list.insert("English");
	list.insert("spanish");
	list.insert("greece");

	list.insert("turkish");
	list.insert("aaaa");
	list.insert("bbbbbb");

	list.insert("sssss");
	list.insert("rrrrr");
	list.insert("tttt");

	list.insert("Engtttlish");
	list.insert("qqqqq");
	list.insert("wwwwww");

	list.insert("eeeee");
	list.insert("eeerrrr");
	list.insert("yyyyyyyy");

	list.insert("uuuuuuu");
	list.insert("iiiiiiiiii");
	list.insert("oooooooo");

	list.insert("pppppppp");
	list.insert("aaaaaaaaaaasdsfss");
	list.insert("zxczvbcbv");

	p_aamp->StoreLanguageList(list);
	EXPECT_EQ(p_aamp->mMaxLanguageCount,16);
}

TEST_F(PrivAampTests,IsAudioLanguageSupportedTest)
{
	bool flag = p_aamp->IsAudioLanguageSupported("LanguageName");
	EXPECT_FALSE(flag);
}

TEST_F(PrivAampTests,IsAudioLanguageSupportedTest_1)
{
	bool flag = p_aamp->IsAudioLanguageSupported("English");
	EXPECT_FALSE(flag);

	flag = p_aamp->IsAudioLanguageSupported("Spanish");
	EXPECT_FALSE(flag);
}

TEST_F(PrivAampTests,SetCurlTimeoutTest)
{
	p_aamp->SetCurlTimeout(12234325,eCURLINSTANCE_PLAYLISTPRECACHE);
}

TEST_F(PrivAampTests,SetCurlTimeoutTest_1)
{
	p_aamp->SetContentType("EAS");
	p_aamp->SetCurlTimeout(12234325,eCURLINSTANCE_AUDIO);
}

TEST_F(PrivAampTests,SetCurlTimeoutTest_2)
{
	p_aamp->SetCurlTimeout(12234325,eCURLINSTANCE_MANIFEST_MAIN);

	p_aamp->SetCurlTimeout(12234325,eCURLINSTANCE_MAX);

	p_aamp->SetCurlTimeout(12234325,AampCurlInstance(13));
}

TEST_F(PrivAampTests,CurlTermTest)
{
	p_aamp->CurlTerm(eCURLINSTANCE_AUDIO,10);
}

TEST_F(PrivAampTests,CurlTermTest_1)
{
	p_aamp->CurlTerm(eCURLINSTANCE_AUDIO,6);

	p_aamp->CurlTerm(eCURLINSTANCE_VIDEO,5);
}

TEST_F(PrivAampTests,GetPlaylistCurlInstanceTest)
{
	AampCurlInstance retVar = p_aamp->GetPlaylistCurlInstance(eMEDIATYPE_PLAYLIST_VIDEO,true);
	EXPECT_EQ(4,retVar);
}

TEST_F(PrivAampTests,GetPlaylistCurlInstanceTest_1)
{
	AampCurlInstance retVar = p_aamp->GetPlaylistCurlInstance(eMEDIATYPE_PLAYLIST_VIDEO,false);
	EXPECT_EQ(5,retVar);

	retVar = p_aamp->GetPlaylistCurlInstance(eMEDIATYPE_PLAYLIST_IFRAME,false);
	EXPECT_EQ(5,retVar);
}

TEST_F(PrivAampTests,GetPlaylistCurlInstanceTest_2)
{
	AampCurlInstance retVar = p_aamp->GetPlaylistCurlInstance(eMEDIATYPE_PLAYLIST_AUDIO,false);
	EXPECT_EQ(6,retVar);

	retVar = p_aamp->GetPlaylistCurlInstance(eMEDIATYPE_PLAYLIST_SUBTITLE,false);
	EXPECT_EQ(7,retVar);

	retVar = p_aamp->GetPlaylistCurlInstance(eMEDIATYPE_PLAYLIST_AUX_AUDIO,false);
	EXPECT_EQ(8,retVar);
}

TEST_F(PrivAampTests,ResetCurrentlyAvailableBandwidthTest)
{
	p_aamp->ResetCurrentlyAvailableBandwidth(123564756,true,15);
	EXPECT_EQ(p_aamp->mAbrBitrateData.size(),0);
}

TEST_F(PrivAampTests,ResetCurrentlyAvailableBWTest_1)
{
	long bitsPerSecond = 123564756;
	bool trickPlay = true;
	int profile = 15;
	p_aamp->ResetCurrentlyAvailableBandwidth(bitsPerSecond,trickPlay,profile);
}

TEST_F(PrivAampTests,ResetCurrentlyAvailableBWTest_2)
{
	long bitsPerSecond = 123564756;
	bool trickPlay = true;
	int profile = 15;
	std::vector< std::pair<long long,long> > mAbrBitrateData;
	mAbrBitrateData.push_back(std::make_pair(243475656835,433554345343));

	p_aamp->ResetCurrentlyAvailableBandwidth(bitsPerSecond,trickPlay,profile);
}

TEST_F(PrivAampTests,GetCurrentlyAvailableBandwidthTest)
{
	long val = p_aamp->GetCurrentlyAvailableBandwidth();
	EXPECT_NE(0,val);
}

TEST_F(PrivAampTests,GetCurrentlyAvailableBandwidthTest_1)
{
	std::vector<BitsPerSecond> tmpData;
	tmpData.push_back(13242352);
	tmpData.push_back(13312242352);

	long val = p_aamp->GetCurrentlyAvailableBandwidth();
	EXPECT_NE(0,val);
}

TEST_F(PrivAampTests,MediaTypeStringTest)
{
	const char *var = p_aamp->MediaTypeString(eMEDIATYPE_VIDEO);
	EXPECT_STREQ(var,"VIDEO");

	var = p_aamp->MediaTypeString(eMEDIATYPE_INIT_VIDEO);
	EXPECT_STREQ(var,"VIDEO");
}

TEST_F(PrivAampTests,MediaTypeStringTest_1)
{
	const char *var = p_aamp->MediaTypeString(eMEDIATYPE_INIT_AUDIO);
	EXPECT_STREQ(var,"AUDIO");

	var = p_aamp->MediaTypeString(eMEDIATYPE_AUDIO);
	EXPECT_STREQ(var,"AUDIO");
}

TEST_F(PrivAampTests,MediaTypeStringTest_2)
{
	const char *var = p_aamp->MediaTypeString(eMEDIATYPE_SUBTITLE);
	EXPECT_STREQ(var,"SUBTITLE");

	var = p_aamp->MediaTypeString(eMEDIATYPE_INIT_SUBTITLE);
	EXPECT_STREQ(var,"SUBTITLE");
}

TEST_F(PrivAampTests,MediaTypeStringTest_3)
{
	const char *var = p_aamp->MediaTypeString(eMEDIATYPE_AUX_AUDIO);
	EXPECT_STREQ(var,"AUX-AUDIO");

	var = p_aamp->MediaTypeString(eMEDIATYPE_INIT_AUX_AUDIO);
	EXPECT_STREQ(var,"AUX-AUDIO");
}

TEST_F(PrivAampTests,MediaTypeStringTest_4)
{
	const char *var = p_aamp->MediaTypeString(eMEDIATYPE_MANIFEST);
	EXPECT_STREQ(var,"MANIFEST");

	var = p_aamp->MediaTypeString(eMEDIATYPE_LICENCE);
	EXPECT_STREQ(var,"LICENCE");
}

TEST_F(PrivAampTests,MediaTypeStringTest_5)
{
	const char *var = p_aamp->MediaTypeString(eMEDIATYPE_IFRAME);
	EXPECT_STREQ(var,"IFRAME");

	var = p_aamp->MediaTypeString(eMEDIATYPE_PLAYLIST_VIDEO);
	EXPECT_STREQ(var,"PLAYLIST_VIDEO");
}
TEST_F(PrivAampTests,MediaTypeStringTest_6)
{
	const char *var = p_aamp->MediaTypeString(eMEDIATYPE_PLAYLIST_AUDIO);
	EXPECT_STREQ(var,"PLAYLIST_AUDIO");

	var = p_aamp->MediaTypeString(eMEDIATYPE_PLAYLIST_SUBTITLE);
	EXPECT_STREQ(var,"PLAYLIST_SUBTITLE");

	var = p_aamp->MediaTypeString(eMEDIATYPE_PLAYLIST_AUX_AUDIO);
	EXPECT_STREQ(var,"PLAYLIST_AUX-AUDIO");

	var = p_aamp->MediaTypeString(eMEDIATYPE_DEFAULT);
	EXPECT_STREQ(var,"Unknown");
}


TEST_F(PrivAampTests,GetFileTest)
{
	const char *url;
	std::string effectiveUrl;
	int http_error;
	AampGrowableBuffer gBuff("GrowableBuffer");
	double downloadTime;BitsPerSecond bitrate;
	int fogError;
EXPECT_FALSE(p_aamp->GetFile("remoteurl",&gBuff,effectiveUrl,&http_error,&downloadTime,"0-150",eCURLINSTANCE_MANIFEST_MAIN,false,eMEDIATYPE_VIDEO,
									&bitrate,&fogError,0.0));
}

TEST_F(PrivAampTests,GetFileTest_1)
{
	const char *url;
	std::string effectiveUrl;
	int http_error;
	AampGrowableBuffer gBuff("GrowableBuffer");
	double downloadTime;
	MediaType mType = eMEDIATYPE_VIDEO; 
	BitsPerSecond bitrate;
	int fogError;
EXPECT_FALSE(p_aamp->GetFile("remoteurl",&gBuff,effectiveUrl,&http_error,&downloadTime,"0-150",eCURLINSTANCE_MANIFEST_MAIN,false,mType,
									&bitrate,&fogError,0.0));
}

TEST_F(PrivAampTests,GetFileTest_2)
{
	const char *url;
	std::string effectiveUrl;
	int http_error;
	AampGrowableBuffer gBuff("GrowableBuffer");
	double downloadTime;
	bool resetBuffer = true;
	MediaType mType = eMEDIATYPE_VIDEO; 
	BitsPerSecond bitrate;
	int fogError;
EXPECT_FALSE(p_aamp->GetFile("remoteurl",&gBuff,effectiveUrl,&http_error,&downloadTime,"0-150",eCURLINSTANCE_MANIFEST_MAIN,resetBuffer,mType,
									&bitrate,&fogError,0.0));
}
TEST_F(PrivAampTests,GetFileTest_3)
{
	const char *url;
	std::string effectiveUrl;
	int http_error;
	AampGrowableBuffer gBuff("GrowableBuffer");
	double downloadTime;
	bool resetBuffer = true;
	MediaType mType = eMEDIATYPE_VIDEO; 
	BitsPerSecond bitrate;
	int fogError;

	p_aamp->EnableDownloads();

	EXPECT_FALSE(p_aamp->GetFile("remoteurl",&gBuff,effectiveUrl,&http_error,&downloadTime,"0-150",eCURLINSTANCE_MANIFEST_MAIN,resetBuffer,mType,
									&bitrate,&fogError,0.0));
}

TEST_F(PrivAampTests,GetFileTest_4)
{
	const char *url;
	std::string effectiveUrl;
	int http_error;
	AampGrowableBuffer gBuff("GrowableBuffer");
	double downloadTime;
	bool resetBuffer = true;
	MediaType mType = eMEDIATYPE_INIT_VIDEO; 
	BitsPerSecond bitrate;
	int fogError;

	p_aamp->EnableDownloads();

	EXPECT_FALSE(p_aamp->GetFile("remoteurl",&gBuff,effectiveUrl,&http_error,&downloadTime,"0-150",eCURLINSTANCE_MANIFEST_MAIN,resetBuffer,mType,
									&bitrate,&fogError,0.0));
}

TEST_F(PrivAampTests,GetOnVideoEndSessionStatDataTest)
{
	std::string stringData= "sampleStringdata";
	p_aamp->GetOnVideoEndSessionStatData(stringData);
}

TEST_F(PrivAampTests,TeardownStreamTest)
{
	p_aamp->TeardownStream(true);
	EXPECT_EQ(0,p_aamp->mDiscontinuityTuneOperationId);
}

TEST_F(PrivAampTests,TeardownStreamTest_1)
{
	p_aamp->TeardownStream(false);
	EXPECT_EQ(0,p_aamp->mDiscontinuityTuneOperationId);
}

TEST_F(PrivAampTests,TeardownStreamTest_2)
{
	EXPECT_EQ(0,p_aamp->rate);
	p_aamp->Stop();
	EXPECT_EQ(1,p_aamp->rate);
	bool flag = p_aamp->IsDiscontinuityProcessPending();
	EXPECT_FALSE(flag);
	p_aamp->SetContentType("VOD");
	PlaybackErrorType errorType = eGST_ERROR_UNDERFLOW;
	MediaType trackType = eMEDIATYPE_VIDEO;

	EXPECT_EQ(0,p_aamp->mDiscontinuityTuneOperationId);
	PrivAAMPState state = eSTATE_IDLE;
	p_aamp->SetState(state);
	p_aamp->ScheduleRetune(errorType,trackType);
	
	EXPECT_EQ(0,p_aamp->mDiscontinuityTuneOperationId);
	
	p_aamp->TeardownStream(false);
	EXPECT_EQ(0,p_aamp->mDiscontinuityTuneOperationId);
}

TEST_F(PrivAampTests,SetupPipeSessionTest)
{
	EXPECT_TRUE(p_aamp->SetupPipeSession());
}

TEST_F(PrivAampTests,ClosePipeSessionTest)
{
	p_aamp->ClosePipeSession();
}

TEST_F(PrivAampTests,SendMessageOverPipeTest)
{
	const char *str;
	p_aamp->SendMessageOverPipe(str,25);
}

TEST_F(PrivAampTests, TuneHelperTest)
{
	TuneType tuneType=eTUNETYPE_SEEK;
	p_aamp->mEncryptedPeriodFound = true;
	p_aamp->mPipelineIsClear = true;
	EXPECT_FALSE(p_aamp->mDisableRateCorrection);

	tuneType=eTUNETYPE_SEEKTOEND;
	p_aamp->TuneHelper(tuneType,true);
	EXPECT_FALSE(p_aamp->mDisableRateCorrection);
}

TEST_F(PrivAampTests, TuneHelperTest_1)
{
	TuneType tuneType=eTUNETYPE_LAST;
	p_aamp->TuneHelper(tuneType,true);

	tuneType=eTUNETYPE_NEW_NORMAL;
	p_aamp->TuneHelper(tuneType,true);
	EXPECT_FALSE(p_aamp->mDisableRateCorrection);
}

TEST_F(PrivAampTests, TuneHelperTest_2)
{
	TuneType tuneType=eTUNETYPE_LAST;
	bool flag = p_aamp->IsNewTune();
	EXPECT_TRUE(flag);

	p_aamp->Tune("sampleUrl",true,NULL,true,false,NULL,true,NULL,0);

	p_aamp->mMediaFormat=eMEDIAFORMAT_HDMI;
	p_aamp->TuneHelper(tuneType,true);

	p_aamp->mMediaFormat=eMEDIAFORMAT_DASH;
	p_aamp->TuneHelper(tuneType,true);
}

TEST_F(PrivAampTests, TuneHelperTest_3)
{
	TuneType tuneType=eTUNETYPE_NEW_NORMAL;
	p_aamp->TuneHelper(tuneType,true);
	bool flag = p_aamp->IsNewTune();
	EXPECT_TRUE(flag);
}

TEST_F(PrivAampTests, ReloadTSBTest)
{
	p_aamp->mTSBEnabled=true;
	p_aamp->mMediaFormat=eMEDIAFORMAT_HLS;
	p_aamp->ReloadTSB();

	p_aamp->mMediaFormat=eMEDIAFORMAT_DASH;
	p_aamp->ReloadTSB();

	EXPECT_NE(0,p_aamp->mMediaFormat);
}

TEST_F(PrivAampTests, TuneTest)
{
	p_aamp->Tune("sampleUrl",true,NULL,true,false,NULL,true,NULL,0);
}

TEST_F(PrivAampTests, TuneTest_1)
{
	p_aamp->Tune("sampleUrl",false,NULL,true,false,NULL,true,NULL,0);
}

TEST_F(PrivAampTests, GetLangCodePreferenceTest)
{
	int langCodePreference = p_aamp->GetLangCodePreference();
	EXPECT_NE(105,langCodePreference);
	EXPECT_EQ(-1,(LangCodePreference)langCodePreference);
}

TEST_F(PrivAampTests, GetMediaFormatTypeTest)
{
	MediaFormat  type = p_aamp->GetMediaFormatType("hdmiin:");
	EXPECT_EQ(5,type);

	type = p_aamp->GetMediaFormatType("cvbsin:");
	EXPECT_EQ(6,type);

	type = p_aamp->GetMediaFormatType("live:");
	EXPECT_EQ(4,type);

	type = p_aamp->GetMediaFormatType("ocap://");
	EXPECT_EQ(8,type);

	type = p_aamp->GetMediaFormatType("http://127.0.0.1");
	EXPECT_EQ(9,type);

	type = p_aamp->GetMediaFormatType("m3u8");
	EXPECT_EQ(9,type);

	type = p_aamp->GetMediaFormatType("mpd");
	EXPECT_EQ(9,type);

	type = p_aamp->GetMediaFormatType("abc");
	EXPECT_EQ(9,type);

	type = p_aamp->GetMediaFormatType("abcpppppppppppppppppppppppppppasadsafadxyz");
	EXPECT_EQ(9,type);

	type = p_aamp->GetMediaFormatType("abcpppppppppppppppppppppppppppasadsafadxyzqwertyuippadsghjkl");
	EXPECT_EQ(9,type);

	type = p_aamp->GetMediaFormatType("abc!!!!!@@@@@@@@@@@#############$$$$$5%^^^ppppppppasadsafadxyzqwertyuippadsghjkl");
	EXPECT_EQ(9,type);
}

TEST_F(PrivAampTests, CheckForDiscontinuityStallTest)
{
	p_aamp->CheckForDiscontinuityStall(eMEDIATYPE_VIDEO);
	EXPECT_EQ(p_aamp->mDiscontinuityTuneOperationId,0);
}

TEST_F(PrivAampTests, GetContentTypeTest)
{
	ContentType type;
	const char *ctype[]= {"Unknown","CDVR","VOD","LINEAR_TV","IVOD","EAS","xfinityhome","DVR","MDVR","IPDVR","PPV","OTT","OTA","HDMI_IN","COMPOSITE_IN","SLE"};
	for (int i= ContentType_UNKNOWN ; i< ContentType_MAX ;i++)
	{
	p_aamp->SetContentType(ctype[i]);
	type=p_aamp->GetContentType();
	EXPECT_EQ(type,i);
	}
	
	p_aamp->SetContentType("SLE");
	type=p_aamp->GetContentType();
	EXPECT_EQ(type,15);


	p_aamp->SetContentType("QWERTY");
	type=p_aamp->GetContentType();
	EXPECT_EQ(type,0);
	
	p_aamp->SetContentType("");
	type=p_aamp->GetContentType();
	EXPECT_EQ(type,0);

	p_aamp->SetContentType(NULL);
	type=p_aamp->GetContentType();
	EXPECT_EQ(type,0);
}

TEST_F(PrivAampTests,IsPlayEnabledTest)
{
	bool val = p_aamp->IsPlayEnabled();
	EXPECT_TRUE(val);
}

TEST_F(PrivAampTests,detachTest)
{
	p_aamp->mbPlayEnabled=true;
	p_aamp->detach();

	p_aamp->mbPlayEnabled=false;
	p_aamp->detach();

	EXPECT_FALSE(p_aamp->pipeline_paused);
	EXPECT_NE(p_aamp->seek_pos_seconds,0);
}

TEST_F(PrivAampTests,getAampCacheHandlerTest)
{
	AampCacheHandler *cHandler = p_aamp->getAampCacheHandler();
}

TEST_F(PrivAampTests,GetMaximumBitrateTest)
{
	long val = p_aamp->GetMaximumBitrate();
	EXPECT_NE(val,110987);

	val = p_aamp->GetMinimumBitrate();
	EXPECT_NE(val,110987);

	val = p_aamp->GetDefaultBitrate();
	EXPECT_NE(val,110987);

	val = p_aamp->GetDefaultBitrate4K();
	EXPECT_NE(val,110987);

	val = p_aamp->GetIframeBitrate();
	EXPECT_NE(val,110987);

	val = p_aamp->GetIframeBitrate4K();
	EXPECT_NE(val,110987);
}

TEST_F(PrivAampTests,PushFragmentTest)
{
	MediaType mediaType = eMEDIATYPE_SUBTITLE;
	char *ptr; 
	size_t len;
	double fragmentTime;
	double fragmentDuration;
	p_aamp->PushFragment(mediaType,NULL,10,10.98,99.99);
}

TEST_F(PrivAampTests,EndOfStreamReachedTest)
{
	p_aamp->EndOfStreamReached(eMEDIATYPE_SUBTITLE);
}

TEST_F(PrivAampTests,EndOfStreamReachedTest_1)
{
	p_aamp->SetState(eSTATE_BUFFERING);
	p_aamp->EndOfStreamReached(eMEDIATYPE_VIDEO);
}

TEST_F(PrivAampTests,GetSeekBaseTest)
{
	double val = p_aamp->GetSeekBase();
	EXPECT_NE(val,110.987);
}

TEST_F(PrivAampTests,GetCurrentDRMTest)
{
	std::shared_ptr<AampDrmHelper> var = p_aamp->GetCurrentDRM();
}

TEST_F(PrivAampTests,GetThumbnailTracksTest)
{
	std::string str = p_aamp->GetThumbnailTracks();
}

TEST_F(PrivAampTests,GetThumbnailsTest)
{
	std::string str = p_aamp->GetThumbnails(10.987676,45.465472);
}

TEST_F(PrivAampTests,GetTuneEventConfigTest)
{
	int val = p_aamp->GetTuneEventConfig(true);
	EXPECT_NE(val,10);

	val = p_aamp->GetTuneEventConfig(false);
	EXPECT_NE(val,10);
}
TEST_F(PrivAampTests,UpdatePreferredAudioListTest)
{
	p_aamp->preferredRenditionString="redention1,";
	p_aamp->preferredRenditionString="redention2,";
	p_aamp->preferredRenditionString="redention3";
	p_aamp->UpdatePreferredAudioList();

	EXPECT_FALSE(p_aamp->preferredRenditionString.empty());
	EXPECT_NE(p_aamp->preferredRenditionList.size(),0);
}

TEST_F(PrivAampTests,UpdatePreferredAudioListTest_1)
{
	p_aamp->preferredCodecString="codec1,";
	p_aamp->preferredCodecString="codec2,";
	p_aamp->preferredCodecString="codec3,";

	p_aamp->UpdatePreferredAudioList();
	
	EXPECT_FALSE(p_aamp->preferredCodecString.empty());
	EXPECT_NE(p_aamp->preferredCodecList.size(),0);
}

TEST_F(PrivAampTests,UpdatePreferredAudioListTest_2)
{
	p_aamp->preferredLanguagesString="language1,";
	p_aamp->preferredLanguagesString="language2,";
	p_aamp->preferredLanguagesString="language3";

	p_aamp->UpdatePreferredAudioList();

	EXPECT_FALSE(p_aamp->preferredLanguagesString.empty());
	EXPECT_NE(p_aamp->preferredLanguagesList.size(),10);
}

TEST_F(PrivAampTests,UpdatePreferredAudioListTest_3)
{
	p_aamp->preferredLabelsString="label1,";
	p_aamp->preferredLabelsString="label2,";
	p_aamp->preferredLabelsString="label3";

	p_aamp->UpdatePreferredAudioList();

	EXPECT_FALSE(p_aamp->preferredLabelsString.empty());
	EXPECT_NE(p_aamp->preferredLabelList.size(),10);
}


TEST_F(PrivAampTests,SetEventPriorityAsyncTuneTest)
{
	p_aamp->SetEventPriorityAsyncTune(true);
}

TEST_F(PrivAampTests,SetEventPriorityAsyncTuneTest_1)
{
	p_aamp->SetEventPriorityAsyncTune(false);
}

TEST_F(PrivAampTests,GetAsyncTuneConfigTest)
{
	p_aamp->GetAsyncTuneConfig();
	EXPECT_FALSE(p_aamp->mAsyncTuneEnabled);
}

TEST_F(PrivAampTests,UpdateVideoRectangleTest)
{
	p_aamp->UpdateVideoRectangle(100,200,300,400);
}

TEST_F(PrivAampTests,UpdateVideoRectangleTest_1)
{
	p_aamp->UpdateVideoRectangle(100,200,1000,4000);
}
TEST_F(PrivAampTests,UpdateVideoRectangleTest_2)
{
	p_aamp->UpdateVideoRectangle(0,0,0,0);
}

TEST_F(PrivAampTests,SetVideoRectangleTest)
{
	p_aamp->SetVideoRectangle(100,200,300,400);
}

TEST_F(PrivAampTests,SetVideoRectangleTest_1)
{
	p_aamp->SetVideoRectangle(100,200,3000,4000);

	p_aamp->SetVideoRectangle(0,0,0,0);
}

TEST_F(PrivAampTests,SetVideoRectangleTest_2)
{
	p_aamp->SetState(eSTATE_PAUSED);

	p_aamp->mMediaFormat = eMEDIAFORMAT_OTA;
	p_aamp->SetVideoRectangle(100,200,300,400);

	p_aamp->mMediaFormat = eMEDIAFORMAT_HLS;
	p_aamp->SetVideoRectangle(100,200,300,400);

	p_aamp->mMediaFormat = eMEDIAFORMAT_DASH;
	p_aamp->SetVideoRectangle(100,200,300,400);
}

TEST_F(PrivAampTests,SetVideoZoomTest)
{
	p_aamp->SetVideoZoom(VIDEO_ZOOM_FULL);

	p_aamp->SetVideoZoom(VIDEO_ZOOM_NONE);
}

TEST_F(PrivAampTests,SetVideoMuteTest)
{
	p_aamp->SetVideoMute(true);

	p_aamp->SetVideoMute(false);
}

TEST_F(PrivAampTests,SetSubtitleMuteTest)
{
	p_aamp->SetSubtitleMute(true);

	p_aamp->SetSubtitleMute(false);
}

TEST_F(PrivAampTests,SetAudioVolumeTest)
{
	p_aamp->SetAudioVolume(0);

	p_aamp->SetAudioVolume(10);
	p_aamp->SetAudioVolume(100);
	p_aamp->SetAudioVolume(1000);
	p_aamp->SetAudioVolume(10000);
}

TEST_F(PrivAampTests,SetAudioVolumeTest_1)
{
	p_aamp->SetAudioVolume(-10);
	p_aamp->SetAudioVolume(-100);
}

TEST_F(PrivAampTests,DisableDownloadsTest)
{
	p_aamp->DisableDownloads();
	EXPECT_FALSE(p_aamp->mDownloadsEnabled);
}

TEST_F(PrivAampTests,DownloadsAreEnabledTest)
{
	p_aamp->DownloadsAreEnabled();
	EXPECT_TRUE(p_aamp->mDownloadsEnabled);
}

TEST_F(PrivAampTests,EnableDownloadsTest)
{
	p_aamp->EnableDownloads();
	EXPECT_TRUE(p_aamp->mDownloadsEnabled);
}

TEST_F(PrivAampTests,InterruptableMsSleepTest)
{
	p_aamp->InterruptableMsSleep(0);
	EXPECT_TRUE(p_aamp->mDownloadsEnabled);
}


TEST_F(PrivAampTests,GetDurationMsTest)
{
	long long val = p_aamp->GetDurationMs();
	EXPECT_EQ(val,0);
}

TEST_F(PrivAampTests,GetDurationMsTest_1)
{
	p_aamp->mMediaFormat = eMEDIAFORMAT_PROGRESSIVE;
	long long val = p_aamp->GetDurationMs();
	EXPECT_EQ(val,0);
}

TEST_F(PrivAampTests,DurationFromStartOfPlaybackMsTest)
{
	long long val = p_aamp->DurationFromStartOfPlaybackMs();
	EXPECT_EQ(val,0);
}

TEST_F(PrivAampTests,DurationFromStartOfPlaybackMsTest_1)
{
	p_aamp->mMediaFormat = eMEDIAFORMAT_PROGRESSIVE;
	long long val = p_aamp->DurationFromStartOfPlaybackMs();
	EXPECT_EQ(val,0);
}

TEST_F(PrivAampTests,GetPositionMsTest)
{
	p_aamp->seek_pos_seconds = 0;
	long long val = p_aamp->GetPositionMs();
	EXPECT_EQ(val,0);
}

TEST_F(PrivAampTests,GetPositionMsTest_1)
{
	p_aamp->seek_pos_seconds = 10109;
	long long val = p_aamp->GetPositionMs();
	EXPECT_NE(val,0);
}

TEST_F(PrivAampTests,LockGetPositionMsTest)
{
	bool flag = p_aamp->LockGetPositionMilliseconds();
	EXPECT_TRUE(flag);
}

TEST_F(PrivAampTests,UnlockGetPositionMsTest)
{
	p_aamp->UnlockGetPositionMilliseconds();
}

TEST_F(PrivAampTests,GetPositionRelativeToSeekMillisecondsTest)
{
	p_aamp->SetState(eSTATE_SEEKING);
	long long val  = p_aamp->GetPositionRelativeToSeekMilliseconds();
	EXPECT_EQ(val,0);
}

TEST_F(PrivAampTests,GetPositionRelativeToSeekMillisecondsTest_1)
{
	p_aamp->seek_pos_seconds = 123450;
	p_aamp->SetState(eSTATE_SEEKING);
	long long val  = p_aamp->GetPositionRelativeToSeekMilliseconds();
	EXPECT_EQ(val,0);
}

TEST_F(PrivAampTests,GetPositionMillisecondsTest)
{
	p_aamp->trickStartUTCMS=123456789;
	long long val  = p_aamp->GetPositionMilliseconds();
	EXPECT_EQ(val,0);
}

TEST_F(PrivAampTests,SendStreamCopyTest)
{
	EXPECT_FALSE(p_aamp->SendStreamCopy(eMEDIATYPE_VIDEO,NULL,20,12.34,34.567,465.7696));
}

TEST_F(PrivAampTests,SendStreamTransferTest)
{
	p_aamp->SendStreamTransfer(eMEDIATYPE_VIDEO,NULL,182.34,374.567,465.7696,true,true);
	p_aamp->SendStreamTransfer(eMEDIATYPE_VIDEO,NULL,182.34,374.567,465.7696,false,false);
	p_aamp->SendStreamTransfer(eMEDIATYPE_VIDEO,NULL,182.34,374.567,465.7696,true,false);
	p_aamp->SendStreamTransfer(eMEDIATYPE_VIDEO,NULL,182.34,374.567,465.7696,false,true);
}

TEST_F(PrivAampTests,IsLiveTest)
{
	EXPECT_FALSE(p_aamp->IsLive());
	EXPECT_FALSE(p_aamp->IsLiveStream());
}

TEST_F(PrivAampTests,IsAudioPlayContextCreationSkippedTest)
{
	p_aamp->IsAudioPlayContextCreationSkipped();
	EXPECT_FALSE(p_aamp->IsAudioPlayContextCreationSkipped());
}

TEST_F(PrivAampTests,stopTest)
{
	p_aamp->Stop();
	EXPECT_FALSE(p_aamp->mAutoResumeTaskPending);
}

TEST_F(PrivAampTests,stopTest_1)
{
	p_aamp->Stop();
	EXPECT_FALSE(p_aamp->mAutoResumeTaskPending);
	EXPECT_FALSE(p_aamp->IsTSBSupported());
}

TEST_F(PrivAampTests,SaveTimedMetadateTest)
{
    p_aamp->SaveTimedMetadata(0, "somesampleString#@$##@","savemetadatacontent", 100);
    p_aamp->SaveTimedMetadata(0, "somesampleString#@$##@","savemetadatacontent", 100,NULL,10);
    p_aamp->SaveTimedMetadata(0, "somesampleString#@$##@","savemetadatacontent", 100,NULL,0);

    EXPECT_NE(p_aamp->timedMetadata.size(),0);
}

TEST_F(PrivAampTests,SaveNewTimedMetadataTest)
{
    p_aamp->SaveNewTimedMetadata(0, "somesampleString#@$##@","savemetadatacontent", 100);
    p_aamp->SaveNewTimedMetadata(0, "somesampleString#@$##@","savemetadatacontent", 100,NULL,10);
    p_aamp->SaveNewTimedMetadata(0, "somesampleString#@$##@","savemetadatacontent", 100,NULL,0);

	EXPECT_NE(p_aamp->timedMetadataNew.size(),0);
}

TEST_F(PrivAampTests,ReportTimedMetadataTest)
{
	p_aamp->ReportTimedMetadata(false);
	EXPECT_NE(p_aamp->mTimedMetadataStartTime,0);

	p_aamp->ReportTimedMetadata(12325454,"sample","sample1",15,true,"sample2",21.332);
}

TEST_F(PrivAampTests,ReportTimedMetadataTest_1)
{
	EXPECT_TRUE(p_aamp->IsNewTune());
	p_aamp->ReportTimedMetadata(true);
	EXPECT_NE(p_aamp->mTimedMetadataStartTime,0);

	p_aamp->ReportTimedMetadata(12325454,"sample","sample1",15,false,"sample2",21.332);
}

TEST_F(PrivAampTests,ReportBulkTimedMetadataTest)
{
	p_aamp->ReportBulkTimedMetadata();
	
	EXPECT_NE(p_aamp->mTimedMetadataStartTime,0);

	EXPECT_EQ(p_aamp->mTimedMetadataDuration,0);
}

TEST_F(PrivAampTests,ReportContentGapTest)
{
	p_aamp->ReportContentGap(132322356,"sample",1232424415);
	p_aamp->ReportContentGap(0,"sample",0);
	p_aamp->ReportContentGap(10,"sample",100);
	p_aamp->ReportContentGap(-10987610,"sample",10.0388755555555);
}

TEST_F(PrivAampTests,InitializeCCTest)
{
	p_aamp->InitializeCC();
}

TEST_F(PrivAampTests,NotifyFirstFrameReceivedTest)
{
	p_aamp->NotifyFirstFrameReceived();
}

TEST_F(PrivAampTests,NotifyFirstFrameReceivedTest_1)
{
	p_aamp->SetState(eSTATE_IDLE);
	p_aamp->NotifyFirstFrameReceived();
}

TEST_F(PrivAampTests,NotifyFirstFrameReceivedTest_2)
{
	TuneType tuneType = eTUNETYPE_NEW_NORMAL;
	p_aamp->TuneHelper(tuneType, false);
	p_aamp->NotifyFirstFrameReceived();

	PrivAAMPState state;
	p_aamp->GetState(state);
	EXPECT_EQ(state,8);
}

TEST_F(PrivAampTests,NotifyFirstFrameReceivedTest_3)
{
	p_aamp->SetState(eSTATE_PLAYING);

	TuneType tuneType = eTUNETYPE_NEW_NORMAL;
	p_aamp->TuneHelper(tuneType, true);
	p_aamp->NotifyFirstFrameReceived();

	PrivAAMPState state;
	p_aamp->GetState(state);
	EXPECT_NE(state,8);
}

TEST_F(PrivAampTests,DiscontinuityTest)
{
	bool flag = p_aamp->Discontinuity(eMEDIATYPE_VIDEO,true);
	EXPECT_TRUE(flag);

	flag = p_aamp->Discontinuity(eMEDIATYPE_VIDEO,false);
	EXPECT_FALSE(flag);
}

TEST_F(PrivAampTests,ScheduleRetuneTest)
{
	p_aamp->ScheduleRetune(eGST_ERROR_PTS,eMEDIATYPE_VIDEO);
	EXPECT_EQ(p_aamp->mDiscontinuityTuneOperationId,0);
}

TEST_F(PrivAampTests,ScheduleRetuneTest_1)
{
	p_aamp->SetState(eSTATE_IDLE);
	TuneType tuneType = eTUNETYPE_SEEKTOLIVE;
	p_aamp->TuneHelper(tuneType, true);
		p_aamp->ScheduleRetune(eGST_ERROR_PTS,eMEDIATYPE_VIDEO);

	EXPECT_EQ(p_aamp->mDiscontinuityTuneOperationId,0);
}

TEST_F(PrivAampTests,ScheduleRetuneTest_2)
{
	p_aamp->SetState(eSTATE_PLAYING);
	p_aamp->ScheduleRetune(eGST_ERROR_VIDEO_BUFFERING,eMEDIATYPE_VIDEO);
	EXPECT_EQ(p_aamp->mDiscontinuityTuneOperationId,0);
}

TEST_F(PrivAampTests,GetStateTest)
{
	p_aamp->SetState(eSTATE_IDLE);

	PrivAAMPState state = eSTATE_IDLE;
	p_aamp->GetState(state);

	state = eSTATE_SEEKING;
	p_aamp->GetState(state);
	p_aamp->SetState(eSTATE_PLAYING);

	state = eSTATE_PREPARING;
	p_aamp->GetState(state);
}

TEST_F(PrivAampTests,IsSinkCacheEmptyTest)
{
	bool flag =	p_aamp->IsSinkCacheEmpty(eMEDIATYPE_VIDEO);
	EXPECT_FALSE(flag);
}

TEST_F(PrivAampTests,ResetEOSSignalledFlagTest)
{
	p_aamp->ResetEOSSignalledFlag();
}

TEST_F(PrivAampTests,NotifyFragmentCachingCompleteTest)
{
	p_aamp->NotifyFragmentCachingComplete();
}

TEST_F(PrivAampTests,NotifyFragmentCachingCompleteTest_1)
{
	p_aamp->SetState(eSTATE_BUFFERING);

	PrivAAMPState state;
	p_aamp->GetState(state);
	EXPECT_EQ(state,5);

	p_aamp->NotifyFragmentCachingComplete();
	EXPECT_NE(state,8);
}

TEST_F(PrivAampTests,SendTunedEventTest)
{
	EXPECT_FALSE(p_aamp->SendTunedEvent(true));

	EXPECT_FALSE(p_aamp->SendTunedEvent(false));
}

TEST_F(PrivAampTests,SendTunedEventTest_1)
{
	TuneType tuneType = eTUNETYPE_NEW_NORMAL;
	p_aamp->TuneHelper(tuneType, true);

	EXPECT_TRUE(p_aamp->SendTunedEvent(true));	
}

TEST_F(PrivAampTests,SendVideoEndEventTest)
{
    EXPECT_FALSE(p_aamp->SendVideoEndEvent());
    CVideoStat * mVideoEnd;
    if( mVideoEnd == NULL)
    {
    mVideoEnd = new CVideoStat("DASH");
    }
    EXPECT_FALSE(p_aamp->SendVideoEndEvent());
}

TEST_F(PrivAampTests,UpdateVideoEndProfileResolutionTest)
{
	p_aamp->UpdateVideoEndProfileResolution(eMEDIATYPE_VIDEO,12345,15,30);

	p_aamp->UpdateVideoEndTsbStatus(TRUE);
	p_aamp->UpdateVideoEndTsbStatus(FALSE);

	p_aamp->UpdateProfileCappedStatus();
}

TEST_F(PrivAampTests,UpdateVideoEndMetricsTest)
{
	p_aamp->UpdateVideoEndMetrics(12.34567);

	std::string strUrl = "strUrl";
	p_aamp->UpdateVideoEndMetrics(eMEDIATYPE_VIDEO, 12345, 123, strUrl, 10.98, 99.99);
}

TEST_F(PrivAampTests,UpdateVideoEndMetricsTest_1)
{
	std::string strUrl = "strUrl";
	p_aamp->UpdateVideoEndMetrics(eMEDIATYPE_VIDEO, 12345, 123, strUrl, 10.98, 99.99,true,true,NULL);
}

TEST_F(PrivAampTests,UpdateVideoEndMetricsTest_2)
{
	std::string strUrl = "strUrl";
	p_aamp->UpdateVideoEndMetrics(eMEDIATYPE_VIDEO, 12345, 123, strUrl, 10.98, 99.99,false,true,NULL);
}

TEST_F(PrivAampTests,UpdateVideoEndMetricsTest_3)
{
	std::string strUrl = "strUrl";
	p_aamp->UpdateVideoEndMetrics(eMEDIATYPE_VIDEO, 12345, 123, strUrl, 10.98, 99.99,false,false,NULL);
}

TEST_F(PrivAampTests,UpdateVideoEndMetricsTest_4)
{
	std::string strUrl = "strUrl";
	p_aamp->UpdateVideoEndMetrics(eMEDIATYPE_VIDEO, 12345, CURLE_ABORTED_BY_CALLBACK, strUrl, 10.98, 99.99,false,false,NULL);
}

TEST_F(PrivAampTests,IsFragmentCachingRequiredTest)
{
	EXPECT_FALSE(p_aamp->IsFragmentCachingRequired());
}

TEST_F(PrivAampTests,GetPlayerVideoSizeTest)
{
	int width  = 1280;
	int height = 720;
	p_aamp->GetPlayerVideoSize(width, height);

	width  = 938710;
	height = 932410;
	p_aamp->GetPlayerVideoSize(width, height);
}

TEST_F(PrivAampTests,SetCallbackAsDispatchedTest)
{
	p_aamp->SetCallbackAsDispatched(56);
	p_aamp->SetCallbackAsDispatched(0);
	p_aamp->SetCallbackAsDispatched(989989);

	p_aamp->SetCallbackAsPending(56);
	p_aamp->SetCallbackAsPending(0);
	p_aamp->SetCallbackAsPending(9999999);
}

TEST_F(PrivAampTests,AddCustomHTTPHeaderTest)
{
	std::vector<std::string> headerValue;
	headerValue.push_back("sample");
	headerValue.push_back("sample:text");
	headerValue.push_back("");
	headerValue.push_back("sampletext&&&&S&&&&&&&&&&&&&&&&&&*&&&&&&&&&&&&&&&&&&&&&&&@$#^^^^^^^^^^^^^^^^^^^^^^^");
	p_aamp->AddCustomHTTPHeader("string",headerValue,false);
}

TEST_F(PrivAampTests,UpdateLiveOffsetTest)
{
	p_aamp->SetContentType("EAS");
	p_aamp->UpdateLiveOffset();
	EXPECT_NE(p_aamp->mLiveOffset,0);
}

TEST_F(PrivAampTests,UpdateLiveOffsetTest_1)
{
	p_aamp->SetContentType("SLE");
	p_aamp->UpdateLiveOffset();
	EXPECT_NE(p_aamp->mLiveOffset,0);
}


TEST_F(PrivAampTests,SendStalledErrorEventTest)
{
	p_aamp->SendStalledErrorEvent();
}

TEST_F(PrivAampTests,UpdateSubtitleTimestampTest)
{
	p_aamp->UpdateSubtitleTimestamp();

	p_aamp->PauseSubtitleParser(true);
	p_aamp->PauseSubtitleParser(false);
}

TEST_F(PrivAampTests,NotifyFirstBufferProcessedTest)
{
	p_aamp->NotifyFirstBufferProcessed();
}

TEST_F(PrivAampTests,NotifyFirstBufferProcessedTest_1)
{
	p_aamp->SetState(eSTATE_IDLE);

	PrivAAMPState state;
	p_aamp->GetState(state);
	EXPECT_EQ(state,0);
}

TEST_F(PrivAampTests,NotifyFirstBufferProcessedTest_2)
{
	p_aamp->SetState(eSTATE_SEEKING);

	TuneType tuneType = eTUNETYPE_NEW_NORMAL;
	p_aamp->TuneHelper(tuneType, false);//true

	PrivAAMPState state;
	p_aamp->GetState(state);
	EXPECT_EQ(state,4);//8 nov8
}

TEST_F(PrivAampTests,NotifyFirstBufferProcessedTest_3)
{
	p_aamp->SetState(eSTATE_SEEKING);

	TuneType tuneType = eTUNETYPE_NEW_NORMAL;
	p_aamp->TuneHelper(tuneType, true);

	PrivAAMPState state;
	p_aamp->GetState(state);
	EXPECT_EQ(state,4);//8 nov8
}

TEST_F(PrivAampTests,ResetTrickStartUTCTimeTest)
{
	p_aamp->ResetTrickStartUTCTime();
	EXPECT_EQ(p_aamp->trickStartUTCMS,0);
}


TEST_F(PrivAampTests,getStreamTypeTest)
{
	int val = p_aamp->getStreamType();
	EXPECT_EQ(10,val);
}


TEST_F(PrivAampTests,GetMediaFormatTypeEnumTest)
{
	int val = p_aamp->GetMediaFormatTypeEnum();
	EXPECT_EQ(0,val);
}


TEST_F(PrivAampTests,NotifyFirstFragmentDecryptedTest)
{
	p_aamp->NotifyFirstFragmentDecrypted();
	EXPECT_TRUE(p_aamp->IsNewTune());
}

TEST_F(PrivAampTests,NotifyFirstFragmentDecryptedTest_1)
{
	EXPECT_TRUE(p_aamp->IsNewTune());
	TuneType tuneType = eTUNETYPE_NEW_NORMAL;
	p_aamp->TuneHelper(tuneType, true);
	p_aamp->NotifyFirstFragmentDecrypted();

	EXPECT_TRUE(p_aamp->IsNewTune());
}

TEST_F(PrivAampTests,IsLiveAdjustRequiredTest)
{
	EXPECT_TRUE(p_aamp->IsLiveAdjustRequired());
}

TEST_F(PrivAampTests,IsLiveAdjustRequiredTest_1)
{
	p_aamp->SetContentType("EAS");
	EXPECT_FALSE(p_aamp->IsLiveAdjustRequired());
}


TEST_F(PrivAampTests,SendHTTPHeaderResponseTest)
{
	p_aamp->SendHTTPHeaderResponse();
}

TEST_F(PrivAampTests,SendSupportedSpeedsChangedEventTest)
{
	p_aamp->SendSupportedSpeedsChangedEvent(TRUE);
	p_aamp->SendSupportedSpeedsChangedEvent(FALSE);
}

TEST_F(PrivAampTests,SendBlockedEventTest)
{
	p_aamp->SendBlockedEvent("SAMPLE","streamCheck");
}

TEST_F(PrivAampTests,SendWatermarkSessionUpdateEventTest)
{
	p_aamp->SendWatermarkSessionUpdateEvent(100,200,"SystemString");
}

TEST_F(PrivAampTests,IsTuneCompletedTest)
{
	EXPECT_FALSE(p_aamp->IsTuneCompleted());
}

TEST_F(PrivAampTests,GetPreferredDRMTest)
{
	int drmType = p_aamp->GetPreferredDRM();
	EXPECT_NE(drmType,0);
}

TEST_F(PrivAampTests,FoundEventBreakTest)
{
	EventBreakInfo info;
	info.payload="payload";
	info.name="sampleTest";
	info.duration=250;
	info.presentationTime=12345;
	p_aamp->FoundEventBreak("adBraeakId",25,info);
	EXPECT_FALSE(p_aamp->mTSBEnabled);
}

TEST_F(PrivAampTests,SetAlternateContentsTest)
{
	p_aamp->SetAlternateContents("adBraeakId","adstringId","http://sampleurl.com");
}


TEST_F(PrivAampTests,SendAdResolvedEventTest)
{
	p_aamp->SendAdResolvedEvent("adBraeakId",true,10,123445);
	EXPECT_TRUE(p_aamp->mDownloadsEnabled);
}

TEST_F(PrivAampTests,SendAdReservationEventTest)
{
	p_aamp->SendAdReservationEvent(AAMP_EVENT_AD_RESERVATION_START,"adbreakId",123445,true);
	p_aamp->SendAdReservationEvent(AAMP_EVENT_AD_RESERVATION_START,"adbreakId",123445,false);
	p_aamp->SendAdReservationEvent(AAMP_EVENT_AD_RESERVATION_END,"adbreakId",123445,true);

	EXPECT_EQ(p_aamp->mAdEventsQ.size(),0);
}

TEST_F(PrivAampTests,SendAdPlacementEventTest)
{
	p_aamp->SendAdPlacementEvent(AAMP_EVENT_AD_PLACEMENT_START,"adStringid",234,5678,true,0);
	p_aamp->SendAdPlacementEvent(AAMP_EVENT_AD_PLACEMENT_START,"adStringid",234,5678,false,0);
	p_aamp->SendAdPlacementEvent(AAMP_EVENT_AD_PLACEMENT_ERROR,"adStringid",234,5678,true,0);
	
	EXPECT_NE(p_aamp->mAdEventsQ.size(),0);
}

TEST_F(PrivAampTests,getStreamTypeStringTest)
{
	std::string str = p_aamp->getStreamTypeString();
	EXPECT_STRNE("samplestring",str.c_str());
}

TEST_F(PrivAampTests,mediaType2BucketTest)
{
	EXPECT_EQ(9,p_aamp->mediaType2Bucket(eMEDIATYPE_VIDEO));
}

TEST_F(PrivAampTests,mediaType2BucketTest_1)
{
	EXPECT_EQ(9,p_aamp->mediaType2Bucket(eMEDIATYPE_VIDEO));
	EXPECT_EQ(10,p_aamp->mediaType2Bucket(eMEDIATYPE_AUDIO));
	EXPECT_EQ(5,p_aamp->mediaType2Bucket(eMEDIATYPE_LICENCE));
	EXPECT_EQ(6,p_aamp->mediaType2Bucket(eMEDIATYPE_IFRAME));

	EXPECT_EQ(20,p_aamp->mediaType2Bucket((MediaType)20));
}

TEST_F(PrivAampTests,SetTunedManifestUrlTest)
{
	p_aamp->SetTunedManifestUrl(TRUE);
	p_aamp->SetTunedManifestUrl(FALSE);

	std::string str = p_aamp->GetTunedManifestUrl();
	EXPECT_TRUE(str.find("_fog"));
}

TEST_F(PrivAampTests,GetNetworkProxyTest)
{
	std::string str = p_aamp->GetNetworkProxy();

	str = p_aamp->GetLicenseReqProxy();
}

TEST_F(PrivAampTests,SignalTrickModeDiscontinuityTest)
{
	p_aamp->SignalTrickModeDiscontinuity();
}

TEST_F(PrivAampTests,IsMuxedStreamTest)
{
	EXPECT_FALSE(p_aamp->IsMuxedStream());
}

TEST_F(PrivAampTests,StopTrackInjectionTest)
{
	p_aamp->StopTrackInjection(eMEDIATYPE_VIDEO);
	p_aamp->StopTrackInjection(eMEDIATYPE_AUDIO);
}

TEST_F(PrivAampTests,ResumeTrackInjectionTest)
{
	p_aamp->ResumeTrackInjection(eMEDIATYPE_VIDEO);
	p_aamp->ResumeTrackInjection(eMEDIATYPE_AUDIO);
}

TEST_F(PrivAampTests,NotifyFirstVideoPTSTest)
{
	p_aamp->NotifyFirstVideoPTS(1234567,345667);
	p_aamp->NotifyFirstVideoPTS(123456723545365,3456678964374);

	p_aamp->NotifyVideoBasePTS(1234567,34566789);
	p_aamp->NotifyVideoBasePTS(123456723545365,3456678964);
}

TEST_F(PrivAampTests,SendVTTCueDataAsEventTest)
{
	p_aamp->SendVTTCueDataAsEvent(NULL);
}

TEST_F(PrivAampTests,IsSubtitleEnabledTest)
{
	EXPECT_FALSE(p_aamp->IsSubtitleEnabled());
}

TEST_F(PrivAampTests,WebVTTCueListenersRegisteredTest)
{
	EXPECT_FALSE(p_aamp->WebVTTCueListenersRegistered());
}

TEST_F(PrivAampTests,SendId3MetadataEventTest)
{
    aamp::id3_metadata::CallbackData * id3Metadata;
    p_aamp->SendId3MetadataEvent(id3Metadata);
}

TEST_F(PrivAampTests,FlushStreamSinkTest)
{
	p_aamp->FlushStreamSink(10.9876,45.54329);

	p_aamp->FlushStreamSink(0,0);
	p_aamp->FlushStreamSink(10.9876098765432198764323,45.543298765432198764323);
}

TEST_F(PrivAampTests,PreCachePlaylistDownloadTaskTest)
{
	p_aamp->PreCachePlaylistDownloadTask();
}

TEST_F(PrivAampTests,GetPreferredTextPropertiesTest)
{
	std::vector<BitsPerSecond> bitrateList;
	p_aamp->SetVideoTracks(bitrateList);

	std::string str = p_aamp->GetAvailableVideoTracks();
}

TEST_F(PrivAampTests,GetAvailableTracksTest)
{
	std::string str1 = p_aamp->GetAvailableAudioTracks(true);
	std::string  str2 = p_aamp->GetAvailableTextTracks();

	str1 = p_aamp->GetAvailableAudioTracks(false);
}

TEST_F(PrivAampTests,GetVideoRectangleTest)
{
	std::string str = p_aamp->GetVideoRectangle();
}

TEST_F(PrivAampTests,SetAppNameTest)
{
	p_aamp->SetAppName("sampleAppName");
	std::string str = p_aamp->GetAppName();
	EXPECT_STREQ("sampleAppName",str.c_str());
}

TEST_F(PrivAampTests,individualizationTest)
{
	p_aamp->individualization("sampleAppName");
}

TEST_F(PrivAampTests,GetInitialBufferDurationTest)
{
	int val = p_aamp->GetInitialBufferDuration();
	EXPECT_NE(12,val);
}

TEST_F(PrivAampTests,IsFirstVideoFrameDisplayedRequiredTest)
{
	EXPECT_FALSE(p_aamp->IsFirstVideoFrameDisplayedRequired());
}

TEST_F(PrivAampTests,NotifyFirstVideoFrameDisplayedTest)
{
	p_aamp->NotifyFirstVideoFrameDisplayed();
}

TEST_F(PrivAampTests,SetStateBufferingIfRequiredTest)
{
	EXPECT_FALSE(p_aamp->SetStateBufferingIfRequired());
}

TEST_F(PrivAampTests,TrackDownloadsAreEnabledTest)
{
    EXPECT_FALSE(p_aamp->TrackDownloadsAreEnabled(eMEDIATYPE_INIT_VIDEO));
    EXPECT_FALSE(p_aamp->TrackDownloadsAreEnabled(eMEDIATYPE_LICENCE));
    EXPECT_FALSE(p_aamp->TrackDownloadsAreEnabled(eMEDIATYPE_PLAYLIST_AUX_AUDIO));
    EXPECT_FALSE(p_aamp->TrackDownloadsAreEnabled(eMEDIATYPE_DEFAULT));
    
    EXPECT_TRUE(p_aamp->TrackDownloadsAreEnabled(eMEDIATYPE_AUX_AUDIO));
    EXPECT_TRUE(p_aamp->TrackDownloadsAreEnabled(eMEDIATYPE_VIDEO));
}


TEST_F(PrivAampTests,StopBufferingTest)
{
    p_aamp->StopBuffering(true);
    p_aamp->StopBuffering(false);
}


TEST_F(PrivAampTests,GetLicenseServerUrlForDrmTest)
{
	std::string str = p_aamp->GetLicenseServerUrlForDrm(eDRM_PlayReady);
	EXPECT_STRNE("sample",str.c_str());
}

TEST_F(PrivAampTests,GetAudioTrackTest)
{
	int val = p_aamp->GetAudioTrack();
	EXPECT_EQ(-1,val);
}

TEST_F(PrivAampTests,GetAudioTrackInfoTest)
{
	std::string str = p_aamp->GetAudioTrackInfo();
	EXPECT_STRNE("sample",str.c_str());
}

TEST_F(PrivAampTests,GetTextTrackInfoTest)
{
	std::string str = p_aamp->GetTextTrackInfo();
	EXPECT_STRNE("sample",str.c_str());
}

TEST_F(PrivAampTests,RefreshSubtitlesTest)
{
	p_aamp->RefreshSubtitles();
}

TEST_F(PrivAampTests,SetTextTrackTest)
{
    p_aamp->SetTextTrack(-1,NULL);
    int val = p_aamp->GetTextTrack();
    EXPECT_EQ(-1,val);
    
    p_aamp->SetTextTrack(1,NULL);
    val = p_aamp->GetTextTrack();
    EXPECT_EQ(-1,val);
}

TEST_F(PrivAampTests,SetCCStatusTest)
{
	EXPECT_FALSE(p_aamp->GetCCStatus());

	p_aamp->SetCCStatus(true);
	EXPECT_TRUE(p_aamp->GetCCStatus());

	p_aamp->SetCCStatus(false);
	EXPECT_FALSE(p_aamp->GetCCStatus());
}

TEST_F(PrivAampTests,NotifyAudioTracksChangedTest)
{
	p_aamp->NotifyAudioTracksChanged();
	p_aamp->NotifyTextTracksChanged();
}

TEST_F(PrivAampTests,SetTextStyleTest)
{
	std::string text = p_aamp->GetTextStyle();
	EXPECT_EQ(text.length(),0);
	EXPECT_STRNE("sample",text.c_str());
}

TEST_F(PrivAampTests,IsActiveInstancePresentTest)
{
	EXPECT_TRUE(p_aamp->IsActiveInstancePresent());
}

TEST_F(PrivAampTests,SetTrackDiscontinuityIgnoredStatusTest)
{
	p_aamp->SetTrackDiscontinuityIgnoredStatus(eMEDIATYPE_VIDEO);
	EXPECT_TRUE(p_aamp->IsDiscontinuityIgnoredForOtherTrack(eMEDIATYPE_VIDEO));

	p_aamp->ResetTrackDiscontinuityIgnoredStatus();
}

TEST_F(PrivAampTests,IsAudioOrVideoOnlyTest)
{
	EXPECT_FALSE(p_aamp->IsAudioOrVideoOnly(FORMAT_INVALID,FORMAT_INVALID,FORMAT_INVALID));
	EXPECT_FALSE(p_aamp->IsAudioOrVideoOnly(FORMAT_VIDEO_ES_MPEG2,FORMAT_AUDIO_ES_AAC,FORMAT_MPEGTS));

}

TEST_F(PrivAampTests,DisableContentRestrictionsTest)
{
	p_aamp->DisableContentRestrictions(10,45,true);
	p_aamp->DisableContentRestrictions();

	p_aamp->DisableContentRestrictions(10,45,false);
}

TEST_F(PrivAampTests,EnableContentRestrictionsTest)
{
	p_aamp->EnableContentRestrictions();
}

TEST_F(PrivAampTests,EnableContentRestrictionsTest_1)
{

	p_aamp->GetMediaFormatType("live:");
	p_aamp->EnableContentRestrictions();


	p_aamp->GetMediaFormatType("mpd");
	p_aamp->EnableContentRestrictions();

	p_aamp->GetMediaFormatType("m3u8");
	p_aamp->EnableContentRestrictions();
}

TEST_F(PrivAampTests,ScheduleAsyncTaskTest)
{
	IdleTask task;void *arg;
	p_aamp->ScheduleAsyncTask(task,arg,"taskName");
}

TEST_F(PrivAampTests,RemoveAsyncTaskTest)
{
	bool flag = p_aamp->RemoveAsyncTask(10);
	EXPECT_FALSE(flag);
}

TEST_F(PrivAampTests,AcquireStreamLockTest)
{
	p_aamp->AcquireStreamLock();
}

TEST_F(PrivAampTests,TryStreamLockTest)
{
	bool flag = p_aamp->TryStreamLock();
	EXPECT_TRUE(flag);

	p_aamp->ReleaseStreamLock();
}

TEST_F(PrivAampTests,IsAuxiliaryAudioEnabledTest)
{
	EXPECT_FALSE(p_aamp->IsAuxiliaryAudioEnabled());
}

TEST_F(PrivAampTests,ResetDiscontinuityInTracksTest)
{
	p_aamp->ResetDiscontinuityInTracks();
}

TEST_F(PrivAampTests,SetPreferredLanguagesTest)
{
	Accessibility *accessibilityItem;
	p_aamp->SetPreferredLanguages("LangList","PrefferedRedention","prefferedType","codeList","LableList",accessibilityItem);
}

TEST_F(PrivAampTests,EnableMediaDownloadsTest)
{
	for (int i = 0; i <= 19; i++)
	{
		p_aamp->EnableMediaDownloads((MediaType)i);
	}
}

TEST_F(PrivAampTests,EnableAllMediaDownloadsTest)
{
	p_aamp->EnableAllMediaDownloads();
}

TEST_F(PrivAampTests,UpdateBufferBasedOnLiveOffsetTest)
{
	p_aamp->UpdateBufferBasedOnLiveOffset();
}

TEST_F(PrivAampTests,GetCustomHeadersTest)
{
	struct curl_slist* headers = p_aamp->GetCustomHeaders(eMEDIATYPE_MANIFEST);
	curl_slist_free_all(headers);
}

TEST_F(PrivAampTests,ProcessID3MetadataTest)
{
 p_aamp->ProcessID3Metadata(NULL,10,eMEDIATYPE_VIDEO,12431);
}

TEST_F(PrivAampTests,GetPauseOnFirstVideoFrameDispTest)
{
	EXPECT_FALSE(p_aamp->GetPauseOnFirstVideoFrameDisp());
}

TEST_F(PrivAampTests,SetLLDashServiceDataTest)
{
	AampLLDashServiceData *data;
 	data = p_aamp->GetLLDashServiceData();
}

TEST_F(PrivAampTests,SetVidTimeScaleTest)
{
 	uint32_t val = 12346;
 	p_aamp->SetVidTimeScale(val);
 	uint32_t val1 = p_aamp->GetVidTimeScale();
 	EXPECT_EQ(val,val1);
}

TEST_F(PrivAampTests,SetAudTimeScaleTest)
{
 	uint32_t val = 12346;
 	p_aamp->SetAudTimeScale(val);
 	uint32_t val1 = p_aamp->GetAudTimeScale();
 	EXPECT_EQ(val,val1);
}

TEST_F(PrivAampTests,SetLLDashSpeedCacheTest)
{
	struct SpeedCache spCache;
	p_aamp->SetLLDashSpeedCache(spCache);

	struct SpeedCache *pCache1 = p_aamp->GetLLDashSpeedCache();
}

TEST_F(PrivAampTests,SetLiveOffsetAppRequestTest)
{
	p_aamp->SetLiveOffsetAppRequest(true);
	EXPECT_TRUE(p_aamp->GetLiveOffsetAppRequest());
}

TEST_F(PrivAampTests,SetLowLatencyServiceConfiguredTest)
{
	p_aamp->SetLowLatencyServiceConfigured(true);
	EXPECT_TRUE(p_aamp->GetLowLatencyServiceConfigured());
}

TEST_F(PrivAampTests,SetLowLatencyServiceConfiguredTest_1)
{
	p_aamp->SetLowLatencyServiceConfigured(false);
	EXPECT_FALSE(p_aamp->GetLowLatencyServiceConfigured());
}

TEST_F(PrivAampTests,SetCurrentLatencyTest)
{
	p_aamp->SetCurrentLatency(123456);
	long val = p_aamp->GetCurrentLatency();
	EXPECT_EQ(val,123456);
}

TEST_F(PrivAampTests,GetMediaStreamContextTest)
{
	MediaStreamContext *mCtx = p_aamp->GetMediaStreamContext(eMEDIATYPE_VIDEO);
}

TEST_F(PrivAampTests,GetPeriodDurationTimeValueTest)
{
	double val = p_aamp->GetPeriodDurationTimeValue();
 	double val1 = p_aamp->GetPeriodStartTimeValue();
 	double val2 = p_aamp->GetPeriodScaledPtoStartTime();

 	std::string str = p_aamp->GetPlaybackStats();

	EXPECT_EQ(val,0);
	EXPECT_EQ(val1,0);
	EXPECT_EQ(val2,0);
	
	EXPECT_STRNE("sample",str.c_str());
}

TEST_F(PrivAampTests,LoadFogConfigTest)
{
	long val = p_aamp->LoadFogConfig();
	EXPECT_EQ(val,0);

	p_aamp->LoadAampAbrConfig();
}

TEST_F(PrivAampTests,GetLicenseCustomDataTest)
{
	std::string str = p_aamp->GetLicenseCustomData();
}

TEST_F(PrivAampTests,UpdateUseSinglePipelineTest1)
{
	p_aamp->UpdateUseSinglePipeline();
}
TEST_F(PrivAampTests,UpdateMaxDRMSessionsTest1)
{
	p_aamp->UpdateMaxDRMSessions();
}
TEST_F(PrivAampTests,UpdateMaxDRMSessionsTest2)
{
	p_aamp->SetState(eSTATE_SEEKING);
	p_aamp->UpdateMaxDRMSessions();
}
TEST_F(PrivAampTests,GetVideoPlaybackQualityTest)
{
	std::string str = p_aamp->GetVideoPlaybackQuality();
	EXPECT_STRNE("sample",str.c_str());
}

TEST_F(PrivAampTests,IsGstreamerSubsEnabledTest)
{
	EXPECT_FALSE(p_aamp->IsGstreamerSubsEnabled());
}

TEST_F(PrivAampTests,GetLastDownloadedManifestTest)
{
	std::string manifest;
	p_aamp->GetLastDownloadedManifest(manifest);
}
TEST_F(PrivAampTests,ID3MetadataHandlerTest)
{
	MediaType mediaType = eMEDIATYPE_AUDIO; 
	const uint8_t* ptr = reinterpret_cast<const uint8_t*>("ID3 Metadata");
	size_t pkt_len = strlen(reinterpret_cast<const char*>(ptr));; 
	SegmentInfo_t info(100.0,90.0,5.0);

	p_aamp->ID3MetadataHandler(mediaType,ptr, pkt_len, info);
}
TEST_F(PrivAampTests,ReportID3MetadataTest)
{
	MediaType mediaType = eMEDIATYPE_AUDIO;
    const uint8_t* ptr = reinterpret_cast<const uint8_t*>("ID3 Metadata");
    uint32_t len = strlen(reinterpret_cast<const char*>(ptr));
    const char* schemeIdURI = "testSchemeIdURI";
    const char* id3Value = "testID3Value";
    uint64_t presTime = 123456789;
    uint32_t id3ID = 123;
    uint32_t eventDur = 456;
    uint32_t tScale = 1000;
    uint64_t tStampOffset = 789012345;

    
    p_aamp->ReportID3Metadata(mediaType, ptr, len, schemeIdURI, id3Value, presTime, id3ID, eventDur, tScale, tStampOffset);
}
TEST_F(PrivAampTests,SetLLDashServiceDataTest1)
{
	AampLLDashServiceData testData;
	p_aamp->SetLLDashServiceData(testData);

}

TEST_F(PrivAampTests,DisableMediaDownloadsTest)
{
	MediaType type = eMEDIATYPE_VIDEO;
	p_aamp->DisableMediaDownloads(type);
}
TEST_F(PrivAampTests,ReplaceKeyIDPsshValidDataTest)
{
	unsigned char inputPsshData[] = {
    0x00, 0x00, 0x00, 0x3c, 0x70, 0x73, 0x73, 0x68, 0x00, 0x00, 0x00, 0x00,
    0xed, 0xef, 0x8b, 0xa9, 0x79, 0xd6, 0x4a, 0xce, 0xa3, 0xc8, 0x27, 0xdc,
    0xd5, 0x1d, 0x21, 0xed, 0x00, 0x00, 0x00, 0x1c, 0x08, 0x01, 0x12, 0x10,
    0x00, 0x00, 0x00, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
    0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0xff, 0x22, 0x06, 0x74, 0x65,
    0x73, 0x74, 0x5f, 0x37
	};
	size_t inputLength = sizeof(inputPsshData);
	size_t outputLength = 0;
    unsigned char* outputPsshData = p_aamp->ReplaceKeyIDPsshData(inputPsshData, inputLength, outputLength);

	EXPECT_GT(outputLength,0);
}
TEST_F(PrivAampTests,ReplaceKeyIDPsshInValidDataTest)
{
	size_t outputLength = 0;
    unsigned char* outputPsshData = p_aamp->ReplaceKeyIDPsshData(nullptr,0, outputLength);

	EXPECT_EQ(outputPsshData,nullptr);
	EXPECT_EQ(outputLength,0);
}
TEST_F(PrivAampTests,IsAudioOrVideoOnlyTest1)
{
	p_aamp->mVideoComponentCount = 0;
	StreamOutputFormat mVideoFormat_result= p_aamp->mVideoFormat = FORMAT_VIDEO_ES_MPEG2;

	StreamOutputFormat videoFormat_result = FORMAT_INVALID;

	StreamOutputFormat audioFormat_result=  FORMAT_INVALID;

	StreamOutputFormat auxFormat_result=  FORMAT_INVALID;
	
	bool result = p_aamp->IsAudioOrVideoOnly(videoFormat_result,audioFormat_result,auxFormat_result);

	EXPECT_TRUE(result);
}
TEST_F(PrivAampTests,IsAudioOrVideoOnlyTest2)
{
	p_aamp->mAudioComponentCount = 0;

	StreamOutputFormat videoFormat_result = FORMAT_INVALID;

	StreamOutputFormat mAudioFormat_result= p_aamp->mAudioFormat = FORMAT_AUDIO_ES_MP3;

	StreamOutputFormat audioFormat_result=  FORMAT_INVALID;

	StreamOutputFormat auxFormat_result=  FORMAT_INVALID;
	
	bool result = p_aamp->IsAudioOrVideoOnly(videoFormat_result,audioFormat_result,auxFormat_result);

	EXPECT_TRUE(result);
}
TEST_F(PrivAampTests,IsAudioOrVideoOnlyTest3)
{
	p_aamp->mAudioComponentCount = 0;

	StreamOutputFormat videoFormat_result = FORMAT_INVALID;

	StreamOutputFormat mAudioFormat_result= p_aamp->mAuxFormat = FORMAT_UNKNOWN;

	StreamOutputFormat audioFormat_result=  FORMAT_INVALID;

	StreamOutputFormat auxFormat_result=  FORMAT_INVALID;
	
	bool result = p_aamp->IsAudioOrVideoOnly(videoFormat_result,audioFormat_result,auxFormat_result);

	EXPECT_TRUE(result);
}

TEST_F(PrivAampTests,GetLicenseServerUrlForDrmTest1)
{
	std::string str = p_aamp->GetLicenseServerUrlForDrm(eDRM_WideVine);
	EXPECT_STRNE("sample",str.c_str());
}
TEST_F(PrivAampTests,GetLicenseServerUrlForDrmTest2)
{
	std::string str = p_aamp->GetLicenseServerUrlForDrm(eDRM_ClearKey);
	EXPECT_STRNE("sample",str.c_str());
}
TEST_F(PrivAampTests,SendTuneMetricsEventTest)
{
	std::string timeMetricData = "Sample time metric data"; 
   p_aamp->SendTuneMetricsEvent(timeMetricData);
}

TEST_F(PrivAampTests,mediaType2BucketTest_122)
{
    EXPECT_EQ(11,p_aamp->mediaType2Bucket(eMEDIATYPE_SUBTITLE));
    EXPECT_EQ(12,p_aamp->mediaType2Bucket(eMEDIATYPE_AUX_AUDIO));
    EXPECT_EQ(0,p_aamp->mediaType2Bucket(eMEDIATYPE_MANIFEST));
    EXPECT_EQ(5,p_aamp->mediaType2Bucket(eMEDIATYPE_INIT_VIDEO));
    EXPECT_EQ(6,p_aamp->mediaType2Bucket(eMEDIATYPE_INIT_AUDIO));
    EXPECT_EQ(7,p_aamp->mediaType2Bucket(eMEDIATYPE_INIT_SUBTITLE));
    EXPECT_EQ(8,p_aamp->mediaType2Bucket(eMEDIATYPE_INIT_AUX_AUDIO));
    EXPECT_EQ(1,p_aamp->mediaType2Bucket(eMEDIATYPE_PLAYLIST_VIDEO));
    EXPECT_EQ(2,p_aamp->mediaType2Bucket(eMEDIATYPE_PLAYLIST_AUDIO));
    EXPECT_EQ(3,p_aamp->mediaType2Bucket(eMEDIATYPE_PLAYLIST_SUBTITLE));
    EXPECT_EQ(4,p_aamp->mediaType2Bucket(eMEDIATYPE_PLAYLIST_AUX_AUDIO));
    EXPECT_EQ(20,p_aamp->mediaType2Bucket((MediaType)20));
}

TEST_F(PrivAampTests, GetCustomLicenseHeaders_EmptyMap)
{
    PrivateInstanceAAMP aamp;
    std::unordered_map<std::string, std::vector<std::string>> customHeaders;
    aamp.GetCustomLicenseHeaders(customHeaders);
    EXPECT_TRUE(customHeaders.empty());
}
TEST_F(PrivAampTests,SetDiscontinuityParamTest1)
{
     p_aamp->SetDiscontinuityParam();
}
TEST_F(PrivAampTests,SetLatencyParamTest1)
{
    p_aamp->SetLatencyParam(2.223);
}
TEST_F(PrivAampTests, SetLatencyParamTest2) 
{
	double latency = DBL_MAX;
	p_aamp->SetLatencyParam(latency); 
}

TEST_F(PrivAampTests, SetLatencyParamTest3) 
{ 
	double latency = DBL_MIN;
	p_aamp->SetLatencyParam(latency); 
}

TEST_F(PrivAampTests, SetLatencyParamTest4) 
{ 
	p_aamp->SetLatencyParam(-5.5); 
}
TEST_F(PrivAampTests, ConvertSpeedToStr1)
 {
    char buffer[10];
    char* ConvertSpeedToStr(long bps, char *str);
    ASSERT_STREQ(ConvertSpeedToStr(5000, buffer), " 5000");
}

TEST_F(PrivAampTests, UpdateVideoEndMetricsTest_12)
{
	AAMPAbrInfo info;

	info.abrCalledFor = AAMPAbrType::AAMPAbrBandwidthUpdate;
	info.currentProfileIndex = 6;
	info.desiredProfileIndex = 5;
	info.currentBandwidth = 1024;
	info.desiredBandwidth = 2028;
	info.networkBandwidth = 5120;
	info.errorType = AAMPNetworkErrorType::AAMPNetworkErrorNone;
	info.errorCode = 0;

	p_aamp->UpdateVideoEndMetrics(info);

}
TEST_F(PrivAampTests, UpdateVideoEndMetricsTest_13)
{
	AAMPAbrInfo info;

	info.abrCalledFor = AAMPAbrType::AAMPAbrFragmentDownloadFailed;
	info.currentProfileIndex = 6;
	info.desiredProfileIndex = 5;
	info.currentBandwidth = 1024;
	info.desiredBandwidth = 2028;
	info.networkBandwidth = 5120;
	info.errorType = AAMPNetworkErrorType::AAMPNetworkErrorNone;
	info.errorCode = 0;

	p_aamp->UpdateVideoEndMetrics(info);

}

TEST_F(PrivAampTests,UpdateVideoEndMetricsTest1)
{ 
	// covering eMEDIATYPE_MANIFEST switch case
	p_aamp->UpdateVideoEndMetrics(12.34567);

	MediaType mediaType = eMEDIATYPE_MANIFEST; 
	BitsPerSecond bitrate = 500000; 
	int curlOrHTTPCode = CURLcode::CURLE_FUNCTION_NOT_FOUND; 
	std::string strUrl = "strUrl"; 
	double duration = DBL_MAX;
	double curlDownloadTime = DBL_MAX;
	bool keyChanged = true;
	bool isEncrypted = true;

	p_aamp->UpdateVideoEndMetrics(mediaType, bitrate, curlOrHTTPCode, strUrl, duration, curlDownloadTime);
}

TEST_F(PrivAampTests,UpdateVideoEndMetricsTest2)
{ 
	// covering eMEDIATYPE_PLAYLIST_VIDEO switch case

	MediaType mediaType = eMEDIATYPE_PLAYLIST_VIDEO; 
	BitsPerSecond bitrate = 500000; 
	int curlOrHTTPCode = CURLcode::CURLE_FUNCTION_NOT_FOUND; 
	std::string strUrl = "strUrl"; 
	double duration = DBL_MIN;
	double curlDownloadTime = DBL_MIN;
	bool keyChanged = true;
	bool isEncrypted = true;

	p_aamp->UpdateVideoEndMetrics(mediaType, bitrate, curlOrHTTPCode, strUrl, duration, curlDownloadTime);
}
TEST_F(PrivAampTests,UpdateVideoEndMetricsTest3)
{ 
	// covering eMEDIATYPE_PLAYLIST_AUDIO switch case

	MediaType mediaType = eMEDIATYPE_PLAYLIST_AUDIO; 
	BitsPerSecond bitrate = 500000; 
	int curlOrHTTPCode = CURLcode::CURLE_FUNCTION_NOT_FOUND; 
	std::string strUrl = "strUrl"; 
	double duration = 20.15;
	double curlDownloadTime = 10.98;
	bool keyChanged = true;
	bool isEncrypted = true;

	p_aamp->UpdateVideoEndMetrics(mediaType, bitrate, curlOrHTTPCode, strUrl, duration, curlDownloadTime);
}
TEST_F(PrivAampTests,UpdateVideoEndMetricsTest4)
{ 
	// covering eMEDIATYPE_PLAYLIST_AUX_AUDIO switch case

	MediaType mediaType = eMEDIATYPE_PLAYLIST_AUX_AUDIO; 
	BitsPerSecond bitrate = 500000; 
	int curlOrHTTPCode = CURLcode::CURLE_FUNCTION_NOT_FOUND; 
	std::string strUrl = "strUrl"; 
	double duration = 20.15;
	double curlDownloadTime = 10.98;
	bool keyChanged = false;
	bool isEncrypted = true;

	p_aamp->UpdateVideoEndMetrics(mediaType, bitrate, curlOrHTTPCode, strUrl, duration, curlDownloadTime);
}
TEST_F(PrivAampTests,UpdateVideoEndMetricsTest5)
{ 
	// covering eMEDIATYPE_PLAYLIST_IFRAME switch case

	MediaType mediaType = eMEDIATYPE_PLAYLIST_IFRAME; 
	BitsPerSecond bitrate = 500000; 
	int curlOrHTTPCode = CURLcode::CURLE_FUNCTION_NOT_FOUND; 
	std::string strUrl = "strUrl"; 
	double duration = 20.15;
	double curlDownloadTime = 10.98;
	bool keyChanged = true;
	bool isEncrypted = true;

	p_aamp->UpdateVideoEndMetrics(mediaType, bitrate, curlOrHTTPCode, strUrl, duration, curlDownloadTime);
}
TEST_F(PrivAampTests,UpdateVideoEndMetricsTest6)
{ 
	// covering eMEDIATYPE_VIDEO switch case

	MediaType mediaType = eMEDIATYPE_VIDEO; 
	BitsPerSecond bitrate = 500000; 
	int curlOrHTTPCode = CURLcode::CURLE_FUNCTION_NOT_FOUND; 
	std::string strUrl = "strUrl"; 
	double duration = 20.15;
	double curlDownloadTime = 10.98;
	bool keyChanged = true;
	bool isEncrypted = true;

	p_aamp->UpdateVideoEndMetrics(mediaType, bitrate, curlOrHTTPCode, strUrl, duration, curlDownloadTime);
}
TEST_F(PrivAampTests,UpdateVideoEndMetricsTest7)
{ 
	// covering eMEDIATYPE_AUDIO switch case

	MediaType mediaType = eMEDIATYPE_AUDIO; 
	BitsPerSecond bitrate = 100000; 
	int curlOrHTTPCode = CURLcode::CURLE_FUNCTION_NOT_FOUND; 
	std::string strUrl = "strUrl"; 
	double duration = 20.15;
	double curlDownloadTime = 10.98;
	bool keyChanged = true;
	bool isEncrypted = true;

	p_aamp->UpdateVideoEndMetrics(mediaType, bitrate, curlOrHTTPCode, strUrl, duration, curlDownloadTime);
}
TEST_F(PrivAampTests,UpdateVideoEndMetricsTest8)
{ 
	// covering eMEDIATYPE_AUX_AUDIO switch case

	MediaType mediaType = eMEDIATYPE_AUX_AUDIO; 
	BitsPerSecond bitrate = 200000; 
	int curlOrHTTPCode = CURLcode::CURLE_FUNCTION_NOT_FOUND; 
	std::string strUrl = "strUrl"; 
	double duration = 20.15;
	double curlDownloadTime = 10.98;
	bool keyChanged = true;
	bool isEncrypted = true;

	p_aamp->UpdateVideoEndMetrics(mediaType, bitrate, curlOrHTTPCode, strUrl, duration, curlDownloadTime);
}
TEST_F(PrivAampTests,UpdateVideoEndMetricsTest9)
{ 
	// covering eMEDIATYPE_IFRAME switch case

	MediaType mediaType = eMEDIATYPE_IFRAME; 
	BitsPerSecond bitrate = 2500000; 
	int curlOrHTTPCode = CURLcode::CURLE_FUNCTION_NOT_FOUND; 
	std::string strUrl = "strUrl"; 
	double duration = DBL_MIN;
	double curlDownloadTime = DBL_MAX;
	bool keyChanged = true;
	bool isEncrypted = true;

	p_aamp->UpdateVideoEndMetrics(mediaType, bitrate, curlOrHTTPCode, strUrl, duration, curlDownloadTime);
}
TEST_F(PrivAampTests,UpdateVideoEndMetricsTest10)
{ 
	// covering eMEDIATYPE_INIT_IFRAME switch case

	MediaType mediaType = eMEDIATYPE_INIT_IFRAME; 
	BitsPerSecond bitrate = 500000; 
	int curlOrHTTPCode = CURLcode::CURLE_FUNCTION_NOT_FOUND; 
	std::string strUrl = "strUrl"; 
	double duration = 10.15;
	double curlDownloadTime = 8.98;
	bool keyChanged = true;
	bool isEncrypted = false;

	p_aamp->UpdateVideoEndMetrics(mediaType, bitrate, curlOrHTTPCode, strUrl, duration, curlDownloadTime);
}

TEST_F(PrivAampTests,UpdateVideoEndMetricsTest11)
{ 
	// covering eMEDIATYPE_INIT_VIDEO switch case

	MediaType mediaType = eMEDIATYPE_INIT_VIDEO; 
	BitsPerSecond bitrate = 500000; 
	int curlOrHTTPCode = CURLcode::CURLE_FUNCTION_NOT_FOUND; 
	std::string strUrl = "strUrl"; 
	double duration = 20.15;
	double curlDownloadTime = 10.98;
	bool keyChanged = false;
	bool isEncrypted = true;

	p_aamp->UpdateVideoEndMetrics(mediaType, bitrate, curlOrHTTPCode, strUrl, duration, curlDownloadTime);
}
TEST_F(PrivAampTests,UpdateVideoEndMetricsTest12)
{ 
	// covering eMEDIATYPE_INIT_AUDIO switch case

	MediaType mediaType = eMEDIATYPE_INIT_AUDIO; 
	BitsPerSecond bitrate = 500000; 
	int curlOrHTTPCode = CURLcode::CURLE_FUNCTION_NOT_FOUND; 
	std::string strUrl = "strUrl"; 
	double duration = 20.15;
	double curlDownloadTime = 10.98;
	bool keyChanged = true;
	bool isEncrypted = false;

	p_aamp->UpdateVideoEndMetrics(mediaType, bitrate, curlOrHTTPCode, strUrl, duration, curlDownloadTime);
}
TEST_F(PrivAampTests,UpdateVideoEndMetricsTest13)
{ 
	// covering eMEDIATYPE_INIT_AUX_AUDIO switch case
	MediaType mediaType = eMEDIATYPE_INIT_AUX_AUDIO; 
	BitsPerSecond bitrate = 500000; 
	int curlOrHTTPCode = CURLcode::CURLE_FUNCTION_NOT_FOUND; 
	std::string strUrl = "strUrl"; 
	double duration = 20.15;
	double curlDownloadTime = 11.8;
	bool keyChanged = false;
	bool isEncrypted = false;

	p_aamp->UpdateVideoEndMetrics(mediaType, bitrate, curlOrHTTPCode, strUrl, duration, curlDownloadTime);
}
TEST_F(PrivAampTests,UpdateVideoEndMetricsTest14)
{ 
	// covering eMEDIATYPE_SUBTITLE switch case
	MediaType mediaType = eMEDIATYPE_SUBTITLE; 
	BitsPerSecond bitrate = 500000; 
	int curlOrHTTPCode = CURLcode::CURLE_FUNCTION_NOT_FOUND; 
	std::string strUrl = "strUrl"; 
	double duration = 20.15;
	double curlDownloadTime = 8.98;
	bool keyChanged = false;
	bool isEncrypted = true;

	p_aamp->UpdateVideoEndMetrics(mediaType, bitrate, curlOrHTTPCode, strUrl, duration, curlDownloadTime);
}
TEST_F(PrivAampTests,UpdateVideoEndMetricsTest15)
{ 
	// covering eMEDIATYPE_DEFAULT switch case

	MediaType mediaType = eMEDIATYPE_DEFAULT; 
	BitsPerSecond bitrate = 500000; 
	int curlOrHTTPCode = CURLcode::CURLE_FUNCTION_NOT_FOUND; 
	std::string strUrl = "strUrl"; 
	double duration = 20.15;
	double curlDownloadTime = 8.98;
	bool keyChanged = false;
	bool isEncrypted = true;

	p_aamp->UpdateVideoEndMetrics(mediaType, bitrate, curlOrHTTPCode, strUrl, duration, curlDownloadTime);
}
TEST_F(PrivAampTests,NotifyFirstBufferProcessedTest1)
{
	//covering if condition when state == eSTATE_IDLE
	p_aamp->SetState(eSTATE_IDLE);
	p_aamp->NotifyFirstBufferProcessed();
}
TEST_F(PrivAampPrivTests,NotifyFirstBufferProcessedTest2)
{
	testp_aamp->CallNotifyFirstBufferProcessed();
}
TEST_F(PrivAampPrivTests,SetStateBufferingIfRequiredTest1)
{
	bool result1 = testp_aamp->CallSetStateBufferingIfRequired();
}
TEST_F(PrivAampTests,getStreamTypeTest0)
{
	//checking if condition MediaFormat == eMEDIAFORMAT_DASH`
	p_aamp->mMediaFormat = eMEDIAFORMAT_DASH;
	int val = p_aamp->getStreamType();
	EXPECT_EQ(val,20);
}

TEST_F(PrivAampTests,getStreamTypeTest1)
{
	//checking if condition MediaFormat == eMEDIAFORMAT_HLS
	p_aamp->mMediaFormat = eMEDIAFORMAT_HLS;
	int val = p_aamp->getStreamType();
	EXPECT_EQ(val,10);
}
TEST_F(PrivAampTests,getStreamTypeTest2)
{
	//checking if condition MediaFormat == eMEDIAFORMAT_PROGRESSIVE
	p_aamp->mMediaFormat = eMEDIAFORMAT_PROGRESSIVE;
	int val = p_aamp->getStreamType();
	EXPECT_EQ(val,30);
}
TEST_F(PrivAampTests,getStreamTypeTest3)
{
	//checking if condition MediaFormat == eMEDIAFORMAT_HLS_MP4
	p_aamp->mMediaFormat = eMEDIAFORMAT_HLS_MP4;
	int val = p_aamp->getStreamType();
	EXPECT_EQ(val,40);
}

TEST_F(PrivAampTests,getStreamTypeTest4)
{
	//checking else condition
	p_aamp->mMediaFormat = eMEDIAFORMAT_UNKNOWN;
	int val = p_aamp->getStreamType();
	EXPECT_EQ(val,0);
}

TEST_F(PrivAampTests,IsFirstVideoFrameDisplayedRequiredTest1)
{
	bool result = p_aamp->IsFirstVideoFrameDisplayedRequired();

	EXPECT_FALSE(result);
}
TEST_F(PrivAampTests,NotifyFirstVideoFrameDisplayedTest1)
{
	p_aamp->TuneHelper(eTUNETYPE_SEEKTOLIVE,true);
	p_aamp->SetState(eSTATE_IDLE);
	p_aamp->NotifyFirstVideoFrameDisplayed();
}
TEST_F(PrivAampPrivTests,NotifyFirstVideoFrameDisplayedTest2)
{
	testp_aamp->CallNotifyFirstVideoFrameDisplayed();
}
TEST_F(PrivAampPrivTests,NotifyFirstVideoFrameDisplayedTest3)
{
	testp_aamp->CallNotifyFirstVideoFrameDisplayed_1();
}
TEST_F(PrivAampTests, ConvertSpeedToStr2)
{
    char buffer[100];
	char* ConvertSpeedToStr(long bps, char *str);
    ConvertSpeedToStr(800000, buffer); 
}
TEST_F(PrivAampTests, ConvertSpeedToStr3)
{
    char buffer[100];
	char* ConvertSpeedToStr(long bps, char *str);
    ConvertSpeedToStr(80000000, buffer); 
}
TEST_F(PrivAampTests, ConvertSpeedToStr4)
{
    char buffer[100];
	char* ConvertSpeedToStr(long bps, char *str);
    ConvertSpeedToStr(8000000000, buffer); 
}

TEST_F(PrivAampTests,NotifyFirstVideoFrameDisplayedTest3)
{
	p_aamp->TuneHelper(eTUNETYPE_SEEKTOLIVE,true);
	p_aamp->SetState(eSTATE_PAUSED);
	p_aamp->SetStateBufferingIfRequired();
	p_aamp->NotifyFirstVideoFrameDisplayed();
}

TEST_F(PrivAampTests, ForceHttpCoversionforFogTest)
{
	std::string url = "http://example.com";
    std::string from = "http://ForceHttpCoversionforFog/from.com";
    std::string to = "http://ForceHttpCoversionforFog/to.com";

	void ForceHttpCoversionforFog(std::string& url,const std::string& from, const std::string& to);

	ForceHttpCoversionforFog(url,from,to);
}
TEST_F(PrivAampTests, getCurrentContentDownloadSpeedTest)
{
	PrivateInstanceAAMP *aamp;
	MediaType fileType = eMEDIATYPE_VIDEO;
	bool bDownloadStart = true;
	long start = 12345;
	double dlnow = 10.50;

	long getCurrentContentDownloadSpeed(PrivateInstanceAAMP *aamp,MediaType fileType,bool bDownloadStart,long start,double dlnow);

	long result = getCurrentContentDownloadSpeed(p_aamp,fileType,bDownloadStart,start,dlnow);

}
TEST_F(PrivAampTests, HandleSSLHeaderCallback_ValidHeader_1)
{
    const char *ptr = "Header: Value\r\n";
    size_t size = 1;
    size_t nmemb = 1;
    void* user_data;
    size_t result = p_aamp->HandleSSLHeaderCallback(ptr, size, nmemb, &user_data);
}
TEST_F(PrivAampTests, AddHighIdleTaskTest)
{
	IdleTask task; 
	void* arg = nullptr;
	DestroyTask dtask;

	p_aamp->AddHighIdleTask(task,&arg,dtask);
}

TEST_F(PrivAampTests, TuneHelperTest_11)
{
	TuneType tuneType=eTUNETYPE_LAST;
	bool flag = p_aamp->IsNewTune();
	EXPECT_TRUE(flag);

	p_aamp->Tune("sampleUrl",true,NULL,true,false,NULL,true,NULL,0);

	//covering if condition for mMediaFormat=eMEDIAFORMAT_PROGRESSIVE
	p_aamp->mMediaFormat=eMEDIAFORMAT_PROGRESSIVE;
	p_aamp->TuneHelper(tuneType,true);

	//covering if condition for mMediaFormat=eMEDIAFORMAT_OTA
	p_aamp->mMediaFormat=eMEDIAFORMAT_OTA;
	p_aamp->TuneHelper(tuneType,true);

	//covering if condition for mMediaFormat=eMEDIAFORMAT_COMPOSITE
	p_aamp->mMediaFormat=eMEDIAFORMAT_COMPOSITE;
	p_aamp->TuneHelper(tuneType,true);

	//covering if condition for mMediaFormat=eMEDIAFORMAT_SMOOTHSTREAMINGMEDIA
	p_aamp->mMediaFormat=eMEDIAFORMAT_SMOOTHSTREAMINGMEDIA;
	p_aamp->TuneHelper(tuneType,true);

}
TEST_F(PrivAampTests, UpdateVideoEndMetricsDelegatesCorrectly) {
    // Setup test data
    MediaType mediaType = eMEDIATYPE_VIDEO;
    BitsPerSecond bitrate = 12;
    int curlOrHTTPCode = 34;
    std::string strUrl = "strUrl";
    double curlDownloadTime = 2.22;
    ManifestData* manifestData;
    // Call UpdateVideoEndMetrics
    p_aamp->UpdateVideoEndMetrics(mediaType, bitrate, curlOrHTTPCode, strUrl, curlDownloadTime, manifestData);
}
TEST_F(PrivAampTests, UpdateVideoEndMetricsDelegatesCorrectly2) {
    // Setup test data
    MediaType mediaType = eMEDIATYPE_VIDEO;
    BitsPerSecond bitrate = 12;
    int curlOrHTTPCode = INT_MAX;
    std::string strUrl = "strUrl";
    double curlDownloadTime = DBL_MAX;
    ManifestData* manifestData;
    // Call UpdateVideoEndMetrics
    p_aamp->UpdateVideoEndMetrics(mediaType, bitrate, curlOrHTTPCode, strUrl, curlDownloadTime, manifestData);
}
TEST_F(PrivAampTests, UpdateVideoEndMetricsDelegatesCorrectly3) {
    // Setup test data
    MediaType mediaType = eMEDIATYPE_VIDEO;
    BitsPerSecond bitrate = 12;
    int curlOrHTTPCode = INT_MIN;
    std::string strUrl = "strUrl";
    double curlDownloadTime = DBL_MIN;
    ManifestData* manifestData;
    // Call UpdateVideoEndMetrics
    p_aamp->UpdateVideoEndMetrics(mediaType, bitrate, curlOrHTTPCode, strUrl, curlDownloadTime, manifestData);
}
TEST_F(PrivAampTests,SendDownloadErrorEventTest2)
{
	p_aamp->mTSBEnabled = true;
	p_aamp->IsTSBSupported();
	p_aamp->SendDownloadErrorEvent(AAMP_TUNE_FAILED_PTS_ERROR,131);
}
TEST_F(PrivAampTests,SendDownloadErrorEventTest4)
{
	p_aamp->SendDownloadErrorEvent(AAMP_TUNE_FAILED_PTS_ERROR,28);
}
TEST_F(PrivAampTests,SendDownloadErrorEventTest5)
{
	p_aamp->SendDownloadErrorEvent(AAMP_TUNE_FAILED_PTS_ERROR,132);
}
TEST_F(PrivAampTests,SendDownloadErrorEventTest6)
{
	p_aamp->SendDownloadErrorEvent(AAMP_TUNE_FAILED_PTS_ERROR,99);
}
TEST_F(PrivAampTests,SendErrorEventTest11)
{
	p_aamp->mTSBEnabled = true;
	p_aamp->IsTSBSupported();
	p_aamp->SetState(eSTATE_INITIALIZED);
	p_aamp->SendErrorEvent(AAMP_TUNE_FAILURE_UNKNOWN,"DESCRIPTION",true,11,12,13,"responseString");
}

TEST_F(PrivAampTests,BlockUntilGstreamerWantsDataTest11)
{
	p_aamp->mbDownloadsBlocked = true;
	p_aamp->mDownloadsEnabled = false;
	p_aamp->BlockUntilGstreamerWantsData(NULL,10,20);
}

TEST_F(PrivAampTests,stopTest_11)
{
	p_aamp->mTSBEnabled = true;
	p_aamp->IsTSBSupported();
	p_aamp->Stop();
}
TEST_F(PrivAampTests,GetLastDownloadedManifestTest1)
{
	std::string manifest;
	p_aamp->mMediaFormat=eMEDIAFORMAT_DASH;
	p_aamp->GetLastDownloadedManifest(manifest);
}


