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

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "MockAampConfig.h"
#include "MockAampScheduler.h"
#include "MockPrivateInstanceAAMP.h"
#include "MockAampGstPlayer.h"
#include "MockStreamAbstractionAAMP.h"
#include "MockAampStreamSinkManager.h"
#include "main_aamp.h"

using ::testing::_;
using ::testing::Return;
using ::testing::SetArgReferee;
using ::testing::AtLeast;
using ::testing::NiceMock;

class PlayerInstanceAAMPTests : public ::testing::Test
{
protected:
    PlayerInstanceAAMP *mPlayerInstance = nullptr;
    PrivateInstanceAAMP *mPrivateInstanceAAMP{};
    AampConfig *mConfig;
    void SetUp() override 
    {
        if(gpGlobalConfig == nullptr)
        {
            gpGlobalConfig =  new AampConfig();
        }

        mPrivateInstanceAAMP = new PrivateInstanceAAMP(gpGlobalConfig);
        mPlayerInstance = new PlayerInstanceAAMP();
        g_mockAampConfig = new NiceMock<MockAampConfig>();
        g_mockAampScheduler = new MockAampScheduler();
        g_mockPrivateInstanceAAMP = new MockPrivateInstanceAAMP();
        g_mockStreamAbstractionAAMP = new MockStreamAbstractionAAMP(mPrivateInstanceAAMP);
        mPrivateInstanceAAMP->mpStreamAbstractionAAMP = g_mockStreamAbstractionAAMP;
        mConfig = new AampConfig();
        mplayer = new TestablePlayerInstanceAAMP();

        g_mockAampGstPlayer = new MockAAMPGstPlayer( mPrivateInstanceAAMP);
        g_mockAampStreamSinkManager = new NiceMock<MockAampStreamSinkManager>();
        g_mockStreamAbstractionAAMP = new MockStreamAbstractionAAMP(mPrivateInstanceAAMP);

        mPrivateInstanceAAMP->mpStreamAbstractionAAMP = g_mockStreamAbstractionAAMP;

   		EXPECT_CALL(*g_mockAampStreamSinkManager, GetStreamSink(_)).WillRepeatedly(Return(g_mockAampGstPlayer));

    }
    
    void TearDown() override 
    {
        delete g_mockPrivateInstanceAAMP;
        g_mockPrivateInstanceAAMP = nullptr;

        delete g_mockAampScheduler;
        g_mockAampScheduler = nullptr;

        delete g_mockAampConfig;
        g_mockAampConfig = nullptr;

        delete g_mockStreamAbstractionAAMP;
        g_mockStreamAbstractionAAMP =nullptr;

        delete mPrivateInstanceAAMP;
        mPrivateInstanceAAMP = nullptr;

        delete g_mockAampGstPlayer;
        g_mockAampGstPlayer = nullptr;

        delete g_mockAampStreamSinkManager;
        g_mockAampStreamSinkManager = nullptr;

        delete mConfig;
        mConfig = nullptr;

        delete mplayer;
        mplayer = nullptr;

    }
    class TestablePlayerInstanceAAMP : public PlayerInstanceAAMP {
public:
    TestablePlayerInstanceAAMP() : PlayerInstanceAAMP(){}
    void SetTextTrack_Internal(int trackId, char *data)
    {
        int x = trackId;
        char* y = new char[50];
        strcpy(y,"data");
        SetTextTrackInternal(x, y);
    }
    void SetRate_Internal(float rate,int overshootcorrection)
    {
        SetRateInternal(rate,overshootcorrection);
    }
    void Seek_Internal(double secondsRelativeToTuneTime, bool keepPaused)
    {
        SeekInternal(secondsRelativeToTuneTime,keepPaused);
    }
};
TestablePlayerInstanceAAMP *mplayer;
};
TEST_F(PlayerInstanceAAMPTests,SeekInternalTest1)
{
    double secondsRelativeToTuneTime = 2.5;
    bool keepPaused = true;
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState()).WillRepeatedly(Return(eSTATE_INITIALIZED));
	
    mPrivateInstanceAAMP->seek_pos_seconds = secondsRelativeToTuneTime ;

    mPrivateInstanceAAMP->mConfig->SetConfigValue(AAMP_APPLICATION_SETTING, eAAMPConfig_PlaybackOffset,secondsRelativeToTuneTime);

    mplayer->Seek_Internal(secondsRelativeToTuneTime,keepPaused);
}
TEST_F(PlayerInstanceAAMPTests,SetRateInternalTest)
{
    float rate = 2.2f;
    int overshootcorrection = 10;
    EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_RepairIframes))
        .WillOnce(Return(true));
    mplayer->SetRate_Internal(rate,overshootcorrection);
}
 TEST_F(PlayerInstanceAAMPTests,SetTextTrack_InternalTest)
 {
    int trackId = 10;
    char data[] = "data";
    mplayer->SetTextTrack_Internal(trackId,data);
 }
// Testing calling PauseAt with position
// Expect to call stop pause position monitoring, followed by 
// start pause position monitoring with the requested position
TEST_F(PlayerInstanceAAMPTests, PauseAt)
{
    double pauseAtSeconds = 100.0;
    long long pauseAtMilliseconds = pauseAtSeconds * 1000;

    EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState()).WillRepeatedly(Return(eSTATE_PLAYING));

    EXPECT_CALL(*g_mockPrivateInstanceAAMP, StopPausePositionMonitoring("PauseAt() called")).Times(1);
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, StartPausePositionMonitoring(pauseAtMilliseconds)).Times(1);

    mPlayerInstance->PauseAt(pauseAtSeconds);
}

// Testing calling PauseAt with position 0
// Expect to call stop pause position monitoring, followed by 
// start pause position monitoring with the requested position
TEST_F(PlayerInstanceAAMPTests, PauseAt_Position0)
{
    double pauseAtSeconds = 0;
    long long pauseAtMilliseconds = pauseAtSeconds * 1000;

    EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState()).WillRepeatedly(Return(eSTATE_PLAYING));

    EXPECT_CALL(*g_mockPrivateInstanceAAMP, StopPausePositionMonitoring("PauseAt() called")).Times(1);
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, StartPausePositionMonitoring(pauseAtMilliseconds)).Times(1);

    mPlayerInstance->PauseAt(pauseAtSeconds);
}

// Testing calling PauseAt with negative value to cancel
// Expect to call stop pause position monitoring
TEST_F(PlayerInstanceAAMPTests, PauseAt_Cancel)
{
    double pauseAtSeconds = -1.0;

    EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState()).WillRepeatedly(Return(eSTATE_PLAYING));

    EXPECT_CALL(*g_mockPrivateInstanceAAMP, StopPausePositionMonitoring("PauseAt() called")).Times(1);
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, StartPausePositionMonitoring(_)).Times(0);

    mPlayerInstance->PauseAt(pauseAtSeconds);
}

// Testing calling PauseAt when already paused
// Don't expect to start pause position monitoring
TEST_F(PlayerInstanceAAMPTests, PauseAt_AlreadyPaused)
{
    double pauseAtSeconds = 100.0;
	mPlayerInstance->aamp->pipeline_paused = true; // FIXME! violates mPlayerInstance->aamp being private
	//mPlayerInstance->SetRate(0); // logically similar, but doesn't work with below code
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState()).WillRepeatedly(Return(eSTATE_PLAYING));
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, StopPausePositionMonitoring("PauseAt() called")).Times(1);
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, StartPausePositionMonitoring(_)).Times(0);
    mPlayerInstance->PauseAt(pauseAtSeconds);
}

// Testing calling PauseAt whilst in error state
// Expect to neither call stop nor start pause position monitoring
TEST_F(PlayerInstanceAAMPTests, PauseAt_InErrorState)
{
    double pauseAtSeconds = -1.0;

    EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState()).WillRepeatedly(Return(eSTATE_ERROR));

    EXPECT_CALL(*g_mockPrivateInstanceAAMP, StopPausePositionMonitoring(_)).Times(0);
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, StartPausePositionMonitoring(_)).Times(0);

    mPlayerInstance->PauseAt(pauseAtSeconds);
}

// Testing calling PauseAt when configured to run async API
// Expect the async scheduler to be called
TEST_F(PlayerInstanceAAMPTests, PauseAtAsync)
{
    double pauseAtSeconds = 100.0;

    EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState()).WillRepeatedly(Return(eSTATE_PLAYING));

    EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_AsyncTune)).WillRepeatedly(Return(true));
    mPlayerInstance->AsyncStartStop();

    EXPECT_CALL(*g_mockAampScheduler, ScheduleTask(_)).WillOnce(Return(1));

    mPlayerInstance->PauseAt(pauseAtSeconds);
    mPrivateInstanceAAMP->mMediaFormat=eMEDIAFORMAT_DASH;
    MediaFormat type = mPrivateInstanceAAMP->GetMediaFormatType("mpd");
}

// Testing calling Tune cancels any pause position monitoring
// Expect StopPausePositionMonitoring to be called at least once 
// (internally Tune can call Stop ao possible for multiple calls)
TEST_F(PlayerInstanceAAMPTests, DISABLED_PauseAt_Tune)
{
    char mainManifestUrl[] = "";

    EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState()).WillRepeatedly(Return(eSTATE_PLAYING));

    EXPECT_CALL(*g_mockPrivateInstanceAAMP, StopPausePositionMonitoring("Tune() called")).Times(1);
	//EXPECT_CALL(*g_mockPrivateInstanceAAMP, Stop(false)).Times(1);
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, StartPausePositionMonitoring(_)).Times(0);

    mPlayerInstance->Tune(mainManifestUrl);
}

// Testing calling detach cancels any pause position monitoring
// Expect StopPausePositionMonitoring to be called at once 
TEST_F(PlayerInstanceAAMPTests, PauseAt_detach)
{
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState()).WillRepeatedly(Return(eSTATE_PLAYING));
    
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, StopPausePositionMonitoring("detach() called")).Times(1);
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, StartPausePositionMonitoring(_)).Times(0);

    mPlayerInstance->detach();
}

// Testing calling SetRate cancels any pause position monitoring
// Expect StopPausePositionMonitoring to be called at once 
TEST_F(PlayerInstanceAAMPTests, PauseAt_SetRate)
{
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState()).WillRepeatedly(Return(eSTATE_PLAYING));
    
    EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_AsyncTune)).WillRepeatedly(Return(true));
    mPlayerInstance->AsyncStartStop();

    EXPECT_CALL(*g_mockAampScheduler, ScheduleTask(_)).WillOnce(Return(1));
    mPlayerInstance->SetRate(1);
}
TEST_F(PlayerInstanceAAMPTests, SetRate_Test)
{
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState()).WillRepeatedly(Return(eSTATE_PLAYING));
    mPlayerInstance->SetRate(0);
}
// Testing calling Stop cancels any pause position monitoring
// Expect StopPausePositionMonitoring to be called at once 
TEST_F(PlayerInstanceAAMPTests, PauseAt_Stop)
{
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState()).WillRepeatedly(Return(eSTATE_PLAYING));

	//EXPECT_CALL(*g_mockPrivateInstanceAAMP, Stop(false)).Times(1);
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, StartPausePositionMonitoring(_)).Times(0);

    mPlayerInstance->Stop();
}

// Testing calling Seek cancels any pause position monitoring
// Expect StopPausePositionMonitoring to be called at once 
TEST_F(PlayerInstanceAAMPTests, PauseAt_Seek)
{
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState()).WillRepeatedly(Return(eSTATE_PLAYING));

    EXPECT_CALL(*g_mockPrivateInstanceAAMP, StopPausePositionMonitoring("Seek() called")).Times(1);
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, StartPausePositionMonitoring(_)).Times(0);

    mPlayerInstance->Seek(1000);
}
TEST_F(PlayerInstanceAAMPTests, ResetConfigurationTests) {
    
    mPlayerInstance-> ResetConfiguration();
}
TEST_F(PlayerInstanceAAMPTests, RegisterEventsTypeTests) {
    EventListener* eventListener = nullptr;
    AAMPEventType type = AAMP_EVENT_TUNED;
    mPlayerInstance-> RegisterEvent(type,eventListener);
    
}
TEST_F(PlayerInstanceAAMPTests, RegisterEventsTests) {
    EventListener* eventListener = nullptr;
    mPlayerInstance-> RegisterEvents(eventListener);
    mPrivateInstanceAAMP->RegisterAllEvents(eventListener);
}

TEST_F(PlayerInstanceAAMPTests, UnRegisterEventsTests) {
    EventListener* eventListener = nullptr;
    mPlayerInstance-> UnRegisterEvents(eventListener);
    mPrivateInstanceAAMP->UnRegisterEvents(eventListener);
}

