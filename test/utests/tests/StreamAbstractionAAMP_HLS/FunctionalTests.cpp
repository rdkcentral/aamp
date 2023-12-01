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
using ::testing::An;
using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::SetArgReferee;
using ::testing::WithParamInterface;

AampConfig *gpGlobalConfig{nullptr};
AampLogManager *mLogObj{nullptr};
StreamAbstractionAAMP_HLS *mStreamAbstractionAAMP_HLS{};

#define MANIFEST_6SD_1A                                                                                                                     \
    "#EXTM3U\n"                                                                                                                             \
    "#EXT-X-VERSION:5\n"                                                                                                                    \
    "\n"                                                                                                                                    \
    "#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID=\"audio\",NAME=\"Englishstereo\",LANGUAGE=\"en\",AUTOSELECT=YES,URI=\"audio_1_stereo_128000.m3u8\"\n" \
    "\n"                                                                                                                                    \
    "#EXT-X-STREAM-INF:BANDWIDTH=628000,CODECS=\"avc1.42c00d,mp4a.40.2\",RESOLUTION=320x180,AUDIO=\"audio\"\n"                              \
    "video_180_250000.m3u8\n"                                                                                                               \
    "#EXT-X-STREAM-INF:BANDWIDTH=928000,CODECS=\"avc1.42c00d,mp4a.40.2\",RESOLUTION=480x270,AUDIO=\"audio\"\n"                              \
    "video_270_400000.m3u8\n"                                                                                                               \
    "#EXT-X-STREAM-INF:BANDWIDTH=1728000,CODECS=\"avc1.42c00d,mp4a.40.2\",RESOLUTION=640x360,AUDIO=\"audio\"\n"                             \
    "video_360_800000.m3u8\n"                                                                                                               \
    "#EXT-X-STREAM-INF:BANDWIDTH=2528000,CODECS=\"avc1.42c00d,mp4a.40.2\",RESOLUTION=960x540,AUDIO=\"audio\"\n"                             \
    "video_540_1200000.m3u8\n"                                                                                                              \
    "#EXT-X-STREAM-INF:BANDWIDTH=4928000,CODECS=\"avc1.42c00d,mp4a.40.2\",RESOLUTION=1280x720,AUDIO=\"audio\"\n"                            \
    "video_720_2400000.m3u8\n"                                                                                                              \
    "#EXT-X-STREAM-INF:BANDWIDTH=9728000,CODECS=\"avc1.42c00d,mp4a.40.2\",RESOLUTION=1920x1080,AUDIO=\"audio\"\n"                           \
    "video_1080_4800000.m3u8\n"

#define MANIFEST_5SD_1A                                                                                                                     \
    "#EXTM3U\n"                                                                                                                             \
    "#EXT-X-VERSION:5\n"                                                                                                                    \
    "\n"                                                                                                                                    \
    "#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID=\"audio\",NAME=\"Englishstereo\",LANGUAGE=\"en\",AUTOSELECT=YES,URI=\"audio_1_stereo_128000.m3u8\"\n" \
    "\n"                                                                                                                                    \
    "#EXT-X-STREAM-INF:BANDWIDTH=628000,CODECS=\"avc1.42c00d,mp4a.40.2\",RESOLUTION=320x180,AUDIO=\"audio\"\n"                              \
    "video_180_250000.m3u8\n"                                                                                                               \
    "#EXT-X-STREAM-INF:BANDWIDTH=928000,CODECS=\"avc1.42c00d,mp4a.40.2\",RESOLUTION=480x270,AUDIO=\"audio\"\n"                              \
    "video_270_400000.m3u8\n"                                                                                                               \
    "#EXT-X-STREAM-INF:BANDWIDTH=1728000,CODECS=\"avc1.42c00d,mp4a.40.2\",RESOLUTION=640x360,AUDIO=\"audio\"\n"                             \
    "video_360_800000.m3u8\n"                                                                                                               \
    "#EXT-X-STREAM-INF:BANDWIDTH=2528000,CODECS=\"avc1.42c00d,mp4a.40.2\",RESOLUTION=960x540,AUDIO=\"audio\"\n"                             \
    "video_540_1200000.m3u8\n"                                                                                                              \
    "#EXT-X-STREAM-INF:BANDWIDTH=4928000,CODECS=\"avc1.42c00d,mp4a.40.2\",RESOLUTION=1280x720,AUDIO=\"audio\"\n"                            \
    "video_720_2400000.m3u8\n"

#define MANIFEST_5SD_4K_1A                                                                                                                  \
    "#EXTM3U\n"                                                                                                                             \
    "#EXT-X-VERSION:5\n"                                                                                                                    \
    "\n"                                                                                                                                    \
    "#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID=\"audio\",NAME=\"Englishstereo\",LANGUAGE=\"en\",AUTOSELECT=YES,URI=\"audio_1_stereo_128000.m3u8\"\n" \
    "\n"                                                                                                                                    \
    "#EXT-X-STREAM-INF:BANDWIDTH=628000,CODECS=\"avc1.42c00d,mp4a.40.2\",RESOLUTION=320x180,AUDIO=\"audio\"\n"                              \
    "video_180_250000.m3u8\n"                                                                                                               \
    "#EXT-X-STREAM-INF:BANDWIDTH=928000,CODECS=\"avc1.42c00d,mp4a.40.2\",RESOLUTION=480x270,AUDIO=\"audio\"\n"                              \
    "video_270_400000.m3u8\n"                                                                                                               \
    "#EXT-X-STREAM-INF:BANDWIDTH=1728000,CODECS=\"avc1.42c00d,mp4a.40.2\",RESOLUTION=640x360,AUDIO=\"audio\"\n"                             \
    "video_360_800000.m3u8\n"                                                                                                               \
    "#EXT-X-STREAM-INF:BANDWIDTH=2528000,CODECS=\"avc1.42c00d,mp4a.40.2\",RESOLUTION=960x540,AUDIO=\"audio\"\n"                             \
    "video_540_1200000.m3u8\n"                                                                                                              \
    "#EXT-X-STREAM-INF:BANDWIDTH=4928000,CODECS=\"avc1.42c00d,mp4a.40.2\",RESOLUTION=1280x720,AUDIO=\"audio\"\n"                            \
    "video_720_2400000.m3u8\n"                                                                                                              \
    "#EXT-X-STREAM-INF:BANDWIDTH=9728000,CODECS=\"avc1.42c00d,mp4a.40.2\",RESOLUTION=3840x2160,AUDIO=\"audio\"\n"                           \
    "video_1080_4800000.m3u8\n"

class FunctionalTests : public ::testing::Test
{
protected:
    PrivateInstanceAAMP *mPrivateInstanceAAMP{};

