/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2020 RDK Management
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
#include "AampCMCDCollector.h"
#include "CMCDHeaders.h"
#include "AampMediaType.h"
#include "AampConfig.h"
using namespace testing;
AampConfig *gpGlobalConfig{nullptr};

CMCDHeaders *CMCDHeaders_obj = new CMCDHeaders; 
class AampCMCDCollectorTest : public Test {
protected:
    void SetUp() override {
        collector = new AampCMCDCollector();
    }

    void TearDown() override {
        delete collector;
        collector = nullptr;
    }

public:
    AampCMCDCollector *collector{nullptr};
};

//New Testcases Start from here

/**
 * TEST_F GTest-Testcase for Initialize function for AampCMCDCollectorTest file
**/
TEST_F(AampCMCDCollectorTest, Initialize_1)
{
    // Arrange: Creating the variables for passing to arguments
    bool bCMCDEnabled_Test = true;
    std::string traceId_Test = " ";

    //Act: Call the function for test
    collector->Initialize(bCMCDEnabled_Test,traceId_Test);
	CMCDHeaders_obj->SetSessionId(traceId_Test);
    std::string sessionId_Test = CMCDHeaders_obj->GetSessionId();
	
    //Assert: Expecting two strings are equal or not
    EXPECT_STRNE(sessionId_Test.c_str(),traceId_Test.c_str());
}

/**
 * TEST_F GTest-Testcase for Initialize function for AampCMCDCollectorTest file
**/
TEST_F(AampCMCDCollectorTest, Initialize_2)
{
    // Arrange: Creating the variables for passing to arguments
    bool bCMCDEnabled_Test = true;
    std::string traceId_Test = "unknown";

    //Act: Call the function for test
    collector->Initialize(bCMCDEnabled_Test,traceId_Test);
	CMCDHeaders_obj->SetSessionId(traceId_Test);
    std::string sessionId_Test = CMCDHeaders_obj->GetSessionId();
	
    //Assert: Expecting two strings are equal or not
    EXPECT_STRNE(sessionId_Test.c_str(),traceId_Test.c_str());
}

/**
 * TEST_F GTest-Testcase for CMCDSetNextObjectRequest function for AampCMCDCollectorTest file
**/
TEST_F(AampCMCDCollectorTest, CMCDSetNextObjectRequestTest_1)
{
    // Arrange: Creating the variables for passing to arguments
    std::string Testurl1("http://example.com");
    std::string Testurl2("http://example.com");
    long CMCDBandwidth_Test = 10;
    bool bCMCDEnabled_Test = true;
    std::string traceId_Test = " ";
    std::map<int, CMCDHeaders *> mCMCDStreamData_Test;

    // Act: Call the function for test
    collector->Initialize(bCMCDEnabled_Test,traceId_Test);
    mCMCDStreamData_Test.find(eMEDIATYPE_INIT_VIDEO);
    collector->CMCDSetNextObjectRequest(Testurl1,CMCDBandwidth_Test,eMEDIATYPE_INIT_VIDEO);
    CMCDHeaders_obj->SetNextUrl(Testurl2);

    //Assert: Expecting two strings are equal or not
    EXPECT_STREQ(Testurl2.c_str(),"http://example.com");
}

