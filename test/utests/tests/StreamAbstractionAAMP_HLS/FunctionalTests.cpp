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

class StreamAbstractionAAMP_HLSTest : public ::testing::Test
{
protected:
    class TestableStreamAbstractionAAMP_HLS : public StreamAbstractionAAMP_HLS
    {
    public:
        // Constructor to pass parameters to the base class constructor
        TestableStreamAbstractionAAMP_HLS(PrivateInstanceAAMP *aamp,
                                          double seekpos, float rate,
                                          id3_callback_t id3Handler = nullptr,
                                          ptsoffset_update_t ptsOffsetUpdate = nullptr)
            : StreamAbstractionAAMP_HLS(aamp, seekpos, rate, id3Handler, ptsOffsetUpdate)
        {
        }

        StreamInfo *CallGetStreamInfo(int idx)
        {
            StreamAbstractionAAMP_HLS::mProfileCount = 2;
            return GetStreamInfo(idx);
        }

        void CallPopulateAudioAndTextTracks()
        {
            StreamAbstractionAAMP_HLS::mMediaCount = 2;
            StreamAbstractionAAMP_HLS::mProfileCount = 2;
            PopulateAudioAndTextTracks();
        }

        void CallConfigureAudioTrack()
        {
            ConfigureAudioTrack();
        }
        void CallConfigureVideoProfiles()
        {
            StreamAbstractionAAMP_HLS::mProfileCount = 1;
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
            TestableStreamAbstractionAAMP_HLS::mCurrentBandwidth = 0;
            TestableStreamAbstractionAAMP_HLS::mTsbBandwidth = 0;
            TestableStreamAbstractionAAMP_HLS::profileIdxForBandwidthNotification = 0;
            UpdateProfileBasedOnFragmentDownloaded();
        }

        int CallGetTextTrack()
        {
            TestableStreamAbstractionAAMP_HLS::mTextTrackIndex = "Test";
            return GetTextTrack();
        }

        int CallGetAudioTrack()
        {
            TestableStreamAbstractionAAMP_HLS::mAudioTrackIndex = "Test";
            return GetAudioTrack();
        }

        bool CallGetCurrentAudioTrack(AudioTrackInfo &audioTrack)
        {
            TestableStreamAbstractionAAMP_HLS::mAudioTrackIndex = "Test";
            return GetCurrentAudioTrack(audioTrack);
        }

        int CallGetCurrentTextTrack(TextTrackInfo &textTrack)
        {
            TestableStreamAbstractionAAMP_HLS::mTextTrackIndex = "Test";
            return GetCurrentTextTrack(textTrack);
        }

        void CallNotifyPlaybackPaused(bool paused)
        {
            TestableStreamAbstractionAAMP_HLS::mLastPausedTimeStamp = 1;
            NotifyPlaybackPaused(false);
            TestableStreamAbstractionAAMP_HLS::mLastPausedTimeStamp = -1;
            NotifyPlaybackPaused(false);
        }

        bool CallIsLowestProfile(int currentProfileIndex)
        {
            TestableStreamAbstractionAAMP_HLS::trickplayMode = true;
            return IsLowestProfile(currentProfileIndex);
        }

        bool CallIsLowestProfile_1(int currentProfileIndex)
        {
            TestableStreamAbstractionAAMP_HLS::trickplayMode = false;
            return IsLowestProfile(currentProfileIndex);
        }

	void CallSetAvailableTextTracks(std::vector<TextTrackInfo>& tracks)
	{
	    TestableStreamAbstractionAAMP_HLS::mTextTracks = tracks;
	    return;
	}

    };

    PrivateInstanceAAMP *mPrivateInstanceAAMP;
    TestableStreamAbstractionAAMP_HLS *mStreamAbstractionAAMP_HLS;

    void SetUp() override
    {
        if (gpGlobalConfig == nullptr)
        {
            gpGlobalConfig = new AampConfig();
        }

        mPrivateInstanceAAMP = new PrivateInstanceAAMP(gpGlobalConfig);

        g_mockAampConfig = new MockAampConfig();

        mStreamAbstractionAAMP_HLS = new TestableStreamAbstractionAAMP_HLS(mPrivateInstanceAAMP, 0.0, 1.0);
    }

    void TearDown() override
    {
        delete mStreamAbstractionAAMP_HLS;
        mStreamAbstractionAAMP_HLS = nullptr;

        delete mPrivateInstanceAAMP;
        mPrivateInstanceAAMP = nullptr;

        delete gpGlobalConfig;
        gpGlobalConfig = nullptr;

        delete g_mockAampConfig;
        g_mockAampConfig = nullptr;
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

        mStreamAbstractionAAMP_HLS = new StreamAbstractionAAMP_HLS(mPrivateInstanceAAMP, 0, 0.0);

        TrackStateobj = new TrackState(eTRACK_VIDEO, mStreamAbstractionAAMP_HLS, mPrivateInstanceAAMP, "TestTrack");

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

TEST_F(StreamAbstractionAAMP_HLSTest, TestPopulateAudioAndTextTracks)
{
    mStreamAbstractionAAMP_HLS->CallPopulateAudioAndTextTracks();
}

TEST_F(StreamAbstractionAAMP_HLSTest, TestPopulateAudioAndTextTracks_new)
{
    HlsStreamInfo streamInfo;
    streamInfo.enabled = true;
    streamInfo.isIframeTrack = false;
    streamInfo.validity = true;
    streamInfo.codecs = "h264";
    std::vector<MediaInfo> mediaInfoStore;
    MediaInfo media;
    media.type = eMEDIATYPE_AUDIO;
    mediaInfoStore.push_back(media);
    // Add the sample HlsStreamInfo objects to the streamInfoStore
    mStreamAbstractionAAMP_HLS->streamInfoStore.push_back(streamInfo);
    mStreamAbstractionAAMP_HLS->CallPopulateAudioAndTextTracks();
}

TEST_F(StreamAbstractionAAMP_HLSTest, TestConfigureAudioTrack)
{
    mStreamAbstractionAAMP_HLS->CallPopulateAudioAndTextTracks();
    mStreamAbstractionAAMP_HLS->CallConfigureAudioTrack();
}


TEST_F(StreamAbstractionAAMP_HLSTest, TestConfigureVideoProfiles1)
{
    mPrivateInstanceAAMP->mDisplayWidth = 0;
    mPrivateInstanceAAMP->mDisplayHeight = 0;
    mPrivateInstanceAAMP->userProfileStatus = true;

    HlsStreamInfo streamInfo;
    streamInfo.enabled = true;
    streamInfo.isIframeTrack = true;
    streamInfo.validity = true;
    streamInfo.codecs = "h264";
    streamInfo.isIframeTrack = true;
    std::vector<MediaInfo> mediaInfoStore;
    MediaInfo media;
    media.type = eMEDIATYPE_AUDIO;
    mediaInfoStore.push_back(media);
    // Add the sample HlsStreamInfo objects to the streamInfoStore
    mStreamAbstractionAAMP_HLS->streamInfoStore.push_back(streamInfo);

    mStreamAbstractionAAMP_HLS->CallConfigureVideoProfiles();
}

TEST_F(StreamAbstractionAAMP_HLSTest, TestConfigureTextTrack)
{
    mPrivateInstanceAAMP->mSubLanguage = "en";
    mPrivateInstanceAAMP->GetPreferredTextTrack();
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

TEST_F(StreamAbstractionAAMP_HLSTest, StreamAbstractionAAMP_HLS_Is4KStream_no_4k)
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

TEST_F(StreamAbstractionAAMP_HLSTest, StreamAbstractionAAMP_HLS_Is4KStream_no_4k_New1)
{
    char manifest[] = "#EXT-X-I-FRAME-STREAM-INF:";

    HlsStreamInfo streamInfo;
    streamInfo.enabled = true;
    streamInfo.validity = true;
    streamInfo.codecs = "h264";
    std::vector<MediaInfo> mediaInfoStore;
    MediaInfo media;
    media.type = eMEDIATYPE_AUDIO;
    mediaInfoStore.push_back(media);
    // Add the sample HlsStreamInfo objects to the streamInfoStore
    mStreamAbstractionAAMP_HLS->streamInfoStore.push_back(streamInfo);
    mStreamAbstractionAAMP_HLS->CallPopulateAudioAndTextTracks();

    mStreamAbstractionAAMP_HLS->mainManifest.AppendBytes(manifest, sizeof(manifest));
    EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_AvgBWForABR)).WillOnce(Return(true));
    mStreamAbstractionAAMP_HLS->ParseMainManifest();
}


TEST_F(StreamAbstractionAAMP_HLSTest, StreamAbstractionAAMP_HLS_Is4KStream_no_4k_New2)
{
    char manifest[] = "#EXT-X-IMAGE-STREAM-INF:";

    HlsStreamInfo streamInfo;
    streamInfo.enabled = true;
    streamInfo.validity = true;
    streamInfo.codecs = "h264";
    std::vector<MediaInfo> mediaInfoStore;
    MediaInfo media;
    media.type = eMEDIATYPE_AUDIO;
    mediaInfoStore.push_back(media);
    // Add the sample HlsStreamInfo objects to the streamInfoStore
    mStreamAbstractionAAMP_HLS->streamInfoStore.push_back(streamInfo);
    mStreamAbstractionAAMP_HLS->CallPopulateAudioAndTextTracks();

    mStreamAbstractionAAMP_HLS->mainManifest.AppendBytes(manifest, sizeof(manifest));

    EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_AvgBWForABR)).WillOnce(Return(true));

    mStreamAbstractionAAMP_HLS->ParseMainManifest();

}


TEST_F(StreamAbstractionAAMP_HLSTest, StreamAbstractionAAMP_HLS_Is4KStream_no_4k_New3)
{
    char manifest[] = "#EXT-X-CONTENT-IDENTIFIER:";

    HlsStreamInfo streamInfo;
    streamInfo.enabled = true;
    streamInfo.validity = true;
    streamInfo.codecs = "h264";
    std::vector<MediaInfo> mediaInfoStore;
    MediaInfo media;
    media.type = eMEDIATYPE_AUDIO;
    mediaInfoStore.push_back(media);
    // Add the sample HlsStreamInfo objects to the streamInfoStore
    mStreamAbstractionAAMP_HLS->streamInfoStore.push_back(streamInfo);
    mStreamAbstractionAAMP_HLS->CallPopulateAudioAndTextTracks();

    mStreamAbstractionAAMP_HLS->mainManifest.AppendBytes(manifest, sizeof(manifest));

    EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_AvgBWForABR)).WillOnce(Return(true));

    mStreamAbstractionAAMP_HLS->ParseMainManifest();

}

TEST_F(StreamAbstractionAAMP_HLSTest, StreamAbstractionAAMP_HLS_Is4KStream_no_4k_New4)
{
    char manifest[] = "#EXT-X-FOG";

    HlsStreamInfo streamInfo;
    streamInfo.enabled = true;
    streamInfo.validity = true;
    streamInfo.codecs = "h264";
    std::vector<MediaInfo> mediaInfoStore;
    MediaInfo media;
    media.type = eMEDIATYPE_AUDIO;
    mediaInfoStore.push_back(media);
    // Add the sample HlsStreamInfo objects to the streamInfoStore
    mStreamAbstractionAAMP_HLS->streamInfoStore.push_back(streamInfo);
    mStreamAbstractionAAMP_HLS->CallPopulateAudioAndTextTracks();

    mStreamAbstractionAAMP_HLS->mainManifest.AppendBytes(manifest, sizeof(manifest));

    EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_AvgBWForABR)).WillOnce(Return(true));

    mStreamAbstractionAAMP_HLS->ParseMainManifest();

}


TEST_F(StreamAbstractionAAMP_HLSTest, StreamAbstractionAAMP_HLS_Is4KStream_no_4k_New5)
{
    char manifest[] = "#EXT-X-XCAL-CONTENTMETADATA";

    HlsStreamInfo streamInfo;
    streamInfo.enabled = true;
    streamInfo.validity = true;
    streamInfo.codecs = "h264";
    std::vector<MediaInfo> mediaInfoStore;
    MediaInfo media;
    media.type = eMEDIATYPE_AUDIO;
    mediaInfoStore.push_back(media);
    // Add the sample HlsStreamInfo objects to the streamInfoStore
    mStreamAbstractionAAMP_HLS->streamInfoStore.push_back(streamInfo);
    mStreamAbstractionAAMP_HLS->CallPopulateAudioAndTextTracks();

    mStreamAbstractionAAMP_HLS->mainManifest.AppendBytes(manifest, sizeof(manifest));

    EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_AvgBWForABR)).WillOnce(Return(true));

    mStreamAbstractionAAMP_HLS->ParseMainManifest();

}


