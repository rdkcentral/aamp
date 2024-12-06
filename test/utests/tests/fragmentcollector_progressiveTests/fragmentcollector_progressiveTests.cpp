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
#include "fragmentcollector_progressive.h"
#include "AampConfig.h"
using namespace testing;
AampConfig *gpGlobalConfig{ nullptr };

class fragmentcollector_progressiveTests : public ::testing::Test
{
protected:

    void SetUp() override
    {  
        PrivateInstanceAAMP* aamp = new PrivateInstanceAAMP();
        aamp = new PrivateInstanceAAMP();
        double seek_pos = 0.0;  // Provide the desired seek_pos value
        float rate = 1.0;       // Provide the desired rate value
     profileEvent = new StreamAbstractionAAMP_PROGRESSIVE(aamp, seek_pos, rate);
  
    }


    void TearDown() override
    {
        delete profileEvent;
        profileEvent = nullptr;
       
    }

   StreamAbstractionAAMP_PROGRESSIVE* profileEvent;
   
};

TEST_F(fragmentcollector_progressiveTests, StopTest) {
    // Call the Start function
   profileEvent->Stop(true);
}

TEST_F(fragmentcollector_progressiveTests,  GetFirstPTSTest){
    double firstPTS = profileEvent->GetFirstPTS();
    // Assert that the returned value is 0.0 (the expected stub value)
    ASSERT_DOUBLE_EQ(firstPTS, 0.0);
}

TEST_F(fragmentcollector_progressiveTests,  GetStreamPositionTests){
    double firstPTS = profileEvent->GetStreamPosition();
    // Assert that the returned value is 0.0 (the expected stub value)
    ASSERT_DOUBLE_EQ(firstPTS, 0.0);
}

TEST_F(fragmentcollector_progressiveTests,  IsInitialCachingSupportedTest){
      // Call the IsInitialCachingSupported function
    bool initialCachingSupported = profileEvent->IsInitialCachingSupported();
    // Assert that the returned value is false (the expected stub value)
    ASSERT_FALSE(initialCachingSupported);
}
TEST_F(fragmentcollector_progressiveTests,GetMaxBitrateTest){
   // Call the GetMaxBitrate function
      BitsPerSecond maxBitrate = profileEvent->GetMaxBitrate();
    // Assert that the returned value is 0 (the expected stub value)
    ASSERT_EQ(maxBitrate, 0);
}
TEST_F(fragmentcollector_progressiveTests, GetStreamFormatTest) {
    // Create variables to hold the output format results
    StreamOutputFormat primaryOutputFormat, audioOutputFormat, auxAudioOutputFormat, subtitleOutputFormat;

    // Call the GetStreamFormat method
    profileEvent->GetStreamFormat(primaryOutputFormat, audioOutputFormat, auxAudioOutputFormat, subtitleOutputFormat);

    ASSERT_EQ(primaryOutputFormat, FORMAT_ISO_BMFF);

    ASSERT_EQ(audioOutputFormat, FORMAT_INVALID);

    ASSERT_EQ(auxAudioOutputFormat, FORMAT_INVALID);

    ASSERT_EQ(subtitleOutputFormat, FORMAT_INVALID);
}

TEST_F(fragmentcollector_progressiveTests, Destructor)
{
    // Create an instance of StreamAbstractionAAMP_PROGRESSIVE
    PrivateInstanceAAMP aamp;
    double seekPosition = 0.0;
    float rate = 1.0;
    StreamAbstractionAAMP_PROGRESSIVE streamAbstraction(&aamp, seekPosition, rate);


    // Call the destructor explicitly (optional)
    streamAbstraction.~StreamAbstractionAAMP_PROGRESSIVE();
}
