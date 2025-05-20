
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
#include "AampBufferControl.h"
#include "AampConfig.h"

using namespace testing;

AampConfig *gpGlobalConfig{nullptr};

class BufferControlExternalDataTest : public testing::Test
{
    public:
    AAMPGstPlayer* player = nullptr;
    AampMediaType mediaType = eMEDIATYPE_DEFAULT;
    AampBufferControl::BufferControlMaster context;

    protected:
    void SetUp() override
    {
        mBufferControl = new AampBufferControl::BufferControlExternalData(player,mediaType);
        mBufferControlMaster = new AampBufferControl::BufferControlMaster();
        mBufferControlTimeBased = new AampBufferControl::BufferControlTimeBased(context);
        mBufferControlByteBased = new AampBufferControl::BufferControlTimeBased(context);
    }
    void TearDown() override
    {
        delete mBufferControl;
        mBufferControl=nullptr;

        delete mBufferControlMaster;
        mBufferControlMaster=nullptr;

        delete mBufferControlTimeBased;
        mBufferControlTimeBased=nullptr;

        delete mBufferControlByteBased;
        mBufferControlByteBased=nullptr;
    }

    public:
    AampBufferControl::BufferControlExternalData *mBufferControl;
    AampBufferControl::BufferControlMaster *mBufferControlMaster;
    AampBufferControl::BufferControlStrategyBase *mBufferControlTimeBased;
    AampBufferControl::BufferControlByteBased *mBufferControlByteBased;
};

TEST_F(BufferControlExternalDataTest, mBufferControlactionDownloadsTest1)
{
    // Arrange: Creating the variables for passing to arguments
    AAMPGstPlayer* player = nullptr;
    AampMediaType mediaType = eMEDIATYPE_DEFAULT;
    bool downloadsEnabled = true;

    //Act: Call the function for test
    mBufferControl->actionDownloads(player,mediaType,downloadsEnabled);

    //Assert: Expecting the values are equal or not
    ASSERT_TRUE(downloadsEnabled);
    EXPECT_EQ(mediaType,eMEDIATYPE_DEFAULT);
}

TEST_F(BufferControlExternalDataTest, mBufferControlactionDownloadsTest2)
{
    // Arrange: Creating the variables for passing to arguments
    AAMPGstPlayer* player = nullptr;
    bool downloadsEnabled = true;
    AampMediaType mediaType[21] = {
    eMEDIATYPE_DEFAULT,
    eMEDIATYPE_VIDEO,
	eMEDIATYPE_AUDIO,
	eMEDIATYPE_SUBTITLE,
    eMEDIATYPE_AUX_AUDIO,
	eMEDIATYPE_MANIFEST,
	eMEDIATYPE_LICENCE,
	eMEDIATYPE_IFRAME,
	eMEDIATYPE_INIT_VIDEO,
	eMEDIATYPE_INIT_AUDIO,
	eMEDIATYPE_INIT_SUBTITLE,
	eMEDIATYPE_INIT_AUX_AUDIO,
	eMEDIATYPE_PLAYLIST_VIDEO,
	eMEDIATYPE_PLAYLIST_AUDIO,
	eMEDIATYPE_PLAYLIST_SUBTITLE,
	eMEDIATYPE_PLAYLIST_AUX_AUDIO,
	eMEDIATYPE_PLAYLIST_IFRAME,
	eMEDIATYPE_INIT_IFRAME,
	eMEDIATYPE_DSM_CC,
	eMEDIATYPE_IMAGE,
	eMEDIATYPE_DEFAULT
    };

    for(int i=0; i<21; i++){
    //Act: Call the function for test
    mBufferControl->actionDownloads(player,mediaType[i],downloadsEnabled);

    //Assert: Expecting downloadsEnabled value is true of false 
    ASSERT_TRUE(downloadsEnabled);
    }
}