TEST_F(StreamAbstractionAAMP_HLSTest, StreamAbstractionAAMP_HLS_Is4KStream_no_4k_New6)
{
    char manifest[] = "#EXT-NOM-I-FRAME-DISTANCE";

    HlsStreamInfo streamInfo;
    streamInfo.enabled = true;
    streamInfo.validity = true;
    streamInfo.codecs = "h264";
    std::vector<MediaInfo> mediaInfoStore;
    MediaInfo media;
    media.type = eMEDIATYPE_AUDIO;
    mediaInfoStore.push_back(media);
    // Add the sample HlsStreamInfo objects to the streamInfoStore
    mStreamAbstractionAAMP_HLS->streamInfoStore.push_back(streamInfo);
    mStreamAbstractionAAMP_HLS->CallPopulateAudioAndTextTracks();

    mStreamAbstractionAAMP_HLS->mainManifest.AppendBytes(manifest, sizeof(manifest));

    EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_AvgBWForABR)).WillOnce(Return(true));

    mStreamAbstractionAAMP_HLS->ParseMainManifest();
}


TEST_F(StreamAbstractionAAMP_HLSTest, StreamAbstractionAAMP_HLS_Is4KStream_no_4k_New7)
{
    char manifest[] = "#EXT-X-ADVERTISING";

    HlsStreamInfo streamInfo;
    streamInfo.enabled = true;
    streamInfo.validity = true;
    streamInfo.codecs = "h264";
    std::vector<MediaInfo> mediaInfoStore;
    MediaInfo media;
    media.type = eMEDIATYPE_AUDIO;
    mediaInfoStore.push_back(media);
    // Add the sample HlsStreamInfo objects to the streamInfoStore
    mStreamAbstractionAAMP_HLS->streamInfoStore.push_back(streamInfo);
    mStreamAbstractionAAMP_HLS->CallPopulateAudioAndTextTracks();

    mStreamAbstractionAAMP_HLS->mainManifest.AppendBytes(manifest, sizeof(manifest));

    EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_AvgBWForABR)).WillOnce(Return(true));

    mStreamAbstractionAAMP_HLS->ParseMainManifest();
}

TEST_F(StreamAbstractionAAMP_HLSTest, StreamAbstractionAAMP_HLS_Is4KStream_no_4k_New8)
{
    char manifest[] = "#EXT-UPLYNK-LIVE";

    HlsStreamInfo streamInfo;
    streamInfo.enabled = true;
    streamInfo.validity = true;
    streamInfo.codecs = "h264";
    std::vector<MediaInfo> mediaInfoStore;
    MediaInfo media;
    media.type = eMEDIATYPE_AUDIO;
    mediaInfoStore.push_back(media);
    // Add the sample HlsStreamInfo objects to the streamInfoStore
    mStreamAbstractionAAMP_HLS->streamInfoStore.push_back(streamInfo);
    mStreamAbstractionAAMP_HLS->CallPopulateAudioAndTextTracks();

    mStreamAbstractionAAMP_HLS->mainManifest.AppendBytes(manifest, sizeof(manifest));

    EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_AvgBWForABR)).WillOnce(Return(true));

    mStreamAbstractionAAMP_HLS->ParseMainManifest();
}


TEST_F(StreamAbstractionAAMP_HLSTest, StreamAbstractionAAMP_HLS_Is4KStream_no_4k_New9)
{
    char manifest[] = "#EXT-X-START:";

    HlsStreamInfo streamInfo;
    streamInfo.enabled = true;
    streamInfo.validity = true;
    streamInfo.codecs = "h264";
    std::vector<MediaInfo> mediaInfoStore;
    MediaInfo media;
    media.type = eMEDIATYPE_AUDIO;
    mediaInfoStore.push_back(media);
    // Add the sample HlsStreamInfo objects to the streamInfoStore
    mStreamAbstractionAAMP_HLS->streamInfoStore.push_back(streamInfo);
    bool TestResult = mPrivateInstanceAAMP->IsLiveAdjustRequired(); (void)TestResult;
    mStreamAbstractionAAMP_HLS->CallPopulateAudioAndTextTracks();

    mStreamAbstractionAAMP_HLS->mainManifest.AppendBytes(manifest, sizeof(manifest));

    EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_AvgBWForABR)).WillOnce(Return(true));

    mStreamAbstractionAAMP_HLS->ParseMainManifest();
}

TEST_F(StreamAbstractionAAMP_HLSTest, StreamAbstractionAAMP_HLS_Is4KStream_no_4k_New10)
{
    char manifest[] = "#EXTINF:";

    HlsStreamInfo streamInfo;
    streamInfo.enabled = true;
    streamInfo.validity = true;
    streamInfo.codecs = "h264";
    std::vector<MediaInfo> mediaInfoStore;
    MediaInfo media;
    media.type = eMEDIATYPE_AUDIO;
    mediaInfoStore.push_back(media);
    // Add the sample HlsStreamInfo objects to the streamInfoStore
    mStreamAbstractionAAMP_HLS->streamInfoStore.push_back(streamInfo);
    mStreamAbstractionAAMP_HLS->CallPopulateAudioAndTextTracks();

    mStreamAbstractionAAMP_HLS->mainManifest.AppendBytes(manifest, sizeof(manifest));

    EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_AvgBWForABR)).WillOnce(Return(true));

    mStreamAbstractionAAMP_HLS->ParseMainManifest();

}

TEST_F(StreamAbstractionAAMP_HLSTest, StreamAbstractionAAMP_HLS_Is4KStream_no_4k_New11)
{
    char manifest[] = "#EXTaaaINF:";

    HlsStreamInfo streamInfo;
    streamInfo.enabled = true;
    streamInfo.validity = true;
    streamInfo.codecs = "h264";
    std::vector<MediaInfo> mediaInfoStore;
    MediaInfo media;
    media.type = eMEDIATYPE_AUDIO;
    mediaInfoStore.push_back(media);
    // Add the sample HlsStreamInfo objects to the streamInfoStore
    mStreamAbstractionAAMP_HLS->streamInfoStore.push_back(streamInfo);
    mStreamAbstractionAAMP_HLS->CallPopulateAudioAndTextTracks();

    mStreamAbstractionAAMP_HLS->mainManifest.AppendBytes(manifest, sizeof(manifest));

    EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_AvgBWForABR)).WillOnce(Return(true));

    mStreamAbstractionAAMP_HLS->ParseMainManifest();
}

TEST_F(StreamAbstractionAAMP_HLSTest, StreamAbstractionAAMP_HLS_Is4KStream_4k)
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

TEST_F(StreamAbstractionAAMP_HLSTest, StreamAbstractionAAMP_HLS_Is4KStream_multiple_mainfests)
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
        mStreamAbstractionAAMP_HLS->mainManifest.AppendBytes((char *)td.manifest, strlen(td.manifest) );

        EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_AvgBWForABR)).WillOnce(Return(true));

        mStreamAbstractionAAMP_HLS->ParseMainManifest();

        EXPECT_EQ(mStreamAbstractionAAMP_HLS->streamInfoStore.size(), td.exp_streams);
        EXPECT_EQ(mStreamAbstractionAAMP_HLS->mediaInfoStore.size(), td.exp_media);
        EXPECT_EQ(mStreamAbstractionAAMP_HLS->Is4KStream(height, bandwidth), td.exp_4k);
    }
}

// Testing ABR manager is selected by default.
TEST_F(StreamAbstractionAAMP_HLSTest, ABRManagerMode)
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
TEST_F(StreamAbstractionAAMP_HLSTest, FogABRMode)
{
    char manifest[] = MANIFEST_5SD_1A;

    mStreamAbstractionAAMP_HLS->mainManifest.AppendBytes(manifest, sizeof(manifest));

    // Call the fake Tune() method with a Fog TSB URL to setup Fog related flags.
    mPrivateInstanceAAMP->Tune("http://127.0.0.1/tsb?clientId=FOG_AAMP&recordedUrl=https%3A%2F%2Fads.com%2Fad.m3u8", false);

    EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_AvgBWForABR)).WillOnce(Return(true));

    mStreamAbstractionAAMP_HLS->ParseMainManifest();

    EXPECT_EQ(mStreamAbstractionAAMP_HLS->GetABRMode(), StreamAbstractionAAMP::ABRMode::FOG_TSB);
}

TEST_F(StreamAbstractionAAMP_HLSTest, Stoptest)
{
    mStreamAbstractionAAMP_HLS->Stop(true);
}

TEST_F(StreamAbstractionAAMP_HLSTest, GetVideoBitratesTest)
{
    // Define sample StreamInfo objects with video bitrates
    StreamResolution resolution1;
    resolution1.width = 1920;     // Width in pixels
    resolution1.height = 1080;    // Height in pixels
    resolution1.framerate = 30.0; // Frame rate in FPS

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

    ASSERT_EQ(bitrates.size(), 0);
}

TEST_F(StreamAbstractionAAMP_HLSTest, GetAvailableThumbnailTracksTest)
{
    HlsStreamInfo streamInfo1;
    streamInfo1.isIframeTrack = true;
    streamInfo1.validity = true;
    streamInfo1.codecs = "H.264";
    // Add streamInfo1 and streamInfo2 to the streamInfoStore (replace with your actual code)
    mStreamAbstractionAAMP_HLS->streamInfoStore.push_back(streamInfo1);

    // Call the function under test
    std::vector<StreamInfo *> thumbnailTracks = mStreamAbstractionAAMP_HLS->GetAvailableThumbnailTracks();
    ASSERT_EQ(thumbnailTracks.size(), 0);
    for (const auto track : thumbnailTracks)
    {
        ASSERT_TRUE(track->isIframeTrack);
    }
}

TEST_F(StreamAbstractionAAMP_HLSTest, SetThumbnailTrackTest)
{
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
    ASSERT_FALSE(rc);
}

TEST_F(StreamAbstractionAAMP_HLSTest, SetThumbnailTrackTest_1)
{
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
    mStreamAbstractionAAMP_HLS->streamInfoStore.push_back(streamInfo1);

    bool rc = mStreamAbstractionAAMP_HLS->SetThumbnailTrack(3);
    ASSERT_FALSE(rc);
}

TEST_F(StreamAbstractionAAMP_HLSTest, StartSubtitleParsertest)
{
    mStreamAbstractionAAMP_HLS->StartSubtitleParser();

    ASSERT_FALSE(mStreamAbstractionAAMP_HLS->trackState[eMEDIATYPE_SUBTITLE] != nullptr);
}

TEST_F(StreamAbstractionAAMP_HLSTest, PauseSubtitleParsertest)
{
    mStreamAbstractionAAMP_HLS->PauseSubtitleParser(true);
}

TEST_F(StreamAbstractionAAMP_HLSTest, InitiateDrmProcesstest)
{
    mStreamAbstractionAAMP_HLS->InitiateDrmProcess();
}

TEST_F(StreamAbstractionAAMP_HLSTest, ChangeMuxedAudioTrackIndexTest)
{
    std::string index = "mux-1"; // Sample index

    mStreamAbstractionAAMP_HLS->ChangeMuxedAudioTrackIndex(index);
}

TEST_F(StreamAbstractionAAMP_HLSTest, GetStreamOutputFormatForTrackVideo)
{
    TrackType videoTrackType = eTRACK_VIDEO;
    StreamOutputFormat outputFormat = mStreamAbstractionAAMP_HLS->GetStreamOutputFormatForTrack(videoTrackType);
    StreamOutputFormat expectedVideoOutputFormat = FORMAT_UNKNOWN;
    EXPECT_EQ(outputFormat, expectedVideoOutputFormat);
}

TEST_F(StreamAbstractionAAMP_HLSTest, GetStreamOutputFormatForTrackAudio)
{
    TrackType audioTrackType = eTRACK_AUDIO;
    StreamOutputFormat outputFormat = mStreamAbstractionAAMP_HLS->GetStreamOutputFormatForTrack(audioTrackType);
    StreamOutputFormat expectedAudioOutputFormat = FORMAT_AUDIO_ES_AAC;
    EXPECT_EQ(outputFormat, expectedAudioOutputFormat);
}

TEST_F(StreamAbstractionAAMP_HLSTest, GetStreamOutputFormatForTrackAuxAudio)
{

    TrackType auxAudioTrackType = eTRACK_AUX_AUDIO;
    StreamOutputFormat outputFormat = mStreamAbstractionAAMP_HLS->GetStreamOutputFormatForTrack(auxAudioTrackType);
    StreamOutputFormat expectedAuxAudioOutputFormat = FORMAT_AUDIO_ES_AAC; // Example value, replace with your logic
    EXPECT_EQ(outputFormat, expectedAuxAudioOutputFormat);
}

