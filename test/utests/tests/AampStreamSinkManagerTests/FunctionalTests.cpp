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

#include "main_aamp.h"
#include "AampStreamSinkManager.h"
#include "MockAampGstPlayer.h"
#include "MockStreamSink.h"
#include "MockPlayerInstanceAAMP.h"

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;

class AampStreamSinkManagerTests : public ::testing::Test
{
protected:
    PlayerInstanceAAMP *mPlayerInstanceAAMP1{};
    PlayerInstanceAAMP *mPlayerInstanceAAMP2{};
    id3_callback_t mId3HandlerCallback1;
    id3_callback_t mId3HandlerCallback2;

    void SetUp() override
    {
        mPlayerInstanceAAMP1 = new PlayerInstanceAAMP();
        mPlayerInstanceAAMP1->mPlayerId = 1;
        
        mPlayerInstanceAAMP2 = new PlayerInstanceAAMP();
        mPlayerInstanceAAMP2->mPlayerId = 2;

        g_mockPlayerInstanceAAMP = new NiceMock<MockPlayerInstanceAAMP>();

        g_mockAampGstPlayer = new MockAAMPGstPlayer( mPlayerInstanceAAMP1);

        const auto id3_callback = std::bind(&PlayerInstanceAAMP::ID3MetadataHandler, mPlayerInstanceAAMP1, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5);
        mId3HandlerCallback1 = id3_callback;
        mId3HandlerCallback2 = id3_callback;
    }

    void TearDown() override
    {
        delete g_mockPlayerInstanceAAMP;
        g_mockPlayerInstanceAAMP = nullptr;

        delete mPlayerInstanceAAMP1;
        mPlayerInstanceAAMP1 = nullptr;

        delete mPlayerInstanceAAMP2;
        mPlayerInstanceAAMP2 = nullptr;

        delete g_mockAampGstPlayer;
        g_mockAampGstPlayer = nullptr;
        AampStreamSinkManager::GetInstance().Clear();
    }

public:

};
/*
    @brief: - Checks that the single pipeline mode of operation is active
    Test Procedure
    Initialize AampStreamSinkManager into single pipeline mode.
    Create 2 Sinks ( i.e. call CreateSinkStream twice).
    The first player is active, get the stream sink for it.
    Make Second Player active, get the stream sink for it.
    Compare it with the stream sink for first player, if found to be same than it is single pipeline mode.
    This test scenario also verfies that the Activate player is working.
*/
TEST_F(AampStreamSinkManagerTests, CheckSetSinglePipelineMode)
{
    AampStreamSinkManager::GetInstance().CreateStreamSink(mPlayerInstanceAAMP1, mId3HandlerCallback1);
    AampStreamSinkManager::GetInstance().SetSinglePipelineMode(mPlayerInstanceAAMP1);
    StreamSink *sink1 = AampStreamSinkManager::GetInstance().GetStreamSink(mPlayerInstanceAAMP1);

    AampStreamSinkManager::GetInstance().CreateStreamSink(mPlayerInstanceAAMP2, mId3HandlerCallback2);
    AampStreamSinkManager::GetInstance().ActivatePlayer(mPlayerInstanceAAMP2);
    StreamSink *sink2 = AampStreamSinkManager::GetInstance().GetStreamSink(mPlayerInstanceAAMP2);

    EXPECT_EQ(sink1, sink2);
}

/* Test Procedure: -
    @brief If there are no active players then mGstPlayer should be returned.
*/
TEST_F(AampStreamSinkManagerTests, GetStoppingStreamSink_SinglePipelineMode_NoActivePlayers)
{
    AampStreamSinkManager::GetInstance().CreateStreamSink(mPlayerInstanceAAMP1, mId3HandlerCallback1);
    AampStreamSinkManager::GetInstance().SetSinglePipelineMode(mPlayerInstanceAAMP1);
    StreamSink *sink1 = AampStreamSinkManager::GetInstance().GetStreamSink(mPlayerInstanceAAMP1);
    AampStreamSinkManager::GetInstance().DeactivatePlayer(mPlayerInstanceAAMP1, true);
    StreamSink *sink2 = AampStreamSinkManager::GetInstance().GetStoppingStreamSink(mPlayerInstanceAAMP1);

    EXPECT_EQ(sink1, sink2);
}