/**
 * TEST_F GTest-Testcase for CMCDSetNextObjectRequest function for AampCMCDCollectorTest file
**/
TEST_F(AampCMCDCollectorTest, CMCDSetNextObjectRequestTest_2)
{ 
    // Arrange: Creating the variables for passing to arguments
    std::string Testurl1("http://example.com");
    std::string Testurl2("http://example.com");
    long CMCDBandwidth_Test = 10;
    bool bCMCDEnabled_Test = true;
    std::string traceId_Test = " ";
    std::map<int, CMCDHeaders *> mCMCDStreamData_Test;

    // Act: Call the function for test
    collector->Initialize(bCMCDEnabled_Test,traceId_Test);
    mCMCDStreamData_Test.find(eMEDIATYPE_DEFAULT);
    collector->CMCDSetNextObjectRequest(Testurl1,CMCDBandwidth_Test,eMEDIATYPE_DEFAULT);

    // Act: Call the function for test
    collector->Initialize(bCMCDEnabled_Test,traceId_Test);
    mCMCDStreamData_Test.find(eMEDIATYPE_INIT_VIDEO);
    collector->CMCDSetNextObjectRequest(Testurl1,CMCDBandwidth_Test,eMEDIATYPE_INIT_VIDEO);
    CMCDHeaders_obj->SetNextUrl(Testurl2);

    //Assert: Expecting two strings are equal or not
    EXPECT_STREQ(Testurl2.c_str(),"http://example.com");
}

/**
 * TEST_F GTest-Testcase for CMCDGetHeaders function for AampCMCDCollectorTest file
**/
TEST_F(AampCMCDCollectorTest, CMCDGetHeadersTest_1)
{ 
    // Arrange: Creating the variables for passing to arguments
    std::map<int, CMCDHeaders *> mCMCDStreamData_Test;
    bool bCMCDEnabled_Test = true;
    std::string traceId_Test = " ";

    // Act: Call the function for test
    collector->Initialize(bCMCDEnabled_Test,traceId_Test);
    mCMCDStreamData_Test.find(eMEDIATYPE_DEFAULT);
    std::vector<std::string> customHeader_Test;
    customHeader_Test.push_back("Test_Hello");
    collector->CMCDGetHeaders(eMEDIATYPE_DEFAULT,customHeader_Test);
}

/**
 * TEST_F GTest-Testcase for CMCDGetHeaders function for AampCMCDCollectorTest file
**/
TEST_F(AampCMCDCollectorTest, CMCDGetHeadersTest_2)
{
    // Arrange: Creating the variables for passing to arguments
    std::map<int, CMCDHeaders *> mCMCDStreamData_Test;
    bool bCMCDEnabled_Test = false;
    std::string traceId_Test = " ";
    std::vector<std::string> customHeader_Test;

    // Act: Call the function for test
    collector->Initialize(bCMCDEnabled_Test,traceId_Test);
    mCMCDStreamData_Test.find(eMEDIATYPE_DEFAULT);
    customHeader_Test.push_back("Test_Hello");
    collector->CMCDGetHeaders(eMEDIATYPE_DEFAULT,customHeader_Test);
}

/**
 * TEST_F GTest-Testcase for CMCDGetHeaders function for AampCMCDCollectorTest file
**/
TEST_F(AampCMCDCollectorTest, CMCDGetHeadersTest_3)
{ 
    // Arrange: Creating the variables for passing to arguments
    std::map<int, CMCDHeaders *> mCMCDStreamData_Test;
    bool bCMCDEnabled_Test = true;
    std::string traceId_Test = " ";
    std::vector<std::string> customHeader_Test;
    
    // Act: Call the function for test
    collector->Initialize(bCMCDEnabled_Test,traceId_Test);
    mCMCDStreamData_Test.find(eMEDIATYPE_VIDEO);
    customHeader_Test.push_back("Test_Hello");
    collector->CMCDGetHeaders(eMEDIATYPE_VIDEO,customHeader_Test);
}

