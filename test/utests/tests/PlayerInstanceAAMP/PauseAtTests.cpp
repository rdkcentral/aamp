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
#include "main_aamp.h"
#include "MockStreamAbstractionAAMP.h"

using ::testing::_;
using ::testing::Return;
using ::testing::SetArgReferee;
using ::testing::AtLeast;

class PauseAtTests : public ::testing::Test
{
protected:
    PlayerInstanceAAMP *mPlayerInstance = nullptr;
    PrivateInstanceAAMP *aampObj = new PrivateInstanceAAMP();

    void SetUp() override 
    {
        AampLogManager *logObj;
        PrivateInstanceAAMP *aamp;
        mPlayerInstance = new PlayerInstanceAAMP();

        g_mockAampConfig = new MockAampConfig();
        g_mockAampScheduler = new MockAampScheduler();
        g_mockPrivateInstanceAAMP = new MockPrivateInstanceAAMP();
        g_mockStreamAbstractionAAMP = new MockStreamAbstractionAAMP(logObj, aamp);
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
    }
};


// Testing calling PauseAt with position
// Expect to call stop pause position monitoring, followed by 
// start pause position monitoring with the requested position
TEST_F(PauseAtTests, PauseAt)
{
    double pauseAtSeconds = 100.0;
    long long pauseAtMilliseconds = pauseAtSeconds * 1000;

    EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState(_)).WillRepeatedly(SetArgReferee<0>(eSTATE_PLAYING));

    EXPECT_CALL(*g_mockPrivateInstanceAAMP, StopPausePositionMonitoring("PauseAt() called")).Times(1);
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, StartPausePositionMonitoring(pauseAtMilliseconds)).Times(1);

    mPlayerInstance->PauseAt(pauseAtSeconds);
}

// Testing calling PauseAt with position 0
// Expect to call stop pause position monitoring, followed by 
// start pause position monitoring with the requested position
TEST_F(PauseAtTests, PauseAt_Position0)
{
    double pauseAtSeconds = 0;
    long long pauseAtMilliseconds = pauseAtSeconds * 1000;

    EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState(_)).WillRepeatedly(SetArgReferee<0>(eSTATE_PLAYING));

    EXPECT_CALL(*g_mockPrivateInstanceAAMP, StopPausePositionMonitoring("PauseAt() called")).Times(1);
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, StartPausePositionMonitoring(pauseAtMilliseconds)).Times(1);

    mPlayerInstance->PauseAt(pauseAtSeconds);
}

// Testing calling PauseAt with negative value to cancel
// Expect to call stop pause position monitoring
TEST_F(PauseAtTests, PauseAt_Cancel)
{
    double pauseAtSeconds = -1.0;

    EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState(_)).WillRepeatedly(SetArgReferee<0>(eSTATE_PLAYING));

    EXPECT_CALL(*g_mockPrivateInstanceAAMP, StopPausePositionMonitoring("PauseAt() called")).Times(1);
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, StartPausePositionMonitoring(_)).Times(0);

    mPlayerInstance->PauseAt(pauseAtSeconds);
}

// Testing calling PauseAt when already paused
// Don't expect to start pause position monitoring
TEST_F(PauseAtTests, PauseAt_AlreadyPaused)
{
    double pauseAtSeconds = 100.0;

    mPlayerInstance->aamp->pipeline_paused = true;

    EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState(_)).WillRepeatedly(SetArgReferee<0>(eSTATE_PLAYING));

    EXPECT_CALL(*g_mockPrivateInstanceAAMP, StopPausePositionMonitoring("PauseAt() called")).Times(1);
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, StartPausePositionMonitoring(_)).Times(0);

    mPlayerInstance->PauseAt(pauseAtSeconds);
}

// Testing calling PauseAt whilst in error state
// Expect to neither call stop nor start pause position monitoring
TEST_F(PauseAtTests, PauseAt_InErrorState)
{
    double pauseAtSeconds = -1.0;

    EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState(_)).WillRepeatedly(SetArgReferee<0>(eSTATE_ERROR));

    EXPECT_CALL(*g_mockPrivateInstanceAAMP, StopPausePositionMonitoring(_)).Times(0);
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, StartPausePositionMonitoring(_)).Times(0);

    mPlayerInstance->PauseAt(pauseAtSeconds);
}