/* Test Procedure: -
    @brief  When there is an active player GetStoppingStreamSink functions the same as GetStreamSink.
*/
TEST_F(AampStreamSinkManagerTests, GetStoppingStreamSink_SinglePipelineMode_ActivePlayers)
{
    AampStreamSinkManager::GetInstance().CreateStreamSink(mPlayerInstanceAAMP1, mId3HandlerCallback1);
    AampStreamSinkManager::GetInstance().SetSinglePipelineMode(mPlayerInstanceAAMP1);
    StreamSink *sink1 = AampStreamSinkManager::GetInstance().GetStreamSink(mPlayerInstanceAAMP1);
    StreamSink *sink2 = AampStreamSinkManager::GetInstance().GetStoppingStreamSink(mPlayerInstanceAAMP1);

    EXPECT_EQ(sink1, sink2);
}

/* Test Procedure: -
    @brief When the active map is empty, but there is an available player, use that.
*/
TEST_F(AampStreamSinkManagerTests, UpdateTuningPlayer_SinglePipelineMode_UseInactivePlayer)
{
    AampStreamSinkManager::GetInstance().CreateStreamSink(mPlayerInstanceAAMP1, mId3HandlerCallback1);
    AampStreamSinkManager::GetInstance().SetSinglePipelineMode(mPlayerInstanceAAMP1);
    AampStreamSinkManager::GetInstance().DeactivatePlayer(mPlayerInstanceAAMP1, true);
    EXPECT_CALL(*g_mockAampGstPlayer, ChangeAamp(mPlayerInstanceAAMP1, _)).Times(1);
    AampStreamSinkManager::GetInstance().UpdateTuningPlayer(mPlayerInstanceAAMP1);
}

/* Test Procedure: -
    @brief Test UpdateTuningPlayer where we wouldn't expect the player to be changed.
*/
TEST_F(AampStreamSinkManagerTests, UpdateTuningPlayer_SinglePipelineMode_NoChange)
{
	/* Undefined pipeline */
    EXPECT_CALL(*g_mockAampGstPlayer, ChangeAamp(mPlayerInstanceAAMP1, _)).Times(0);
    AampStreamSinkManager::GetInstance().UpdateTuningPlayer(mPlayerInstanceAAMP1);

    /* Single pipeline not yet created */
    AampStreamSinkManager::GetInstance().UpdateTuningPlayer(mPlayerInstanceAAMP1);

    /* Active player exists */
    AampStreamSinkManager::GetInstance().CreateStreamSink(mPlayerInstanceAAMP1, mId3HandlerCallback1);
    AampStreamSinkManager::GetInstance().SetSinglePipelineMode(mPlayerInstanceAAMP1);
    AampStreamSinkManager::GetInstance().UpdateTuningPlayer(mPlayerInstanceAAMP1);

	/* No inactive stream sink for player
	 * Another player must exist so that the single pipeline isn't destroyed when the active player is deleted,
	 * so create a second player, which is currently inactive. */
	AampStreamSinkManager::GetInstance().CreateStreamSink(mPlayerInstanceAAMP2, mId3HandlerCallback2);
	AampStreamSinkManager::GetInstance().DeleteStreamSink(mPlayerInstanceAAMP1);
	AampStreamSinkManager::GetInstance().UpdateTuningPlayer(mPlayerInstanceAAMP1);
}

/* Test Procedure: -
    @brief Test UpdateTuningPlayer, we wouldn't expect the player to change in multi pipeline mode.
*/
TEST_F(AampStreamSinkManagerTests, UpdateTuningPlayer_MultiPipelineMode_NoChange)
{
    AampStreamSinkManager::GetInstance().CreateStreamSink(mPlayerInstanceAAMP1, mId3HandlerCallback1);
	AampStreamSinkManager::GetInstance().ActivatePlayer(mPlayerInstanceAAMP1);
    EXPECT_CALL(*g_mockAampGstPlayer, ChangeAamp(mPlayerInstanceAAMP1, _)).Times(0);
    AampStreamSinkManager::GetInstance().UpdateTuningPlayer(mPlayerInstanceAAMP1);
}