TEST_F(PlayerInstanceAAMPTests, SetSegmentInjectFailCountTest1) {
    //checking random value
    int value = 10;
    mPrivateInstanceAAMP->mConfig->SetConfigValue(AAMP_APPLICATION_SETTING, eAAMPConfig_SegmentInjectThreshold,value);
    mPlayerInstance->SetSegmentInjectFailCount(value);
}
TEST_F(PlayerInstanceAAMPTests, SetSegmentInjectFailCountTest2) {
    //checking Maximum value
    int value = INT_MAX;
    mPrivateInstanceAAMP->mConfig->SetConfigValue(AAMP_APPLICATION_SETTING, eAAMPConfig_SegmentInjectThreshold,value);
    mPlayerInstance->SetSegmentInjectFailCount(value);
}
TEST_F(PlayerInstanceAAMPTests, SetSegmentInjectFailCountTest3) {
    //checking Minimum value
    int value = INT_MIN;
    mPrivateInstanceAAMP->mConfig->SetConfigValue(AAMP_APPLICATION_SETTING, eAAMPConfig_SegmentInjectThreshold,value);
    mPlayerInstance->SetSegmentInjectFailCount(value);
}
TEST_F(PlayerInstanceAAMPTests, SetSegmentInjectFailCountTest4) {
    //checking negative value
    int value = -50;
    mPrivateInstanceAAMP->mConfig->SetConfigValue(AAMP_APPLICATION_SETTING, eAAMPConfig_SegmentInjectThreshold,value);
    mPlayerInstance->SetSegmentInjectFailCount(value);
}
TEST_F(PlayerInstanceAAMPTests, SetSegmentDecryptFailCountTest1) {
    //checking random value
    int value = 5;
    mPrivateInstanceAAMP->mConfig->SetConfigValue(AAMP_APPLICATION_SETTING, eAAMPConfig_DRMDecryptThreshold,value);
    mPlayerInstance->SetSegmentDecryptFailCount(value);
}
TEST_F(PlayerInstanceAAMPTests, SetSegmentDecryptFailCountTest2) {
    //checking Maximum value
    int value = INT_MAX;
    mPrivateInstanceAAMP->mConfig->SetConfigValue(AAMP_APPLICATION_SETTING, eAAMPConfig_DRMDecryptThreshold,value);
    mPlayerInstance->SetSegmentDecryptFailCount(value);
}
TEST_F(PlayerInstanceAAMPTests, SetSegmentDecryptFailCountTest3) {
    //checking Minimum value
    int value = INT_MIN;
    mPrivateInstanceAAMP->mConfig->SetConfigValue(AAMP_APPLICATION_SETTING, eAAMPConfig_DRMDecryptThreshold,value);
    mPlayerInstance->SetSegmentDecryptFailCount(value);
}
TEST_F(PlayerInstanceAAMPTests, SetSegmentDecryptFailCountTest4) {
    //checking negative value
    int value = -5;
    mPrivateInstanceAAMP->mConfig->SetConfigValue(AAMP_APPLICATION_SETTING, eAAMPConfig_DRMDecryptThreshold,value);
    mPlayerInstance->SetSegmentDecryptFailCount(value);
}
TEST_F(PlayerInstanceAAMPTests, SetInitialBufferDurationTest1) {
    //checking random value
    int durationSec = 10;
    mPrivateInstanceAAMP->mConfig->SetConfigValue(AAMP_APPLICATION_SETTING, eAAMPConfig_InitialBuffer,durationSec);
    mPlayerInstance->SetInitialBufferDuration(durationSec);
}
TEST_F(PlayerInstanceAAMPTests, SetInitialBufferDurationTest2) {
    //checking Maximum value
    int durationSec = INT_MAX;
    mPrivateInstanceAAMP->mConfig->SetConfigValue(AAMP_APPLICATION_SETTING, eAAMPConfig_InitialBuffer,durationSec);
    mPlayerInstance->SetInitialBufferDuration(durationSec);
}
TEST_F(PlayerInstanceAAMPTests, SetInitialBufferDurationTest3) {
    //checking minimum value
    int durationSec = INT_MIN;
    mPrivateInstanceAAMP->mConfig->SetConfigValue(AAMP_APPLICATION_SETTING, eAAMPConfig_InitialBuffer,durationSec);
    mPlayerInstance->SetInitialBufferDuration(durationSec);
}
TEST_F(PlayerInstanceAAMPTests, SetInitialBufferDurationTest4) {
    //checking negative value
    int durationSec = -10;
    mPrivateInstanceAAMP->mConfig->SetConfigValue(AAMP_APPLICATION_SETTING, eAAMPConfig_InitialBuffer,durationSec);
    mPlayerInstance->SetInitialBufferDuration(durationSec);
}
TEST_F(PlayerInstanceAAMPTests, GetInitialBufferDurationTest) {
    
    EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_InitialBuffer))
        .WillOnce(Return(5000)); 
    int initialBufferDuration = mPlayerInstance->GetInitialBufferDuration();

    EXPECT_EQ(initialBufferDuration,5000);
}

TEST_F(PlayerInstanceAAMPTests, SetMaxPlaylistCacheSizeTest1) {
    //checking random value
    int Cachesize = 100;
    mPrivateInstanceAAMP->mConfig->SetConfigValue(AAMP_APPLICATION_SETTING, eAAMPConfig_MaxPlaylistCacheSize,Cachesize);
    mPlayerInstance->SetMaxPlaylistCacheSize(Cachesize);
}
TEST_F(PlayerInstanceAAMPTests, SetMaxPlaylistCacheSizeTest2) {
    //checking Maximum value
    int Cachesize = INT_MAX;
    mPrivateInstanceAAMP->mConfig->SetConfigValue(AAMP_APPLICATION_SETTING, eAAMPConfig_MaxPlaylistCacheSize,Cachesize);
    mPlayerInstance->SetMaxPlaylistCacheSize(Cachesize);
}
TEST_F(PlayerInstanceAAMPTests, SetMaxPlaylistCacheSizeTest3) {
    //checking Minimum value
    int Cachesize = INT_MIN;
    mPrivateInstanceAAMP->mConfig->SetConfigValue(AAMP_APPLICATION_SETTING, eAAMPConfig_MaxPlaylistCacheSize,Cachesize);
    mPlayerInstance->SetMaxPlaylistCacheSize(Cachesize);
}
TEST_F(PlayerInstanceAAMPTests, SetMaxPlaylistCacheSizeTest4) {
    //checking negative value
    int Cachesize = -100;
    mPrivateInstanceAAMP->mConfig->SetConfigValue(AAMP_APPLICATION_SETTING, eAAMPConfig_MaxPlaylistCacheSize,Cachesize);
    mPlayerInstance->SetMaxPlaylistCacheSize(Cachesize);
}
TEST_F(PlayerInstanceAAMPTests, SetRampDownLimitTest1) {
    //checking random value
    int expectedLimit = 50;
    mPrivateInstanceAAMP->mConfig->SetConfigValue(AAMP_APPLICATION_SETTING, eAAMPConfig_RampDownLimit ,expectedLimit);
    mPlayerInstance->SetRampDownLimit(expectedLimit);
}
TEST_F(PlayerInstanceAAMPTests, SetRampDownLimitTest2) {
    //checking Maximum value
    int expectedLimit = INT_MAX;
    mPrivateInstanceAAMP->mConfig->SetConfigValue(AAMP_APPLICATION_SETTING, eAAMPConfig_RampDownLimit ,expectedLimit);
    mPlayerInstance->SetRampDownLimit(expectedLimit);
}
TEST_F(PlayerInstanceAAMPTests, SetRampDownLimitTest3) {
    //checking Minimum value
    int expectedLimit = INT_MIN;
    mPrivateInstanceAAMP->mConfig->SetConfigValue(AAMP_APPLICATION_SETTING, eAAMPConfig_RampDownLimit ,expectedLimit);
    mPlayerInstance->SetRampDownLimit(expectedLimit);
}
TEST_F(PlayerInstanceAAMPTests, SetRampDownLimitTest4) {
    //checking negative value
    int expectedLimit = -10;
    mPrivateInstanceAAMP->mConfig->SetConfigValue(AAMP_APPLICATION_SETTING, eAAMPConfig_RampDownLimit ,expectedLimit);
    mPlayerInstance->SetRampDownLimit(expectedLimit);
}
TEST_F(PlayerInstanceAAMPTests, SetMinimumBitrate_ValidBitrate) {
    //checking for bitrate > 0  
    BitsPerSecond bitrate = 1000000;
    mPrivateInstanceAAMP->mConfig->SetConfigValue(AAMP_APPLICATION_SETTING, eAAMPConfig_MinBitrate ,int(bitrate));
    mPlayerInstance->SetMinimumBitrate(bitrate);
}
TEST_F(PlayerInstanceAAMPTests, SetMinimumBitrate_InvalidBitrate) {
    //checking bitrate < 0 ;
    BitsPerSecond bitrate = -500000; 
    mPlayerInstance->SetMinimumBitrate(bitrate);
}

TEST_F(PlayerInstanceAAMPTests, GetMaximumBitrate) {

    BitsPerSecond expectedMaxBitrate = 6000;
    EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_MaxBitrate))
        .WillOnce(Return(expectedMaxBitrate));

    BitsPerSecond maxBitrate = mPlayerInstance->GetMaximumBitrate();
    EXPECT_EQ(expectedMaxBitrate,maxBitrate);
}
TEST_F(PlayerInstanceAAMPTests, SetMaximumBitrateTest1) {
    //checking bitrate greater than 0 values
    BitsPerSecond expectedMinBitrate = 10000;
    mPrivateInstanceAAMP->mConfig->SetConfigValue(AAMP_APPLICATION_SETTING, eAAMPConfig_MaxBitrate ,expectedMinBitrate);
    mPlayerInstance->SetMaximumBitrate(expectedMinBitrate);
}
TEST_F(PlayerInstanceAAMPTests, SetMaximumBitrateTest2) {
    //checking bitrate less than 0 values
    BitsPerSecond expectedMinBitrate = 0;
    mPlayerInstance->SetMaximumBitrate(expectedMinBitrate);
}
TEST_F(PlayerInstanceAAMPTests, GetMinimumBitrate1) {
    //checking random values
    BitsPerSecond expectedMinBitrate = 10000;
    EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_MinBitrate))
        .WillOnce(Return(expectedMinBitrate));

    BitsPerSecond minBitrate = mPlayerInstance->GetMinimumBitrate();
    EXPECT_EQ(minBitrate,expectedMinBitrate);
}

TEST_F(PlayerInstanceAAMPTests, GetRampDownLimitTest) {

     EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_RampDownLimit))
        .WillOnce(Return(10000)); 
    int RampDownlimit = mPlayerInstance->GetRampDownLimit();

    EXPECT_EQ(RampDownlimit,10000);
}

TEST_F(PlayerInstanceAAMPTests, SetLanguageFormatTest) {
   
   bool useRole = true;
   LangCodePreference lang [] = {
    ISO639_NO_LANGCODE_PREFERENCE,
    ISO639_PREFER_3_CHAR_BIBLIOGRAPHIC_LANGCODE,
    ISO639_PREFER_3_CHAR_TERMINOLOGY_LANGCODE,
    ISO639_PREFER_2_CHAR_LANGCODE
   };
   for(LangCodePreference preference : lang)
   {
    mPrivateInstanceAAMP->mConfig->SetConfigValue(AAMP_APPLICATION_SETTING, eAAMPConfig_LanguageCodePreference ,preference);
        mPlayerInstance->SetLanguageFormat(preference, useRole);
   }
}

TEST_F(PlayerInstanceAAMPTests, SeekToLiveTest){
    //checking true condition
    bool keepPaused = true;
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState()).WillRepeatedly(Return(eSTATE_PLAYING));

    EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_AsyncTune)).WillRepeatedly(Return(true));
    mPlayerInstance->AsyncStartStop();

    EXPECT_CALL(*g_mockAampScheduler, ScheduleTask(_)).WillOnce(Return(1));

    mPlayerInstance->SeekToLive(keepPaused);
}
TEST_F(PlayerInstanceAAMPTests, SeekToLiveTest_1){
    //checking false condition
    bool keepPaused = false;
    mPlayerInstance->SeekToLive(keepPaused);
}

TEST_F(PlayerInstanceAAMPTests, SetSlowMotionPlayRateTest1){
    //checking random float value
    float rate = 0.5f;
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState()).WillRepeatedly(Return(eSTATE_PLAYING));
    mPlayerInstance->SetSlowMotionPlayRate(rate);
}
TEST_F(PlayerInstanceAAMPTests, SetSlowMotionPlayRateTest2){
    //Maximum float value
    float rate = FLT_MAX;
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState()).WillRepeatedly(Return(eSTATE_PLAYING));
    mPlayerInstance->SetSlowMotionPlayRate(rate);
}
TEST_F(PlayerInstanceAAMPTests, SetSlowMotionPlayRateTest3){
    //Minimum float value
    float rate = FLT_MIN;
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState()).WillRepeatedly(Return(eSTATE_PLAYING));
    mPlayerInstance->SetSlowMotionPlayRate(rate);
}
TEST_F(PlayerInstanceAAMPTests,SetRateAndSeekvalidTest1)
{
    int rate = 1;
    double secondsRelativeToTuneTime = AAMP_SEEK_TO_LIVE_POSITION;
    TuneType tuneType = eTUNETYPE_SEEKTOLIVE;

    EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_RepairIframes))
        .WillOnce(Return(true));
    mPlayerInstance->SetRateAndSeek(rate,secondsRelativeToTuneTime);
}
TEST_F(PlayerInstanceAAMPTests,SetRateAndSeekvalidTest2)
{
    int rate = 64;
    double secondsRelativeToTuneTime = AAMP_SEEK_TO_LIVE_POSITION;
    TuneType tuneType = eTUNETYPE_SEEKTOLIVE;

    EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_RepairIframes))
        .WillOnce(Return(true));
    mPlayerInstance->SetRateAndSeek(rate,secondsRelativeToTuneTime);
}
TEST_F(PlayerInstanceAAMPTests, SetVideoRectangleTest1) {
    //checking random values
    int x = 10; 
    int y = 20;
    int w = 640;
    int h = 480;

    mPlayerInstance->SetVideoRectangle(x, y, w, h);
}
TEST_F(PlayerInstanceAAMPTests, SetVideoRectangleTest2) {
    //checking Maximum values
    int x = INT_MAX; 
    int y = INT_MAX;
    int w = INT_MAX;
    int h = INT_MAX;

    mPlayerInstance->SetVideoRectangle(x, y, w, h);
}
TEST_F(PlayerInstanceAAMPTests, SetVideoRectangleTest3) {
    //checking Minimum values
    int x = INT_MIN; 
    int y = INT_MIN;
    int w = INT_MIN;
    int h = INT_MIN;

    mPlayerInstance->SetVideoRectangle(x, y, w, h);
}

TEST_F(PlayerInstanceAAMPTests, SetVideoZoomTest0) {
    VideoZoomMode zoom = VIDEO_ZOOM_NONE;
    mPlayerInstance->SetVideoZoom(zoom);
}
TEST_F(PlayerInstanceAAMPTests, SetVideoZoomTest1) {
	VideoZoomMode zoom = VIDEO_ZOOM_DIRECT;
	mPlayerInstance->SetVideoZoom(zoom);
}
TEST_F(PlayerInstanceAAMPTests, SetVideoZoomTest2) {
	VideoZoomMode zoom = VIDEO_ZOOM_NORMAL;
	mPlayerInstance->SetVideoZoom(zoom);
}
TEST_F(PlayerInstanceAAMPTests, SetVideoZoomTest3) {
	VideoZoomMode zoom = VIDEO_ZOOM_16X9_STRETCH;
	mPlayerInstance->SetVideoZoom(zoom);
}
TEST_F(PlayerInstanceAAMPTests, SetVideoZoomTest4) {
	VideoZoomMode zoom = VIDEO_ZOOM_4x3_PILLAR_BOX;
	mPlayerInstance->SetVideoZoom(zoom);
}
TEST_F(PlayerInstanceAAMPTests, SetVideoZoomTest5) {
	VideoZoomMode zoom = VIDEO_ZOOM_FULL;
	mPlayerInstance->SetVideoZoom(zoom);
}
TEST_F(PlayerInstanceAAMPTests, SetVideoZoomTest6) {
	VideoZoomMode zoom = VIDEO_ZOOM_GLOBAL;
	mPlayerInstance->SetVideoZoom(zoom);
}