// Testing calling PauseAt when configured to run async API
// Expect the async scheduler to be called
TEST_F(PauseAtTests, PauseAtAsync)
{
    double pauseAtSeconds = 100.0;

    EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState(_)).WillRepeatedly(SetArgReferee<0>(eSTATE_PLAYING));

    EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_AsyncTune)).WillRepeatedly(Return(true));
    mPlayerInstance->AsyncStartStop();

    EXPECT_CALL(*g_mockAampScheduler, ScheduleTask(_)).WillOnce(Return(1));

    mPlayerInstance->PauseAt(pauseAtSeconds);
}

// Testing calling Tune cancels any pause position monitoring
// Expect StopPausePositionMonitoring to be called at least once 
// (internally Tune can call Stop ao possible for multiple calls)
TEST_F(PauseAtTests, PauseAt_Tune)
{
    char mainManifestUrl[] = "";

    EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState(_)).WillRepeatedly(SetArgReferee<0>(eSTATE_PLAYING));

    EXPECT_CALL(*g_mockPrivateInstanceAAMP, StopPausePositionMonitoring("Tune() called")).Times(1);
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, StopPausePositionMonitoring("Stop() called")).Times(1);
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, StartPausePositionMonitoring(_)).Times(0);

    mPlayerInstance->Tune(mainManifestUrl);
}

// Testing calling detach cancels any pause position monitoring
// Expect StopPausePositionMonitoring to be called at once 
TEST_F(PauseAtTests, PauseAt_detach)
{
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState(_)).WillRepeatedly(SetArgReferee<0>(eSTATE_PLAYING));
    
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, StopPausePositionMonitoring("detach() called")).Times(1);
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, StartPausePositionMonitoring(_)).Times(0);

    mPlayerInstance->detach();
}

// Testing calling SetRate cancels any pause position monitoring
// Expect StopPausePositionMonitoring to be called at once 
TEST_F(PauseAtTests, PauseAt_SetRate)
{
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState(_)).WillRepeatedly(SetArgReferee<0>(eSTATE_PLAYING));

    EXPECT_CALL(*g_mockPrivateInstanceAAMP, StopPausePositionMonitoring("SetRate() called")).Times(1);
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, StartPausePositionMonitoring(_)).Times(0);

    mPlayerInstance->SetRate(1);
}

// Testing calling Stop cancels any pause position monitoring
// Expect StopPausePositionMonitoring to be called at once 
TEST_F(PauseAtTests, PauseAt_Stop)
{
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState(_)).WillRepeatedly(SetArgReferee<0>(eSTATE_PLAYING));

    EXPECT_CALL(*g_mockPrivateInstanceAAMP, StopPausePositionMonitoring("Stop() called")).Times(1);
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, StartPausePositionMonitoring(_)).Times(0);

    mPlayerInstance->Stop();
}

// Testing calling Seek cancels any pause position monitoring
// Expect StopPausePositionMonitoring to be called at once 
TEST_F(PauseAtTests, PauseAt_Seek)
{
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState(_)).WillRepeatedly(SetArgReferee<0>(eSTATE_PLAYING));

    EXPECT_CALL(*g_mockPrivateInstanceAAMP, StopPausePositionMonitoring("Seek() called")).Times(1);
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, StartPausePositionMonitoring(_)).Times(0);

    mPlayerInstance->Seek(1000);
}

TEST_F(PauseAtTests, RegisterEventsTests) {
    EventListener* eventListener = nullptr;
    mPlayerInstance-> RegisterEvents(eventListener);
    aampObj->RegisterAllEvents(eventListener);
}

TEST_F(PauseAtTests, UnRegisterEventsTests) {
    EventListener* eventListener = nullptr;
    mPlayerInstance-> UnRegisterEvents(eventListener);
    aampObj->UnRegisterEvents(eventListener);
}

TEST_F(PauseAtTests, SetSegmentInjectFailCountTest) {
    int value = 5;
    mPlayerInstance->SetSegmentInjectFailCount(value);
}

TEST_F(PauseAtTests, SetSegmentDecryptFailCountTest) {
    int value = 5;
    mPlayerInstance->SetSegmentDecryptFailCount(value);
}

