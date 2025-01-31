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

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <chrono>
#include "MockTSBStore.h"
#include "MockMediaStreamContext.h"
#include "MockPrivateInstanceAAMP.h"
#include "MockAampConfig.h"
#include "MockAampUtils.h"
#include "AampTSBSessionManager.h"
#include "AampConfig.h"
#include "AampLogManager.h"
#include <thread>
#include <unistd.h>

using ::testing::_;
using ::testing::DoAll;
using ::testing::InvokeWithoutArgs;
using ::testing::StrictMock;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::SaveArgPointee;
using ::testing::SetArgPointee;
using ::testing::NiceMock;
using ::testing::Invoke;

AampConfig *gpGlobalConfig{nullptr};

class FunctionalTests : public ::testing::Test
{
protected:
    AampTSBSessionManager *mAampTSBSessionManager;
    PrivateInstanceAAMP *aamp{};
    static constexpr const char *TEST_BASE_URL = "http://server/";
    static constexpr const char *TEST_DATA = "This is a dummy data";
    std::string TEST_PERIOD_ID = "1";
    std::shared_ptr<TSB::Store> mTSBStore;

    void SetUp() override
    {
        AampLogManager::setLogLevel(eLOGLEVEL_TRACE);   // Enable all levels of AAMP logging
        if (gpGlobalConfig == nullptr)
        {
            gpGlobalConfig = new AampConfig();
        }
        g_mockAampConfig = new NiceMock<MockAampConfig>();
        // Set TSB log level to TRACE
        EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_TsbLogLevel))
            .WillOnce(Return(static_cast<int>(TSB::LogLevel::TRACE)));

        aamp = new PrivateInstanceAAMP(gpGlobalConfig);
        mAampTSBSessionManager = new AampTSBSessionManager(aamp);
        TSB::Store::Config config;
        mTSBStore = std::make_shared<TSB::Store>(config, AampLogManager::aampLogger, TSB::LogLevel::TRACE);
        g_mockTSBStore = new MockTSBStore();
        g_mockMediaStreamContext = new StrictMock<MockMediaStreamContext>();
        g_mockPrivateInstanceAAMP = new StrictMock<MockPrivateInstanceAAMP>();
		g_mockAampUtils = new NiceMock<MockAampUtils>();

        EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetTSBStore(_,_,_)).WillRepeatedly(Return(mTSBStore));
        mAampTSBSessionManager->SetTsbLength(5);
        mAampTSBSessionManager->SetTsbLocation("/tmp");
        mAampTSBSessionManager->SetTsbMinFreePercentage(5);

        // Initialize necessary objects and configurations
        mAampTSBSessionManager->Init();
        // Wait to mWriteThread to start, we need to optimize this later
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }

    void TearDown() override
    {
        mAampTSBSessionManager->Flush();

        delete g_mockAampUtils;
        g_mockAampUtils = nullptr;

        delete g_mockPrivateInstanceAAMP;
        g_mockPrivateInstanceAAMP = nullptr;

        delete mAampTSBSessionManager;
        mAampTSBSessionManager = nullptr;

        mTSBStore = nullptr;

        delete g_mockTSBStore;
        g_mockTSBStore = nullptr;

        delete g_mockMediaStreamContext;
        g_mockMediaStreamContext = nullptr;

        delete g_mockAampConfig;
        g_mockAampConfig = nullptr;
    }

};

TEST_F(FunctionalTests, ConvertMediaType)
{
    AampMediaType convertedType = mAampTSBSessionManager->ConvertMediaType(eMEDIATYPE_INIT_VIDEO);
    EXPECT_EQ(convertedType, eMEDIATYPE_VIDEO);

    convertedType = mAampTSBSessionManager->ConvertMediaType(eMEDIATYPE_INIT_AUDIO);
    EXPECT_EQ(convertedType, eMEDIATYPE_AUDIO);

    convertedType = mAampTSBSessionManager->ConvertMediaType(eMEDIATYPE_INIT_SUBTITLE);
    EXPECT_EQ(convertedType, eMEDIATYPE_SUBTITLE);

    convertedType = mAampTSBSessionManager->ConvertMediaType(eMEDIATYPE_INIT_AUX_AUDIO);
    EXPECT_EQ(convertedType, eMEDIATYPE_AUX_AUDIO);

    convertedType = mAampTSBSessionManager->ConvertMediaType(eMEDIATYPE_INIT_IFRAME);
    EXPECT_EQ(convertedType, eMEDIATYPE_IFRAME);
}