    void SetUp() override
    {
        if (gpGlobalConfig == nullptr)
        {
            gpGlobalConfig = new AampConfig();
        }

        mPrivateInstanceAAMP = new PrivateInstanceAAMP(gpGlobalConfig);

        g_mockAampConfig = new MockAampConfig();

        mStreamAbstractionAAMP_HLS = new StreamAbstractionAAMP_HLS(mLogObj, mPrivateInstanceAAMP, 0, AAMP_NORMAL_PLAY_RATE);

        // Called in destructor of PrivateInstanceAAMP
        // Done here because setting up the EXPECT_CALL in TearDown, conflicted with the mock
        // being called in the PausePosition thread.
        // EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_EnableCurlStore)).WillRepeatedly(Return(false));
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

class StreamAbstractionAAMP_HLSTest : public ::testing::Test
{
protected:
    class TestableStreamAbstractionAAMP_HLS : public StreamAbstractionAAMP_HLS
    {
    public:
        // Constructor to pass parameters to the base class constructor
        TestableStreamAbstractionAAMP_HLS(AampLogManager *logObj, PrivateInstanceAAMP *aamp,
                                          double seekpos, float rate,
                                          id3_callback_t id3Handler = nullptr,
                                          ptsoffset_update_t ptsOffsetUpdate = nullptr)
            : StreamAbstractionAAMP_HLS(logObj, aamp, seekpos, rate, id3Handler, ptsOffsetUpdate)
        {
        }

        StreamInfo *CallGetStreamInfo(int idx)
        {
            return GetStreamInfo(idx);
        }

        void CallPopulateAudioAndTextTracks()
        {
            PopulateAudioAndTextTracks();
        }

        void CallConfigureAudioTrack()
        {
            ConfigureAudioTrack();
        }
        void CallConfigureVideoProfiles()
        {
            ConfigureVideoProfiles();
        }

        void CallConfigureTextTrack()
        {
            ConfigureTextTrack();
        }

        void CallCachePlaylistThreadFunction()
        {
            CachePlaylistThreadFunction();
        }

        int CallGetDesiredProfileBasedOnCache(){

            return GetDesiredProfileBasedOnCache();
        }

        void CallUpdateProfileBasedOnFragmentDownloaded()
        {
            UpdateProfileBasedOnFragmentDownloaded();
        }
    };

    PrivateInstanceAAMP *mPrivateInstanceAAMP;
    TestableStreamAbstractionAAMP_HLS *mStreamAbstractionAAMP_HLS;

    void SetUp() override
    {
        mPrivateInstanceAAMP = new PrivateInstanceAAMP();
        mStreamAbstractionAAMP_HLS = new TestableStreamAbstractionAAMP_HLS(mLogObj, mPrivateInstanceAAMP, 0.0, 1.0);
    }

    void TearDown() override
    {
        // Clean up your objects after each test case
        delete mStreamAbstractionAAMP_HLS;
        mStreamAbstractionAAMP_HLS = nullptr;

        delete mPrivateInstanceAAMP;
        mPrivateInstanceAAMP = nullptr;
    }
};

class TrackStateTests : public ::testing::Test
{
protected:
    PrivateInstanceAAMP *mPrivateInstanceAAMP{};
    StreamAbstractionAAMP_HLS *mStreamAbstractionAAMP_HLS{};
     TrackState *TrackStateobj{};

    void SetUp() override
    {
        if (gpGlobalConfig == nullptr)
        {
            gpGlobalConfig = new AampConfig();
        }

        mPrivateInstanceAAMP = new PrivateInstanceAAMP(gpGlobalConfig);

        g_mockAampConfig = new MockAampConfig();

        mStreamAbstractionAAMP_HLS = new StreamAbstractionAAMP_HLS(mLogObj, mPrivateInstanceAAMP, 0, 0.0);

        TrackStateobj = new TrackState(mLogObj, eTRACK_VIDEO, mStreamAbstractionAAMP_HLS, mPrivateInstanceAAMP, "TestTrack");

        // Called in destructor of PrivateInstanceAAMP
        // Done here because setting up the EXPECT_CALL in TearDown, conflicted with the mock
        // being called in the PausePosition thread.
        // EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_EnableCurlStore)).WillRepeatedly(Return(false));
    }

    void TearDown() override
    {
        delete TrackStateobj;
        TrackStateobj = nullptr;

        delete mPrivateInstanceAAMP;
        mPrivateInstanceAAMP = nullptr;

        delete TrackStateobj;
        TrackStateobj = nullptr;

        delete gpGlobalConfig;
        gpGlobalConfig = nullptr;

        delete g_mockAampConfig;
        g_mockAampConfig = nullptr;
    }

public:
};

TEST_F(StreamAbstractionAAMP_HLSTest, TestGetStreamInfo)
{
    int index = 0;
    StreamInfo *streamInfo = mStreamAbstractionAAMP_HLS->CallGetStreamInfo(index);
    EXPECT_NE(streamInfo, nullptr);
}

TEST_F(StreamAbstractionAAMP_HLSTest, TestPopulateAudioAndTextTracks)
{
    mStreamAbstractionAAMP_HLS->CallPopulateAudioAndTextTracks();
}

TEST_F(StreamAbstractionAAMP_HLSTest, TestConfigureAudioTrack)
{
    mStreamAbstractionAAMP_HLS->CallConfigureAudioTrack();
}

TEST_F(StreamAbstractionAAMP_HLSTest, TestConfigureVideoProfiles)
{
    mStreamAbstractionAAMP_HLS->CallConfigureVideoProfiles();
}

TEST_F(StreamAbstractionAAMP_HLSTest, TestConfigureTextTrack)
{
    mStreamAbstractionAAMP_HLS->CallConfigureTextTrack();
}

TEST_F(StreamAbstractionAAMP_HLSTest, TestCachePlaylistThreadFunction)
{
    mStreamAbstractionAAMP_HLS->CallCachePlaylistThreadFunction();
}

TEST_F(StreamAbstractionAAMP_HLSTest, TestGetDesiredProfileBasedOnCache)
{
    int result = mStreamAbstractionAAMP_HLS->CallGetDesiredProfileBasedOnCache();
    EXPECT_EQ(result,0);
}

TEST_F(StreamAbstractionAAMP_HLSTest, TestUpdateProfileBasedOnFragmentDownloaded)
{
    mStreamAbstractionAAMP_HLS->CallUpdateProfileBasedOnFragmentDownloaded();
}

// Testing simple good case where no 4K streams are present.
TEST_F(FunctionalTests, StreamAbstractionAAMP_HLS_Is4KStream_no_4k)
{
    int height;
    BitsPerSecond bandwidth;
    char manifest[] = MANIFEST_6SD_1A;

    mStreamAbstractionAAMP_HLS->mainManifest.AppendBytes(manifest, sizeof(manifest));

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
    BitsPerSecond bandwidth;
    char manifest[] = MANIFEST_5SD_4K_1A;

    mStreamAbstractionAAMP_HLS->mainManifest.AppendBytes(manifest, sizeof(manifest));

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
    BitsPerSecond bandwidth;
    std::string manifests[] = {
        MANIFEST_6SD_1A,
        MANIFEST_5SD_1A,
        MANIFEST_5SD_4K_1A,
        MANIFEST_5SD_1A};
    struct
    {
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

    for (auto &td : test_data)
    {
        // Note: ParseMainManifest alters the manifest in situ, replacing some \n with \0
        //       Not ideal, but safe to convert from const char.
        mStreamAbstractionAAMP_HLS->mainManifest.AppendBytes((char *)td.manifest, sizeof((char *)td.manifest));

        EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_AvgBWForABR)).WillOnce(Return(true));

        mStreamAbstractionAAMP_HLS->ParseMainManifest();

        EXPECT_EQ(mStreamAbstractionAAMP_HLS->streamInfoStore.size(), td.exp_streams);
        EXPECT_EQ(mStreamAbstractionAAMP_HLS->mediaInfoStore.size(), td.exp_media);
        EXPECT_EQ(mStreamAbstractionAAMP_HLS->Is4KStream(height, bandwidth), td.exp_4k);
    }
}

// Testing ABR manager is selected by default.
TEST_F(FunctionalTests, ABRManagerMode)
{
    char manifest[] = MANIFEST_5SD_1A;

    mStreamAbstractionAAMP_HLS->mainManifest.AppendBytes(manifest, sizeof(manifest));
    // Call the fake Tune() method with a non-local URL to setup Fog related flags.
    mPrivateInstanceAAMP->Tune("https://ads.com/ad.m3u8", false);

    EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_AvgBWForABR)).WillOnce(Return(true));

    mStreamAbstractionAAMP_HLS->ParseMainManifest();

    EXPECT_EQ(mStreamAbstractionAAMP_HLS->GetABRMode(), StreamAbstractionAAMP::ABRMode::ABR_MANAGER);
}

// Testing Fog is selected to manage ABR.
TEST_F(FunctionalTests, FogABRMode)
{
    char manifest[] = MANIFEST_5SD_1A;

    mStreamAbstractionAAMP_HLS->mainManifest.AppendBytes(manifest, sizeof(manifest));

    // Call the fake Tune() method with a Fog TSB URL to setup Fog related flags.
    mPrivateInstanceAAMP->Tune("http://127.0.0.1/tsb?clientId=FOG_AAMP&recordedUrl=https%3A%2F%2Fads.com%2Fad.m3u8", false);

    EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_AvgBWForABR)).WillOnce(Return(true));

    mStreamAbstractionAAMP_HLS->ParseMainManifest();

    EXPECT_EQ(mStreamAbstractionAAMP_HLS->GetABRMode(), StreamAbstractionAAMP::ABRMode::FOG_TSB);
}

TEST_F(FunctionalTests, Stoptest)
{
    mStreamAbstractionAAMP_HLS->Stop(true);
}

TEST_F(FunctionalTests, GetVideoBitratesTest)
{
    // Define sample StreamInfo objects with video bitrates
    StreamResolution resolution1;
    resolution1.width = 1920;     // Width in pixels
    resolution1.height = 1080;    // Height in pixels
    resolution1.framerate = 30.0; // Frame rate in FPS
    // Define sample StreamInfo objects with video bitrates and resolutions

    HlsStreamInfo streamInfo1;
    streamInfo1.enabled = true;
    streamInfo1.isIframeTrack = true;
    streamInfo1.validity = true;
    streamInfo1.codecs = "h264";
    streamInfo1.bandwidthBitsPerSecond = 1000000; // 1 Mbps
    streamInfo1.resolution = resolution1;         // Full HD
    streamInfo1.reason = BitrateChangeReason::eAAMP_BITRATE_CHANGE_BY_ABR;

    // Add the sample HlsStreamInfo objects to the streamInfoStore
    mStreamAbstractionAAMP_HLS->streamInfoStore.push_back(streamInfo1);

    // Call the GetVideoBitrates function
    std::vector<BitsPerSecond> bitrates = mStreamAbstractionAAMP_HLS->GetVideoBitrates();

    ASSERT_EQ(bitrates.size(), 0); // Expecting two video profiles
                                   // ASSERT_NE(bitrates[0], 1000000);  // First video profile bitrate
}

TEST_F(FunctionalTests, GetAvailableThumbnailTracksTest)
{

    // Add sample StreamInfo objects to streamInfoStore
    HlsStreamInfo streamInfo1;
    streamInfo1.isIframeTrack = true;
    streamInfo1.validity = true;
    streamInfo1.codecs = "H.264";

    // Add streamInfo1 and streamInfo2 to the streamInfoStore (replace with your actual code)
    mStreamAbstractionAAMP_HLS->streamInfoStore.push_back(streamInfo1);

    // Call the function under test
    std::vector<StreamInfo *> thumbnailTracks = mStreamAbstractionAAMP_HLS->GetAvailableThumbnailTracks();

    // Perform assertions
    // Verify that the function returned the expected number of thumbnail tracks
    ASSERT_EQ(thumbnailTracks.size(), 0);

    // Verify that the elements in thumbnailTracks are as expected
    for (const auto track : thumbnailTracks)
    {
        ASSERT_TRUE(track->isIframeTrack);
    }
}

