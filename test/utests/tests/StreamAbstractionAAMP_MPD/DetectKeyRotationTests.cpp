/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2026 RDK Management
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
 * @file DetectKeyRotationTests.cpp
 * @brief Unit tests for StreamAbstractionAAMP_MPD::DetectKeyRotationOnManifestRefresh
 *
 * Tests cover:
 *  - Two periods that share the same cenc:default_KID → no key rotation detected
 *  - New period whose video adaptation set carries a different cenc:default_KID
 *    while the audio adaptation set keeps the original KID → only the video
 *    adaptation set triggers key-rotation detection
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <libxml/xmlreader.h>
#include "priv_aamp.h"
#include "AampConfig.h"
#include "AampLogManager.h"
#include "AampMPDUtils.h"
#include "fragmentcollector_mpd.h"
#include "MediaStreamContext.h"
#include "MockAampConfig.h"
#include "MockAampUtils.h"
#include "MockAampGstPlayer.h"
#include "MockPrivateInstanceAAMP.h"
#include "MockMediaStreamContext.h"
#include "MockAampMPDDownloader.h"
#include "MockAampStreamSinkManager.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::StrictMock;

// ---------------------------------------------------------------------------
// Testable subclass: exposes protected members needed by the tests.
// DetectKeyRotationOnManifestRefresh and mKeyRotationEarlyDetectedPeriodIds
// are both in the protected section of StreamAbstractionAAMP_MPD, so a
// derived class can access them directly.
// ---------------------------------------------------------------------------
class TestableStreamAbstractionAAMP_MPD : public StreamAbstractionAAMP_MPD
{
public:
    using StreamAbstractionAAMP_MPD::StreamAbstractionAAMP_MPD;

    /** Call the protected detection function. */
    void CallDetectKeyRotation(ManifestDownloadResponsePtr resp)
    {
        DetectKeyRotationOnManifestRefresh(resp);
    }

    /** Read the dedup list populated by detection. */
    const std::vector<std::string>& GetDetectedIds() const
    {
        return mKeyRotationEarlyDetectedPeriodIds;
    }

    /** Directly set the parse helper (simulates a completed Init()). */
    void SetParseHelper(AampMPDParseHelperPtr helper)
    {
        mMPDParseHelper = helper;
    }
};

// ---------------------------------------------------------------------------
// Test fixture
// ---------------------------------------------------------------------------
class DetectKeyRotationTests : public ::testing::Test
{
protected:
    PrivateInstanceAAMP *mPrivateInstanceAAMP{nullptr};
    TestableStreamAbstractionAAMP_MPD *mMPD{nullptr};
    CDAIObject *mCdaiObj{nullptr};
    // Keeps the previous manifest's IMPD alive (AampMPDParseHelper holds a
    // raw IMPD* so the owning shared_ptr must outlive the test body).
    ManifestDownloadResponsePtr mPrevManifestResponse;

    static constexpr const char *TEST_MANIFEST_URL = "http://host/asset/manifest.mpd";

    // ------------------------------------------------------------------
    // Manifests
    // All ContentProtection elements use only urn:mpeg:dash:mp4protection:2011
    // (no DRM UUID), so CreateDrmHelper() returns nullptr and
    // QueueContentProtection() is a no-op.  Detection is verified entirely
    // through mKeyRotationEarlyDetectedPeriodIds.
    //
    // KID_A = "11111111-1111-1111-1111-111111111111"  (reference / original)
    // KID_B = "22222222-2222-2222-2222-222222222222"  (rotated key)
    // ------------------------------------------------------------------