/**
 * TEST_F GTest-Testcase for CMCDGetHeaders function for AampCMCDCollectorTest file
**/
TEST_F(AampCMCDCollectorTest, CMCDGetHeadersTest_4)
{ 
    // Arrange: Creating the variables for passing to arguments
    std::map<int, CMCDHeaders *> mCMCDStreamData_Test;
    bool bCMCDEnabled_Test = true;
    std::string traceId_Test = " ";
    std::string headerValue_Test;
    std::vector<std::string> customHeader_Test;
    std::unordered_map<std::string, std::vector<std::string>> CMCDCustomHeaders_Test;
    
    // Assign vectors of strings to different keys
    CMCDCustomHeaders_Test["key1"] = {"value1", "value2", "value3"};
    CMCDCustomHeaders_Test["key2"] = {"value4", "value5"};

    // Act: Call the function for test
    for(std::unordered_map<std::string, std::vector<std::string>>::iterator it = CMCDCustomHeaders_Test.begin(); it != CMCDCustomHeaders_Test.end(); it++)
    {
        headerValue_Test = it->first;
        customHeader_Test = it->second;
    }
    collector->Initialize(bCMCDEnabled_Test,traceId_Test);
    mCMCDStreamData_Test.find(eMEDIATYPE_VIDEO);
    collector->CMCDGetHeaders(eMEDIATYPE_VIDEO,customHeader_Test);
}

/**
 * TEST_F GTest-Testcase for CMCDGetHeaders function for AampCMCDCollectorTest file
**/
TEST_F(AampCMCDCollectorTest, CMCDGetHeadersTest_5)
{ 
    // Arrange: Creating the variables for passing to arguments
    std::map<int, CMCDHeaders *> mCMCDStreamData_Test;
    bool bCMCDEnabled_Test = true;
    std::string traceId_Test = " ";
    std::vector<std::string> customHeader_Test;
    std::unordered_map<std::string, std::vector<std::string>> CMCDCustomHeaders_Test;
    
    // Assign vectors of strings to different keys
    CMCDCustomHeaders_Test["key1"] = {"vector1", "vector2"};
    CMCDCustomHeaders_Test["key2"] = {"vector3"};

    // Act: Call the function for test
    for(const auto& pair : CMCDCustomHeaders_Test)
    {
        for(const std::string&headerValue_Test:pair.second)
        {
            customHeader_Test.push_back(headerValue_Test);
        }
    }

    collector->Initialize(bCMCDEnabled_Test,traceId_Test);
    mCMCDStreamData_Test.find(eMEDIATYPE_VIDEO);
    collector->CMCDGetHeaders(eMEDIATYPE_VIDEO,customHeader_Test);

    //Assert: Expecting are equal or not
    //ASSERT_EQ(CMCDCustomHeaders_Test["key1"].size(),2);
    //ASSERT_EQ(CMCDCustomHeaders_Test["key2"].size(),1);

}

/** 
 * TEST_F GTest-Testcase for CMCDGetHeaders function for AampCMCDCollectorTest file
**/
TEST_F(AampCMCDCollectorTest, CMCDSetNetworkMetricsTest_1)
{ 
    // Arrange: Creating the variables for passing to arguments
    std::map<int, CMCDHeaders *> mCMCDStreamData_Test;
    bool bCMCDEnabled_Test = true;
    std::string traceId_Test = " ";
    int startTransferTime = 2;
    int totalTime = 10;
    int dnsLookUpTime = 5;

    // Act: Call the function for test
    collector->Initialize(bCMCDEnabled_Test,traceId_Test);
    mCMCDStreamData_Test.find(eMEDIATYPE_DEFAULT);
    collector->CMCDSetNetworkMetrics(eMEDIATYPE_DEFAULT,startTransferTime,totalTime,dnsLookUpTime);

    CMCDHeaders_obj->GetNetworkMetrics(startTransferTime,totalTime,dnsLookUpTime);
    CMCDHeaders_obj->SetNetworkMetrics(startTransferTime,totalTime,dnsLookUpTime);
    
    //Assert: Expecting are equal or not
    EXPECT_EQ(startTransferTime,2);
    EXPECT_EQ(totalTime,10);
    EXPECT_EQ(dnsLookUpTime,5);
}