TEST_F(BufferControlExternalDataTest, mBufferControlactionDownloadsTest3)
{
    // Arrange: Creating the variables for passing to arguments
    AAMPGstPlayer* player = nullptr;
    AampMediaType mediaType = eMEDIATYPE_DEFAULT;
    bool downloadsEnabled = false;

    //Act: Call the function for test
    mBufferControl->actionDownloads(player,mediaType,downloadsEnabled);

    //Assert: Expecting the values are equal or not
    ASSERT_FALSE(downloadsEnabled);
    EXPECT_EQ(mediaType,eMEDIATYPE_DEFAULT);
}

TEST_F(BufferControlExternalDataTest, BufferControlMasterteardownStartTest)
{
    //Act: Calling the teardownStart function for test
    mBufferControlMaster->teardownStart();
}

TEST_F(BufferControlExternalDataTest, BufferControlMasterteardownEndTest)
{
    //Act: Calling the teardownEnd function for test
    mBufferControlMaster->teardownEnd();
}

TEST_F(BufferControlExternalDataTest, BufferControlMasterflushTest)
{
    //Act: Calling the flush function for test
    mBufferControlMaster->flush();
}

TEST_F(BufferControlExternalDataTest, BufferControlMasterneedDataTest1)
{
    // Arrange: Creating the variables for passing to arguments
    AAMPGstPlayer* player = nullptr;
    AampMediaType mediaType = eMEDIATYPE_DEFAULT;

    //Act: Call the function for test
    mBufferControlMaster->needData(player,mediaType);

    //Assert: Expecting the mediatype is equal or not
    ASSERT_EQ(mBufferControlMaster->getMediaType(), mediaType);
}

TEST_F(BufferControlExternalDataTest, BufferControlMasterneedDataTest2)
{
    // Arrange: Creating the variables for passing to arguments
    AAMPGstPlayer* player = nullptr;
    AampMediaType mediaType[21] = {
    eMEDIATYPE_DEFAULT,
    eMEDIATYPE_VIDEO,
	eMEDIATYPE_AUDIO,
	eMEDIATYPE_SUBTITLE,
    eMEDIATYPE_AUX_AUDIO,
	eMEDIATYPE_MANIFEST,
	eMEDIATYPE_LICENCE,
	eMEDIATYPE_IFRAME,
	eMEDIATYPE_INIT_VIDEO,
	eMEDIATYPE_INIT_AUDIO,
	eMEDIATYPE_INIT_SUBTITLE,
	eMEDIATYPE_INIT_AUX_AUDIO,
	eMEDIATYPE_PLAYLIST_VIDEO,
	eMEDIATYPE_PLAYLIST_AUDIO,
	eMEDIATYPE_PLAYLIST_SUBTITLE,
	eMEDIATYPE_PLAYLIST_AUX_AUDIO,
	eMEDIATYPE_PLAYLIST_IFRAME,
	eMEDIATYPE_INIT_IFRAME,
	eMEDIATYPE_DSM_CC,
	eMEDIATYPE_IMAGE,
	eMEDIATYPE_DEFAULT
    };

    for(int i=0; i<21; i++){
    //Act: Call the function for test
    mBufferControlMaster->needData(player,mediaType[i]);

    //Assert: Expecting the mediatype values are equal or not
    ASSERT_EQ(mBufferControlMaster->getMediaType(), mediaType[i]);
    }
}

TEST_F(BufferControlExternalDataTest, BufferControlMasternotifyFragmentInjectTest1)
{
    // Arrange: Creating the variables for passing to arguments
    AAMPGstPlayer* player = nullptr;
    AampMediaType mediaType = eMEDIATYPE_DEFAULT;
    double fpts = 22.2;
    double fdts = 33.3;
    double duration = 44.4;
    bool firstBuffer = true;

    //Act: Call the function for test
    mBufferControlMaster->notifyFragmentInject(player,mediaType,fpts,fdts,duration,firstBuffer);

    //Assert: Expecting the values are equal or not
    EXPECT_EQ(mediaType,eMEDIATYPE_DEFAULT);
    EXPECT_EQ(fpts,22.2);
    EXPECT_EQ(fdts,33.3);
    EXPECT_EQ(duration,44.4);
    EXPECT_TRUE(firstBuffer);
}

