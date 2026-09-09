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

/**
 * @file GetAvailableAudioTracksTests.cpp
 * @brief L1 unit tests for PrivateInstanceAAMP::GetAvailableAudioTracks()
 *
 * Verifies that the JSON emitted by GetAvailableAudioTracks() includes a
 * "selected" boolean field that is true for the currently active audio track
 * and false for all other tracks.
 *
 * Regression coverage for TST_2020_UVE_AudioCodecChange: the L3 test failed
 * because no audio track had selected:true in the getAvailableAudioTracks()
 * API response, making it impossible to identify the active codec after a
 * period transition on the Rialto/DASH pipeline.
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <cjson/cJSON.h>

#include "priv_aamp.h"
#include "AampConfig.h"
#include "MockAampGstPlayer.h"
#include "MockStreamAbstractionAAMP.h"
#include "MockAampStreamSinkManager.h"
#include "MockCJsonManager.h"

// External declaration of global mock pointer (defined in FakeCJSON.cpp)
extern std::shared_ptr<MockCJsonManager> g_mockCJsonManager;

using ::testing::_;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::NiceMock;
using ::testing::Not;
using ::testing::StrEq;
using ::testing::AnyNumber;
using ::testing::DoAll;
using ::testing::SetArgReferee;

// Sentinel cJSON pointer values – addresses are never dereferenced; they just
// need to be distinct non-null values that the mock can echo back.
static cJSON *const kFakeArray = reinterpret_cast<cJSON *>(0x9000);
static cJSON *const kFakeObj0  = reinterpret_cast<cJSON *>(0x9010);
static cJSON *const kFakeObj1  = reinterpret_cast<cJSON *>(0x9020);
static cJSON *const kFakeObj2  = reinterpret_cast<cJSON *>(0x9030);

class GetAvailableAudioTracksTests : public ::testing::Test
{
protected:
    PrivateInstanceAAMP *mPrivateInstanceAAMP{};
    std::vector<AudioTrackInfo> mTrackList;

    void SetUp() override
    {
        gpGlobalConfig = new AampConfig();
        mPrivateInstanceAAMP = new PrivateInstanceAAMP(gpGlobalConfig);

        g_mockStreamAbstractionAAMP =
            std::make_shared<NiceMock<MockStreamAbstractionAAMP>>(mPrivateInstanceAAMP);
        g_mockAampGstPlayer =
            std::make_shared<NiceMock<MockAAMPGstPlayer>>(mPrivateInstanceAAMP);
        g_mockAampStreamSinkManager =
            std::make_shared<NiceMock<MockAampStreamSinkManager>>();

        mPrivateInstanceAAMP->mpStreamAbstractionAAMP =
            g_mockStreamAbstractionAAMP.get();

        g_mockCJsonManager = std::make_shared<NiceMock<MockCJsonManager>>();
    }

    void TearDown() override
    {
        // Null out the pointer before deleting to prevent PrivateInstanceAAMP
        // from trying to SAFE_DELETE the mock object a second time.
        if (mPrivateInstanceAAMP)
            mPrivateInstanceAAMP->mpStreamAbstractionAAMP = nullptr;

        delete mPrivateInstanceAAMP;
        mPrivateInstanceAAMP = nullptr;

        g_mockStreamAbstractionAAMP.reset();
        g_mockAampGstPlayer.reset();
        g_mockAampStreamSinkManager.reset();
        g_mockCJsonManager.reset();

        delete gpGlobalConfig;
        gpGlobalConfig = nullptr;
    }

    /**
     * Wire the cJSON skeleton and stream-abstraction mocks for a three-track
     * scenario.  The caller sets EXPECT_CALL expectations for "selected" AFTER
     * calling this helper (so they are registered later and thus take priority
     * in GMock's LIFO matching order over the catch-all set up here).
     *
     * Track list order (== iteration order in production code):
     *   [0] kFakeObj0  index="2-0"  ec-3
     *   [1] kFakeObj1  index="1-0"  mp4a.40.2
     *   [2] kFakeObj2  index="3-0"  mp4a.40.5
     *
     * @param selectedIndex  index string of the track that should be active.
     */
    void SetupThreeTracks(const std::string &selectedIndex)
    {
        mTrackList.clear();

        // Use the default ctor and set fields individually to control exactly
        // which cJSON Add*ToObject calls the production code makes.
        AudioTrackInfo t0, t1, t2;

        t0.index = "2-0"; t0.name = "root_audio111"; t0.language = "en";
        t0.codec = "ec-3"; t0.rendition = "alternate"; t0.isAvailable = true;

        t1.index = "1-0"; t1.name = "root_audio110"; t1.language = "en";
        t1.codec = "mp4a.40.2"; t1.rendition = "main"; t1.isDefault = true; t1.isAvailable = true;

        t2.index = "3-0"; t2.name = "root_audio112"; t2.language = "en";
        t2.codec = "mp4a.40.5"; t2.rendition = "dub"; t2.isAvailable = true;

        mTrackList = {t0, t1, t2};

        // cJSON skeleton: one array, one object per track
        EXPECT_CALL(*g_mockCJsonManager, CreateArray())
            .WillOnce(Return(kFakeArray));
        EXPECT_CALL(*g_mockCJsonManager, CreateObject())
            .WillOnce(Return(kFakeObj0))
            .WillOnce(Return(kFakeObj1))
            .WillOnce(Return(kFakeObj2));
        EXPECT_CALL(*g_mockCJsonManager, AddItemToArray(kFakeArray, _))
            .Times(3).WillRepeatedly(Return(cJSON_True));
        EXPECT_CALL(*g_mockCJsonManager, Print(kFakeArray))
            .WillOnce(Return("[]"));
        EXPECT_CALL(*g_mockCJsonManager, Delete(kFakeArray));

        // Catch-all for every AddBoolToObject call that is NOT for "selected"
        // (i.e. "default" and "availability").  This must be registered BEFORE
        // the per-track "selected" expectations so those take priority in GMock's
        // LIFO matching order.
        EXPECT_CALL(*g_mockCJsonManager,
                    AddBoolToObject(_, Not(StrEq("selected")), _))
            .Times(AnyNumber())
            .WillRepeatedly(Return(nullptr));

        // Stream abstraction mocks
        EXPECT_CALL(*g_mockStreamAbstractionAAMP, GetAvailableAudioTracks(_))
            .WillOnce(ReturnRef(mTrackList));

        AudioTrackInfo selected;
        bool found = false;
        for (const auto &t : mTrackList)
        {
            if (t.index == selectedIndex) { selected = t; found = true; break; }
        }
        EXPECT_CALL(*g_mockStreamAbstractionAAMP, GetCurrentAudioTrack(_))
            .WillOnce(DoAll(SetArgReferee<0>(selected), Return(found)));
    }
};

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