/*
    @brief: - Tests the scenario when single PlayerInstanceAAMP is deleted, the sink is deleted.
    Test Procedure: -
    In Single pipeline mode, create a stream sink.
    Delete the PlayerInstanceAAMP, this should delete the player since was only one PlayerInstanceAAMP.
*/
TEST_F(AampStreamSinkManagerTests, DeleteStreamSinkTest1)
{
    AampStreamSinkManager::GetInstance().CreateStreamSink(mPlayerInstanceAAMP1, mId3HandlerCallback1);
    AampStreamSinkManager::GetInstance().SetSinglePipelineMode(mPlayerInstanceAAMP1);
    AampStreamSinkManager::GetInstance().DeleteStreamSink(mPlayerInstanceAAMP1);

    StreamSink *sink1 = AampStreamSinkManager::GetInstance().GetStreamSink(mPlayerInstanceAAMP1);
    EXPECT_EQ(sink1, nullptr);
}

/*
    @brief: - Tests the scenario when two PlayerInstanceAAMP exists and one of them is active
              then deleting the inactive PlayerInstanceAAMP has no effect on active instance
    Test Procedure: -
    In Single pipeline mode.
    Create two stream sink - first player is active.
    Delete the inactiveplayer.
    Calling GetStreamSink return AAMPGstPlayer type of Object.
*/
TEST_F(AampStreamSinkManagerTests, DeleteStreamSinkTest2)
{

    AAMPGstPlayer gstPlayerobj { mPlayerInstanceAAMP1, mId3HandlerCallback1};

    AampStreamSinkManager::GetInstance().CreateStreamSink(mPlayerInstanceAAMP1, mId3HandlerCallback1);
    AampStreamSinkManager::GetInstance().SetSinglePipelineMode(mPlayerInstanceAAMP1);
    AampStreamSinkManager::GetInstance().CreateStreamSink(mPlayerInstanceAAMP2, mId3HandlerCallback2);
    AampStreamSinkManager::GetInstance().DeleteStreamSink(mPlayerInstanceAAMP2);

    StreamSink *sink1 = AampStreamSinkManager::GetInstance().GetStreamSink(mPlayerInstanceAAMP1);
    EXPECT_EQ(typeid(*sink1), typeid(gstPlayerobj));
}

/*
    @brief: - When an active mPlayerInstanceAAMP is deactivated, it gets assigned to AampStreamSinkInactive
    Test Procedure: -
    Enable single pipeline mode and Call Create stream sink
    Check the typeid of returned from GetStreamSink Matches AAMPGstPlayer
    Now deactivate player
    Check the typeid of returned from GetStreamSink Matches AampStreamSinkInactive
*/
TEST_F(AampStreamSinkManagerTests, Deactivateplayer)
{
    AampStreamSinkInactive inactiveSink {mId3HandlerCallback1};
    AAMPGstPlayer gstPlayerobj { mPlayerInstanceAAMP1, mId3HandlerCallback1};

    AampStreamSinkManager::GetInstance().CreateStreamSink(mPlayerInstanceAAMP1, mId3HandlerCallback1);
    AampStreamSinkManager::GetInstance().SetSinglePipelineMode(mPlayerInstanceAAMP1);
    StreamSink *sink1 = AampStreamSinkManager::GetInstance().GetStreamSink(mPlayerInstanceAAMP1);
    EXPECT_EQ(typeid(gstPlayerobj), typeid(*sink1));

    AampStreamSinkManager::GetInstance().DeactivatePlayer(mPlayerInstanceAAMP1, false);
    StreamSink *sink2 = AampStreamSinkManager::GetInstance().GetStreamSink(mPlayerInstanceAAMP1);
    EXPECT_EQ(typeid(*sink2), typeid(inactiveSink));
}