TEST_F(BufferControlExternalDataTest, BufferControlMasternotifyFragmentInjectTest2)
{
    // Arrange: Creating the variables for passing to arguments
    AAMPGstPlayer* player = nullptr;
    AampMediaType mediaType = eMEDIATYPE_DEFAULT;
    double fpts = DBL_MAX;
    double fdts = DBL_MAX;
    double duration = DBL_MAX;
    bool firstBuffer = false;

    //Act: Call the function for test
    mBufferControlMaster->notifyFragmentInject(player,mediaType,fpts,fdts,duration,firstBuffer);

    //Assert: Expecting the values are equal or not
    EXPECT_EQ(mediaType,eMEDIATYPE_DEFAULT);
    EXPECT_FALSE(firstBuffer);
}

TEST_F(BufferControlExternalDataTest, BufferControlMasternotifyFragmentInjectTest3)
{
    // Arrange: Creating the variables for passing to arguments
    AAMPGstPlayer* player = nullptr;
    AampMediaType mediaType = eMEDIATYPE_DEFAULT;
    double fpts = DBL_MIN;
    double fdts = DBL_MIN;
    double duration = DBL_MIN;
    bool firstBuffer = false;

    //Act: Call the function for test
    mBufferControlMaster->notifyFragmentInject(player,mediaType,fpts,fdts,duration,firstBuffer);

    //Assert: Expecting the values are equal or not
    EXPECT_EQ(mediaType,eMEDIATYPE_DEFAULT);
    EXPECT_FALSE(firstBuffer);
}


TEST_F(BufferControlExternalDataTest, BufferControlMasternotifyFragmentInjectTest4)
{
    // Arrange: Creating the variables for passing to arguments
    AAMPGstPlayer* player = nullptr;
    AampMediaType mediaType = eMEDIATYPE_DEFAULT;
    double fpts = 0.0;
    double fdts = 0.0;
    double duration = 0.0;
    bool firstBuffer = true;

    //Act: Call the function for test
    mBufferControlMaster->notifyFragmentInject(player,mediaType,fpts,fdts,duration,firstBuffer);

    //Assert: Expecting the values are equal or not
    EXPECT_EQ(mediaType,eMEDIATYPE_DEFAULT);
    EXPECT_EQ(fpts,0.0);
    EXPECT_EQ(fdts,0.0);
    EXPECT_EQ(duration,0.0);
    EXPECT_TRUE(firstBuffer);
}

TEST_F(BufferControlExternalDataTest, BufferControlMasternotifyFragmentInjectTest5)
{
    // Arrange: Creating the variables for passing to arguments
    AAMPGstPlayer* player = nullptr;
    double fpts = 1.2;
    double fdts = 1.3;
    double duration = 1.4;
    bool firstBuffer = false;
    AampMediaType mediaType[21] = {
    eMEDIATYPE_DEFAULT,
    eMEDIATYPE_VIDEO,
	eMEDIATYPE_AUDIO,
	eMEDIATYPE_SUBTITLE,
    eMEDIATYPE_AUX_AUDIO,
	eMEDIATYPE_MANIFEST,
	eMEDIATYPE_LICENCE,
	eMEDIATYPE_IFRAME,
	eMEDIATYPE_INIT_VIDEO,
	eMEDIATYPE_INIT_AUDIO,
	eMEDIATYPE_INIT_SUBTITLE,
	eMEDIATYPE_INIT_AUX_AUDIO,
	eMEDIATYPE_PLAYLIST_VIDEO,
	eMEDIATYPE_PLAYLIST_AUDIO,
	eMEDIATYPE_PLAYLIST_SUBTITLE,
	eMEDIATYPE_PLAYLIST_AUX_AUDIO,
	eMEDIATYPE_PLAYLIST_IFRAME,
	eMEDIATYPE_INIT_IFRAME,
	eMEDIATYPE_DSM_CC,
	eMEDIATYPE_IMAGE,
	eMEDIATYPE_DEFAULT
    };

    for(int i=0; i<21; i++){
    //Act: Call the function for test
    mBufferControlMaster->notifyFragmentInject(player,mediaType[i],fpts,fdts,duration,firstBuffer);

    //Assert: Expecting the values are equal or not
    ASSERT_EQ(mBufferControlMaster->getMediaType(), mediaType[i]);
    EXPECT_FALSE(firstBuffer);
    }

    //Assert: Expecting the values are equal or not
    EXPECT_EQ(fpts,1.2);
    EXPECT_EQ(fdts,1.3);
    EXPECT_EQ(duration,1.4);
    EXPECT_FALSE(firstBuffer);
}

