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
#include "AudioCMCDHeaders.h"

class AudioCMCDHeadersTest : public ::testing::Test
{
protected:
    AudioCMCDHeaders* audioCMCDHeader;
    class TestableAudioCMCDHeaders : public AudioCMCDHeaders
    {
    public:

        TestableAudioCMCDHeaders():AudioCMCDHeaders()
        {
        }

        void CallBuildCMCDCustomHeaders(std::unordered_map<std::string, std::vector<std::string>> &mCMCDCustomHeaders){
            TestableAudioCMCDHeaders::bufferStarvation = true;
            TestableAudioCMCDHeaders::mediaType = "INIT_AUDIO";
            TestableAudioCMCDHeaders::dnsLookUptime = 2;
            std::unordered_map<std::string, std::vector<std::string>> customHeaders;
            BuildCMCDCustomHeaders(customHeaders);
            TestableAudioCMCDHeaders::bufferStarvation = false;
            TestableAudioCMCDHeaders::dnsLookUptime = 0;
            TestableAudioCMCDHeaders::mediaType = "INIT_VIDEO";
            BuildCMCDCustomHeaders(customHeaders);
            TestableAudioCMCDHeaders::mNextRange = "test";
            BuildCMCDCustomHeaders(customHeaders);
        }
    };

    TestableAudioCMCDHeaders *audioCMCDHeaders;
    void SetUp() override
    {
        audioCMCDHeader = new AudioCMCDHeaders();
        audioCMCDHeaders = new TestableAudioCMCDHeaders();
    }

    void TearDown() override
    
    {
        delete audioCMCDHeaders;
        audioCMCDHeaders = nullptr;
        delete audioCMCDHeader;
        audioCMCDHeader = nullptr;
    }
};

// Test case to verify the behavior of BuildCMCDCustomHeaders
TEST_F(AudioCMCDHeadersTest, BuildCMCDCustomHeadersTest)
{
    // Arrange
    std::unordered_map<std::string, std::vector<std::string>> customHeaders;
    // Act
    audioCMCDHeaders->CallBuildCMCDCustomHeaders(customHeaders);
}
