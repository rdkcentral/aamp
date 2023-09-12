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
#include <gmock/gmock.h>

#include "priv_aamp.h"

#include "AampConfig.h"
#include "AampStreamSinkManager.h"
#include "aampgstplayer.h"
#include "MockAampGstPlayer.h"

using ::testing::_;

AampConfig *gpGlobalConfig=NULL;
AampLogManager *mLogObj=NULL;

class AampStreamSinkManagerTests : public ::testing::Test
{
protected:
    PrivateInstanceAAMP *mPrivateInstanceAAMP1{};
    PrivateInstanceAAMP *mPrivateInstanceAAMP2{};
    id3_callback_t mId3HandlerCallback1;
    id3_callback_t mId3HandlerCallback2;
    AampLogManager *mLogObj1{};
    AampLogManager *mLogObj2{};

    void SetUp() override
    {
        if(gpGlobalConfig == nullptr)
        {
            gpGlobalConfig =  new AampConfig();
        }

        mLogObj1 = new AampLogManager();
        mLogObj1->setPlayerId(1);

        mLogObj2 = new AampLogManager();
        mLogObj2->setPlayerId(2);

        mPrivateInstanceAAMP1 = new PrivateInstanceAAMP(gpGlobalConfig);
        mPrivateInstanceAAMP1->mPlayerId = 1;

        mPrivateInstanceAAMP2 = new PrivateInstanceAAMP(gpGlobalConfig);
        mPrivateInstanceAAMP2->mPlayerId = 2;

        g_mockAampGstPlayer = new MockAAMPGstPlayer(mLogObj1, mPrivateInstanceAAMP1);

        mId3HandlerCallback1 = std::bind(&PrivateInstanceAAMP::ID3MetadataHandler, mPrivateInstanceAAMP1, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4);
        mId3HandlerCallback2 = std::bind(&PrivateInstanceAAMP::ID3MetadataHandler, mPrivateInstanceAAMP2, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4);
    }

    void TearDown() override
    {
        delete mPrivateInstanceAAMP1;
        mPrivateInstanceAAMP1 = nullptr;

        delete gpGlobalConfig;
        gpGlobalConfig = nullptr;

        delete mPrivateInstanceAAMP2;
        mPrivateInstanceAAMP2 = nullptr;

        delete g_mockAampGstPlayer;
        g_mockAampGstPlayer = nullptr;

        delete mLogObj1;
        mLogObj1 = nullptr;

        delete mLogObj2;
        mLogObj2 = nullptr;

        AampStreamSinkManager::GetInstance().Clear();
    }

public:

};
/*
    @brief: - Checks that the single pipeline mode of operation is active
    Test Procedure
    Initialise AampStreamSinkManager into single pipeline mode.
    Create 2 Sinks ( i.e. call CreateSinkStream twice).
    The first player is active, get the stream sink for it.
    Make Second Player active, get the stream sink for it.
    Compare it with the stream sink for first player, if found to be same than it is single pipeline mode.
    This test scenario also verfies that the Activate player is working.
*/
TEST_F(AampStreamSinkManagerTests, CheckSetSinglePipelineMode)
{
    AampStreamSinkManager::GetInstance().SetSinglePipelineMode();
    AampStreamSinkManager::GetInstance().CreateStreamSink(mLogObj1, mPrivateInstanceAAMP1, mId3HandlerCallback1);
    StreamSink *sink1 = AampStreamSinkManager::GetInstance().GetStreamSink(mPrivateInstanceAAMP1);

    AampStreamSinkManager::GetInstance().CreateStreamSink(mLogObj2, mPrivateInstanceAAMP2, mId3HandlerCallback2);
    AampStreamSinkManager::GetInstance().ActivatePlayer(mPrivateInstanceAAMP2);
    StreamSink *sink2 = AampStreamSinkManager::GetInstance().GetStreamSink(mPrivateInstanceAAMP2);

    EXPECT_EQ(sink1, sink2);
}