/*  @brief : - Establishes that the multipipeline mode of operation is active.
    Test Procedure: -
    Create 2 Players ( i.e. call CreateSinkStream twice)
    The first player is active, get the stream sink for it and store it
    Make Second Player active, get the stream sink for it.
    Compare it with the stream sink for first player, they should be different
*/
TEST_F(AampStreamSinkManagerTests, CheckMultiPipelineMode)
{
    AampStreamSinkManager::GetInstance().CreateStreamSink(mPlayerInstanceAAMP1, mId3HandlerCallback1);
    AampStreamSinkManager::GetInstance().CreateStreamSink(mPlayerInstanceAAMP2, mId3HandlerCallback2);
    StreamSink *sink1 = AampStreamSinkManager::GetInstance().GetStreamSink(mPlayerInstanceAAMP1);

    AampStreamSinkManager::GetInstance().ActivatePlayer(mPlayerInstanceAAMP2);
    StreamSink *sink2 = AampStreamSinkManager::GetInstance().GetStreamSink(mPlayerInstanceAAMP2);

    EXPECT_NE(sink1, sink2);
}

/* Test Procedure: -
    @brief In MultiPipeline mode GetStoppingStreamSink returns the same as GetStreamSink
*/
TEST_F(AampStreamSinkManagerTests, GetStoppingStreamSink_MultiPipelineMode)
{
    AampStreamSinkManager::GetInstance().CreateStreamSink(mPlayerInstanceAAMP1, mId3HandlerCallback1);
    StreamSink *sink1 = AampStreamSinkManager::GetInstance().GetStreamSink(mPlayerInstanceAAMP1);
    StreamSink *sink2 = AampStreamSinkManager::GetInstance().GetStoppingStreamSink(mPlayerInstanceAAMP1);

    EXPECT_EQ(sink1, sink2);
}

/* Test Procedure: -
    @brief Without Creating any sink; call GetStreamSink; nullptr should be returned
*/
TEST_F(AampStreamSinkManagerTests, MultiPipelineMode_CheckGetStreamSink1)
{
    StreamSink *sink1 = AampStreamSinkManager::GetInstance().GetStreamSink(mPlayerInstanceAAMP1);
    EXPECT_EQ(sink1, nullptr);
}

/*  Test Procedure
    @brief Verifies deletion of a sink
    Create StreamSink, verify that sink if of AAMPGstPlayer.
    Delete it,call to GetStreamSink Should return null.
*/
TEST_F(AampStreamSinkManagerTests, MultiPipelineMode_CheckGetStreamSink2)
{
    AAMPGstPlayer gstPlayerobj { mPlayerInstanceAAMP1, mId3HandlerCallback1};
    AampStreamSinkManager::GetInstance().CreateStreamSink(mPlayerInstanceAAMP1, mId3HandlerCallback1);

    StreamSink *sink1 = AampStreamSinkManager::GetInstance().GetStreamSink(mPlayerInstanceAAMP1);
    EXPECT_EQ(typeid(gstPlayerobj), typeid(*sink1));

    AampStreamSinkManager::GetInstance().DeleteStreamSink(mPlayerInstanceAAMP1);
    sink1 = AampStreamSinkManager::GetInstance().GetStreamSink(mPlayerInstanceAAMP1);
    EXPECT_EQ(sink1, nullptr);
}