TEST_F(PauseAtTests, SetInitialBufferDurationTest) {
    int durationSec = 10;
    mPlayerInstance->SetInitialBufferDuration(durationSec);
}
TEST_F(PauseAtTests, GetInitialBufferDurationTest) {
    
    EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_InitialBuffer))
        .WillOnce(Return(5000)); 
    int initialBufferDuration = mPlayerInstance->GetInitialBufferDuration();

    EXPECT_EQ(initialBufferDuration,5000);
}

TEST_F(PauseAtTests, SetMaxPlaylistCacheSizeTest) {
    int Cachesize = 100;
    mPlayerInstance->SetMaxPlaylistCacheSize(Cachesize);
}

TEST_F(PauseAtTests, SetRampDownLimitTest) {
    int expectedLimit = 50;
    mPlayerInstance->SetRampDownLimit(expectedLimit);
}

TEST_F(PauseAtTests, SetMinimumBitrate_ValidBitrate) {
    
    BitsPerSecond bitrate = 1000000;

    mPlayerInstance->SetMinimumBitrate(bitrate);
}

TEST_F(PauseAtTests, SetMinimumBitrate_InvalidBitrate) {
   
    long bitrate = -500000; 

    mPlayerInstance->SetMinimumBitrate(bitrate);

}

TEST_F(PauseAtTests, SetMaximumBitrate) {

    long bitrate = 5000;
    mPlayerInstance->SetMaximumBitrate(bitrate);
}

TEST_F(PauseAtTests, SetMaximumBitrate1) {

    long bitrate = -5000;
    mPlayerInstance->SetMaximumBitrate(bitrate);
}


TEST_F(PauseAtTests, GetMaximumBitrate) {

    BitsPerSecond expectedMaxBitrate = 6000;
    EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_MaxBitrate))
        .WillOnce(Return(expectedMaxBitrate));

    BitsPerSecond maxBitrate = mPlayerInstance->GetMaximumBitrate();
    EXPECT_EQ(expectedMaxBitrate,maxBitrate);
}

TEST_F(PauseAtTests, GetMinimumBitrate) {

    BitsPerSecond expectedMinBitrate = 10000;
    EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_MinBitrate))
        .WillOnce(Return(expectedMinBitrate));

    BitsPerSecond minBitrate = mPlayerInstance->GetMinimumBitrate();
    EXPECT_EQ(minBitrate,expectedMinBitrate);
}

TEST_F(PauseAtTests, GetRampDownLimitTest) {

     EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_RampDownLimit))
        .WillOnce(Return(10000)); 
    int RampDownlimit = mPlayerInstance->GetRampDownLimit();

    EXPECT_EQ(RampDownlimit,10000);
}

TEST_F(PauseAtTests, SetLanguageFormatTest) {
   
    LangCodePreference preferredFormat = ISO639_NO_LANGCODE_PREFERENCE;
    bool useRole = true;
    
    mPlayerInstance->SetLanguageFormat(preferredFormat, useRole);
}

TEST_F(PauseAtTests, SeekToLiveTest){

    bool pause = true;
    mPlayerInstance->SeekToLive(pause);
}

TEST_F(PauseAtTests, SetSlowMotionPlayRateTest){

    float rate = 0.5f;
    mPlayerInstance->SetSlowMotionPlayRate(rate);
}

TEST_F(PauseAtTests, SetVideoRectangleTest) {
   
    int x = 10; 
    int y = 20;
    int w = 640;
    int h = 480;

    mPlayerInstance->SetVideoRectangle(x, y, w, h);
}

TEST_F(PauseAtTests, SetVideoZoomTest) {
    
    VideoZoomMode zoom = VIDEO_ZOOM_FULL;
    mPlayerInstance->SetVideoZoom(zoom);
}

TEST_F(PauseAtTests, SetVideoMute_NotNullAamp) {
    bool muted = true;
    mPlayerInstance->SetVideoMute(muted);
}

TEST_F(PauseAtTests, SetSubtitleMuteTest) {
    bool muted = true; 
   mPlayerInstance->SetSubtitleMute(muted);
}

TEST_F(PauseAtTests, SetAudioVolumeTest) {
    int volume = 50; 
   mPlayerInstance->SetAudioVolume(volume);
}

TEST_F(PauseAtTests, SetLanguageTest) {
    const char* language = "english";

    mPlayerInstance->SetPreferredLanguages(language);
   mPlayerInstance->SetLanguage(language);
}