TEST_F(BufferControlExternalDataTest, BufferControlMasterenoughDataTest1)
{
    // Arrange: Creating the variables for passing to arguments
    AAMPGstPlayer* player = nullptr;
    AampMediaType mediaType = eMEDIATYPE_DEFAULT;

    //Act: Call the function for test
    mBufferControlMaster->enoughData(player,mediaType);

    //Assert: Expecting the values are equal or not
    ASSERT_EQ(mBufferControlMaster->getMediaType(), mediaType);
}

TEST_F(BufferControlExternalDataTest, BufferControlMasterenoughDataTest2)
{
    // Arrange: Creating the variables for passing to arguments
    AAMPGstPlayer* player = nullptr;
    AampMediaType mediaType[21] = {
    eMEDIATYPE_DEFAULT,
    eMEDIATYPE_VIDEO,
	eMEDIATYPE_AUDIO,
	eMEDIATYPE_SUBTITLE,
    eMEDIATYPE_AUX_AUDIO,
	eMEDIATYPE_MANIFEST,
	eMEDIATYPE_LICENCE,
	eMEDIATYPE_IFRAME,
	eMEDIATYPE_INIT_VIDEO,
	eMEDIATYPE_INIT_AUDIO,
	eMEDIATYPE_INIT_SUBTITLE,
	eMEDIATYPE_INIT_AUX_AUDIO,
	eMEDIATYPE_PLAYLIST_VIDEO,
	eMEDIATYPE_PLAYLIST_AUDIO,
	eMEDIATYPE_PLAYLIST_SUBTITLE,
	eMEDIATYPE_PLAYLIST_AUX_AUDIO,
	eMEDIATYPE_PLAYLIST_IFRAME,
	eMEDIATYPE_INIT_IFRAME,
	eMEDIATYPE_DSM_CC,
	eMEDIATYPE_IMAGE,
	eMEDIATYPE_DEFAULT
    };

    for(int i=0; i<21; i++){
    //Act: Call the function for test
    mBufferControlMaster->enoughData(player,mediaType[i]);

    //Assert: Expecting the values are equal or not
    ASSERT_EQ(mBufferControlMaster->getMediaType(), mediaType[i]);
    }
}


TEST_F(BufferControlExternalDataTest, BufferControlMasterunderflowTest1)
{
    // Arrange: Creating the variables for passing to arguments
    AAMPGstPlayer* player = nullptr;
    AampMediaType mediaType=eMEDIATYPE_DEFAULT;

    //Act: Call the function for test
    mBufferControlMaster->underflow(player,mediaType);
}

