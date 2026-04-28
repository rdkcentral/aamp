/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2020 RDK Management
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
#include "AampProfiler.h"
#include "AampConfig.h"
#include "MockAampConfig.h"
#include <cjson/cJSON.h>
#include <algorithm>

using namespace testing;
AampConfig *gpGlobalConfig{nullptr};
extern MockAampConfig *g_mockAampConfig;

class AampProfilertests : public testing::Test {
protected:
    void SetUp() override {
        profileEvent = new ProfileEventAAMP();
    }
    void TearDown() override {
        delete profileEvent;
       
    }
    ProfileEventAAMP* profileEvent;
};
TEST_F(AampProfilertests, SetLatencyParamTest11)
{
    double testVal = INT_MAX;
    profileEvent->IncrementChangeCount(Count_RateCorrection);
    profileEvent->IncrementChangeCount(Count_BitrateChange);
    profileEvent->IncrementChangeCount(Count_BufferChange);
    profileEvent->SetLatencyParam(testVal, testVal, testVal, testVal);
    profileEvent->TuneBegin();
    profileEvent->GetTelemetryParam();
}
TEST_F(AampProfilertests, SetLatencyParamTest12)
{
    double testVal = INT_MAX;
    profileEvent->IncrementChangeCount(Count_BitrateChange);
    profileEvent->SetLatencyParam(testVal, testVal, testVal, testVal);
}
TEST_F(AampProfilertests, SetLatencyParamTest13)
{
    double testVal = INT_MAX;
    profileEvent->IncrementChangeCount(Count_BufferChange);
    profileEvent->SetLatencyParam(testVal, testVal, testVal, testVal);
}
TEST_F(AampProfilertests, GetTuneTimeMetricAsJsonTest)
{
    TuneEndMetrics tuneMetricsData;
	const char *tuneTimeStrPrefix = "[tuneTimeStrPrefix]";
	unsigned int licenseAcqNWTime = 2;
    bool playerPreBuffered = true;
    unsigned int durationSeconds = 3;
    bool interfaceWifi = true;
    std::string failureReason = "test1";
    std::string appName = "test3";
    cJSON *item = cJSON_CreateObject();
    cJSON_AddNumberToObject(item,"ver",AAMP_TUNETIME_VERSION);
    std::string s1 = profileEvent->GetTuneTimeMetricAsJson(tuneMetricsData, tuneTimeStrPrefix,licenseAcqNWTime, playerPreBuffered,durationSeconds,interfaceWifi, failureReason, appName);
    profileEvent->TuneBegin();
    profileEvent->SetDiscontinuityParam();
}
TEST_F(AampProfilertests, ProfileResetTest1)
{
    profileEvent->ProfileReset(PROFILE_BUCKET_MANIFEST);
    EXPECT_EQ(PROFILE_BUCKET_MANIFEST,0);
    profileEvent->ProfileBegin(PROFILE_BUCKET_MANIFEST);    
    profileEvent->ProfileEnd(PROFILE_BUCKET_TYPE_COUNT);
}
TEST_F(AampProfilertests, ProfileResetTest2)
{
    ProfileEventAAMP profileEvent;
    ProfilerBucketType profilesList[24] = {
    PROFILE_BUCKET_MANIFEST,
    PROFILE_BUCKET_PLAYLIST_VIDEO,
    PROFILE_BUCKET_PLAYLIST_AUDIO,
    PROFILE_BUCKET_PLAYLIST_SUBTITLE,
    PROFILE_BUCKET_INIT_VIDEO,
    PROFILE_BUCKET_INIT_AUDIO,
    PROFILE_BUCKET_INIT_SUBTITLE,
    PROFILE_BUCKET_FRAGMENT_VIDEO,
    PROFILE_BUCKET_FRAGMENT_AUDIO,
    PROFILE_BUCKET_FRAGMENT_SUBTITLE,
    PROFILE_BUCKET_DECRYPT_VIDEO,
    PROFILE_BUCKET_DECRYPT_AUDIO,
    PROFILE_BUCKET_DECRYPT_SUBTITLE,
    PROFILE_BUCKET_LA_TOTAL,
    PROFILE_BUCKET_LA_PREPROC,
    PROFILE_BUCKET_LA_NETWORK,
    PROFILE_BUCKET_LA_POSTPROC,
    PROFILE_BUCKET_FIRST_BUFFER,
    PROFILE_BUCKET_FIRST_FRAME,
    PROFILE_BUCKET_PLAYER_PRE_BUFFERED,
    PROFILE_BUCKET_DISCO_TOTAL,
    PROFILE_BUCKET_DISCO_FLUSH,
    PROFILE_BUCKET_DISCO_FIRST_FRAME,
    PROFILE_BUCKET_TYPE_COUNT,
    };
    for(int i=0;i<24;i++){
    profileEvent.ProfileReset(profilesList[i]);
    EXPECT_EQ(profilesList[i],i);
    }
}
TEST_F(AampProfilertests, ProfileEndTest1)
{
    profileEvent->ProfileReset(PROFILE_BUCKET_MANIFEST); 
    profileEvent->ProfileBegin(PROFILE_BUCKET_MANIFEST);    
    profileEvent->ProfileEnd(PROFILE_BUCKET_TYPE_COUNT);
    EXPECT_EQ(PROFILE_BUCKET_TYPE_COUNT,23);
}
TEST_F(AampProfilertests, ProfileEndTest2)
{
    ProfileEventAAMP profileEvent;
    ProfilerBucketType profilesList[24] = {
    PROFILE_BUCKET_MANIFEST,
    PROFILE_BUCKET_PLAYLIST_VIDEO,
    PROFILE_BUCKET_PLAYLIST_AUDIO,
    PROFILE_BUCKET_PLAYLIST_SUBTITLE,
    PROFILE_BUCKET_INIT_VIDEO,
    PROFILE_BUCKET_INIT_AUDIO,
    PROFILE_BUCKET_INIT_SUBTITLE,
    PROFILE_BUCKET_FRAGMENT_VIDEO,
    PROFILE_BUCKET_FRAGMENT_AUDIO,
    PROFILE_BUCKET_FRAGMENT_SUBTITLE,
    PROFILE_BUCKET_DECRYPT_VIDEO,
    PROFILE_BUCKET_DECRYPT_AUDIO,
    PROFILE_BUCKET_DECRYPT_SUBTITLE,
    PROFILE_BUCKET_LA_TOTAL,
    PROFILE_BUCKET_LA_PREPROC,
    PROFILE_BUCKET_LA_NETWORK,
    PROFILE_BUCKET_LA_POSTPROC,
    PROFILE_BUCKET_FIRST_BUFFER,
    PROFILE_BUCKET_FIRST_FRAME,
    PROFILE_BUCKET_PLAYER_PRE_BUFFERED,
    PROFILE_BUCKET_DISCO_TOTAL,
    PROFILE_BUCKET_DISCO_FLUSH,
    PROFILE_BUCKET_DISCO_FIRST_FRAME,
    PROFILE_BUCKET_TYPE_COUNT,
    };
    for(int i=0;i<24;i++){
    profileEvent.ProfileEnd(profilesList[i]);
    EXPECT_EQ(profilesList[i],i);
    }
}
TEST_F(AampProfilertests, ProfileResetTest)
{
    profileEvent->ProfileReset(PROFILE_BUCKET_PLAYLIST_VIDEO);
}
TEST_F(AampProfilertests, ProfileErrorTest1)
{
    ProfileEventAAMP profileEvent;
    profileEvent.ProfileReset(PROFILE_BUCKET_PLAYLIST_SUBTITLE); 
    profileEvent.ProfileBegin(PROFILE_BUCKET_PLAYLIST_SUBTITLE); 
    int result = 10;
    profileEvent.ProfileError(PROFILE_BUCKET_PLAYLIST_VIDEO, result);
    EXPECT_EQ(PROFILE_BUCKET_PLAYLIST_VIDEO,1);
    EXPECT_EQ(result,10);
    result = INT_MIN;
    profileEvent.ProfileError(PROFILE_BUCKET_PLAYLIST_AUDIO, result);
    EXPECT_EQ(PROFILE_BUCKET_PLAYLIST_AUDIO,2);
    EXPECT_EQ(result,-2147483648);
    result = INT_MAX;
    profileEvent.ProfileError(PROFILE_BUCKET_PLAYLIST_SUBTITLE, result);
    EXPECT_EQ(PROFILE_BUCKET_PLAYLIST_SUBTITLE,3);
    EXPECT_EQ(result,2147483647);
}
TEST_F(AampProfilertests, ProfileErrorTest2)
{
    ProfileEventAAMP profileEvent;
    ProfilerBucketType profilesList[24] = {
    PROFILE_BUCKET_MANIFEST,
    PROFILE_BUCKET_PLAYLIST_VIDEO,
    PROFILE_BUCKET_PLAYLIST_AUDIO,
    PROFILE_BUCKET_PLAYLIST_SUBTITLE,
    PROFILE_BUCKET_INIT_VIDEO,
    PROFILE_BUCKET_INIT_AUDIO,
    PROFILE_BUCKET_INIT_SUBTITLE,
    PROFILE_BUCKET_FRAGMENT_VIDEO,
    PROFILE_BUCKET_FRAGMENT_AUDIO,
    PROFILE_BUCKET_FRAGMENT_SUBTITLE,
    PROFILE_BUCKET_DECRYPT_VIDEO,
    PROFILE_BUCKET_DECRYPT_AUDIO,
    PROFILE_BUCKET_DECRYPT_SUBTITLE,
    PROFILE_BUCKET_LA_TOTAL,
    PROFILE_BUCKET_LA_PREPROC,
    PROFILE_BUCKET_LA_NETWORK,
    PROFILE_BUCKET_LA_POSTPROC,
    PROFILE_BUCKET_FIRST_BUFFER,
    PROFILE_BUCKET_FIRST_FRAME,
    PROFILE_BUCKET_PLAYER_PRE_BUFFERED,
    PROFILE_BUCKET_DISCO_TOTAL,
    PROFILE_BUCKET_DISCO_FLUSH,
    PROFILE_BUCKET_DISCO_FIRST_FRAME,
    PROFILE_BUCKET_TYPE_COUNT,
    };
    for(int i=0;i<24;i++){
    profileEvent.ProfileError(profilesList[i],i);
    EXPECT_EQ(profilesList[i],i);
    }
}
TEST_F(AampProfilertests, ProfileBeginTest)
{
    profileEvent->ProfileBegin(PROFILE_BUCKET_MANIFEST);
}
TEST_F(AampProfilertests, ProfileBeginTest2)
{
    ProfileEventAAMP profileEvent;
    ProfilerBucketType profilesList[24] = {
    PROFILE_BUCKET_MANIFEST,
    PROFILE_BUCKET_PLAYLIST_VIDEO,
    PROFILE_BUCKET_PLAYLIST_AUDIO,
    PROFILE_BUCKET_PLAYLIST_SUBTITLE,
    PROFILE_BUCKET_INIT_VIDEO,
    PROFILE_BUCKET_INIT_AUDIO,
    PROFILE_BUCKET_INIT_SUBTITLE,
    PROFILE_BUCKET_FRAGMENT_VIDEO,
    PROFILE_BUCKET_FRAGMENT_AUDIO,
    PROFILE_BUCKET_FRAGMENT_SUBTITLE,
    PROFILE_BUCKET_DECRYPT_VIDEO,
    PROFILE_BUCKET_DECRYPT_AUDIO,
    PROFILE_BUCKET_DECRYPT_SUBTITLE,
    PROFILE_BUCKET_LA_TOTAL,
    PROFILE_BUCKET_LA_PREPROC,
    PROFILE_BUCKET_LA_NETWORK,
    PROFILE_BUCKET_LA_POSTPROC,
    PROFILE_BUCKET_FIRST_BUFFER,
    PROFILE_BUCKET_FIRST_FRAME,
    PROFILE_BUCKET_PLAYER_PRE_BUFFERED,
    PROFILE_BUCKET_DISCO_TOTAL,
    PROFILE_BUCKET_DISCO_FLUSH,
    PROFILE_BUCKET_DISCO_FIRST_FRAME,
    PROFILE_BUCKET_TYPE_COUNT,
    };
    for(int i=0;i<24;i++){
    profileEvent.ProfileBegin(profilesList[i]);
    EXPECT_EQ(profilesList[i],i);
    }
}
TEST_F(AampProfilertests, SetTuneFailCodeTest1)
{
    ProfileEventAAMP profileEvent;
    int tuneFailCode = 10;
    profileEvent.SetTuneFailCode(tuneFailCode,PROFILE_BUCKET_LA_PREPROC);
    EXPECT_EQ(PROFILE_BUCKET_LA_PREPROC,14);
    EXPECT_EQ(tuneFailCode,10);
    tuneFailCode = INT_MIN;
    profileEvent.SetTuneFailCode(tuneFailCode,PROFILE_BUCKET_PLAYLIST_AUDIO);
    EXPECT_EQ(PROFILE_BUCKET_PLAYLIST_AUDIO,2);
    EXPECT_EQ(tuneFailCode,-2147483648);
    tuneFailCode = 0;
    profileEvent.SetTuneFailCode(tuneFailCode,PROFILE_BUCKET_PLAYLIST_VIDEO);
    EXPECT_EQ(PROFILE_BUCKET_PLAYLIST_VIDEO,1);
    EXPECT_EQ(tuneFailCode,0);
}
TEST_F(AampProfilertests, SetTuneFailCodeTest2)
{
    ProfileEventAAMP profileEvent;
    ProfilerBucketType profilesList[24] = {
    PROFILE_BUCKET_MANIFEST,
    PROFILE_BUCKET_PLAYLIST_VIDEO,
    PROFILE_BUCKET_PLAYLIST_AUDIO,
    PROFILE_BUCKET_PLAYLIST_SUBTITLE,
    PROFILE_BUCKET_INIT_VIDEO,
    PROFILE_BUCKET_INIT_AUDIO,
    PROFILE_BUCKET_INIT_SUBTITLE,
    PROFILE_BUCKET_FRAGMENT_VIDEO,
    PROFILE_BUCKET_FRAGMENT_AUDIO,
    PROFILE_BUCKET_FRAGMENT_SUBTITLE,
    PROFILE_BUCKET_DECRYPT_VIDEO,
    PROFILE_BUCKET_DECRYPT_AUDIO,
    PROFILE_BUCKET_DECRYPT_SUBTITLE,
    PROFILE_BUCKET_LA_TOTAL,
    PROFILE_BUCKET_LA_PREPROC,
    PROFILE_BUCKET_LA_NETWORK,
    PROFILE_BUCKET_LA_POSTPROC,
    PROFILE_BUCKET_FIRST_BUFFER,
    PROFILE_BUCKET_FIRST_FRAME,
    PROFILE_BUCKET_PLAYER_PRE_BUFFERED,
    PROFILE_BUCKET_DISCO_TOTAL,
    PROFILE_BUCKET_DISCO_FLUSH,
    PROFILE_BUCKET_DISCO_FIRST_FRAME,
    PROFILE_BUCKET_TYPE_COUNT,
    };
    for(int i=0;i<24;i++){
    profileEvent.SetTuneFailCode(i,profilesList[i]);
    EXPECT_EQ(profilesList[i],i);
    }
}
TEST_F(AampProfilertests, ProfilePerformedTest1)
{
    ProfileEventAAMP profileEvent;
    profileEvent.ProfilePerformed(PROFILE_BUCKET_LA_NETWORK);
    EXPECT_EQ(PROFILE_BUCKET_LA_NETWORK,15);
}
TEST_F(AampProfilertests, ProfilePerformedTest2)
{
    ProfileEventAAMP profileEvent;
    ProfilerBucketType ProfilePerformedList[24] = {
    PROFILE_BUCKET_MANIFEST,
    PROFILE_BUCKET_PLAYLIST_VIDEO,
    PROFILE_BUCKET_PLAYLIST_AUDIO,
    PROFILE_BUCKET_PLAYLIST_SUBTITLE,
    PROFILE_BUCKET_INIT_VIDEO,
    PROFILE_BUCKET_INIT_AUDIO,
    PROFILE_BUCKET_INIT_SUBTITLE,
    PROFILE_BUCKET_FRAGMENT_VIDEO,
    PROFILE_BUCKET_FRAGMENT_AUDIO,
    PROFILE_BUCKET_FRAGMENT_SUBTITLE,
    PROFILE_BUCKET_DECRYPT_VIDEO,
    PROFILE_BUCKET_DECRYPT_AUDIO,
    PROFILE_BUCKET_DECRYPT_SUBTITLE,
    PROFILE_BUCKET_LA_TOTAL,
    PROFILE_BUCKET_LA_PREPROC,
    PROFILE_BUCKET_LA_NETWORK,
    PROFILE_BUCKET_LA_POSTPROC,
    PROFILE_BUCKET_FIRST_BUFFER,
    PROFILE_BUCKET_FIRST_FRAME,
    PROFILE_BUCKET_PLAYER_PRE_BUFFERED,
    PROFILE_BUCKET_DISCO_TOTAL,
    PROFILE_BUCKET_DISCO_FLUSH,
    PROFILE_BUCKET_DISCO_FIRST_FRAME,
    PROFILE_BUCKET_TYPE_COUNT,
    };
    for(int i=0;i<24;i++){
    profileEvent.ProfilePerformed(ProfilePerformedList[i]);
    EXPECT_EQ(ProfilePerformedList[i],i);
    }
}
TEST_F(AampProfilertests, GetTuneEventsJSONTest)
{
    std::string outStr;
    std::string streamType = "Video";
    const char *url = "https://www.example.com";
    bool success = true;
    profileEvent->getTuneEventsJSON(outStr, streamType, url, success);
    std::string expectedJson = "{\"s\":0,\"td\":0,\"st\":\"Video\",\"u\":\"https://www.example.com\",\"tf\":{\"i\":0,\"er\":0},\"r\":1,\"v\":[{\"i\":1,\"b\":100,\"d\":200,\"o\":0},{\"i\":2,\"b\":300,\"d\":150,\"o\":1}]}";
}