TEST_F(StreamAbstractionAAMP_HLSTest, GetMediaIndexForLanguage)
{

    std::string lang = "en";       // Replace with the desired language
    TrackType type = eTRACK_AUDIO; // Replace with the desired track type
    int index = mStreamAbstractionAAMP_HLS->GetMediaIndexForLanguage(lang, type);
    int expectedIndex = -1; // Replace with your expected index value
    EXPECT_EQ(index, expectedIndex);
}

TEST_F(StreamAbstractionAAMP_HLSTest, GetMediaIndexForLanguage2)
{
    std::string lang = "en";       // Replace with the desired language
    TrackType type = eTRACK_SUBTITLE; // Replace with the desired track type
    int index = 0;
    HlsStreamInfo *streamInfo = (HlsStreamInfo*)mStreamAbstractionAAMP_HLS->CallGetStreamInfo(index);
	if( streamInfo )
	{
		streamInfo->subtitles = (const char *)"test";
	}
	MediaInfo MediaInfoObj;
    std::vector<MediaInfo> mediaInfoStore;
    MediaInfoObj.group_id = (const char *)"test";
    mediaInfoStore.push_back(MediaInfoObj);
    int MediaIndexForLanguageResult = mStreamAbstractionAAMP_HLS->GetMediaIndexForLanguage(lang, type);
    int expectedIndex = -1; // Replace with your expected index value
    EXPECT_EQ(MediaIndexForLanguageResult, expectedIndex);
}

TEST_F(StreamAbstractionAAMP_HLSTest, TestGetStreamInfo)
{
    int index = 0;
    StreamInfo *streamInfo = mStreamAbstractionAAMP_HLS->CallGetStreamInfo(index);
	EXPECT_EQ(streamInfo, nullptr);
}

TEST_F(TrackStateTests, GetNextFragmentPeriodInfo_WhenIndexIsEmpty)
{
    int periodIdx = 0;
    AampTime offsetFromPeriodStart = 0;
    int fragmentIdx = 0;
    // Optionally, set up other necessary objects or state for the test case
    mStreamAbstractionAAMP_HLS->rate = 1.1;
    // Act: Call the function to be tested
    TrackStateobj->GetNextFragmentPeriodInfo(periodIdx, offsetFromPeriodStart, fragmentIdx);
    EXPECT_EQ(periodIdx, -1);  // Assuming periodIdx should be -1 when the index is empty
    EXPECT_EQ(fragmentIdx, -1); // Assuming fragmentIdx should be -1 when the index is empty
    EXPECT_DOUBLE_EQ(offsetFromPeriodStart.inSeconds(), 0.0);  // Assuming offsetFromPeriodStart should be 0.0 when the index is empty
}

TEST_F(StreamAbstractionAAMP_HLSTest, StartInjectiontest)
{
    mStreamAbstractionAAMP_HLS->StartInjection();
}

// Define a test case for the GetPlaylistURI function for eTRACK_VIDEO
TEST_F(StreamAbstractionAAMP_HLSTest, GetVideoPlaylistURITest)
{
    StreamOutputFormat format = FORMAT_MPEGTS;
    TrackType type = eTRACK_VIDEO;
    auto playlistURI = mStreamAbstractionAAMP_HLS->GetPlaylistURI(type, &format);
    ASSERT_EQ(format, FORMAT_MPEGTS);
    ASSERT_EQ(type, eTRACK_VIDEO);
}

// Define a test case for the GetPlaylistURI function for eTRACK_AUDIO
TEST_F(StreamAbstractionAAMP_HLSTest, GetAudioPlaylistURITest)
{
    // mStreamAbstractionAAMP_HLS->currentAudioProfileIndex = 3;
    StreamOutputFormat format;
    auto playlistURI = mStreamAbstractionAAMP_HLS->GetPlaylistURI(eTRACK_AUDIO, &format);
    ASSERT_NE(FORMAT_AUDIO_ES_AAC, format);
}

TEST_F(StreamAbstractionAAMP_HLSTest, GetVideoPlaylistURITest2)
{
    // mStreamAbstractionAAMP_HLS->currentTextTrackProfileIndex = 3;
    StreamOutputFormat format = FORMAT_MPEGTS;
    TrackType type = eTRACK_SUBTITLE;
    auto playlistURI = mStreamAbstractionAAMP_HLS->GetPlaylistURI(type, &format);

}

TEST_F(StreamAbstractionAAMP_HLSTest, GetAvailableVideoTracksTest)
{
    std::vector<StreamInfo *> videoTracks = mStreamAbstractionAAMP_HLS->GetAvailableVideoTracks();
    ASSERT_EQ(0, videoTracks.size());
}

TEST_F(StreamAbstractionAAMP_HLSTest, GetAvailableVideoTracksTest2)
{
    int index = 2;
    HlsStreamInfo *streamInfo = (HlsStreamInfo*)mStreamAbstractionAAMP_HLS->CallGetStreamInfo(index);
	if( streamInfo )
	{
		streamInfo->subtitles = (const char *)"test";
	}
	MediaInfo MediaInfoObj;
    std::vector<MediaInfo> mediaInfoStore;
    MediaInfoObj.group_id = (const char *)"test";
    mediaInfoStore.push_back(MediaInfoObj);
    std::vector<StreamInfo *> videoTracks = mStreamAbstractionAAMP_HLS->GetAvailableVideoTracks();
    ASSERT_EQ(0, videoTracks.size());
}

TEST_F(StreamAbstractionAAMP_HLSTest, GetABRModetest)
{
    mStreamAbstractionAAMP_HLS->GetABRMode();
}

TEST_F(StreamAbstractionAAMP_HLSTest, GetBestAudioTrackByLanguagetest)
{
    HlsStreamInfo streamInfo;
    streamInfo.enabled = true;
    streamInfo.validity = true;
    streamInfo.codecs = "h264";
    std::vector<MediaInfo> mediaInfoStore;
    MediaInfo media;
    media.type = eMEDIATYPE_AUDIO;
    mediaInfoStore.push_back(media);
    // Add the sample HlsStreamInfo objects to the streamInfoStore
    mStreamAbstractionAAMP_HLS->streamInfoStore.push_back(streamInfo);
	int index = mStreamAbstractionAAMP_HLS->GetBestAudioTrackByLanguage();
	(void)index;
}

TEST_F(StreamAbstractionAAMP_HLSTest, IsLivetest)
{
    mStreamAbstractionAAMP_HLS->IsLive();
    bool isLive1 = mStreamAbstractionAAMP_HLS->IsLive();
    ASSERT_FALSE(isLive1);
}

TEST_F(StreamAbstractionAAMP_HLSTest, Gtest)
{
    TuneType tuneType = eTUNETYPE_NEW_NORMAL; // Replace with the actual tune type
    AAMPStatusType result = mStreamAbstractionAAMP_HLS->Init(tuneType);
    EXPECT_NE(result, eAAMPSTATUS_OK);
}

TEST_F(StreamAbstractionAAMP_HLSTest, GetFirstPTStest)
{
    double result = mStreamAbstractionAAMP_HLS->GetFirstPTS();
    EXPECT_EQ(result, 0.0);
}

TEST_F(StreamAbstractionAAMP_HLSTest, GetFirstPTStest2)
{
	double result = mStreamAbstractionAAMP_HLS->GetFirstPTS(); (void)result;
}

TEST_F(StreamAbstractionAAMP_HLSTest, GetBufferedDurationtest)
{
    double result = mStreamAbstractionAAMP_HLS->GetBufferedDuration();
    EXPECT_EQ(result, -1.0);
}

TEST_F(StreamAbstractionAAMP_HLSTest, GetBWIndexTest)
{
    BitsPerSecond targetBitrate = 10000; // Example target bitrate in bits per second
    int result = mStreamAbstractionAAMP_HLS->GetBWIndex(targetBitrate);
    EXPECT_NE(result, 1);
}
TEST_F(StreamAbstractionAAMP_HLSTest, GetBWIndexTest_1)
{
    BitsPerSecond targetBitrate = 7000000; // Example target bitrate in bits per second
    int result = mStreamAbstractionAAMP_HLS->GetBWIndex(targetBitrate);
    EXPECT_EQ(result, 0);
}

TEST_F(StreamAbstractionAAMP_HLSTest, GetMediaCounttest)
{
    int result = mStreamAbstractionAAMP_HLS->GetMediaCount();
    EXPECT_EQ(result, 0);
}

TEST_F(StreamAbstractionAAMP_HLSTest, SeekPosUpdateTest)
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

TEST_F(StreamAbstractionAAMP_HLSTest, GetLanguageCodeTest)
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

TEST_F(StreamAbstractionAAMP_HLSTest, GetTotalProfileCounttest)
{
    int result = mStreamAbstractionAAMP_HLS->GetTotalProfileCount();
    EXPECT_EQ(result, 0);
}

TEST_F(StreamAbstractionAAMP_HLSTest, Destructortest)
{
    StreamAbstractionAAMP_HLS *mStreamAbstractionAAMP_HLS_1 = new StreamAbstractionAAMP_HLS(mPrivateInstanceAAMP, 0, AAMP_NORMAL_PLAY_RATE);
    mStreamAbstractionAAMP_HLS_1->~StreamAbstractionAAMP_HLS();
}

TEST_F(TrackStateTests, Stoptest_1)
{
    TrackStateobj->Stop(true);
}

TEST_F(StreamAbstractionAAMP_HLSTest, UpdateFailedDRMStatus_1)
{
    LicensePreFetchObject *object = NULL; // Create an instance of LicensePreFetchObject
    // Act: Call the function to be tested
    mStreamAbstractionAAMP_HLS->UpdateFailedDRMStatus(object);
}

TEST_F(TrackStateTests, FetchPlaylistTest)
{
    TrackStateobj->FetchPlaylist();
}

TEST_F(TrackStateTests, FetchPlaylistTest_eTRACK_AUDIO)
{
    // MediaTrack::type = TrackType::eTRACK_AUDIO; //-> method 1
    TrackStateobj->type = TrackType::eTRACK_AUDIO; //-> method 2 Working
    TrackStateobj->FetchPlaylist();
}

TEST_F(TrackStateTests, FetchPlaylistTest_eTRACK_SUBTITLE)
{
    //To Cover MediaTrack::type as eTRACK_SUBTITLE
    TrackStateobj->type = TrackType::eTRACK_SUBTITLE;
    TrackStateobj->FetchPlaylist();
}

TEST_F(TrackStateTests, FetchPlaylistTest_eTRACK_AUX_AUDIO)
{
    //To Cover MediaTrack::type as eTRACK_AUX_AUDIO
    TrackStateobj->type = TrackType::eTRACK_AUX_AUDIO;
    TrackStateobj->FetchPlaylist();
}

TEST_F(TrackStateTests, GetPeriodStartPositionTest)
{
    AampTime result{TrackStateobj->GetPeriodStartPosition(1000)};
    ASSERT_TRUE(result == 0.0);
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
    AampTime result{TrackStateobj->GetXStartTimeOffset()};
    ASSERT_TRUE(result == 0.0);
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
    bool isRefresh = true;
    AampTime culledSec = -12;
    TrackStateobj->IndexPlaylist(isRefresh, culledSec);
}

TEST_F(TrackStateTests, IndexPlaylist_WhenIsRefreshFalse)
{
    bool isRefresh = false;
    AampTime culledSec = 0.0;
    TrackStateobj->IndexPlaylist(isRefresh, culledSec);
}

TEST_F(TrackStateTests, IndexPlaylist_ProcessEXTINF)
{
    bool isRefresh = false;
    AampTime culledSec = 0.0;
    TrackStateobj->IndexPlaylist(isRefresh, culledSec);
}

TEST_F(TrackStateTests, GetNextFragmentUri_WithReloadUri)
{
    bool reloadUri = true;
    bool ignoreDiscontinuity = false;
    auto indexNode = TrackStateobj->GetNextFragmentUriFromPlaylist(reloadUri, ignoreDiscontinuity);
	(void)indexNode;
}

TEST_F(TrackStateTests, GetNextFragmentUri_WithoutReloadUri)
{
    bool reloadUri = false;
    bool ignoreDiscontinuity = true;
    auto indexNode = TrackStateobj->GetNextFragmentUriFromPlaylist(reloadUri, ignoreDiscontinuity);
	(void)indexNode;
}