TEST_F(PlayerInstanceAAMPTests, SetVideoMute_NotNullAamp1) {
    bool muted = true;
    mPlayerInstance->SetVideoMute(muted);
}
TEST_F(PlayerInstanceAAMPTests, SetVideoMute_NotNullAamp2) {
    bool muted = false;
    mPlayerInstance->SetVideoMute(muted);
}
TEST_F(PlayerInstanceAAMPTests, SetSubtitleMuteTest1) {
    //checking true condition
    bool muted = true; 
   mPlayerInstance->SetSubtitleMute(muted);
}
TEST_F(PlayerInstanceAAMPTests, SetSubtitleMuteTest2) {
    //checking false condition
    bool muted = false; 
   mPlayerInstance->SetSubtitleMute(muted);
}
TEST_F(PlayerInstanceAAMPTests, SetAudioVolumeTest1) {
    //checking correct volume
    int volume = 50; 
    // Call the method to be tested
   mPlayerInstance->SetAudioVolume(volume);
}
TEST_F(PlayerInstanceAAMPTests, SetAudioVolumeTest2) {
    //checking volume greater than 100
    int volume = 101; 
    // Call the method to be tested
   mPlayerInstance->SetAudioVolume(volume);
}
TEST_F(PlayerInstanceAAMPTests, SetAudioVolumeTest3) {
    //checking volume less than 0
    int volume = -1; 
    // Call the method to be tested
   mPlayerInstance->SetAudioVolume(volume);
}
TEST_F(PlayerInstanceAAMPTests, SetLanguageTest1) {
    const char* language = "english";

    // mPlayerInstance->SetPreferredLanguages(language);
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState()).WillRepeatedly(Return(eSTATE_PLAYING));

    EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_AsyncTune)).WillRepeatedly(Return(true));
    mPlayerInstance->AsyncStartStop();

    EXPECT_CALL(*g_mockAampScheduler, ScheduleTask(_)).WillOnce(Return(1));
    // Call the method to be tested
   mPlayerInstance->SetLanguage(language);
}
TEST_F(PlayerInstanceAAMPTests, SetLanguageTest2) {
    const char* language = "english";
   mPlayerInstance->SetLanguage(language);
}
TEST_F(PlayerInstanceAAMPTests, SetSubscribedTagsTest) {
     std::vector<std::string> subscribedTags = { "tag1", "tag2" };

    // Call the method to be tested
   mPlayerInstance->SetSubscribedTags(subscribedTags);
}

TEST_F(PlayerInstanceAAMPTests, SubscribeResponseHeadersTest) {
    std::vector<std::string> responseHeaders = { "header1: value1", "header2: value2" }; 
    // Call the method to be tested
   mPlayerInstance->SubscribeResponseHeaders(responseHeaders);
}

TEST_F(PlayerInstanceAAMPTests, AddEventListenerTest) {
    AAMPEventType eventType = AAMP_EVENT_TUNED;
    EventListener* eventListener = nullptr ;

    // Call the method to be tested
   mPlayerInstance->AddEventListener(eventType,eventListener);
   mPrivateInstanceAAMP->AddEventListener(eventType,eventListener);
}

TEST_F(PlayerInstanceAAMPTests, RemoveEventListenerTest) {
    AAMPEventType eventType = AAMP_EVENT_TUNED;
    EventListener* eventListener = nullptr;

    // Call the method to be tested
   mPlayerInstance->RemoveEventListener(eventType,eventListener);
    mPrivateInstanceAAMP->RemoveEventListener(eventType,eventListener);
}

TEST_F(PlayerInstanceAAMPTests, IsLiveTest) {
    // Call the method to be tested
   bool islive = mPlayerInstance->IsLive();
   EXPECT_FALSE(islive);
}

TEST_F(PlayerInstanceAAMPTests, IsJsInfoLoggingEnabledTest) {
    
    bool expectedValue = true;
    EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_JsInfoLogging))
        .WillOnce(Return(expectedValue));
   bool isLoggingEnabled = mPlayerInstance->IsJsInfoLoggingEnabled();
   EXPECT_TRUE(isLoggingEnabled);
}

TEST_F(PlayerInstanceAAMPTests, GetCurrentDRMTest) {
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState()).WillRepeatedly(Return(eSTATE_PLAYING));
    const char* expectedDrmName = "DRM";
    // DrmHelperPtr helper = mPrivateInstanceAAMP->GetCurrentDRM();
    //helper->friendlyName();
    std::string drmName =  mPlayerInstance->GetDRM();
}

TEST_F(PlayerInstanceAAMPTests, AddPageHeadersTest) {
    
    std::map<std::string, std::string> pageHeaders;
    pageHeaders["Header1"] = "pageHeaders_Value1";
    pageHeaders["Header2"] = "pageHeaders_Value2"; 

    bool expectedValue = true;
    EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_AllowPageHeaders))
        .WillOnce(Return(expectedValue));
    mPlayerInstance->AddPageHeaders(pageHeaders);
}

TEST_F(PlayerInstanceAAMPTests, AddCustomHTTPHeaderTest) {
    std::string headerName = "CustomHeader";
    std::vector<std::string> headerValue = { "headerValue1", "headerValue2" }; 
    bool isLicenseHeader = true; 
   
    // Call the method to be tested
    mPlayerInstance->AddCustomHTTPHeader(headerName, headerValue, isLicenseHeader);
}

TEST_F(PlayerInstanceAAMPTests, SetLicenseServerURLTest1) {
    
    const char* prUrl = "https://playready.example.com/";
    mPrivateInstanceAAMP->mConfig->SetConfigValue(AAMP_APPLICATION_SETTING, eAAMPConfig_PRLicenseServerUrl ,prUrl);
    mPlayerInstance->SetLicenseServerURL(prUrl, eDRM_PlayReady);
}
TEST_F(PlayerInstanceAAMPTests, SetLicenseServerURLTest2){
    const char* wvUrl = "https://widevine.example.com/";
     mPrivateInstanceAAMP->mConfig->SetConfigValue(AAMP_APPLICATION_SETTING, eAAMPConfig_PRLicenseServerUrl ,wvUrl);
   mPlayerInstance->SetLicenseServerURL(wvUrl, eDRM_WideVine);
}
TEST_F(PlayerInstanceAAMPTests, SetLicenseServerURLTest3){
    const char* ckUrl = "https://clearkey.example.com/";
     mPrivateInstanceAAMP->mConfig->SetConfigValue(AAMP_APPLICATION_SETTING, eAAMPConfig_CKLicenseServerUrl ,ckUrl);
    mPlayerInstance->SetLicenseServerURL(ckUrl, eDRM_ClearKey);
}
TEST_F(PlayerInstanceAAMPTests, SetLicenseServerURLTest4){
    const char* invalidUrl = "https://invalid.example.com/";
     mPrivateInstanceAAMP->mConfig->SetConfigValue(AAMP_APPLICATION_SETTING, eAAMPConfig_LicenseServerUrl ,invalidUrl);
   mPlayerInstance->SetLicenseServerURL(invalidUrl, eDRM_MAX_DRMSystems);
}
TEST_F(PlayerInstanceAAMPTests, SetLicenseServerURLTest5){
   const char* invalidUrl1 = "https://invalid1.example.com/";
    mPlayerInstance->SetLicenseServerURL(invalidUrl1, static_cast<DRMSystems>(-1));
}
TEST_F(PlayerInstanceAAMPTests, SetAnonymousRequestTest1) {
    //checking true condition
    bool isAnonymous = true;
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState()).WillRepeatedly(Return(eSTATE_PLAYING));
    mPrivateInstanceAAMP->mConfig->SetConfigValue(AAMP_APPLICATION_SETTING, eAAMPConfig_AnonymousLicenseRequest ,isAnonymous);
    mPlayerInstance->SetAnonymousRequest(isAnonymous);
}
TEST_F(PlayerInstanceAAMPTests, SetAnonymousRequestTest2) {
    //checking true condition
    bool isAnonymous = false;
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState()).WillRepeatedly(Return(eSTATE_PLAYING));
    mPrivateInstanceAAMP->mConfig->SetConfigValue(AAMP_APPLICATION_SETTING, eAAMPConfig_AnonymousLicenseRequest ,isAnonymous);
    mPlayerInstance->SetAnonymousRequest(isAnonymous);
}
TEST_F(PlayerInstanceAAMPTests, SetAvgBWForABRTest1) {
    //checking true condition
    bool useAvgBW  = true;
    mPrivateInstanceAAMP->mConfig->SetConfigValue(AAMP_APPLICATION_SETTING, eAAMPConfig_AvgBWForABR ,useAvgBW);
    mPlayerInstance->SetAvgBWForABR(useAvgBW);
}
TEST_F(PlayerInstanceAAMPTests, SetAvgBWForABRTest2) {
    //checking false condition
    bool useAvgBW  = false;
    mPrivateInstanceAAMP->mConfig->SetConfigValue(AAMP_APPLICATION_SETTING, eAAMPConfig_AvgBWForABR ,useAvgBW);
    mPlayerInstance->SetAvgBWForABR(useAvgBW);
}
TEST_F(PlayerInstanceAAMPTests, SetPreCacheTimeWindowTest1) {
   //checking random values
   int nTimeWindow = 30;
   EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState()).WillRepeatedly(Return(eSTATE_PLAYING));
    mPrivateInstanceAAMP->mConfig->SetConfigValue(AAMP_APPLICATION_SETTING, eAAMPConfig_PreCachePlaylistTime ,nTimeWindow);
    mPlayerInstance->SetPreCacheTimeWindow(nTimeWindow);
}
TEST_F(PlayerInstanceAAMPTests, SetPreCacheTimeWindowTest2) {
   //checking Maximum values
   int nTimeWindow = INT_MAX;
   EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState()).WillRepeatedly(Return(eSTATE_PLAYING));
    mPrivateInstanceAAMP->mConfig->SetConfigValue(AAMP_APPLICATION_SETTING, eAAMPConfig_PreCachePlaylistTime ,nTimeWindow);
    mPlayerInstance->SetPreCacheTimeWindow(nTimeWindow);
}
TEST_F(PlayerInstanceAAMPTests, SetPreCacheTimeWindowTest3) {
   //checking Minimum values
   int nTimeWindow = INT_MIN;
   EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState()).WillRepeatedly(Return(eSTATE_PLAYING));
    mPrivateInstanceAAMP->mConfig->SetConfigValue(AAMP_APPLICATION_SETTING, eAAMPConfig_PreCachePlaylistTime ,nTimeWindow);
    mPlayerInstance->SetPreCacheTimeWindow(nTimeWindow);
}
TEST_F(PlayerInstanceAAMPTests, SetPreCacheTimeWindowTest4) {
   //checking negative values
   int nTimeWindow = -30;
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState()).WillRepeatedly(Return(eSTATE_PLAYING));
    mPrivateInstanceAAMP->mConfig->SetConfigValue(AAMP_APPLICATION_SETTING, eAAMPConfig_PreCachePlaylistTime ,nTimeWindow);
    mPlayerInstance->SetPreCacheTimeWindow(nTimeWindow);
}
TEST_F(PlayerInstanceAAMPTests, SetVODTrickplayFPSTest1) {
    //checking random values
    int vodTrickplayFPS = 60;
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState()).WillRepeatedly(Return(eSTATE_PLAYING));
    mPrivateInstanceAAMP->mConfig->SetConfigValue(AAMP_APPLICATION_SETTING, eAAMPConfig_VODTrickPlayFPS ,vodTrickplayFPS);
    mPlayerInstance->SetVODTrickplayFPS(vodTrickplayFPS);
}
TEST_F(PlayerInstanceAAMPTests, SetVODTrickplayFPSTest2) {
    //checking Maximum value
    int vodTrickplayFPS = INT_MAX;
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState()).WillRepeatedly(Return(eSTATE_PLAYING));
    mPrivateInstanceAAMP->mConfig->SetConfigValue(AAMP_APPLICATION_SETTING, eAAMPConfig_VODTrickPlayFPS ,vodTrickplayFPS);
    mPlayerInstance->SetVODTrickplayFPS(vodTrickplayFPS);
}
TEST_F(PlayerInstanceAAMPTests, SetVODTrickplayFPSTest3) {
    //checking Minimum value
    int vodTrickplayFPS = INT_MIN;
     EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState()).WillRepeatedly(Return(eSTATE_PLAYING));
    mPrivateInstanceAAMP->mConfig->SetConfigValue(AAMP_APPLICATION_SETTING, eAAMPConfig_VODTrickPlayFPS ,vodTrickplayFPS);
    mPlayerInstance->SetVODTrickplayFPS(vodTrickplayFPS);
}
TEST_F(PlayerInstanceAAMPTests, SetVODTrickplayFPSTest4) {
    //checking negative value
    int vodTrickplayFPS = -10;
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState()).WillRepeatedly(Return(eSTATE_PLAYING));
    mPrivateInstanceAAMP->mConfig->SetConfigValue(AAMP_APPLICATION_SETTING, eAAMPConfig_VODTrickPlayFPS ,vodTrickplayFPS);
    mPlayerInstance->SetVODTrickplayFPS(vodTrickplayFPS);
}
TEST_F(PlayerInstanceAAMPTests, SetLinearTrickplayFPSTest1) {
    //checking random values
    int linearTrickplayFPS = 30;
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState()).WillRepeatedly(Return(eSTATE_PLAYING));
    mPrivateInstanceAAMP->mConfig->SetConfigValue(AAMP_APPLICATION_SETTING, eAAMPConfig_LinearTrickPlayFPS ,linearTrickplayFPS);
    mPlayerInstance->SetLinearTrickplayFPS(linearTrickplayFPS);
}
TEST_F(PlayerInstanceAAMPTests, SetLinearTrickplayFPSTest2) {
    //checking Maximum value
    int linearTrickplayFPS = INT_MAX;
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState()).WillRepeatedly(Return(eSTATE_PLAYING));
    mPrivateInstanceAAMP->mConfig->SetConfigValue(AAMP_APPLICATION_SETTING, eAAMPConfig_LinearTrickPlayFPS ,linearTrickplayFPS);
    mPlayerInstance->SetLinearTrickplayFPS(linearTrickplayFPS);
}
TEST_F(PlayerInstanceAAMPTests, SetLinearTrickplayFPSTest3) {
    //checking Minimum value
    int linearTrickplayFPS = INT_MIN;
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState()).WillRepeatedly(Return(eSTATE_PLAYING));
    mPrivateInstanceAAMP->mConfig->SetConfigValue(AAMP_APPLICATION_SETTING, eAAMPConfig_LinearTrickPlayFPS ,linearTrickplayFPS);
    mPlayerInstance->SetLinearTrickplayFPS(linearTrickplayFPS);
}
TEST_F(PlayerInstanceAAMPTests, SetLiveOffsetTest1) {
    //checking random value
    double liveoffset = 10.0;
    mPrivateInstanceAAMP->SetLiveOffsetAppRequest(true);
    mPrivateInstanceAAMP->mConfig->SetConfigValue(AAMP_APPLICATION_SETTING, eAAMPConfig_LiveOffset ,liveoffset);
    mPlayerInstance->SetLiveOffset(liveoffset);
}
TEST_F(PlayerInstanceAAMPTests, SetLiveOffsetTest2) {
   //checking Maximum value
    double liveoffset = DBL_MAX;
    mPrivateInstanceAAMP->SetLiveOffsetAppRequest(true);
    mPrivateInstanceAAMP->mConfig->SetConfigValue(AAMP_APPLICATION_SETTING, eAAMPConfig_LiveOffset ,liveoffset);
    mPlayerInstance->SetLiveOffset(liveoffset);
}
TEST_F(PlayerInstanceAAMPTests, SetLiveOffsetTest3) {
   //checking Minimum value
    double liveoffset = DBL_MIN;
    mPrivateInstanceAAMP->SetLiveOffsetAppRequest(true);
    mPrivateInstanceAAMP->mConfig->SetConfigValue(AAMP_APPLICATION_SETTING, eAAMPConfig_LiveOffset ,liveoffset);
    mPlayerInstance->SetLiveOffset(liveoffset);
}
TEST_F(PlayerInstanceAAMPTests, SetLiveOffset4KTest1) {
    //checking random value
    double liveoffset = 15.0;
    mPrivateInstanceAAMP->SetLiveOffsetAppRequest(true);
     mPrivateInstanceAAMP->mConfig->SetConfigValue(AAMP_APPLICATION_SETTING, eAAMPConfig_LiveOffset4K ,liveoffset);
    mPlayerInstance->SetLiveOffset4K(liveoffset);
}
TEST_F(PlayerInstanceAAMPTests, SetLiveOffset4KTest2) {
    //checking Maximum value
    double liveoffset = DBL_MAX;
    mPrivateInstanceAAMP->SetLiveOffsetAppRequest(true);
     mPrivateInstanceAAMP->mConfig->SetConfigValue(AAMP_APPLICATION_SETTING, eAAMPConfig_LiveOffset4K ,liveoffset);
    mPlayerInstance->SetLiveOffset4K(liveoffset);
}
TEST_F(PlayerInstanceAAMPTests, SetLiveOffset4KTest3) {
    //checking Minimum value
    double liveoffset = DBL_MIN;
    mPrivateInstanceAAMP->SetLiveOffsetAppRequest(true);
     mPrivateInstanceAAMP->mConfig->SetConfigValue(AAMP_APPLICATION_SETTING, eAAMPConfig_LiveOffset4K ,liveoffset);
    mPlayerInstance->SetLiveOffset4K(liveoffset);
}
TEST_F(PlayerInstanceAAMPTests, SetStallErrorCodeTest1) {
    //checking random  value
    int errorCode = 404;
    mPrivateInstanceAAMP->mConfig->SetConfigValue(AAMP_APPLICATION_SETTING, eAAMPConfig_StallErrorCode ,errorCode);
    mPlayerInstance->SetStallErrorCode(errorCode);
}
TEST_F(PlayerInstanceAAMPTests, SetStallErrorCodeTest2) {
    //checking maximum  value
    int errorCode = INT_MAX;
    mPrivateInstanceAAMP->mConfig->SetConfigValue(AAMP_APPLICATION_SETTING, eAAMPConfig_StallErrorCode ,errorCode);
    mPlayerInstance->SetStallErrorCode(errorCode);
}
TEST_F(PlayerInstanceAAMPTests, SetStallErrorCodeTest3) {
    //checking minimum  value
    int errorCode = INT_MIN;
    mPrivateInstanceAAMP->mConfig->SetConfigValue(AAMP_APPLICATION_SETTING, eAAMPConfig_StallErrorCode ,errorCode);
    mPlayerInstance->SetStallErrorCode(errorCode);
}
TEST_F(PlayerInstanceAAMPTests, SetStallTimeoutTest1) {
     //checking random  value
    int timeoutMS = 5000;
    mPrivateInstanceAAMP->mConfig->SetConfigValue(AAMP_APPLICATION_SETTING, eAAMPConfig_StallTimeoutMS ,timeoutMS);
    mPlayerInstance->SetStallTimeout(timeoutMS);
}
TEST_F(PlayerInstanceAAMPTests, SetStallTimeoutTest2) {
     //checking maximum  value
    int timeoutMS = INT_MAX;
    mPrivateInstanceAAMP->mConfig->SetConfigValue(AAMP_APPLICATION_SETTING, eAAMPConfig_StallTimeoutMS ,timeoutMS);
    mPlayerInstance->SetStallTimeout(timeoutMS);
}
TEST_F(PlayerInstanceAAMPTests, SetStallTimeoutTest3) {
     //checking minimum  value
    int timeoutMS = INT_MIN;
    mPrivateInstanceAAMP->mConfig->SetConfigValue(AAMP_APPLICATION_SETTING, eAAMPConfig_StallTimeoutMS ,timeoutMS);
    mPlayerInstance->SetStallTimeout(timeoutMS);
}
TEST_F(PlayerInstanceAAMPTests, SetReportIntervalTest) {
    mPlayerInstance->SetReportInterval(5000);
}