/**
 * When track "2-0" (ec-3) is the active track, AddBoolToObject must be called
 * with ("selected", true) for that track's cJSON object and ("selected", false)
 * for every other track's object.
 *
 * This is the direct regression test for TST_2020_UVE_AudioCodecChange.
 * Note: the boolean value passed to cJSON_AddBoolToObject is a C++ bool
 * (true==1, false==0), NOT the cJSON type flags cJSON_True/cJSON_False.
 */
TEST_F(GetAvailableAudioTracksTests, SelectedTrack_HasSelectedTrue)
{
    SetupThreeTracks("2-0");  // ec-3 track is active

    // KEY ASSERTIONS – registered after the catch-all so they are tried first
    EXPECT_CALL(*g_mockCJsonManager,
                AddBoolToObject(kFakeObj0, StrEq("selected"), true));
    EXPECT_CALL(*g_mockCJsonManager,
                AddBoolToObject(kFakeObj1, StrEq("selected"), false));
    EXPECT_CALL(*g_mockCJsonManager,
                AddBoolToObject(kFakeObj2, StrEq("selected"), false));

    std::string result = mPrivateInstanceAAMP->GetAvailableAudioTracks(false);
    EXPECT_FALSE(result.empty());
}

/**
 * When a different track is active the correct object gets selected:true.
 */
TEST_F(GetAvailableAudioTracksTests, DifferentSelectedTrack_HasSelectedTrue)
{
    SetupThreeTracks("1-0");  // mp4a main track is active

    EXPECT_CALL(*g_mockCJsonManager,
                AddBoolToObject(kFakeObj0, StrEq("selected"), false));
    EXPECT_CALL(*g_mockCJsonManager,
                AddBoolToObject(kFakeObj1, StrEq("selected"), true));
    EXPECT_CALL(*g_mockCJsonManager,
                AddBoolToObject(kFakeObj2, StrEq("selected"), false));

    mPrivateInstanceAAMP->GetAvailableAudioTracks(false);
}