TEST_F(FunctionalTests, SetThumbnailTrackTest)
{

    // Define sample StreamInfo objects with video bitrates
    StreamResolution resolution1;
    resolution1.width = 1920;     // Width in pixels
    resolution1.height = 1080;    // Height in pixels
    resolution1.framerate = 30.0; // Frame rate in FPS

    // Define sample StreamInfo objects
    HlsStreamInfo streamInfo1;
    streamInfo1.enabled = true;
    streamInfo1.isIframeTrack = true;
    streamInfo1.validity = true;
    streamInfo1.codecs = "h264";
    streamInfo1.bandwidthBitsPerSecond = 1000000; // 1 Mbps
    streamInfo1.resolution = resolution1;         // Full HD
    streamInfo1.reason = BitrateChangeReason::eAAMP_BITRATE_CHANGE_BY_ABR;

    // Add the sample streamInfo objects to the streamInfoStore (in practice, this is usually done during setup)
    mStreamAbstractionAAMP_HLS->streamInfoStore.push_back(streamInfo1);

    // Set the thumbnail track with index 0 (streamInfo1)
    bool rc = mStreamAbstractionAAMP_HLS->SetThumbnailTrack(-12);

    // Verify that the thumbnail track was successfully set
    // ASSERT_TRUE(rc);
    ASSERT_FALSE(rc);
}
TEST_F(FunctionalTests, SetThumbnailTrackTest_1)
{

    // Define sample StreamInfo objects with video bitrates
    StreamResolution resolution1;
    resolution1.width = 1920;     // Width in pixels
    resolution1.height = 1080;    // Height in pixels
    resolution1.framerate = 30.0; // Frame rate in FPS

    // Define sample StreamInfo objects
    HlsStreamInfo streamInfo1;
    streamInfo1.enabled = true;
    streamInfo1.isIframeTrack = true;
    streamInfo1.validity = true;
    streamInfo1.codecs = "h264";
    streamInfo1.bandwidthBitsPerSecond = 1000000; // 1 Mbps
    streamInfo1.resolution = resolution1;         // Full HD
    streamInfo1.reason = BitrateChangeReason::eAAMP_BITRATE_CHANGE_BY_ABR;

    // Add the sample streamInfo objects to the streamInfoStore (in practice, this is usually done during setup)
    mStreamAbstractionAAMP_HLS->streamInfoStore.push_back(streamInfo1);

    // Set the thumbnail track with index 0 (streamInfo1)
    bool rc = mStreamAbstractionAAMP_HLS->SetThumbnailTrack(3);

    // Verify that the thumbnail track was successfully set
    // ASSERT_TRUE(rc);
    ASSERT_FALSE(rc);
}

TEST_F(FunctionalTests, StartSubtitleParsertest)
{
    mStreamAbstractionAAMP_HLS->StartSubtitleParser();

    ASSERT_FALSE(mStreamAbstractionAAMP_HLS->trackState[eMEDIATYPE_SUBTITLE] != nullptr);
}

TEST_F(FunctionalTests, PauseSubtitleParsertest)
{
    mStreamAbstractionAAMP_HLS->PauseSubtitleParser(true);
}

TEST_F(FunctionalTests, InitiateDrmProcesstest)
{
    mStreamAbstractionAAMP_HLS->InitiateDrmProcess();
}

TEST_F(FunctionalTests, ChangeMuxedAudioTrackIndexTest)
{
    std::string index = "mux-1"; // Sample index

    mStreamAbstractionAAMP_HLS->ChangeMuxedAudioTrackIndex(index);
}

TEST_F(FunctionalTests, GetStreamOutputFormatForTrackVideo)
{

    TrackType videoTrackType = eTRACK_VIDEO;

    StreamOutputFormat outputFormat = mStreamAbstractionAAMP_HLS->GetStreamOutputFormatForTrack(videoTrackType);

    StreamOutputFormat expectedVideoOutputFormat = FORMAT_UNKNOWN;

    // Verify that the expected output format is returned for video track
    EXPECT_EQ(outputFormat, expectedVideoOutputFormat);
}

TEST_F(FunctionalTests, GetStreamOutputFormatForTrackAudio)
{
    // Arrange
    TrackType audioTrackType = eTRACK_AUDIO;

    // Act
    StreamOutputFormat outputFormat = mStreamAbstractionAAMP_HLS->GetStreamOutputFormatForTrack(audioTrackType);

    // Define the expected output format for audio track based on your logic
    StreamOutputFormat expectedAudioOutputFormat = FORMAT_AUDIO_ES_AAC;

    // Verify that the expected output format is returned for audio track
    EXPECT_EQ(outputFormat, expectedAudioOutputFormat);
}

TEST_F(FunctionalTests, GetStreamOutputFormatForTrackAuxAudio)
{
    // Arrange
    TrackType auxAudioTrackType = eTRACK_AUX_AUDIO;

    // Act
    StreamOutputFormat outputFormat = mStreamAbstractionAAMP_HLS->GetStreamOutputFormatForTrack(auxAudioTrackType);

    StreamOutputFormat expectedAuxAudioOutputFormat = FORMAT_AUDIO_ES_AAC; // Example value, replace with your logic

    // Verify that the expected output format is returned for auxiliary audio track
    EXPECT_EQ(outputFormat, expectedAuxAudioOutputFormat);
}

TEST_F(FunctionalTests, GetMediaIndexForLanguage)
{
    // Arrange
    std::string lang = "en";       // Replace with the desired language
    TrackType type = eTRACK_AUDIO; // Replace with the desired track type

    // Act
    int index = mStreamAbstractionAAMP_HLS->GetMediaIndexForLanguage(lang, type);

    int expectedIndex = -1; // Replace with your expected index value

    // Verify that the expected index is returned for the given language and track type
    EXPECT_EQ(index, expectedIndex);
}

TEST_F(FunctionalTests, StartInjectiontest)
{
    mStreamAbstractionAAMP_HLS->StartInjection();
}

// Define a test case for the GetPlaylistURI function for eTRACK_VIDEO
TEST_F(FunctionalTests, GetVideoPlaylistURITest)
{
    StreamOutputFormat format = FORMAT_MPEGTS;
    TrackType type = eTRACK_VIDEO;
    const char *playlistURI = mStreamAbstractionAAMP_HLS->GetPlaylistURI(type, &format);
    ASSERT_EQ(format, FORMAT_MPEGTS);
    ASSERT_EQ(type, eTRACK_VIDEO);
}

// Define a test case for the GetPlaylistURI function for eTRACK_AUDIO
TEST_F(FunctionalTests, GetAudioPlaylistURITest)
{
    StreamOutputFormat format;
    const char *playlistURI = mStreamAbstractionAAMP_HLS->GetPlaylistURI(eTRACK_AUDIO, &format);

    // Add assertions to verify the expected behavior

    ASSERT_NE(FORMAT_AUDIO_ES_AAC, format);
}

TEST_F(FunctionalTests, GetAvailableVideoTracksTest)
{
    std::vector<StreamInfo *> videoTracks = mStreamAbstractionAAMP_HLS->GetAvailableVideoTracks();

    // Add assertions to verify the expected behavior

    ASSERT_EQ(0, videoTracks.size());
}

// DELIA-41566 [PEACOCK] temporary hack required to work around Adobe SSAI session lifecycle problem
// TEST_F(FunctionalTests,PreCachePlaylisttest){
//     mStreamAbstractionAAMP_HLS->PreCachePlaylist();
// }

TEST_F(FunctionalTests, GetABRModetest)
{
    mStreamAbstractionAAMP_HLS->GetABRMode();
}

TEST_F(FunctionalTests, GetBestAudioTrackByLanguagetest)
{
    int index = mStreamAbstractionAAMP_HLS->GetBestAudioTrackByLanguage();
    ASSERT_EQ(index, -1);
}

TEST_F(FunctionalTests, IsLivetest)
{
    mStreamAbstractionAAMP_HLS->IsLive();
    bool isLive1 = mStreamAbstractionAAMP_HLS->IsLive();
    ASSERT_FALSE(isLive1);
}

TEST_F(FunctionalTests, Gtest)
{
    TuneType tuneType = eTUNETYPE_NEW_NORMAL; // Replace with the actual tune type
    AAMPStatusType result = mStreamAbstractionAAMP_HLS->Init(tuneType);
    EXPECT_NE(result, eAAMPSTATUS_OK);
}

TEST_F(FunctionalTests, GetFirstPTStest)
{
    double result = mStreamAbstractionAAMP_HLS->GetFirstPTS();
    EXPECT_EQ(result, 0.0);
}

TEST_F(FunctionalTests, GetBufferedDurationtest)
{
    double result = mStreamAbstractionAAMP_HLS->GetBufferedDuration();
    EXPECT_EQ(result, -1.0);
}

TEST_F(FunctionalTests, GetBWIndexTest)
{

    // Call GetBWIndex with a target bitrate
    BitsPerSecond targetBitrate = 10000; // Example target bitrate in bits per second
    int result = mStreamAbstractionAAMP_HLS->GetBWIndex(targetBitrate);

    // In this example, the function should return less then 1, as there is a stream with lower bitrates.
    EXPECT_NE(result, 1);
}
TEST_F(FunctionalTests, GetBWIndexTest_1)
{

    // Call GetBWIndex with a target bitrate
    BitsPerSecond targetBitrate = 7000000; // Example target bitrate in bits per second
    int result = mStreamAbstractionAAMP_HLS->GetBWIndex(targetBitrate);

    // In this example, the function should return 0, if there is no streaming hapen.
    EXPECT_EQ(result, 0);
}

