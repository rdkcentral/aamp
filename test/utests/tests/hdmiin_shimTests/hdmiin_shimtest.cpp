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

#include "hdmiin_shim.h"
#include <gtest/gtest.h>
#include "priv_aamp.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <assert.h>
#include "AampUtils.h"

using namespace testing;
AampConfig *gpGlobalConfig{nullptr};
AampLogManager *mLogObj{nullptr};
PrivateInstanceAAMP *mPrivateInstanceAAMP{};

class StreamAbstractionAAMP_HDMIINTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        mPrivateInstanceAAMP = new PrivateInstanceAAMP();
        mLogObj = new AampLogManager();
        HDMIinput= new StreamAbstractionAAMP_HDMIIN(mLogObj, mPrivateInstanceAAMP, 0.0, 1.0);
    }

    void TearDown() override
    {
        delete HDMIinput;
    }

    StreamAbstractionAAMP_HDMIIN *HDMIinput;
};

TEST_F(StreamAbstractionAAMP_HDMIINTest, DestructorTest)
{
    StreamAbstractionAAMP_HDMIIN* HDMIinput_1 = new StreamAbstractionAAMP_HDMIIN(mLogObj, mPrivateInstanceAAMP, 0.0, 1.0);
    // Act: Call the destructor explicitly
    HDMIinput_1->~StreamAbstractionAAMP_HDMIIN();
}

TEST_F(StreamAbstractionAAMP_HDMIINTest, InitRegistersEvents)
{
    // Act:Call the Init function
    TuneType tuneType =  eTUNETYPE_NEW_NORMAL; // Replace with the actual tune type
    AAMPStatusType result = HDMIinput->Init(tuneType);

   // Assert:Check if the result matches the expected value
    EXPECT_EQ(result, eAAMPSTATUS_OK);    
}

TEST_F(StreamAbstractionAAMP_HDMIINTest,StopTest)
{
    // Act: Call the Stop function with clearChannelData set to true
    HDMIinput->Stop(true);
}

TEST_F(StreamAbstractionAAMP_HDMIINTest, StartTest)
{    
     // Call the Startfunction 
     HDMIinput->Start();
}