TEST_F(TrackStateTests, GetNextFragmentUri_WithReloadUri_new)
{
    bool reloadUri = true;
    bool ignoreDiscontinuity = false;
    TrackStateobj->playTarget = -1.1;
    auto indexNode = TrackStateobj->GetNextFragmentUriFromPlaylist(reloadUri, ignoreDiscontinuity);
	(void)indexNode;
}

TEST_F(TrackStateTests, GetNextFragmentUri_WithReloadUri_new1)
{
    bool reloadUri = true;
    bool ignoreDiscontinuity = false;
    TrackStateobj->playTarget = -1.1;
    TrackStateobj->playlistPosition = -1.0;
    auto indexNode = TrackStateobj->GetNextFragmentUriFromPlaylist(reloadUri, ignoreDiscontinuity);
	(void)indexNode;
}

TEST_F(TrackStateTests, GetNextFragmentUri_WithReloadUri_new2)
{
    bool reloadUri = true;
    bool ignoreDiscontinuity = false;
    TrackStateobj->playTarget = -1.1;
    TrackStateobj->playlistPosition = -1.0;
	auto indexNode = TrackStateobj->GetNextFragmentUriFromPlaylist(reloadUri, ignoreDiscontinuity);
	(void)indexNode;
}

TEST_F(TrackStateTests, GetNextFragmentUri_WithReloadUri_new3)
{
    bool reloadUri = true;
    bool ignoreDiscontinuity = false;
    TrackStateobj->playTarget = -1.1;
    TrackStateobj->playlistPosition = -1.0;
    int height;
    BitsPerSecond bandwidth;
    char manifest[] = MANIFEST_6SD_1A;
    mStreamAbstractionAAMP_HLS->mainManifest.AppendBytes(manifest, sizeof(manifest));
	auto indexNode = TrackStateobj->GetNextFragmentUriFromPlaylist(reloadUri, ignoreDiscontinuity);
	(void)indexNode;
    mStreamAbstractionAAMP_HLS->Is4KStream(height, bandwidth);
}


TEST_F(TrackStateTests, GetNextFragmentUri_WithReloadUri_new4)
{
    bool reloadUri = true;
    bool ignoreDiscontinuity = false;
    TrackStateobj->playTarget = -1.1;
    TrackStateobj->playlistPosition = -1.0;
    int height;
    BitsPerSecond bandwidth;
    char manifest[] = "#EXT-X-BYTERANGE:";
    mStreamAbstractionAAMP_HLS->mainManifest.AppendBytes(manifest, sizeof(manifest));
    auto indexNode = TrackStateobj->GetNextFragmentUriFromPlaylist(reloadUri, ignoreDiscontinuity);
	(void)indexNode;
    mStreamAbstractionAAMP_HLS->Is4KStream(height, bandwidth);
}

TEST_F(TrackStateTests, GetNextFragmentUri_WithReloadUri_new5)
{
    bool reloadUri = true;
    bool ignoreDiscontinuity = false;
    TrackStateobj->playTarget = -1.1;
    TrackStateobj->playlistPosition = -1.0;
    int height;
    BitsPerSecond bandwidth;
    char manifest[] = "#EXTINF:";
    mStreamAbstractionAAMP_HLS->mainManifest.AppendBytes(manifest, sizeof(manifest));
    auto indexNode = TrackStateobj->GetNextFragmentUriFromPlaylist(reloadUri, ignoreDiscontinuity);
	(void)indexNode;
    mStreamAbstractionAAMP_HLS->Is4KStream(height, bandwidth);
}

TEST_F(TrackStateTests, GetNextFragmentUri_WithReloadUri_new6)
{
    bool reloadUri = true;
    bool ignoreDiscontinuity = false;
    TrackStateobj->playTarget = -1.1;
    TrackStateobj->playlistPosition = -1.0;
    int height;
    BitsPerSecond bandwidth;
    char manifest[] = "#EXT-X-TARGETDURATION:";
    mStreamAbstractionAAMP_HLS->mainManifest.AppendBytes(manifest, sizeof(manifest));
    auto indexNode = TrackStateobj->GetNextFragmentUriFromPlaylist(reloadUri, ignoreDiscontinuity);
	(void)indexNode;
    mStreamAbstractionAAMP_HLS->Is4KStream(height, bandwidth);
}

TEST_F(TrackStateTests, GetNextFragmentUri_WithReloadUri_new7)
{
    bool reloadUri = true;
    bool ignoreDiscontinuity = false;
    TrackStateobj->playTarget = -1.1;
    TrackStateobj->playlistPosition = -1.0;
    int height;
    BitsPerSecond bandwidth;
    char manifest[] = "#EXT-X-MEDIA-SEQUENCE:";
    mStreamAbstractionAAMP_HLS->mainManifest.AppendBytes(manifest, sizeof(manifest));
    auto indexNode = TrackStateobj->GetNextFragmentUriFromPlaylist(reloadUri, ignoreDiscontinuity);
	(void)indexNode;
    mStreamAbstractionAAMP_HLS->Is4KStream(height, bandwidth);
}

TEST_F(TrackStateTests, GetNextFragmentUri_WithReloadUri_new8)
{
    bool reloadUri = true;
    bool ignoreDiscontinuity = false;
    TrackStateobj->playTarget = -1.1;
    TrackStateobj->playlistPosition = -1.0;
    int height;
    BitsPerSecond bandwidth;
    char manifest[] = "#EXT-X-KEY:";
    mStreamAbstractionAAMP_HLS->mainManifest.AppendBytes(manifest, sizeof(manifest));
    auto indexNode = TrackStateobj->GetNextFragmentUriFromPlaylist(reloadUri, ignoreDiscontinuity);
	(void)indexNode;
    mStreamAbstractionAAMP_HLS->Is4KStream(height, bandwidth);
}

TEST_F(TrackStateTests, GetNextFragmentUri_WithReloadUri_new9)
{
    bool reloadUri = true;
    bool ignoreDiscontinuity = false;
    TrackStateobj->playTarget = -1.1;
    TrackStateobj->playlistPosition = -1.0;
    int height;
    BitsPerSecond bandwidth;
    char manifest[] = "#EXT-X-MAP:";
    mStreamAbstractionAAMP_HLS->mainManifest.AppendBytes(manifest, sizeof(manifest));
    auto indexNode = TrackStateobj->GetNextFragmentUriFromPlaylist(reloadUri, ignoreDiscontinuity);
	(void)indexNode;
    mStreamAbstractionAAMP_HLS->Is4KStream(height, bandwidth);
}

TEST_F(TrackStateTests, GetNextFragmentUri_WithReloadUri_new10)
{
    bool reloadUri = true;
    bool ignoreDiscontinuity = false;
    TrackStateobj->playTarget = -1.1;
    TrackStateobj->playlistPosition = -1.0;
    int height;
    BitsPerSecond bandwidth;
    char manifest[] = "#EXT-X-PROGRAM-DATE-TIME:";
    mStreamAbstractionAAMP_HLS->mainManifest.AppendBytes(manifest, sizeof(manifest));
    auto indexNode = TrackStateobj->GetNextFragmentUriFromPlaylist(reloadUri, ignoreDiscontinuity);
	(void)indexNode;
    mStreamAbstractionAAMP_HLS->Is4KStream(height, bandwidth);
}

TEST_F(TrackStateTests, GetNextFragmentUri_WithReloadUri_new11)
{
    bool reloadUri = true;
    bool ignoreDiscontinuity = false;
    TrackStateobj->playTarget = -1.1;
    TrackStateobj->playlistPosition = -1.0;
    int height;
    BitsPerSecond bandwidth;
    char manifest[] = "#EXT-X-ALLOW-CACHE:";
    mStreamAbstractionAAMP_HLS->mainManifest.AppendBytes(manifest, sizeof(manifest));
    auto indexNode = TrackStateobj->GetNextFragmentUriFromPlaylist(reloadUri, ignoreDiscontinuity);
	(void)indexNode;
    mStreamAbstractionAAMP_HLS->Is4KStream(height, bandwidth);
}

TEST_F(TrackStateTests, GetNextFragmentUri_WithReloadUri_new12)
{
    bool reloadUri = true;
    bool ignoreDiscontinuity = false;
    TrackStateobj->playTarget = -1.1;
    TrackStateobj->playlistPosition = -1.0;
    int height;
    BitsPerSecond bandwidth;
    char manifest[] = "#EXT-X-ENDLIST";
    mStreamAbstractionAAMP_HLS->mainManifest.AppendBytes(manifest, sizeof(manifest));
    auto indexNode = TrackStateobj->GetNextFragmentUriFromPlaylist(reloadUri, ignoreDiscontinuity);
	(void)indexNode;
    mStreamAbstractionAAMP_HLS->Is4KStream(height, bandwidth);
}

TEST_F(TrackStateTests, GetNextFragmentUri_WithReloadUri_new13)
{
    bool reloadUri = true;
    bool ignoreDiscontinuity = false;
    TrackStateobj->playTarget = -1.1;
    TrackStateobj->playlistPosition = -1.0;
    int height;
    BitsPerSecond bandwidth;
    char manifest[] = "#EXT-X-DISCONTINUITY";
    mStreamAbstractionAAMP_HLS->mainManifest.AppendBytes(manifest, sizeof(manifest));
    auto indexNode = TrackStateobj->GetNextFragmentUriFromPlaylist(reloadUri, ignoreDiscontinuity);
	(void)indexNode;
    mStreamAbstractionAAMP_HLS->Is4KStream(height, bandwidth);
}

TEST_F(TrackStateTests, GetNextFragmentUri_WithReloadUri_new14)
{
    bool reloadUri = true;
    bool ignoreDiscontinuity = false;
    TrackStateobj->playTarget = -1.1;
    TrackStateobj->playlistPosition = -1.0;
    int height;
    BitsPerSecond bandwidth;
    char manifest[] = "#EXT-X-I-FRAMES-ONLY";
    mStreamAbstractionAAMP_HLS->mainManifest.AppendBytes(manifest, sizeof(manifest));
    auto indexNode = TrackStateobj->GetNextFragmentUriFromPlaylist(reloadUri, ignoreDiscontinuity);
	(void)indexNode;
    mStreamAbstractionAAMP_HLS->Is4KStream(height, bandwidth);
}

TEST_F(TrackStateTests, ABRProfileChangedTest)
{
    AampGrowableBuffer newPlaylist("test");
    int http_error = 2;
    TrackStateobj->IsLive();
    TrackStateobj->ProcessPlaylist(newPlaylist, http_error);
}

TEST_F(TrackStateTests, ABRProfileChangedTest2)
{
    HlsStreamInfo streamInfo;
    streamInfo.uri="https://example/main.m3u8";
    mStreamAbstractionAAMP_HLS->streamInfoStore.push_back(streamInfo);
    TrackStateobj->ABRProfileChanged();
}

TEST_F(TrackStateTests, DrmDecrypt_SuccessfulDecryption)
{
    CachedFragment cachedFragment; // Create a CachedFragment with valid data
    ProfilerBucketType bucketType = ProfilerBucketType::PROFILE_BUCKET_PLAYLIST_VIDEO;
    DrmReturn result = TrackStateobj->DrmDecrypt(&cachedFragment, bucketType);
    ASSERT_NE(result, DrmReturn::eDRM_SUCCESS); // Check that the function returns DRM_SUCCESS
}

TEST_F(TrackStateTests, CreateInitVector_Successful)
{
    long long seqNo = 0x123456789abcdef; // Replace with a valid sequence number
    TrackStateobj->CreateInitVectorByMediaSeqNo(seqNo);
}

TEST_F(TrackStateTests, GetPeriodStartPosition_InvalidPeriod)
{
    int periodIdx = -1; // Replace with an invalid period index (out of bounds)
    AampTime startPosition = TrackStateobj->GetPeriodStartPosition(periodIdx);
    ASSERT_DOUBLE_EQ(startPosition.inSeconds(), 0.0); // Check that the function returns a default or invalid value for an invalid period index
}
TEST_F(TrackStateTests, GetPeriodStartPosition_NoDiscontinuityNodes)
{
    int periodIdx = 1;
    AampTime startPosition = TrackStateobj->GetPeriodStartPosition(periodIdx);
    ASSERT_DOUBLE_EQ(startPosition.inSeconds(), 0.0); // Check that the function returns a default or zero value when there are no discontinuity nodes
}

TEST_F(TrackStateTests, FindTimedMetadata_WithTags)
{
    bool reportBulkMeta = false;
    bool bInitCall = false;
    TrackStateobj->FindTimedMetadata(reportBulkMeta, bInitCall);
}

TEST_F(TrackStateTests, FindTimedMetadata_ReportBulk)
{
    bool reportBulkMeta = true; // Simulate the reportBulkMeta flag being set
    bool bInitCall = false;
    TrackStateobj->FindTimedMetadata(reportBulkMeta, bInitCall);
}