TEST_F(PauseAtTests, SetSubscribedTagsTest) {
     std::vector<std::string> subscribedTags = { "tag1", "tag2" };

   mPlayerInstance->SetSubscribedTags(subscribedTags);
}

TEST_F(PauseAtTests, SubscribeResponseHeadersTest) {
    std::vector<std::string> responseHeaders = { "header1: value1", "header2: value2" }; 

   mPlayerInstance->SubscribeResponseHeaders(responseHeaders);
}

TEST_F(PauseAtTests, AddEventListenerTest) {
    AAMPEventType eventType = AAMP_EVENT_TUNED;
    EventListener* eventListener = nullptr ;

   mPlayerInstance->AddEventListener(eventType,eventListener);
   aampObj->AddEventListener(eventType,eventListener);
}

TEST_F(PauseAtTests, RemoveEventListenerTest) {
    AAMPEventType eventType = AAMP_EVENT_TUNED;
    EventListener* eventListener = nullptr;
   mPlayerInstance->RemoveEventListener(eventType,eventListener);
    aampObj->RemoveEventListener(eventType,eventListener);
}

TEST_F(PauseAtTests, IsLiveTest) {
   bool islive = mPlayerInstance->IsLive();
   EXPECT_FALSE(islive);
}

TEST_F(PauseAtTests, IsJsInfoLoggingEnabledTest) {
    
    bool expectedValue = true;
    EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_JsInfoLogging))
        .WillOnce(Return(expectedValue));
   bool isLoggingEnabled = mPlayerInstance->IsJsInfoLoggingEnabled();
   EXPECT_TRUE(isLoggingEnabled);
}

TEST_F(PauseAtTests, GetCurrentDRMTest) {
    const char* expectedDrmName = "DRM"; 
    const char* drmName =  mPlayerInstance->GetCurrentDRM();
    aampObj->GetCurrentDRM();
}

TEST_F(PauseAtTests, AddPageHeadersTest) {
    
    std::map<std::string, std::string> pageHeaders;
    pageHeaders["Header1"] = "pageHeaders_Value1";
    pageHeaders["Header2"] = "pageHeaders_Value2"; 

    mPlayerInstance->AddPageHeaders(pageHeaders);
}

TEST_F(PauseAtTests, AddCustomHTTPHeaderTest) {
    std::string headerName = "CustomHeader";
    std::vector<std::string> headerValue = { "headerValue1", "headerValue2" }; 
    bool isLicenseHeader = true; 

    mPlayerInstance->AddCustomHTTPHeader(headerName, headerValue, isLicenseHeader);
}

TEST_F(PauseAtTests, SetLicenseServerURLTest) {
    
    const char* prUrl = "https://playready.example.com/";
    mPlayerInstance->SetLicenseServerURL(prUrl, eDRM_PlayReady);

    const char* wvUrl = "https://widevine.example.com/";
   mPlayerInstance->SetLicenseServerURL(wvUrl, eDRM_WideVine);

    const char* ckUrl = "https://clearkey.example.com/";
    mPlayerInstance->SetLicenseServerURL(ckUrl, eDRM_ClearKey);

    const char* invalidUrl = "https://invalid.example.com/";
   mPlayerInstance->SetLicenseServerURL(invalidUrl, eDRM_MAX_DRMSystems);

   const char* invalidUrl1 = "https://invalid1.example.com/";
    mPlayerInstance->SetLicenseServerURL(invalidUrl, static_cast<DRMSystems>(-1));

}

TEST_F(PauseAtTests, SetAnonymousRequestTest) {
   
    mPlayerInstance->SetAnonymousRequest(true);

}

TEST_F(PauseAtTests, SetAvgBWForABRTest) {
   
    mPlayerInstance->SetAvgBWForABR(true);
}

TEST_F(PauseAtTests, SetPreCacheTimeWindowTest) {
   
   int nTimeWindow = 30;
    mPlayerInstance->SetPreCacheTimeWindow(nTimeWindow);
}

TEST_F(PauseAtTests, SetVODTrickplayFPSTest) {
   
    int vodTrickplayFPS = 60;
    mPlayerInstance->SetVODTrickplayFPS(vodTrickplayFPS);
}