TEST_F(PlayerInstanceAAMPTests, SetInitFragTimeoutRetryCount1) {
    //checking random  value
    int count = 4;
    mPrivateInstanceAAMP->mConfig->SetConfigValue(AAMP_APPLICATION_SETTING, eAAMPConfig_InitFragmentRetryCount ,count);
    mPlayerInstance->SetInitFragTimeoutRetryCount(count);
}
TEST_F(PlayerInstanceAAMPTests, SetInitFragTimeoutRetryCount2) {
    //checking negative value
    int count = -1;
    mPrivateInstanceAAMP->mConfig->SetConfigValue(AAMP_APPLICATION_SETTING, eAAMPConfig_InitFragmentRetryCount ,count);
    mPlayerInstance->SetInitFragTimeoutRetryCount(count);
}
TEST_F(PlayerInstanceAAMPTests, SetInitFragTimeoutRetryCount3) {
     //checking Maximum value
    int count = INT_MAX;
    mPrivateInstanceAAMP->mConfig->SetConfigValue(AAMP_APPLICATION_SETTING, eAAMPConfig_InitFragmentRetryCount ,count);
    mPlayerInstance->SetInitFragTimeoutRetryCount(count);
}
TEST_F(PlayerInstanceAAMPTests, SetInitFragTimeoutRetryCount4) {
    //checking Minimum value
    int count = INT_MIN;
    mPrivateInstanceAAMP->mConfig->SetConfigValue(AAMP_APPLICATION_SETTING, eAAMPConfig_InitFragmentRetryCount ,count);
    mPlayerInstance->SetInitFragTimeoutRetryCount(count);
}
TEST_F(PlayerInstanceAAMPTests, GetPlaybackPositionTest) {

    double expectedPosition = 100.00;
    double position = mPlayerInstance->GetPlaybackPosition();
}
TEST_F(PlayerInstanceAAMPTests, GetPlaybackDurationTest) {
    double playbackDuration = mPlayerInstance->GetPlaybackDuration();
}
TEST_F(PlayerInstanceAAMPTests, GetIdNotNullTest1) {
    //checking for random value
    mPlayerInstance->SetId( 123 );
    int playerId = mPlayerInstance->GetId();

    EXPECT_EQ(playerId,123);
}
TEST_F(PlayerInstanceAAMPTests, GetIdNullTest2) {
	//checking for Null condition
	mPlayerInstance->aamp = nullptr; // FIXME! violates mPlayerInstance->aamp being private
	int playerId = mPlayerInstance->GetId();
	EXPECT_EQ(playerId,-1);
}
TEST_F(PlayerInstanceAAMPTests, GetIdNotNullTest3) {
    //checking for Maximum value
    mPlayerInstance->SetId(INT_MAX);
    int playerId = mPlayerInstance->GetId();

    EXPECT_EQ(playerId,INT_MAX);
}
TEST_F(PlayerInstanceAAMPTests, GetIdNotNullTest4) {
    //checking for Minimum value
    mPlayerInstance->SetId(INT_MIN);
    int playerId = mPlayerInstance->GetId();

    EXPECT_EQ(playerId,INT_MIN);
}
TEST_F(PlayerInstanceAAMPTests, GetStateTest) {
  
    AAMPPlayerState state = mPlayerInstance->GetState();
}

TEST_F(PlayerInstanceAAMPTests, SetVideoBitrateTest1) {
    //checking random bitrate value
    BitsPerSecond bitrate = 3000000; 
   mPlayerInstance->SetVideoBitrate(bitrate);
}
TEST_F(PlayerInstanceAAMPTests, SetVideoBitrateTest2) {
    //checking minimum bitrate value
    BitsPerSecond bitrate = std::numeric_limits<BitsPerSecond>::min();
   mPlayerInstance->SetVideoBitrate(bitrate);
}
TEST_F(PlayerInstanceAAMPTests, SetVideoBitrateTest3) {
    //checking maximum bitrate value
    BitsPerSecond bitrate = std::numeric_limits<BitsPerSecond>::max(); 
   mPlayerInstance->SetVideoBitrate(bitrate);
}
TEST_F(PlayerInstanceAAMPTests, SetVideoBitrateTest4) {
    //checking bitrate = 0 condition
    BitsPerSecond bitrate = 0; 
    mPlayerInstance->SetVideoBitrate(0);
}
TEST_F(PlayerInstanceAAMPTests, GetAudioBitrateTest1) {
    //random bitrate
    BitsPerSecond audioBitrate = 128000; 
    BitsPerSecond retrievedAudioBitrate = mPlayerInstance->GetAudioBitrate();
}
TEST_F(PlayerInstanceAAMPTests, GetAudioBitrateTest2) {
    //Minimum audio bitrate
    BitsPerSecond maxAudioBitrate = std::numeric_limits<BitsPerSecond>::min();
    BitsPerSecond retrievedMinAudioBitrate = mPlayerInstance->GetAudioBitrate();
}
TEST_F(PlayerInstanceAAMPTests, GetAudioBitrateTest3) {
    //Maximum audio bitrate
    BitsPerSecond maxAudioBitrate = std::numeric_limits<BitsPerSecond>::max();
    BitsPerSecond retrievedMinAudioBitrate = mPlayerInstance->GetAudioBitrate();
}
TEST_F(PlayerInstanceAAMPTests, SetAudioBitrateTest1) {
    //random Set_audio bitrate
    BitsPerSecond audioBitrate = 96000; 
    mPlayerInstance->SetAudioBitrate(audioBitrate);
}
TEST_F(PlayerInstanceAAMPTests, SetAudioBitrateTest2) {
    //Maximum Set_audio bitrate
    BitsPerSecond audioBitrate = std::numeric_limits<BitsPerSecond>::max(); 
    mPlayerInstance->SetAudioBitrate(audioBitrate);
}
TEST_F(PlayerInstanceAAMPTests, SetAudioBitrateTest3) {
    //Minimum Set_audio bitrate
    BitsPerSecond audioBitrate = std::numeric_limits<BitsPerSecond>::min(); 
    mPlayerInstance->SetAudioBitrate(audioBitrate);
}
TEST_F(PlayerInstanceAAMPTests, GetVideoZoomDefault){
	int ZoomMode = mPlayerInstance->GetVideoZoom();
	EXPECT_EQ(ZoomMode,VIDEO_ZOOM_NONE);
}
TEST_F(PlayerInstanceAAMPTests, GetVideoZoomTest1) {
    //checking zoom mode = VIDEO_ZOOM_FULL
    mPlayerInstance->SetVideoZoom(VIDEO_ZOOM_FULL);
    int ZoomMode = mPlayerInstance->GetVideoZoom();
    EXPECT_EQ(ZoomMode,VIDEO_ZOOM_FULL);
}
TEST_F(PlayerInstanceAAMPTests, GetVideoZoomTest2) {
    //checking zoom mode = VIDEO_ZOOM_NONE
    mPlayerInstance->SetVideoZoom(VIDEO_ZOOM_NONE);
    int ZoomMode = mPlayerInstance->GetVideoZoom();
    EXPECT_EQ(ZoomMode,VIDEO_ZOOM_NONE);
}
TEST_F(PlayerInstanceAAMPTests, GetVideoMuteTest1) {
    //checking true condition
	mPlayerInstance->SetVideoMute(true);
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState()).WillRepeatedly(Return(eSTATE_PLAYING));
    bool retrievedVideoMute = mPlayerInstance->GetVideoMute();
    EXPECT_TRUE(retrievedVideoMute);
}
TEST_F(PlayerInstanceAAMPTests, GetVideoMuteTest2) {
    //checking false condition
    mPlayerInstance->SetVideoMute(false);
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState()).WillRepeatedly(Return(eSTATE_PLAYING));
    bool retrievedVideoMute = mPlayerInstance->GetVideoMute();
    EXPECT_FALSE(retrievedVideoMute);
}
TEST_F(PlayerInstanceAAMPTests, GetAudioVolumeTest1) {
	mPlayerInstance->aamp->audio_volume = 50; // FIXME! violates mPlayerInstance->aamp being private
    //mPlayerInstance->SetAudioVolume(50); // logically similar, but doesn't work with below code
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState()).WillRepeatedly(Return(eSTATE_IDLE));
    int retrievedAudioVolume = mPlayerInstance->GetAudioVolume();
    EXPECT_EQ(retrievedAudioVolume,50);
}
TEST_F(PlayerInstanceAAMPTests, GetAudioVolumeTest2) {
    //checking Maximum value
	mPlayerInstance->aamp->audio_volume = INT_MAX; // FIXME! violates mPlayerInstance->aamp being private
	//mPlayerInstance->SetAudioVolume(INT_MAX); // logically similar, but doesn't work with below code
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState()).WillRepeatedly(Return(eSTATE_IDLE));
    int retrievedAudioVolume = mPlayerInstance->GetAudioVolume();
    EXPECT_EQ(retrievedAudioVolume,INT_MAX);
}
TEST_F(PlayerInstanceAAMPTests, GetAudioVolumeTest3) {
    //checking Minimum value
	mPlayerInstance->aamp->audio_volume = INT_MIN; // FIXME! violates mPlayerInstance->aamp being private
	//mPlayerInstance->SetAudioVolume(INT_MIN); // logically similar, but doesn't work with below code
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState()).WillRepeatedly(Return(eSTATE_IDLE));
    int retrievedAudioVolume = mPlayerInstance->GetAudioVolume();

    EXPECT_EQ(retrievedAudioVolume,INT_MIN);
}
TEST_F(PlayerInstanceAAMPTests, GetPlaybackRateTest_1) {
    //checking false condition
	mPlayerInstance->aamp->pipeline_paused = false; // FIXME! violates mPlayerInstance->aamp being private
	mPlayerInstance->aamp->rate = 10.9f;
    mPlayerInstance->SetRate(10.9f); // logically similar, but doesn't work with below code
    int retrievedPlaybackRate = mPlayerInstance->GetPlaybackRate();

    EXPECT_EQ(retrievedPlaybackRate,10);
}
TEST_F(PlayerInstanceAAMPTests, GetPlaybackRateTest_2) {
    //checking true condition
	mPlayerInstance->SetRate(0);
    int retrievedPlaybackRate = mPlayerInstance->GetPlaybackRate();
    EXPECT_EQ(retrievedPlaybackRate,0);
}