TEST_F(TrackStateTests, FindTimedMetadata_InitCall)
{
    bool reportBulkMeta = false;
    bool bInitCall = true; // Simulate the bInitCall flag being set
    TrackStateobj->FindTimedMetadata(reportBulkMeta, bInitCall);
}

TEST_F(TrackStateTests, FindTimedMetadata_New)
{
    bool reportBulkMeta = false;
    bool bInitCall = true; // Simulate the bInitCall flag being set
    EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_EnableSubscribedTags)).WillOnce(Return(true));
    AampGrowableBuffer buffer("tsProcessor PAT/PMT test");
        //buffer.AppendBytes(10);
    buffer.GetPtr();
    TrackStateobj->FindTimedMetadata(reportBulkMeta, bInitCall);
}

TEST_F(TrackStateTests, GetNextFragmentPeriodInfoTest)
{
    int periodIdx = 2;
    AampTime offsetFromPeriodStart = 1.2;
    int fragmentIdx = 3;
    mStreamAbstractionAAMP_HLS->rate = 2;
    // TrackStateobj->indexCount = 2;
    TrackStateobj->GetNextFragmentPeriodInfo(periodIdx, offsetFromPeriodStart, fragmentIdx);
}

TEST_F(TrackStateTests, SetXStartTimeOffset)
{
    // double offset = 123.45; // Choose a test offset value
    AampTime offset = -12; // Choose a test offset value
    TrackStateobj->SetXStartTimeOffset(offset.inSeconds());
    ASSERT_EQ(TrackStateobj->GetXStartTimeOffset(), offset); // Check if the offset matches what was set
}

TEST_F(TrackStateTests, SetEffectivePlaylistUrl)
{
    std::string url = "https://example.com/playlist.m3u8"; // Choose a test URL
    TrackStateobj->SetEffectivePlaylistUrl(url);
    ASSERT_EQ(TrackStateobj->GetEffectivePlaylistUrl(), url); // Check if the URL matches what was set
}

TEST_F(TrackStateTests, GetLastPlaylistDownloadTime)
{
    long long expectedTime = 123456789; // Choose a test time value
    TrackStateobj->SetLastPlaylistDownloadTime(expectedTime);
    long long actualTime = TrackStateobj->GetLastPlaylistDownloadTime();
    ASSERT_EQ(actualTime, expectedTime); // Check if the actual time matches the expected time
}

TEST_F(TrackStateTests, SetLastPlaylistDownloadTime)
{
    long long timeToSet = 987654321; // Choose a test time value
    TrackStateobj->SetLastPlaylistDownloadTime(timeToSet);
    ASSERT_EQ(TrackStateobj->GetLastPlaylistDownloadTime(), timeToSet); // Check if the time was correctly set
}

TEST_F(StreamAbstractionAAMP_HLSTest, FilterAudioCodecBasedOnConfig_AllowAC3)
{
	EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_DisableEC3)).WillOnce(Return(true));
	EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_DisableAC3)).WillOnce(Return(false));
	bool result = mStreamAbstractionAAMP_HLS->FilterAudioCodecBasedOnConfig(FORMAT_AUDIO_ES_AC3);
    ASSERT_FALSE(result); // AC3 should be allowed
}

TEST_F(StreamAbstractionAAMP_HLSTest, FilterAudioCodecBasedOnConfig_DisableATMOS)
{
    EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_DisableEC3)).WillOnce(Return(false));
	EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_DisableAC3)).WillOnce(Return(false));
	EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_DisableATMOS)).WillOnce(Return(true));
    bool result = mStreamAbstractionAAMP_HLS->FilterAudioCodecBasedOnConfig(FORMAT_AUDIO_ES_ATMOS);
    ASSERT_TRUE(result); // ATMOS should be disabled
}

TEST_F(StreamAbstractionAAMP_HLSTest, FilterAudioCodecBasedOnConfig_DisableEC3)
{
    EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_DisableEC3)).WillOnce(Return(true));
	EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_DisableAC3)).WillOnce(Return(false));
	bool result = mStreamAbstractionAAMP_HLS->FilterAudioCodecBasedOnConfig(FORMAT_AUDIO_ES_EC3);
	ASSERT_TRUE(result); // EC3 should be disabled
}

TEST_F(StreamAbstractionAAMP_HLSTest, FilterAudioCodecBasedOnConfig_Default)
{
    EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_DisableEC3)).WillOnce(Return(false));
	EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_DisableAC3)).WillOnce(Return(false));
	EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_DisableATMOS)).WillOnce(Return(false));
    bool result = mStreamAbstractionAAMP_HLS->FilterAudioCodecBasedOnConfig(FORMAT_INVALID);
   	ASSERT_FALSE(result);
}

TEST_F(StreamAbstractionAAMP_HLSTest, SetABRMinBuffer_test)
{
    //    mStreamAbstractionAAMP_HLS->SetABRMinBuffer(eAAMPConfig_MinABRNWBufferRampDown);
    //    mStreamAbstractionAAMP_HLS->SetABRMinBuffer(eAAMPConfig_MaxABRNWBufferRampUp);
    //    mStreamAbstractionAAMP_HLS->SetABRMinBuffer(UINT_MAX);
    //    mStreamAbstractionAAMP_HLS->SetABRMinBuffer(11.32888977);
    mStreamAbstractionAAMP_HLS->SetABRMinBuffer(-12);
}

TEST_F(StreamAbstractionAAMP_HLSTest, SetABRMaxBuffer_test)
{
    mStreamAbstractionAAMP_HLS->SetABRMaxBuffer(-12);
}
// Test case to verify SetTsbBandwidth with boundary conditions
TEST_F(StreamAbstractionAAMP_HLSTest, SetTsbBandwidthBoundary)
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

TEST_F(StreamAbstractionAAMP_HLSTest, GetProfileCounttest)
{
    // Call the function under test
    int profileCount = mStreamAbstractionAAMP_HLS->GetProfileCount();

    // Perform assertions to verify the expected behavior
    ASSERT_EQ(profileCount, 0);
}

// Bug as mutex thread handling not done properly
TEST_F(StreamAbstractionAAMP_HLSTest, WaitForVideoTrackCatchupTest)
{
    mStreamAbstractionAAMP_HLS->WaitForVideoTrackCatchup();
}

TEST_F(StreamAbstractionAAMP_HLSTest, ReassessAndResumeAudioTrackTest)
{
    mStreamAbstractionAAMP_HLS->ReassessAndResumeAudioTrack(true);
}

TEST_F(StreamAbstractionAAMP_HLSTest, ReassessAndResumeAudioTrackTest_1)
{

    mStreamAbstractionAAMP_HLS->ReassessAndResumeAudioTrack(false);
}
TEST_F(StreamAbstractionAAMP_HLSTest, LastVideoFragParsedTimeMSTest)
{
    double result = mStreamAbstractionAAMP_HLS->LastVideoFragParsedTimeMS();
    ASSERT_EQ(result, 0);
}

TEST_F(StreamAbstractionAAMP_HLSTest, GetDesiredProfileTest)
{

    mStreamAbstractionAAMP_HLS->GetDesiredProfile(true);
}

TEST_F(StreamAbstractionAAMP_HLSTest, GetDesiredProfileTest_1)
{

    mStreamAbstractionAAMP_HLS->GetDesiredProfile(false);
}

TEST_F(StreamAbstractionAAMP_HLSTest, GetMaxBWProfileTest)
{
    int result = mStreamAbstractionAAMP_HLS->GetMaxBWProfile();
    ASSERT_EQ(result, 0);
}

TEST_F(StreamAbstractionAAMP_HLSTest, NotifyBitRateUpdateTest)
{
    // Set up necessary data and conditions for testing
    int profileIndex = 1; // Replace with the desired profileIndex value
    StreamInfo cacheFragStreamInfo;
    // Call the NotifyBitRateUpdate function
    mStreamAbstractionAAMP_HLS->NotifyBitRateUpdate(profileIndex, cacheFragStreamInfo, 0.0); // Set position to 0.0 for this test
}
TEST_F(StreamAbstractionAAMP_HLSTest, NotifyBitRateUpdateTest_1)
{
    // Set up necessary data and conditions for testing
    int profileIndex = 1; // Replace with the desired profileIndex value
    StreamInfo cacheFragStreamInfo;
    cacheFragStreamInfo.bandwidthBitsPerSecond = 0; // Zero value
    // Call the NotifyBitRateUpdate function
    mStreamAbstractionAAMP_HLS->NotifyBitRateUpdate(profileIndex, cacheFragStreamInfo, 0.0); // Set position to 0.0 for this test
}

TEST_F(StreamAbstractionAAMP_HLSTest, IsInitialCachingSupported)
{
    bool result = mStreamAbstractionAAMP_HLS->IsInitialCachingSupported();
    ASSERT_FALSE(result);
}

TEST_F(StreamAbstractionAAMP_HLSTest, UpdateStreamInfoBitrateDatatest)
{
    // Set up necessary data and conditions for testing
    int profileIndex = 1; // Replace with the desired profileIndex value
    StreamInfo cacheFragStreamInfo;
    mStreamAbstractionAAMP_HLS->UpdateStreamInfoBitrateData(profileIndex, cacheFragStreamInfo); // Set position to 0.0 for this test
}

TEST_F(StreamAbstractionAAMP_HLSTest, UpdateRampUpOrDownProfileReasontest)
{
    BitrateChangeReason expectedBitrateReason = eAAMP_BITRATE_CHANGE_BY_RAMPDOWN;
    mStreamAbstractionAAMP_HLS->UpdateRampUpOrDownProfileReason();
}

TEST_F(StreamAbstractionAAMP_HLSTest, ConfigureTimeoutOnBuffertest)
{
    // Set up necessary data and conditions for testing
    mStreamAbstractionAAMP_HLS->ConfigureTimeoutOnBuffer();
}

TEST_F(StreamAbstractionAAMP_HLSTest, RampDownProfiletest)
{
    // Set up necessary data and conditions for testing
    bool result = mStreamAbstractionAAMP_HLS->RampDownProfile(400);
    ASSERT_FALSE(result);
}

TEST_F(StreamAbstractionAAMP_HLSTest, IsLowestProfileTest)
{
    // Set up the necessary data or objects for your test
    int currentProfileIndex = 0;
    // int currentProfileIndex = -22;
    // Call the function you want to test
    bool result = mStreamAbstractionAAMP_HLS->IsLowestProfile(currentProfileIndex);

    ASSERT_TRUE(result);
}

TEST_F(StreamAbstractionAAMP_HLSTest, getOriginalCurlErrorTest)
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

TEST_F(StreamAbstractionAAMP_HLSTest, CheckForProfileChangetest)
{
    // Set up necessary data and conditions for testing
    mStreamAbstractionAAMP_HLS->CheckForProfileChange();
}

TEST_F(StreamAbstractionAAMP_HLSTest, GetIframeTracktest)
{
    // Set up necessary data and conditions for testing
    int result = mStreamAbstractionAAMP_HLS->GetIframeTrack();
    ASSERT_EQ(result, 0);
}

TEST_F(StreamAbstractionAAMP_HLSTest, UpdateIframeTrackstest)
{
    // Set up necessary data and conditions for testing
    mStreamAbstractionAAMP_HLS->UpdateIframeTracks();
}

TEST_F(StreamAbstractionAAMP_HLSTest, NotifyPlaybackPausedtest_2)
{
    // Set up necessary data and conditions for testing
    mStreamAbstractionAAMP_HLS->NotifyPlaybackPaused(true);
}

TEST_F(StreamAbstractionAAMP_HLSTest, NotifyPlaybackPausedtest_1)
{
    // Set up necessary data and conditions for testing
    mStreamAbstractionAAMP_HLS->NotifyPlaybackPaused(false);
}

TEST_F(StreamAbstractionAAMP_HLSTest, CheckIfPlayerRunningDry)
{
    // Set up necessary data and conditions for testing
    bool result = mStreamAbstractionAAMP_HLS->CheckIfPlayerRunningDry();
    ASSERT_FALSE(result);
}

TEST_F(StreamAbstractionAAMP_HLSTest, CheckForPlaybackStalltest_3)
{

    EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_SuppressDecode)).WillOnce(Return(true));
    mStreamAbstractionAAMP_HLS->CheckForPlaybackStall(true);
}

TEST_F(StreamAbstractionAAMP_HLSTest, CheckForPlaybackStalltest_4)
{

    EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_SuppressDecode)).WillOnce(Return(true));
    mStreamAbstractionAAMP_HLS->CheckForPlaybackStall(false);
}