TEST_F(PauseAtTests, SetLinearTrickplayFPSTest) {
   
    int linearTrickplayFPS = 30;
    mPlayerInstance->SetLinearTrickplayFPS(linearTrickplayFPS);
}

TEST_F(PauseAtTests, SetLiveOffsetTest) {
   
    double liveoffset = 10.0;
    mPlayerInstance->SetLiveOffset(liveoffset);

}

TEST_F(PauseAtTests, SetLiveOffset4KTest) {
    mPlayerInstance->SetLiveOffset4K(15.0);
}

TEST_F(PauseAtTests, SetStallErrorCodeTest) {
    mPlayerInstance->SetStallErrorCode(404);
}

TEST_F(PauseAtTests, SetStallTimeoutTest) {
    mPlayerInstance->SetStallTimeout(5000);
}

TEST_F(PauseAtTests, SetReportIntervalTest) {
    mPlayerInstance->SetReportInterval(5000);
}

TEST_F(PauseAtTests, SetInitFragTimeoutRetryCount) {
    
    mPlayerInstance->SetInitFragTimeoutRetryCount(5);
    
}

TEST_F(PauseAtTests, GetPlaybackPositionTest) {

    double expectedPosition = 100.00;
    double position = mPlayerInstance->GetPlaybackPosition();
}

TEST_F(PauseAtTests, GetPlaybackDurationTest) {
    double playbackDuration = mPlayerInstance->GetPlaybackDuration();
}

TEST_F(PauseAtTests, GetIdNotNullTest) {

    mPlayerInstance->aamp->mPlayerId = 123;
    int playerId = mPlayerInstance->GetId();

    EXPECT_EQ(playerId,123);
}
TEST_F(PauseAtTests, GetIdNullTest) {

    mPlayerInstance->aamp = nullptr;
    int playerId = mPlayerInstance->GetId();

    EXPECT_EQ(playerId,-1);
}
TEST_F(PauseAtTests, GetStateTest) {
  
    PrivAAMPState state = mPlayerInstance->GetState();
}

TEST_F(PauseAtTests, SetVideoBitrateTest) {
    BitsPerSecond bitrate = 3000000; 
   mPlayerInstance->SetVideoBitrate(bitrate);

    mPlayerInstance->SetVideoBitrate(0);
}

TEST_F(PauseAtTests, GetAudioBitrateTest) {
    BitsPerSecond audioBitrate = 128000; 
    BitsPerSecond retrievedAudioBitrate = mPlayerInstance->GetAudioBitrate();
}

TEST_F(PauseAtTests, SetAudioBitrateTest) {
    BitsPerSecond audioBitrate = 96000; 
    mPlayerInstance->SetAudioBitrate(audioBitrate);
 
    BitsPerSecond newAudioBitrate = 128000; 
    mPlayerInstance->SetAudioBitrate(newAudioBitrate);
}


TEST_F(PauseAtTests, GetVideoZoomTest) {
    
    mPlayerInstance->aamp->zoom_mode = VIDEO_ZOOM_FULL;
    int ZoomMode = mPlayerInstance->GetVideoZoom();
    EXPECT_EQ(ZoomMode,VIDEO_ZOOM_FULL);
}

TEST_F(PauseAtTests, GetVideoMuteTest) {
   
    mPlayerInstance->aamp->video_muted = true;
    bool retrievedVideoMute = mPlayerInstance->GetVideoMute();
    EXPECT_TRUE(retrievedVideoMute);
}

TEST_F(PauseAtTests, GetAudioVolumeTest) {

    mPlayerInstance->aamp->audio_volume = 50; 
    int retrievedAudioVolume = mPlayerInstance->GetAudioVolume();

    EXPECT_EQ(retrievedAudioVolume,50);
}
TEST_F(PauseAtTests, GetPlaybackRateTest_1) {

    mPlayerInstance->aamp->pipeline_paused = false;
    mPlayerInstance->aamp->rate = 1;
    int retrievedPlaybackRate = mPlayerInstance->GetPlaybackRate();

    EXPECT_EQ(retrievedPlaybackRate,1);

}
TEST_F(PauseAtTests, GetPlaybackRateTest_2) {

    mPlayerInstance->aamp->pipeline_paused = true;
    
    int retrievedPlaybackRate = mPlayerInstance->GetPlaybackRate();

    EXPECT_EQ(retrievedPlaybackRate,0);

}
TEST_F(PauseAtTests, GetAudioTrackTest) {

    mPlayerInstance->aamp->SetAudioTrack(1);
   int audioTrack = mPlayerInstance->GetAudioTrack();

   EXPECT_NE(audioTrack,1);
}
TEST_F(PauseAtTests, GetManifestTest) {
    
    std::string result = mPlayerInstance->GetManifest();
}
TEST_F(PauseAtTests, SetInitialBitrateTest) {
    BitsPerSecond bitrate = 1500; 
    mPlayerInstance->SetInitialBitrate(bitrate);
}