/**
 * TEST_F GTest-Testcase for CMCDSetNetworkMetrics function for AampCMCDCollectorTest file
**/
TEST_F(AampCMCDCollectorTest, CMCDSetNetworkMetricsTest_2)
{ 
    // Arrange: Creating the variables for passing to arguments
    std::map<int, CMCDHeaders *> mCMCDStreamData_Test;
    bool bCMCDEnabled_Test = false;
    std::string traceId_Test = " ";
    int startTransferTime = 0;
    int totalTime = 5;
    int dnsLookUpTime = 0;

    // Act: Call the function for test
    collector->Initialize(bCMCDEnabled_Test,traceId_Test);
    mCMCDStreamData_Test.find(eMEDIATYPE_DEFAULT);
    collector->CMCDSetNetworkMetrics(eMEDIATYPE_DEFAULT,startTransferTime,totalTime,dnsLookUpTime);

    CMCDHeaders_obj->GetNetworkMetrics(startTransferTime,totalTime,dnsLookUpTime);
    CMCDHeaders_obj->SetNetworkMetrics(startTransferTime,totalTime,dnsLookUpTime);
    
    //Assert: Expecting values are equal or not
    EXPECT_EQ(startTransferTime,0);
    EXPECT_EQ(totalTime,5);
    EXPECT_EQ(dnsLookUpTime,0);
}

/**
 * TEST_F GTest-Testcase for CMCDSetNetworkMetrics function for AampCMCDCollectorTest file
**/
TEST_F(AampCMCDCollectorTest, CMCDSetNetworkMetricsTest_3)
{ 
    // Arrange: Creating the variables for passing to arguments
    std::map<int, CMCDHeaders *> mCMCDStreamData_Test;
    bool bCMCDEnabled_Test = true;
    std::string traceId_Test = " ";
    int startTransferTime = 0;
    int totalTime = 0;
    int dnsLookUpTime = 0;

    // Act: Call the function for test
    collector->Initialize(bCMCDEnabled_Test,traceId_Test);
    mCMCDStreamData_Test.find(eMEDIATYPE_VIDEO);
    collector->CMCDSetNetworkMetrics(eMEDIATYPE_VIDEO,startTransferTime,totalTime,dnsLookUpTime);

    CMCDHeaders_obj->GetNetworkMetrics(startTransferTime,totalTime,dnsLookUpTime);
    CMCDHeaders_obj->SetNetworkMetrics(startTransferTime,totalTime,dnsLookUpTime);
    
    //Assert: Expecting values are equal or not
    EXPECT_EQ(startTransferTime,0);
    EXPECT_EQ(totalTime,0);
    EXPECT_EQ(dnsLookUpTime,0);
}

/**
 * TEST_F GTest-Testcase for SetBitrates function for AampCMCDCollectorTest file
**/
TEST_F(AampCMCDCollectorTest, SetBitratesTest_1)
{ 
    // Arrange: Creating the variables for passing to arguments
    std::map<int, CMCDHeaders *> mCMCDStreamData_Test;
    bool bCMCDEnabled_Test = true;
    std::string traceId_Test = " ";
    std::vector<BitsPerSecond> bitrateList_Test;

    // Act: Call the function for test
    bitrateList_Test.push_back(10);
    bitrateList_Test.push_back(20);
    bitrateList_Test.push_back(30);
    collector->Initialize(bCMCDEnabled_Test,traceId_Test);
    mCMCDStreamData_Test.find(eMEDIATYPE_DEFAULT);
    collector->SetBitrates(eMEDIATYPE_DEFAULT,bitrateList_Test);
    long maxBitrate = *max_element(bitrateList_Test.begin(), bitrateList_Test.end());

    //Assert: Expecting values are equal or not
    EXPECT_EQ(maxBitrate,30);
}

