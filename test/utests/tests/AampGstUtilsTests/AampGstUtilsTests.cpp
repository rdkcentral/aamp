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
#include <gmock/gmock.h>
#include <chrono>

#include "AampGstUtils.h"
#include "MockAampGstUtils.h"

using ::testing::_;
using ::testing::Return;
using ::testing::StrEq;

AampConfig *gpGlobalConfig{nullptr};
AampLogManager *mLogObj{nullptr};

class AampGstUtilsTests : public ::testing::Test
{
protected:

    void SetUp() override
    {
        g_mockAampGstUtils = new MockAampGstUtils();
    }

    void TearDown() override
    {
        delete g_mockAampGstUtils;
        g_mockAampGstUtils = nullptr;
    }

public:

};

TEST_F(AampGstUtilsTests, esMP3test)
{
    GstCaps dummycaps;
    GstCaps *caps{&dummycaps};
    EXPECT_CALL(*g_mockAampGstUtils,gst_caps_new_simple(StrEq("audio/mpeg"),StrEq("mpegversion"), G_TYPE_INT, 1, NULL)).WillOnce(Return(caps));

    EXPECT_TRUE(GetGstCaps(FORMAT_AUDIO_ES_MP3)==caps);
}

TEST_F(AampGstUtilsTests, GstCapsFormatsTest)
{
    GstCaps dummycapslist;
    GstCaps *caps{&dummycapslist};
    StreamOutputFormat GstCapsFormats[16] = {
	FORMAT_MPEGTS,
	FORMAT_ISO_BMFF,
	FORMAT_AUDIO_ES_MP3,
	FORMAT_AUDIO_ES_AAC,
	FORMAT_AUDIO_ES_AC3,
	FORMAT_AUDIO_ES_AC4,
	FORMAT_SUBTITLE_TTML,
	FORMAT_SUBTITLE_WEBVTT,
	FORMAT_SUBTITLE_MP4,
	FORMAT_AUDIO_ES_ATMOS,
	FORMAT_AUDIO_ES_EC3,
	FORMAT_VIDEO_ES_H264,
	FORMAT_VIDEO_ES_HEVC,
	FORMAT_VIDEO_ES_MPEG2,
	FORMAT_UNKNOWN,
	FORMAT_INVALID
    };

    for(int i=0;i<16;i++){
    GetGstCaps(GstCapsFormats[i]);
    ASSERT_FALSE(GetGstCaps(GstCapsFormats[i]));
    }
}