/*
    @brief: - verifies that ChangeAamp receives expected parameters
    Test Procedure
    Initialize AampStreamSinkManager into single pipeline mode.
    Create 1 sink, it will be the active sink. Verify test requirments
*/
TEST_F(AampStreamSinkManagerTests, ChangeAampTests)
{
    EXPECT_CALL(*g_mockAampGstPlayer, ChangeAamp(mPlayerInstanceAAMP1, _)).Times(0);
    EXPECT_CALL(*g_mockAampGstPlayer, ChangeAamp(mPlayerInstanceAAMP2, _)).Times(0);

    AampStreamSinkManager::GetInstance().CreateStreamSink(mPlayerInstanceAAMP1, mId3HandlerCallback1);
    AampStreamSinkManager::GetInstance().SetSinglePipelineMode(mPlayerInstanceAAMP1);

    AampStreamSinkManager::GetInstance().CreateStreamSink(mPlayerInstanceAAMP2, mId3HandlerCallback2);

    EXPECT_CALL(*g_mockAampGstPlayer, ChangeAamp(mPlayerInstanceAAMP1, _)).Times(0);
    AampStreamSinkManager::GetInstance().ActivatePlayer(mPlayerInstanceAAMP1);

    /* ActivatePlayer() calls PlayerInstanceAAMP::GetPositionMs() to get the current position of the
    second AAMP private instance and AAMPGstPlayer::Flush() with the position in seconds. */
    long long positionMs = 5000;
    EXPECT_CALL(*g_mockPlayerInstanceAAMP, GetPositionMs()).WillOnce(Return(positionMs));
    double positionSec = (positionMs / 1000.0);
    EXPECT_CALL(*g_mockAampGstPlayer, ChangeAamp(mPlayerInstanceAAMP2, _)).Times(1);
    EXPECT_CALL(*g_mockAampGstPlayer, Flush(positionSec, _, true)).Times(1);
    AampStreamSinkManager::GetInstance().ActivatePlayer(mPlayerInstanceAAMP2);
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

    EXPECT_CALL(*g_mockAampGstPlayer, ChangeAamp(mPlayerInstanceAAMP1, _)).Times(0);
    AampStreamSinkManager::GetInstance().CreateStreamSink(mPlayerInstanceAAMP1, mId3HandlerCallback1);
    AampStreamSinkManager::GetInstance().SetSinglePipelineMode(mPlayerInstanceAAMP1);

    EXPECT_CALL(*g_mockAampGstPlayer, SetEncryptedAamp(mPlayerInstanceAAMP1));
    AampStreamSinkManager::GetInstance().SetEncryptedHeaders(mPlayerInstanceAAMP1, set_headers);
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
    AAMPGstPlayer gstPlayerobj {mPlayerInstanceAAMP1, mId3HandlerCallback1};

    AampStreamSinkManager::GetInstance().CreateStreamSink(mPlayerInstanceAAMP1, mId3HandlerCallback1);
    AampStreamSinkManager::GetInstance().SetSinglePipelineMode(mPlayerInstanceAAMP1);

    AampStreamSinkManager::GetInstance().CreateStreamSink(mPlayerInstanceAAMP2, mId3HandlerCallback2);

    StreamSink *sink1 = AampStreamSinkManager::GetInstance().GetActiveStreamSink(mPlayerInstanceAAMP1);
    ASSERT_NE(nullptr, sink1);
    EXPECT_EQ(typeid(gstPlayerobj), typeid(*sink1));

    StreamSink *sink2 = AampStreamSinkManager::GetInstance().GetActiveStreamSink(mPlayerInstanceAAMP2);
    ASSERT_NE(nullptr, sink2);
    EXPECT_EQ(sink1, sink2);
}