/**
 * TEST_F GTest-Testcase for SetBitrates function for AampCMCDCollectorTest file
**/
TEST_F(AampCMCDCollectorTest, SetBitratesTest_2)
{ 
    // Arrange: Creating the variables for passing to arguments
    std::map<int, CMCDHeaders *> mCMCDStreamData_Test;
    bool bCMCDEnabled_Test = true;
    std::string traceId_Test = " ";
    std::vector<BitsPerSecond> bitrateList_Test;

    // Act: Call the function for test
    bitrateList_Test.push_back(10);
    bitrateList_Test.push_back(20);
    bitrateList_Test.push_back(30);
    collector->Initialize(bCMCDEnabled_Test,traceId_Test);
    mCMCDStreamData_Test.find(eMEDIATYPE_VIDEO);
    collector->SetBitrates(eMEDIATYPE_VIDEO,bitrateList_Test);
    long maxBitrate = *max_element(bitrateList_Test.begin(), bitrateList_Test.end());

    //Assert: Expecting values are equal or not
    EXPECT_EQ(maxBitrate,30);
}

/**
 * TEST_F GTest-Testcase for SetBitrates function for AampCMCDCollectorTest file
**/
TEST_F(AampCMCDCollectorTest, SetBitratesTest_3)
{ 
    // Arrange: Creating the variables for passing to arguments
    std::map<int, CMCDHeaders *> mCMCDStreamData_Test;
    bool bCMCDEnabled_Test = true;
    std::string traceId_Test = " ";
    std::vector<BitsPerSecond> bitrateList_Test;
    
    // Act: Call the function for test
    bitrateList_Test.push_back(10);
    bitrateList_Test.push_back(20);
    bitrateList_Test.push_back(30);
    collector->Initialize(bCMCDEnabled_Test,traceId_Test);
    mCMCDStreamData_Test.find(eMEDIATYPE_AUDIO);
    collector->SetBitrates(eMEDIATYPE_AUDIO,bitrateList_Test);
    long maxBitrate = *max_element(bitrateList_Test.begin(), bitrateList_Test.end());

    //Assert: Expecting values are equal or not
    EXPECT_EQ(maxBitrate,30);
}

/**
 * TEST_F GTest-Testcase for SetBitrates function for AampCMCDCollectorTest file
**/
TEST_F(AampCMCDCollectorTest, SetBitratesTest_4)
{ 
    // Arrange: Creating the variables for passing to arguments
    std::map<int, CMCDHeaders *> mCMCDStreamData_Test;
    bool bCMCDEnabled_Test = false;
    std::string traceId_Test = " ";
    std::vector<BitsPerSecond> bitrateList_Test;

    // Act: Call the function for test
    bitrateList_Test.push_back(10);
    bitrateList_Test.push_back(20);
    bitrateList_Test.push_back(30);
    collector->Initialize(bCMCDEnabled_Test,traceId_Test);
    mCMCDStreamData_Test.find(eMEDIATYPE_DEFAULT);
    collector->SetBitrates(eMEDIATYPE_DEFAULT,bitrateList_Test);
    long maxBitrate = *max_element(bitrateList_Test.begin(), bitrateList_Test.end());

    //Assert: Expecting values are equal or not
    EXPECT_EQ(maxBitrate,30);
}

/**
 * TEST_F GTest-Testcase for SetTrackData function for AampCMCDCollectorTest file
**/
TEST_F(AampCMCDCollectorTest, SetTrackDataTest_1)
{ 
    // Arrange: Creating the variables for passing to arguments
    std::map<int, CMCDHeaders *> mCMCDStreamData_Test;
    bool bCMCDEnabled_Test = true;
    std::string traceId_Test = " ";
    bool IsMuxed = true;
    bool bufferRedStatus = true;
    int bufferedDuration = 10;
    int currentBitrate = 10;
    
    // Act: Call the function for test
    collector->Initialize(bCMCDEnabled_Test,traceId_Test);
    mCMCDStreamData_Test.find(eMEDIATYPE_DEFAULT);
    collector->SetTrackData(eMEDIATYPE_DEFAULT,bufferRedStatus,bufferedDuration,currentBitrate,IsMuxed);

	CMCDHeaders_obj->SetMediaType("MUXED");
    std::string mediaType = CMCDHeaders_obj->GetMediaType();
	
    //Assert: Expecting values are equal or not
    EXPECT_STREQ(mediaType.c_str(),"MUXED");
    EXPECT_EQ(IsMuxed,true);
}