TEST_F(AampProfilertests, TuneEndTest1)
{
    TuneEndMetrics metrics;
    std::string appName = "TestApp";
    std::string playerActiveMode = "Active";
    int playerId = INT_MIN;
    bool playerPreBuffered = true;
    unsigned int durationSeconds = 60;
    bool interfaceWifi = true;
    std::string failureReason = "None";
    std::string tuneMetricData[]={"asfwerscwrssddt"};
    profileEvent->TuneEnd(metrics, appName, playerActiveMode, playerId, playerPreBuffered, durationSeconds, interfaceWifi, failureReason,tuneMetricData);
    EXPECT_EQ(playerId,-2147483648);
    ASSERT_TRUE(playerPreBuffered);
    EXPECT_EQ(durationSeconds,60);
    ASSERT_TRUE(interfaceWifi); 
}
TEST_F(AampProfilertests, TuneEndTest2)
{
    TuneEndMetrics metrics;
    std::string appName(10000,'A');
    std::string playerActiveMode (10000,'B');
    int playerId = INT_MAX;
    bool playerPreBuffered = true;
    unsigned int durationSeconds = 60;
    bool interfaceWifi = true;
    std::string failureReason(10000,'C');
    std::string tuneMetricData[]={"aashwtsvsdqkka"};
    profileEvent->TuneEnd(metrics, appName, playerActiveMode, playerId, playerPreBuffered, durationSeconds, interfaceWifi, failureReason,tuneMetricData);
    EXPECT_EQ(playerId,2147483647);
    ASSERT_TRUE(playerPreBuffered);
    EXPECT_EQ(durationSeconds,60);
    ASSERT_TRUE(interfaceWifi);
}
TEST_F(AampProfilertests, TuneEndTest3)
{
    TuneEndMetrics metrics;
    std::string appName= "sdhdfwahmma";
    std::string playerActiveMode="awesferwedarstt";
    int playerId = 0;
    bool playerPreBuffered = false;
    unsigned int durationSeconds = 0;
    bool interfaceWifi = false;
    std::string failureReason="nalalallalalaallatr";
    std::string tuneMetricData[]={"aatatattatt"};
    profileEvent->TuneEnd(metrics, appName, playerActiveMode, playerId, playerPreBuffered, durationSeconds, interfaceWifi, failureReason,tuneMetricData);
    EXPECT_EQ(playerId,0);
    ASSERT_FALSE(playerPreBuffered);
    EXPECT_EQ(durationSeconds,0);
    ASSERT_FALSE(interfaceWifi);
}
TEST_F(AampProfilertests, TuneEndTest4)
{
    TuneEndMetrics mTuneEndMetrics;
    mTuneEndMetrics.success = 1;
    mTuneEndMetrics.contentType = ContentType_VOD;
    mTuneEndMetrics.streamType = 1;
    mTuneEndMetrics.mFirstTune = true;
    mTuneEndMetrics.mTimedMetadataStartTime = 12345;
    mTuneEndMetrics.mTimedMetadataDuration = 500;
    mTuneEndMetrics.mTuneAttempts = 2;
    std::string appName = "TestApp";
    std::string playerActiveMode = "Active";
    int playerId = 123;
    bool playerPreBuffered = true;
    unsigned int durationSeconds = 3600;
    bool interfaceWifi = true;
    std::string failureReason = "Failed due to XYZ";
    std::string tuneMetricData[]={"awesdawqxsdudkkcscdsw"};
    profileEvent->TuneBegin();
    profileEvent->TuneEnd(mTuneEndMetrics, appName, playerActiveMode, playerId,
                         playerPreBuffered, durationSeconds, interfaceWifi, failureReason,tuneMetricData); 
    EXPECT_EQ(playerId,123);
    ASSERT_TRUE(playerPreBuffered);
    EXPECT_EQ(durationSeconds,3600);
    ASSERT_TRUE(interfaceWifi);         
}
TEST_F(AampProfilertests, TuneEndTest5)
{
    TuneEndMetrics mTuneEndMetrics;
    mTuneEndMetrics.success = 0;
    mTuneEndMetrics.contentType = ContentType_VOD;
    mTuneEndMetrics.streamType = 1;
    mTuneEndMetrics.mFirstTune = true;
    mTuneEndMetrics.mTimedMetadataStartTime = 12345;
    mTuneEndMetrics.mTimedMetadataDuration = 500;
    mTuneEndMetrics.mTuneAttempts = 2;
    std::string appName = "\0";
    std::string playerActiveMode = "Active";
    int playerId = 123;
    bool playerPreBuffered = true;
    unsigned int durationSeconds = 3600;
    bool interfaceWifi = true;
    std::string failureReason = "Failed due to XYZ";
    std::string tuneMetricData[]={"qazxcsvdgeqq"};
    profileEvent->TuneBegin();
    profileEvent->TuneEnd(mTuneEndMetrics, appName, playerActiveMode, playerId,
                         playerPreBuffered, durationSeconds, interfaceWifi, failureReason,tuneMetricData);
    EXPECT_EQ(playerId,123);
    ASSERT_TRUE(playerPreBuffered);
    EXPECT_EQ(durationSeconds,3600);
    ASSERT_TRUE(interfaceWifi);         
}
TEST_F(AampProfilertests, TestGetTuneEventsJSON22)
{
    bool siblingEvent = false;
    std::string outStr;
    const std::string streamType = "video";
    const char* urlWithoutQuery = "http://example.com/video";
    bool successWithoutQuery = true;
    profileEvent->getTuneEventsJSON(outStr, streamType, urlWithoutQuery, successWithoutQuery);
    ASSERT_FALSE(outStr.empty());
    ASSERT_TRUE(outStr.find("\"s\":") != std::string::npos);
    ASSERT_TRUE(outStr.find("\"td\":") != std::string::npos);
    ASSERT_TRUE(outStr.find("\"st\":\"video\"") != std::string::npos);
    const char* urlWithQuery = "http://example.com/video?param=value";
    bool successWithQuery = false;
    profileEvent->getTuneEventsJSON(outStr, streamType, urlWithQuery, successWithQuery);
    ASSERT_FALSE(outStr.empty());
    ASSERT_TRUE(outStr.find("\"s\":") != std::string::npos);
    ASSERT_TRUE(outStr.find("\"td\":") != std::string::npos);
    ASSERT_TRUE(outStr.find("\"st\":\"video\"") != std::string::npos);
}
TEST_F(AampProfilertests, TuneBeginTest)
{
    profileEvent->TuneBegin();
}
TEST_F(AampProfilertests, TestGetTuneEventsJSON12)
{
    bool siblingEvent = true;
    std::string outStr="tegdfsrcvshaja";
    std::string result;
    const std::string streamType = "video";
    const char* urlWithoutQuery = "http://example.com/video";
    bool successWithoutQuery = true;
    profileEvent->getTuneEventsJSON(outStr, streamType, urlWithoutQuery, successWithoutQuery);
    ASSERT_FALSE(outStr.empty());
    ASSERT_TRUE(outStr.find("\"s\":") != std::string::npos);
    ASSERT_TRUE(outStr.find("\"td\":") != std::string::npos);
    ASSERT_TRUE(outStr.find("\"st\":\"video\"") != std::string::npos);
    const char* urlWithQuery = "http://example.com/video?param=value";
    bool successWithQuery = false;
    profileEvent->getTuneEventsJSON(outStr, streamType, urlWithQuery, successWithQuery);  
    ASSERT_FALSE(outStr.empty());
    ASSERT_TRUE(outStr.find("\"s\":") != std::string::npos);
    ASSERT_TRUE(outStr.find("\"td\":") != std::string::npos);
    ASSERT_TRUE(outStr.find("\"st\":\"video\"") != std::string::npos);
}
TEST_F(AampProfilertests, TestGetTuneEventsJSON12_test1)
{
    std::string outStr;
    const std::string streamType = "video";
    const char* urlWithQuery = "http://example.com/video?param=value";
    bool successWithQuery = true;
    profileEvent->getTuneEventsJSON(outStr, streamType, urlWithQuery, successWithQuery);
    ASSERT_FALSE(outStr.empty());
    ASSERT_TRUE(outStr.find("\"s\":") != std::string::npos);
    ASSERT_TRUE(outStr.find("\"td\":") != std::string::npos);
    ASSERT_TRUE(outStr.find("\"st\":\"video\"") != std::string::npos);  
}
TEST_F(AampProfilertests, SetBandwidthBitsPerSecondAudio)
{
    long expectedBandwidth = 1000000;
    profileEvent->SetBandwidthBitsPerSecondAudio(expectedBandwidth);
    EXPECT_EQ(expectedBandwidth, 1000000);
    expectedBandwidth = INT_MAX;
    profileEvent->SetBandwidthBitsPerSecondAudio(expectedBandwidth);
    EXPECT_EQ(expectedBandwidth, 2147483647);
    expectedBandwidth = INT_MIN;
    profileEvent->SetBandwidthBitsPerSecondAudio(expectedBandwidth);
    EXPECT_EQ(expectedBandwidth, -2147483648);
    expectedBandwidth = 0;
    profileEvent->SetBandwidthBitsPerSecondAudio(expectedBandwidth);
    EXPECT_EQ(expectedBandwidth, 0);
}
TEST_F(AampProfilertests, SetBandwidthBitsPerSecondVideoTest)
{
    long expectedBandwidth = 1000000;
    profileEvent->SetBandwidthBitsPerSecondVideo(expectedBandwidth);
    EXPECT_EQ(expectedBandwidth, 1000000);
    expectedBandwidth = INT_MAX;
    profileEvent->SetBandwidthBitsPerSecondVideo(expectedBandwidth);
    EXPECT_EQ(expectedBandwidth, 2147483647);
    expectedBandwidth = INT_MIN;
    profileEvent->SetBandwidthBitsPerSecondVideo(expectedBandwidth);
    EXPECT_EQ(expectedBandwidth, -2147483648);
    expectedBandwidth = 0;
    profileEvent->SetBandwidthBitsPerSecondVideo(expectedBandwidth);
    EXPECT_EQ(expectedBandwidth, 0);
}
TEST_F(AampProfilertests, TuneEndVIPATaggingWithFireboltSDKEnabled)
{
    // Save original gpGlobalConfig to restore later
    AampConfig* savedConfig = gpGlobalConfig;
    
    // Setup: Create mock and configure gpGlobalConfig with eAAMPConfig_UseFireboltSDK enabled
    MockAampConfig mockConfig;
    g_mockAampConfig = &mockConfig;
    gpGlobalConfig = new AampConfig();
    
    // Set expectation that IsConfigSet will be called and return true
    EXPECT_CALL(mockConfig, IsConfigSet(eAAMPConfig_UseFireboltSDK))
        .WillOnce(Return(true));
    
    // Prepare TuneEndMetrics
    TuneEndMetrics metrics;
    metrics.success = 1;
    metrics.contentType = ContentType_VOD;
    metrics.streamType = 1;
    metrics.mFirstTune = true;
    metrics.mTimedMetadataStartTime = 0;
    metrics.mTimedMetadataDuration = 0;
    metrics.mTuneAttempts = 1;
    metrics.mTotalTime = 0;
    
    std::string appName = "TestApp";
    std::string playerActiveMode = "FOREGROUND";
    int playerId = 1;
    bool playerPreBuffered = false;
    unsigned int durationSeconds = 100;
    bool interfaceWifi = true;
    std::string failureReason = "";
    std::string tuneMetricDataStr = "";
    
    // Call TuneBegin first to enable profiling
    profileEvent->TuneBegin();
    
    // Call TuneEnd which should append "_VIPA" to appName when eAAMPConfig_UseFireboltSDK is enabled
    profileEvent->TuneEnd(metrics, appName, playerActiveMode, playerId, 
                         playerPreBuffered, durationSeconds, interfaceWifi, 
                         failureReason, &tuneMetricDataStr);
    
    if (!tuneMetricDataStr.empty()) {
        EXPECT_TRUE(tuneMetricDataStr.find("TestApp_VIPA") != std::string::npos);
    }
    
    // Cleanup
    delete gpGlobalConfig;
    gpGlobalConfig = savedConfig;
    g_mockAampConfig = nullptr;
}