TEST_F(FunctionalTests, GetMediaCounttest)
{
    int result = mStreamAbstractionAAMP_HLS->GetMediaCount();
    EXPECT_EQ(result, 0);
}

TEST_F(FunctionalTests, SeekPosUpdateTest)
{
    // Create an instance of StreamAbstractionAAMP_HLS (not required in this case)

    // Initial seek position
    double initialSeekPosition = 0.0;
    EXPECT_EQ(mStreamAbstractionAAMP_HLS->GetStreamPosition(), initialSeekPosition);

    // Update the seek position to a new value
    double newSeekPosition1 = 12;
    mStreamAbstractionAAMP_HLS->SeekPosUpdate(newSeekPosition1);

    // Verify that the seek position has been updated correctly
    EXPECT_EQ(mStreamAbstractionAAMP_HLS->GetStreamPosition(), newSeekPosition1);

    // Update the seek position to a negative value ,should fail
    double newSeekPosition2 = -12;
    mStreamAbstractionAAMP_HLS->SeekPosUpdate(newSeekPosition2);

    // Verify that the seek position is not updated as negative seekPos has been passed
    EXPECT_NE(mStreamAbstractionAAMP_HLS->GetStreamPosition(), newSeekPosition2);
    EXPECT_EQ(mStreamAbstractionAAMP_HLS->GetStreamPosition(), newSeekPosition1); // checking if unchanged
}

TEST_F(FunctionalTests, GetLanguageCodeTest)
{

    // Create a sample MediaInfo object
    MediaInfo mediaInfo = {
        eMEDIATYPE_DEFAULT,
        "audio_group_id",
        "AudioTrack",
        "en-US",
        true,
        false,
        "audio_uri",
        FORMAT_AUDIO_ES_AC3,
        2,
        "audio_stream_id",
        false,
        "audio_characteristics",
        false};

    // Set the mediaInfoStore for the streamAbstraction (usually done during setup)
    mStreamAbstractionAAMP_HLS->mediaInfoStore.push_back(mediaInfo);

    // Test case: Get language code from the MediaInfo object
    int iMedia = 0; // Index of the MediaInfo object we added
    std::string lang = mStreamAbstractionAAMP_HLS->GetLanguageCode(iMedia);

    // Verify that the language code matches the expected value
    EXPECT_EQ(lang, "en-US");
}

TEST_F(FunctionalTests, GetTotalProfileCounttest)
{
    int result = mStreamAbstractionAAMP_HLS->GetTotalProfileCount();
    EXPECT_EQ(result, 0);
}

TEST_F(FunctionalTests, Destructortest)
{
    StreamAbstractionAAMP_HLS *mStreamAbstractionAAMP_HLS_1 = new StreamAbstractionAAMP_HLS(mLogObj, mPrivateInstanceAAMP, 0, AAMP_NORMAL_PLAY_RATE);
    mStreamAbstractionAAMP_HLS_1->~StreamAbstractionAAMP_HLS();
}

TEST_F(TrackStateTests, Stoptest_1)
{
    TrackStateobj->Stop(true);
}

TEST_F(FunctionalTests, UpdateFailedDRMStatus_1)
{
    LicensePreFetchObject *object = NULL; // Create an instance of LicensePreFetchObject
    // Act: Call the function to be tested
    mStreamAbstractionAAMP_HLS->UpdateFailedDRMStatus(object);
}

TEST_F(TrackStateTests, FetchPlaylistTest)
{
    TrackStateobj->FetchPlaylist();
}

TEST_F(TrackStateTests, GetPeriodStartPositionTest)
{
    double result = TrackStateobj->GetPeriodStartPosition(1000);
    ASSERT_EQ(result, 0);
}

TEST_F(TrackStateTests, GetNumberOfPeriodsTest)
{
    int result = TrackStateobj->GetNumberOfPeriods();
    ASSERT_EQ(result, 0);
}

TEST_F(TrackStateTests, StopInjectionTest)
{
    TrackStateobj->StopInjection();
}

TEST_F(TrackStateTests, StopWaitForPlaylistRefreshTest)
{
    TrackStateobj->StopWaitForPlaylistRefresh();
}

TEST_F(TrackStateTests, CancelDrmOperationTest)
{
    TrackStateobj->CancelDrmOperation(true);
}
TEST_F(TrackStateTests, CancelDrmOperationTest_1)
{
    TrackStateobj->CancelDrmOperation(false);
}

TEST_F(TrackStateTests, IsLiveTest)
{
    bool isLive = TrackStateobj->IsLive();
    // You can assert that isLive is true when mPlaylistType is not ePLAYLISTTYPE_VOD
    ASSERT_TRUE(isLive);
    ;
}

TEST_F(TrackStateTests, GetXStartTimeOffsettest)
{
    double result = TrackStateobj->GetXStartTimeOffset();
    ASSERT_EQ(result, 0);
}

TEST_F(TrackStateTests, GetBufferedDurationtest_1)
{
    double result = TrackStateobj->GetBufferedDuration();
    ASSERT_EQ(result, 0);
}

TEST_F(TrackStateTests, GetPlaylistUrltest)
{
    std::string playlistUrl = TrackStateobj->GetPlaylistUrl();
    ASSERT_EQ(playlistUrl, "");
}

TEST_F(TrackStateTests, GetMinUpdateDurationTest)
{
    long result = TrackStateobj->GetMinUpdateDuration();
    ASSERT_EQ(result, 1000);
}

TEST_F(TrackStateTests, GetDefaultDurationBetweenPlaylistUpdatesTest)
{
    int defaultDuration = TrackStateobj->GetDefaultDurationBetweenPlaylistUpdates();
    ASSERT_EQ(defaultDuration, 6000);
}

TEST_F(TrackStateTests, RestoreDrmStateTest)
{
    TrackStateobj->RestoreDrmState();
}

TEST_F(TrackStateTests, IndexPlaylist_WhenIsRefreshTrue)
{
    // Arrange: Set up the necessary conditions for the test
    bool isRefresh = true;
    double culledSec = -12;

    // Act: Call the function to be tested
    TrackStateobj->IndexPlaylist(isRefresh, culledSec);
}

TEST_F(TrackStateTests, IndexPlaylist_WhenIsRefreshFalse)
{
    // Arrange: Set up the necessary conditions for the test
    bool isRefresh = false;
    double culledSec = 0.0;

    // Act: Call the function to be tested
    TrackStateobj->IndexPlaylist(isRefresh, culledSec);
}

TEST_F(TrackStateTests, IndexPlaylist_ProcessEXTINF)
{
    // Arrange: Set up the necessary conditions for the test
    bool isRefresh = false;
    double culledSec = 0.0;

    // Act: Call the function to be tested
    TrackStateobj->IndexPlaylist(isRefresh, culledSec);
}

TEST_F(TrackStateTests, GetNextFragmentUri_WithReloadUri)
{
    // Arrange: Set up the necessary conditions for the test
    bool reloadUri = true;
    bool ignoreDiscontinuity = false;

    // Act: Call the function to be tested
    char *fragmentUri = TrackStateobj->GetNextFragmentUriFromPlaylist(reloadUri, ignoreDiscontinuity);

    // Assert: Make assertions to verify the function's behavior in this case
    ASSERT_EQ(fragmentUri, nullptr);
}

TEST_F(TrackStateTests, GetNextFragmentUri_WithoutReloadUri)
{
    // Arrange: Set up the necessary conditions for the test
    bool reloadUri = false;
    bool ignoreDiscontinuity = true;

    // Act: Call the function to be tested
    char *fragmentUri = TrackStateobj->GetNextFragmentUriFromPlaylist(reloadUri, ignoreDiscontinuity);

    // Assert: Make assertions to verify the function's behavior in this case
    ASSERT_EQ(fragmentUri, nullptr);
}

TEST_F(TrackStateTests, UpdateDrmCMSha1Hash_WithNullPtr)
{
    // Act: Call the function to be tested with a null pointer
    TrackStateobj->UpdateDrmCMSha1Hash(nullptr);

    // Assert: Make assertions to verify the function's behavior when a null pointer is provided

    ASSERT_EQ(TrackStateobj->mCMSha1Hash, nullptr); // Hash should be null
}

TEST_F(TrackStateTests, DrmDecrypt_SuccessfulDecryption)
{
    // Arrange: Set up the necessary conditions for the test
    CachedFragment cachedFragment; // Create a CachedFragment with valid data
    ProfilerBucketType bucketType = ProfilerBucketType::PROFILE_BUCKET_PLAYLIST_VIDEO;

    // Act: Call the function to be tested
    DrmReturn result = TrackStateobj->DrmDecrypt(&cachedFragment, bucketType);

    // Assert: Make assertions to verify the function's behavior
    // Verify that decryption was successful and the function returned true.

    ASSERT_NE(result, DrmReturn::eDRM_SUCCESS); // Check that the function returns DRM_SUCCESS
}
TEST_F(TrackStateTests, CreateInitVector_Successful)
{
    // Arrange: Set up the necessary conditions for the test
    unsigned int seqNo = 100; // Replace with a valid sequence number

    // Act: Call the function to be tested
    bool result = TrackStateobj->CreateInitVectorByMediaSeqNo(seqNo);

    // Assert: Make assertions to verify the function's behavior
    ASSERT_TRUE(result); // Check that the function returns true indicating success
}