TEST_F(FunctionalTests, TSBWriteTests)
{
    std::shared_ptr<CachedFragment> cachedFragment = std::make_shared<CachedFragment>();
    double FRAG_DURATION = 3.0;

    cachedFragment->initFragment = true;
    cachedFragment->duration = 0;
    cachedFragment->position = 0;
    cachedFragment->fragment.AppendBytes(TEST_DATA, strlen(TEST_DATA));

    // Add video init fragment to TSB successfullly
    std::string url = std::string(TEST_BASE_URL) + std::string("vinit.mp4");
    cachedFragment->type = eMEDIATYPE_INIT_VIDEO;

    EXPECT_CALL(*g_mockTSBStore, Write(url, TEST_DATA, strlen(TEST_DATA))).WillOnce(Return(TSB::Status::OK));
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetVidTimeScale()).WillRepeatedly(Return(1));
    mAampTSBSessionManager->EnqueueWrite(url, cachedFragment, TEST_PERIOD_ID);
    std::this_thread::sleep_for(std::chrono::milliseconds(25));

    // Add video init fragment to TSB which already exists
    EXPECT_CALL(*g_mockTSBStore, Write(url, TEST_DATA, strlen(TEST_DATA))).WillOnce(Return(TSB::Status::ALREADY_EXISTS));
    mAampTSBSessionManager->EnqueueWrite(url, cachedFragment, TEST_PERIOD_ID);
    std::this_thread::sleep_for(std::chrono::milliseconds(25));
    
    // Add video fragment 1 to TSB successfully
    url = std::string(TEST_BASE_URL) + std::string("video1.mp4");
    cachedFragment->type = eMEDIATYPE_VIDEO;
    cachedFragment->initFragment = 0;
    cachedFragment->duration = FRAG_DURATION;  // 3
    cachedFragment->position += FRAG_DURATION; // pos = 0

    EXPECT_CALL(*g_mockTSBStore, Write(url,_,_)).WillOnce(Return(TSB::Status::OK));
    mAampTSBSessionManager->EnqueueWrite(url, cachedFragment, TEST_PERIOD_ID); // initurlaudio
    std::this_thread::sleep_for(std::chrono::milliseconds(25));

    // Add video fragment 2 to TSB successfully
    url = std::string(TEST_BASE_URL) + std::string("video2.mp4");
    cachedFragment->position += FRAG_DURATION; // pos = 3

    EXPECT_CALL(*g_mockTSBStore, Write(url,_,_)).WillOnce(Return(TSB::Status::ALREADY_EXISTS));
    mAampTSBSessionManager->EnqueueWrite(url, cachedFragment, TEST_PERIOD_ID);
    std::this_thread::sleep_for(std::chrono::milliseconds(25));
    double TSBDuration = mAampTSBSessionManager->GetTotalStoreDuration(eMEDIATYPE_VIDEO);
    EXPECT_EQ(TSBDuration, FRAG_DURATION);

    // Add video fragment 3 to TSB which fails with no space and then writes on next iteration
    url = std::string(TEST_BASE_URL) + std::string("video3.mp4");
    std::string urlToRemove = std::string(TEST_BASE_URL) + std::string("video1.mp4");
    cachedFragment->position += FRAG_DURATION; // pos = 6

    EXPECT_CALL(*g_mockTSBStore, Write(url,_,_))
        .WillOnce(Return(TSB::Status::NO_SPACE))
        .WillOnce(Return(TSB::Status::OK));
    EXPECT_CALL(*g_mockTSBStore, Delete(urlToRemove)).Times(1);
    mAampTSBSessionManager->EnqueueWrite(url, cachedFragment, TEST_PERIOD_ID);
    std::this_thread::sleep_for(std::chrono::milliseconds(25));

    // Check the final TSB store duration is updated
    TSBDuration = mAampTSBSessionManager->GetTotalStoreDuration(eMEDIATYPE_VIDEO);
    EXPECT_EQ(TSBDuration, FRAG_DURATION);

}

