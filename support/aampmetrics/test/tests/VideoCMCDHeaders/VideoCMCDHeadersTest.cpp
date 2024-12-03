/*
* If not stated otherwise in this file or this component's license file the
* following copyright and licenses apply:
*
* Copyright 2024 RDK Management
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
#include "VideoCMCDHeaders.h"

class VideoCMCDHeadersTest : public ::testing::Test
{
protected:
    
    class TestableVideoCMCDHeaders : public VideoCMCDHeaders
    {
    public:

        TestableVideoCMCDHeaders():VideoCMCDHeaders()
        {
        }

        void CallBuildCMCDCustomHeaders(std::unordered_map<std::string, std::vector<std::string>> &mCMCDCustomHeaders){
            TestableVideoCMCDHeaders::bufferStarvation = true;
            TestableVideoCMCDHeaders::mediaType = "INIT_AUDIO";
            TestableVideoCMCDHeaders::dnsLookUptime = 2;
            std::unordered_map<std::string, std::vector<std::string>> customHeaders;
            BuildCMCDCustomHeaders(customHeaders);
            TestableVideoCMCDHeaders::bufferStarvation = false;
            TestableVideoCMCDHeaders::dnsLookUptime = 0;
            TestableVideoCMCDHeaders::mediaType = "INIT_VIDEO";
            BuildCMCDCustomHeaders(customHeaders);
            TestableVideoCMCDHeaders::mNextRange = "test";
            BuildCMCDCustomHeaders(customHeaders);
        }
    };

    TestableVideoCMCDHeaders *videoCMCDHeaders;
    void SetUp() override
    {
       
        videoCMCDHeaders = new TestableVideoCMCDHeaders();
    }

    void TearDown() override
    
    {
        delete videoCMCDHeaders;
        videoCMCDHeaders = nullptr;
        
    }
};

// Test case to verify the behavior of BuildCMCDCustomHeaders
TEST_F(VideoCMCDHeadersTest, BuildCMCDCustomHeadersTest)
{
    // Arrange
    std::unordered_map<std::string, std::vector<std::string>> customHeaders;
    // Act
    videoCMCDHeaders->CallBuildCMCDCustomHeaders(customHeaders);
}