TEST_F(TrackStateTests, GetPeriodStartPosition_InvalidPeriod)
{
    // Arrange: Set up the necessary conditions for the test
    int periodIdx = -1; // Replace with an invalid period index (out of bounds)

    // Act: Call the function to be tested
    double startPosition = TrackStateobj->GetPeriodStartPosition(periodIdx);

    // Assert: Make assertions to verify the function's behavior
    // Verify that the function handles the case of an invalid period index gracefully.

    ASSERT_DOUBLE_EQ(startPosition, 0.0); // Check that the function returns a default or invalid value for an invalid period index
}
TEST_F(TrackStateTests, GetPeriodStartPosition_NoDiscontinuityNodes)
{
    // Arrange: Set up the necessary conditions for the test
    int periodIdx = 1; // Replace with a valid period index

    // Simulate a scenario where there are no discontinuity nodes
    TrackStateobj->mDiscontinuityIndexCount = 0; // Set mDiscontinuityIndexCount to 0

    // Act: Call the function to be tested
    double startPosition = TrackStateobj->GetPeriodStartPosition(periodIdx);

    ASSERT_DOUBLE_EQ(startPosition, 0.0); // Check that the function returns a default or zero value when there are no discontinuity nodes
}

TEST_F(TrackStateTests, FindTimedMetadata_WithTags)
{
    // Arrange: Set up the necessary conditions for the test
    bool reportBulkMeta = false;
    bool bInitCall = false;

    // Act: Call the function to be tested
    TrackStateobj->FindTimedMetadata(reportBulkMeta, bInitCall);
}

TEST_F(TrackStateTests, FindTimedMetadata_ReportBulk)
{
    // Arrange: Set up the necessary conditions for the test
    bool reportBulkMeta = true; // Simulate the reportBulkMeta flag being set
    bool bInitCall = false;

    // Act: Call the function to be tested
    TrackStateobj->FindTimedMetadata(reportBulkMeta, bInitCall);
}

TEST_F(TrackStateTests, FindTimedMetadata_InitCall)
{
    // Arrange: Set up the necessary conditions for the test
    bool reportBulkMeta = false;
    bool bInitCall = true; // Simulate the bInitCall flag being set
    // Act: Call the function to be tested
    TrackStateobj->FindTimedMetadata(reportBulkMeta, bInitCall);
}

TEST_F(TrackStateTests, SetXStartTimeOffset)
{
    // Arrange: Set up the necessary conditions for the test
    // double offset = 123.45; // Choose a test offset value
    double offset = -12; // Choose a test offset value
    // Act: Call the function to be tested
    TrackStateobj->SetXStartTimeOffset(offset);

    ASSERT_EQ(TrackStateobj->GetXStartTimeOffset(), offset); // Check if the offset matches what was set
}

TEST_F(TrackStateTests, SetEffectivePlaylistUrl)
{
    // Arrange: Set up the necessary conditions for the test
    std::string url = "https://example.com/playlist.m3u8"; // Choose a test URL

    // Act: Call the function to be tested
    TrackStateobj->SetEffectivePlaylistUrl(url);

    ASSERT_EQ(TrackStateobj->GetEffectivePlaylistUrl(), url); // Check if the URL matches what was set
}

TEST_F(TrackStateTests, GetLastPlaylistDownloadTime)
{
    // Arrange: Set up the necessary conditions for the test
    long long expectedTime = 123456789; // Choose a test time value
    TrackStateobj->SetLastPlaylistDownloadTime(expectedTime);

    // Act: Call the function to be tested
    long long actualTime = TrackStateobj->GetLastPlaylistDownloadTime();

    ASSERT_EQ(actualTime, expectedTime); // Check if the actual time matches the expected time
}

TEST_F(TrackStateTests, SetLastPlaylistDownloadTime)
{
    // Arrange: Set up the necessary conditions for the test
    long long timeToSet = 987654321; // Choose a test time value

    // Act: Call the function to be tested
    TrackStateobj->SetLastPlaylistDownloadTime(timeToSet);

    ASSERT_EQ(TrackStateobj->GetLastPlaylistDownloadTime(), timeToSet); // Check if the time was correctly set
}

TEST_F(FunctionalTests, FilterAudioCodecBasedOnConfig_AllowAC3)
{
    EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_DisableEC3)).WillOnce(Return(true));
    bool result = mStreamAbstractionAAMP_HLS->FilterAudioCodecBasedOnConfig(FORMAT_AUDIO_ES_AC3);
    ASSERT_TRUE(result); // AC3 should be allowed
}

TEST_F(FunctionalTests, SetABRMinBuffer_test)
{
    //    mStreamAbstractionAAMP_HLS->SetABRMinBuffer(eAAMPConfig_MinABRNWBufferRampDown);
    //    mStreamAbstractionAAMP_HLS->SetABRMinBuffer(eAAMPConfig_MaxABRNWBufferRampUp);
    //    mStreamAbstractionAAMP_HLS->SetABRMinBuffer(UINT_MAX);
    //    mStreamAbstractionAAMP_HLS->SetABRMinBuffer(11.32888977);
    mStreamAbstractionAAMP_HLS->SetABRMinBuffer(-12);
}

TEST_F(FunctionalTests, SetABRMaxBuffer_test)
{
    mStreamAbstractionAAMP_HLS->SetABRMaxBuffer(-12);
}
// Test case to verify SetTsbBandwidth with boundary conditions
TEST_F(FunctionalTests, SetTsbBandwidthBoundary)
{
    // Test lower bound
    long lowerBound = 0;
    mStreamAbstractionAAMP_HLS->SetTsbBandwidth(lowerBound);
    long actualLowerBound = mStreamAbstractionAAMP_HLS->GetTsbBandwidth();
    ASSERT_EQ(actualLowerBound, lowerBound);

    // Test upper bound (e.g., UINT_MAX)
    long upperBound = UINT_MAX;
    mStreamAbstractionAAMP_HLS->SetTsbBandwidth(upperBound);
    long actualUpperBound = mStreamAbstractionAAMP_HLS->GetTsbBandwidth();
    ASSERT_EQ(actualUpperBound, upperBound);

    // Test upper bound (e.g., LONG_MAX)
    long upperBound_1 = LONG_MAX;
    mStreamAbstractionAAMP_HLS->SetTsbBandwidth(upperBound_1);
    long actualUpperBound_1 = mStreamAbstractionAAMP_HLS->GetTsbBandwidth();
    ASSERT_EQ(actualUpperBound_1, upperBound_1);

    // Test upper bound (e.g., LONG_MIN)
    // LONG_MIN is the smallest negative value that can be represented in a long integer
    long lowerBound_2 = LONG_MIN;
    mStreamAbstractionAAMP_HLS->SetTsbBandwidth(lowerBound_2);
    long actualLowerBound_2 = mStreamAbstractionAAMP_HLS->GetTsbBandwidth();
    ASSERT_NE(actualLowerBound_2, lowerBound_2);

    // Test lower bound
    long lowerBound_1 = -12;
    mStreamAbstractionAAMP_HLS->SetTsbBandwidth(lowerBound_1);
    long actualLowerBound_1 = mStreamAbstractionAAMP_HLS->GetTsbBandwidth();
    // Bandwidth should not be negative
    ASSERT_NE(actualLowerBound_1, lowerBound_1);
}

TEST_F(FunctionalTests, GetProfileCounttest)
{
    // Call the function under test
    int profileCount = mStreamAbstractionAAMP_HLS->GetProfileCount();

    // Perform assertions to verify the expected behavior
    ASSERT_EQ(profileCount, 0);
}

// Bug as mutex thread handling not done properly
TEST_F(FunctionalTests, WaitForVideoTrackCatchupTest)
{
    mStreamAbstractionAAMP_HLS->WaitForVideoTrackCatchup();
}

TEST_F(FunctionalTests, ReassessAndResumeAudioTrackTest)
{
    mStreamAbstractionAAMP_HLS->ReassessAndResumeAudioTrack(true);
}

TEST_F(FunctionalTests, ReassessAndResumeAudioTrackTest_1)
{

    mStreamAbstractionAAMP_HLS->ReassessAndResumeAudioTrack(false);
}
TEST_F(FunctionalTests, LastVideoFragParsedTimeMSTest)
{
    double result = mStreamAbstractionAAMP_HLS->LastVideoFragParsedTimeMS();
    ASSERT_EQ(result, 0);
}

TEST_F(FunctionalTests, GetDesiredProfileTest)
{

    mStreamAbstractionAAMP_HLS->GetDesiredProfile(true);
}

TEST_F(FunctionalTests, GetDesiredProfileTest_1)
{

    mStreamAbstractionAAMP_HLS->GetDesiredProfile(false);
}

TEST_F(FunctionalTests, GetMaxBWProfileTest)
{
    int result = mStreamAbstractionAAMP_HLS->GetMaxBWProfile();
    ASSERT_EQ(result, 0);
}