TEST_F(PauseAtTests, GetInitialBitrateTest) {
    BitsPerSecond expectedBitrate = 2500; 
    BitsPerSecond result = mPlayerInstance->GetInitialBitrate();
}
TEST_F(PauseAtTests, SetInitialBitrate4KTest) {
    BitsPerSecond bitrate4K = 25000000; 
    mPlayerInstance->SetInitialBitrate4K(bitrate4K);
}
TEST_F(PauseAtTests, GetInitialBitrate4kTest) {
    BitsPerSecond expectedBitrate = 25000000; 
    EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_DefaultBitrate4K))
        .WillOnce(Return(expectedBitrate)); 
    BitsPerSecond result = mPlayerInstance->GetInitialBitrate4k();

    EXPECT_EQ(result,expectedBitrate);
}
TEST_F(PauseAtTests, SetNetworkTimeoutTest) {
    BitsPerSecond timeout = 10.0; 
    mPlayerInstance->SetNetworkTimeout(timeout);
}
TEST_F(PauseAtTests, SetManifestTimeoutTest) {
    BitsPerSecond timeout = 5.0; 
    mPlayerInstance->SetManifestTimeout(timeout);
}
TEST_F(PauseAtTests, SetPlaylistTimeoutTest) {
    BitsPerSecond timeout = 8.0; 
    mPlayerInstance->SetPlaylistTimeout(timeout);
}
TEST_F(PauseAtTests, SetDownloadBufferSizeTest) {
    BitsPerSecond buffersize = 1024; 
    mPlayerInstance->SetDownloadBufferSize(buffersize);
}
TEST_F(PauseAtTests, SetPreferredDRMTest)
{
    DRMSystems drmtype = eDRM_PlayReady;
    mPlayerInstance->SetPreferredDRM(drmtype);

}
TEST_F(PauseAtTests, SetDisable4KTest)
{
    mPlayerInstance->SetDisable4K(true);
}

TEST_F(PauseAtTests, SetBulkTimedMetaReportTest)
{
   mPlayerInstance->SetBulkTimedMetaReport(true);
}

TEST_F(PauseAtTests, SetRetuneForUnpairedDiscontinuityTest)
{
    mPlayerInstance->SetRetuneForUnpairedDiscontinuity(true);
}

TEST_F(PauseAtTests, SetRetuneForGSTInternalErrorTest)
{
    mPlayerInstance->SetRetuneForGSTInternalError(true);
}

TEST_F(PauseAtTests, SetAlternateContentsTest)
{
    std::string adBreakId = "adBreak1";
    std::string adId = "ad1";
    std::string url = "http://example.com/ad1";

    mPlayerInstance->SetAlternateContents(adBreakId, adId, url);
}

TEST_F(PauseAtTests, SetNetworkProxyTest)
{
    const char* proxy = "http://example-proxy.com:8080";

    mPlayerInstance->SetNetworkProxy(proxy);
}

TEST_F(PauseAtTests, SetLicenseReqProxyTest)
{
    const char* licenseProxy = "http://license-proxy.com:8080";

    mPlayerInstance->SetLicenseReqProxy(licenseProxy);
}
TEST_F(PauseAtTests, SetDownloadStallTimeoutTest)
{
    int stallTimeout = 100;
    mPlayerInstance->SetDownloadStallTimeout(stallTimeout);
}
TEST_F(PauseAtTests, SetDownloadStartTimeoutTest)
{
    int startTimeout = 50;
    mPlayerInstance->SetDownloadStartTimeout(startTimeout);
}
TEST_F(PauseAtTests, SetDownloadLowBWTimeoutTest)
{
    int lowBWTimeout = 10;
    mPlayerInstance->SetDownloadLowBWTimeout(lowBWTimeout);
}

