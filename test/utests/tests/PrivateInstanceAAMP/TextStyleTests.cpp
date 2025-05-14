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

#include "priv_aamp.h"

#include "AampConfig.h"
#include "MockAampConfig.h"
#include "MockAampGstPlayer.h"
#include "MockStreamAbstractionAAMP.h"
#include "MockAampStreamSinkManager.h"

using ::testing::_;
using ::testing::WithParamInterface;
using ::testing::An;
using ::testing::DoAll;
using ::testing::SetArgReferee;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::NiceMock;
using ::testing::AnyNumber;

class TextStyleTests : public ::testing::Test
{
protected:
    PrivateInstanceAAMP *mPrivateInstanceAAMP{};

    void SetUp() override
    {
        if(gpGlobalConfig == nullptr)
        {
            gpGlobalConfig =  new AampConfig();
        }

        mPrivateInstanceAAMP = new PrivateInstanceAAMP(gpGlobalConfig);

        g_mockAampGstPlayer = new MockAAMPGstPlayer( mPrivateInstanceAAMP);
        g_mockStreamAbstractionAAMP = new MockStreamAbstractionAAMP(mPrivateInstanceAAMP);
		g_mockAampStreamSinkManager = new NiceMock<MockAampStreamSinkManager>();

        mPrivateInstanceAAMP->mpStreamAbstractionAAMP = g_mockStreamAbstractionAAMP;

   		EXPECT_CALL(*g_mockAampStreamSinkManager, GetStreamSink(_)).WillRepeatedly(Return(g_mockAampGstPlayer));
    }

    void TearDown() override
    {
        delete mPrivateInstanceAAMP;
        mPrivateInstanceAAMP = nullptr;

        delete g_mockStreamAbstractionAAMP;
        g_mockStreamAbstractionAAMP = nullptr;

        delete g_mockAampGstPlayer;
        g_mockAampGstPlayer = nullptr;

        delete gpGlobalConfig;
        gpGlobalConfig = nullptr;

		delete g_mockAampStreamSinkManager;
		g_mockAampStreamSinkManager = nullptr;
    }

public:

};

// Test calling GetTextStyle
// Simulate not handled by StreamAbstractionAAMP nor StreamSink
TEST_F(TextStyleTests, GetTextStyle)
{
    // Check that TextStyle has not been applied
    EXPECT_TRUE(mPrivateInstanceAAMP->GetTextStyle().empty());
}

// Test calling SetTextStyle
// Simulate handled by StreamAbstractionAAMP
TEST_F(TextStyleTests, SetTextStyle_ViaStreamAbstraction)
{
    std::string options = "{ \"penSize\":\"small\" }";

    ASSERT_TRUE(mPrivateInstanceAAMP->GetTextStyle().empty());

    // Check that StreamAbstractionAAMP::SetTextStyle is called
    EXPECT_CALL(*g_mockStreamAbstractionAAMP, SetTextStyle(options)).WillOnce(Return(true));

    // Check that StreamAbstractionAAMP::SetTextStyle is called
    EXPECT_CALL(*g_mockAampGstPlayer, SetTextStyle(options)).Times(0);

    mPrivateInstanceAAMP->SetTextStyle(options);

    // Check that TextStyle has been applied
    EXPECT_EQ(options, mPrivateInstanceAAMP->GetTextStyle());
}

// Test calling SetTextStyle
// Simulate handled by StreamSink
TEST_F(TextStyleTests, SetTextStyle_ViaStreamSink)
{
    std::string options = "{ \"penSize\":\"small\" }";

    ASSERT_TRUE(mPrivateInstanceAAMP->GetTextStyle().empty());

    // Check that StreamAbstractionAAMP::SetTextStyle is called
    EXPECT_CALL(*g_mockStreamAbstractionAAMP, SetTextStyle(options)).WillOnce(Return(false));

    // Check that StreamSink::SetTextStyle is called
    EXPECT_CALL(*g_mockAampGstPlayer, SetTextStyle(options)).WillOnce(Return(true));

    mPrivateInstanceAAMP->SetTextStyle(options);

    // Check that TextStyle has been applied
    EXPECT_EQ(options, mPrivateInstanceAAMP->GetTextStyle());
}

// Test calling SetTextStyle
// Simulate not handled by StreamAbstractionAAMP nor StreamSink
TEST_F(TextStyleTests, SetTextStyle_NotHandled)
{
    std::string options = "{ \"penSize\":\"small\" }";

    ASSERT_TRUE(mPrivateInstanceAAMP->GetTextStyle().empty());

    // Check that StreamAbstractionAAMP::SetTextStyle is called
    EXPECT_CALL(*g_mockStreamAbstractionAAMP, SetTextStyle(options)).WillOnce(Return(false));

    // Check that StreamSink::SetTextStyle is called
    EXPECT_CALL(*g_mockAampGstPlayer, SetTextStyle(options)).WillOnce(Return(false));

    mPrivateInstanceAAMP->SetTextStyle(options);

    // Check that TextStyle has not been applied
    EXPECT_TRUE(mPrivateInstanceAAMP->GetTextStyle().empty());
}