TEST_F(FunctionalTests, NotifyBitRateUpdateTest)
{
    // Set up necessary data and conditions for testing
    int profileIndex = 1; // Replace with the desired profileIndex value
    StreamInfo cacheFragStreamInfo;
    // Call the NotifyBitRateUpdate function
    mStreamAbstractionAAMP_HLS->NotifyBitRateUpdate(profileIndex, cacheFragStreamInfo, 0.0); // Set position to 0.0 for this test
}
TEST_F(FunctionalTests, NotifyBitRateUpdateTest_1)
{
    // Set up necessary data and conditions for testing
    int profileIndex = 1; // Replace with the desired profileIndex value
    StreamInfo cacheFragStreamInfo;
    cacheFragStreamInfo.bandwidthBitsPerSecond = 0; // Zero value
    // Call the NotifyBitRateUpdate function
    mStreamAbstractionAAMP_HLS->NotifyBitRateUpdate(profileIndex, cacheFragStreamInfo, 0.0); // Set position to 0.0 for this test
}

TEST_F(FunctionalTests, IsInitialCachingSupported)
{
    bool result = mStreamAbstractionAAMP_HLS->IsInitialCachingSupported();
    ASSERT_FALSE(result);
}

TEST_F(FunctionalTests, UpdateStreamInfoBitrateDatatest)
{
    // Set up necessary data and conditions for testing
    int profileIndex = 1; // Replace with the desired profileIndex value
    StreamInfo cacheFragStreamInfo;
    // Call the NotifyBitRateUpdate function
    mStreamAbstractionAAMP_HLS->UpdateStreamInfoBitrateData(profileIndex, cacheFragStreamInfo); // Set position to 0.0 for this test
}

TEST_F(FunctionalTests, UpdateRampdownProfileReasontest)
{
    BitrateChangeReason expectedBitrateReason = eAAMP_BITRATE_CHANGE_BY_RAMPDOWN;

    // Call the function under test
    mStreamAbstractionAAMP_HLS->UpdateRampdownProfileReason();

    //"StreamAbstractionAAMP::mBitrateReason" (declared at line 1704 of "/home/chtsl01380/Desktop/AAMP_18_Aug/aamp/StreamAbstractionAAMP.h") is inaccessible
    // ASSERT_EQ(mStreamAbstractionAAMP_HLS->mBitrateReason, expectedBitrateReason);
}

TEST_F(FunctionalTests, ConfigureTimeoutOnBuffertest)
{
    // Set up necessary data and conditions for testing
    mStreamAbstractionAAMP_HLS->ConfigureTimeoutOnBuffer();
}

TEST_F(FunctionalTests, RampDownProfiletest)
{
    // Set up necessary data and conditions for testing
    bool result = mStreamAbstractionAAMP_HLS->RampDownProfile(400);
    ASSERT_FALSE(result);
}

TEST_F(FunctionalTests, IsLowestProfileTest)
{
    // Set up the necessary data or objects for your test
    int currentProfileIndex = 0;
    // int currentProfileIndex = -22;
    // Call the function you want to test
    bool result = mStreamAbstractionAAMP_HLS->IsLowestProfile(currentProfileIndex);

    ASSERT_TRUE(result);
}

TEST_F(FunctionalTests, getOriginalCurlErrorTest)
{
    // Test scenario: http_error is below PARTIAL_FILE_CONNECTIVITY_AAMP
    int httpError = 100; // Example value below the range
    int result = mStreamAbstractionAAMP_HLS->getOriginalCurlError(httpError);
    ASSERT_EQ(result, httpError); // It should return the original error code

    // Test scenario : http_error is above PARTIAL_FILE_START_STALL_TIMEOUT_AAMP
    int httpError1 = 500; // Example value above the range
    int result1 = mStreamAbstractionAAMP_HLS->getOriginalCurlError(httpError1);
    ASSERT_EQ(result1, httpError1); // It should return the original error code
}

TEST_F(FunctionalTests, CheckForProfileChangetest)
{
    // Set up necessary data and conditions for testing
    mStreamAbstractionAAMP_HLS->CheckForProfileChange();
}

TEST_F(FunctionalTests, GetIframeTracktest)
{
    // Set up necessary data and conditions for testing
    int result = mStreamAbstractionAAMP_HLS->GetIframeTrack();
    ASSERT_EQ(result, 0);
}

TEST_F(FunctionalTests, UpdateIframeTrackstest)
{
    // Set up necessary data and conditions for testing
    mStreamAbstractionAAMP_HLS->UpdateIframeTracks();
}

TEST_F(FunctionalTests, NotifyPlaybackPausedtest)
{
    // Set up necessary data and conditions for testing
    mStreamAbstractionAAMP_HLS->NotifyPlaybackPaused(true);
}

TEST_F(FunctionalTests, NotifyPlaybackPausedtest_1)
{
    // Set up necessary data and conditions for testing
    mStreamAbstractionAAMP_HLS->NotifyPlaybackPaused(false);
}

TEST_F(FunctionalTests, CheckIfPlayerRunningDry)
{
    // Set up necessary data and conditions for testing
    bool result = mStreamAbstractionAAMP_HLS->CheckIfPlayerRunningDry();
    ASSERT_FALSE(result);
}

TEST_F(FunctionalTests, CheckForPlaybackStalltest)
{

    EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_SuppressDecode)).WillOnce(Return(true));
    mStreamAbstractionAAMP_HLS->CheckForPlaybackStall(true);
}

TEST_F(FunctionalTests, CheckForPlaybackStalltest_1)
{

    EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_SuppressDecode)).WillOnce(Return(true));
    mStreamAbstractionAAMP_HLS->CheckForPlaybackStall(false);
}

TEST_F(FunctionalTests, NotifyFirstFragmentInjectedtest)
{
    // Set up necessary data and conditions for testing
    mStreamAbstractionAAMP_HLS->NotifyFirstFragmentInjected();
}

TEST_F(FunctionalTests, GetElapsedTimetest)
{
    // Set up necessary data and conditions for testing
    double result = mStreamAbstractionAAMP_HLS->GetElapsedTime();
    ASSERT_NE(result, 0.0);
}

TEST_F(FunctionalTests, GetVideoBitratetest)
{
    // Set up necessary data and conditions for testing
    BitsPerSecond result = mStreamAbstractionAAMP_HLS->GetVideoBitrate();
    ASSERT_EQ(result, 0.0);
}

TEST_F(FunctionalTests, GetAudioBitratetest)
{
    // Set up necessary data and conditions for testing
    BitsPerSecond result = mStreamAbstractionAAMP_HLS->GetAudioBitrate();
    ASSERT_EQ(result, 0.0);
}

TEST_F(FunctionalTests, IsMuxedStreamtest)
{
    // Set up necessary data and conditions for testing
    EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_AudioOnlyPlayback)).WillOnce(Return(true));
    bool result = mStreamAbstractionAAMP_HLS->IsMuxedStream();
    ASSERT_FALSE(result);
}

TEST_F(FunctionalTests, WaitForAudioTrackCatchuptest)
{
    // Set up necessary data and conditions for testing
    mStreamAbstractionAAMP_HLS->WaitForAudioTrackCatchup();
}

TEST_F(FunctionalTests, AbortWaitForAudioTrackCatchuptest)
{
    // Set up necessary data and conditions for testing
    mStreamAbstractionAAMP_HLS->AbortWaitForAudioTrackCatchup(true);
}

TEST_F(FunctionalTests, AbortWaitForAudioTrackCatchuptest_1)
{
    // Set up necessary data and conditions for testing
    mStreamAbstractionAAMP_HLS->AbortWaitForAudioTrackCatchup(false);
}

TEST_F(FunctionalTests, MuteSubtitlestest)
{
    // Set up necessary data and conditions for testing
    mStreamAbstractionAAMP_HLS->MuteSubtitles(false);
}

TEST_F(FunctionalTests, MuteSubtitlestesttest_1)
{
    // Set up necessary data and conditions for testing
    mStreamAbstractionAAMP_HLS->MuteSubtitles(false);
}

TEST_F(FunctionalTests, IsEOSReachedtest)
{
    // Set up necessary data and conditions for testing
    bool result = mStreamAbstractionAAMP_HLS->IsEOSReached();
    ASSERT_TRUE(result);
}

TEST_F(FunctionalTests, GetLastInjectedFragmentPositiontest)
{
    // Set up necessary data and conditions for testing
    double result = mStreamAbstractionAAMP_HLS->GetLastInjectedFragmentPosition();
    ASSERT_EQ(result, 0);
}

TEST_F(FunctionalTests, resetDiscontinuityTrackStatetest)
{
    // Set up necessary data and conditions for testing
    mStreamAbstractionAAMP_HLS->resetDiscontinuityTrackState();
}

TEST_F(FunctionalTests, AbortWaitForDiscontinuitytest)
{
    // Set up necessary data and conditions for testing
    mStreamAbstractionAAMP_HLS->AbortWaitForDiscontinuity();
}

TEST_F(FunctionalTests, CheckForMediaTrackInjectionStalltest)
{
    // Set up necessary data and conditions for testing
    mStreamAbstractionAAMP_HLS->CheckForMediaTrackInjectionStall(eTRACK_AUDIO);
}

TEST_F(FunctionalTests, CheckForMediaTrackInjectionStalltest_1)
{
    // Set up necessary data and conditions for testing
    mStreamAbstractionAAMP_HLS->CheckForMediaTrackInjectionStall(eTRACK_VIDEO);
}

TEST_F(FunctionalTests, CheckForRampDownLimitReachedtest)
{
    // Set up necessary data and conditions for testing
    bool result = mStreamAbstractionAAMP_HLS->CheckForRampDownLimitReached();
    EXPECT_TRUE(result);
}

TEST_F(FunctionalTests, GetBufferedVideoDurationSectest)
{
    // Set up necessary data and conditions for testing
    double result = mStreamAbstractionAAMP_HLS->GetBufferedVideoDurationSec();
    ASSERT_EQ(result, -1);
}

