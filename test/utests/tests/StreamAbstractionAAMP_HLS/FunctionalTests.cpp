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

#include "priv_aamp.h"

#include "AampConfig.h"
#include "AampScheduler.h"
#include "AampLogManager.h"
#include "fragmentcollector_hls.h"
#include "MockAampConfig.h"
#include "MockAampGstPlayer.h"
#include "MockAampScheduler.h"

using ::testing::_;
using ::testing::WithParamInterface;
using ::testing::An;
using ::testing::DoAll;
using ::testing::SetArgReferee;
using ::testing::Invoke;
using ::testing::Return;

AampConfig *gpGlobalConfig{nullptr};
AampLogManager *mLogObj{nullptr};

#define MANIFEST_6SD_1A \
        "#EXTM3U\n" \
        "#EXT-X-VERSION:5\n" \
        "\n" \
        "#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID=\"audio\",NAME=\"Englishstereo\",LANGUAGE=\"en\",AUTOSELECT=YES,URI=\"audio_1_stereo_128000.m3u8\"\n" \
        "\n" \
        "#EXT-X-STREAM-INF:BANDWIDTH=628000,CODECS=\"avc1.42c00d,mp4a.40.2\",RESOLUTION=320x180,AUDIO=\"audio\"\n" \
        "video_180_250000.m3u8\n" \
        "#EXT-X-STREAM-INF:BANDWIDTH=928000,CODECS=\"avc1.42c00d,mp4a.40.2\",RESOLUTION=480x270,AUDIO=\"audio\"\n" \
        "video_270_400000.m3u8\n" \
        "#EXT-X-STREAM-INF:BANDWIDTH=1728000,CODECS=\"avc1.42c00d,mp4a.40.2\",RESOLUTION=640x360,AUDIO=\"audio\"\n" \
        "video_360_800000.m3u8\n" \
        "#EXT-X-STREAM-INF:BANDWIDTH=2528000,CODECS=\"avc1.42c00d,mp4a.40.2\",RESOLUTION=960x540,AUDIO=\"audio\"\n" \
        "video_540_1200000.m3u8\n" \
        "#EXT-X-STREAM-INF:BANDWIDTH=4928000,CODECS=\"avc1.42c00d,mp4a.40.2\",RESOLUTION=1280x720,AUDIO=\"audio\"\n" \
        "video_720_2400000.m3u8\n" \
        "#EXT-X-STREAM-INF:BANDWIDTH=9728000,CODECS=\"avc1.42c00d,mp4a.40.2\",RESOLUTION=1920x1080,AUDIO=\"audio\"\n" \
        "video_1080_4800000.m3u8\n"

#define MANIFEST_5SD_1A \
        "#EXTM3U\n" \
        "#EXT-X-VERSION:5\n" \
        "\n" \
        "#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID=\"audio\",NAME=\"Englishstereo\",LANGUAGE=\"en\",AUTOSELECT=YES,URI=\"audio_1_stereo_128000.m3u8\"\n" \
        "\n" \
        "#EXT-X-STREAM-INF:BANDWIDTH=628000,CODECS=\"avc1.42c00d,mp4a.40.2\",RESOLUTION=320x180,AUDIO=\"audio\"\n" \
        "video_180_250000.m3u8\n" \
        "#EXT-X-STREAM-INF:BANDWIDTH=928000,CODECS=\"avc1.42c00d,mp4a.40.2\",RESOLUTION=480x270,AUDIO=\"audio\"\n" \
        "video_270_400000.m3u8\n" \
        "#EXT-X-STREAM-INF:BANDWIDTH=1728000,CODECS=\"avc1.42c00d,mp4a.40.2\",RESOLUTION=640x360,AUDIO=\"audio\"\n" \
        "video_360_800000.m3u8\n" \
        "#EXT-X-STREAM-INF:BANDWIDTH=2528000,CODECS=\"avc1.42c00d,mp4a.40.2\",RESOLUTION=960x540,AUDIO=\"audio\"\n" \
        "video_540_1200000.m3u8\n" \
        "#EXT-X-STREAM-INF:BANDWIDTH=4928000,CODECS=\"avc1.42c00d,mp4a.40.2\",RESOLUTION=1280x720,AUDIO=\"audio\"\n" \
        "video_720_2400000.m3u8\n"

#define MANIFEST_5SD_4K_1A \
        "#EXTM3U\n" \
        "#EXT-X-VERSION:5\n" \
        "\n" \
        "#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID=\"audio\",NAME=\"Englishstereo\",LANGUAGE=\"en\",AUTOSELECT=YES,URI=\"audio_1_stereo_128000.m3u8\"\n" \
        "\n" \
        "#EXT-X-STREAM-INF:BANDWIDTH=628000,CODECS=\"avc1.42c00d,mp4a.40.2\",RESOLUTION=320x180,AUDIO=\"audio\"\n" \
        "video_180_250000.m3u8\n" \
        "#EXT-X-STREAM-INF:BANDWIDTH=928000,CODECS=\"avc1.42c00d,mp4a.40.2\",RESOLUTION=480x270,AUDIO=\"audio\"\n" \
        "video_270_400000.m3u8\n" \
        "#EXT-X-STREAM-INF:BANDWIDTH=1728000,CODECS=\"avc1.42c00d,mp4a.40.2\",RESOLUTION=640x360,AUDIO=\"audio\"\n" \
        "video_360_800000.m3u8\n" \
        "#EXT-X-STREAM-INF:BANDWIDTH=2528000,CODECS=\"avc1.42c00d,mp4a.40.2\",RESOLUTION=960x540,AUDIO=\"audio\"\n" \
        "video_540_1200000.m3u8\n" \
        "#EXT-X-STREAM-INF:BANDWIDTH=4928000,CODECS=\"avc1.42c00d,mp4a.40.2\",RESOLUTION=1280x720,AUDIO=\"audio\"\n" \
        "video_720_2400000.m3u8\n" \
        "#EXT-X-STREAM-INF:BANDWIDTH=9728000,CODECS=\"avc1.42c00d,mp4a.40.2\",RESOLUTION=3840x2160,AUDIO=\"audio\"\n" \
        "video_1080_4800000.m3u8\n"