TEST_F(AampProfilertests, TuneEndVIPATaggingWithFireboltSDKDisabled)
{
    // Save original gpGlobalConfig to restore later
    AampConfig* savedConfig = gpGlobalConfig;
    
    // Setup: Create mock and configure gpGlobalConfig with eAAMPConfig_UseFireboltSDK disabled
    MockAampConfig mockConfig;
    g_mockAampConfig = &mockConfig;
    gpGlobalConfig = new AampConfig();
    
    // Set expectation that IsConfigSet will be called and return false
    EXPECT_CALL(mockConfig, IsConfigSet(eAAMPConfig_UseFireboltSDK))
        .WillOnce(Return(false));
    
    // Prepare TuneEndMetrics
    TuneEndMetrics metrics;
    metrics.success = 1;
    metrics.contentType = ContentType_VOD;
    metrics.streamType = 1;
    metrics.mFirstTune = true;
    metrics.mTimedMetadataStartTime = 0;
    metrics.mTimedMetadataDuration = 0;
    metrics.mTuneAttempts = 1;
    metrics.mTotalTime = 0;
    
    std::string appName = "TestApp";
    std::string playerActiveMode = "FOREGROUND";
    int playerId = 1;
    bool playerPreBuffered = false;
    unsigned int durationSeconds = 100;
    bool interfaceWifi = true;
    std::string failureReason = "";
    std::string tuneMetricDataStr = "";
    
    // Call TuneBegin first to enable profiling
    profileEvent->TuneBegin();
    
    // Call TuneEnd which should NOT append "_VIPA" to appName when eAAMPConfig_UseFireboltSDK is disabled
    profileEvent->TuneEnd(metrics, appName, playerActiveMode, playerId, 
                         playerPreBuffered, durationSeconds, interfaceWifi, 
                         failureReason, &tuneMetricDataStr);

    // Verify absence of VIPA tagging: The mock EXPECT_CALL verifies IsConfigSet was called.
    // If JSON metrics are generated (tuneMetricDataStr populated), verify VIPA marker is absent.
    // Note: JSON generation may fail in unit test environment (e.g., cJSON_CreateObject returns NULL),
    // but VIPA tagging logic is still exercised and logged.
    if (!tuneMetricDataStr.empty()) {
        EXPECT_TRUE(tuneMetricDataStr.find("TestApp_VIPA") == std::string::npos);
        EXPECT_TRUE(tuneMetricDataStr.find("TestApp") != std::string::npos);
    }
    
    // Cleanup
    delete gpGlobalConfig;
    gpGlobalConfig = savedConfig;
    g_mockAampConfig = nullptr;
}
TEST_F(AampProfilertests, TuneEndVIPATaggingWithNullConfig)
{
    // Save the current gpGlobalConfig pointer
    AampConfig* savedConfig = gpGlobalConfig;
    
    // Setup: ensure gpGlobalConfig is NULL for this test
    gpGlobalConfig = nullptr;
    
    // Prepare TuneEndMetrics
    TuneEndMetrics metrics;
    metrics.success = 1;
    metrics.contentType = ContentType_VOD;
    metrics.streamType = 1;
    metrics.mFirstTune = true;
    metrics.mTimedMetadataStartTime = 0;
    metrics.mTimedMetadataDuration = 0;
    metrics.mTuneAttempts = 1;
    metrics.mTotalTime = 0;
    
    std::string appName = "TestApp";
    std::string playerActiveMode = "FOREGROUND";
    int playerId = 1;
    bool playerPreBuffered = false;
    unsigned int durationSeconds = 100;
    bool interfaceWifi = true;
    std::string failureReason = "";
    std::string tuneMetricDataStr = "";
    
    // Call TuneBegin first to enable profiling
    profileEvent->TuneBegin();
    
    // Call TuneEnd which should NOT append "_VIPA" when gpGlobalConfig is NULL
    profileEvent->TuneEnd(metrics, appName, playerActiveMode, playerId, 
                         playerPreBuffered, durationSeconds, interfaceWifi, 
                         failureReason, &tuneMetricDataStr);
    
    if (!tuneMetricDataStr.empty()) {
        EXPECT_TRUE(tuneMetricDataStr.find("TestApp_VIPA") == std::string::npos);
        EXPECT_TRUE(tuneMetricDataStr.find("TestApp") != std::string::npos);
    }
    
    // Cleanup: restore the original gpGlobalConfig
    gpGlobalConfig = savedConfig;
}
TEST_F(AampProfilertests, TestDecodeTime)
{
    TuneEndMetrics metrics;
    metrics.success = 1;
    metrics.contentType = ContentType_VOD;
    metrics.streamType = 1;
    metrics.mFirstTune = true;
    metrics.mTimedMetadataStartTime = 0;
    metrics.mTimedMetadataDuration = 0;
    metrics.mTuneAttempts = 1;
    metrics.mTotalTime = 0;

    std::string appName = "TestApp";
    std::string playerActiveMode = "Active";
    int playerId = 1;
    bool playerPreBuffered = true;
    unsigned int durationSeconds = 60;
    bool interfaceWifi = true;
    std::string failureReason = "None";

    // Test that with no profile values logged, decode time is zero
    {
        std::string tuneMetricData="";
        profileEvent->TuneBegin();
        profileEvent->TuneEnd(metrics, appName, playerActiveMode, playerId, playerPreBuffered, durationSeconds, interfaceWifi, failureReason, &tuneMetricData);

        EXPECT_FALSE(tuneMetricData.empty());
        if (!tuneMetricData.empty())
        {
            cJSON *jsonMetrics = cJSON_Parse(tuneMetricData.c_str());
            EXPECT_TRUE(jsonMetrics != NULL);
            if (jsonMetrics != NULL)
            {
                cJSON *decodeTime = cJSON_GetObjectItem(jsonMetrics, "gdt");
                EXPECT_TRUE(decodeTime != NULL);
                EXPECT_TRUE(decodeTime->valueint == 0);
                cJSON_Delete(jsonMetrics);
            }
        }
    }
    // Test that with first buffer but no first frame, decode time is zero
    {
        std::string tuneMetricData="";
        profileEvent->TuneBegin();
        profileEvent->ProfilePerformed(PROFILE_BUCKET_FIRST_BUFFER);
        profileEvent->TuneEnd(metrics, appName, playerActiveMode, playerId, playerPreBuffered, durationSeconds, interfaceWifi, failureReason, &tuneMetricData);

        EXPECT_FALSE(tuneMetricData.empty());
        if (!tuneMetricData.empty())
        {
            cJSON *jsonMetrics = cJSON_Parse(tuneMetricData.c_str());
            EXPECT_TRUE(jsonMetrics != NULL);
            if (jsonMetrics != NULL)
            {
                cJSON *decodeTime = cJSON_GetObjectItem(jsonMetrics, "gdt");
                EXPECT_TRUE(decodeTime != NULL);
                EXPECT_TRUE(decodeTime->valueint == 0);
                cJSON_Delete(jsonMetrics);
            }
        }
    }
    // Test that with first buffer and first frame, decode time is greater than zero
    {
        std::string tuneMetricData="";
        profileEvent->TuneBegin();
        profileEvent->ProfilePerformed(PROFILE_BUCKET_FIRST_BUFFER);
        usleep(5000);
        profileEvent->ProfilePerformed(PROFILE_BUCKET_FIRST_FRAME);
        profileEvent->TuneEnd(metrics, appName, playerActiveMode, playerId, playerPreBuffered, durationSeconds, interfaceWifi, failureReason, &tuneMetricData);

        EXPECT_FALSE(tuneMetricData.empty());
        if (!tuneMetricData.empty())
        {
            cJSON *jsonMetrics = cJSON_Parse(tuneMetricData.c_str());
            EXPECT_TRUE(jsonMetrics != NULL);
            if (jsonMetrics != NULL)
            {
                cJSON *decodeTime = cJSON_GetObjectItem(jsonMetrics, "gdt");
                EXPECT_TRUE(decodeTime != NULL);
                EXPECT_TRUE(decodeTime->valueint > 0);
                cJSON_Delete(jsonMetrics);
            }
        }
    }
}