/*
    @brief: - Tests the scenario when single PrivateInstanceAAMP is deleted, the sink is deleted.
    Test Procedure: -
    In Single pipeline mode, create a stream sink.
    Delete the PrivateInstanceAAMP, this should delete the player since was only one PrivateInstanceAAMP.
*/
TEST_F(AampStreamSinkManagerTests, DeleteStreamSinkTest1)
{
    AampStreamSinkManager::GetInstance().SetSinglePipelineMode();
    AampStreamSinkManager::GetInstance().CreateStreamSink(mLogObj1, mPrivateInstanceAAMP1, mId3HandlerCallback1);
    AampStreamSinkManager::GetInstance().DeleteStreamSink(mPrivateInstanceAAMP1);

    StreamSink *sink1 = AampStreamSinkManager::GetInstance().GetStreamSink(mPrivateInstanceAAMP1);
    EXPECT_EQ(sink1, nullptr);
}
/*
    @brief: - Tests the scenario when two PrivateInstanceAAMP exists and one of them is active
              then deleting the inactive PrivateInstanceAAMP has no effect on active instance
    Test Procedure: -
    In Single pipeline mode.
    Create two stream sink - first player is active.
    Delete the inactiveplayer.
    Calling GetStreamSink return AAMPGstPlayer type of Object.
*/
TEST_F(AampStreamSinkManagerTests, DeleteStreamSinkTest2)
{

    AAMPGstPlayer gstPlayerobj {mLogObj1, mPrivateInstanceAAMP1, mId3HandlerCallback1};

    AampStreamSinkManager::GetInstance().SetSinglePipelineMode();
    AampStreamSinkManager::GetInstance().CreateStreamSink(mLogObj1, mPrivateInstanceAAMP1, mId3HandlerCallback1);
    AampStreamSinkManager::GetInstance().CreateStreamSink(mLogObj2, mPrivateInstanceAAMP2, mId3HandlerCallback2);
    AampStreamSinkManager::GetInstance().DeleteStreamSink(mPrivateInstanceAAMP2);

    StreamSink *sink1 = AampStreamSinkManager::GetInstance().GetStreamSink(mPrivateInstanceAAMP1);
    EXPECT_EQ(typeid(*sink1), typeid(gstPlayerobj));
}

/*
    @brief: - When an active mPrivateInstanceAAMP is deactivated, it gets assigned to AampStreamSinkInactive
    Test Procedure: -
    Enable single pipeline mode and Call Create stream sink
    Check the typeid of returned from GetStreamSink Matches AAMPGstPlayer
    Now deactivate player
    Check the typeid of returned from GetStreamSink Matches AampStreamSinkInactive
*/
TEST_F(AampStreamSinkManagerTests, Deactivateplayer)
{
    AampStreamSinkInactive inactiveSink {mLogObj1, mId3HandlerCallback1};
    AAMPGstPlayer gstPlayerobj {mLogObj1, mPrivateInstanceAAMP1, mId3HandlerCallback1};

    AampStreamSinkManager::GetInstance().SetSinglePipelineMode();
    AampStreamSinkManager::GetInstance().CreateStreamSink(mLogObj1, mPrivateInstanceAAMP1, mId3HandlerCallback1);
    StreamSink *sink1 = AampStreamSinkManager::GetInstance().GetStreamSink(mPrivateInstanceAAMP1);
    EXPECT_EQ(typeid(gstPlayerobj), typeid(*sink1));

    AampStreamSinkManager::GetInstance().DeactivatePlayer(mPrivateInstanceAAMP1, false);
    StreamSink *sink2 = AampStreamSinkManager::GetInstance().GetStreamSink(mPrivateInstanceAAMP1);
    EXPECT_EQ(typeid(*sink2), typeid(inactiveSink));
}