TEST_F(PlayerInstanceAAMPTests, GetAudioTrackTest) {
	mPlayerInstance->SetAudioTrack(1);
	int audioTrack = mPlayerInstance->GetAudioTrack();
	EXPECT_NE(audioTrack,1);
}
TEST_F(PlayerInstanceAAMPTests, GetManifestTest) {
    std::string expectedManifest = "Sample Manifest";
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState()).WillRepeatedly(Return(eSTATE_PLAYING));
    mPrivateInstanceAAMP->mMediaFormat = eMEDIAFORMAT_DASH;
    mPrivateInstanceAAMP->GetLastDownloadedManifest(expectedManifest);
    std::string result = mPlayerInstance->GetManifest();
}

TEST_F(PlayerInstanceAAMPTests, GetManifestTest2) {
    mPrivateInstanceAAMP->SetState(eSTATE_IDLE);
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState()).WillRepeatedly(Return(eSTATE_IDLE));
    mPrivateInstanceAAMP->mMediaFormat = eMEDIAFORMAT_DASH;
    std::string result = mPlayerInstance->GetManifest();
    EXPECT_EQ(result,"");
}

TEST_F(PlayerInstanceAAMPTests, GetManifestTest3) {
    mPrivateInstanceAAMP->SetState(eSTATE_ERROR);
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState()).WillRepeatedly(Return(eSTATE_ERROR));
    mPrivateInstanceAAMP->mMediaFormat = eMEDIAFORMAT_DASH;
    std::string result = mPlayerInstance->GetManifest();
    EXPECT_EQ(result,"");
}

TEST_F(PlayerInstanceAAMPTests, GetManifestTest4) {
    mPrivateInstanceAAMP->SetState(eSTATE_RELEASED);
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState()).WillRepeatedly(Return(eSTATE_RELEASED));
    mPrivateInstanceAAMP->mMediaFormat = eMEDIAFORMAT_DASH;
    std::string result = mPlayerInstance->GetManifest();
    EXPECT_EQ(result,"");
}

TEST_F(PlayerInstanceAAMPTests, GetManifestTest5) {
    mPrivateInstanceAAMP->SetState(eSTATE_STOPPED);
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState()).WillRepeatedly(Return(eSTATE_STOPPED));
    mPrivateInstanceAAMP->mMediaFormat = eMEDIAFORMAT_DASH;
    std::string result = mPlayerInstance->GetManifest();
    EXPECT_EQ(result,"");
}

TEST_F(PlayerInstanceAAMPTests, SetInitialBitrateTest) {
    BitsPerSecond bitrate = 1500; 
    mPlayerInstance->SetInitialBitrate(bitrate);
}

TEST_F(PlayerInstanceAAMPTests, GetInitialBitrateTest) {
    BitsPerSecond expectedBitrate = 2500; 
    BitsPerSecond result = mPlayerInstance->GetInitialBitrate();
}
TEST_F(PlayerInstanceAAMPTests, SetInitialBitrate4KTest) {
    BitsPerSecond bitrate4K = 25000000; 
    mPlayerInstance->SetInitialBitrate4K(bitrate4K);
}
TEST_F(PlayerInstanceAAMPTests, GetInitialBitrate4kTest) {
    BitsPerSecond expectedBitrate = 25000000; 
    EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_DefaultBitrate4K))
        .WillOnce(Return(expectedBitrate)); 
    BitsPerSecond result = mPlayerInstance->GetInitialBitrate4k();

    EXPECT_EQ(result,expectedBitrate);
}
TEST_F(PlayerInstanceAAMPTests, SetNetworkTimeoutTest1) {
    //checking random value
    double timeout = 10.0; 
    mPlayerInstance->SetNetworkTimeout(timeout);
}
TEST_F(PlayerInstanceAAMPTests, SetNetworkTimeoutTest2) {
    //checking Maximum value
    double timeout = DBL_MAX; 
    mPlayerInstance->SetNetworkTimeout(timeout);
}
TEST_F(PlayerInstanceAAMPTests, SetNetworkTimeoutTest3) {
    //checking Minimum value
    double timeout = DBL_MIN; 
    mPlayerInstance->SetNetworkTimeout(timeout);
}
TEST_F(PlayerInstanceAAMPTests, SetManifestTimeoutTest1) {
    //checking random value
    double timeout = 5.0; 
    mPlayerInstance->SetManifestTimeout(timeout);
}
TEST_F(PlayerInstanceAAMPTests, SetManifestTimeoutTest2) {
    //checking Maximum value
    double timeout = DBL_MAX; 
    mPlayerInstance->SetManifestTimeout(timeout);
}
TEST_F(PlayerInstanceAAMPTests, SetManifestTimeoutTest3) {
    //checking Minimum value
    double timeout = DBL_MIN;
    mPlayerInstance->SetManifestTimeout(timeout);
}
TEST_F(PlayerInstanceAAMPTests, SetPlaylistTimeoutTest1) {
    //checking random value
    double timeout = 8.0; 
    mPlayerInstance->SetPlaylistTimeout(timeout);
}
TEST_F(PlayerInstanceAAMPTests, SetPlaylistTimeoutTest2) {
    //checking Maximum value
    double timeout = DBL_MAX; 
    mPlayerInstance->SetPlaylistTimeout(timeout);
}
TEST_F(PlayerInstanceAAMPTests, SetPlaylistTimeoutTest3) {
    //checking Minimum value
    double timeout = DBL_MIN; 
    mPlayerInstance->SetPlaylistTimeout(timeout);
}
TEST_F(PlayerInstanceAAMPTests, SetDownloadBufferSizeTest1) {
    //checking random value
    int buffersize = 1024; 
    mPlayerInstance->SetDownloadBufferSize(buffersize);
}
TEST_F(PlayerInstanceAAMPTests, SetDownloadBufferSizeTest2) {
    //checking Maximum value
    int buffersize = INT_MAX; 
    mPlayerInstance->SetDownloadBufferSize(buffersize);
}
TEST_F(PlayerInstanceAAMPTests, SetDownloadBufferSizeTest3) {
    //checking Minimum value
    int buffersize = INT_MIN; 
    mPlayerInstance->SetDownloadBufferSize(buffersize);
}
TEST_F(PlayerInstanceAAMPTests, SetDownloadBufferSizeTest4) {
    //checking negative value
    int buffersize = -500; 
    mPlayerInstance->SetDownloadBufferSize(buffersize);
}
TEST_F(PlayerInstanceAAMPTests, SetPreferredDRMTest)
{
    //checking drmtype not equal to eDRM_NONE using loop
    DRMSystems drmtype_list [] = {    
	eDRM_WideVine,
	eDRM_PlayReady,
	eDRM_CONSEC_agnostic,
	eDRM_Adobe_Access,
	eDRM_Vanilla_AES,
	eDRM_ClearKey,
	eDRM_MAX_DRMSystems
    };
   for(DRMSystems drmtype : drmtype_list){
     mPlayerInstance->SetPreferredDRM(drmtype);}
}
TEST_F(PlayerInstanceAAMPTests, SetPreferredDRMNoneTest1)
{
    //checking drmtype equal to eDRM_NONE
    DRMSystems drmtype = eDRM_NONE;
    mPlayerInstance->SetPreferredDRM(drmtype);
}
TEST_F(PlayerInstanceAAMPTests, SetDisable4KTest1)
{
    //checking true condition
    bool Value =  true;
    mPlayerInstance->SetDisable4K(Value);
}
TEST_F(PlayerInstanceAAMPTests, SetDisable4KTest2)
{
    //checking false condition
    bool Value =  false;
    mPlayerInstance->SetDisable4K(Value);
}
TEST_F(PlayerInstanceAAMPTests, SetBulkTimedMetaReportTest1)
{
    //checking true condition
    bool bValue =  true;
   mPlayerInstance->SetBulkTimedMetaReport(bValue);
}
TEST_F(PlayerInstanceAAMPTests, SetBulkTimedMetaReportTest2)
{
    //checking false condition
    bool bValue =  false;
   mPlayerInstance->SetBulkTimedMetaReport(bValue);
}
TEST_F(PlayerInstanceAAMPTests, SetRetuneForUnpairedDiscontinuityTest1)
{
    //checking true condition
    bool bValue =  true;
    mPlayerInstance->SetRetuneForUnpairedDiscontinuity(bValue);
}
TEST_F(PlayerInstanceAAMPTests, SetRetuneForUnpairedDiscontinuityTest2)
{
    //checking false condition
    bool bValue =  false;
    mPlayerInstance->SetRetuneForUnpairedDiscontinuity(bValue);
}
TEST_F(PlayerInstanceAAMPTests, SetRetuneForGSTInternalErrorTest1)
{
    //checking true condition
    bool bValue =  true;
    mPlayerInstance->SetRetuneForGSTInternalError(bValue);
}
TEST_F(PlayerInstanceAAMPTests, SetRetuneForGSTInternalErrorTest2)
{
    //checking false condition
    bool bValue =  false;
    mPlayerInstance->SetRetuneForGSTInternalError(bValue);
}
TEST_F(PlayerInstanceAAMPTests, SetAlternateContentsTest1)
{
    //checking random string
    std::string adBreakId = "adBreak1";
    std::string adId = "ad1";
    std::string url = "http://example.com/ad1";

    mPlayerInstance->SetAlternateContents(adBreakId, adId, url);
}
TEST_F(PlayerInstanceAAMPTests, SetAlternateContentsTest)
{
    //checking long string with 100000 character 
    std::string adBreakId(100000,'a');
    std::string adId(100000,'b');
    std::string url(100000,'h');

    mPlayerInstance->SetAlternateContents(adBreakId, adId, url);
}
TEST_F(PlayerInstanceAAMPTests, SetNetworkProxyTest)
{
    const char* proxy = "http://example-proxy.com:8080";

    mPlayerInstance->SetNetworkProxy(proxy);
}

TEST_F(PlayerInstanceAAMPTests, SetLicenseReqProxyTest)
{
    const char* licenseProxy = "http://license-proxy.com:8080";

    mPlayerInstance->SetLicenseReqProxy(licenseProxy);
}
TEST_F(PlayerInstanceAAMPTests, SetDownloadStallTimeoutTest1)
{
    //checking random values
    int stallTimeout = 10;
    mPlayerInstance->SetDownloadStallTimeout(stallTimeout);
}
TEST_F(PlayerInstanceAAMPTests, SetDownloadStallTimeoutTest2)
{
    //checking Maximum values
    int stallTimeout = INT_MAX;
    mPlayerInstance->SetDownloadStallTimeout(stallTimeout);
}
TEST_F(PlayerInstanceAAMPTests, SetDownloadStallTimeoutTest3)
{
    //checking Minimum values
    int stallTimeout = INT_MIN;
    mPlayerInstance->SetDownloadStallTimeout(stallTimeout);
}
TEST_F(PlayerInstanceAAMPTests, SetDownloadStallTimeoutTest4)
{
    //checking negative values
    int stallTimeout = -100;
    mPlayerInstance->SetDownloadStallTimeout(stallTimeout);
}
TEST_F(PlayerInstanceAAMPTests, SetDownloadStartTimeoutTest1)
{
    //checking Maximum value
    int startTimeout = INT_MAX;
    mPlayerInstance->SetDownloadStartTimeout(startTimeout);
}
TEST_F(PlayerInstanceAAMPTests, SetDownloadStartTimeoutTest2)
{
    //checking Minimum value
    int startTimeout = INT_MIN;
    mPlayerInstance->SetDownloadStartTimeout(startTimeout);
}
TEST_F(PlayerInstanceAAMPTests, SetDownloadStartTimeoutTest3)
{
    //checking negative value
    int startTimeout = -10;
    mPlayerInstance->SetDownloadStartTimeout(startTimeout);
}
TEST_F(PlayerInstanceAAMPTests, SetDownloadLowBWTimeoutTest1)
{
    //checking random value
    int lowBWTimeout = 10;
    mPlayerInstance->SetDownloadLowBWTimeout(lowBWTimeout);
}
TEST_F(PlayerInstanceAAMPTests, SetDownloadLowBWTimeoutTest2)
{
    //checking max value
    int lowBWTimeout = INT_MAX;
    mPlayerInstance->SetDownloadLowBWTimeout(lowBWTimeout);
}
TEST_F(PlayerInstanceAAMPTests, SetDownloadLowBWTimeoutTest3)
{
    //checking min value
    int lowBWTimeout = INT_MIN;
    mPlayerInstance->SetDownloadLowBWTimeout(lowBWTimeout);
}
TEST_F(PlayerInstanceAAMPTests, SetDownloadLowBWTimeoutTest4)
{
    //checking negative value
    int lowBWTimeout = -1;
    mPlayerInstance->SetDownloadLowBWTimeout(lowBWTimeout);
}
TEST_F(PlayerInstanceAAMPTests, SetPreferredSubtitleLanguageIdleState1) 
{
    //checking random value
    const char* language = "English";  
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState()).WillRepeatedly(Return(eSTATE_PLAYING));
    mPlayerInstance->SetPreferredSubtitleLanguage(language);
}
TEST_F(PlayerInstanceAAMPTests, SetPreferredSubtitleLanguageIdleState2) 
{
    //cchecking maximum char
	char language[] = { CHAR_MAX, 0x00 };
    mPlayerInstance->SetPreferredSubtitleLanguage(language);
}
TEST_F(PlayerInstanceAAMPTests, SetPreferredSubtitleLanguageIdleState3) 
{
    //cchecking minimum char
	char language[] = { CHAR_MIN, 0x00 };  
    mPlayerInstance->SetPreferredSubtitleLanguage(language);
}
TEST_F(PlayerInstanceAAMPTests, SetWesterosSinkConfigTest1)
{
    //checking true condition
    bool bValue = true;
    mPlayerInstance->SetWesterosSinkConfig(bValue);
}
TEST_F(PlayerInstanceAAMPTests, SetWesterosSinkConfigTest2)
{
    //checking false condition
    bool bValue = false;
    mPlayerInstance->SetWesterosSinkConfig(bValue);
}
TEST_F(PlayerInstanceAAMPTests, SetLicenseCachingTest1)
{
    //checking true condition
    bool bValue = true;
    mPlayerInstance->SetLicenseCaching(bValue);
}
TEST_F(PlayerInstanceAAMPTests, SetLicenseCachingTest2)
{
    //checking false condition
    bool bValue = false;
    mPlayerInstance->SetLicenseCaching(bValue);
}
TEST_F(PlayerInstanceAAMPTests, SetOutputResolutionCheckTest1)
{
    //checking true condition
    bool bValue = true;
    mPlayerInstance->SetOutputResolutionCheck(bValue);
}
TEST_F(PlayerInstanceAAMPTests, SetOutputResolutionCheckTest2)
{
    //checking false condition
    bool bValue = false;
    mPlayerInstance->SetOutputResolutionCheck(bValue);
}
TEST_F(PlayerInstanceAAMPTests, SetMatchingBaseUrlConfigTest1)
{
    //checking true condition
    bool bValue = true;
    mPlayerInstance->SetMatchingBaseUrlConfig(bValue);
}
TEST_F(PlayerInstanceAAMPTests, SetMatchingBaseUrlConfigTest2)
{
    //checking false condition
    bool bValue = false;
    mPlayerInstance->SetMatchingBaseUrlConfig(bValue);
}
TEST_F(PlayerInstanceAAMPTests, SetNewABRConfigTest)
{
    bool bValue = true;
    mPlayerInstance->SetNewABRConfig(bValue);
}
TEST_F(PlayerInstanceAAMPTests, SetPropagateUriParametersTest)
{
    bool bValue = true;
    mPlayerInstance->SetPropagateUriParameters(bValue);
}