/**
 * TEST_F GTest-Testcase for SetTrackData function for AampCMCDCollectorTest file
**/
TEST_F(AampCMCDCollectorTest, SetTrackDataTest_2)
{ 
    // Arrange: Creating the variables for passing to arguments
    std::map<int, CMCDHeaders *> mCMCDStreamData_Test;
    bool bCMCDEnabled_Test = false;
    std::string traceId_Test = " ";
    bool IsMuxed = true;
    bool bufferRedStatus = true;
    int bufferedDuration = 10;
    int currentBitrate = 10;

    // Act: Call the function for test
    collector->Initialize(bCMCDEnabled_Test,traceId_Test);
    mCMCDStreamData_Test.find(eMEDIATYPE_DEFAULT);
    collector->SetTrackData(eMEDIATYPE_DEFAULT,bufferRedStatus,bufferedDuration,currentBitrate,IsMuxed);

	CMCDHeaders_obj->SetMediaType("MUXED");
    std::string mediaType = CMCDHeaders_obj->GetMediaType();
	
    //Assert: Expecting values are equal or not
    EXPECT_STREQ(mediaType.c_str(),"MUXED");
    EXPECT_EQ(IsMuxed,true);
}

/**
 * TEST_F GTest-Testcase for SetTrackData function for AampCMCDCollectorTest file
*/
TEST_F(AampCMCDCollectorTest, SetTrackDataTest_3)
{ 
    // Arrange: Creating the variables for passing to arguments
    std::map<int, CMCDHeaders *> mCMCDStreamData_Test;
    bool bCMCDEnabled_Test = true;
    std::string traceId_Test = " ";
    bool IsMuxed = true;
    bool bufferRedStatus = true;
    int bufferedDuration = 10;
    int currentBitrate = 10;

    // Act: Call the function for test
    collector->Initialize(bCMCDEnabled_Test,traceId_Test);
    mCMCDStreamData_Test.find(eMEDIATYPE_VIDEO);
    collector->SetTrackData(eMEDIATYPE_VIDEO,bufferRedStatus,bufferedDuration,currentBitrate,IsMuxed);

	CMCDHeaders_obj->SetMediaType("MUXED");
    std::string mediaType = CMCDHeaders_obj->GetMediaType();
	
    //Assert: Expecting values are equal or not
    EXPECT_STREQ(mediaType.c_str(),"MUXED");
    EXPECT_EQ(IsMuxed,true);
}

/**
 * TEST_F GTest-Testcase for SetTrackData function for AampCMCDCollectorTest file
**/
TEST_F(AampCMCDCollectorTest, SetTrackDataTest_4)
{ 
    // Arrange: Creating the variables for passing to arguments
    std::map<int, CMCDHeaders *> mCMCDStreamData_Test;
    bool bCMCDEnabled_Test = true;
    std::string traceId_Test = " ";
    bool IsMuxed = true;
    bool bufferRedStatus = true;
    int bufferedDuration = 10;
    int currentBitrate = 10;
    
    // Act: Call the function for test
    collector->Initialize(bCMCDEnabled_Test,traceId_Test);
    mCMCDStreamData_Test.find(eMEDIATYPE_INIT_VIDEO);
    collector->SetTrackData(eMEDIATYPE_INIT_VIDEO,bufferRedStatus,bufferedDuration,currentBitrate,IsMuxed);

	CMCDHeaders_obj->SetMediaType("MUXED");
    std::string mediaType = CMCDHeaders_obj->GetMediaType();
	
    //Assert: Expecting values are equal or not
    EXPECT_STREQ(mediaType.c_str(),"MUXED");
    EXPECT_EQ(IsMuxed,true);
}