TEST_F(BufferControlExternalDataTest, BufferControlMasterunderflowTest2)
{
    // Arrange: Creating the variables for passing to arguments
    AAMPGstPlayer* player = nullptr;
    AampMediaType mediaType[21] = {
    eMEDIATYPE_DEFAULT,
    eMEDIATYPE_VIDEO,
	eMEDIATYPE_AUDIO,
	eMEDIATYPE_SUBTITLE,
    eMEDIATYPE_AUX_AUDIO,
	eMEDIATYPE_MANIFEST,
	eMEDIATYPE_LICENCE,
	eMEDIATYPE_IFRAME,
	eMEDIATYPE_INIT_VIDEO,
	eMEDIATYPE_INIT_AUDIO,
	eMEDIATYPE_INIT_SUBTITLE,
	eMEDIATYPE_INIT_AUX_AUDIO,
	eMEDIATYPE_PLAYLIST_VIDEO,
	eMEDIATYPE_PLAYLIST_AUDIO,
	eMEDIATYPE_PLAYLIST_SUBTITLE,
	eMEDIATYPE_PLAYLIST_AUX_AUDIO,
	eMEDIATYPE_PLAYLIST_IFRAME,
	eMEDIATYPE_INIT_IFRAME,
	eMEDIATYPE_DSM_CC,
	eMEDIATYPE_IMAGE,
	eMEDIATYPE_DEFAULT
    };

    for(int i=0; i<21; i++){
    //Act: Call the function for test
    mBufferControlMaster->underflow(player,mediaType[i]);

    //Assert: Expecting the values are equal or not
    ASSERT_EQ(mBufferControlMaster->getMediaType(), mediaType[i]);
    }
}

TEST_F(BufferControlExternalDataTest, BufferControlMasterupdateTest1)
{
    // Arrange: Creating the variables for passing to arguments
    AAMPGstPlayer* player = nullptr;
    AampMediaType mediaType=eMEDIATYPE_DEFAULT;

    //Act: Call the function for test
    mBufferControlMaster->update(player,mediaType);

    //Assert: Expecting the values are equal or not
    ASSERT_EQ(mBufferControlMaster->getMediaType(), mediaType);
}

TEST_F(BufferControlExternalDataTest, BufferControlMasterupdateTest2)
{
    // Arrange: Creating the variables for passing to arguments
    AAMPGstPlayer* player = nullptr;
    AampMediaType mediaType[21] = {
    eMEDIATYPE_DEFAULT,
    eMEDIATYPE_VIDEO,
	eMEDIATYPE_AUDIO,
	eMEDIATYPE_SUBTITLE,
    eMEDIATYPE_AUX_AUDIO,
	eMEDIATYPE_MANIFEST,
	eMEDIATYPE_LICENCE,
	eMEDIATYPE_IFRAME,
	eMEDIATYPE_INIT_VIDEO,
	eMEDIATYPE_INIT_AUDIO,
	eMEDIATYPE_INIT_SUBTITLE,
	eMEDIATYPE_INIT_AUX_AUDIO,
	eMEDIATYPE_PLAYLIST_VIDEO,
	eMEDIATYPE_PLAYLIST_AUDIO,
	eMEDIATYPE_PLAYLIST_SUBTITLE,
	eMEDIATYPE_PLAYLIST_AUX_AUDIO,
	eMEDIATYPE_PLAYLIST_IFRAME,
	eMEDIATYPE_INIT_IFRAME,
	eMEDIATYPE_DSM_CC,
	eMEDIATYPE_IMAGE,
	eMEDIATYPE_DEFAULT
    };

    for(int i=0; i<21; i++){
    //Act: Call the function for test
    mBufferControlMaster->update(player,mediaType[i]);

    //Assert: Expecting the values are equal or not
    ASSERT_EQ(mBufferControlMaster->getMediaType(), mediaType[i]);
    }
}

TEST_F(BufferControlExternalDataTest, getRateTest)
{
    // Arrange: Creating the variables for passing to arguments
    float mRate;

    //Act: Call the function for test
    mRate = mBufferControl->getRate();

    //Assert: Expecting the values are equal or not
    ASSERT_NE(mRate,-1);
}

TEST_F(BufferControlExternalDataTest, getTimeBasedBufferSecondsTest)
{
    // Arrange: Creating the variables for passing to arguments
    float mTimeBasedBufferSeconds;

    //Act: Call the function for test
    mTimeBasedBufferSeconds = mBufferControl->getTimeBasedBufferSeconds();

    //Assert: Expecting the values are equal or not
    ASSERT_NE(mTimeBasedBufferSeconds,3.3);
}

TEST_F(BufferControlExternalDataTest, ShouldBeTimeBasedTest)
{
    // Arrange: Creating the variables for passing to arguments
    bool mTimeBasedBufferSecondsResult;

    //Act: Call the function for test
    mTimeBasedBufferSecondsResult = mBufferControl->ShouldBeTimeBased();
}