    /// @brief Reference manifest: period p1 with video (id=10001) and
    ///        audio (id=20001), both protected with KID_A.
    static constexpr const char *kPrevManifest = R"(<?xml version="1.0" encoding="utf-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011"
     xmlns:cenc="urn:mpeg:cenc:2013"
     profiles="urn:mpeg:dash:profile:isoff-live:2011"
     type="dynamic" minimumUpdatePeriod="PT5S" minBufferTime="PT2S">
  <Period id="p1" start="PT0S">
    <AdaptationSet id="10001" contentType="video" mimeType="video/mp4">
      <ContentProtection schemeIdUri="urn:mpeg:dash:mp4protection:2011"
                         value="cbcs"
                         cenc:default_KID="11111111-1111-1111-1111-111111111111"/>
      <Representation id="v1" bandwidth="800000" codecs="avc1.640028"
                      width="1280" height="720">
        <SegmentTemplate timescale="90000"
                         initialization="v_init.mp4"
                         media="v_$Number$.m4s" startNumber="1">
          <SegmentTimeline><S t="0" d="90000" r="9"/></SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
    <AdaptationSet id="20001" contentType="audio" mimeType="audio/mp4">
      <ContentProtection schemeIdUri="urn:mpeg:dash:mp4protection:2011"
                         value="cbcs"
                         cenc:default_KID="11111111-1111-1111-1111-111111111111"/>
      <Representation id="a1" bandwidth="128000" codecs="mp4a.40.2"
                      audioSamplingRate="48000">
        <SegmentTemplate timescale="48000"
                         initialization="a_init.mp4"
                         media="a_$Number$.m4s" startNumber="1">
          <SegmentTimeline><S t="0" d="48000" r="9"/></SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
</MPD>)";

    /// @brief Refreshed manifest where the new period p2 carries the same
    ///        KID_A on both video and audio → no key rotation expected.
    static constexpr const char *kManifestSameKID = R"(<?xml version="1.0" encoding="utf-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011"
     xmlns:cenc="urn:mpeg:cenc:2013"
     profiles="urn:mpeg:dash:profile:isoff-live:2011"
     type="dynamic" minimumUpdatePeriod="PT5S" minBufferTime="PT2S">
  <Period id="p1" start="PT0S">
    <AdaptationSet id="10001" contentType="video" mimeType="video/mp4">
      <ContentProtection schemeIdUri="urn:mpeg:dash:mp4protection:2011"
                         value="cbcs"
                         cenc:default_KID="11111111-1111-1111-1111-111111111111"/>
      <Representation id="v1" bandwidth="800000" codecs="avc1.640028"
                      width="1280" height="720">
        <SegmentTemplate timescale="90000"
                         initialization="v_init.mp4"
                         media="v_$Number$.m4s" startNumber="1">
          <SegmentTimeline><S t="0" d="90000" r="9"/></SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
    <AdaptationSet id="20001" contentType="audio" mimeType="audio/mp4">
      <ContentProtection schemeIdUri="urn:mpeg:dash:mp4protection:2011"
                         value="cbcs"
                         cenc:default_KID="11111111-1111-1111-1111-111111111111"/>
      <Representation id="a1" bandwidth="128000" codecs="mp4a.40.2"
                      audioSamplingRate="48000">
        <SegmentTemplate timescale="48000"
                         initialization="a_init.mp4"
                         media="a_$Number$.m4s" startNumber="1">
          <SegmentTimeline><S t="0" d="48000" r="9"/></SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
  <Period id="p2" start="PT30S">
    <AdaptationSet id="10001" contentType="video" mimeType="video/mp4">
      <ContentProtection schemeIdUri="urn:mpeg:dash:mp4protection:2011"
                         value="cbcs"
                         cenc:default_KID="11111111-1111-1111-1111-111111111111"/>
      <Representation id="v2" bandwidth="800000" codecs="avc1.640028"
                      width="1280" height="720">
        <SegmentTemplate timescale="90000"
                         initialization="v2_init.mp4"
                         media="v2_$Number$.m4s" startNumber="1">
          <SegmentTimeline><S t="0" d="90000" r="9"/></SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
    <AdaptationSet id="20001" contentType="audio" mimeType="audio/mp4">
      <ContentProtection schemeIdUri="urn:mpeg:dash:mp4protection:2011"
                         value="cbcs"
                         cenc:default_KID="11111111-1111-1111-1111-111111111111"/>
      <Representation id="a2" bandwidth="128000" codecs="mp4a.40.2"
                      audioSamplingRate="48000">
        <SegmentTemplate timescale="48000"
                         initialization="a2_init.mp4"
                         media="a2_$Number$.m4s" startNumber="1">
          <SegmentTimeline><S t="0" d="48000" r="9"/></SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