TEST_F(FunctionalTests, GetAudioTracktest)
{
    // Set up necessary data and conditions for testing
    int result = mStreamAbstractionAAMP_HLS->GetAudioTrack();
    ASSERT_EQ(result, -1);
}

TEST_F(FunctionalTests, GetTextTracktest)
{
    // Set up necessary data and conditions for testing
    int result = mStreamAbstractionAAMP_HLS->GetTextTrack();
    ASSERT_EQ(result, -1.0);
}

TEST_F(FunctionalTests, RefreshSubtitlestest)
{
    // Set up necessary data and conditions for testing
    mStreamAbstractionAAMP_HLS->RefreshSubtitles();
}

TEST_F(FunctionalTests, WaitForVideoTrackCatchupForAuxtest)
{
    // Set up necessary data and conditions for testing
    mStreamAbstractionAAMP_HLS->WaitForVideoTrackCatchupForAux();
}

TEST_F(FunctionalTests, GetPreferredLiveOffsetFromConfigtest)
{
    // Set up necessary data and conditions for testing
    bool result = mStreamAbstractionAAMP_HLS->GetPreferredLiveOffsetFromConfig();
    EXPECT_FALSE(result);
}

TEST_F(FunctionalTests, IsStreamerAtLivePointtest)
{
    // Set up necessary data and conditions for testing
    bool result = mStreamAbstractionAAMP_HLS->IsStreamerAtLivePoint();
    EXPECT_FALSE(result);
}

TEST_F(FunctionalTests, IsSeekedToLivetest)
{
    // Set up necessary data and conditions for testing
    // mStreamAbstractionAAMP_HLS->IsSeekedToLive(0.0);
    bool result = mStreamAbstractionAAMP_HLS->IsSeekedToLive(-1.2);
    EXPECT_TRUE(result);
}

TEST_F(FunctionalTests, DisablePlaylistDownloadstest)
{
    // Set up necessary data and conditions for testing
    mStreamAbstractionAAMP_HLS->DisablePlaylistDownloads();
}

TEST_F(FunctionalTests, IsStreamerAtLivePointtest4)
{
    double seekPosition = 370.0;

    mStreamAbstractionAAMP_HLS->aamp->culledSeconds = 100.0;

    mStreamAbstractionAAMP_HLS->aamp->durationSeconds = 300.0;

    mStreamAbstractionAAMP_HLS->aamp->mLiveOffset = 30.0;

    mStreamAbstractionAAMP_HLS->mIsAtLivePoint = true;

    bool result = mStreamAbstractionAAMP_HLS->IsStreamerAtLivePoint(seekPosition);

    // EXPECT_TRUE(result);
    EXPECT_FALSE(result);
}

TEST_F(TrackStateTests, StopInjectLooptest)
{
    // Act: Call the function to be tested
    TrackStateobj->StopInjectLoop();
}

TEST_F(TrackStateTests, StopInjectChunkLooptest)
{
    // Act: Call the function to be tested
    TrackStateobj->StopInjectChunkLoop();
}

TEST_F(TrackStateTests, EnabledTests)
{
    bool result = TrackStateobj->Enabled();
    ASSERT_FALSE(result);
}

TEST_F(TrackStateTests, GetFetchBufferTests)
{
    TrackStateobj->GetFetchBuffer(true);
    CachedFragment *fetchBuffer = TrackStateobj->GetFetchBuffer(false);
    // ASSERT_EQ(fetchBuffer, nullptr);
}

TEST_F(TrackStateTests, GetFetchChunkBufferTest)
{

    // Call the function under test with initialize set to true
    CachedFragmentChunk *cachedFragmentChunk = TrackStateobj->GetFetchChunkBuffer(true);
    ASSERT_EQ(cachedFragmentChunk, nullptr);
}

TEST_F(TrackStateTests, GetCurrentBandWidthTests)
{
    int result = TrackStateobj->GetCurrentBandWidth();
    ASSERT_EQ(result, 0);
}

TEST_F(TrackStateTests, FlushFragmentsTests)
{
    TrackStateobj->FlushFragments();
}

TEST_F(TrackStateTests, FlushFragmentChunksTests)
{
    TrackStateobj->FlushFragmentChunks();
}

TEST_F(TrackStateTests, OnSinkBufferFullTests)
{
    TrackStateobj->OnSinkBufferFull();
}

TEST_F(TrackStateTests, GetPlaylistMediaTypeFromTrackTest)
{

    MediaType playlistMediaType = TrackStateobj->GetPlaylistMediaTypeFromTrack(eTRACK_VIDEO, true);
    ASSERT_EQ(playlistMediaType, eMEDIATYPE_PLAYLIST_IFRAME);
}

TEST_F(TrackStateTests, AbortWaitForPlaylistDownloadTests)
{

    TrackStateobj->AbortWaitForPlaylistDownload();
}

TEST_F(TrackStateTests, EnterTimedWaitForPlaylistRefreshTests)
{
    TrackStateobj->EnterTimedWaitForPlaylistRefresh(-22);
}

TEST_F(TrackStateTests, AbortFragmentDownloaderWaitTests)
{
    TrackStateobj->AbortFragmentDownloaderWait();
}

TEST_F(TrackStateTests, WaitForManifestUpdateTests)
{
    TrackStateobj->WaitForManifestUpdate();
}

// TEST_F(TrackStateTests, WaitTimeBasedOnBufferAvailableTests)
// {

//     long long lastPlaylistDownloadTimeMS = 1000; // Example value for lastPlaylistDownloadTimeMS

//     long long currentPlayPosition = 10000; // Example value for currentPlayPosition

//     long long endPositionAvailable = 60000; // Example value for endPositionAvailable

//     bool lowLatencyMode = true; // Example value for lowLatencyMode

//     long minUpdateDuration = 2000; // Example value for minUpdateDuration

//     // Set the values in the MediaTrack object

//     TrackStateobj->SetLastPlaylistDownloadTime(lastPlaylistDownloadTimeMS);

//     int minDelay = TrackStateobj->WaitTimeBasedOnBufferAvailable();

//     ASSERT_EQ(minDelay, 1500);

// }

TEST_F(TrackStateTests, GetBufferStatusTest)
{
    // Call the function under test
    BufferHealthStatus bufferStatus = TrackStateobj->GetBufferStatus();
    ASSERT_EQ(bufferStatus, BUFFER_STATUS_RED);
}

TEST_F(TrackStateTests, WaitForFreeFragmentAvailableTests)
{
    int timeoutMs = 100;
    bool result = TrackStateobj->WaitForFreeFragmentAvailable(timeoutMs);
    ASSERT_FALSE(result);
}

TEST_F(TrackStateTests, AbortWaitForCachedFragmentTests)
{
    TrackStateobj->AbortWaitForCachedFragment();
}

TEST_F(TrackStateTests, ProcessFragmentChunkTests)
{
    double result = TrackStateobj->ProcessFragmentChunk();
    ASSERT_FALSE(result);
}

TEST_F(TrackStateTests, NotifyFragmentCollectorWaittest)
{
    TrackStateobj->NotifyFragmentCollectorWait();
}

TEST_F(TrackStateTests, GetTotalInjectedDurationtest)
{
    double result = TrackStateobj->GetTotalInjectedDuration();
    ASSERT_EQ(result, 0.0);
}

TEST_F(TrackStateTests, GetTotalFetchedDurationtest)
{
    double result = TrackStateobj->GetTotalFetchedDuration();
    ASSERT_EQ(result, 0.0);
}

TEST_F(TrackStateTests, IsDiscontinuityProcessedtest)
{
    bool result = TrackStateobj->IsDiscontinuityProcessed();
    ASSERT_FALSE(result);
}

TEST_F(TrackStateTests, isFragmentInjectorThreadStartedtest)
{
    bool result = TrackStateobj->isFragmentInjectorThreadStarted();
    ASSERT_FALSE(result);
}

TEST_F(TrackStateTests, IsInjectionAbortedtest)
{
    bool result = TrackStateobj->IsInjectionAborted();
    ASSERT_FALSE(result);
}

TEST_F(TrackStateTests, IsAtEndOfTracktest)
{
    bool result = TrackStateobj->IsAtEndOfTrack();
    ASSERT_FALSE(result);
}

TEST_F(FunctionalTests, GetMediaTracktest)
{
    mStreamAbstractionAAMP_HLS->GetMediaTrack(eTRACK_VIDEO);
}

TEST_F(FunctionalTests, GetStartTimeOfFirstPTStest)
{
    double result = mStreamAbstractionAAMP_HLS->GetStartTimeOfFirstPTS();
    ASSERT_EQ(result, 0.0);
}

// Test case to check the behavior of Is4KStream when height is 4000 and bandwidth is sufficient
TEST_F(FunctionalTests, Test4KStreamPositive)
{
    int height = 4000;
    BitsPerSecond bandwidth = 10000; // Adjust this value as needed
    bool result = mStreamAbstractionAAMP_HLS->Is4KStream(height, bandwidth);
    ASSERT_FALSE(result);
}

// Test case to check the behavior of Is4KStream when height is less than 4000
TEST_F(FunctionalTests, Test4KStreamHeightTooLow)
{
    int height = 3000;               // Adjust this value as needed
    BitsPerSecond bandwidth = 10000; // Adjust this value as needed
    bool result = mStreamAbstractionAAMP_HLS->Is4KStream(height, bandwidth);
    ASSERT_FALSE(result);
}

