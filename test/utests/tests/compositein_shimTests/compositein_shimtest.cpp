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
#include "compositein_shim.h"
#include "videoin_shim.h"
#include <string>
#include <stdint.h>
#include <iostream>
#include "priv_aamp.h"

using namespace testing;
AampConfig *gpGlobalConfig{nullptr};

PrivateInstanceAAMP *mPrivateInstanceAAMP{};

class  StreamAbstractionAAMP_COMPOSITEINTEST: public ::testing::Test {
protected:
    
    void SetUp() override {
            
             mPrivateInstanceAAMP = new PrivateInstanceAAMP();
             compositeInput = StreamAbstractionAAMP_COMPOSITEIN::GetInstance(mPrivateInstanceAAMP, 0.0, 1.0);
    
        }
    
        void TearDown() override {
            compositeInput->ResetInstance();
        }

    StreamAbstractionAAMP_COMPOSITEIN* compositeInput;
};

TEST_F(StreamAbstractionAAMP_COMPOSITEINTEST, InitTest) {
    
    // Act: Call the Init function with a specific TuneType
    AAMPStatusType result = compositeInput->Init(TuneType::eTUNETYPE_NEW_NORMAL);

    //  Assert: Check if the result matches the expected value
    EXPECT_EQ(result, eAAMPSTATUS_OK); 
}

/* 
While calling this function getting segment falt as in function defination aamp obj pointion to NULL
*/
TEST_F(StreamAbstractionAAMP_COMPOSITEINTEST, StartTest)
{
    // Call the Startfunction
    compositeInput->Start();
}

TEST_F(StreamAbstractionAAMP_COMPOSITEINTEST, StopTest) {
    
    // Act: Call the Stop function with clearChannelData set to true
    compositeInput->Stop(true);
}