TEST_F(FunctionalTests, Cullsegments)
{
    double FRAG_DURATION = 3.0;
    double MANIFEST_DURATION = 30.0;
    std::shared_ptr<CachedFragment> cachedFragment = std::make_shared<CachedFragment>();
    cachedFragment->initFragment = true;
    cachedFragment->fragment.AppendBytes(TEST_DATA, strlen(TEST_DATA));

    EXPECT_CALL(*g_mockTSBStore, Write(_,_,_)).WillRepeatedly(Return(TSB::Status::OK));
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetVidTimeScale()).WillRepeatedly(Return(1));
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetAudTimeScale()).WillRepeatedly(Return(1));

    std::string initUrl = std::string(TEST_BASE_URL) + std::string("init.mp4");
    cachedFragment->type = eMEDIATYPE_INIT_VIDEO;
    mAampTSBSessionManager->EnqueueWrite(initUrl, cachedFragment, TEST_PERIOD_ID);
    std::this_thread::sleep_for(std::chrono::milliseconds(25));

    cachedFragment->type = eMEDIATYPE_INIT_AUDIO;
    mAampTSBSessionManager->EnqueueWrite(initUrl, cachedFragment, TEST_PERIOD_ID);
    std::this_thread::sleep_for(std::chrono::milliseconds(25));

    std::string videoUrl = std::string(TEST_BASE_URL) + std::string("video.mp4");
    std::string audioUrl = std::string(TEST_BASE_URL) + std::string("audio.mp4");

    cachedFragment->duration = FRAG_DURATION;
    cachedFragment->initFragment = false;
    cachedFragment->type = eMEDIATYPE_VIDEO;
    mAampTSBSessionManager->EnqueueWrite(videoUrl, cachedFragment, TEST_PERIOD_ID);
    std::this_thread::sleep_for(std::chrono::milliseconds(25));

    cachedFragment->type = eMEDIATYPE_AUDIO;
    mAampTSBSessionManager->EnqueueWrite(audioUrl, cachedFragment, TEST_PERIOD_ID);
    std::this_thread::sleep_for(std::chrono::milliseconds(25));

    // Add another set of video and audio fragments to exceed TSB length
    cachedFragment->position += FRAG_DURATION;
    cachedFragment->absPosition += FRAG_DURATION;
    cachedFragment->type = eMEDIATYPE_VIDEO;
    mAampTSBSessionManager->EnqueueWrite(videoUrl, cachedFragment, TEST_PERIOD_ID);
    std::this_thread::sleep_for(std::chrono::milliseconds(25));

    cachedFragment->type = eMEDIATYPE_AUDIO;
    mAampTSBSessionManager->EnqueueWrite(audioUrl, cachedFragment, TEST_PERIOD_ID);
    std::this_thread::sleep_for(std::chrono::milliseconds(25));

    double TSBDuration = mAampTSBSessionManager->GetTotalStoreDuration(eMEDIATYPE_VIDEO);
    EXPECT_EQ(TSBDuration, FRAG_DURATION * 2);

    EXPECT_CALL(*g_mockTSBStore, Delete(videoUrl)).Times(1);
    EXPECT_CALL(*g_mockTSBStore, Delete(audioUrl)).Times(1);
    mAampTSBSessionManager->UpdateProgress(MANIFEST_DURATION, 0);

    // Check TSB store duration after culling. Only one fragment each should be present.
    TSBDuration = mAampTSBSessionManager->GetTotalStoreDuration(eMEDIATYPE_VIDEO);
    EXPECT_EQ(TSBDuration, FRAG_DURATION);
    TSBDuration = mAampTSBSessionManager->GetTotalStoreDuration(eMEDIATYPE_AUDIO);
    EXPECT_EQ(TSBDuration, FRAG_DURATION);
}