TEST_F(PauseAtTests, SetPreferredSubtitleLanguageIdleState) {
    const char* language = "English";  
    mPlayerInstance->SetPreferredSubtitleLanguage(language);
}
TEST_F(PauseAtTests, SetParallelPlaylistDLTest)
{
    bool bValue = true;
    mPlayerInstance->SetParallelPlaylistDL(bValue);
}
TEST_F(PauseAtTests, SetParallelPlaylistRefreshTest)
{
    bool bValue = true;
    mPlayerInstance->SetParallelPlaylistRefresh(bValue);
}
TEST_F(PauseAtTests, SetWesterosSinkConfigTest)
{
    bool bValue = true;
    mPlayerInstance->SetWesterosSinkConfig(bValue);
}
TEST_F(PauseAtTests, SetLicenseCachingTest)
{
    bool bValue = true;
    mPlayerInstance->SetLicenseCaching(bValue);
}
TEST_F(PauseAtTests, SetOutputResolutionCheckTest)
{
    bool bValue = true;
    mPlayerInstance->SetOutputResolutionCheck(bValue);
}
TEST_F(PauseAtTests, SetMatchingBaseUrlConfigTest)
{
    bool bValue = true;
    mPlayerInstance->SetMatchingBaseUrlConfig(bValue);
}

TEST_F(PauseAtTests, SetNewABRConfigTest)
{
    bool bValue = true;
    mPlayerInstance->SetNewABRConfig(bValue);
}
TEST_F(PauseAtTests, SetPropagateUriParametersTest)
{
    bool bValue = true;
    mPlayerInstance->SetPropagateUriParameters(bValue);
}

TEST_F(PauseAtTests, ApplyArtificialDownloadDelayTest)
{
    mPlayerInstance->ApplyArtificialDownloadDelay(100);
}

TEST_F(PauseAtTests, SetSslVerifyPeerConfigTest)
{
    mPlayerInstance->SetSslVerifyPeerConfig(true);
}

TEST_F(PauseAtTests, SetAudioTrackTest)
{
    std::string language = "eng";
    std::string rendition = "main";
    std::string type = "audio";
    std::string codec = "aac";
    unsigned int channel = 2;
    std::string label = "English";

    mPlayerInstance->SetAudioTrack(language, rendition, type, codec, channel, label);
}
TEST_F(PauseAtTests, SetPreferredCodecTest)
{
    const char* codecList = "codec1,codec2,codec3";
    mPlayerInstance->SetPreferredCodec(codecList);
}

TEST_F(PauseAtTests, SetPreferredLabelsTest)
{
    const char* labelList = "label1,label2,label3";
    mPlayerInstance->SetPreferredLabels(labelList);
}

TEST_F(PauseAtTests, SetPreferredRenditionsTest)
{
    const char* renditionList = "rendition1,rendition2,rendition3";
    mPlayerInstance->SetPreferredRenditions(renditionList);
}
TEST_F(PauseAtTests, SetPreferredLanguagesTest)
{
    mPlayerInstance->SetPreferredLanguages("en,es,fr", "HD", "video", "h264", "main", nullptr);
}
TEST_F(PauseAtTests, SetPreferredTextLanguagesTest)
{
    mPlayerInstance->SetPreferredTextLanguages("en,es,fr");
}
TEST_F(PauseAtTests, GetPreferredDRMTest)
{
    DRMSystems expectedDRM = eDRM_WideVine;
    DRMSystems result = mPlayerInstance->GetPreferredDRM();
}
TEST_F(PauseAtTests, GetPreferredLanguagesTest)
{
     mPlayerInstance->SetPreferredLanguages("en,es,fr", "HD", "video", "h264", "main", nullptr);

    const char* preferredLanguages =  mPlayerInstance->GetPreferredLanguages();
    aampObj->preferredLanguagesString.c_str();
}
TEST_F(PauseAtTests, SetNewAdBreakerConfigTest)
{
    mPlayerInstance->SetNewAdBreakerConfig(true);

}