/**
 * TEST_F GTest-Testcase for SetTrackData function for AampCMCDCollectorTest file
**/
TEST_F(AampCMCDCollectorTest, SetTrackDataTest_5)
{ 
    // Arrange: Creating the variables for passing to arguments
    std::map<int, CMCDHeaders *> mCMCDStreamData_Test;
    bool bCMCDEnabled_Test = true;
    std::string traceId_Test = " ";
    bool IsMuxed = false;
    bool bufferRedStatus = true;
    int bufferedDuration = 10;
    int currentBitrate = 10;
    
    // Act: Call the function for test
    collector->Initialize(bCMCDEnabled_Test,traceId_Test);
    mCMCDStreamData_Test.find(eMEDIATYPE_INIT_VIDEO);
    collector->SetTrackData(eMEDIATYPE_INIT_VIDEO,bufferRedStatus,bufferedDuration,currentBitrate,IsMuxed);

	CMCDHeaders_obj->SetMediaType("MUXED");
    std::string mediaType = CMCDHeaders_obj->GetMediaType();
	
    //Assert: Expecting two strings are equal or not
    EXPECT_STREQ(mediaType.c_str(),"MUXED");
    EXPECT_EQ(IsMuxed,false);
}

/**
 * TEST_F GTest-Testcase for SetTrackData function for AampCMCDCollectorTest file
**/
TEST_F(AampCMCDCollectorTest, SetTrackDataTest_6)
{ 
    // Arrange: Creating the variables for passing to arguments
    std::map<int, CMCDHeaders *> mCMCDStreamData_Test;
    bool bCMCDEnabled_Test = true;
    std::string traceId_Test = " ";
    bool IsMuxed = true;
    bool bufferRedStatus = true;
    int bufferedDuration = 10;
    int currentBitrate = 10;
    
    // Act: Call the function for test
    collector->Initialize(bCMCDEnabled_Test,traceId_Test);
    mCMCDStreamData_Test.find(eMEDIATYPE_AUDIO);
    collector->SetTrackData(eMEDIATYPE_AUDIO,bufferRedStatus,bufferedDuration,currentBitrate,IsMuxed);

	CMCDHeaders_obj->SetMediaType("MUXED");
    std::string mediaType = CMCDHeaders_obj->GetMediaType();
	
    //Assert: Expecting two strings are equal or not
    EXPECT_STREQ(mediaType.c_str(),"MUXED");
    EXPECT_EQ(IsMuxed,true);
}

/**
 * TEST_F GTest-Testcase for SetTrackData function for AampCMCDCollectorTest file
**/
TEST_F(AampCMCDCollectorTest, SetTrackDataTest_7)
{ 
    // Arrange: Creating the variables for passing to arguments
    std::map<int, CMCDHeaders *> mCMCDStreamData_Test;
    bool bCMCDEnabled_Test = true;
    std::string traceId_Test = " ";
    bool IsMuxed = true;
    bool bufferRedStatus = true;
    int bufferedDuration = 10;
    int currentBitrate = 10;

    // Act: Call the function for test
    collector->Initialize(bCMCDEnabled_Test,traceId_Test);
    mCMCDStreamData_Test.find(eMEDIATYPE_INIT_AUDIO);
    collector->SetTrackData(eMEDIATYPE_INIT_AUDIO,bufferRedStatus,bufferedDuration,currentBitrate,IsMuxed);

	CMCDHeaders_obj->SetMediaType("MUXED");
    std::string mediaType = CMCDHeaders_obj->GetMediaType();
	
    //Assert: Expecting two strings are equal or not
    EXPECT_STREQ(mediaType.c_str(),"MUXED");
    EXPECT_EQ(IsMuxed,true);
}

