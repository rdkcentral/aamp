
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
#include <gtest/gtest.h>
#include "MediaStreamContext.h"
#include "fragmentcollector_mpd.h"
#include "AampMemoryUtils.h"
#include "isobmff/isobmffbuffer.h"
#include "AampCacheHandler.h"
#include "../priv_aamp.h"
#include "AampDRMLicPreFetcherInterface.h"
#include "AampConfig.h"
#include "MockAampConfig.h"

#include "fragmentcollector_mpd.h"
#include "StreamAbstractionAAMP.h"

using namespace testing;

AampConfig *gpGlobalConfig{nullptr};

class MediaStreamContextTest : public testing::Test
{
    protected:
    void SetUp() override
    {
        if(gpGlobalConfig == nullptr)
        {
            gpGlobalConfig =  new AampConfig();
        }
        mStreamAbstractionAAMP_MPD = new StreamAbstractionAAMP_MPD(NULL,123.45,12.34);
        mPrivateInstanceAAMP = new PrivateInstanceAAMP(gpGlobalConfig);
        mMediaStreamContext = new MediaStreamContext(eTRACK_VIDEO,mStreamAbstractionAAMP_MPD,mPrivateInstanceAAMP,"SAMPLETEXT");
        g_mockAampConfig = new MockAampConfig();
    }
    
    void TearDown() override
    {
        delete mPrivateInstanceAAMP;
        mPrivateInstanceAAMP = nullptr;
        
        delete mStreamAbstractionAAMP_MPD;
        mStreamAbstractionAAMP_MPD = nullptr;
        
        delete mMediaStreamContext;
        mMediaStreamContext = nullptr;
        
        delete g_mockAampConfig;
        g_mockAampConfig = nullptr;
    }
    public:
    StreamAbstractionAAMP_MPD *mStreamAbstractionAAMP_MPD;
    PrivateInstanceAAMP *mPrivateInstanceAAMP;
    MediaStreamContext *mMediaStreamContext;
};

TEST_F(MediaStreamContextTest, GetContextTest)
{
    //Act:call GetContext function
    mMediaStreamContext->GetContext();
}

TEST_F(MediaStreamContextTest, CacheFragmentChunkTest)
{
    //Act:call CacheFragmentChunk fucntion
    bool retResult = mMediaStreamContext->CacheFragmentChunk(eMEDIATYPE_VIDEO,NULL, 12345678,"remoteUrl",123456789);
    EXPECT_FALSE(retResult);
}

TEST_F(MediaStreamContextTest,ProcessPlaylistTest)
{
    AampGrowableBuffer newPlaylist("download-PlaylistManifest");
    //Act:call ProcessPlaylist function
    mMediaStreamContext->ProcessPlaylist(newPlaylist,1);
}

TEST_F(MediaStreamContextTest,SignalTrickModeDiscontinuityTest)
{
    //Act SignalTrickModeDiscontinuity function
    mMediaStreamContext->SignalTrickModeDiscontinuity();
}

TEST_F(MediaStreamContextTest, IsAtEndOfTrackTest)
{
    //Act:call IsAtEndOfTrack function
    bool eosReachedResult = mMediaStreamContext->IsAtEndOfTrack();
    //Assert:check eosReachedResult value
    EXPECT_FALSE(eosReachedResult);
}

TEST_F(MediaStreamContextTest, PlaylistUrlTest)
{
    //Act:call GetPlaylistUrl function
    string mPlaylistUrlResult = mMediaStreamContext->GetPlaylistUrl();
    //Assert:check mPlaylistUrlResult value
    EXPECT_EQ(mPlaylistUrlResult,"");
}

TEST_F(MediaStreamContextTest, PlaylistUrlTest_1)
{
        //Act:call SetEffectivePlaylistUrl fucntion
    mMediaStreamContext->SetEffectivePlaylistUrl("https://sampleurl");
    //Assert:check for mEffectiveUrl value
    EXPECT_EQ(mMediaStreamContext->GetEffectivePlaylistUrl(),mMediaStreamContext->mEffectiveUrl);

    EXPECT_EQ(mMediaStreamContext->mEffectiveUrl,"https://sampleurl");
}

TEST_F(MediaStreamContextTest, PlaylistUrlTest_2)
{
    //Act:call SetEffectivePlaylistUrl fucntion
    mMediaStreamContext->SetEffectivePlaylistUrl("https://sampleurlQWERTYTIPI[asfdfghjkklzxvxcnbcbmv");
    //Assert:check for mEffectiveUrl value
    EXPECT_EQ(mMediaStreamContext->GetEffectivePlaylistUrl(),mMediaStreamContext->mEffectiveUrl);
}