/**
 * When GetCurrentAudioTrack() returns false (no active track identified),
 * all tracks must receive selected:false.
 */
TEST_F(GetAvailableAudioTracksTests, NoCurrentTrack_AllSelectedFalse)
{
    mTrackList.clear();
    AudioTrackInfo t0, t1;
    t0.index = "2-0"; t0.name = "root_audio111"; t0.language = "en";
    t0.codec = "ec-3"; t0.rendition = "alternate"; t0.isAvailable = true;
    t1.index = "1-0"; t1.name = "root_audio110"; t1.language = "en";
    t1.codec = "mp4a.40.2"; t1.rendition = "main"; t1.isAvailable = true;
    mTrackList = {t0, t1};

    EXPECT_CALL(*g_mockCJsonManager, CreateArray()).WillOnce(Return(kFakeArray));
    EXPECT_CALL(*g_mockCJsonManager, CreateObject())
        .WillOnce(Return(kFakeObj0))
        .WillOnce(Return(kFakeObj1));
    EXPECT_CALL(*g_mockCJsonManager, AddItemToArray(kFakeArray, _))
        .Times(2).WillRepeatedly(Return(cJSON_True));
    EXPECT_CALL(*g_mockCJsonManager, Print(kFakeArray)).WillOnce(Return("[]"));
    EXPECT_CALL(*g_mockCJsonManager, Delete(kFakeArray));

    // Catch-all for "default" / "availability" – registered before the specific
    // "selected" expectations.
    EXPECT_CALL(*g_mockCJsonManager,
                AddBoolToObject(_, Not(StrEq("selected")), _))
        .Times(AnyNumber())
        .WillRepeatedly(Return(nullptr));

    EXPECT_CALL(*g_mockStreamAbstractionAAMP, GetAvailableAudioTracks(_))
        .WillOnce(ReturnRef(mTrackList));
    EXPECT_CALL(*g_mockStreamAbstractionAAMP, GetCurrentAudioTrack(_))
        .WillOnce(Return(false));

    // All tracks must be selected:false when no current track is known
    EXPECT_CALL(*g_mockCJsonManager,
                AddBoolToObject(kFakeObj0, StrEq("selected"), false));
    EXPECT_CALL(*g_mockCJsonManager,
                AddBoolToObject(kFakeObj1, StrEq("selected"), false));

    mPrivateInstanceAAMP->GetAvailableAudioTracks(false);
}

/**
 * GetCurrentAudioTrack() must be called for non-TSB streams (was gated on
 * IsLocalAAMPTsb() before the fix).
 */
TEST_F(GetAvailableAudioTracksTests, GetCurrentAudioTrack_CalledForNonTsbStream)
{
    mTrackList.clear();
    AudioTrackInfo t;
    t.index = "0"; t.name = "audio0"; t.language = "en"; t.isAvailable = true;
    mTrackList = {t};

    EXPECT_CALL(*g_mockCJsonManager, CreateArray()).WillOnce(Return(kFakeArray));
    EXPECT_CALL(*g_mockCJsonManager, CreateObject()).WillOnce(Return(kFakeObj0));
    EXPECT_CALL(*g_mockCJsonManager, AddItemToArray(_, _))
        .WillRepeatedly(Return(cJSON_True));
    EXPECT_CALL(*g_mockCJsonManager, Print(_)).WillOnce(Return("[]"));
    EXPECT_CALL(*g_mockCJsonManager, Delete(_));

    EXPECT_CALL(*g_mockStreamAbstractionAAMP, GetAvailableAudioTracks(_))
        .WillOnce(ReturnRef(mTrackList));

    // Core regression guard: before the fix, GetCurrentAudioTrack was only
    // called when IsLocalAAMPTsb() was true.
    EXPECT_CALL(*g_mockStreamAbstractionAAMP, GetCurrentAudioTrack(_))
        .Times(1)
        .WillOnce(Return(false));

    mPrivateInstanceAAMP->GetAvailableAudioTracks(false);
    // IsLocalAAMPTsb() is false by default on a freshly constructed instance
    EXPECT_FALSE(mPrivateInstanceAAMP->IsLocalAAMPTsb());
}
