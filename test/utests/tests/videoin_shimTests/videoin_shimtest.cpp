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
#include "videoin_shim.h"
#include "priv_aamp.h"
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <assert.h>
#include "AampUtils.h"

using namespace testing;
AampConfig *gpGlobalConfig{nullptr};

class StreamAbstractionAAMP_VIDEOINTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        auto aamp = new PrivateInstanceAAMP();
        std::string type = "HDMI";
        std::string name = "Name";       // Provide the appropriate name
        PlayerThunderAccessPlugin callSign = PlayerThunderAccessPlugin::COMPOSITEINPUT; // Provide the appropriate callSign
        videoinShim = new TestableStreamAbstraction(name, callSign, aamp, 0.0, 1.0, type);
    }

    void TearDown() override
    {
        delete videoinShim;
    }

    class TestableStreamAbstraction : public StreamAbstractionAAMP_VIDEOIN
    {
    public:
        TestableStreamAbstraction(const std::string &name, const PlayerThunderAccessPlugin &callSign,
                                  PrivateInstanceAAMP *aamp,
                                  double startTime, double playRate, const std::string &type)
            : StreamAbstractionAAMP_VIDEOIN(name, callSign, aamp, startTime, playRate, type)
        {
        }

        AAMPStatusType CallInitHelper(TuneType tuneType)
        {
            return InitHelper(tuneType);
        }

        void CallStartHelper(int port)
        {
            StartHelper(port);
        }

        void CallStopHelper()
        {
            StopHelper();
        }
    };

    TestableStreamAbstraction *videoinShim;
};


TEST_F(StreamAbstractionAAMP_VIDEOINTest,InitHelpertest)
{
    TuneType tuneType = eTUNETYPE_NEW_NORMAL;
    AAMPStatusType result = videoinShim->CallInitHelper(tuneType);
    EXPECT_EQ(result, eAAMPSTATUS_OK);
}

TEST_F(StreamAbstractionAAMP_VIDEOINTest,StartHelpertest)
{
    int port = 8080;
    videoinShim->CallStartHelper(port);
}

TEST_F(StreamAbstractionAAMP_VIDEOINTest,StopHelpertest)
{
    videoinShim->CallStopHelper();
}

TEST_F(StreamAbstractionAAMP_VIDEOINTest, InitTest)
{
    TuneType tuneType = eTUNETYPE_NEW_NORMAL; // Set the tuneType as needed

    AAMPStatusType result = videoinShim->Init(tuneType);

    EXPECT_EQ(result, eAAMPSTATUS_OK);
}

TEST_F(StreamAbstractionAAMP_VIDEOINTest,  StartTest)
{
    videoinShim->Start();
}

TEST_F(StreamAbstractionAAMP_VIDEOINTest,  StopTest)
{
    videoinShim->Stop(true);
}

TEST_F(StreamAbstractionAAMP_VIDEOINTest,  GetStreamFormatTest){


    // Initialize output format variables with some non-default values
    StreamOutputFormat primaryFormat = FORMAT_UNKNOWN;
    StreamOutputFormat audioFormat = FORMAT_UNKNOWN;
    StreamOutputFormat auxAudioFormat = FORMAT_UNKNOWN;
    StreamOutputFormat subtitleFormat = FORMAT_UNKNOWN;

    // Call the GetStreamFormat function
    videoinShim->GetStreamFormat(primaryFormat, audioFormat, auxAudioFormat, subtitleFormat);

    // Assert that the output formats are set to FORMAT_INVALID
    ASSERT_EQ(primaryFormat, FORMAT_INVALID);
    ASSERT_EQ(audioFormat, FORMAT_INVALID);

}

TEST_F(StreamAbstractionAAMP_VIDEOINTest,  GetFirstPTSTest){

    double firstPTS = videoinShim->GetFirstPTS();
    // Assert that the returned value is 0.0 (the expected stub value)
    ASSERT_DOUBLE_EQ(firstPTS, 0.0);

}

TEST_F(StreamAbstractionAAMP_VIDEOINTest,IsInitialCachingSupportedTest){

    // Call the IsInitialCachingSupported function
    bool initialCachingSupported = videoinShim->IsInitialCachingSupported();
    // Assert that the returned value is false (the expected stub value)
    ASSERT_FALSE(initialCachingSupported);
}

TEST_F(StreamAbstractionAAMP_VIDEOINTest,GetMaxBitrateTest){

    // Call the GetMaxBitrate function
      BitsPerSecond maxBitrate = videoinShim->GetMaxBitrate();
    // Assert that the returned value is 0 (the expected stub value)
    ASSERT_EQ(maxBitrate, 0);

}

TEST_F(StreamAbstractionAAMP_VIDEOINTest, SetVideoRectangleTest) {

    // Set up the expected parameters
    int x = 10, y = 20, w = 640, h = 480; // Example values
    // JsonObject expectedParams;
    // expectedParams["x"] = 10;
    // expectedParams["y"] = 20;
    // expectedParams["w"] = 640;
    // expectedParams["h"] = 480;

    // Call the SetVideoRectangle function
    videoinShim->SetVideoRectangle(x, y, w, h);
}