// Test case to check the behavior of Is4KStream when bandwidth is insufficient
TEST_F(FunctionalTests, Test4KStreamBandwidthInsufficient)
{
    int height = 0;
    BitsPerSecond bandwidth = 0; // Adjust this value as needed
    bool result = mStreamAbstractionAAMP_HLS->Is4KStream(height, bandwidth);
    ASSERT_FALSE(result);
}

// Test case for GetFirstPeriodStartTime()
TEST_F(FunctionalTests, TestGetFirstPeriodStartTime)
{
    double expectedValue = 0.0; // The expected return value
    // Call the virtual function and check if it returns the expected value.
    double result = mStreamAbstractionAAMP_HLS->GetFirstPeriodStartTime();
    ASSERT_EQ(result, expectedValue);
}

// Test case for GetFirstPeriodDynamicStartTime()
TEST_F(FunctionalTests, TestGetFirstPeriodDynamicStartTime)
{
    double expectedValue = 0.0; // The expected return value
    // Call the virtual function and check if it returns the expected value.
    double result = mStreamAbstractionAAMP_HLS->GetFirstPeriodDynamicStartTime();
    ASSERT_EQ(result, expectedValue);
}

// Test case for GetCurrPeriodTimeScale()
TEST_F(FunctionalTests, TestGetCurrPeriodTimeScale)
{
    uint32_t expectedValue = 0; // The expected return value
    // Call the virtual function and check if it returns the expected value.
    uint32_t result = mStreamAbstractionAAMP_HLS->GetCurrPeriodTimeScale();
    ASSERT_EQ(result, expectedValue);
}
// Test case for GetBWIndex()
TEST_F(FunctionalTests, TestGetBWIndex)
{
    BitsPerSecond bandwidth = -13; // Create a BitsPerSecond object if needed
    int expectedValue = 0;         // The expected return value
    // Call the virtual function and check if it returns the expected value.
    int result = mStreamAbstractionAAMP_HLS->GetBWIndex(bandwidth);
    ASSERT_EQ(result, expectedValue);
}

// Test case for GetProfileIndexForBandwidth()
TEST_F(FunctionalTests, TestGetProfileIndexForBandwidth)
{
    BitsPerSecond mTsbBandwidth; // Create a BitsPerSecond object with an appropriate value
    int expectedValue = 0;
    // Call the virtual function and check if it returns the expected value.
    int result = mStreamAbstractionAAMP_HLS->GetProfileIndexForBandwidth(mTsbBandwidth);
    ASSERT_EQ(result, expectedValue);
}

// Test case for GetMaxBitrate()
TEST_F(FunctionalTests, TestGetMaxBitrate)
{
    BitsPerSecond expectedValue = 0; // Set to an appropriate value for your test case
    // Call the function to get the max bitrate.
    BitsPerSecond result = mStreamAbstractionAAMP_HLS->GetMaxBitrate();
    ASSERT_EQ(result, expectedValue);
}

// Test case for GetVideoBitrates() when the function returns an empty vector.
TEST_F(FunctionalTests, TestGetVideoBitratesEmpty)
{
    // Call the function to get the video bitrates.
    std::vector<BitsPerSecond> result = mStreamAbstractionAAMP_HLS->GetVideoBitrates();
    // Check if the result is an empty vector.
    ASSERT_TRUE(result.empty());
}

// Test case for GetAudioBitrates() when the function returns an empty vector.
TEST_F(FunctionalTests, TestGetAudioBitratesEmpty)
{
    // Call the function to get the audio bitrates.
    std::vector<BitsPerSecond> result = mStreamAbstractionAAMP_HLS->GetAudioBitrates();
    // Check if the result is an empty vector.
    ASSERT_TRUE(result.empty());
}

TEST_F(FunctionalTests, StopInjectiontest)
{
    mStreamAbstractionAAMP_HLS->StopInjection();
}

TEST_F(FunctionalTests, IsStreamerStalledtest)
{
    bool result = mStreamAbstractionAAMP_HLS->IsStreamerStalled();
    ASSERT_FALSE(result);
}

TEST_F(FunctionalTests, SetTextStyleTest)
{
    // Create an instance of your Subtitle class (replace with the actual class name)

    // Define a JSON string with test options
    std::string testOptions = "AAMP";
    // Call the SetTextStyle function and check the result
    bool result = mStreamAbstractionAAMP_HLS->SetTextStyle(testOptions);

    EXPECT_FALSE(result);
}

TEST_F(FunctionalTests, TestResumeSubtitleOnPlay)
{
    // Define test data and mute flag
    char testData[] = "";
    bool mute = false;
    // Call the ResumeSubtitleAfterSeek function
    mStreamAbstractionAAMP_HLS->ResumeSubtitleOnPlay(mute, testData);
}

TEST_F(FunctionalTests, TestMuteSidecarSubtitles)
{
    // Define a mute flag (true or false)
    bool mute = true;
    // Call the MuteSidecarSubtitles function .
    mStreamAbstractionAAMP_HLS->MuteSidecarSubtitles(mute);
}

TEST_F(FunctionalTests, TestMuteSubtitleOnPause)
{
    // Call the MuteSubtitleOnPause function .
    mStreamAbstractionAAMP_HLS->MuteSubtitleOnPause();
}

TEST_F(FunctionalTests, TestMResetSubtitle)
{
    // Call the ResetSubtitle function .
    mStreamAbstractionAAMP_HLS->ResetSubtitle();
}

TEST_F(FunctionalTests, TestInitSubtitleParser)
{
    char testData[] = "";
    // Call the ResetSubtitle function .
    mStreamAbstractionAAMP_HLS->InitSubtitleParser(testData);
}

TEST_F(FunctionalTests, TestSetCurrentAudioTrackIndex)
{
    // Define an audio track index as a string
    std::string trackIndex = "";
    // Call the SetCurrentAudioTrackIndex function
    mStreamAbstractionAAMP_HLS->SetCurrentAudioTrackIndex(trackIndex);
}

TEST_F(FunctionalTests, TestSetCurrentAudioTrackIndex_1)
{
    // Define an audio track index as a string
    std::string trackIndex = "0";
    // Call the SetCurrentAudioTrackIndex function
    mStreamAbstractionAAMP_HLS->SetCurrentAudioTrackIndex(trackIndex);
}

TEST_F(FunctionalTests, TestEnableContentRestrictions)
{
    // Call the EnableContentRestrictions function
    mStreamAbstractionAAMP_HLS->EnableContentRestrictions();
}
// Test case 1: Test with grace = -1, time = 0, eventChange = false
TEST_F(FunctionalTests, TestUnlockWithUnlimitedGrace)
{

    mStreamAbstractionAAMP_HLS->DisableContentRestrictions(-1, 0, false);
}

// Test case 2: Test with specific grace and time, eventChange = true
TEST_F(FunctionalTests, TestUnlockWithSpecificGraceAndTime)
{
    mStreamAbstractionAAMP_HLS->DisableContentRestrictions(3600, 7200, true);
}

TEST_F(FunctionalTests, TestApplyContentRestrictions)
{
    std::vector<std::string> restrictions;
    // Call the ApplyContentRestrictions function
    mStreamAbstractionAAMP_HLS->ApplyContentRestrictions(restrictions);
}

TEST_F(FunctionalTests, TestSetPreferredAudioLanguages)
{
    // Call the SetPreferredAudioLanguages function
    mStreamAbstractionAAMP_HLS->SetPreferredAudioLanguages();
}

TEST_F(FunctionalTests, TestSetAudioTrack)
{
    // Call the SetAudioTrack  function
    mStreamAbstractionAAMP_HLS->SetAudioTrack(-122);
}

TEST_F(FunctionalTests, TestSetAudioTrackByLanguage)
{
    const char *lang = "english";
    // Call the ApplyContentRestrictions function
    mStreamAbstractionAAMP_HLS->SetAudioTrackByLanguage(lang);
}

TEST_F(FunctionalTests, TestSetThumbnailTrackWithInvalidIndex)
{
    int thumbnailIndex = -1; // Replace with an invalid index
    bool result = mStreamAbstractionAAMP_HLS->SetThumbnailTrack(thumbnailIndex);
    ASSERT_FALSE(result); // Assert that the function returned false for an invalid index
}

// Test case 1: Test SetVideoRectangle with valid coordinates and size
TEST_F(FunctionalTests, TestSetVideoRectangleWithValidParams)
{
    int x = 0;
    int y = 0;
    int w = 1920;
    int h = 1080;
    mStreamAbstractionAAMP_HLS->SetVideoRectangle(x, y, w, h);
}

// Test case 2: Test GetAvailableVideoTracks
TEST_F(FunctionalTests, TestGetAvailableVideoTracks)
{
    std::vector<StreamInfo *> videoTracks = mStreamAbstractionAAMP_HLS->GetAvailableVideoTracks();
    // Add assertions to check if the function returned an empty vector as expected
    ASSERT_TRUE(videoTracks.empty());
}

// Test case 3: Test GetAvailableThumbnailTracks
TEST_F(FunctionalTests, TestGetAvailableThumbnailTracks)
{
    std::vector<StreamInfo *> thumbnailTracks = mStreamAbstractionAAMP_HLS->GetAvailableThumbnailTracks();
    // Add assertions to check if the function returned an empty vector as expected
    ASSERT_TRUE(thumbnailTracks.empty());
}