/*  @brief : - Establishes that the multipiepline mode of operation is active.
    Test Procedure: -
    Create 2 Players ( i.e. call CreateSinkStream twice)
    The first player is active, get the stream sink for it and store it
    Make Second Player active, get the stream sink for it.
    Compare it with the stream sink for first player, they should be different
*/
TEST_F(AampStreamSinkManagerTests, CheckMultiPipelineMode)
{
    AampStreamSinkManager::GetInstance().CreateStreamSink(mLogObj1, mPrivateInstanceAAMP1, mId3HandlerCallback1);
    AampStreamSinkManager::GetInstance().CreateStreamSink(mLogObj2, mPrivateInstanceAAMP2, mId3HandlerCallback2);
    StreamSink *sink1 = AampStreamSinkManager::GetInstance().GetStreamSink(mPrivateInstanceAAMP1);

    AampStreamSinkManager::GetInstance().ActivatePlayer(mPrivateInstanceAAMP2);
    StreamSink *sink2 = AampStreamSinkManager::GetInstance().GetStreamSink(mPrivateInstanceAAMP2);

    EXPECT_NE(sink1, sink2);
}
/* Test Procedure: -
    @brief Without Creating any sink; call GetStreamSink; nullptr should be returned
*/
TEST_F(AampStreamSinkManagerTests, MultiPipelineMode_CheckGetStreamSink1)
{
    StreamSink *sink1 = AampStreamSinkManager::GetInstance().GetStreamSink(mPrivateInstanceAAMP1);
    EXPECT_EQ(sink1, nullptr);
}

/*  Test Procedure
    @brief Verifies deletion of a sink
    Create StreamSink, verify that sink if of AAMPGstPlayer.
    Delete it,call to GetStreamSink Should return null.
*/
TEST_F(AampStreamSinkManagerTests, MultiPipelineMode_CheckGetStreamSink2)
{

    AAMPGstPlayer gstPlayerobj {mLogObj1, mPrivateInstanceAAMP1, mId3HandlerCallback1};
    AampStreamSinkManager::GetInstance().CreateStreamSink(mLogObj1, mPrivateInstanceAAMP1, mId3HandlerCallback1);

    StreamSink *sink1 = AampStreamSinkManager::GetInstance().GetStreamSink(mPrivateInstanceAAMP1);
    EXPECT_EQ(typeid(gstPlayerobj), typeid(*sink1));

    AampStreamSinkManager::GetInstance().DeleteStreamSink(mPrivateInstanceAAMP1);
    sink1 = AampStreamSinkManager::GetInstance().GetStreamSink(mPrivateInstanceAAMP1);
    EXPECT_EQ(sink1, nullptr);
}

/*
    @brief: - verifies that ChangeAamp receives expected parameters
    Test Procedure
    Initialise AampStreamSinkManager into single pipeline mode.
    Create 1 sink, it will be the active sink. Verify test requirments
*/
TEST_F(AampStreamSinkManagerTests, ChangeAampTests)
{
    AampStreamSinkManager::GetInstance().SetSinglePipelineMode();

    EXPECT_CALL(*g_mockAampGstPlayer, ChangeAamp(mPrivateInstanceAAMP1, mLogObj1, _)).Times(0);
    EXPECT_CALL(*g_mockAampGstPlayer, ChangeAamp(mPrivateInstanceAAMP2, mLogObj2, _)).Times(0);

    AampStreamSinkManager::GetInstance().CreateStreamSink(mLogObj1, mPrivateInstanceAAMP1, mId3HandlerCallback1);
    AampStreamSinkManager::GetInstance().CreateStreamSink(mLogObj2, mPrivateInstanceAAMP2, mId3HandlerCallback2);

    EXPECT_CALL(*g_mockAampGstPlayer, ChangeAamp(mPrivateInstanceAAMP1, mLogObj1, _)).Times(0);
    AampStreamSinkManager::GetInstance().ActivatePlayer(mPrivateInstanceAAMP1);

    EXPECT_CALL(*g_mockAampGstPlayer, ChangeAamp(mPrivateInstanceAAMP2, mLogObj2, _)).Times(1);
    AampStreamSinkManager::GetInstance().ActivatePlayer(mPrivateInstanceAAMP2);
}

