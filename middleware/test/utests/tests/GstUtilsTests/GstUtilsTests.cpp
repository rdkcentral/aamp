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

#include "middleware/GstUtils.h"
#include "MockGStreamer.h"

#include "main_aamp.h"



using ::testing::_;
using ::testing::Return;
using ::testing::StrEq;

AampConfig *gpGlobalConfig{nullptr};

class GstUtilsTests : public ::testing::Test
{
protected:

    void SetUp() override
    {
        g_mockGStreamer = new MockGStreamer();
    }

    void TearDown() override
    {
        delete g_mockGStreamer;
        g_mockGStreamer = nullptr;
    }

public:

};

TEST_F(GstUtilsTests, esMP3test)
{
    GstCaps dummycaps;
    GstCaps *caps{&dummycaps};
    EXPECT_CALL(*g_mockGStreamer,gst_caps_new_simple(StrEq("audio/mpeg"),StrEq("mpegversion"), G_TYPE_INT, 1, NULL)).WillOnce(Return(caps));

    EXPECT_TRUE(GetCaps(GST_FORMAT_AUDIO_ES_MP3)==caps);
}

TEST_F(GstUtilsTests, GstCapsFormatsTest)
{
    GstCaps dummycapslist;
    GstCaps *caps{&dummycapslist};
    GstStreamOutputFormat GstCapsFormats[16] = {
    GST_FORMAT_INVALID,         /**< Invalid format */
    GST_FORMAT_MPEGTS,          /**< MPEG Transport Stream */
    GST_FORMAT_ISO_BMFF,        /**< ISO Base Media File format */
    GST_FORMAT_AUDIO_ES_MP3,    /**< MP3 Audio Elementary Stream */
    GST_FORMAT_AUDIO_ES_AAC,    /**< AAC Audio Elementary Stream */
    GST_FORMAT_AUDIO_ES_AC3,    /**< AC3 Audio Elementary Stream */
    GST_FORMAT_AUDIO_ES_EC3,    /**< Dolby Digital Plus Elementary Stream */
    GST_FORMAT_AUDIO_ES_ATMOS,  /**< ATMOS Audio stream */
    GST_FORMAT_AUDIO_ES_AC4,    /**< AC4 Dolby Audio stream */
    GST_FORMAT_VIDEO_ES_H264,   /**< MPEG-4 Video Elementary Stream */
    GST_FORMAT_VIDEO_ES_HEVC,   /**< HEVC video elementary stream */
    GST_FORMAT_VIDEO_ES_MPEG2,  /**< MPEG-2 Video Elementary Stream */
    GST_FORMAT_SUBTITLE_WEBVTT, /**< WebVTT subtitle Stream */
    GST_FORMAT_SUBTITLE_TTML,   /**< WebVTT subtitle Stream */
    GST_FORMAT_SUBTITLE_MP4,    /**< Generic MP4 stream */
    GST_FORMAT_UNKNOWN          /**< Unknown Format */
    };

    for(int i=0;i<16;i++){
    GetCaps(GstCapsFormats[i]);
    ASSERT_FALSE(GetCaps(GstCapsFormats[i]));
    }
}