</MPD>)";

    /// @brief Refreshed manifest where the new period p2 has KID_B on both
    ///        video (id=10001) and audio (id=20001) adaptation sets →
    ///        key-rotation detection fires for both track types.
    //
    // KID_B = "22222222-2222-2222-2222-222222222222"
    static constexpr const char *kManifestDiffBothKID = R"(<?xml version="1.0" encoding="utf-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011"
     xmlns:cenc="urn:mpeg:cenc:2013"
     profiles="urn:mpeg:dash:profile:isoff-live:2011"
     type="dynamic" minimumUpdatePeriod="PT5S" minBufferTime="PT2S">
  <Period id="p1" start="PT0S">
    <AdaptationSet id="10001" contentType="video" mimeType="video/mp4">
      <ContentProtection schemeIdUri="urn:mpeg:dash:mp4protection:2011"
                         value="cbcs"
                         cenc:default_KID="11111111-1111-1111-1111-111111111111"/>
      <Representation id="v1" bandwidth="800000" codecs="avc1.640028"
                      width="1280" height="720">
        <SegmentTemplate timescale="90000"
                         initialization="v_init.mp4"
                         media="v_$Number$.m4s" startNumber="1">
          <SegmentTimeline><S t="0" d="90000" r="9"/></SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
    <AdaptationSet id="20001" contentType="audio" mimeType="audio/mp4">
      <ContentProtection schemeIdUri="urn:mpeg:dash:mp4protection:2011"
                         value="cbcs"
                         cenc:default_KID="11111111-1111-1111-1111-111111111111"/>
      <Representation id="a1" bandwidth="128000" codecs="mp4a.40.2"
                      audioSamplingRate="48000">
        <SegmentTemplate timescale="48000"
                         initialization="a_init.mp4"
                         media="a_$Number$.m4s" startNumber="1">
          <SegmentTimeline><S t="0" d="48000" r="9"/></SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
  <Period id="p2" start="PT30S">
    <AdaptationSet id="10001" contentType="video" mimeType="video/mp4">
      <ContentProtection schemeIdUri="urn:mpeg:dash:mp4protection:2011"
                         value="cbcs"
                         cenc:default_KID="22222222-2222-2222-2222-222222222222"/>
      <Representation id="v2" bandwidth="800000" codecs="avc1.640028"
                      width="1280" height="720">
        <SegmentTemplate timescale="90000"
                         initialization="v2_init.mp4"
                         media="v2_$Number$.m4s" startNumber="1">
          <SegmentTimeline><S t="0" d="90000" r="9"/></SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
    <AdaptationSet id="20001" contentType="audio" mimeType="audio/mp4">
      <ContentProtection schemeIdUri="urn:mpeg:dash:mp4protection:2011"
                         value="cbcs"
                         cenc:default_KID="22222222-2222-2222-2222-222222222222"/>
      <Representation id="a2" bandwidth="128000" codecs="mp4a.40.2"
                      audioSamplingRate="48000">
        <SegmentTemplate timescale="48000"
                         initialization="a2_init.mp4"
                         media="a2_$Number$.m4s" startNumber="1">
          <SegmentTimeline><S t="0" d="48000" r="9"/></SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
