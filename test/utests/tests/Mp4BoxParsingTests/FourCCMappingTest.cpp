/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2025 RDK Management
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
#include <vector>
#include <utility>
#include "MP4Demux.h"

class FourCCMappingTest : public ::testing::Test
{
    void SetUp() override
    {
        // Setup code if needed
    }

    void TearDown() override
    {
        // Teardown code if needed
    }
};

TEST_F(FourCCMappingTest, TestCodecMappings)
{
    std::vector< std::pair<uint32_t, GstStreamOutputFormat> > testCases = {
        { MultiChar_Constant("avcC"), GST_FORMAT_VIDEO_ES_H264 },
        { MultiChar_Constant("hvcC"), GST_FORMAT_VIDEO_ES_HEVC },
        { MultiChar_Constant("esds"), GST_FORMAT_AUDIO_ES_AAC_RAW },
        { MultiChar_Constant("dec3"), GST_FORMAT_AUDIO_ES_EC3 },
        { MultiChar_Constant("xvid"), GST_FORMAT_UNKNOWN } // Unknown FourCC
    };

    for (const auto& testCase : testCases)
    {
        GstStreamOutputFormat format = GetGstStreamOutputFormatFromFourCC(testCase.first);
        EXPECT_EQ(format, testCase.second) << "Failed for FourCC: " << FourCCToString(testCase.first);
    }
}

TEST_F(FourCCMappingTest, TestCipherTypeMappings)
{
    std::vector< std::pair<uint32_t, CipherType> > testCases = {
        { MultiChar_Constant("cenc"), CIPHER_TYPE_CENC },
        { MultiChar_Constant("cbcs"), CIPHER_TYPE_CBCS },
        { MultiChar_Constant("abcd"), CIPHER_TYPE_NONE } // Unknown FourCC
    };

    for (const auto& testCase : testCases)
    {
        CipherType cipher = GetCipherTypeFromFourCC(testCase.first);
        EXPECT_EQ(cipher, testCase.second) << "Failed for FourCC: " << FourCCToString(testCase.first);
    }
}