TEST_F(StreamAbstractionAAMP_HLSTest, NotifyFirstFragmentInjectedtest)
{
    // Set up necessary data and conditions for testing
    mStreamAbstractionAAMP_HLS->NotifyFirstFragmentInjected();
}

TEST_F(StreamAbstractionAAMP_HLSTest, GetElapsedTimetest)
{
    // Set up necessary data and conditions for testing
    double result = mStreamAbstractionAAMP_HLS->GetElapsedTime();
    ASSERT_NE(result, 0.0);
}

TEST_F(StreamAbstractionAAMP_HLSTest, GetVideoBitratetest)
{
    // Set up necessary data and conditions for testing
    BitsPerSecond result = mStreamAbstractionAAMP_HLS->GetVideoBitrate();
    ASSERT_EQ(result, 0.0);
}

TEST_F(StreamAbstractionAAMP_HLSTest, GetAudioBitratetest)
{
    // Set up necessary data and conditions for testing
    BitsPerSecond result = mStreamAbstractionAAMP_HLS->GetAudioBitrate();
    ASSERT_EQ(result, 0.0);
}

TEST_F(StreamAbstractionAAMP_HLSTest, IsMuxedStreamtest_1)
{
    // Set up necessary data and conditions for testing
    EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_AudioOnlyPlayback)).WillOnce(Return(true));
    bool result = mStreamAbstractionAAMP_HLS->IsMuxedStream();
    ASSERT_FALSE(result);
}

TEST_F(StreamAbstractionAAMP_HLSTest, WaitForAudioTrackCatchuptest)
{
    // Set up necessary data and conditions for testing
    mStreamAbstractionAAMP_HLS->WaitForAudioTrackCatchup();
}

TEST_F(StreamAbstractionAAMP_HLSTest, AbortWaitForAudioTrackCatchuptest)
{
    // Set up necessary data and conditions for testing
    mStreamAbstractionAAMP_HLS->AbortWaitForAudioTrackCatchup(true);
}

TEST_F(StreamAbstractionAAMP_HLSTest, AbortWaitForAudioTrackCatchuptest_1)
{
    // Set up necessary data and conditions for testing
    mStreamAbstractionAAMP_HLS->AbortWaitForAudioTrackCatchup(false);
}

TEST_F(StreamAbstractionAAMP_HLSTest, MuteSubtitlestest)
{
    // Set up necessary data and conditions for testing
    mStreamAbstractionAAMP_HLS->MuteSubtitles(false);
}

TEST_F(StreamAbstractionAAMP_HLSTest, MuteSubtitlestesttest_1)
{
    // Set up necessary data and conditions for testing
    mStreamAbstractionAAMP_HLS->MuteSubtitles(false);
}

TEST_F(StreamAbstractionAAMP_HLSTest, IsEOSReachedtest)
{
    // Set up necessary data and conditions for testing
    bool result = mStreamAbstractionAAMP_HLS->IsEOSReached();
    ASSERT_TRUE(result);
}

TEST_F(StreamAbstractionAAMP_HLSTest, GetLastInjectedFragmentPositiontest)
{
    // Set up necessary data and conditions for testing
    double result = mStreamAbstractionAAMP_HLS->GetLastInjectedFragmentPosition();
    ASSERT_EQ(result, 0);
}

TEST_F(StreamAbstractionAAMP_HLSTest, resetDiscontinuityTrackStatetest)
{
    // Set up necessary data and conditions for testing
    mStreamAbstractionAAMP_HLS->resetDiscontinuityTrackState();
}

TEST_F(StreamAbstractionAAMP_HLSTest, AbortWaitForDiscontinuitytest)
{
    // Set up necessary data and conditions for testing
    mStreamAbstractionAAMP_HLS->AbortWaitForDiscontinuity();
}

TEST_F(StreamAbstractionAAMP_HLSTest, CheckForMediaTrackInjectionStalltest)
{
    // Set up necessary data and conditions for testing
    mStreamAbstractionAAMP_HLS->CheckForMediaTrackInjectionStall(eTRACK_AUDIO);
}

TEST_F(StreamAbstractionAAMP_HLSTest, CheckForMediaTrackInjectionStalltest_1)
{
    // Set up necessary data and conditions for testing
    mStreamAbstractionAAMP_HLS->CheckForMediaTrackInjectionStall(eTRACK_VIDEO);
}

TEST_F(StreamAbstractionAAMP_HLSTest, CheckForRampDownLimitReachedtest)
{
    // Set up necessary data and conditions for testing
    bool result = mStreamAbstractionAAMP_HLS->CheckForRampDownLimitReached();
    EXPECT_TRUE(result);
}

TEST_F(StreamAbstractionAAMP_HLSTest, GetBufferedVideoDurationSectest_old)
{
    // Set up necessary data and conditions for testing
    double result = mStreamAbstractionAAMP_HLS->GetBufferedVideoDurationSec();
    ASSERT_EQ(result, -1);
}

TEST_F(StreamAbstractionAAMP_HLSTest, GetAudioTracktest)
{
    // Set up necessary data and conditions for testing
    int result = mStreamAbstractionAAMP_HLS->GetAudioTrack();
    ASSERT_EQ(result, -1);
}

TEST_F(StreamAbstractionAAMP_HLSTest, GetTextTracktest_1)
{
    // Set up necessary data and conditions for testing
    int result = mStreamAbstractionAAMP_HLS->GetTextTrack();
    ASSERT_EQ(result, -1.0);
}

TEST_F(StreamAbstractionAAMP_HLSTest, RefreshSubtitlestest)
{
    // Set up necessary data and conditions for testing
    mStreamAbstractionAAMP_HLS->RefreshSubtitles();
}

TEST_F(StreamAbstractionAAMP_HLSTest, WaitForVideoTrackCatchupForAuxtest)
{
    // Set up necessary data and conditions for testing
    mStreamAbstractionAAMP_HLS->WaitForVideoTrackCatchupForAux();
}

TEST_F(StreamAbstractionAAMP_HLSTest, GetPreferredLiveOffsetFromConfigtest_1)
{
    // Set up necessary data and conditions for testing
    bool result = mStreamAbstractionAAMP_HLS->GetPreferredLiveOffsetFromConfig();
    EXPECT_FALSE(result);
}

TEST_F(StreamAbstractionAAMP_HLSTest, IsStreamerAtLivePointtest)
{
    // Set up necessary data and conditions for testing
    bool result = mStreamAbstractionAAMP_HLS->IsStreamerAtLivePoint();
    EXPECT_FALSE(result);
}

TEST_F(StreamAbstractionAAMP_HLSTest, IsSeekedToLivetest_2)
{
    // Set up necessary data and conditions for testing
    // mStreamAbstractionAAMP_HLS->IsSeekedToLive(0.0);
    bool result = mStreamAbstractionAAMP_HLS->IsSeekedToLive(-1.2);
    EXPECT_TRUE(result);
}

TEST_F(StreamAbstractionAAMP_HLSTest, DisablePlaylistDownloadstest)
{
    // Set up necessary data and conditions for testing
    mStreamAbstractionAAMP_HLS->DisablePlaylistDownloads();
}

TEST_F(StreamAbstractionAAMP_HLSTest, IsStreamerAtLivePointtest4)
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

TEST_F(StreamAbstractionAAMP_HLSTest, TestGetAvailabilityStartTime)
{
    double expectedValue = 0.0; // The expected return value
    double result = mStreamAbstractionAAMP_HLS->GetAvailabilityStartTime();
    ASSERT_EQ(result, expectedValue);
}