</MPD>)";

    /// @brief Refreshed manifest where the new period p2 has KID_B on the
    ///        video adaptation set (id=10001) but keeps KID_A on audio
    ///        (id=20001) → only video triggers key-rotation detection.
    static constexpr const char *kManifestDiffVideoKID = R"(<?xml version="1.0" encoding="utf-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011"
     xmlns:cenc="urn:mpeg:cenc:2013"
     profiles="urn:mpeg:dash:profile:isoff-live:2011"
     type="dynamic" minimumUpdatePeriod="PT5S" minBufferTime="PT2S">
  <Period id="p1" start="PT0S">
    <AdaptationSet id="10001" contentType="video" mimeType="video/mp4">
      <ContentProtection schemeIdUri="urn:mpeg:dash:mp4protection:2011"
                         value="cbcs"
                         cenc:default_KID="11111111-1111-1111-1111-111111111111"/>
      <Representation id="v1" bandwidth="800000" codecs="avc1.640028"
                      width="1280" height="720">
        <SegmentTemplate timescale="90000"
                         initialization="v_init.mp4"
                         media="v_$Number$.m4s" startNumber="1">
          <SegmentTimeline><S t="0" d="90000" r="9"/></SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
    <AdaptationSet id="20001" contentType="audio" mimeType="audio/mp4">
      <ContentProtection schemeIdUri="urn:mpeg:dash:mp4protection:2011"
                         value="cbcs"
                         cenc:default_KID="11111111-1111-1111-1111-111111111111"/>
      <Representation id="a1" bandwidth="128000" codecs="mp4a.40.2"
                      audioSamplingRate="48000">
        <SegmentTemplate timescale="48000"
                         initialization="a_init.mp4"
                         media="a_$Number$.m4s" startNumber="1">
          <SegmentTimeline><S t="0" d="48000" r="9"/></SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
  <Period id="p2" start="PT30S">
    <AdaptationSet id="10001" contentType="video" mimeType="video/mp4">
      <ContentProtection schemeIdUri="urn:mpeg:dash:mp4protection:2011"
                         value="cbcs"
                         cenc:default_KID="22222222-2222-2222-2222-222222222222"/>
      <Representation id="v2" bandwidth="800000" codecs="avc1.640028"
                      width="1280" height="720">
        <SegmentTemplate timescale="90000"
                         initialization="v2_init.mp4"
                         media="v2_$Number$.m4s" startNumber="1">
          <SegmentTimeline><S t="0" d="90000" r="9"/></SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
    <AdaptationSet id="20001" contentType="audio" mimeType="audio/mp4">
      <ContentProtection schemeIdUri="urn:mpeg:dash:mp4protection:2011"
                         value="cbcs"
                         cenc:default_KID="11111111-1111-1111-1111-111111111111"/>
      <Representation id="a2" bandwidth="128000" codecs="mp4a.40.2"
                      audioSamplingRate="48000">
        <SegmentTemplate timescale="48000"
                         initialization="a2_init.mp4"
                         media="a2_$Number$.m4s" startNumber="1">
          <SegmentTimeline><S t="0" d="48000" r="9"/></SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