TEST_F(PauseAtTests, SetVideoTracksTest)
{
    std::vector<BitsPerSecond> bitrates = {500000, 1000000, 1500000};
    mPlayerInstance->SetVideoTracks(bitrates);
}
TEST_F(PauseAtTests, SetAppNameTest)
{
    mPlayerInstance->SetAppName("MyApp");
}

TEST_F(PauseAtTests, SetNativeCCRenderingTest)
{
    mPlayerInstance->SetNativeCCRendering(true);
}

TEST_F(PauseAtTests, SetTuneEventConfigTest)
{
    mPlayerInstance->SetTuneEventConfig(1);
}

TEST_F(PauseAtTests, EnableVideoRectangleTest)
{
    mPlayerInstance->EnableVideoRectangle(true);
}

TEST_F(PauseAtTests, SetAudioTrackTest1)
{
    mPlayerInstance->SetAudioTrack(1);

}
TEST_F(PauseAtTests, GetTextTrackTest)
{
    int trackId = mPlayerInstance->GetTextTrack();
}

TEST_F(PauseAtTests, SetCCStatusTest)
{
    mPlayerInstance->SetCCStatus(true);
    EXPECT_FALSE(mPlayerInstance->GetCCStatus()); 
}

TEST_F(PauseAtTests, SetTextStyleTest)
{
    mPlayerInstance->SetTextStyle("font-family: Arial; font-size: 16px; color: white;");
}
TEST_F(PauseAtTests, SetInitRampdownLimitTest)
{
    int limit = 10;
    mPlayerInstance->SetInitRampdownLimit(limit);
}
TEST_F(PauseAtTests, SetThumbnailTrackTest)
{
    int thumbIndex = 2;
    bool result = mPlayerInstance->SetThumbnailTrack(thumbIndex);
}
TEST_F(PauseAtTests, EnableSeekableRangeTest)
{
    bool bValue = true;
    mPlayerInstance->EnableSeekableRange(bValue);
}
TEST_F(PauseAtTests, SetReportVideoPTSTest)
{
    bool bValue = true;
    mPlayerInstance->SetReportVideoPTS(bValue);
}
TEST_F(PauseAtTests, DisableContentRestrictionsTest)
{
    long grace = 600;
    long time = 3600;
    bool eventChange = true;
    mPlayerInstance->DisableContentRestrictions(grace,time,eventChange);

}
TEST_F(PauseAtTests, EnableContentRestrictionsTest)
{
    mPlayerInstance->EnableContentRestrictions();
}
TEST_F(PauseAtTests, ManageAsyncTuneConfigTest)
{
    const char* mainManifestUrl = "http://example.com/main.mpd";
    mPlayerInstance->ManageAsyncTuneConfig(mainManifestUrl);
}
TEST_F(PauseAtTests, SetAsyncTuneConfigTest)
{
    bool bValue = true;
    mPlayerInstance->SetAsyncTuneConfig(bValue);
}
TEST_F(PauseAtTests, PersistBitRateOverSeekTest)
{
    bool bValue = true;
    mPlayerInstance->PersistBitRateOverSeek(bValue);
}
TEST_F(PauseAtTests, SetPausedBehaviorTest)
{
    int behavior = 100;
    mPlayerInstance->SetPausedBehavior(behavior);
}
TEST_F(PauseAtTests, SetUseAbsoluteTimelineTest)
{
    bool configState = true;
    mPlayerInstance->SetUseAbsoluteTimeline(configState);
}
TEST_F(PauseAtTests, SetRepairIframesTest)
{
    bool configState = true;
    mPlayerInstance->SetRepairIframes(configState);
}
TEST_F(PauseAtTests, XRESupportedTuneTest)
{
    bool xreSupported = true;
    mPlayerInstance->XRESupportedTune(xreSupported);
}
TEST_F(PauseAtTests, SetLicenseCustomDataTest)
{
    const char* customData = "customData"; 
    mPlayerInstance->SetLicenseCustomData(customData);
}
TEST_F(PauseAtTests, SetContentProtectionDataUpdateTimeoutTest)
{
    int timeout = 50;
    mPlayerInstance->SetContentProtectionDataUpdateTimeout(timeout);
}
TEST_F(PauseAtTests, SetRuntimeDRMConfigSupportTest)
{
    bool DynamicDRMSupported = true;
    mPlayerInstance->SetRuntimeDRMConfigSupport(DynamicDRMSupported);
}