/*  @brief: - Tests the API SetEncryptedAamp, SetEncryptedHeaders, GetEncryptedHeaders and ReinjectEncryptedHeaders.
    Test procedure
    Set the singlepipeline mode and create streamsink.
    Set the encrypted headers to a test data via SetEncryptedHeaders.
    Read back the encrypted data via GetEncryptedHeaders.
    The read test data should match the set test data.
    Further read using GetEncryptedHeaders should mismatch set data
    Call to ReinjectEncryptedHeaders, followed by GetEncryptedHeaders, returns headers one more time.
*/
TEST_F(AampStreamSinkManagerTests, CheckEncyptedHeaders)
{
    std::map<int, std::string> set_headers;
    std::map<int, std::string> get_headers;

    set_headers.insert({1, "Test String"});

    AampStreamSinkManager::GetInstance().SetSinglePipelineMode();

    EXPECT_CALL(*g_mockAampGstPlayer, ChangeAamp(mPrivateInstanceAAMP1, mLogObj1, _)).Times(0);
    AampStreamSinkManager::GetInstance().CreateStreamSink(mLogObj1, mPrivateInstanceAAMP1, mId3HandlerCallback1);

    EXPECT_CALL(*g_mockAampGstPlayer, SetEncryptedAamp(mPrivateInstanceAAMP1));
    AampStreamSinkManager::GetInstance().SetEncryptedHeaders(mPrivateInstanceAAMP1, set_headers);
    AampStreamSinkManager::GetInstance().GetEncryptedHeaders(get_headers);
    EXPECT_EQ(set_headers, get_headers);

    //Once the GetEncryptedHeaders is called, subsequent call should return nothing
    AampStreamSinkManager::GetInstance().GetEncryptedHeaders(get_headers);
    EXPECT_NE(set_headers, get_headers);

    AampStreamSinkManager::GetInstance().ReinjectEncryptedHeaders();
    AampStreamSinkManager::GetInstance().GetEncryptedHeaders(get_headers);
    EXPECT_EQ(set_headers, get_headers);
}

/*  @brief: - Tests the API GetActiveStreamSink
    Test procedure
    Set the singlepipeline mode and create streamsink X 2. First instance is active
    Verify that type id of sink matches AAMPGstPlayer when GetActiveStreamSink called on active instance
    call to GetActiveStreamSink with instance of inactive PrivateInstance should return the sink of active player.
*/
TEST_F(AampStreamSinkManagerTests, CheckGetActiveStreamSink)
{
    AAMPGstPlayer gstPlayerobj {mLogObj1, mPrivateInstanceAAMP1, mId3HandlerCallback1};

    AampStreamSinkManager::GetInstance().SetSinglePipelineMode();
    AampStreamSinkManager::GetInstance().CreateStreamSink(mLogObj1, mPrivateInstanceAAMP1, mId3HandlerCallback1);
    AampStreamSinkManager::GetInstance().CreateStreamSink(mLogObj2, mPrivateInstanceAAMP2, mId3HandlerCallback2);

    StreamSink *sink1 = AampStreamSinkManager::GetInstance().GetActiveStreamSink(mPrivateInstanceAAMP1);
    ASSERT_NE(nullptr, sink1);
    EXPECT_EQ(typeid(gstPlayerobj), typeid(*sink1));

    StreamSink *sink2 = AampStreamSinkManager::GetInstance().GetActiveStreamSink(mPrivateInstanceAAMP2);
    ASSERT_NE(nullptr, sink2);
    EXPECT_EQ(sink1, sink2);
}