TEST_F(PlayerInstanceAAMPTests, ApplyArtificialDownloadDelayTest1)
{
    //checking for random value
    unsigned int DownloadDelayInMs = 100;
    mPlayerInstance->ApplyArtificialDownloadDelay(DownloadDelayInMs);
}
TEST_F(PlayerInstanceAAMPTests, ApplyArtificialDownloadDelayTest2)
{
    //checking for Maximum value
    unsigned int DownloadDelayInMs = UINT_MAX;
    mPlayerInstance->ApplyArtificialDownloadDelay(DownloadDelayInMs);
}
TEST_F(PlayerInstanceAAMPTests, SetSslVerifyPeerConfigTest1)
{
    //checking for true condition
    bool bValue = true;
    mPlayerInstance->SetSslVerifyPeerConfig(bValue);
}
TEST_F(PlayerInstanceAAMPTests, SetSslVerifyPeerConfigTest2)
{
    //checking for false condition
    bool bValue = false;
    mPlayerInstance->SetSslVerifyPeerConfig(bValue);
}
TEST_F(PlayerInstanceAAMPTests, SetAudioTrackTest1)
{
    //checking random values
    std::string language = "eng";
    std::string rendition = "main";
    std::string type = "audio";
    std::string codec = "aac";
    unsigned int channel = 2;
    std::string label = "English";

    EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState()).WillRepeatedly(Return(eSTATE_PLAYING));

    EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_AsyncTune)).WillRepeatedly(Return(true));
    mPlayerInstance->AsyncStartStop();

    EXPECT_CALL(*g_mockAampScheduler, ScheduleTask(_)).WillOnce(Return(1));
    mPlayerInstance->SetAudioTrack(language, rendition, type, codec, channel, label);
}
TEST_F(PlayerInstanceAAMPTests, SetAudioTrackTest2)
{
    //checking for long string
    std::string language(1000000,'L');
    std::string rendition(1000000,'R');
    std::string type(1000000,'T');
    std::string codec(1000000,'C');
    unsigned int channel = UINT_MAX;
    std::string label(1000000,'L');

    mPlayerInstance->SetAudioTrack(language, rendition, type, codec, channel, label);
}
TEST_F(PlayerInstanceAAMPTests, SetAudioTrackTest3)
{
    //checking for empty string
    std::string language = "";
    std::string rendition = "";
    std::string type = "";
    std::string codec = "";
    unsigned int channel = UINT_MAX;
    std::string label = "";

    mPlayerInstance->SetAudioTrack(language, rendition, type, codec, channel, label);
}
TEST_F(PlayerInstanceAAMPTests, SetPreferredCodecTest)
{
    const char* codecList = "codec1,codec2,codec3";
    mPlayerInstance->SetPreferredCodec(codecList);
}

TEST_F(PlayerInstanceAAMPTests, SetPreferredLabelsTest)
{
    const char* labelList = "label1,label2,label3";
    mPlayerInstance->SetPreferredLabels(labelList);
}

TEST_F(PlayerInstanceAAMPTests, SetPreferredRenditionsTest)
{
    const char* renditionList = "rendition1,rendition2,rendition3";
    mPlayerInstance->SetPreferredRenditions(renditionList);
}
TEST_F(PlayerInstanceAAMPTests, GetPreferredTextPropertiesTest)
{
    std::string result = "TextProperties";
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState()).WillRepeatedly(Return(eSTATE_PLAYING));
   std::string textProperties = mPlayerInstance->GetPreferredTextProperties();
   EXPECT_STREQ(result.c_str(),textProperties.c_str());

}
TEST_F(PlayerInstanceAAMPTests, GetPreferredAudioPropertiesTest)
{
    std::string audio_result = "AudioProperties";
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState()).WillRepeatedly(Return(eSTATE_PLAYING));
   std::string audioProperties = mPlayerInstance->GetPreferredAudioProperties();
    EXPECT_STREQ(audio_result.c_str(),audioProperties.c_str());
}
TEST_F(PlayerInstanceAAMPTests, SetPreferredLanguagesTest)
{
    mPlayerInstance->SetPreferredLanguages("en,es,fr", "HD", "video", "h264", "main", nullptr);
}
TEST_F(PlayerInstanceAAMPTests, SetPreferredTextLanguagesTest)
{
    mPlayerInstance->SetPreferredTextLanguages("en,es,fr");
}
TEST_F(PlayerInstanceAAMPTests, GetPreferredDRMTest)
{
    DRMSystems expectedDRM = eDRM_WideVine;
    DRMSystems result = mPlayerInstance->GetPreferredDRM();
}
TEST_F(PlayerInstanceAAMPTests, GetPreferredLanguagesTest)
{
    mPrivateInstanceAAMP->preferredLanguagesString = "english,french,spanish";

    std::string preferredLanguages = mPlayerInstance->GetPreferredLanguages();

}
TEST_F(PlayerInstanceAAMPTests, GetPreferredLanguagesEmptyTest)
{
    std::string lang = mPlayerInstance->GetPreferredLanguages();
}
TEST_F(PlayerInstanceAAMPTests, SetNewAdBreakerConfigTest1)
{
    //checking true condition
    bool bValue = true;
    mPlayerInstance->SetNewAdBreakerConfig(bValue);
}
TEST_F(PlayerInstanceAAMPTests, SetNewAdBreakerConfigTest2)
{
    //checking false condition
    bool bValue = false;
    mPlayerInstance->SetNewAdBreakerConfig(bValue);
}
TEST_F(PlayerInstanceAAMPTests, SetVideoTracksTest)
{
    std::vector<BitsPerSecond> bitrates = {500000, 1000000, 1500000};
    mPlayerInstance->SetVideoTracks(bitrates);
}
TEST_F(PlayerInstanceAAMPTests, SetAppNameTest)
{
    mPlayerInstance->SetAppName("MyApp");
}

TEST_F(PlayerInstanceAAMPTests, SetNativeCCRenderingTest1)
{
    //checking true condition
    bool enable = true;
    mPlayerInstance->SetNativeCCRendering(enable);
}
TEST_F(PlayerInstanceAAMPTests, SetNativeCCRenderingTest2)
{
    //checking false condition
    bool enable = false;
    mPlayerInstance->SetNativeCCRendering(enable);
}
TEST_F(PlayerInstanceAAMPTests, SetTuneEventConfigTest1)
{
    //checking for random value
    int tuneEventType = 1;
    mPlayerInstance->SetTuneEventConfig(tuneEventType);
}
TEST_F(PlayerInstanceAAMPTests, SetTuneEventConfigTest2)
{
    //checking Maximum value
    int tuneEventType = INT_MAX;
    mPlayerInstance->SetTuneEventConfig(tuneEventType);
}
TEST_F(PlayerInstanceAAMPTests, SetTuneEventConfigTest3)
{
    //checking for Minimum value
    int tuneEventType = INT_MIN;
    mPlayerInstance->SetTuneEventConfig(tuneEventType);
}
TEST_F(PlayerInstanceAAMPTests, SetTuneEventConfigTest4)
{
    //checking for negative value
    int tuneEventType = -10;
    mPlayerInstance->SetTuneEventConfig(tuneEventType);
}

TEST_F(PlayerInstanceAAMPTests, EnableVideoRectangleTest)
{
    mPlayerInstance->EnableVideoRectangle(true);
}

TEST_F(PlayerInstanceAAMPTests, EnableVideoRectangle_1Test)
{
     bool expectedValue = true;
    EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_UseWesterosSink)).WillOnce(Return(expectedValue));
    mPlayerInstance->EnableVideoRectangle(false);
}
TEST_F(PlayerInstanceAAMPTests, EnableVideoRectangle_2Test)
{
     bool expectedValue = false;
    EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_UseWesterosSink)).WillOnce(Return(expectedValue));
    mPlayerInstance->EnableVideoRectangle(false);
}

TEST_F(PlayerInstanceAAMPTests, SetAudioTrackTest)
{
    mPlayerInstance->SetAudioTrack(1);
}

TEST_F(PlayerInstanceAAMPTests, GetTextTrackTest)
{
    int trackId = mPlayerInstance->GetTextTrack();
}

TEST_F(PlayerInstanceAAMPTests, SetCCStatusTest)
{
    mPlayerInstance->SetCCStatus(true);
    EXPECT_FALSE(mPlayerInstance->GetCCStatus()); 
}

TEST_F(PlayerInstanceAAMPTests, SetTextStyleTest)
{
    const std::string options = "SampleTextStyle";
    mPlayerInstance->SetTextStyle(options);
}
TEST_F(PlayerInstanceAAMPTests, GetTextStyleTest){
    const std::string expectedTextStyle = "TextStyle";
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState()).WillRepeatedly(Return(eSTATE_PLAYING));
    std::string result = mPlayerInstance->GetTextStyle();
    EXPECT_STREQ(expectedTextStyle.c_str(),result.c_str());
}