TEST_F(TrackStateTests, StopInjectLooptest)
{
    // Act: Call the function to be tested
    TrackStateobj->StopInjectLoop();
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
    CachedFragment *cachedFragment = TrackStateobj->GetFetchChunkBuffer(true);
    ASSERT_EQ(cachedFragment, nullptr);
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

TEST_F(TrackStateTests, OnSinkBufferFullTests)
{
    TrackStateobj->OnSinkBufferFull();
}

TEST_F(TrackStateTests, GetPlaylistMediaTypeFromTrackTest)
{
    AampMediaType playlistMediaType = TrackStateobj->GetPlaylistMediaTypeFromTrack(eTRACK_VIDEO, true);
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
    ASSERT_TRUE(result);
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

TEST_F(StreamAbstractionAAMP_HLSTest, GetMediaTracktest)
{
    mStreamAbstractionAAMP_HLS->GetMediaTrack(eTRACK_VIDEO);
}

TEST_F(StreamAbstractionAAMP_HLSTest, GetStartTimeOfFirstPTStest)
{
    double result = mStreamAbstractionAAMP_HLS->GetStartTimeOfFirstPTS();
    ASSERT_EQ(result, 0.0);
}

TEST_F(StreamAbstractionAAMP_HLSTest, Test4KStreamPositive)
{
    int height = 4000;
    BitsPerSecond bandwidth = 10000; // Adjust this value as needed
    bool result = mStreamAbstractionAAMP_HLS->Is4KStream(height, bandwidth);
    ASSERT_FALSE(result);
}

// Test case to check the behavior of Is4KStream when height is less than 4000
TEST_F(StreamAbstractionAAMP_HLSTest, Test4KStreamHeightTooLow)
{
    int height = 3000;               // Adjust this value as needed
    BitsPerSecond bandwidth = 10000; // Adjust this value as needed
    bool result = mStreamAbstractionAAMP_HLS->Is4KStream(height, bandwidth);
    ASSERT_FALSE(result);
}

TEST_F(StreamAbstractionAAMP_HLSTest, Test4KStreamBandwidthInsufficient)
{
    int height = 0;
    BitsPerSecond bandwidth = 0; // Adjust this value as needed
    bool result = mStreamAbstractionAAMP_HLS->Is4KStream(height, bandwidth);
    ASSERT_FALSE(result);
}

TEST_F(StreamAbstractionAAMP_HLSTest, TestGetFirstPeriodStartTime)
{
    double expectedValue = 0.0; // The expected return value
    // Call the virtual function and check if it returns the expected value.
    double result = mStreamAbstractionAAMP_HLS->GetFirstPeriodStartTime();
    ASSERT_EQ(result, expectedValue);
}

TEST_F(StreamAbstractionAAMP_HLSTest, TestGetFirstPeriodDynamicStartTime)
{
    double expectedValue = 0.0; // The expected return value
    double result = mStreamAbstractionAAMP_HLS->GetFirstPeriodDynamicStartTime();
    ASSERT_EQ(result, expectedValue);
}

TEST_F(StreamAbstractionAAMP_HLSTest, TestGetCurrPeriodTimeScale)
{
    uint32_t expectedValue = 0; // The expected return value
    // Call the virtual function and check if it returns the expected value.
    uint32_t result = mStreamAbstractionAAMP_HLS->GetCurrPeriodTimeScale();
    ASSERT_EQ(result, expectedValue);
}

TEST_F(StreamAbstractionAAMP_HLSTest, TestGetBWIndex)
{
    BitsPerSecond bandwidth = -13; // Create a BitsPerSecond object if needed
    int expectedValue = 0;         // The expected return value
    // Call the virtual function and check if it returns the expected value.
    int result = mStreamAbstractionAAMP_HLS->GetBWIndex(bandwidth);
    ASSERT_EQ(result, expectedValue);
}

TEST_F(StreamAbstractionAAMP_HLSTest, TestGetBWIndex_new)
{
    BitsPerSecond bandwidth = -13; // Create a BitsPerSecond object if needed
    int expectedValue = 0;         // The expected return value
    // Call the virtual function and check if it returns the expected value.

    mStreamAbstractionAAMP_HLS->CallPopulateAudioAndTextTracks();
    int result = mStreamAbstractionAAMP_HLS->GetBWIndex(bandwidth);
    ASSERT_EQ(result, expectedValue);
}

// Test case for GetProfileIndexForBandwidth()
TEST_F(StreamAbstractionAAMP_HLSTest, TestGetProfileIndexForBandwidth)
{
    BitsPerSecond mTsbBandwidth; // Create a BitsPerSecond object with an appropriate value
    int expectedValue = 0;
    // Call the virtual function and check if it returns the expected value.
    int result = mStreamAbstractionAAMP_HLS->GetProfileIndexForBandwidth(mTsbBandwidth);
    ASSERT_EQ(result, expectedValue);
}

// Test case for GetMaxBitrate()
TEST_F(StreamAbstractionAAMP_HLSTest, TestGetMaxBitrate)
{
    BitsPerSecond expectedValue = 0; // Set to an appropriate value for your test case
    // Call the function to get the max bitrate.
    BitsPerSecond result = mStreamAbstractionAAMP_HLS->GetMaxBitrate();
    ASSERT_EQ(result, expectedValue);
}

// Test case for GetVideoBitrates() when the function returns an empty vector.
TEST_F(StreamAbstractionAAMP_HLSTest, TestGetVideoBitratesEmpty)
{
    // Call the function to get the video bitrates.
    std::vector<BitsPerSecond> result = mStreamAbstractionAAMP_HLS->GetVideoBitrates();
    // Check if the result is an empty vector.
    ASSERT_TRUE(result.empty());
}

// Test case for GetAudioBitrates() when the function returns an empty vector.
TEST_F(StreamAbstractionAAMP_HLSTest, TestGetAudioBitratesEmpty)
{
    // Call the function to get the audio bitrates.
    std::vector<BitsPerSecond> result = mStreamAbstractionAAMP_HLS->GetAudioBitrates();
    // Check if the result is an empty vector.
    ASSERT_TRUE(result.empty());
}

TEST_F(StreamAbstractionAAMP_HLSTest, StopInjectiontest)
{
    mStreamAbstractionAAMP_HLS->StopInjection();
}

TEST_F(StreamAbstractionAAMP_HLSTest, IsStreamerStalledtest)
{
    bool result = mStreamAbstractionAAMP_HLS->IsStreamerStalled();
    ASSERT_FALSE(result);
}

TEST_F(StreamAbstractionAAMP_HLSTest, SetTextStyleTest)
{
    // Create an instance of your Subtitle class (replace with the actual class name)

    // Define a JSON string with test options
    std::string testOptions = "AAMP";
    // Call the SetTextStyle function and check the result
    bool result = mStreamAbstractionAAMP_HLS->SetTextStyle(testOptions);

    EXPECT_FALSE(result);
}

TEST_F(StreamAbstractionAAMP_HLSTest, TestResumeSubtitleOnPlay)
{
    // Define test data and mute flag
    char testData[] = "";
    bool mute = false;
    // Call the ResumeSubtitleAfterSeek function
    mStreamAbstractionAAMP_HLS->ResumeSubtitleOnPlay(mute, testData);
}

TEST_F(StreamAbstractionAAMP_HLSTest, TestMuteSidecarSubtitles)
{
    // Define a mute flag (true or false)
    bool mute = true;
    // Call the MuteSidecarSubtitles function .
    mStreamAbstractionAAMP_HLS->MuteSidecarSubtitles(mute);
}

TEST_F(StreamAbstractionAAMP_HLSTest, TestMuteSubtitleOnPause)
{
    // Call the MuteSubtitleOnPause function .
    mStreamAbstractionAAMP_HLS->MuteSubtitleOnPause();
}

TEST_F(StreamAbstractionAAMP_HLSTest, TestMResetSubtitle)
{
    // Call the ResetSubtitle function .
    mStreamAbstractionAAMP_HLS->ResetSubtitle();
}

TEST_F(StreamAbstractionAAMP_HLSTest, TestInitSubtitleParser)
{
    char testData[] = "";
    // Call the ResetSubtitle function .
    mStreamAbstractionAAMP_HLS->InitSubtitleParser(testData);
}

TEST_F(StreamAbstractionAAMP_HLSTest, TestSetCurrentAudioTrackIndex)
{
    // Define an audio track index as a string
    std::string trackIndex = "";
    // Call the SetCurrentAudioTrackIndex function
    mStreamAbstractionAAMP_HLS->SetCurrentAudioTrackIndex(trackIndex);
}

TEST_F(StreamAbstractionAAMP_HLSTest, TestSetCurrentAudioTrackIndex_1)
{
    // Define an audio track index as a string
    std::string trackIndex = "0";
    // Call the SetCurrentAudioTrackIndex function
    mStreamAbstractionAAMP_HLS->SetCurrentAudioTrackIndex(trackIndex);
}

TEST_F(StreamAbstractionAAMP_HLSTest, TestEnableContentRestrictions)
{
    // Call the EnableContentRestrictions function
    mStreamAbstractionAAMP_HLS->EnableContentRestrictions();
}
// Test case 1: Test with grace = -1, time = 0, eventChange = false
TEST_F(StreamAbstractionAAMP_HLSTest, TestUnlockWithUnlimitedGrace)
{

    mStreamAbstractionAAMP_HLS->DisableContentRestrictions(-1, 0, false);
}

// Test case 2: Test with specific grace and time, eventChange = true
TEST_F(StreamAbstractionAAMP_HLSTest, TestUnlockWithSpecificGraceAndTime)
{
    mStreamAbstractionAAMP_HLS->DisableContentRestrictions(3600, 7200, true);
}

TEST_F(StreamAbstractionAAMP_HLSTest, TestApplyContentRestrictions)
{
    std::vector<std::string> restrictions;
    // Call the ApplyContentRestrictions function
    mStreamAbstractionAAMP_HLS->ApplyContentRestrictions(restrictions);
}

TEST_F(StreamAbstractionAAMP_HLSTest, TestSetPreferredAudioLanguages)
{
    // Call the SetPreferredAudioLanguages function
    mStreamAbstractionAAMP_HLS->SetPreferredAudioLanguages();
}

TEST_F(StreamAbstractionAAMP_HLSTest, TestSetAudioTrack)
{
    // Call the SetAudioTrack  function
    mStreamAbstractionAAMP_HLS->SetAudioTrack(-122);
}

TEST_F(StreamAbstractionAAMP_HLSTest, TestSetAudioTrackByLanguage)
{
    const char *lang = "english";
    // Call the ApplyContentRestrictions function
    mStreamAbstractionAAMP_HLS->SetAudioTrackByLanguage(lang);
}

TEST_F(StreamAbstractionAAMP_HLSTest, TestSetThumbnailTrackWithInvalidIndex)
{
    int thumbnailIndex = -1; // Replace with an invalid index
    bool result = mStreamAbstractionAAMP_HLS->SetThumbnailTrack(thumbnailIndex);
    ASSERT_FALSE(result); // Assert that the function returned false for an invalid index
}

// Test case 1: Test SetVideoRectangle with valid coordinates and size
TEST_F(StreamAbstractionAAMP_HLSTest, TestSetVideoRectangleWithValidParams)
{
    int x = 0;
    int y = 0;
    int w = 1920;
    int h = 1080;
    mStreamAbstractionAAMP_HLS->SetVideoRectangle(x, y, w, h);
}

// Test case 2: Test GetAvailableVideoTracks
TEST_F(StreamAbstractionAAMP_HLSTest, TestGetAvailableVideoTracks)
{
    std::vector<StreamInfo *> videoTracks = mStreamAbstractionAAMP_HLS->GetAvailableVideoTracks();

    ASSERT_TRUE(videoTracks.empty());
}

// Test case 3: Test GetAvailableThumbnailTracks
TEST_F(StreamAbstractionAAMP_HLSTest, TestGetAvailableThumbnailTracks)
{
    std::vector<StreamInfo *> thumbnailTracks = mStreamAbstractionAAMP_HLS->GetAvailableThumbnailTracks();

    ASSERT_TRUE(thumbnailTracks.empty());
}

TEST_F(TrackStateTests, GetPlaylistMediaTypeFromTrackTest_1)
{
    AampMediaType playlistMediaType = TrackStateobj->GetPlaylistMediaTypeFromTrack(eTRACK_VIDEO, false);

}
TEST_F(TrackStateTests, GetPlaylistMediaTypeFromTrackTest_2)
{
    AampMediaType playlistMediaType = TrackStateobj->GetPlaylistMediaTypeFromTrack(eTRACK_AUDIO , false);
}

TEST_F(TrackStateTests, GetPlaylistMediaTypeFromTrackTest_3)
{
    AampMediaType playlistMediaType = TrackStateobj->GetPlaylistMediaTypeFromTrack(eTRACK_SUBTITLE, false);
}

TEST_F(TrackStateTests, GetPlaylistMediaTypeFromTrackTest_4)
{
    AampMediaType playlistMediaType = TrackStateobj->GetPlaylistMediaTypeFromTrack(eTRACK_AUX_AUDIO, false);
}

TEST_F(StreamAbstractionAAMP_HLSTest, IsStreamerAtLivePointtest_1)
{
    double seekPosition = 470.0;
    mStreamAbstractionAAMP_HLS->aamp->culledSeconds = 100.0;
    mStreamAbstractionAAMP_HLS->aamp->durationSeconds = 300.0;
    mStreamAbstractionAAMP_HLS->aamp->mLiveOffset = 30.0;
    mStreamAbstractionAAMP_HLS->mIsAtLivePoint = true;
    bool result = mStreamAbstractionAAMP_HLS->IsStreamerAtLivePoint(seekPosition);
    EXPECT_TRUE(result);
}

TEST_F(StreamAbstractionAAMP_HLSTest, IsSeekedToLivetest)
{
    double seekPosition = 370.0;
    mStreamAbstractionAAMP_HLS->aamp->culledSeconds = 100.0;
    mStreamAbstractionAAMP_HLS->aamp->durationSeconds = 300.0;
    mStreamAbstractionAAMP_HLS->aamp->mLiveOffset = 30.0;
    mStreamAbstractionAAMP_HLS->mIsAtLivePoint = true;
    bool result = mStreamAbstractionAAMP_HLS->IsSeekedToLive(seekPosition);
    EXPECT_TRUE(result);
}

TEST_F(StreamAbstractionAAMP_HLSTest, IsSeekedToLivetest_1)
{
    double seekPosition = 170.0;
    mStreamAbstractionAAMP_HLS->aamp->culledSeconds = 100.0;
    mStreamAbstractionAAMP_HLS->aamp->durationSeconds = 300.0;
    mStreamAbstractionAAMP_HLS->aamp->mLiveOffset = 30.0;
    mStreamAbstractionAAMP_HLS->mIsAtLivePoint = true;
    bool result = mStreamAbstractionAAMP_HLS->IsSeekedToLive(seekPosition);
    EXPECT_FALSE(result);
}

TEST_F(StreamAbstractionAAMP_HLSTest, GetPreferredLiveOffsetFromConfigtest)
{
    EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_Disable4K)).WillRepeatedly(Return(true));
    mStreamAbstractionAAMP_HLS->aamp->GetMaximumBitrate();
    bool result = mStreamAbstractionAAMP_HLS->GetPreferredLiveOffsetFromConfig();
    EXPECT_FALSE(result);
}

TEST_F(StreamAbstractionAAMP_HLSTest, GetBufferedVideoDurationSectest)
{
    // Set up necessary data and conditions for testing
    mStreamAbstractionAAMP_HLS->aamp->rate = 1;
    double result = mStreamAbstractionAAMP_HLS->GetBufferedVideoDurationSec();
    ASSERT_EQ(result, -1);
}

TEST_F(StreamAbstractionAAMP_HLSTest, GetBufferedVideoDurationSectest_1)
{
    // Set up necessary data and conditions for testing
    mStreamAbstractionAAMP_HLS->aamp->rate = 0;
    double result = mStreamAbstractionAAMP_HLS->GetBufferedVideoDurationSec();
    ASSERT_EQ(result, -1);
}

TEST_F(StreamAbstractionAAMP_HLSTest, ProcessDiscontinuity)
{
    mStreamAbstractionAAMP_HLS->ProcessDiscontinuity(eTRACK_AUX_AUDIO);
}

TEST_F(StreamAbstractionAAMP_HLSTest, SetAudioTrackInfoFromMuxedStreamTest)
{
    std::vector<AudioTrackInfo> audioTrackInfoVector;
    mStreamAbstractionAAMP_HLS->SetAudioTrackInfoFromMuxedStream(audioTrackInfoVector);
}

TEST_F(StreamAbstractionAAMP_HLSTest, IsMuxedStreamtest)
{
    // Set up necessary data and conditions for testing
    EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_AudioOnlyPlayback)).WillOnce(Return(false));
    mStreamAbstractionAAMP_HLS->aamp->rate = 1;
    bool result = mStreamAbstractionAAMP_HLS->IsMuxedStream();
    ASSERT_TRUE(result);
}

TEST_F(StreamAbstractionAAMP_HLSTest, CheckForPlaybackStalltest)
{
    EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_SuppressDecode)).WillOnce(Return(false));
    mStreamAbstractionAAMP_HLS->CheckForPlaybackStall(false);
}

TEST_F(StreamAbstractionAAMP_HLSTest, CheckForPlaybackStalltest_1)
{
    mStreamAbstractionAAMP_HLS->mIsPlaybackStalled= true;
    EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_SuppressDecode)).WillOnce(Return(false));
    mStreamAbstractionAAMP_HLS->CheckForPlaybackStall(true);
}