TEST_F(BufferControlExternalDataTest, needData_enoughData_Test)
{
    //Act: Call the function for test
    mBufferControlTimeBased->needData();
    mBufferControlTimeBased->enoughData();

    //Assert: Expecting the values are equal or not
    ASSERT_EQ(mBufferControlMaster->getMediaType(), mediaType);
}

TEST_F(BufferControlExternalDataTest, BufferControlTimeBasedupdateTest)
{
    // Arrange: Creating the variables for passing to arguments
    AampBufferControl::BufferControlExternalData externalData(player, mediaType);
    auto extraData = mBufferControl->getExtraDataCache();
    extraData.StreamReady = 1;

    //Act: Call the function for test
    mBufferControlTimeBased->update(externalData);

    //Assert: Expecting the values are equal or not
    ASSERT_EQ(mBufferControlMaster->getMediaType(), mediaType);
}

TEST_F(BufferControlExternalDataTest, BufferControlTimeBasedunderflowTest)
{
    //Act: Calling the underflow function for test
    mBufferControlTimeBased->underflow();
}

TEST_F(BufferControlExternalDataTest, BufferControlTimeBasednotifyFragmentInjectTest1)
{
    // Arrange: Creating the variables for passing to arguments
    AampBufferControl::BufferControlExternalData externalData1(player, mediaType);
    double fpts = 22.2;
    double fdts = 33.3;
    double duration = 44.4;
    bool firstBuffer = true;

    //Act: Call the function for test
    mBufferControlTimeBased->notifyFragmentInject(externalData1,fpts,fdts,duration,firstBuffer);

    //Assert: Expecting the values are equal or not
    EXPECT_EQ(fpts,22.2);
    EXPECT_EQ(fdts,33.3);
    EXPECT_EQ(duration,44.4);
    EXPECT_TRUE(firstBuffer);
}

TEST_F(BufferControlExternalDataTest, BufferControlTimeBasednotifyFragmentInjectTest2)
{
    // Arrange: Creating the variables for passing to arguments
    AampBufferControl::BufferControlExternalData externalData1(player, mediaType);
    double fpts = DBL_MAX;
    double fdts = DBL_MAX;
    double duration = DBL_MAX;
    bool firstBuffer = false;

    //Act: Call the function for test
    mBufferControlTimeBased->notifyFragmentInject(externalData1,fpts,fdts,duration,firstBuffer);

    //Assert: Expecting the values are equal or not
    EXPECT_FALSE(firstBuffer);
}

TEST_F(BufferControlExternalDataTest, BufferControlTimeBasednotifyFragmentInjectTest3)
{
    // Arrange: Creating the variables for passing to arguments
    AampBufferControl::BufferControlExternalData externalData1(player, mediaType);
    double fpts = DBL_MIN;
    double fdts = DBL_MIN;
    double duration = DBL_MIN;
    bool firstBuffer = false;

    //Act: Call the function for test
    mBufferControlTimeBased->notifyFragmentInject(externalData1,fpts,fdts,duration,firstBuffer);

    //Assert: Expecting the values are equal or not
    EXPECT_FALSE(firstBuffer);
}

TEST_F(BufferControlExternalDataTest, BufferControlTimeBasednotifyFragmentInjectTest5)
{
    // Arrange: Creating the variables for passing to arguments
    AampBufferControl::BufferControlExternalData externalData1(player, mediaType);
    double fpts = 0.0;
    double fdts = 0.0;
    double duration = 0.0;
    bool firstBuffer = true;

    //Act: Call the function for test
    mBufferControlTimeBased->notifyFragmentInject(externalData1,fpts,fdts,duration,firstBuffer);

    //Assert: Expecting the values are equal or not
    EXPECT_EQ(fpts,0.0);
    EXPECT_EQ(fdts,0.0);
    EXPECT_EQ(duration,0.0);
    EXPECT_TRUE(firstBuffer);
}