TEST_F(PlayerInstanceAAMPTests, SetInitRampdownLimitTest1)
{
    //checking random value
    int limit = 10;
    mPlayerInstance->SetInitRampdownLimit(limit);
}
TEST_F(PlayerInstanceAAMPTests, SetInitRampdownLimitTest2)
{
    //checking Maximum value
    int limit = INT_MAX;
    mPlayerInstance->SetInitRampdownLimit(limit);
}
TEST_F(PlayerInstanceAAMPTests, SetInitRampdownLimitTest3)
{
    //checking Minimum value
    int limit = INT_MIN;
    mPlayerInstance->SetInitRampdownLimit(limit);
}
TEST_F(PlayerInstanceAAMPTests, SetInitRampdownLimitTest4)
{
    //checking negative value
    int limit = -1;
    mPlayerInstance->SetInitRampdownLimit(limit);
}
TEST_F(PlayerInstanceAAMPTests, SetThumbnailTrackTest1)
{
    //checking random value
    int thumbIndex = 2;
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState()).WillRepeatedly(Return(eSTATE_PLAYING));
    bool result = mPlayerInstance->SetThumbnailTrack(thumbIndex);
    EXPECT_FALSE(result);
}
TEST_F(PlayerInstanceAAMPTests, SetThumbnailTrackTest2)
{
    //Checking Maximum value
    int thumbIndex = INT_MAX;
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState()).WillRepeatedly(Return(eSTATE_PLAYING));
    bool result = mPlayerInstance->SetThumbnailTrack(thumbIndex);
}
TEST_F(PlayerInstanceAAMPTests, SetThumbnailTrackTest3)
{
    //Checking Minimum value
    int thumbIndex = INT_MIN;
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState()).WillRepeatedly(Return(eSTATE_PLAYING));
    bool result = mPlayerInstance->SetThumbnailTrack(thumbIndex);
}
TEST_F(PlayerInstanceAAMPTests, EnableSeekableRangeTest1)
{
    //checking true condition
    bool bValue = true;
    mPlayerInstance->EnableSeekableRange(bValue);
}
TEST_F(PlayerInstanceAAMPTests, EnableSeekableRangeTest2)
{
    //checking false condition
    bool bValue = false;
    mPlayerInstance->EnableSeekableRange(bValue);
}
TEST_F(PlayerInstanceAAMPTests, SetReportVideoPTSTest1)
{
    //checking false condition
    bool bValue = true;
    mPlayerInstance->SetReportVideoPTS(bValue);
}
TEST_F(PlayerInstanceAAMPTests, SetReportVideoPTSTest2)
{
    //checking false condition
    bool bValue = false;
    mPlayerInstance->SetReportVideoPTS(bValue);
}
TEST_F(PlayerInstanceAAMPTests, DisableContentRestrictionsTest1)
{
    //checking random values
    long grace = 600;
    long time = 3600;
    bool eventChange = true;
    mPlayerInstance->DisableContentRestrictions(grace,time,eventChange);
}
TEST_F(PlayerInstanceAAMPTests, DisableContentRestrictionsTest2)
{
    //checking Maximum values along with false condition
    long grace = LONG_MAX;
    long time = LONG_MAX;
    bool eventChange = false;
    mPlayerInstance->DisableContentRestrictions(grace,time,eventChange);
}
TEST_F(PlayerInstanceAAMPTests, DisableContentRestrictionsTest3)
{
    //checking Minimum values along with false condition
    long grace = LONG_MIN;
    long time = LONG_MIN;
    bool eventChange = false;
    mPlayerInstance->DisableContentRestrictions(grace,time,eventChange);
}
TEST_F(PlayerInstanceAAMPTests, EnableContentRestrictionsTest)
{
    mPlayerInstance->EnableContentRestrictions();
}
TEST_F(PlayerInstanceAAMPTests, ManageAsyncTuneConfigTest)
{
    const char* mainManifestUrl = "http://example.com/main.mpd";
    mPlayerInstance->ManageAsyncTuneConfig(mainManifestUrl);
}
TEST_F(PlayerInstanceAAMPTests, SetAsyncTuneConfigTest1)
{
    //checking true condition
    bool bValue = true;
    mPlayerInstance->SetAsyncTuneConfig(bValue);
}
TEST_F(PlayerInstanceAAMPTests, SetAsyncTuneConfigTest2)
{
    //checking false condition
    bool bValue = false;
    mPlayerInstance->SetAsyncTuneConfig(bValue);
}
TEST_F(PlayerInstanceAAMPTests, PersistBitRateOverSeekTest1)
{
    //checking true condition
    bool bValue = true;
    mPlayerInstance->PersistBitRateOverSeek(bValue);
}
TEST_F(PlayerInstanceAAMPTests, PersistBitRateOverSeekTest2)
{
    //checking false condition
    bool bValue = false;
    mPlayerInstance->PersistBitRateOverSeek(bValue);
}
TEST_F(PlayerInstanceAAMPTests, SetPausedBehaviorTest)
{
    //checking random value
    int behavior = 3;
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState()).WillRepeatedly(Return(eSTATE_PLAYING));
    mPrivateInstanceAAMP->mConfig->SetConfigValue(AAMP_APPLICATION_SETTING, eAAMPConfig_LivePauseBehavior ,behavior);
    mPlayerInstance->SetPausedBehavior(behavior);
}
TEST_F(PlayerInstanceAAMPTests, SetPausedBehaviorTest1)
{
    //checking boundary value
    int behavior = 4;
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState()).WillRepeatedly(Return(eSTATE_PLAYING));
    mPlayerInstance->SetPausedBehavior(behavior);
}
TEST_F(PlayerInstanceAAMPTests, SetPausedBehaviorTest2)
{
    //checking Maximum value
    int behavior = INT_MAX;
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState()).WillRepeatedly(Return(eSTATE_PLAYING));
    mPlayerInstance->SetPausedBehavior(behavior);
}
TEST_F(PlayerInstanceAAMPTests, SetPausedBehaviorTest3)
{
    //checking minimum value
    int behavior = INT_MIN;
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState()).WillRepeatedly(Return(eSTATE_PLAYING));
    mPlayerInstance->SetPausedBehavior(behavior);
}
TEST_F(PlayerInstanceAAMPTests, SetUseAbsoluteTimelineTest1)
{
    //checking true condition
    bool configState = true;
    mPlayerInstance->SetUseAbsoluteTimeline(configState);
}
TEST_F(PlayerInstanceAAMPTests, SetUseAbsoluteTimelineTest)
{
    //checking false condition
    bool configState = false;
    mPlayerInstance->SetUseAbsoluteTimeline(configState);
}
TEST_F(PlayerInstanceAAMPTests, SetRepairIframesTest1)
{
    //checking false condition
    bool configState = false;
    mPlayerInstance->SetRepairIframes(configState);
}
TEST_F(PlayerInstanceAAMPTests, SetRepairIframesTest2)
{
    //checking true condition
    bool configState = true;
    mPlayerInstance->SetRepairIframes(configState);
}
TEST_F(PlayerInstanceAAMPTests, XRESupportedTuneTest1)
{
    //checking true condition
    bool xreSupported = true;
    mPlayerInstance->XRESupportedTune(xreSupported);
}
TEST_F(PlayerInstanceAAMPTests, XRESupportedTuneTest2)
{
    //checking false condition
    bool xreSupported = false;
    mPlayerInstance->XRESupportedTune(xreSupported);
}
TEST_F(PlayerInstanceAAMPTests, SetLicenseCustomDataTest)
{
    const char* customData = "customData"; 
    mPlayerInstance->SetLicenseCustomData(customData);
}
TEST_F(PlayerInstanceAAMPTests, SetContentProtectionDataUpdateTimeoutTest1)
{
    //checking random value
    int timeout = 50;
    mPlayerInstance->SetContentProtectionDataUpdateTimeout(timeout);
}
TEST_F(PlayerInstanceAAMPTests, SetContentProtectionDataUpdateTimeoutTest2)
{
    //checking Maximum value
    int timeout = INT_MAX;
    mPlayerInstance->SetContentProtectionDataUpdateTimeout(timeout);
}
TEST_F(PlayerInstanceAAMPTests, SetContentProtectionDataUpdateTimeoutTest3)
{
    //checking Minimum value
    int timeout = INT_MIN;
    mPlayerInstance->SetContentProtectionDataUpdateTimeout(timeout);
}
TEST_F(PlayerInstanceAAMPTests, SetContentProtectionDataUpdateTimeoutTest4)
{
    //checking negative value
    int timeout = -10;
    mPlayerInstance->SetContentProtectionDataUpdateTimeout(timeout);
}
TEST_F(PlayerInstanceAAMPTests, SetRuntimeDRMConfigSupportTest1)
{
    //checking true condition
    bool DynamicDRMSupported = true;
    mPlayerInstance->SetRuntimeDRMConfigSupport(DynamicDRMSupported);
}
TEST_F(PlayerInstanceAAMPTests, SetRuntimeDRMConfigSupportTest2)
{
    //checking false condition
    bool DynamicDRMSupported = false;
    mPlayerInstance->SetRuntimeDRMConfigSupport(DynamicDRMSupported);
}
TEST_F(PlayerInstanceAAMPTests, GetVideoRectangleTest) {
	std::string expectedRectangle = "VideoRectangle";
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState()).WillRepeatedly(Return(eSTATE_PLAYING));
	std::string videoRectangle = mPlayerInstance->GetVideoRectangle();
	EXPECT_STREQ(expectedRectangle.c_str(),videoRectangle.c_str());
}
TEST_F(PlayerInstanceAAMPTests, GetThumbnailsTest)
{
    std::string expectedThumbnail = "Thumbnail";
    double tStart = 10.0;
    double tEnd = 20.0;
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState()).WillRepeatedly(Return(eSTATE_PLAYING));

    std::string result = mPlayerInstance->GetThumbnails(tStart, tEnd);
    EXPECT_STREQ(result.c_str(),expectedThumbnail.c_str());
}

TEST_F(PlayerInstanceAAMPTests, SetStereoOnlyPlaybackTest1)
{
    //checking true condtion
    bool bValue = true;
    mPlayerInstance->SetStereoOnlyPlayback(bValue);
}
TEST_F(PlayerInstanceAAMPTests, SetStereoOnlyPlaybackTest2)
{
    //checking false condition
    bool bValue = false;
    mPlayerInstance->SetStereoOnlyPlayback(bValue);
}
TEST_F(PlayerInstanceAAMPTests, SetSessionTokenTest1)
{
    //checking random strings
    std::string sessionToken = "my_session_token";
    mPlayerInstance->SetSessionToken(sessionToken);
}
TEST_F(PlayerInstanceAAMPTests, SetSessionTokenTest2)
{
    //checking large string
    std::string sessionToken(100000,'A');
    mPlayerInstance->SetSessionToken(sessionToken);
}
TEST_F(PlayerInstanceAAMPTests, SetSessionTokenTest3)
{
    //checking short string
    std::string sessionToken = "s";
    mPlayerInstance->SetSessionToken(sessionToken);
}
TEST_F(PlayerInstanceAAMPTests, SetSessionTokenTest4)
{
    //checking empty string
    std::string sessionToken = "";
    mPlayerInstance->SetSessionToken(sessionToken);
}
TEST_F(PlayerInstanceAAMPTests, GetAvailableVideoTracksTest)
{
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState()).WillRepeatedly(Return(eSTATE_PLAYING));
    std::string result = mPrivateInstanceAAMP->GetAvailableVideoTracks();
    
    std::string availableTracks = mPlayerInstance->GetAvailableVideoTracks();

}

TEST_F(PlayerInstanceAAMPTests, GetAvailableAudioTracksTest1)
{
    std::string result;
    mPrivateInstanceAAMP->SetState(eSTATE_ERROR);
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState()).WillRepeatedly(Return(eSTATE_ERROR));
    std::string availableTracks = mPlayerInstance->GetAvailableAudioTracks();
    EXPECT_STREQ(result.c_str(),availableTracks.c_str());
}

TEST_F(PlayerInstanceAAMPTests, GetAvailableAudioTracksTest2)
{
    std::string result;
    mPrivateInstanceAAMP->SetState(eSTATE_IDLE);
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState()).WillRepeatedly(Return(eSTATE_IDLE));
    std::string availableTracks = mPlayerInstance->GetAvailableAudioTracks();
    EXPECT_STREQ(result.c_str(),availableTracks.c_str());
}

TEST_F(PlayerInstanceAAMPTests, GetAudioTrackInfoTest)
{
    std::string result = "AudioTrack";
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState()).WillRepeatedly(Return(eSTATE_PLAYING));
    std::string textProperties = mPlayerInstance->GetAudioTrackInfo();
    EXPECT_STREQ(result.c_str(),textProperties.c_str());
}
TEST_F(PlayerInstanceAAMPTests, GetTextTrackInfoTest)
{
    std::string text_result = "TextTrack";
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState()).WillRepeatedly(Return(eSTATE_PLAYING));
    std::string textProperties = mPlayerInstance->GetTextTrackInfo();
    EXPECT_STREQ(text_result.c_str(),textProperties.c_str());
}

TEST_F(PlayerInstanceAAMPTests, GetAvailableThumbnailTracksTest)
{
   std::string expectedQuality = "ThumbnailTracks";
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState()).WillRepeatedly(Return(eSTATE_PLAYING));
    std::string result = mPlayerInstance->GetAvailableThumbnailTracks();
    EXPECT_STREQ(expectedQuality.c_str(),result.c_str());
}
TEST_F(PlayerInstanceAAMPTests, GetVideoPlaybackQualityTest1)
{
    //checking for normal string
    std::string expectedquality = "videoplayback";
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState()).WillRepeatedly(Return(eSTATE_PLAYING));
    std::string result = mPlayerInstance->GetVideoPlaybackQuality();
    EXPECT_STREQ(result.c_str(),expectedquality.c_str());

}
TEST_F(PlayerInstanceAAMPTests, GetAAMPConfigTests)
{
    std::string expectedJsonStr = "expected_json_string";
    bool result = mConfig->GetAampConfigJSONStr(expectedJsonStr);
    EXPECT_FALSE(result);
    std::string jsonStr = mPlayerInstance->GetAAMPConfig();

}

TEST_F(PlayerInstanceAAMPTests, GetPlaybackStatsTests)
{
    std::string result_stats = mPrivateInstanceAAMP->GetPlaybackStats();
    std::string stats = mPlayerInstance->GetPlaybackStats();
}
TEST_F(PlayerInstanceAAMPTests, ProcessContentProtectionDataConfigTests)
{
   const char* jsonBuffer = "json_buffer";
   cJSON *cfgdata = cJSON_Parse(jsonBuffer);
    mPlayerInstance->ProcessContentProtectionDataConfig(jsonBuffer);
}

TEST_F(PlayerInstanceAAMPTests,SetCEAFormatTest1)
{
    int expectedFormat = 1;
    mPlayerInstance->SetCEAFormat(expectedFormat);
}
TEST_F(PlayerInstanceAAMPTests,SetCEAFormatTest2)
{
    int expectedFormat = INT_MIN;
    mPlayerInstance->SetCEAFormat(expectedFormat);
}
TEST_F(PlayerInstanceAAMPTests,SetCEAFormatTest3)
{
    int expectedFormat = INT_MAX;
    mPlayerInstance->SetCEAFormat(expectedFormat);
}
TEST_F(PlayerInstanceAAMPTests,IsOOBCCRenderingSupportedTest)
{
    mPlayerInstance->IsOOBCCRenderingSupported();
}

TEST_F(PlayerInstanceAAMPTests, GetCurrentAudioLanguageTest1)
{
    // Scenario 1: Expected language
    const char* expectedLanguage = "English";
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState()).WillRepeatedly(Return(eSTATE_PLAYING));
    long result = mPlayerInstance->GetVideoBitrate();
    int trackIndex = mPlayerInstance->GetAudioTrack();
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetAudioTrack()).Times(0);
    EXPECT_CALL(*g_mockStreamAbstractionAAMP, GetAvailableAudioTracks(_))
		.Times(0);
    std::string language = mPlayerInstance->GetAudioLanguage();
}
TEST_F(PlayerInstanceAAMPTests, GetCurrentAudioLanguageTest2)
{
    // Scenario 2: Minimum length language
    char minLanguage = CHAR_MIN;
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState()).WillRepeatedly(Return(eSTATE_PLAYING));
    int minTrackIndex = mPlayerInstance->GetAudioTrack();
    std::string minLanguageResult = mPlayerInstance->GetAudioLanguage();
}
TEST_F(PlayerInstanceAAMPTests, GetCurrentAudioLanguageTest3)
{
    // Scenario 3: Minimum length language
    const char* minLanguage = "a";
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState()).WillRepeatedly(Return(eSTATE_PLAYING));
    int minTrackIndex = mPlayerInstance->GetAudioTrack();
    std::string minLanguageResult = mPlayerInstance->GetAudioLanguage();
}
TEST_F(PlayerInstanceAAMPTests,GetCurrentAudioLanguageTest4)
{
    // Scenario 4: Maximum length language
    char expectedLanguage = CHAR_MAX;
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState()).WillRepeatedly(Return(eSTATE_PLAYING));

    int trackIndex = mPlayerInstance->GetAudioTrack();
    std::string language = mPlayerInstance->GetAudioLanguage();
}
TEST_F(PlayerInstanceAAMPTests, GetCurrentAudioLanguageTest5)
{
    // Scenario 5: Maximum length language
    const char* maxLanguage = "ThisIsALongLanguageStringForTestingPurpose";
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState()).WillRepeatedly(Return(eSTATE_PLAYING));
    int maxTrackIndex = mPlayerInstance->GetAudioTrack();
    std::string maxLanguageResult = mPlayerInstance->GetAudioLanguage();
}
TEST_F(PlayerInstanceAAMPTests, GetCurrentAudioLanguageTest6)
{
    // Scenario 6: Null language
    const char* nullLanguage = nullptr;
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState()).WillRepeatedly(Return(eSTATE_PLAYING));
    int nullTrackIndex = mPlayerInstance->GetAudioTrack();
    std::string nullLanguageResult = mPlayerInstance->GetAudioLanguage();
}
TEST_F(PlayerInstanceAAMPTests,SetTextTrackTest)
{
    int trackID = 1;
    char* ccData = new char[50];
    strcpy(ccData,"Closed caption data");
    mPlayerInstance->SetTextTrack(trackID,ccData);
}
TEST_F(PlayerInstanceAAMPTests,InitAAMPConfigTest)
{
    const char* jsonStr = "{\"key\": \"value\"}";
    mPlayerInstance->AsyncStartStop();
    mPlayerInstance->InitAAMPConfig(jsonStr);
}