TEST_F(MediaStreamContextTest, PlaylistUrlTest_3)
{
    //Act:call SetEffectivePlaylistUrl fucntion
    mMediaStreamContext->SetEffectivePlaylistUrl("https://sampleurl@@@@@@@@@@@@@3#################4$%^^&&&&**QWERTYTIPI[asfdfghjkklzxvxcnbcbmv");
    //Assert:check for mEffectiveUrl value
    EXPECT_EQ(mMediaStreamContext->GetEffectivePlaylistUrl(),mMediaStreamContext->mEffectiveUrl);

}

TEST_F(MediaStreamContextTest, PlaylistUrlTest_4)
{
    //Act:call SetEffectivePlaylistUrl fucntion
    mMediaStreamContext->SetEffectivePlaylistUrl("");
    //Assert:check for mEffectiveUrl value
    EXPECT_EQ(mMediaStreamContext->GetEffectivePlaylistUrl(),mMediaStreamContext->mEffectiveUrl);
}

TEST_F(MediaStreamContextTest, PlaylistDownloadTimeTest)
{
    //Act:call SetLastPlaylistDownloadTime fucntion
    mMediaStreamContext->SetLastPlaylistDownloadTime(29955777888);
    //Assert:check mLastPlaylistDownloadTimeMs value
    EXPECT_EQ(mMediaStreamContext->GetLastPlaylistDownloadTime(),mMediaStreamContext->context->mLastPlaylistDownloadTimeMs);

    EXPECT_EQ(mMediaStreamContext->context->mLastPlaylistDownloadTimeMs,29955777888);
}

TEST_F(MediaStreamContextTest, PlaylistDownloadTimeTest_1)
{
    //Act:call SetLastPlaylistDownloadTime fucntion
    mMediaStreamContext->SetLastPlaylistDownloadTime(299557778882352);
    //Assert:check mLastPlaylistDownloadTimeMs value
    EXPECT_EQ(mMediaStreamContext->GetLastPlaylistDownloadTime(),mMediaStreamContext->context->mLastPlaylistDownloadTimeMs);
}

TEST_F(MediaStreamContextTest, PlaylistDownloadTimeTest_2)
{
    //Act:call SetLastPlaylistDownloadTime fucntion
    mMediaStreamContext->SetLastPlaylistDownloadTime(0);
    //Assert:check mLastPlaylistDownloadTimeMs value
    EXPECT_EQ(mMediaStreamContext->GetLastPlaylistDownloadTime(),mMediaStreamContext->context->mLastPlaylistDownloadTimeMs);

}

TEST_F(MediaStreamContextTest, PlaylistDownloadTimeTest_3)
{
    //Act:call SetLastPlaylistDownloadTime fucntion
    mMediaStreamContext->SetLastPlaylistDownloadTime(-12345678);
    //Assert:check mLastPlaylistDownloadTimeMs value
    EXPECT_EQ(mMediaStreamContext->GetLastPlaylistDownloadTime(),mMediaStreamContext->context->mLastPlaylistDownloadTimeMs);
}

TEST_F(MediaStreamContextTest, PlaylistDownloadTimeTest_4)
{
    //Act:call SetLastPlaylistDownloadTime fucntion
    mMediaStreamContext->SetLastPlaylistDownloadTime(-12345678987654322);
    //Assert:check mLastPlaylistDownloadTimeMs value
    EXPECT_EQ(mMediaStreamContext->GetLastPlaylistDownloadTime(),mMediaStreamContext->context->mLastPlaylistDownloadTimeMs);
}

TEST_F(MediaStreamContextTest, MinUpdateDurationTest)
{
    //Act:call GetMinUpdateDuration fucntion
    long mMinUpdateDurationMsResult = mMediaStreamContext->GetMinUpdateDuration();
    //Assert:check mMinUpdateDurationMsResult variable value
    EXPECT_EQ(mMinUpdateDurationMsResult, DEFAULT_INTERVAL_BETWEEN_MPD_UPDATES_MS);
}

TEST_F(MediaStreamContextTest, DefaultDurationTest)
{
    //Act:call GetDefaultDurationBetweenPlaylistUpdates fucntion
    int durationValue = mMediaStreamContext->GetDefaultDurationBetweenPlaylistUpdates();
    //Assert:check durationValue variable value
    EXPECT_EQ(durationValue,6000);
}