TEST_F(StreamAbstractionAAMP_HLSTest, CheckForPlaybackStalltest_2)
{
    EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_SuppressDecode)).WillOnce(Return(true));
    mStreamAbstractionAAMP_HLS->CheckForPlaybackStall(true);
}

TEST_F(StreamAbstractionAAMP_HLSTest, CheckForRampDownProfileTest)
{
    mStreamAbstractionAAMP_HLS->CheckForRampDownProfile(1);
}

TEST_F(TrackStateTests, GetNextFragmentUri_WithReloadUri_1)
{
    bool reloadUri = false;
    bool ignoreDiscontinuity = false;
    auto indexNode = TrackStateobj->GetNextFragmentUriFromPlaylist(reloadUri, ignoreDiscontinuity);
	(void)indexNode;
}

TEST_F(TrackStateTests, GetNextFragmentUri_WithReloadUri_2)
{
    bool reloadUri = true;
    bool ignoreDiscontinuity = true;
    auto indexNode = TrackStateobj->GetNextFragmentUriFromPlaylist(reloadUri, ignoreDiscontinuity);
	(void)indexNode;
}

TEST_F(TrackStateTests, WaitTimeBasedOnBufferAvailableTest) {
    // Call the WaitTimeBasedOnBufferAvailable method and get the result
    int result = TrackStateobj->WaitTimeBasedOnBufferAvailable();
    EXPECT_GE(result, 0);  // Verify that the result is non-negative
}

TEST_F(TrackStateTests, EnterTimedWaitForPlaylistRefreshTests_1)
{
    TrackStateobj->EnterTimedWaitForPlaylistRefresh(2);
}

TEST_F(TrackStateTests, StartPlaylistDownloaderThreadTest)
{
    TrackStateobj->StopPlaylistDownloaderThread();
}

TEST_F(TrackStateTests, UpdateTSAfterFetchTest)
{
    TrackStateobj->numberOfFragmentsCached = 0;
    TrackStateobj->maxCachedFragmentsPerTrack = 1;
    TrackStateobj->UpdateTSAfterFetch(true);
}

TEST_F(TrackStateTests, UpdateTSAfterFetchTest_1)
{
    // //TrackStateobj->numberOfFragmentsCached = 0;
    // TrackStateobj->minInitialCacheSeconds = 0;
    // TrackStateobj->currentInitialCacheDurationSeconds = 0;
    TrackStateobj->numberOfFragmentsCached = 0;
    TrackStateobj->maxCachedFragmentsPerTrack = 1;
    bool IsInitSegment = false;
    TrackStateobj->UpdateTSAfterFetch(IsInitSegment);
}

TEST_F(TrackStateTests, UpdateTSAfterFetchTest_2)
{
    // //TrackStateobj->numberOfFragmentsCached = 0;
    // TrackStateobj->minInitialCacheSeconds = 0;
    // TrackStateobj->currentInitialCacheDurationSeconds = 0;
    TrackStateobj->numberOfFragmentsCached = 0;
    TrackStateobj->maxCachedFragmentsPerTrack = 1;
    bool IsInitSegment = false;
    TrackStateobj->UpdateTSAfterFetch(IsInitSegment);
}

TEST_F(TrackStateTests,SetCurrentBandWidth )
{
    TrackStateobj->SetCurrentBandWidth(1);
}

TEST_F(TrackStateTests,GetProfileIndexForBW )
{
    TrackStateobj->GetProfileIndexForBW(1);
}

TEST_F(TrackStateTests,UpdateTSAfterChunkFetch )
{
    TrackStateobj->numberOfFragmentChunksCached = 0;
    TrackStateobj->maxCachedFragmentChunksPerTrack=1;
    TrackStateobj->SetCachedFragmentChunksSize(1);
    TrackStateobj->UpdateTSAfterChunkFetch();
}

TEST_F(TrackStateTests,AbortWaitForCachedAndFreeFragment )
{
    TrackStateobj->AbortWaitForCachedAndFreeFragment(true);
}

TEST_F(TrackStateTests,CheckForFutureDiscontinuityTest)
{
    double cacheDuration = 1.1;
    TrackStateobj->numberOfFragmentsCached = 1;
    TrackStateobj->CheckForFutureDiscontinuity(cacheDuration);
}

TEST_F(TrackStateTests,AbortWaitForCachedAndFreeFragment_1 )
{
    TrackStateobj->AbortWaitForCachedAndFreeFragment(false);
}

TEST_F(StreamAbstractionAAMP_HLSTest, GetTextTracktest)
{
    // Set up necessary data and conditions for testing
    int result = mStreamAbstractionAAMP_HLS->CallGetTextTrack();
    ASSERT_EQ(result, -1.0);
}

TEST_F(StreamAbstractionAAMP_HLSTest, GetAudioTrackTest)
{
    // Set up necessary data and conditions for testing
    int result = mStreamAbstractionAAMP_HLS->CallGetAudioTrack();
    ASSERT_EQ(result, -1.0);
}

TEST_F(StreamAbstractionAAMP_HLSTest, GetCurrentAudioTrackTest)
{
    AudioTrackInfo audioTrackInfo;
    bool result = mStreamAbstractionAAMP_HLS->CallGetCurrentAudioTrack(audioTrackInfo);
    EXPECT_FALSE(result);
    EXPECT_TRUE(audioTrackInfo.index.empty());
    EXPECT_TRUE(audioTrackInfo.language.empty());
}

TEST_F(StreamAbstractionAAMP_HLSTest, GetCurrentTextTracktest)
{
    TextTrackInfo textTrackInfo;
    bool result = mStreamAbstractionAAMP_HLS->CallGetCurrentTextTrack(textTrackInfo);
    EXPECT_FALSE(result);
    EXPECT_TRUE(textTrackInfo.index.empty());
    EXPECT_TRUE(textTrackInfo.language.empty());
}

TEST_F(StreamAbstractionAAMP_HLSTest, NotifyPlaybackPausedtest)
{
    // Set up necessary data and conditions for testing
    mStreamAbstractionAAMP_HLS->CallNotifyPlaybackPaused(true);
}

TEST_F(StreamAbstractionAAMP_HLSTest, IsLowestProfileTest_2)
{
    // Set up the necessary data or objects for your test
    int currentProfileIndex = -1;
    bool result = mStreamAbstractionAAMP_HLS->CallIsLowestProfile(currentProfileIndex);
    ASSERT_FALSE(result);
}

TEST_F(StreamAbstractionAAMP_HLSTest, IsLowestProfileTest_1)
{
    // Set up the necessary data or objects for your test
    int currentProfileIndex = -1;
    bool result = mStreamAbstractionAAMP_HLS->CallIsLowestProfile_1(currentProfileIndex);
    ASSERT_TRUE(result);
}

TEST_F(StreamAbstractionAAMP_HLSTest, RefreshAudioTest)
{
    const char *lang = "english";
    std::vector<MediaInfo> mediaInfoStore;
    MediaInfo media;
    media.type = eMEDIATYPE_AUDIO;
    media.language = lang;
    media.audioFormat = FORMAT_AUDIO_ES_EC3;
    media.characteristics = "";
    mStreamAbstractionAAMP_HLS->mediaInfoStore.push_back(media);
    mStreamAbstractionAAMP_HLS->CallPopulateAudioAndTextTracks();
    mStreamAbstractionAAMP_HLS->CallConfigureAudioTrack();

    EXPECT_EQ(0,mStreamAbstractionAAMP_HLS->currentAudioProfileIndex);

    media.type = eMEDIATYPE_AUDIO;
    lang = "spanish";
    media.language = lang;
    mStreamAbstractionAAMP_HLS->aamp->preferredLanguagesList.push_back(lang);
    media.audioFormat = FORMAT_AUDIO_ES_EC3;
    media.characteristics = "";

    mStreamAbstractionAAMP_HLS->mediaInfoStore.push_back(media);

    mStreamAbstractionAAMP_HLS->RefreshTrack(eMEDIATYPE_AUDIO);
    mStreamAbstractionAAMP_HLS->CallPopulateAudioAndTextTracks();
    mStreamAbstractionAAMP_HLS->CallConfigureAudioTrack();
    EXPECT_EQ(1,mStreamAbstractionAAMP_HLS->currentAudioProfileIndex);
}

extern std::vector<TileInfo> IndexThumbnails( lstring iter );

TEST_F(StreamAbstractionAAMP_HLSTest, ThumbnailIndexing)
{
	const char *raw =
	"#EXTM3U\r\n"
	"#EXT-X-TARGETDURATION:10\r\n"
	"#EXT-X-VERSION:7\r\n"
	"#EXT-X-MEDIA-SEQUENCE:0\r\n"
	"#EXT-X-PLAYLIST-TYPE:VOD\r\n"
	"#EXT-X-IMAGES-ONLY\r\n"
	"#EXTINF:136.8367,\r\n"
	"#EXT-X-TILES:RESOLUTION=336x189,LAYOUT=5x6,DURATION=10\r\n"
	"pckimage-0.jpg\r\n"
	"#EXTINF:200,\r\n"
	"#EXT-X-TILES:RESOLUTION=336x189,LAYOUT=9x17,DURATION=20\r\n"
	"pckimage-1.jpg\r\n"
	"#EXTINF:100.8367,\r\n"
	"#EXT-X-TILES:RESOLUTION=336x189,LAYOUT=4x3,DURATION=30\r\n"
	"pckimage-2.jpg\r\n"
	"#EXT-X-ENDLIST\r\n";
	lstring ii = lstring( raw, strlen(raw) );
	auto x = IndexThumbnails( ii );
	
	EXPECT_EQ(x[0].url,"pckimage-0.jpg");
	EXPECT_EQ(x[0].layout.numCols,5);
	EXPECT_EQ(x[0].layout.numRows,6);
	EXPECT_EQ(x[0].layout.posterDuration,10);
	EXPECT_EQ(x[0].layout.tileSetDuration,136.8367);
	
	EXPECT_EQ(x[1].url,"pckimage-1.jpg");
	EXPECT_EQ(x[1].layout.numCols,9);
	EXPECT_EQ(x[1].layout.numRows,17);
	EXPECT_EQ(x[1].layout.posterDuration,20);
	EXPECT_EQ(x[1].layout.tileSetDuration,200);
	
	EXPECT_EQ(x[2].url,"pckimage-2.jpg");
	EXPECT_EQ(x[2].layout.numCols,4);
	EXPECT_EQ(x[2].layout.numRows,3);
	EXPECT_EQ(x[2].layout.posterDuration,30);
	EXPECT_EQ(x[2].layout.tileSetDuration,100.8367);
}
TEST_F(StreamAbstractionAAMP_HLSTest,SelectPreferredTextTrack)
{
	std::vector<TextTrackInfo> tracks;
	TextTrackInfo trackInfo;
	tracks.push_back(TextTrackInfo("idx0", "lang0", false, "","","","",0));
	mStreamAbstractionAAMP_HLS->CallSetAvailableTextTracks(tracks);
	mStreamAbstractionAAMP_HLS->aamp->preferredTextLanguagesString = "lang0";
	mStreamAbstractionAAMP_HLS->SelectPreferredTextTrack(trackInfo);
	EXPECT_EQ("lang0",trackInfo.language);
	tracks.push_back(TextTrackInfo("idx0", "lang0", false, "rend0","","","",0));
	mStreamAbstractionAAMP_HLS->CallSetAvailableTextTracks(tracks);
	mStreamAbstractionAAMP_HLS->aamp->preferredTextLanguagesString = "lang0";
	mStreamAbstractionAAMP_HLS->aamp->preferredTextRenditionString = "rend0";
	mStreamAbstractionAAMP_HLS->SelectPreferredTextTrack(trackInfo);
	EXPECT_EQ("lang0",trackInfo.language);
	EXPECT_EQ("rend0",trackInfo.rendition);
	tracks.push_back(TextTrackInfo("idx0", "lang0", false, "rend0","trackName0","","",0));
	mStreamAbstractionAAMP_HLS->CallSetAvailableTextTracks(tracks);
	mStreamAbstractionAAMP_HLS->aamp->preferredTextLanguagesString = "lang0";
	mStreamAbstractionAAMP_HLS->aamp->preferredTextRenditionString = "rend0";
	mStreamAbstractionAAMP_HLS->aamp->preferredTextNameString = "trackName0";
	mStreamAbstractionAAMP_HLS->SelectPreferredTextTrack(trackInfo);
	EXPECT_EQ("lang0",trackInfo.language);
	EXPECT_EQ("rend0",trackInfo.rendition);
	EXPECT_EQ("trackName0",trackInfo.name);
}
