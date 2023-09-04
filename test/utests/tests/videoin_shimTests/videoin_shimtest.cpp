#include <gtest/gtest.h>
#include "videoin_shim.h"
#include "priv_aamp.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <assert.h>
#include "AampUtils.h"

using namespace testing;
AampConfig *gpGlobalConfig{nullptr};

class StreamAbstractionAAMP_VIDEOINTest : public ::testing::Test
{
    
    // public:
    //  AAMPStatusType callInitHelper(TuneType tuneType) {
    //     InitHelper(tuneType);
    // }
    protected:
    void SetUp() override
    {
    auto aamp = new PrivateInstanceAAMP();
    auto logmanager = new AampLogManager();
    std::string name = "Name"; // Provide the appropriate name
    std::string callSign = "CallSign"; // Provide the appropriate callSign
    videoinShim = new StreamAbstractionAAMP_VIDEOIN(name, callSign, logmanager, aamp, 0.0, 1.0);
        
    }

    void TearDown() override
    {
        //delete videoinShim;
    }

    StreamAbstractionAAMP_VIDEOIN *videoinShim;
};

// Google Test for StreamAbstractionAAMP_VIDEOIN::Init
TEST_F(StreamAbstractionAAMP_VIDEOINTest, InitTest)
{
    TuneType tuneType = eTUNETYPE_NEW_NORMAL; // Set the tuneType as needed

    AAMPStatusType result = videoinShim->Init(tuneType);

    EXPECT_EQ(result, eAAMPSTATUS_OK);
}
// Google Test for StreamAbstractionAAMP_VIDEOIN Destructor

TEST_F(StreamAbstractionAAMP_VIDEOINTest,  DestructorTest)
{
    videoinShim->~StreamAbstractionAAMP_VIDEOIN();
}
// Google Test for StreamAbstractionAAMP_VIDEOIN Start 
TEST_F(StreamAbstractionAAMP_VIDEOINTest,  StartTest)
{
    videoinShim->Start();
}
// Google Test for StreamAbstractionAAMP_VIDEOIN Stop
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
// Define a test case for the SetVideoRectangle function
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