TEST_F(PlayerInstanceAAMPTests,SetAuxiliaryLanguageTest1)
{
    //checking minimum string
    std::string language = "a";
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState()).WillRepeatedly(Return(eSTATE_PLAYING));

    EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_AsyncTune)).WillRepeatedly(Return(true));
    mPlayerInstance->AsyncStartStop();

    EXPECT_CALL(*g_mockAampScheduler, ScheduleTask(_)).WillOnce(Return(1));
    mPlayerInstance->SetAuxiliaryLanguage(language);
}
TEST_F(PlayerInstanceAAMPTests,SetAuxiliaryLanguageTest2)
{
    //checking maximum string
    std::string language(1000000,'A');
    mPlayerInstance->SetAuxiliaryLanguage(language);
}

TEST_F(PlayerInstanceAAMPTests,SetAuxiliaryLanguageTest3)
{
    //checking random string
    std::string language = "English";
    mPlayerInstance->SetAuxiliaryLanguage(language);
}

TEST_F(PlayerInstanceAAMPTests,SetPlaybackSpeedTest1)
{
    //checking null speed
    float nullspeed = 0.0f;
    mPlayerInstance->SetPlaybackSpeed(nullspeed);
}
TEST_F(PlayerInstanceAAMPTests,SetPlaybackSpeedTest2)
{
    //checking min speed
    float minspeed = 0.1f;
    mPlayerInstance->SetPlaybackSpeed(minspeed);
}
TEST_F(PlayerInstanceAAMPTests,SetPlaybackSpeedTest3)
{
    //checking max speed
    float maxspeed = 5.0f;
    mPlayerInstance->SetPlaybackSpeed(maxspeed);
}
TEST_F(PlayerInstanceAAMPTests,SetPlaybackSpeedTest4)
{
    //checking negative speed
    float negativespeed = -0.5f;
    mPlayerInstance->SetPlaybackSpeed(negativespeed);
}
TEST_F(PlayerInstanceAAMPTests,Tune_msyncenabledTest)
{
    // Scenario 1: random values
    
    const char *mainManifestUrl = "https://example.com";
    bool autoPlay = true;
    const char *contentType = "video";
    bool bFirstAttempt = true;
    bool bFinalAttempt = false;
    const char *traceUUID = "12345";
    bool audioDecoderStreamSync = true;
    const char *refreshManifestUrl = "https://example.comm";
	int mpdStitchingMode = 10;

    EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState()).WillRepeatedly(Return(eSTATE_PLAYING));

    EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_AsyncTune)).WillRepeatedly(Return(true));
    mPlayerInstance->AsyncStartStop();

    EXPECT_CALL(*g_mockAampScheduler, ScheduleTask(_)).WillOnce(Return(1));

    mPlayerInstance->Tune(mainManifestUrl,autoPlay,contentType,bFirstAttempt,bFinalAttempt,traceUUID,audioDecoderStreamSync,refreshManifestUrl,mpdStitchingMode);
}
TEST_F(PlayerInstanceAAMPTests,TuneTest1)
{
    // Scenario 1: random values
    const char *mainManifestUrl = "https://example.com";
    const char *contentType = "video";
    bool bFirstAttempt = true;
    bool bFinalAttempt = false;
    const char *traceUUID = "12345";
    bool audioDecoderStreamSync = true;

    mPlayerInstance->Tune(mainManifestUrl,contentType,bFirstAttempt,bFinalAttempt,traceUUID,audioDecoderStreamSync);
}
TEST_F(PlayerInstanceAAMPTests, TuneTest2)
{
    // Scenario 2: Minimum values
    const char *minManifestUrl = "https://min.example.com";
    char minContentType = CHAR_MIN;
    bool minFirstAttempt = false;
    bool minFinalAttempt = false;
    char minTraceUUID = CHAR_MIN;
    bool minAudioDecoderStreamSync = false;

    mPlayerInstance->Tune(minManifestUrl, &minContentType, minFirstAttempt, minFinalAttempt, &minTraceUUID, minAudioDecoderStreamSync);
}
TEST_F(PlayerInstanceAAMPTests, TuneTest3)
{
    // Scenario 3: Maximum values
    const char *maxManifestUrl = "https://max.example.com";
    char maxContentType = CHAR_MAX;
    bool maxFirstAttempt = true;
    bool maxFinalAttempt = true;
    char maxTraceUUID = CHAR_MAX;
    bool maxAudioDecoderStreamSync = true;

    mPlayerInstance->Tune(maxManifestUrl, &maxContentType, maxFirstAttempt, maxFinalAttempt, &maxTraceUUID, maxAudioDecoderStreamSync);
}

TEST_F(PlayerInstanceAAMPTests, TuneTest4)
{
	const char *mainManifestUrl = "https://example.com";
	bool autoPlay = true;
	const char *contentType = "video";
	bool bFirstAttempt = true;
	bool bFinalAttempt = false;
	const char *traceUUID = "12345";
	bool audioDecoderStreamSync = true;
	const char *refreshManifestUrl = "https://example.comm";
	int mpdStitchingMode = 10;
	static const char* manifestData = R"(<?xml version="1.0" encoding="UTF-8"?><MPD xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xmlns="urn:mpeg:dash:schema:mpd:2011" xmlns:scte35="http://www.scte.org/schemas/35/2014SCTE35.xsd" xsi:schemaLocation="urn:mpeg:dash:schema:mpd:2011 DASH-MPD.xsd" profiles="urn:mpeg:dash:profile:isoff-live:2011" type="static" minBufferTime="PT5.000S" maxSegmentDuration="PT2.005S" availabilityStartTime="2016-01-20T21:10:02Z" mediaPresentationDuration="PT193.680S"><Period id="period0"><AdaptationSet mimeType="video/mp4" segmentAlignment="true" startWithSAP="1" maxWidth="1920" maxHeight="1080" maxFrameRate="30000/1001" par="1:1"><SegmentTemplate timescale="90000" initialization="$RepresentationID$-Header.m4s" media="$RepresentationID$-270146-i-$Number$.m4s" startNumber="1" duration="179704" presentationTimeOffset="0"/><Representation id="v1_257" bandwidth="1200000" codecs="avc1.4D401E" width="768" height="432" frameRate="30000/1001" sar="1:1" scanType="progressive"/><Representation id="v2_257" bandwidth="1850000" codecs="avc1.4D401E" width="1024" height="576" frameRate="30000/1001" sar="1:1" scanType="progressive"/><Representation id="v3_257" bandwidth="2850000" codecs="avc1.4D401E" width="1280" height="720" frameRate="30000/1001" sar="1:1" scanType="progressive"/><Representation id="v4_257" bandwidth="200000" codecs="avc1.4D401E" width="320" height="180" frameRate="30000/1001" sar="1:1" scanType="progressive"/><Representation id="v5_257" bandwidth="300000" codecs="avc1.4D401E" width="320" height="180" frameRate="30000/1001" sar="1:1" scanType="progressive"/><Representation id="v6_257" bandwidth="4300000" codecs="avc1.4D401E" width="1280" height="720" frameRate="30000/1001" sar="1:1" scanType="progressive"/><Representation id="v7_257" bandwidth="5300000" codecs="avc1.4D401E" width="1920" height="1080" frameRate="30000/1001" sar="1:1" scanType="progressive"/><Representation id="v8_257" bandwidth="480000" codecs="avc1.4D401E" width="512" height="288" frameRate="30000/1001" sar="1:1" scanType="progressive"/><Representation id="v9_257" bandwidth="750000" codecs="avc1.4D401E" width="640" height="360" frameRate="30000/1001" sar="1:1" scanType="progressive"/></AdaptationSet><AdaptationSet mimeType="audio/mp4" segmentAlignment="true" startWithSAP="1" lang="qaa"><SegmentTemplate timescale="90000" initialization="$RepresentationID$-Header.m4s" media="$RepresentationID$-270146-i-$Number$.m4s" startNumber="1" duration="179704" presentationTimeOffset="0"/><Representation id="v4_258" bandwidth="130800" codecs="mp4a.40.2" audioSamplingRate="48000"><AudioChannelConfiguration schemeIdUri="urn:mpeg:dash:23003:3:audio_channel_configuration:2011" value="2"/></Representation></AdaptationSet></Period></MPD>)";

	std::string session_id {"0259343c-cffc-4659-bcd8-97f9dd36f6b1"};

	EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState()).WillRepeatedly(Return(eSTATE_PLAYING));

	EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_AsyncTune)).WillRepeatedly(Return(true));
	mPlayerInstance->AsyncStartStop();

	EXPECT_CALL(*g_mockAampScheduler, ScheduleTask(_)).WillOnce(Return(1));

	mPlayerInstance->Tune(mainManifestUrl,autoPlay,contentType,bFirstAttempt,bFinalAttempt,traceUUID,audioDecoderStreamSync,refreshManifestUrl,mpdStitchingMode,session_id,manifestData);
}

TEST_F(PlayerInstanceAAMPTests, updateManifestTest1)
{
	mPlayerInstance->updateManifest(nullptr);
}

TEST_F(PlayerInstanceAAMPTests, updateManifestTest2)
{
	const char* manifestData = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n<MPD xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"";
	mPlayerInstance->updateManifest(manifestData);
}

TEST_F(PlayerInstanceAAMPTests,SetTextTrackTest1)
{
	int trackID = 1;
	mPlayerInstance->SetTextTrack(trackID,NULL);
}

// Test pausing
TEST_F(PlayerInstanceAAMPTests, SetRateTest_Pause) {
    mPlayerInstance->aamp = mPrivateInstanceAAMP;
    mPrivateInstanceAAMP->rate = AAMP_NORMAL_PLAY_RATE;
    mPrivateInstanceAAMP->pipeline_paused = false;
    mPrivateInstanceAAMP->mbPlayEnabled = true;
    mPrivateInstanceAAMP->SetLocalAAMPTsb(false);

    EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState()).WillRepeatedly(Return(eSTATE_PLAYING));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, StopDownloads()).Times(1);
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, NotifyPlaybackPaused(true)).Times(1);
	EXPECT_CALL(*g_mockAampGstPlayer, Pause(true, false)).Times(1);
    
    mPlayerInstance->SetRate(0);

    EXPECT_EQ(mPrivateInstanceAAMP->pipeline_paused, true);
}

// Test pausing with local TSB
TEST_F(PlayerInstanceAAMPTests, SetRateTest_LocalTSB_Pause) {
    mPlayerInstance->aamp = mPrivateInstanceAAMP;
    mPrivateInstanceAAMP->rate = AAMP_NORMAL_PLAY_RATE;
    mPrivateInstanceAAMP->pipeline_paused = false;
    mPrivateInstanceAAMP->mbPlayEnabled = true;
    mPrivateInstanceAAMP->SetLocalAAMPTsb(true);

    EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState()).WillRepeatedly(Return(eSTATE_PLAYING));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, StopDownloads()).Times(0);
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, NotifyPlaybackPaused(true)).Times(1);
	EXPECT_CALL(*g_mockAampGstPlayer, Pause(true, false)).Times(1);
    
    mPlayerInstance->SetRate(0);

    EXPECT_EQ(mPrivateInstanceAAMP->pipeline_paused, true);
}

// Test resuming from being paused on live with local TSB
TEST_F(PlayerInstanceAAMPTests, SetRateTest_LocalTSB_ResumeFromLive) {

    long long seek_pos_seconds = 15000.0;

    mPlayerInstance->aamp = mPrivateInstanceAAMP;
    mPrivateInstanceAAMP->rate = AAMP_NORMAL_PLAY_RATE;
    mPrivateInstanceAAMP->pipeline_paused = true;
    mPrivateInstanceAAMP->mbPlayEnabled = true;
    mPrivateInstanceAAMP->SetLocalAAMPTsb(true);

    EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState()).WillRepeatedly(Return(eSTATE_PLAYING));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, StopDownloads()).Times(0);
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, IsLocalAAMPTsbInjection()).WillRepeatedly(Return(false));

	EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetPositionMilliseconds()).WillRepeatedly(Return(seek_pos_seconds));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SetState(eSTATE_SEEKING)).Times(1);
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, TuneHelper(eTUNETYPE_SEEK, false)).Times(1);

    mPlayerInstance->SetRate(1.0);

    EXPECT_EQ(mPrivateInstanceAAMP->seek_pos_seconds, seek_pos_seconds / 1000);
    EXPECT_EQ(mPrivateInstanceAAMP->pipeline_paused, false);
}

// Test resuming from being paused in local TSB playback
TEST_F(PlayerInstanceAAMPTests, SetRateTest_LocalTSB_ResumeFromTSB) {

    mPlayerInstance->aamp = mPrivateInstanceAAMP;
    mPrivateInstanceAAMP->rate = AAMP_NORMAL_PLAY_RATE;
    mPrivateInstanceAAMP->pipeline_paused = true;
    mPrivateInstanceAAMP->mbPlayEnabled = true;
    mPrivateInstanceAAMP->SetLocalAAMPTsb(true);

    EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState()).WillRepeatedly(Return(eSTATE_PLAYING));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, StopDownloads()).Times(0);
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, IsLocalAAMPTsbInjection()).WillRepeatedly(Return(true));

	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SetState(_)).Times(0);
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, TuneHelper(_, _)).Times(0);
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, NotifyPlaybackPaused(false)).Times(1);
	EXPECT_CALL(*g_mockAampGstPlayer, Pause(false, false)).Times(1);

    mPlayerInstance->SetRate(1.0);

    EXPECT_EQ(mPrivateInstanceAAMP->pipeline_paused, false);
}

// Test forward 2x from being paused in local TSB playback
TEST_F(PlayerInstanceAAMPTests, SetRateTest_LocalTSB_TrickPlayWhenPausedFromTSB) {

	mPlayerInstance->aamp = mPrivateInstanceAAMP;
	mPrivateInstanceAAMP->rate = AAMP_NORMAL_PLAY_RATE;
	mPrivateInstanceAAMP->pipeline_paused = true;
	mPrivateInstanceAAMP->mbPlayEnabled = true;
	mPrivateInstanceAAMP->mIsIframeTrackPresent = true;
	mPrivateInstanceAAMP->SetLocalAAMPTsb(true);

	EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState()).WillRepeatedly(Return(eSTATE_PLAYING));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, StopDownloads()).Times(0);
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, IsLocalAAMPTsbInjection()).WillRepeatedly(Return(true));

	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SetState(_)).Times(0);
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, TuneHelper(eTUNETYPE_SEEK, _)).Times(1);
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, NotifySpeedChanged(2.0,_)).Times(1);
	//calling AAMPGstPlayer::Pause(false,false) would cause the pipeline to play at x1
	// before changing the speed to x2. Make sure that it is not happening.
	EXPECT_CALL(*g_mockAampGstPlayer, Pause(false, false)).Times(0);

	mPlayerInstance->SetRate(2.0);
	EXPECT_EQ(mPrivateInstanceAAMP->pipeline_paused, false);
	EXPECT_EQ(mPrivateInstanceAAMP->rate, 2.0);

}
