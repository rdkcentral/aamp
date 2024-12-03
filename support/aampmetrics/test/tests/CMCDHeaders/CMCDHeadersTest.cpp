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
#include "CMCDHeaders.h"

class CMCDHeadersTest : public ::testing::Test {
protected:
 
    CMCDHeaders* cmcdHeaders;
    void SetUp() override {
        cmcdHeaders = new CMCDHeaders();
    }
    void TearDown() override {
        delete cmcdHeaders;
        cmcdHeaders = nullptr;
    }
};

TEST_F(CMCDHeadersTest, SessionIdTest) {
    // Set a session ID
    std::string sessionId = "12345";
    cmcdHeaders->SetSessionId(sessionId);
    EXPECT_EQ(cmcdHeaders->GetSessionId(), sessionId);
}

TEST_F(CMCDHeadersTest, MediaTypeTest) {
    // Set a media type
    std::string mediaType = "video";
    cmcdHeaders->SetMediaType(mediaType);
    EXPECT_EQ(cmcdHeaders->GetMediaType(), mediaType);
}

TEST_F(CMCDHeadersTest, BitrateTest) {
    // Set bitrate and top bitrate
    int bitrate = 5000;
    cmcdHeaders->SetBitrate(bitrate);
    int topBitrate = 8000;
    cmcdHeaders->SetTopBitrate(topBitrate);
}

TEST_F(CMCDHeadersTest, BufferTest) {
    // Set buffer length and starvation
    int bufferLength = 10;
    cmcdHeaders->SetBufferLength(bufferLength);
    bool bufferStarvation = true;
    cmcdHeaders->SetBufferStarvation(bufferStarvation);
}

TEST_F(CMCDHeadersTest, NetworkMetricsTest) {
    // Set network metrics
    int startTransferTime = 100;
    int totalTime = 500;
    int dnsLookUpTime = 50;
    cmcdHeaders->SetNetworkMetrics(startTransferTime, totalTime, dnsLookUpTime);
    int retrievedStartTransferTime, retrievedTotalTime, retrievedDnsLookUpTime;
    cmcdHeaders->GetNetworkMetrics(retrievedStartTransferTime, retrievedTotalTime, retrievedDnsLookUpTime);
    EXPECT_EQ(retrievedStartTransferTime, startTransferTime);
    EXPECT_EQ(retrievedTotalTime, totalTime);
    EXPECT_EQ(retrievedDnsLookUpTime, dnsLookUpTime);
}

TEST_F(CMCDHeadersTest, NextUrlTest) {
    // Set a next URL
    std::string nextUrl = "https://example.com";
    cmcdHeaders->SetNextUrl(nextUrl);
}

TEST_F(CMCDHeadersTest, CustomHeadersTest) {
    // Create a map for custom headers
    std::unordered_map<std::string, std::vector<std::string>> customHeaders;
    customHeaders["Header1"] = {"Value1", "Value2"};
    customHeaders["Header2"] = {"Value3"};
    cmcdHeaders->BuildCMCDCustomHeaders(customHeaders);
}

TEST_F(CMCDHeadersTest, NextRangeTest) {
    // Set a next range
    std::string nextRange = "bytes=100-200";
    cmcdHeaders->SetNextRange(nextRange);
}