/**
 * TEST_F GTest-Testcase for CMCDSetNextRangeRequest function for AampCMCDCollectorTest file
**/
TEST_F(AampCMCDCollectorTest, CMCDSetNextRangeRequestTest_1)
{ 
    // Arrange: Creating the variables for passing to arguments
    std::map<int, CMCDHeaders *> mCMCDStreamData_Test;
    bool bCMCDEnabled_Test = true;
    long bandwidth = 0;
    std::string nextrange(" ");
    std::string CMCDNextRangeRequest(" ");

    // Act: Call the function for test
    collector->Initialize(bCMCDEnabled_Test,nextrange);
    mCMCDStreamData_Test.find(eMEDIATYPE_DEFAULT);
    collector->CMCDSetNextRangeRequest(nextrange,bandwidth,eMEDIATYPE_DEFAULT);

    CMCDHeaders_obj->SetNextRange(CMCDNextRangeRequest);

    //Assert: Expecting two strings are equal or not
    EXPECT_STREQ(CMCDNextRangeRequest.c_str(),nextrange.c_str());
}

/**
 * TEST_F GTest-Testcase for CMCDSetNextRangeRequest function for AampCMCDCollectorTest file
**/
TEST_F(AampCMCDCollectorTest, CMCDSetNextRangeRequestTest_2)
{ 
    // Arrange: Creating the variables for passing to arguments
    std::map<int, CMCDHeaders *> mCMCDStreamData_Test;
    bool bCMCDEnabled_Test = true;
    long bandwidth = 10;
    std::string nextrange(" ");
    std::string CMCDNextRangeRequest(" ");

    // Act: Call the function for test
    collector->Initialize(bCMCDEnabled_Test,nextrange);
    mCMCDStreamData_Test.find(eMEDIATYPE_DEFAULT);
    collector->CMCDSetNextRangeRequest(nextrange,bandwidth,eMEDIATYPE_DEFAULT);

    //Assert: Expecting two strings are equal or not
    EXPECT_STREQ(CMCDNextRangeRequest.c_str(),nextrange.c_str());
}

/**
 * TEST_F GTest-Testcase for CMCDSetNextRangeRequest function for AampCMCDCollectorTest file
**/
TEST_F(AampCMCDCollectorTest, CMCDSetNextRangeRequestTest_3)
{ 
    // Arrange: Creating the variables for passing to arguments
    std::map<int, CMCDHeaders *> mCMCDStreamData_Test;
    bool bCMCDEnabled_Test = true;
    long bandwidth = 10;
    std::string nextrange(" ");
    std::string CMCDNextRangeRequest(" ");

    // Act: Call the function for test
    collector->Initialize(bCMCDEnabled_Test,nextrange);
    mCMCDStreamData_Test.find(eMEDIATYPE_INIT_AUDIO);
    collector->CMCDSetNextRangeRequest(nextrange,bandwidth,eMEDIATYPE_INIT_AUDIO);

    //Assert: Expecting two strings are equal or not
    EXPECT_STREQ(CMCDNextRangeRequest.c_str(),nextrange.c_str());
}

/**
 * TEST_F GTest-Testcase for CMCDSetNextRangeRequest function for AampCMCDCollectorTest file
**/
TEST_F(AampCMCDCollectorTest, CMCDSetNextRangeRequestTest_4)
{ 
    // Arrange: Creating the variables for passing to arguments
    std::map<int, CMCDHeaders *> mCMCDStreamData_Test;
    bool bCMCDEnabled_Test = false;
    long bandwidth = 10;
    std::string nextrange("eMEDIATYPE_INIT_AUDIO");
    std::string CMCDNextRangeRequest("eMEDIATYPE_INIT_AUDIO");

    // Act: Call the function for test
    collector->Initialize(bCMCDEnabled_Test,nextrange);
    mCMCDStreamData_Test.find(eMEDIATYPE_INIT_AUDIO);
    collector->CMCDSetNextRangeRequest(nextrange,bandwidth,eMEDIATYPE_INIT_AUDIO);

    //Assert: Expecting two strings are equal or not
    EXPECT_STREQ(CMCDNextRangeRequest.c_str(),nextrange.c_str());
}