</MPD>)";

    void SetUp() override
    {
        if (gpGlobalConfig == nullptr)
        {
            gpGlobalConfig = new AampConfig();
        }
        mPrivateInstanceAAMP = new PrivateInstanceAAMP(gpGlobalConfig);

        g_mockAampConfig = new NiceMock<MockAampConfig>();
        g_mockPrivateInstanceAAMP = new NiceMock<MockPrivateInstanceAAMP>();
        g_mockMediaStreamContext = new NiceMock<MockMediaStreamContext>();
        g_mockAampMPDDownloader = new NiceMock<MockAampMPDDownloader>();

        mMPD = new TestableStreamAbstractionAAMP_MPD(
            mPrivateInstanceAAMP, 0.0, AAMP_NORMAL_PLAY_RATE);
        mCdaiObj = new CDAIObjectMPD(mPrivateInstanceAAMP);
        mMPD->SetCDAIObject(mCdaiObj);

        // Seed the component with the reference manifest so every test starts
        // from the same known-good previous state (period p1, KID_A).
        SetupPreviousManifest(kPrevManifest);
    }

    void TearDown() override
    {
        mPrivateInstanceAAMP->GetAampTrackWorkerManager()->RemoveWorkers();

        delete mMPD;
        mMPD = nullptr;

        delete mCdaiObj;
        mCdaiObj = nullptr;

        delete mPrivateInstanceAAMP;
        mPrivateInstanceAAMP = nullptr;

        delete gpGlobalConfig;
        gpGlobalConfig = nullptr;

        delete g_mockAampConfig;
        g_mockAampConfig = nullptr;

        delete g_mockPrivateInstanceAAMP;
        g_mockPrivateInstanceAAMP = nullptr;

        delete g_mockMediaStreamContext;
        g_mockMediaStreamContext = nullptr;

        delete g_mockAampMPDDownloader;
        g_mockAampMPDDownloader = nullptr;

        mPrevManifestResponse.reset();
    }

    /**
     * @brief Parse a manifest XML string into a ManifestDownloadResponsePtr.
     *
     * Mimics the production flow: libxml2 → MPDProcessNode → ToMPD() →
     * AampMPDParseHelper::Initialize().
     */
    ManifestDownloadResponsePtr ParseManifest(const char *manifest)
    {
        ManifestDownloadResponsePtr response = MakeSharedManifestDownloadResponsePtr();
        response->mMPDStatus = AAMPStatusType::eAAMPSTATUS_OK;
        response->mMPDDownloadResponse->iHttpRetValue = 200;
        response->mMPDDownloadResponse->sEffectiveUrl = std::string(TEST_MANIFEST_URL);
        response->mMPDDownloadResponse->mDownloadData.assign(
            manifest, manifest + strlen(manifest));

        std::string manifestStr(manifest);
        xmlTextReaderPtr reader = xmlReaderForMemory(
            (char *)manifestStr.c_str(), (int)manifestStr.length(),
            NULL, NULL, 0);
        if (reader != NULL)
        {
            if (xmlTextReaderRead(reader))
            {
                response->mRootNode = MPDProcessNode(&reader, TEST_MANIFEST_URL);
                if (response->mRootNode != NULL)
                {
                    auto *mpd = response->mRootNode->ToMPD();
                    if (mpd != nullptr)
                    {
                        std::shared_ptr<dash::mpd::IMPD> tmp_ptr(mpd);
                        response->mMPDInstance = tmp_ptr;
                        response->GetMPDParseHelper()->Initialize(mpd);
                    }
                }
            }
            xmlFreeTextReader(reader);
        }
        return response;
    }

    /**
     * @brief Parse the "previous" manifest and install its parse helper into
     *        the MPD object, simulating a completed initial tune.
     */
    void SetupPreviousManifest(const char *manifest)
    {
        // Store the response as a member so the IMPD shared_ptr is kept alive
        // for the lifetime of the test.  AampMPDParseHelper holds only a raw
        // IMPD* so the owner must not be destroyed before the test body ends.
        mPrevManifestResponse = ParseManifest(manifest);
        mMPD->SetParseHelper(mPrevManifestResponse->GetMPDParseHelper());
    }
};

// ---------------------------------------------------------------------------
// Test cases
// ---------------------------------------------------------------------------

/**
 * @brief When the refreshed manifest adds a new period (p2) whose
 *        cenc:default_KID is identical to the reference period (p1) on both
 *        video (id=10001) and audio (id=20001) adaptation sets, no key
 *        rotation should be detected.
 *
 * Precondition:  mMPDParseHelper is seeded with kPrevManifest (p1, KID_A).
 * Input:         Refreshed manifest kManifestSameKID (p1 + p2, both KID_A).
 * Expected:      mKeyRotationEarlyDetectedPeriodIds remains empty.
 */