/*  @brief : - Establishes that the multipipeline mode of operation is active when client StreamSink set.
    Test Procedure: -
    Check that no StreamSink associated with Players
    Set a unique client StreamSink to 2 Players ( i.e. call SetStreamSink twice)
    Get each StreamSink and check they return the corresponding client StreamSink
    Get active StreamSink and check they return the corresponding client StreamSink
    Delete each StreamSink associated with each Player and check getting StreamSink returns nullptr
*/
TEST_F(AampStreamSinkManagerTests, CheckSetStreamSink)
{
    MockStreamSink streamSinkMock1, streamSinkMock2;

    StreamSink *sink1 = AampStreamSinkManager::GetInstance().GetStreamSink(mPlayerInstanceAAMP1);
    EXPECT_EQ(sink1, nullptr);

    StreamSink *sink2 = AampStreamSinkManager::GetInstance().GetStreamSink(mPlayerInstanceAAMP2);
    EXPECT_EQ(sink2, nullptr);

    AampStreamSinkManager::GetInstance().SetStreamSink(mPlayerInstanceAAMP1, &streamSinkMock1);
    AampStreamSinkManager::GetInstance().SetStreamSink(mPlayerInstanceAAMP2, &streamSinkMock2);

    sink1 = AampStreamSinkManager::GetInstance().GetStreamSink(mPlayerInstanceAAMP1);
    EXPECT_EQ(sink1, &streamSinkMock1);

    sink2 = AampStreamSinkManager::GetInstance().GetStreamSink(mPlayerInstanceAAMP2);
    EXPECT_EQ(sink2, &streamSinkMock2);

    sink1 = AampStreamSinkManager::GetInstance().GetActiveStreamSink(mPlayerInstanceAAMP1);
    EXPECT_EQ(sink1, &streamSinkMock1);

    sink2 = AampStreamSinkManager::GetInstance().GetActiveStreamSink(mPlayerInstanceAAMP2);
    EXPECT_EQ(sink2, &streamSinkMock2);

    AampStreamSinkManager::GetInstance().DeleteStreamSink(mPlayerInstanceAAMP1);
    sink1 = AampStreamSinkManager::GetInstance().GetStreamSink(mPlayerInstanceAAMP1);
    EXPECT_EQ(sink1, nullptr);

    AampStreamSinkManager::GetInstance().DeleteStreamSink(mPlayerInstanceAAMP2);
    sink2 = AampStreamSinkManager::GetInstance().GetStreamSink(mPlayerInstanceAAMP2);
    EXPECT_EQ(sink2, nullptr);
}

/*  @brief : - Extra test to check consistent behavior with obtaining StreamSink
*/
TEST_F(AampStreamSinkManagerTests, CheckMultipipeline1)
{
    AampStreamSinkManager::GetInstance().CreateStreamSink(mPlayerInstanceAAMP1, mId3HandlerCallback1);
    AampStreamSinkManager::GetInstance().CreateStreamSink(mPlayerInstanceAAMP2, mId3HandlerCallback2);

    StreamSink *sink1 = AampStreamSinkManager::GetInstance().GetStreamSink(mPlayerInstanceAAMP1);
    StreamSink *sink2 = AampStreamSinkManager::GetInstance().GetStreamSink(mPlayerInstanceAAMP2);
    StreamSink *sink;

    sink = AampStreamSinkManager::GetInstance().GetActiveStreamSink(mPlayerInstanceAAMP1);
    ASSERT_NE(nullptr, sink);
    EXPECT_EQ(sink, sink1);

    sink = AampStreamSinkManager::GetInstance().GetActiveStreamSink(mPlayerInstanceAAMP2);
    ASSERT_NE(nullptr, sink2);
    EXPECT_EQ(sink, sink2);

    sink = AampStreamSinkManager::GetInstance().GetStreamSink(mPlayerInstanceAAMP1);
    ASSERT_NE(nullptr, sink);
    EXPECT_EQ(sink, sink1);

    sink = AampStreamSinkManager::GetInstance().GetStreamSink(mPlayerInstanceAAMP2);
    ASSERT_NE(nullptr, sink2);
    EXPECT_EQ(sink, sink2);
}

/*  @brief : - Extra test to check consistent behavior with obtaining StreamSink after deletion
*/
TEST_F(AampStreamSinkManagerTests,  CheckMultipipeline2)
{
    AampStreamSinkManager::GetInstance().CreateStreamSink(mPlayerInstanceAAMP1, mId3HandlerCallback1);

    StreamSink *sink1 = AampStreamSinkManager::GetInstance().GetStreamSink(mPlayerInstanceAAMP1);
    StreamSink *sink;

    AampStreamSinkManager::GetInstance().DeleteStreamSink(mPlayerInstanceAAMP1);

    sink = AampStreamSinkManager::GetInstance().GetStreamSink(mPlayerInstanceAAMP1);
    EXPECT_EQ(sink, nullptr);

    AampStreamSinkManager::GetInstance().CreateStreamSink(mPlayerInstanceAAMP1, mId3HandlerCallback1);

    sink = AampStreamSinkManager::GetInstance().GetStreamSink(mPlayerInstanceAAMP1);
    ASSERT_NE(nullptr, sink);
}