TEST_F(FunctionalTests, TSBReadTests)
{
    constexpr double FRAG_FIRST_POS = 99.0;
    constexpr double FRAG_FIRST_ABS_POS = 999.0;
    constexpr double FRAG_DURATION = 2.0;
    constexpr double FRAG_FIRST_PTS = 69.0;
    constexpr double FRAG_PTS_OFFSET = -50.0;
    size_t TEST_DATA_LEN = strlen(TEST_DATA);
    class MediaStreamContext videoCtx(eTRACK_VIDEO, NULL, aamp, "video");

    std::shared_ptr<CachedFragment> cachedFragment = std::make_shared<CachedFragment>();
    cachedFragment->initFragment = true;
    cachedFragment->fragment.AppendBytes(TEST_DATA, TEST_DATA_LEN);

    EXPECT_CALL(*g_mockTSBStore, Write(_,_,_)).WillRepeatedly(Return(TSB::Status::OK));
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetVidTimeScale()).WillRepeatedly(Return(1));
    EXPECT_CALL(*g_mockAampUtils, RecalculatePTS(eMEDIATYPE_INIT_VIDEO,_,_,_)).Times(1).WillOnce(Return(0.0));
    EXPECT_CALL(*g_mockAampUtils, RecalculatePTS(eMEDIATYPE_VIDEO,_,_,_)).Times(2).WillRepeatedly(Return(FRAG_FIRST_PTS));

    std::string initUrl = std::string(TEST_BASE_URL) + std::string("init.mp4");
    cachedFragment->type = eMEDIATYPE_INIT_VIDEO;
    mAampTSBSessionManager->EnqueueWrite(initUrl, cachedFragment, TEST_PERIOD_ID);
    std::this_thread::sleep_for(std::chrono::milliseconds(25));

    std::string videoUrl = std::string(TEST_BASE_URL) + std::string("video.mp4");
    cachedFragment->position = FRAG_FIRST_POS;
    cachedFragment->absPosition = FRAG_FIRST_ABS_POS;
    cachedFragment->duration = FRAG_DURATION;
    cachedFragment->PTSOffsetSec = FRAG_PTS_OFFSET;
    cachedFragment->initFragment = false;
    cachedFragment->type = eMEDIATYPE_VIDEO;
    mAampTSBSessionManager->EnqueueWrite(videoUrl, cachedFragment, TEST_PERIOD_ID);
    std::this_thread::sleep_for(std::chrono::milliseconds(25));

    cachedFragment->position += FRAG_DURATION;
    cachedFragment->absPosition += FRAG_DURATION;
    cachedFragment->PTSOffsetSec = FRAG_PTS_OFFSET;
    cachedFragment->type = eMEDIATYPE_VIDEO;
    mAampTSBSessionManager->EnqueueWrite(videoUrl, cachedFragment, TEST_PERIOD_ID);
    std::this_thread::sleep_for(std::chrono::milliseconds(25));

    double pos = FRAG_FIRST_ABS_POS;
    AAMPStatusType status = mAampTSBSessionManager->InvokeTsbReaders(pos, 1.0, eTUNETYPE_NEW_NORMAL);
    EXPECT_EQ(eAAMPSTATUS_OK, status);
    EXPECT_DOUBLE_EQ(FRAG_FIRST_ABS_POS, pos);

    EXPECT_TRUE(mAampTSBSessionManager->GetTsbReader(eMEDIATYPE_VIDEO)->TrackEnabled());
    EXPECT_FALSE(mAampTSBSessionManager->GetTsbReader(eMEDIATYPE_AUDIO)->TrackEnabled());

    EXPECT_CALL(*g_mockTSBStore, GetSize(_)).WillRepeatedly(Return(TEST_DATA_LEN));
    EXPECT_CALL(*g_mockTSBStore, Read(initUrl, _, _)).WillOnce(Return(TSB::Status::OK));
    EXPECT_CALL(*g_mockTSBStore, Read(videoUrl, _, _)).WillOnce(Return(TSB::Status::OK));

    EXPECT_CALL(*g_mockMediaStreamContext, CacheTsbFragment(_))
        .Times(2)
        .WillOnce(Return(true))
        .WillOnce(Invoke([](std::shared_ptr<CachedFragment> fragment)
        {
            EXPECT_DOUBLE_EQ(fragment->position, FRAG_FIRST_PTS + FRAG_PTS_OFFSET);
            return true;
        }));

    bool result = mAampTSBSessionManager->PushNextTsbFragment(&videoCtx);
    EXPECT_TRUE(result);
}