class FunctionalTests : public ::testing::Test
{
protected:
    PrivateInstanceAAMP *mPrivateInstanceAAMP;
    StreamAbstractionAAMP_HLS *mStreamAbstractionAAMP_HLS;

    void SetUp() override
    {
        if(gpGlobalConfig == nullptr)
        {
            gpGlobalConfig =  new AampConfig();
        }

        mPrivateInstanceAAMP = new PrivateInstanceAAMP(gpGlobalConfig);

        g_mockAampConfig = new MockAampConfig();

        mStreamAbstractionAAMP_HLS = new StreamAbstractionAAMP_HLS(mLogObj, mPrivateInstanceAAMP, 0, AAMP_NORMAL_PLAY_RATE);

        // Called in destructor of PrivateInstanceAAMP
        // Done here because setting up the EXPECT_CALL in TearDown, conflicted with the mock
        // being called in the PausePosition thread.
        EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_EnableCurlStore)).WillRepeatedly(Return(false));
    }

    void TearDown() override
    {
        delete mPrivateInstanceAAMP;
        mPrivateInstanceAAMP = nullptr;

        delete mStreamAbstractionAAMP_HLS;
        mStreamAbstractionAAMP_HLS = nullptr;

        delete gpGlobalConfig;
        gpGlobalConfig = nullptr;

        delete g_mockAampConfig;
        g_mockAampConfig = nullptr;
    }

public:

};

// Testing simple good case where no 4K streams are present.
TEST_F(FunctionalTests, StreamAbstractionAAMP_HLS_Is4KStream_no_4k)
{
    int height;
    long bandwidth;
    char manifest[] = MANIFEST_6SD_1A;

    mStreamAbstractionAAMP_HLS->mainManifest.ptr = manifest;

    EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_AvgBWForABR)).WillOnce(Return(true));

    mStreamAbstractionAAMP_HLS->ParseMainManifest();

    EXPECT_EQ(mStreamAbstractionAAMP_HLS->streamInfoStore.size(), 6);
    EXPECT_EQ(mStreamAbstractionAAMP_HLS->mediaInfoStore.size(), 1);
    EXPECT_EQ(mStreamAbstractionAAMP_HLS->Is4KStream(height, bandwidth), false);
}

// Testing simple good case where a 4K stream is present.
TEST_F(FunctionalTests, StreamAbstractionAAMP_HLS_Is4KStream_4k)
{
    int height;
    long bandwidth;
    char manifest[] = MANIFEST_5SD_4K_1A;

    mStreamAbstractionAAMP_HLS->mainManifest.ptr = manifest;

    EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_AvgBWForABR)).WillOnce(Return(true));

    mStreamAbstractionAAMP_HLS->ParseMainManifest();

    EXPECT_EQ(mStreamAbstractionAAMP_HLS->streamInfoStore.size(), 6);
    EXPECT_EQ(mStreamAbstractionAAMP_HLS->mediaInfoStore.size(), 1);
    EXPECT_EQ(mStreamAbstractionAAMP_HLS->Is4KStream(height, bandwidth), true);
}

// Testing media / srteam stores are correct size after parsing a new manifest
TEST_F(FunctionalTests, StreamAbstractionAAMP_HLS_Is4KStream_multiple_mainfests)
{
    int height;
    long bandwidth;
    std::string manifests[] = {
        MANIFEST_6SD_1A,
        MANIFEST_5SD_1A,
        MANIFEST_5SD_4K_1A,
        MANIFEST_5SD_1A
    };
    struct {
        const char *manifest;
        int exp_media;
        int exp_streams;
        bool exp_4k;
    } test_data[] = {
        {manifests[0].c_str(), 1, 6, false},
        {manifests[1].c_str(), 1, 5, false},
        {manifests[2].c_str(), 1, 6, true},
        {manifests[3].c_str(), 1, 5, false},
    };

    for (auto& td : test_data)
    {
        // Note: ParseMainManifest alters the manifest in situ, replacing some \n with \0
        //       Not ideal, but safe to convert from const char.
        mStreamAbstractionAAMP_HLS->mainManifest.ptr = (char *)td.manifest;

        EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_AvgBWForABR)).WillOnce(Return(true));

        mStreamAbstractionAAMP_HLS->ParseMainManifest();

        EXPECT_EQ(mStreamAbstractionAAMP_HLS->streamInfoStore.size(), td.exp_streams);
        EXPECT_EQ(mStreamAbstractionAAMP_HLS->mediaInfoStore.size(), td.exp_media);
        EXPECT_EQ(mStreamAbstractionAAMP_HLS->Is4KStream(height, bandwidth), td.exp_4k);
    }
}