TEST_F(DetectKeyRotationTests, SameDefaultKeyIdInBothPeriods_NoKeyRotationDetected)
{
    SetupPreviousManifest(kPrevManifest);

    auto newResp = ParseManifest(kManifestSameKID);
    mMPD->CallDetectKeyRotation(newResp);

    EXPECT_TRUE(mMPD->GetDetectedIds().empty())
        << "No key rotation expected when both periods share the same default KID";
}

/**
 * @brief When the refreshed manifest adds a new period (p2) whose video
 *        adaptation set (id=10001) carries a different cenc:default_KID
 *        (KID_B) while the audio adaptation set (id=20001) keeps the
 *        original KID_A, only the video adaptation set should trigger
 *        key-rotation detection.
 *
 * Precondition:  mMPDParseHelper is seeded with kPrevManifest (p1, KID_A).
 * Input:         Refreshed manifest kManifestDiffVideoKID
 *                (p1 KID_A; p2 video KID_B, p2 audio KID_A).
 * Expected:      Exactly one dedup entry "p2:10001" is recorded; the audio
 *                adaptation set (id=20001) produces no entry.
 */
TEST_F(DetectKeyRotationTests, DifferentDefaultKeyIdForVideoAdaptationSet_KeyRotationDetected)
{
    AAMPLOG_INFO("Testing with refreshed manifest where video adaptation set has a different default KID than the previous manifest");
    SetupPreviousManifest(kPrevManifest);
    AAMPLOG_INFO("Previous manifest parsed and set up with period p1 having KID_A on both video and audio adaptation sets");
    auto newResp = ParseManifest(kManifestDiffVideoKID);
    mMPD->CallDetectKeyRotation(newResp);

    const auto &ids = mMPD->GetDetectedIds();
    ASSERT_EQ(ids.size(), 1u)
        << "Expected exactly one detection for the video adaptation set";
    // Dedup key format: periodId + ":" + std::to_string(mediaType integer)
    // eMEDIATYPE_VIDEO == 0
    EXPECT_EQ(ids[0], "p2:0")
        << "Key rotation should be detected for the video media type (eMEDIATYPE_VIDEO=0)";
}

/**
 * @brief When the refreshed manifest adds a new period (p2) where BOTH the
 *        video adaptation set (id=10001) and the audio adaptation set
 *        (id=20001) carry a different cenc:default_KID (KID_B) compared to
 *        the reference period (p1, KID_A), key-rotation detection must fire
 *        for both track types.
 *
 * Precondition:  mMPDParseHelper is seeded with kPrevManifest (p1, KID_A).
 * Input:         Refreshed manifest kManifestDiffBothKID
 *                (p1 KID_A; p2 video KID_B, p2 audio KID_B).
 * Expected:      Exactly two dedup entries: "p2:0" (video) and "p2:1" (audio).
 */
TEST_F(DetectKeyRotationTests, BothVideoAndAudioHaveDifferentDefaultKeyId_BothDetected)
{
    auto newResp = ParseManifest(kManifestDiffBothKID);
    mMPD->CallDetectKeyRotation(newResp);

    const auto &ids = mMPD->GetDetectedIds();
    ASSERT_EQ(ids.size(), 2u)
        << "Expected key rotation detected for both video and audio adaptation sets";

    // Dedup key format: periodId + ":" + std::to_string(mediaType integer)
    // eMEDIATYPE_VIDEO == 0, eMEDIATYPE_AUDIO == 1
    const bool hasVideo = (std::find(ids.begin(), ids.end(), "p2:0") != ids.end());
    const bool hasAudio = (std::find(ids.begin(), ids.end(), "p2:1") != ids.end());
    EXPECT_TRUE(hasVideo) << "Key rotation not detected for video (expected \"p2:0\")";
    EXPECT_TRUE(hasAudio) << "Key rotation not detected for audio (expected \"p2:1\")";
}
