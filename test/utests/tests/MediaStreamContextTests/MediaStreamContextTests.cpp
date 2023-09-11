
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
#include "fragmentcollector_mpd.h"
#include "StreamAbstractionAAMP.h"
using namespace testing;
AampConfig *gpGlobalConfig{nullptr};
AampLogManager *mLogObj = NULL;
class MediaStreamContextTest : public testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {
    delete mLogObj;
    mLogObj = nullptr;
    delete aamp;
    aamp = nullptr;
    delete ctx;
    ctx = nullptr;
    delete msc;
    msc = nullptr;
    }
public:
    StreamAbstractionAAMP_MPD *ctx=new StreamAbstractionAAMP_MPD(NULL,NULL,123.45,12.34);
    PrivateInstanceAAMP *aamp = new PrivateInstanceAAMP(NULL);
    MediaStreamContext *msc = new MediaStreamContext(mLogObj,eTRACK_VIDEO,ctx,aamp,"SAMPLETEXT");
};

TEST_F(MediaStreamContextTest, GetContextTest)
{
    //Act:call GetContext function
    msc->GetContext();
}

TEST_F(MediaStreamContextTest, CacheFragmentChunkTest)
{
    //Act:call CacheFragmentChunk fucntion
    bool val = msc->CacheFragmentChunk(eMEDIATYPE_VIDEO,NULL, 12345678,"remoteUrl",123456789);
    EXPECT_FALSE(val);
}

TEST_F(MediaStreamContextTest,ProcessPlaylistTest)
{
    AampGrowableBuffer newPlaylist("download-PlaylistManifest");
    //Act:call ProcessPlaylist function
    msc->ProcessPlaylist(newPlaylist,1);
}

TEST_F(MediaStreamContextTest,SignalTrickModeDiscontinuityTest)
{
    //Act SignalTrickModeDiscontinuity function
    msc->SignalTrickModeDiscontinuity();
}

TEST_F(MediaStreamContextTest, EffectivePlaylistUrlTest)
{
    //Arrange:assign downloadedDuration value
    msc->downloadedDuration=10.88;
    //Act:call GetBufferedDuration function
    double  val1 = msc->GetBufferedDuration();
    //Assert:check val1 value
    EXPECT_NE(val1,00.00);

    //Arrange:assign downloadedDuration value
    msc->downloadedDuration=00.00;
    //Act:call GetBufferedDuration function
    val1 = msc->GetBufferedDuration();
    //Assert:check val1 value
    EXPECT_EQ(val1,00.00);

    //Arrange:assign downloadedDuration value
    msc->downloadedDuration=aamp->GetPositionMs() / 1000.00;
    //Act:call GetBufferedDuration function
    val1 = msc->GetBufferedDuration();
    //Assert:check val1 value
    EXPECT_EQ(val1,00.00);
}

TEST_F(MediaStreamContextTest, IsAtEndOfTrackTest)
{
    //Act:call IsAtEndOfTrack function
    bool b1=msc->IsAtEndOfTrack();
    //Assert:check b1 value
    EXPECT_FALSE(b1);
}

TEST_F(MediaStreamContextTest, PlaylistUrlTest)
{
    //Act:call GetPlaylistUrl function
    string str = msc->GetPlaylistUrl();
    //Assert:check str value
    EXPECT_EQ(str,"");
    //Act:call SetEffectivePlaylistUrl fucntion
    msc->SetEffectivePlaylistUrl("https://sampleurl");
    //Assert:check for mEffectiveUrl value
    EXPECT_EQ(msc->GetEffectivePlaylistUrl(),msc->mEffectiveUrl);
    //Act:call SetEffectivePlaylistUrl fucntion
    msc->SetEffectivePlaylistUrl("https://sampleurlQWERTYTIPI[asfdfghjkklzxvxcnbcbmv");
    //Assert:check for mEffectiveUrl value
    EXPECT_EQ(msc->GetEffectivePlaylistUrl(),msc->mEffectiveUrl);
    //Act:call SetEffectivePlaylistUrl fucntion
    msc->SetEffectivePlaylistUrl("https://sampleurl@@@@@@@@@@@@@3#################4$%^^&&&&**QWERTYTIPI[asfdfghjkklzxvxcnbcbmv");
    //Assert:check for mEffectiveUrl value
    EXPECT_EQ(msc->GetEffectivePlaylistUrl(),msc->mEffectiveUrl);
    //Act:call SetEffectivePlaylistUrl fucntion
    msc->SetEffectivePlaylistUrl("");
    //Assert:check for mEffectiveUrl value
    EXPECT_EQ(msc->GetEffectivePlaylistUrl(),msc->mEffectiveUrl);
}

TEST_F(MediaStreamContextTest, PlaylistDownloadTimeTest)
{
    //Act:call SetLastPlaylistDownloadTime fucntion
    msc->SetLastPlaylistDownloadTime(29955777888);
    //Assert:check mLastPlaylistDownloadTimeMs value
    EXPECT_EQ(msc->GetLastPlaylistDownloadTime(),msc->context->mLastPlaylistDownloadTimeMs);
    //Act:call SetLastPlaylistDownloadTime fucntion
    msc->SetLastPlaylistDownloadTime(299557778882352);
    //Assert:check mLastPlaylistDownloadTimeMs value
    EXPECT_EQ(msc->GetLastPlaylistDownloadTime(),msc->context->mLastPlaylistDownloadTimeMs);
    //Act:call SetLastPlaylistDownloadTime fucntion
    msc->SetLastPlaylistDownloadTime(0);
    //Assert:check mLastPlaylistDownloadTimeMs value
    EXPECT_EQ(msc->GetLastPlaylistDownloadTime(),msc->context->mLastPlaylistDownloadTimeMs);
    //Act:call SetLastPlaylistDownloadTime fucntion
    msc->SetLastPlaylistDownloadTime(-12345678);
    //Assert:check mLastPlaylistDownloadTimeMs value
    EXPECT_EQ(msc->GetLastPlaylistDownloadTime(),msc->context->mLastPlaylistDownloadTimeMs);
    //Act:call SetLastPlaylistDownloadTime fucntion
    msc->SetLastPlaylistDownloadTime(-12345678987654322);
    //Assert:check mLastPlaylistDownloadTimeMs value
    EXPECT_EQ(msc->GetLastPlaylistDownloadTime(),msc->context->mLastPlaylistDownloadTimeMs);
}

TEST_F(MediaStreamContextTest, MinUpdateDurationTest)
{
    //Act:call GetMinUpdateDuration fucntion
    long var = msc->GetMinUpdateDuration();
    //Assert:check var variable value
    // EXPECT_NE(var,0);
}

TEST_F(MediaStreamContextTest, DefaultDurationTest)
{
    //Act:call GetDefaultDurationBetweenPlaylistUpdates fucntion
    int dur = msc->GetDefaultDurationBetweenPlaylistUpdates();
    //Assert:check dur variable value
    EXPECT_EQ(dur,6000);
}
