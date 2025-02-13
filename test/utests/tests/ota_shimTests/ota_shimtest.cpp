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

#include "AampUtils.h"
#include "ota_shim.h"
#include "priv_aamp.h"
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <assert.h>
#include "StreamAbstractionAAMP.h"
#include <gtest/gtest.h>

using namespace testing;
AampConfig *gpGlobalConfig{nullptr};

PrivateInstanceAAMP *mPrivateInstanceAAMP{};

class StreamAbstractionAAMP_OTATest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        mPrivateInstanceAAMP = new PrivateInstanceAAMP();
        aamp_ota = new TestableStreamAbstractionOTA(mPrivateInstanceAAMP, 0.0, 1.0);
    }

    void TearDown() override
    {
        delete aamp_ota;
    }

    class TestableStreamAbstractionOTA : public StreamAbstractionAAMP_OTA
    {
    public:
        // Make the test class a friend of the StreamAbstractionAAMP_OTA class
        friend class StreamAbstractionAAMP_OTATest;

        TestableStreamAbstractionOTA(PrivateInstanceAAMP *aamp,
                                     double startTime, double playRate)
            : StreamAbstractionAAMP_OTA(aamp, startTime, playRate)
        {
        }
        void CallGetAudioTracks()
        {
            GetAudioTracks();
        }

        void CallNotifyAudioTrackChange(const std::vector<AudioTrackInfo> &tracks)
        {
            NotifyAudioTrackChange(tracks);
        }

        void CallGetTextTracks()
        {
            GetTextTracks();
        }

    };

    PrivateInstanceAAMP *mPrivateInstanceAAMP;
    TestableStreamAbstractionOTA *aamp_ota;
};

TEST_F(StreamAbstractionAAMP_OTATest, TestGetAudioTracks)
{
    aamp_ota->CallGetAudioTracks();
}

TEST_F(StreamAbstractionAAMP_OTATest, TestNotifyAudioTrackChange)
{

    std::vector<AudioTrackInfo> tracks;
    aamp_ota->CallNotifyAudioTrackChange(tracks);
}

TEST_F(StreamAbstractionAAMP_OTATest, TestGetTextTracks)
{
    aamp_ota->CallGetTextTracks();
}

// Define a test case for the Init function
TEST_F(StreamAbstractionAAMP_OTATest, InitTest)
{
    // Act:Call the Init function
    TuneType tuneType = eTUNETYPE_NEW_NORMAL; // Replace with the actual tune type
    AAMPStatusType result = aamp_ota->Init(tuneType);
    // For example, check if retval is equal to the expected result
    EXPECT_EQ(result, eAAMPSTATUS_OK);
}

/*For this function getting segment fault as WAYLAND_DISPLAY: NULL!
In test environment doesn't have the "WAYLAND_DISPLAY" environment variable set.
*/

// Define a test case for the Start function
 TEST_F(StreamAbstractionAAMP_OTATest, StartTest) {
     // Act:Call the Start function
    aamp_ota->Start();
 }

TEST_F(StreamAbstractionAAMP_OTATest, GetStreamFormatTest)
{
    // Initialize output format variables with some non-default values
    StreamOutputFormat primaryFormat = FORMAT_UNKNOWN;
    StreamOutputFormat audioFormat = FORMAT_UNKNOWN;
    StreamOutputFormat auxAudioFormat = FORMAT_UNKNOWN;
    StreamOutputFormat subtitleFormat = FORMAT_UNKNOWN;

    // Call the GetStreamFormat function
    aamp_ota->GetStreamFormat(primaryFormat, audioFormat, auxAudioFormat, subtitleFormat);

    // Assert that the output formats are set to FORMAT_INVALID
    ASSERT_EQ(primaryFormat, FORMAT_INVALID);
    ASSERT_EQ(audioFormat, FORMAT_INVALID);
    ASSERT_EQ(auxAudioFormat, FORMAT_INVALID);
    ASSERT_EQ(subtitleFormat, FORMAT_INVALID);
}

TEST_F(StreamAbstractionAAMP_OTATest, GetFirstPTSTest)
{
    double firstPTS = aamp_ota->GetFirstPTS();
   // Assert that the returned value is 0.0 (the expected stub value)
    ASSERT_DOUBLE_EQ(firstPTS, 0.0);
}

TEST_F(StreamAbstractionAAMP_OTATest, IsInitialCachingSupportedTest)
{
    // Call the IsInitialCachingSupported function
    bool initialCachingSupported = aamp_ota->IsInitialCachingSupported();
    // Assert that the returned value is false (the expected stub value)
    ASSERT_FALSE(initialCachingSupported);
}

TEST_F(StreamAbstractionAAMP_OTATest, GetMaxBitrateTest)
{
    // Call the GetMaxBitrate function
    BitsPerSecond maxBitrate = aamp_ota->GetMaxBitrate();

    // Assert that the returned value is 0 (the expected stub value)
    ASSERT_EQ(maxBitrate, 0);
}

TEST_F(StreamAbstractionAAMP_OTATest, SetaudiotrackTest)
{

    // Set the sample tracks in the StreamAbstractionAAMP_OTA instance
    aamp_ota->SetAudioTrack(-1);
    ASSERT_EQ(aamp_ota->GetAudioTrack(), -1);
}

TEST_F(StreamAbstractionAAMP_OTATest, DisableContentRestrictionsTest)
{

    // Set the sample tracks in the StreamAbstractionAAMP_OTA instance
    aamp_ota->DisableContentRestrictions(-1, -1, false);
}

TEST_F(StreamAbstractionAAMP_OTATest, EnableContentRestrictionsTest)
{
    aamp_ota->EnableContentRestrictions();
}

TEST_F(StreamAbstractionAAMP_OTATest, SetPreferredAudioLanguagesTest)
{
    aamp_ota->SetPreferredAudioLanguages();
}

TEST_F(StreamAbstractionAAMP_OTATest, GetCurrentAudioTrackTest)
{

    // Sample audio track data
    AudioTrackInfo sampleAudioTrack("0", "English", "main", "English Audio", "AAC", 2, 128000);

    // Call GetCurrentAudioTrack with the sample audio track data
    bool result = aamp_ota->GetCurrentAudioTrack(sampleAudioTrack);

    EXPECT_EQ(sampleAudioTrack.index, "0");
    EXPECT_EQ(sampleAudioTrack.language, "English");
}

TEST_F(StreamAbstractionAAMP_OTATest, GetAvailableAudioTracksTest)
{
    aamp_ota->GetAvailableAudioTracks(false);
}

TEST_F(StreamAbstractionAAMP_OTATest, GetAvailableTextTracksTest)
{
    aamp_ota->GetAvailableTextTracks(false);
}

TEST_F(StreamAbstractionAAMP_OTATest, GetAudioTrack)
{

    int result = aamp_ota->GetAudioTrack();
    EXPECT_EQ(result, -1.0);
}

TEST_F(StreamAbstractionAAMP_OTATest, SetAudioTrackByLanguagetest)
{
    // Sample audio track data
    AudioTrackInfo sampleAudioTrack("0", "English", "main", "English Audio", "AAC", 2, 128000);
    const char language[] = "English";
    // Call GetCurrentAudioTrack with the sample audio track data
    aamp_ota->SetAudioTrackByLanguage(language);
}
