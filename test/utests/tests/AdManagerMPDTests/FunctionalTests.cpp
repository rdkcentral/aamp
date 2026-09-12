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

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <atomic>
#include <chrono>
#include <future>
#include <limits>
#include <memory>
#include <thread>
#include <mutex>
#include "priv_aamp.h"
#include "AampConfig.h"
#include "AampUtils.h"
#include "AampLogManager.h"
#include "admanager_mpd.h"
#include "MockPrivateInstanceAAMP.h"
#include "AampMPDUtils.h"

#include "libdash/IMPD.h"
#include "libdash/INode.h"
#include "libdash/IDASHManager.h"
#include "libdash/IProducerReferenceTime.h"
#include "libdash/xml/Node.h"
#include "libdash/helpers/Time.h"
#include "libdash/xml/DOMParser.h"
#include <libxml/xmlreader.h>

using ::testing::_;
using ::testing::Return;
using ::testing::StrictMock;
using ::testing::WithArgs;
using ::testing::Invoke;
using ::testing::StrEq;
using ::testing::DoAll;
using ::testing::InvokeWithoutArgs;

using namespace dash::xml;
using namespace dash::mpd;

AampConfig *gpGlobalConfig{nullptr};

// Shared 10-second ad manifest used across SetAlternateContents and static-manifest tests.
static const char *kSharedTenSecondAdManifest =
R"(<?xml version="1.0" encoding="UTF-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" xmlns:scte35="urn:scte:scte35:2014:xml+bin" xmlns:scte214="scte214" xmlns:cenc="urn:mpeg:cenc:2013" xmlns:mspr="mspr" type="static" id="TSS_ICEJ010_010-LIN_c4_HD" profiles="urn:mpeg:dash:profile:isoff-on-demand:2011" minBufferTime="PT0H0M1.000S" maxSegmentDuration="PT0H0M1S" mediaPresentationDuration="PT0H0M10.027S">
  <Period id="1" start="PT0H0M0.000S">
    <AdaptationSet id="1" contentType="video" mimeType="video/mp4" segmentAlignment="true" startWithSAP="1">
      <Role schemeIdUri="urn:mpeg:dash:role:2011" value="main"/>
      <SegmentTemplate initialization="manifest/track-video-repid-$RepresentationID$-tc--header.mp4" media="manifest/track-video-repid-$RepresentationID$-tc--frag-$Number$.mp4" timescale="48000" startNumber="0">
        <SegmentTimeline>
          <S t="0" d="92160" r="3"/>
          <S t="368640" d="111360" r="0"/>
        </SegmentTimeline>
      </SegmentTemplate>
      <Representation id="LE5" bandwidth="5250000" codecs="hvc1.1.6.L123.b0" width="1920" height="1080" frameRate="50">
      </Representation>
    </AdaptationSet>
    <AdaptationSet id="2" contentType="audio" mimeType="audio/mp4" lang="en">
      <AudioChannelConfiguration schemeIdUri="tag:dolby.com,2014:dash:audio_channel_configuration:2011" value="a000"/>
      <Role schemeIdUri="urn:mpeg:dash:role:2011" value="main"/>
      <SegmentTemplate initialization="manifest-eac3/track-audio-repid-$RepresentationID$-tc--header.mp4" media="manifest-eac3/track-audio-repid-$RepresentationID$-tc--frag-$Number$.mp4" timescale="48000" startNumber="0">
        <SegmentTimeline>
          <S t="0" d="92160" r="3"/>
          <S t="368640" d="112128" r="0"/>
        </SegmentTimeline>
      </SegmentTemplate>
      <Representation id="DDen" bandwidth="99450" codecs="ec-3" audioSamplingRate="48000">
      </Representation>
    </AdaptationSet>
  </Period>
</MPD>
)";

/**
 * @brief AdManagerMPDTests tests common base class.
 */
class AdManagerMPDTests : public ::testing::Test
{
protected:
  PrivateInstanceAAMP *mPrivateInstanceAAMP;
  CDAIObjectMPD *mCdaiObj;
  PrivateCDAIObjectMPD* mPrivateCDAIObjectMPD;
  const char* mManifest;
  IMPD *mMPD;
  AampMPDParseHelperPtr mAdMPDParseHelper;
  static constexpr const char *TEST_AD_MANIFEST_URL = "http://host/ad/manifest.mpd";
  static constexpr const char *TEST_AD_MANIFEST_HOST = "http://host/ad/";
  static constexpr const char *TEST_FOG_AD_MANIFEST_URL = "http://127.0.0.1:9080/adrec?clientId=FOG_AAMP&recordedUrl=http%3A%2F%2Fhost%2Fad%2Fmanifest.mpd";
    static constexpr const char *TEST_FOG_AD_MANIFEST_HOST = "http://127.0.0.1:9080/";
  static constexpr const char *TEST_FOG_MAIN_MANIFEST_URL = "http://127.0.0.1:9080/recording/manifest.mpd";

  void SetUp()
  {
    if(gpGlobalConfig == nullptr)
    {
      gpGlobalConfig =  new AampConfig();
    }

    mPrivateInstanceAAMP = new PrivateInstanceAAMP(gpGlobalConfig);
    AampLogManager::setLogLevel(eLOGLEVEL_TRACE);
    AampLogManager::lockLogLevel(true);

    g_mockPrivateInstanceAAMP = std::make_shared<StrictMock<MockPrivateInstanceAAMP>>();

    EXPECT_CALL(*g_mockPrivateInstanceAAMP, DownloadsAreEnabled()).WillRepeatedly(Return(true));
    mCdaiObj = new CDAIObjectMPD(mPrivateInstanceAAMP);
    mPrivateCDAIObjectMPD = mCdaiObj->GetPrivateCDAIObjectMPD();
    EXPECT_TRUE(mPrivateCDAIObjectMPD->mAdObjThreadID.joinable());

    mManifest = nullptr;
    mMPD = nullptr;
    mAdMPDParseHelper = nullptr;
  }

  void TearDown()
  {
    delete mCdaiObj;
    mCdaiObj = nullptr;
    mPrivateCDAIObjectMPD = nullptr;

    g_mockPrivateInstanceAAMP.reset();

    delete mPrivateInstanceAAMP;
    mPrivateInstanceAAMP = nullptr;

    delete gpGlobalConfig;
    gpGlobalConfig = nullptr;

    mAdMPDParseHelper = nullptr;

    if (mMPD)
    {
      delete mMPD;
      mMPD = nullptr;
    }

    mManifest = nullptr;
  }

public:
  bool GetManifest(std::string remoteUrl, std::vector<uint8_t> &buffer, std::string& effectiveUrl, int& httpError)
  {
    /* Setup fake buffer contents. */
    buffer.clear();
    buffer.assign(mManifest, mManifest + strlen(mManifest));
    effectiveUrl = remoteUrl;
    httpError = 200;

    return true;
  }

  void InitializeAdMPD(const char *manifest, bool isFOG = false, bool fogDownloadSuccess = true, int count = 1, int httpError = 404)
  {
    std::string adManifestUrl = TEST_AD_MANIFEST_URL;
    if (manifest)
    {
      mManifest = manifest;
      mPrivateInstanceAAMP->rate = AAMP_NORMAL_PLAY_RATE;
      // remoteUrl, manifest, effectiveUrl
      EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetFile (adManifestUrl, _,_ , _, _, _, _, _, _, _, _, _, _, _, _))
              .Times(count)
              .WillRepeatedly(WithArgs<0,2,3,4>(Invoke(this, &AdManagerMPDTests::GetManifest)));
      if (isFOG)
      {
        // If the ADs are to be recorded by FOG, then the manifest will be downloaded again from FOG
        // remoteUrl, manifest, effectiveUrl, httpError
        std::string adFogManifestUrl = TEST_FOG_AD_MANIFEST_URL;
        if (fogDownloadSuccess)
        {
          EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetFile (adFogManifestUrl, _, _, _, _, _, _, _, _, _, _, _, _, _, _))
              .WillOnce(WithArgs<0,2,3,4>(Invoke(this, &AdManagerMPDTests::GetManifest)));
        }
        else
        {
          EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetFile (adFogManifestUrl, _, _, _, _, _, _, _, _, _, _, _, _, _, _))
              .WillOnce(Return(false));
        }
      }
    }
    else
    {
      EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetFile (adManifestUrl, _, _, _, _, _, _, _, _, _, _, _, _, _, _))
              .WillOnce(WithArgs<4>(Invoke([httpError](int& err){
                err = httpError;
                return false;
              })));
    }
  }

  void ProcessSourceMPD(const char* manifest)
  {
    if (manifest)
    {
      mManifest = manifest;
      std::string manifestStr = mManifest;
      xmlTextReaderPtr reader = xmlReaderForMemory( (char *)manifestStr.c_str(), (int) manifestStr.length(), NULL, NULL, 0);
      if (reader != NULL)
      {
        if (xmlTextReaderRead(reader))
        {
          Node *rootNode = MPDProcessNode(&reader, TEST_AD_MANIFEST_URL);
          if(rootNode != NULL)
          {
            if (mMPD)
            {
              delete mMPD;
              mMPD = nullptr;
            }
            mMPD = rootNode->ToMPD();
            delete rootNode;
          }
        }
      }
      xmlFreeTextReader(reader);
      if (mMPD)
      {
        mAdMPDParseHelper = std::make_shared<AampMPDParseHelper>();
        mAdMPDParseHelper->Initialize(mMPD);
      }
    }
  }

  /**
   * @brief Constructs the ad initialization URI based on the host and path.
   * @param host The host URL
   * @param path The path to the ad manifest
   *
   * @return The complete ad initialization URL
   */
  std::string GetFullURI(const std::string &host, const std::string &path)
  {
    return host + path;
  }
};

/**
 * @brief Tests the functionality of inserting into the period map.
 *
 * This test case verifies that the insertion of elements into the period map is working correctly.
 * It checks if the elements are inserted in the expected order and if the map size is updated accordingly.
 *
 * @note This test case is part of the AdManagerMPDTests fixture.
 */
TEST_F(AdManagerMPDTests, InsertToPeriodMapTest)
{
  // Create a mock period
  Period period;
  period.SetId("testPeriodId");

  // Call the function to test
  mPrivateCDAIObjectMPD->InsertToPeriodMap(&period);

  // Verify the result
  ASSERT_TRUE(mPrivateCDAIObjectMPD->isPeriodExist("testPeriodId"));
  ASSERT_FALSE(mPrivateCDAIObjectMPD->isPeriodExist("missingPeriodId"));
}

/**
 * @brief Tests the functionality of pruning the period maps.
 *
 * This test case verifies that the `PrunePeriodMaps` function correctly removes ad breaks and periods
 * that are not present in the provided list of new period IDs.
 * This test case also verifies that the `ClearMaps` function correctly removes all ad breaks and periods
 *
 * @note This test case is part of the AdManagerMPDTests fixture.
 */
TEST_F(AdManagerMPDTests, PrunePeriodMapsTest)
{
  // Add some ad breaks and periods to the period maps
  mPrivateCDAIObjectMPD->mAdBreaks = {
    {"testPeriodId1", AdBreakObject()},
    {"testPeriodId2", AdBreakObject()},
    {"testPeriodId3", AdBreakObject()}
  };
  mPrivateCDAIObjectMPD->mPeriodMap = {
    {"testPeriodId1", Period2AdData()},
    {"testPeriodId2", Period2AdData()},
    {"testPeriodId3", Period2AdData()}
  };

  // Create a new period ID list
  std::vector<std::string> newPeriodIds = {"testPeriodId2", "testPeriodId3"};

  // Call the function to test
  mPrivateCDAIObjectMPD->PrunePeriodMaps(newPeriodIds);

  // Verify the result
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks.size(), 2);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap.size(), 2);
  EXPECT_TRUE(mPrivateCDAIObjectMPD->isAdBreakObjectExist("testPeriodId2"));
  EXPECT_TRUE(mPrivateCDAIObjectMPD->isAdBreakObjectExist("testPeriodId3"));
  EXPECT_TRUE(mPrivateCDAIObjectMPD->isPeriodExist("testPeriodId2"));
  EXPECT_TRUE(mPrivateCDAIObjectMPD->isPeriodExist("testPeriodId3"));

  EXPECT_FALSE(mPrivateCDAIObjectMPD->isAdBreakObjectExist("testPeriodId1"));
  EXPECT_FALSE(mPrivateCDAIObjectMPD->isPeriodExist("testPeriodId1"));

  // Call the function to test
  mPrivateCDAIObjectMPD->ClearMaps();
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks.size(), 0);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap.size(), 0);
}

/**
 * @brief Tests the functionality of the UpdatePlacementObj function when adBrkId equals endPeriodId.
 *
 * This test case verifies that the `UpdatePlacementObj` function correctly sets the next placement object
 * when the ad break ID is the same as the end period ID.
 *
 * @note This test case is part of the AdManagerMPDTests fixture.
 */
TEST_F(AdManagerMPDTests, UpdatePlacementObjTest)
{
  // Empty mAdtoInsertInNextBreakVec
  // Call the function to test
  PlacementObj nextPlacementObj = mPrivateCDAIObjectMPD->UpdatePlacementObj("testAdBrkId1", "testAdBrkId1");
  // Verify the result
  EXPECT_EQ(nextPlacementObj.pendingAdbrkId, "");

  // Call the function to test
  nextPlacementObj = mPrivateCDAIObjectMPD->UpdatePlacementObj("testAdBrkId1", "testAdBrkId2");
  // Verify the result
  EXPECT_EQ(nextPlacementObj.pendingAdbrkId, "");

  // Initialize mAdtoInsertInNextBreakVec
  mPrivateCDAIObjectMPD->mAdtoInsertInNextBreakVec = {
      {"testAdBrkId1", "testAdBrkId1", 0, 0, 0, 0, false},
      {"testAdBrkId2", "testAdBrkId2", 0, 0, 0, 0, false},
      {"testAdBrkId3", "testAdBrkId3", 0, 0, 0, 0, false}
  };

  // Call the function to test when adBrkId equals endPeriodId
  nextPlacementObj = mPrivateCDAIObjectMPD->UpdatePlacementObj("testAdBrkId1", "testAdBrkId1");
  // Verify the result
  EXPECT_EQ(nextPlacementObj.pendingAdbrkId, "testAdBrkId2");


  // Call the function to test when adBrkId not equals to endPeriodId
  nextPlacementObj = mPrivateCDAIObjectMPD->UpdatePlacementObj("testAdBrkId2", "testAdBrkId3");
  // Verify the result
  EXPECT_EQ(nextPlacementObj.pendingAdbrkId, "testAdBrkId3");

  // Call the function to test endPeriodId not available in mAdtoInsertInNextBreakVec
  nextPlacementObj = mPrivateCDAIObjectMPD->UpdatePlacementObj("testAdBrkId2", "testAdBrkId4");
  // Verify the result
  EXPECT_EQ(nextPlacementObj.pendingAdbrkId, "testAdBrkId3");

  // Call the function to test endPeriodId not available and end of mAdtoInsertInNextBreakVec
  nextPlacementObj = mPrivateCDAIObjectMPD->UpdatePlacementObj("testAdBrkId3", "testAdBrkId4");
  // Verify the result
  EXPECT_EQ(nextPlacementObj.pendingAdbrkId, "");

  // Call the function to test adBrkId equals endPeriodId and end of mAdtoInsertInNextBreakVec
  nextPlacementObj = mPrivateCDAIObjectMPD->UpdatePlacementObj("testAdBrkId3", "testAdBrkId3");
  // Verify the result
  EXPECT_EQ(nextPlacementObj.pendingAdbrkId, "");
}

/**
 * @brief Tests the functionality of the SetAlternateContents method
 * 1. when adId and url are empty.
 * 2. when adId and url are not empty and ad break object doesn't exists.
 */
TEST_F(AdManagerMPDTests, SetAlternateContentsTests_1)
{
  std::string periodId = "testPeriodId";
  std::string adId = "";
  std::string url = "";
  uint64_t startMS = 0;
  uint32_t breakdur = 0;
  AAMPCDAIError adErrorCode = eCDAI_ERROR_UNKNOWN;
  // Call the function to test
  mPrivateCDAIObjectMPD->SetAlternateContents(periodId, adId, url, startMS, breakdur);

  // Verify the result
  EXPECT_TRUE(mPrivateCDAIObjectMPD->isAdBreakObjectExist(periodId));

  // New periodId which is not present in mAdBreaks
  periodId = "testPeriodId1";
  adId = "testAdId1";
  url = "testAdUrl1";
  EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendAdResolvedEvent(adId, false, 0, 0, adErrorCode)).Times(1);
  // Call the function to test when adbreak object doesn't exist and adId and url not empty
  mPrivateCDAIObjectMPD->SetAlternateContents(periodId, adId, url, startMS, breakdur);

  // Verify the result
  EXPECT_FALSE(mPrivateCDAIObjectMPD->isAdBreakObjectExist(periodId));

}

/**
 * @brief Tests the functionality of the SetAlternateContents method when adId and url are not empty and ad break object exists.
 */
TEST_F(AdManagerMPDTests, SetAlternateContentsTests_2)
{
  const char *manifest = kSharedTenSecondAdManifest;
  std::string periodId = "testPeriodId";
  std::string adId = "testAdId";
  std::string url = "";
  uint64_t startMS = 0;
  uint32_t breakdur = 10000;
  bool timedout = false;
  bool threadStarted = false;
  AAMPCDAIError adErrorCode = eCDAI_ERROR_NONE;

  // To create an empty ad break object
  mPrivateCDAIObjectMPD->SetAlternateContents(periodId, "", "", startMS, breakdur);

  url = TEST_AD_MANIFEST_URL;
  InitializeAdMPD(manifest);

  // mIsFogTSB is false, so downloaded from CDN and ad resolved event is sent
  auto adResolvedPromise = std::make_shared<std::promise<void>>();
  auto adResolvedFuture = adResolvedPromise->get_future();
  EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendAdResolvedEvent(adId, true, startMS, 10000, adErrorCode))
      .Times(1)
      .WillOnce(InvokeWithoutArgs([adResolvedPromise]{ adResolvedPromise->set_value(); }));

  // We would like to also validate that AbortWaitForNextAdResolved is invoked
  std::thread t([this, &timedout, &threadStarted]{
    // wait for 1sec on the conditional signal to confirm it doesn't timeout
    std::unique_lock<std::mutex> lock(this->mPrivateCDAIObjectMPD->mAdPlacementMtx);
    threadStarted = true;
    timedout = (std::cv_status::timeout == this->mPrivateCDAIObjectMPD->mAdPlacementCV.wait_for(lock, std::chrono::milliseconds(2000)));
  });
  // wait till t starts
  while(!threadStarted)
  {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  std::string adInitUrl = GetFullURI(TEST_AD_MANIFEST_HOST, "manifest/track-video-repid-LE5-tc--header.mp4");
  EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetFile (adInitUrl, _, _, _, _, _, _, _, _, _, _, _, _, _, _))
              .WillOnce(Return(true));
  adInitUrl = GetFullURI(TEST_AD_MANIFEST_HOST, "manifest-eac3/track-audio-repid-DDen-tc--header.mp4");
  EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetFile (adInitUrl, _, _, _, _, _, _, _, _, _, _, _, _, _, _))
              .WillOnce(Return(true));
  mPrivateCDAIObjectMPD->SetAlternateContents(periodId, adId, url, startMS, breakdur);
  const auto adResolvedStatus = adResolvedFuture.wait_for(std::chrono::seconds(5));
  t.join(); // join unconditionally before asserting to avoid std::terminate on test failure
  ASSERT_EQ(adResolvedStatus, std::future_status::ready);

  // Verify the result
  // mAdBreak updated and placementObj created
  EXPECT_TRUE(mPrivateCDAIObjectMPD->mAdObjThreadID.joinable());
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPlacementObj.pendingAdbrkId, periodId);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdtoInsertInNextBreakVec.size(), 1);
  EXPECT_EQ((mPrivateCDAIObjectMPD->mAdBreaks[periodId].ads)->size(), 1);
  EXPECT_TRUE((mPrivateCDAIObjectMPD->mAdBreaks[periodId].ads)->at(0).resolved);
  EXPECT_FALSE((mPrivateCDAIObjectMPD->mAdBreaks[periodId].ads)->at(0).invalid);
  // Make sure that WaitForNextAdResolved returns true since the ad is resolved
  EXPECT_TRUE(mPrivateCDAIObjectMPD->WaitForNextAdResolved(50));
  // Make sure we didn't timeout
  EXPECT_FALSE(timedout);

}

/**
 * @brief Tests the functionality of the SetAlternateContents method for FOGTSB and ad break object exists.
 */
TEST_F(AdManagerMPDTests, SetAlternateContentsTests_3)
{
    static const char *manifest =
R"(<?xml version="1.0" encoding="UTF-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" xmlns:scte35="urn:scte:scte35:2014:xml+bin" xmlns:scte214="scte214" xmlns:cenc="urn:mpeg:cenc:2013" xmlns:mspr="mspr" type="static" id="TSS_ICEJ010_010-LIN_c4_HD" profiles="urn:mpeg:dash:profile:isoff-on-demand:2011" minBufferTime="PT0H0M1.000S" maxSegmentDuration="PT0H0M1S" mediaPresentationDuration="PT0H0M10.027S">
  <Period id="1" start="PT0H0M0.000S">
    <AdaptationSet id="1" contentType="video" mimeType="video/mp4" segmentAlignment="true" startWithSAP="1">
      <Role schemeIdUri="urn:mpeg:dash:role:2011" value="main"/>
      <SegmentTemplate initialization="manifest/track-video-repid-$RepresentationID$-tc--header.mp4" media="manifest/track-video-repid-$RepresentationID$-tc--frag-$Number$.mp4" timescale="48000" startNumber="0">
        <SegmentTimeline>
          <S t="0" d="92160" r="3"/>
          <S t="368640" d="111360" r="0"/>
        </SegmentTimeline>
      </SegmentTemplate>
      <Representation id="LE5" bandwidth="5250000" codecs="hvc1.1.6.L123.b0" width="1920" height="1080" frameRate="50">
      </Representation>
    </AdaptationSet>
    <AdaptationSet id="2" contentType="audio" mimeType="audio/mp4" lang="en">
      <AudioChannelConfiguration schemeIdUri="tag:dolby.com,2014:dash:audio_channel_configuration:2011" value="a000"/>
      <Role schemeIdUri="urn:mpeg:dash:role:2011" value="main"/>
      <SegmentTemplate initialization="manifest-eac3/track-audio-repid-$RepresentationID$-tc--header.mp4" media="manifest-eac3/track-audio-repid-$RepresentationID$-tc--frag-$Number$.mp4" timescale="48000" startNumber="0">
        <SegmentTimeline>
          <S t="0" d="92160" r="3"/>
          <S t="368640" d="112128" r="0"/>
        </SegmentTimeline>
      </SegmentTemplate>
      <Representation id="DDen" bandwidth="99450" codecs="ec-3" audioSamplingRate="48000">
      </Representation>
    </AdaptationSet>
  </Period>
</MPD>
)";
  std::string periodId = "testPeriodId";
  std::string adId = "testAdId";
  std::string url = "";
  uint64_t startMS = 0;
  uint32_t breakdur = 10000;
  AAMPCDAIError adErrorCode = eCDAI_ERROR_NONE;

  // To create an empty ad break object
  mPrivateCDAIObjectMPD->SetAlternateContents(periodId, "", "", startMS, breakdur);

  url = TEST_AD_MANIFEST_URL;
  mPrivateCDAIObjectMPD->mIsFogTSB = true;
  mPrivateInstanceAAMP->SetManifestUrl(TEST_FOG_MAIN_MANIFEST_URL);
  InitializeAdMPD(manifest, true);

  // mIsFogTSB is true, so downloaded from CDN and redirected to FOG and ad resolved event is sent
  // Use shared_ptr so the promise outlives this stack frame if wait_for times out and ASSERT
  // returns early before TearDown shuts down the background ad thread.
  auto adResolvedPromise = std::make_shared<std::promise<void>>();
  auto adResolvedFuture = adResolvedPromise->get_future();
  EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendAdResolvedEvent(adId, true, startMS, 10000, adErrorCode))
      .Times(1)
      .WillOnce(InvokeWithoutArgs([adResolvedPromise]{ adResolvedPromise->set_value(); }));

  std::string adInitUrl = GetFullURI(TEST_FOG_AD_MANIFEST_HOST, "manifest/track-video-repid-LE5-tc--header.mp4");
  EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetFile (adInitUrl, _, _, _, _, _, _, _, _, _, _, _, _, _, _))
              .WillOnce(Return(true));
  adInitUrl = GetFullURI(TEST_FOG_AD_MANIFEST_HOST, "manifest-eac3/track-audio-repid-DDen-tc--header.mp4");
  EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetFile (adInitUrl, _, _, _, _, _, _, _, _, _, _, _, _, _, _))
              .WillOnce(Return(true));
  mPrivateCDAIObjectMPD->SetAlternateContents(periodId, adId, url, startMS, breakdur);
  ASSERT_EQ(adResolvedFuture.wait_for(std::chrono::seconds(5)), std::future_status::ready);

  // Verify the result
  // mAdBreak updated and placementObj created
  EXPECT_TRUE(mPrivateCDAIObjectMPD->mAdObjThreadID.joinable());
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPlacementObj.pendingAdbrkId, periodId);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdtoInsertInNextBreakVec.size(), 1);
  EXPECT_EQ((mPrivateCDAIObjectMPD->mAdBreaks[periodId].ads)->size(), 1);
  EXPECT_STREQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId].ads->at(0).url.c_str(), TEST_FOG_AD_MANIFEST_URL);
  EXPECT_TRUE((mPrivateCDAIObjectMPD->mAdBreaks[periodId].ads)->at(0).resolved);
  EXPECT_FALSE(mPrivateCDAIObjectMPD->mAdBreaks[periodId].ads->at(0).invalid);

}

/**
 * @brief Tests the functionality of the SetAlternateContents method when ad download fails
 */
TEST_F(AdManagerMPDTests, SetAlternateContentsTests_4)
{
  std::string periodId = "testPeriodId";
  std::string adId = "testAdId";
  std::string url = "";
  uint64_t startMS = 0;
  uint32_t breakdur = 10000;
  // Empty manifest for failure
  const char *manifest = nullptr;
  bool timedout = false;
  bool threadStarted = false;

  // To create an empty ad break object
  mPrivateCDAIObjectMPD->SetAlternateContents(periodId, "", "", startMS, breakdur);

  url = TEST_AD_MANIFEST_URL;
  InitializeAdMPD(manifest);

  // mIsFogTSB is false, so downloaded from CDN and ad resolved event status should be false
  auto adResolvedPromise = std::make_shared<std::promise<void>>();
  auto adResolvedFuture = adResolvedPromise->get_future();
  EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendAdResolvedEvent(adId, false, 0, 0, eCDAI_ERROR_DELIVERY_HTTP_ERROR))
      .Times(1)
      .WillOnce(InvokeWithoutArgs([adResolvedPromise]{ adResolvedPromise->set_value(); }));

  // We would like to also validate that AbortWaitForNextAdResolved is invoked
  std::thread t([this, &timedout, &threadStarted]{
    // wait for 1sec on the conditional signal to confirm it doesn't timeout
    std::unique_lock<std::mutex> lock(this->mPrivateCDAIObjectMPD->mAdPlacementMtx);
    threadStarted = true;
    timedout = (std::cv_status::timeout == this->mPrivateCDAIObjectMPD->mAdPlacementCV.wait_for(lock, std::chrono::milliseconds(2000)));
  });
  // wait till t starts
  while(!threadStarted)
  {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  mPrivateCDAIObjectMPD->SetAlternateContents(periodId, adId, url, startMS, breakdur);
  const auto adResolvedStatus = adResolvedFuture.wait_for(std::chrono::seconds(5));
  t.join(); // join unconditionally before asserting to avoid std::terminate on test failure
  ASSERT_EQ(adResolvedStatus, std::future_status::ready);

  // Verify the result
  // mAdBreak updated and placementObj not created
  EXPECT_TRUE(mPrivateCDAIObjectMPD->mAdObjThreadID.joinable());
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPlacementObj.pendingAdbrkId, "");
  EXPECT_TRUE(mPrivateCDAIObjectMPD->mAdtoInsertInNextBreakVec.empty());
  EXPECT_EQ((mPrivateCDAIObjectMPD->mAdBreaks[periodId].ads)->size(), 1);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId].ads->at(0).adId, adId);
  EXPECT_TRUE(mPrivateCDAIObjectMPD->mAdBreaks[periodId].ads->at(0).resolved);
  EXPECT_TRUE(mPrivateCDAIObjectMPD->mAdBreaks[periodId].ads->at(0).invalid);
  // Make sure that WaitForNextAdResolved returns true since the ad is resolved
  EXPECT_TRUE(mPrivateCDAIObjectMPD->WaitForNextAdResolved(50));
  // Make sure we didn't timeout
  EXPECT_FALSE(timedout);
}

/**
 * @brief Tests the functionality of the SetAlternateContents method when ad download fails in FOG.
 */
TEST_F(AdManagerMPDTests, SetAlternateContentsTests_5)
{
    static const char *manifest =
R"(<?xml version="1.0" encoding="UTF-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" xmlns:scte35="urn:scte:scte35:2014:xml+bin" xmlns:scte214="scte214" xmlns:cenc="urn:mpeg:cenc:2013" xmlns:mspr="mspr" type="static" id="TSS_ICEJ010_010-LIN_c4_HD" profiles="urn:mpeg:dash:profile:isoff-on-demand:2011" minBufferTime="PT0H0M1.000S" maxSegmentDuration="PT0H0M1S" mediaPresentationDuration="PT0H0M10.027S">
  <Period id="1" start="PT0H0M0.000S">
    <AdaptationSet id="1" contentType="video" mimeType="video/mp4" segmentAlignment="true" startWithSAP="1">
      <Role schemeIdUri="urn:mpeg:dash:role:2011" value="main"/>
      <SegmentTemplate initialization="manifest/track-video-repid-$RepresentationID$-tc--header.mp4" media="manifest/track-video-repid-$RepresentationID$-tc--frag-$Number$.mp4" timescale="48000" startNumber="0">
        <SegmentTimeline>
          <S t="0" d="92160" r="3"/>
          <S t="368640" d="111360" r="0"/>
        </SegmentTimeline>
      </SegmentTemplate>
      <Representation id="LE5" bandwidth="5250000" codecs="hvc1.1.6.L123.b0" width="1920" height="1080" frameRate="50">
      </Representation>
    </AdaptationSet>
    <AdaptationSet id="2" contentType="audio" mimeType="audio/mp4" lang="en">
      <AudioChannelConfiguration schemeIdUri="tag:dolby.com,2014:dash:audio_channel_configuration:2011" value="a000"/>
      <Role schemeIdUri="urn:mpeg:dash:role:2011" value="main"/>
      <SegmentTemplate initialization="manifest-eac3/track-audio-repid-$RepresentationID$-tc--header.mp4" media="manifest-eac3/track-audio-repid-$RepresentationID$-tc--frag-$Number$.mp4" timescale="48000" startNumber="0">
        <SegmentTimeline>
          <S t="0" d="92160" r="3"/>
          <S t="368640" d="112128" r="0"/>
        </SegmentTimeline>
      </SegmentTemplate>
      <Representation id="DDen" bandwidth="99450" codecs="ec-3" audioSamplingRate="48000">
      </Representation>
    </AdaptationSet>
  </Period>
</MPD>
)";
  std::string periodId = "testPeriodId";
  std::string adId = "testAdId";
  std::string url = "";
  uint64_t startMS = 0;
  uint32_t breakdur = 10000;

  // To create an empty ad break object
  mPrivateCDAIObjectMPD->SetAlternateContents(periodId, "", "", startMS, breakdur);

  url = TEST_AD_MANIFEST_URL;
  mPrivateCDAIObjectMPD->mIsFogTSB = true;
  mPrivateInstanceAAMP->SetManifestUrl(TEST_FOG_MAIN_MANIFEST_URL);
  InitializeAdMPD(manifest, true, false);

  // mIsFogTSB is true, so downloaded from CDN and redirected to FOG which fails.
  // Here, ad resolved event is sent with true and CDN url is cached
  // Use shared_ptr so the promise outlives this stack frame if wait_for times out and ASSERT
  // returns early before TearDown shuts down the background ad thread.
  auto adResolvedPromise = std::make_shared<std::promise<void>>();
  auto adResolvedFuture = adResolvedPromise->get_future();
  EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendAdResolvedEvent(adId, true, startMS, 10000, eCDAI_ERROR_NONE))
      .Times(1)
      .WillOnce(InvokeWithoutArgs([adResolvedPromise]{ adResolvedPromise->set_value(); }));
  std::string adInitUrl = GetFullURI(TEST_AD_MANIFEST_HOST, "manifest/track-video-repid-LE5-tc--header.mp4");
  EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetFile (adInitUrl, _, _, _, _, _, _, _, _, _, _, _, _, _, _))
              .WillOnce(Return(true));
  adInitUrl = GetFullURI(TEST_AD_MANIFEST_HOST, "manifest-eac3/track-audio-repid-DDen-tc--header.mp4");
  EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetFile (adInitUrl, _, _, _, _, _, _, _, _, _, _, _, _, _, _))
              .WillOnce(Return(true));
  mPrivateCDAIObjectMPD->SetAlternateContents(periodId, adId, url, startMS, breakdur);
  ASSERT_EQ(adResolvedFuture.wait_for(std::chrono::seconds(5)), std::future_status::ready);

  // Verify the result
  // mAdBreak updated and placementObj created
  EXPECT_TRUE(mPrivateCDAIObjectMPD->mAdObjThreadID.joinable());
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPlacementObj.pendingAdbrkId, periodId);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdtoInsertInNextBreakVec.size(), 1);
  EXPECT_EQ((mPrivateCDAIObjectMPD->mAdBreaks[periodId].ads)->size(), 1);
  EXPECT_STREQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId].ads->at(0).url.c_str(), TEST_AD_MANIFEST_URL);
  EXPECT_TRUE(mPrivateCDAIObjectMPD->mAdBreaks[periodId].ads->at(0).resolved);
  EXPECT_FALSE(mPrivateCDAIObjectMPD->mAdBreaks[periodId].ads->at(0).invalid);

}

/**
 * @brief Tests the functionality of the SetAlternateContents method when multiple ads are places in an adbreak
 */
TEST_F(AdManagerMPDTests, SetAlternateContentsTests_6)
{
    static const char *manifest =
R"(<?xml version="1.0" encoding="UTF-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" xmlns:scte35="urn:scte:scte35:2014:xml+bin" xmlns:scte214="scte214" xmlns:cenc="urn:mpeg:cenc:2013" xmlns:mspr="mspr" type="static" id="TSS_ICEJ010_010-LIN_c4_HD" profiles="urn:mpeg:dash:profile:isoff-on-demand:2011" minBufferTime="PT0H0M1.000S" maxSegmentDuration="PT0H0M1S" mediaPresentationDuration="PT0H0M10.027S">
  <Period id="1" start="PT0H0M0.000S">
    <AdaptationSet id="1" contentType="video" mimeType="video/mp4" segmentAlignment="true" startWithSAP="1">
      <Role schemeIdUri="urn:mpeg:dash:role:2011" value="main"/>
      <SegmentTemplate initialization="manifest/track-video-repid-$RepresentationID$-tc--header.mp4" media="manifest/track-video-repid-$RepresentationID$-tc--frag-$Number$.mp4" timescale="48000" startNumber="0">
        <SegmentTimeline>
          <S t="0" d="92160" r="3"/>
          <S t="368640" d="111360" r="0"/>
        </SegmentTimeline>
      </SegmentTemplate>
      <Representation id="LE5" bandwidth="5250000" codecs="hvc1.1.6.L123.b0" width="1920" height="1080" frameRate="50">
      </Representation>
    </AdaptationSet>
    <AdaptationSet id="2" contentType="audio" mimeType="audio/mp4" lang="en">
      <AudioChannelConfiguration schemeIdUri="tag:dolby.com,2014:dash:audio_channel_configuration:2011" value="a000"/>
      <Role schemeIdUri="urn:mpeg:dash:role:2011" value="main"/>
      <SegmentTemplate initialization="manifest-eac3/track-audio-repid-$RepresentationID$-tc--header.mp4" media="manifest-eac3/track-audio-repid-$RepresentationID$-tc--frag-$Number$.mp4" timescale="48000" startNumber="0">
        <SegmentTimeline>
          <S t="0" d="92160" r="3"/>
          <S t="368640" d="112128" r="0"/>
        </SegmentTimeline>
      </SegmentTemplate>
      <Representation id="DDen" bandwidth="99450" codecs="ec-3" audioSamplingRate="48000">
      </Representation>
    </AdaptationSet>
  </Period>
</MPD>
)";
  std::string periodId = "testPeriodId";
  std::string adId1 = "testAdId1";
  std::string adId2 = "testAdId2";
  std::string url = "";
  uint64_t startMS = 0;
  uint32_t breakdur = 20000;
  uint32_t adDuration = 10000;
  AAMPCDAIError adErrorCode = eCDAI_ERROR_NONE;

  // To create an empty ad break object
  mPrivateCDAIObjectMPD->SetAlternateContents(periodId, "", "", startMS, breakdur);

  url = TEST_AD_MANIFEST_URL;
  mPrivateInstanceAAMP->SetManifestUrl(TEST_FOG_MAIN_MANIFEST_URL);
  // Set the expect for GetFile twice with same manifest
  InitializeAdMPD(manifest, false, false, 2);

  // mIsFogTSB is true, so downloaded from CDN and redirected to FOG which fails.
  // Here, ad resolved event is sent with true and CDN url is cached
  // Both ads resolve independently; use a counting latch so we wait until both have fired.
  auto adResolvedCount = std::make_shared<std::atomic<int>>(2);
  auto adResolvedPromise = std::make_shared<std::promise<void>>();
  auto adResolvedFuture = adResolvedPromise->get_future();
  auto adResolvedLatch = [adResolvedCount, adResolvedPromise]{
      if (--(*adResolvedCount) == 0) adResolvedPromise->set_value();
  };
  EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendAdResolvedEvent(adId1, true, startMS, 10000, adErrorCode))
      .Times(1).WillOnce(InvokeWithoutArgs(adResolvedLatch));
  EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendAdResolvedEvent(adId2, true, startMS + adDuration, 10000, adErrorCode))
      .Times(1).WillOnce(InvokeWithoutArgs(adResolvedLatch));
  std::string adInitUrl = GetFullURI(TEST_AD_MANIFEST_HOST, "manifest/track-video-repid-LE5-tc--header.mp4");
  EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetFile (adInitUrl, _, _, _, _, _, _, _, _, _, _, _, _, _, _))
              .WillRepeatedly(Return(true));
  adInitUrl = GetFullURI(TEST_AD_MANIFEST_HOST, "manifest-eac3/track-audio-repid-DDen-tc--header.mp4");
  EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetFile (adInitUrl, _, _, _, _, _, _, _, _, _, _, _, _, _, _))
              .WillRepeatedly(Return(true));
  mPrivateCDAIObjectMPD->SetAlternateContents(periodId, adId1, url, startMS, adDuration);
  mPrivateCDAIObjectMPD->SetAlternateContents(periodId, adId2, url, startMS, adDuration);
  ASSERT_EQ(adResolvedFuture.wait_for(std::chrono::seconds(5)), std::future_status::ready);

  // Verify the result
  // mAdBreak updated and placementObj created
  EXPECT_TRUE(mPrivateCDAIObjectMPD->mAdObjThreadID.joinable());
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPlacementObj.pendingAdbrkId, periodId);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdtoInsertInNextBreakVec.size(), 1);
  EXPECT_EQ((mPrivateCDAIObjectMPD->mAdBreaks[periodId].ads)->size(), 2);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId].offset2Ad.size(), 1);
  if (!mPrivateCDAIObjectMPD->mPeriodMap[periodId].offset2Ad.empty())
  {
    EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId].offset2Ad[0].adIdx, 0);
  }
  EXPECT_STREQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId].ads->at(0).url.c_str(), TEST_AD_MANIFEST_URL);
  EXPECT_TRUE(mPrivateCDAIObjectMPD->mAdBreaks[periodId].ads->at(0).resolved);
  EXPECT_FALSE(mPrivateCDAIObjectMPD->mAdBreaks[periodId].ads->at(0).invalid);
  EXPECT_TRUE(mPrivateCDAIObjectMPD->mAdBreaks[periodId].ads->at(1).resolved);
  EXPECT_FALSE(mPrivateCDAIObjectMPD->mAdBreaks[periodId].ads->at(1).invalid);
}

/**
 * @brief Test error scenario for SetAlternateContents when ad break is invalid.
 *
 * This test ensures that if an ad break is marked invalid, SetAlternateContents
 * triggers SendAdResolvedEvent with eCDAI_ERROR_DECISION_TIMEOUT.
 */
TEST_F(AdManagerMPDTests, SetAlternateContentsTests_7)
{
    std::string periodId = "testPeriodId";
    std::string adId = "testAdId";
    uint64_t startMS = 0;
    uint32_t breakdur = 1000;
    AAMPCDAIError adErrorCode = eCDAI_ERROR_DECISIONING_TIMEOUT;

    // Create an ad break object and mark it invalid
    mPrivateCDAIObjectMPD->SetAlternateContents(periodId, "", "", startMS, breakdur);
    mPrivateCDAIObjectMPD->mAdBreaks[periodId].invalid = true;

    // Expect SendAdResolvedEvent to be called with the timeout error
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendAdResolvedEvent(adId, false, 0, 0, adErrorCode)).Times(1);

    // Try to set alternate contents for the invalid ad break
    mPrivateCDAIObjectMPD->SetAlternateContents(periodId, adId, TEST_AD_MANIFEST_URL, startMS, breakdur);

    // Optionally, verify that the ad was not cached
    EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId].ads->size(), 0);
    // Also verify that the ad was not added to the mPlacementObj
    EXPECT_EQ(mPrivateCDAIObjectMPD->mPlacementObj.pendingAdbrkId, "");
    EXPECT_TRUE(mPrivateCDAIObjectMPD->mAdtoInsertInNextBreakVec.empty());
    EXPECT_EQ((mPrivateCDAIObjectMPD->mAdBreaks[periodId].ads)->size(), 0);
}

/**
 * @brief Test error scenario for SetAlternateContents when ad break has no space left.
 *
 * This test ensures that if the ad break is full (adsDuration equals break duration),
 * SetAlternateContents triggers SendAdResolvedEvent with eCDAI_ERROR_INVALID_SPECIFICATION.
 */
TEST_F(AdManagerMPDTests, SetAlternateContentsTests_8)
{
    std::string periodId = "testPeriodId";
    std::string adId = "testAdId";
    uint64_t startMS = 0;
    uint32_t breakdur = 10000;
    AAMPCDAIError errorCode = eCDAI_ERROR_INVALID_SPECIFICATION;

    mPrivateCDAIObjectMPD->SetAlternateContents(periodId, "", "", startMS, breakdur);
    mPrivateCDAIObjectMPD->mAdBreaks[periodId].adsDuration = 10000; // Fill up the ad break

    EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendAdResolvedEvent(adId, false, 0, 0, errorCode)).Times(1);

    mPrivateCDAIObjectMPD->SetAlternateContents(periodId, adId, TEST_AD_MANIFEST_URL, startMS, breakdur);

    // Ad should not be cached
    EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId].ads->size(), 0);
    // Also verify that the ad was not added to the mPlacementObj
    EXPECT_EQ(mPrivateCDAIObjectMPD->mPlacementObj.pendingAdbrkId, "");
    EXPECT_TRUE(mPrivateCDAIObjectMPD->mAdtoInsertInNextBreakVec.empty());
    EXPECT_EQ((mPrivateCDAIObjectMPD->mAdBreaks[periodId].ads)->size(), 0);
}

/**
 * @brief Test error scenario for SetAlternateContents when ad break does not exist.
 *
 * This test ensures that if SetAlternateContents is called for a non-existent ad break,
 * it triggers SendAdResolvedEvent with eCDAI_ERROR_UNKNOWN.
 */
TEST_F(AdManagerMPDTests, SetAlternateContentsTests_9)
{
    std::string periodId = "nonexistentPeriod";
    std::string adId = "testAdId";
    uint64_t startMS = 0;
    uint32_t breakdur = 1000;
    AAMPCDAIError adErrorCode = eCDAI_ERROR_UNKNOWN;

    // Do NOT create the ad break object

    // Expect SendAdResolvedEvent to be called with the delivery error
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendAdResolvedEvent(adId, false, 0, 0, adErrorCode)).Times(1);

    // Try to set alternate contents for a non-existent ad break
    mPrivateCDAIObjectMPD->SetAlternateContents(periodId, adId, TEST_AD_MANIFEST_URL, startMS, breakdur);

    // Optionally, verify that the ad break was not created
    EXPECT_FALSE(mPrivateCDAIObjectMPD->isAdBreakObjectExist(periodId));
    // Also verify that the ad was not added to the mPlacementObj
    EXPECT_EQ(mPrivateCDAIObjectMPD->mPlacementObj.pendingAdbrkId, "");
    EXPECT_TRUE(mPrivateCDAIObjectMPD->mAdtoInsertInNextBreakVec.empty());
}

/**
 * @brief Test error scenario for FulFillAdObject with invalid ad manifest.
 *
 * This test ensures that when FulFillAdObject is called and the ad manifest is invalid,
 * SendAdResolvedEvent is triggered with eCDAI_ERROR_INVALID_MANIFEST.
 */
TEST_F(AdManagerMPDTests, SetAlternateContentsTests_10)
{
    std::string periodId = "testPeriodId";
    std::string adId = "testAdId";
    std::string url = TEST_AD_MANIFEST_URL;
    uint64_t startMS = 0;
    uint32_t breakdur = 10000;
    AAMPCDAIError expectedError = eCDAI_ERROR_INVALID_MANIFEST;
    const char* invalidManifest = "<MPD><Invalid></MPD>";

    mPrivateCDAIObjectMPD->SetAlternateContents(periodId, "", "", startMS, breakdur);

    InitializeAdMPD(invalidManifest);

    auto adResolvedPromise = std::make_shared<std::promise<void>>();
    auto adResolvedFuture = adResolvedPromise->get_future();
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendAdResolvedEvent(adId, false, 0, 0, expectedError))
        .Times(1)
        .WillOnce(InvokeWithoutArgs([adResolvedPromise]{ adResolvedPromise->set_value(); }));
    // Now set the ad
    mPrivateCDAIObjectMPD->SetAlternateContents(periodId, adId, url, startMS, breakdur);
    ASSERT_EQ(adResolvedFuture.wait_for(std::chrono::seconds(5)), std::future_status::ready);

    // Verify the result
    // mAdBreak updated and placementObj not created
    EXPECT_TRUE(mPrivateCDAIObjectMPD->mAdObjThreadID.joinable());
    EXPECT_EQ(mPrivateCDAIObjectMPD->mPlacementObj.pendingAdbrkId, "");
    EXPECT_TRUE(mPrivateCDAIObjectMPD->mAdtoInsertInNextBreakVec.empty());
    EXPECT_EQ((mPrivateCDAIObjectMPD->mAdBreaks[periodId].ads)->size(), 1);
    EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId].ads->at(0).adId, adId);
    EXPECT_TRUE(mPrivateCDAIObjectMPD->mAdBreaks[periodId].ads->at(0).resolved);
    EXPECT_TRUE(mPrivateCDAIObjectMPD->mAdBreaks[periodId].ads->at(0).invalid);
}

/**
 * @brief Test error scenario for FulFillAdObject when manifest fetch fails and http_error < 100.
 *
 * This test ensures that if GetFile fails and http_error is less than 100,
 * FulFillAdObject triggers SendAdResolvedEvent with eCDAI_ERROR_DELIVERY_ERROR.
 */
TEST_F(AdManagerMPDTests, SetAlternateContentsTests_11)
{
    std::string periodId = "testPeriodId";
    std::string adId = "testAdId";
    std::string url = TEST_AD_MANIFEST_URL;
    uint64_t startMS = 0;
    uint32_t breakdur = 10000;
    AAMPCDAIError expectedError = eCDAI_ERROR_DELIVERY_ERROR;

    mPrivateCDAIObjectMPD->SetAlternateContents(periodId, "", "", startMS, breakdur);
    InitializeAdMPD(nullptr, false, false, 1, 28); // No manifest

    auto adResolvedPromise = std::make_shared<std::promise<void>>();
    auto adResolvedFuture = adResolvedPromise->get_future();
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendAdResolvedEvent(adId, false, 0, 0, expectedError))
        .Times(1)
        .WillOnce(InvokeWithoutArgs([adResolvedPromise]{ adResolvedPromise->set_value(); }));
    // Now set the ad
    mPrivateCDAIObjectMPD->SetAlternateContents(periodId, adId, url, startMS, breakdur);
    ASSERT_EQ(adResolvedFuture.wait_for(std::chrono::seconds(5)), std::future_status::ready);

    // Verify the result
    // mAdBreak updated and placementObj not created
    EXPECT_TRUE(mPrivateCDAIObjectMPD->mAdObjThreadID.joinable());
    EXPECT_EQ(mPrivateCDAIObjectMPD->mPlacementObj.pendingAdbrkId, "");
    EXPECT_TRUE(mPrivateCDAIObjectMPD->mAdtoInsertInNextBreakVec.empty());
    EXPECT_EQ((mPrivateCDAIObjectMPD->mAdBreaks[periodId].ads)->size(), 1);
    EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId].ads->at(0).adId, adId);
    EXPECT_TRUE(mPrivateCDAIObjectMPD->mAdBreaks[periodId].ads->at(0).resolved);
    EXPECT_TRUE(mPrivateCDAIObjectMPD->mAdBreaks[periodId].ads->at(0).invalid);
}

/**
 * @brief Test error scenario for FulFillAdObject when manifest fetch fails and http_error >= 100.
 *
 * This test ensures that if GetFile fails and http_error is greater than or equal to 100,
 * FulFillAdObject triggers SendAdResolvedEvent with eCDAI_ERROR_DELIVERY_HTTP_ERROR.
 */
TEST_F(AdManagerMPDTests, SetAlternateContentsTests_12)
{
    std::string periodId = "testPeriodId";
    std::string adId = "testAdId";
    std::string url = TEST_AD_MANIFEST_URL;
    uint64_t startMS = 0;
    uint32_t breakdur = 10000;
    AAMPCDAIError expectedError = eCDAI_ERROR_DELIVERY_HTTP_ERROR;

    mPrivateCDAIObjectMPD->SetAlternateContents(periodId, "", "", startMS, breakdur);
    InitializeAdMPD(nullptr, false, false, 1, 404); // No manifest

    auto adResolvedPromise = std::make_shared<std::promise<void>>();
    auto adResolvedFuture = adResolvedPromise->get_future();
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendAdResolvedEvent(adId, false, 0, 0, expectedError))
        .Times(1)
        .WillOnce(InvokeWithoutArgs([adResolvedPromise]{ adResolvedPromise->set_value(); }));
    // Now set the ad
    mPrivateCDAIObjectMPD->SetAlternateContents(periodId, adId, url, startMS, breakdur);
    ASSERT_EQ(adResolvedFuture.wait_for(std::chrono::seconds(5)), std::future_status::ready);

    // Verify the result
    // mAdBreak updated and placementObj not created
    EXPECT_TRUE(mPrivateCDAIObjectMPD->mAdObjThreadID.joinable());
    EXPECT_EQ(mPrivateCDAIObjectMPD->mPlacementObj.pendingAdbrkId, "");
    EXPECT_TRUE(mPrivateCDAIObjectMPD->mAdtoInsertInNextBreakVec.empty());
    EXPECT_EQ((mPrivateCDAIObjectMPD->mAdBreaks[periodId].ads)->size(), 1);
    EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId].ads->at(0).adId, adId);
    EXPECT_TRUE(mPrivateCDAIObjectMPD->mAdBreaks[periodId].ads->at(0).resolved);
    EXPECT_TRUE(mPrivateCDAIObjectMPD->mAdBreaks[periodId].ads->at(0).invalid);
}

/**
 * @brief Test error scenario for FulFillAdObject when AdNode is marked invalid.
 *
 * This test ensures that if an AdNode in the ad break is marked invalid,
 * FulFillAdObject triggers SendAdResolvedEvent with eCDAI_ERROR_DELIVERY_TIMEOUT.
 */
TEST_F(AdManagerMPDTests, SetAlternateContentsTests_13)
{
    // Arrange
    std::string periodId = "testPeriodId";
    std::string adId = "testAdId";
    std::string url = TEST_AD_MANIFEST_URL;
    uint64_t startMS = 0;
    uint32_t breakdur = 10000;
    AAMPCDAIError expectedError = eCDAI_ERROR_DELIVERY_TIMEOUT;
    const char *manifest =
         "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
         "<MPD><Period id=\"1\"><AdaptationSet contentType=\"video\"><Representation><SegmentTemplate media=\"video.mp4\" initialization=\"video_init.mp4\"/></Representation></AdaptationSet>"
         "<AdaptationSet contentType=\"audio\"><Representation><SegmentTemplate media=\"audio.mp4\" initialization=\"audio_init.mp4\"/></Representation></AdaptationSet></Period></MPD>";
    mPrivateCDAIObjectMPD->SetAlternateContents(periodId, "", "", startMS, breakdur);

    // Set up GetFile expectations for video and audio init segments
    std::string videoInitUrl = TEST_AD_MANIFEST_HOST + std::string("video_init.mp4");
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetFile(videoInitUrl, _, _, _, _, _, _, _, _, _, _, _, _, _, _))
      .WillOnce(Return(true));
    std::string audioInitUrl = TEST_AD_MANIFEST_HOST + std::string("audio_init.mp4");
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetFile(audioInitUrl, _, _, _, _, _, _, _, _, _, _, _, _, _, _))
      .WillOnce(Return(true));
    // Set up the mock for GetFile before any SetAlternateContents calls
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetFile(url, _, _, _, _, _, _, _, _, _, _, _, _, _, _))
      .WillOnce(WithArgs<0,2,3,4>(Invoke([this, periodId, manifest](std::string remoteUrl, std::vector<uint8_t> &buffer, std::string& effectiveUrl, int& httpError)
        {
            buffer.clear();
            buffer.assign(manifest, manifest + strlen(manifest));
            httpError = 200;
            effectiveUrl = remoteUrl;
            if (!this->mPrivateCDAIObjectMPD->mAdBreaks[periodId].ads->empty())
            {
              this->mPrivateCDAIObjectMPD->mAdBreaks[periodId].ads->at(0).invalid = true;
            }
            return true;
        })));

    auto adResolvedPromise = std::make_shared<std::promise<void>>();
    auto adResolvedFuture = adResolvedPromise->get_future();
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendAdResolvedEvent(adId, false, 0, 0, expectedError))
        .Times(1)
        .WillOnce(InvokeWithoutArgs([adResolvedPromise]{ adResolvedPromise->set_value(); }));

    // Create initial ad break

    // Try to add the ad that should fail with timeout
    mPrivateCDAIObjectMPD->SetAlternateContents(periodId, adId, url, startMS, breakdur);
    ASSERT_EQ(adResolvedFuture.wait_for(std::chrono::seconds(5)), std::future_status::ready);

    // Verify the result
    // mAdBreak updated and placementObj not created
    EXPECT_TRUE(mPrivateCDAIObjectMPD->mAdObjThreadID.joinable());
    EXPECT_EQ(mPrivateCDAIObjectMPD->mPlacementObj.pendingAdbrkId, "");
    EXPECT_TRUE(mPrivateCDAIObjectMPD->mAdtoInsertInNextBreakVec.empty());
    EXPECT_EQ((mPrivateCDAIObjectMPD->mAdBreaks[periodId].ads)->size(), 1);
    EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId].ads->at(0).adId, adId);
    EXPECT_TRUE(mPrivateCDAIObjectMPD->mAdBreaks[periodId].ads->at(0).resolved);
    EXPECT_TRUE(mPrivateCDAIObjectMPD->mAdBreaks[periodId].ads->at(0).invalid);
}

/**
 * @brief Test error scenario for FulFillAdObject when manifest contains multiple periods.
 *
 * This test ensures that if the ad manifest contains more than one period,
 * FulFillAdObject triggers SendAdResolvedEvent with eCDAI_ERROR_INVALID_MEDIA.
 */
TEST_F(AdManagerMPDTests, SetAlternateContentsTests_14)
{
    // Arrange
    std::string periodId = "testPeriodId";
    std::string adId = "testAdId";
    std::string url = TEST_AD_MANIFEST_URL;
    uint64_t startMS = 0;
    uint32_t breakdur = 10000;
    AAMPCDAIError expectedError = eCDAI_ERROR_INVALID_MEDIA;

    // Create an ad break object and add the test ad
    mPrivateCDAIObjectMPD->SetAlternateContents(periodId, "", "", startMS, breakdur);

    // Prepare manifest with multiple periods - should trigger error
    const char *manifest =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<MPD type=\"static\" xmlns=\"urn:mpeg:dash:schema:mpd:2011\" minBufferTime=\"PT1.5S\">"
        "<Period id=\"1\"><AdaptationSet contentType=\"video\"></AdaptationSet>"
        "<AdaptationSet contentType=\"audio\"></AdaptationSet></Period>"
        "<Period id=\"2\"><AdaptationSet contentType=\"video\"></AdaptationSet>"
        "<AdaptationSet contentType=\"audio\"></AdaptationSet></Period>"
        "</MPD>";

    InitializeAdMPD(manifest);

    // Expect error event due to multiple periods
    auto adResolvedPromise = std::make_shared<std::promise<void>>();
    auto adResolvedFuture = adResolvedPromise->get_future();
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendAdResolvedEvent(adId, false, 0, 0, expectedError))
        .Times(1)
        .WillOnce(InvokeWithoutArgs([adResolvedPromise]{ adResolvedPromise->set_value(); }));

    // Act - Set alternate contents with the ad
    mPrivateCDAIObjectMPD->SetAlternateContents(periodId, adId, url, startMS, breakdur);
    ASSERT_EQ(adResolvedFuture.wait_for(std::chrono::seconds(5)), std::future_status::ready);

    // Assert
    // mAdBreak updated and placementObj not created
    EXPECT_TRUE(mPrivateCDAIObjectMPD->mAdObjThreadID.joinable());
    EXPECT_EQ(mPrivateCDAIObjectMPD->mPlacementObj.pendingAdbrkId, "");
    EXPECT_TRUE(mPrivateCDAIObjectMPD->mAdtoInsertInNextBreakVec.empty());
    EXPECT_EQ((mPrivateCDAIObjectMPD->mAdBreaks[periodId].ads)->size(), 1);
    EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId].ads->at(0).adId, adId);
    EXPECT_TRUE(mPrivateCDAIObjectMPD->mAdBreaks[periodId].ads->at(0).resolved);
    EXPECT_TRUE(mPrivateCDAIObjectMPD->mAdBreaks[periodId].ads->at(0).invalid);
}

/**
 * @brief Tests the functionality of the CheckForAdStart method
 * 1. When periodId is not in mPeriodMap.
 * 2. When periodId is in mPeriodMap and adBreakId is empty.
 */
TEST_F(AdManagerMPDTests, CheckForAdStartTests_1)
{
    float rate = 1.0;
    bool init = false;
    std::string periodId = "testPeriodId";
    double offSet = 0.0;
    std::string breakId;
    double adOffset;

    int adIdx = mPrivateCDAIObjectMPD->CheckForAdStart(rate, init, periodId, offSet, breakId, adOffset);

    // Verify the result
    EXPECT_EQ(-1, adIdx);

    // Add periodId to mPeriodMap with empty adBreakId
    mPrivateCDAIObjectMPD->mPeriodMap[periodId] = Period2AdData();

    adIdx = mPrivateCDAIObjectMPD->CheckForAdStart(rate, init, periodId, offSet, breakId, adOffset);
    // Verify the result
    EXPECT_EQ(-1, adIdx);
}

/**
 * @brief Tests the functionality of the CheckForAdStart method when periodId is in mPeriodMap and adBreakId is not empty.
 * 1. Check for empty ads in the adBreak.
 * 2. Check for seamless and ads in the adBreak
 * 3. Check for discrete and ads in the adBreak. Here, playback starts from second ad
 */
TEST_F(AdManagerMPDTests, CheckForAdStartTests_2)
{
    float rate = 1.0;
    bool init = false;
    std::string periodId = "testPeriodId";
    double offSet = 0.0;
    std::string breakId = "";
    double adOffset = -1;

    // Add periodId to mPeriodMap with non-empty adBreakId
    mPrivateCDAIObjectMPD->mPeriodMap[periodId] = Period2AdData();
    mPrivateCDAIObjectMPD->mPeriodMap[periodId].adBreakId = "testPeriodId";
    mPrivateCDAIObjectMPD->mAdBreaks = {
        {"testPeriodId", AdBreakObject()}
    };

    int result = mPrivateCDAIObjectMPD->CheckForAdStart(rate, init, periodId, offSet, breakId, adOffset);

    // Verify the result
    // There are no ads in the adBreak, so the result should be -1
    EXPECT_EQ(-1, result);

    // Add ads to mPeriodMap
    mPrivateCDAIObjectMPD->mPeriodMap[periodId] = Period2AdData(false, "testPeriodId", 60000 /*in ms*/,
        {
          std::make_pair (0, AdOnPeriod(0, 0)), // for adId1 idx=0, offset=0s
          std::make_pair (30000, AdOnPeriod(1, 0)) // for adId2 idx=1, offset=0s
        });
    // Add ads to the adBreak
    mPrivateCDAIObjectMPD->mAdBreaks["testPeriodId"].ads = std::make_shared<std::vector<AdNode>>();
    mPrivateCDAIObjectMPD->mAdBreaks["testPeriodId"].ads->emplace_back(false, false, true, "adId1", "url", 30000, "testPeriodId", 0, nullptr);
    mPrivateCDAIObjectMPD->mAdBreaks["testPeriodId"].ads->emplace_back(false, false, true, "adId2", "url", 30000, "testPeriodId", 30000, nullptr);

    // reset
    breakId = "";
    adOffset = -1;
    // seamless playback
    result = mPrivateCDAIObjectMPD->CheckForAdStart(rate, init, periodId, offSet, breakId, adOffset);
    // Verify the result, we should get adIdx 0
    EXPECT_EQ(0, result);
    EXPECT_EQ("testPeriodId", breakId);
    EXPECT_EQ(0, adOffset);

    // reset
    breakId = "";
    adOffset = -1;
    // discrete playback and playback start from second ad
    init = true;
    offSet = 35;
    result = mPrivateCDAIObjectMPD->CheckForAdStart(rate, init, periodId, offSet, breakId, adOffset);
    // Verify the result, we should get adIdx 1
    EXPECT_EQ(1, result);
    EXPECT_EQ("testPeriodId", breakId);
    // 35 - 30
    EXPECT_EQ(5, adOffset);


    // reset
    breakId = "";
    adOffset = -1;
    mPrivateCDAIObjectMPD->mAdBreaks["testPeriodId"].invalid = true;
    // Make the adBreak invalid
    result = mPrivateCDAIObjectMPD->CheckForAdStart(rate, init, periodId, offSet, breakId, adOffset);

    // Verify the result
    // There are ads in the adBreak, but entire adBreadk is marked as invalid, so the result should be -1
    EXPECT_EQ(-1, result);
    EXPECT_EQ("", breakId);
}

/**
 * @brief Tests the functionality of the PlaceAds method when MPD is empty and PlacementObj is not populated.
 */
TEST_F(AdManagerMPDTests, PlaceAdsTests_1)
{
  mPrivateCDAIObjectMPD->PlaceAds(nullptr);
  static const char *manifest =
R"(<?xml version="1.0" encoding="UTF-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" xmlns:scte35="urn:scte:scte35:2014:xml+bin" xmlns:scte214="scte214" xmlns:cenc="urn:mpeg:cenc:2013" xmlns:mspr="mspr" type="static" id="TSS_ICEJ010_010-LIN_c4_HD" profiles="urn:mpeg:dash:profile:isoff-on-demand:2011" minBufferTime="PT0H0M1.000S" maxSegmentDuration="PT0H0M1S" mediaPresentationDuration="PT0H0M10.027S">
  <Period id="1" start="PT0H0M0.000S">
    <AdaptationSet id="1" contentType="video" mimeType="video/mp4" segmentAlignment="true" startWithSAP="1">
      <Role schemeIdUri="urn:mpeg:dash:role:2011" value="main"/>
      <SegmentTemplate initialization="manifest/track-video-repid-$RepresentationID$-tc--header.mp4" media="manifest/track-video-repid-$RepresentationID$-tc--frag-$Number$.mp4" timescale="48000" startNumber="0">
        <SegmentTimeline>
          <S t="0" d="92160" r="3"/>
          <S t="368640" d="111360" r="0"/>
        </SegmentTimeline>
      </SegmentTemplate>
      <Representation id="LE5" bandwidth="5250000" codecs="hvc1.1.6.L123.b0" width="1920" height="1080" frameRate="50">
      </Representation>
    </AdaptationSet>
    <AdaptationSet id="2" contentType="audio" mimeType="audio/mp4" lang="en">
      <AudioChannelConfiguration schemeIdUri="tag:dolby.com,2014:dash:audio_channel_configuration:2011" value="a000"/>
      <Role schemeIdUri="urn:mpeg:dash:role:2011" value="main"/>
      <SegmentTemplate initialization="manifest-eac3/track-audio-repid-$RepresentationID$-tc--header.mp4" media="manifest-eac3/track-audio-repid-$RepresentationID$-tc--frag-$Number$.mp4" timescale="48000" startNumber="0">
        <SegmentTimeline>
          <S t="0" d="92160" r="3"/>
          <S t="368640" d="112128" r="0"/>
        </SegmentTimeline>
      </SegmentTemplate>
      <Representation id="DDen" bandwidth="99450" codecs="ec-3" audioSamplingRate="48000">
      </Representation>
    </AdaptationSet>
  </Period>
</MPD>
)";
  ProcessSourceMPD(manifest);
  mPrivateCDAIObjectMPD->PlaceAds(mAdMPDParseHelper);
}


/**
 * @brief Tests the functionality of the PlaceAds method when openPeriodID is valid
 * 1. Also, verifies the same mpd passed to PlaceAds() again
 */
TEST_F(AdManagerMPDTests, PlaceAdsTests_2)
{
  // not adding scte35 markers. These are mocked in PrivateObjectMPD instance
  static const char *manifest =
R"(<?xml version="1.0" encoding="utf-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" availabilityStartTime="2023-01-01T00:00:00Z" maxSegmentDuration="PT2S" minBufferTime="PT4.000S" minimumUpdatePeriod="P100Y" profiles="urn:dvb:dash:profile:dvb-dash:2014,urn:dvb:dash:profile:dvb-dash:isoff-ext-live:2014" publishTime="2023-01-01T00:01:00Z" timeShiftBufferDepth="PT5M" type="dynamic">
  <Period id="testPeriodId0" start="PT0S">
    <AdaptationSet id="0" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p0_init.mp4" media="video_p0_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="0" d="5000" r="14" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
  <Period id="testPeriodId1" start="PT30S">
    <AdaptationSet id="1" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p1_init.mp4" media="video_p1_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="0" d="5000" r="2" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
</MPD>
)";
  std::string periodId = "testPeriodId1";
  // testPeriodId1 has 3 fragments added in the mock
  ProcessSourceMPD(manifest);
  // Set curEndNumber to 0
  mPrivateCDAIObjectMPD->mPlacementObj = PlacementObj(periodId, periodId, 0, 0, 0, 0, false);

  // Add ads to the adBreak
  mPrivateCDAIObjectMPD->mAdBreaks = {
    {periodId, AdBreakObject(30000, std::make_shared<std::vector<AdNode>>(), "", 0, 30000)}
  };
  mPrivateCDAIObjectMPD->mAdBreaks[periodId].ads->emplace_back(false, false, true, "adId1", "url", 30000, periodId, 0, nullptr);

  // Add ads to mPeriodMap. mPeriodMap[periodId].adBreakId is non-empty for live at the beginning as per SetAlternateContents
  mPrivateCDAIObjectMPD->mPeriodMap[periodId] = Period2AdData(false, periodId, 0 /*in ms*/,
    {
      std::make_pair (0, AdOnPeriod(0, 0)), // for adId1 idx=0, offset=0s
    });
  mPrivateCDAIObjectMPD->PlaceAds(mAdMPDParseHelper);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPlacementObj.curEndNumber, 3);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId].duration, 6000); // in ms

  // Update with same mpd again, and the params should not change
  mPrivateCDAIObjectMPD->PlaceAds(mAdMPDParseHelper);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPlacementObj.curEndNumber, 3);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId].duration, 6000); // in ms
}

/**
 * @brief Tests the functionality of the PlaceAds method when openPeriodID is finished and new period is added
 * 1. Verifies that the openPeriod is closed and ads are placed
 */
TEST_F(AdManagerMPDTests, PlaceAdsTests_3)
{
  // not adding scte35 markers. These are mocked in PrivateObjectMPD instance
  // testPeriodId1 has 2 new fragments added
  // testPeriodId2 has 2 new fragments added but without any ad mapping
  static const char *manifest =
R"(<?xml version="1.0" encoding="utf-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" availabilityStartTime="2023-01-01T00:00:00Z" maxSegmentDuration="PT2S" minBufferTime="PT4.000S" minimumUpdatePeriod="P100Y" profiles="urn:dvb:dash:profile:dvb-dash:2014,urn:dvb:dash:profile:dvb-dash:isoff-ext-live:2014" publishTime="2023-01-01T00:01:00Z" timeShiftBufferDepth="PT5M" type="dynamic">
  <Period id="testPeriodId0" start="PT0S">
    <AdaptationSet id="0" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p0_init.mp4" media="video_p0_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="0" d="5000" r="14" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
  <Period id="testPeriodId1" start="PT30S">
    <AdaptationSet id="1" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p1_init.mp4" media="video_p1_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="0" d="5000" r="14" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
  <Period id="testPeriodId2" start="PT60S">
    <AdaptationSet id="1" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p1_init.mp4" media="video_p1_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="0" d="5000" r="1" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
</MPD>
)";
  std::string periodId = "testPeriodId1";
  ProcessSourceMPD(manifest);
  // Set curEndNumber to 13, adNextOffset = (13)*2000
  mPrivateCDAIObjectMPD->mPlacementObj = PlacementObj(periodId, periodId, 13, 0, 26000, 0, false);

  // Add ads to the adBreak
  mPrivateCDAIObjectMPD->mAdBreaks = {
    {periodId, AdBreakObject(30000, std::make_shared<std::vector<AdNode>>(), "", 0, 30000)}
  };
  mPrivateCDAIObjectMPD->mAdBreaks[periodId].ads->emplace_back(false, false, true, "adId1", "url", 30000, periodId, 0, nullptr);

  // Add ads to mPeriodMap. mPeriodMap[periodId].adBreakId is non-empty for live at the beginning as per SetAlternateContents
  mPrivateCDAIObjectMPD->mPeriodMap[periodId] = Period2AdData(false, periodId, 26000 /*in ms*/,
    {
      std::make_pair (0, AdOnPeriod(0, 0)), // for adId1 idx=0, offset=0s
    });
  mPrivateCDAIObjectMPD->PlaceAds(mAdMPDParseHelper);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId].ads->at(0).placed, true);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId].endPeriodOffset, 0);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId].endPeriodId, "testPeriodId2"); // next period
  EXPECT_TRUE(mPrivateCDAIObjectMPD->mAdBreaks[periodId].mAdBreakPlaced); // adBreak placed
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPlacementObj.curAdIdx, -1); // since no new placementObj exists
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPlacementObj.curEndNumber, 0);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId].duration, 30000); // in ms
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId].adBreakId, "testPeriodId1");
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId].offset2Ad[0].adIdx, 0);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId].offset2Ad[0].adStartOffset, 0);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId].ads->at(0).basePeriodId, "testPeriodId1"); //baseperiod of adId1
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId].ads->at(0).basePeriodOffset, 0);
}

/**
 * @brief PlaceAds cancels ad with early resumption and clears pendingAdCancel when next period is added before openPeriod is finished. This test verifies that scenario.
 */
TEST_F(AdManagerMPDTests, PlaceAds_EarlyResumptionTests)
{
  // Step 1: Initial manifest with only p1
  static const char *manifest1 =
R"(<?xml version="1.0" encoding="utf-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" type="dynamic">
  <Period id="p1" start="PT0S">
    <AdaptationSet id="0" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p0_init.mp4" media="video_p0_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="0" d="5000" r="5" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
</MPD>
)";
  // Step 2: Updated manifest with p1 and p2
  static const char *manifest2 =
R"(<?xml version="1.0" encoding="utf-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" type="dynamic">
  <Period id="p1" start="PT0S">
    <AdaptationSet id="0" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p0_init.mp4" media="video_p0_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="0" d="5000" r="5" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
  <Period id="p2" start="PT30S">
    <AdaptationSet id="1" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p1_init.mp4" media="video_p1_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="0" d="5000" r="5" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
</MPD>
)";
  std::string periodId = "p1";
  std::string nextPeriodId = "p2";

  // Initial setup: only p1
  ProcessSourceMPD(manifest1);
  mPrivateCDAIObjectMPD->mPlacementObj = PlacementObj(periodId, periodId, 5, 0, 25000, 0, false);
  mPrivateCDAIObjectMPD->mAdBreaks = {
    {periodId, AdBreakObject(30000, std::make_shared<std::vector<AdNode>>(), "", 0, 30000)}
  };
  mPrivateCDAIObjectMPD->mAdBreaks[periodId].ads->emplace_back(false, false, true, "adId1", "url", 30000, periodId, 0, nullptr);
  mPrivateCDAIObjectMPD->mAdBreaks[periodId].cancelAtPeriodId = nextPeriodId;
  mPrivateCDAIObjectMPD->mPeriodMap[periodId] = Period2AdData(false, periodId, 25000,
    { std::make_pair(0, AdOnPeriod(0, 0)) });

  // First PlaceAds call: only p1 present
  mPrivateCDAIObjectMPD->PlaceAds(mAdMPDParseHelper);

  // Now update manifest to add p2 (next period)
  ProcessSourceMPD(manifest2);

  // Second PlaceAds call: p2 now present, triggers early resumption logic and should also clear pendingAdCancel
  mPrivateCDAIObjectMPD->PlaceAds(mAdMPDParseHelper);

  // Validate: pendingAdCancel is now false, ad cancelled, adNextOffset = 0
  EXPECT_FALSE(mPrivateCDAIObjectMPD->mPlacementObj.pendingAdCancel);
  EXPECT_TRUE(mPrivateCDAIObjectMPD->mAdBreaks[periodId].ads->at(0).cancelled);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPlacementObj.adNextOffset, 0);
	EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId].endPeriodId, periodId);
	EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId].endPeriodOffset, 27000);
}

/**
 * @brief Tests the functionality of the PlaceAds method when openPeriodID is finished and new period is added
 * 1. Verifies that the openPeriod is closed and ads are placed
 * 2. TODO: [VKB] Also verifies that newPeriod is not updated (shouldn't we fix it?)
 */
TEST_F(AdManagerMPDTests, PlaceAdsTests_4)
{
  // not adding scte35 markers. These are mocked in PrivateObjectMPD instance
  // testPeriodId1 has 2 new fragments added
  // testPeriodId2 has 2 new fragments added
  static const char *manifest =
R"(<?xml version="1.0" encoding="utf-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" availabilityStartTime="2023-01-01T00:00:00Z" maxSegmentDuration="PT2S" minBufferTime="PT4.000S" minimumUpdatePeriod="P100Y" profiles="urn:dvb:dash:profile:dvb-dash:2014,urn:dvb:dash:profile:dvb-dash:isoff-ext-live:2014" publishTime="2023-01-01T00:01:00Z" timeShiftBufferDepth="PT5M" type="dynamic">
  <Period id="testPeriodId0" start="PT0S">
    <AdaptationSet id="0" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p0_init.mp4" media="video_p0_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="0" d="5000" r="14" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
  <Period id="testPeriodId1" start="PT30S">
    <AdaptationSet id="1" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p1_init.mp4" media="video_p1_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="0" d="5000" r="14" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
  <Period id="testPeriodId2" start="PT60S">
    <AdaptationSet id="1" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p1_init.mp4" media="video_p1_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="0" d="5000" r="1" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
</MPD>
)";
  std::string periodId1 = "testPeriodId1";
  std::string periodId2 = "testPeriodId2";
  ProcessSourceMPD(manifest);
  // Set curEndNumber to 13, adNextOffset = (13)*2000
  mPrivateCDAIObjectMPD->mPlacementObj = PlacementObj(periodId1, periodId1, 13, 0, 26000, 0, false);
  mPrivateCDAIObjectMPD->mAdtoInsertInNextBreakVec.emplace_back(periodId2, periodId2, 0, 0, 0, 0, false); // second ad break in vector

  // Add ads to the adBreak
  mPrivateCDAIObjectMPD->mAdBreaks = {
    {periodId1, AdBreakObject(30000, std::make_shared<std::vector<AdNode>>(), "", 0, 30000)},
    {periodId2, AdBreakObject(30000, std::make_shared<std::vector<AdNode>>(), "", 0, 30000)}
  };
  mPrivateCDAIObjectMPD->mAdBreaks[periodId1].ads->emplace_back(false, false, true, "adId1", "url1", 30000, periodId1, 0, nullptr);
  mPrivateCDAIObjectMPD->mAdBreaks[periodId2].ads->emplace_back(false, false, true, "adId2", "url2", 30000, periodId2, 0, nullptr);

  // Add ads to mPeriodMap. mPeriodMap[periodId].adBreakId is non-empty for live at the beginning as per SetAlternateContents
  mPrivateCDAIObjectMPD->mPeriodMap[periodId1] = Period2AdData(false, periodId1, 26000 /*in ms*/,
    {
      std::make_pair (0, AdOnPeriod(0, 0)), // for adId1 idx=0, offset=0s
    });
  mPrivateCDAIObjectMPD->mPeriodMap[periodId2] = Period2AdData(false, periodId2, 0 /*in ms*/,
    {
      std::make_pair (0, AdOnPeriod(0, 0)), // for adId1 idx=0, offset=0s
    });
  mPrivateCDAIObjectMPD->PlaceAds(mAdMPDParseHelper);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].ads->at(0).placed, true);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].endPeriodOffset, 0);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].endPeriodId, "testPeriodId2"); // next period
  EXPECT_TRUE(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].mAdBreakPlaced); // adBreak placed
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].ads->at(0).basePeriodId, "testPeriodId1");
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].ads->at(0).basePeriodOffset, 0);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId1].adBreakId, "testPeriodId1");
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId1].offset2Ad[0].adIdx, 0);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId1].offset2Ad[0].adStartOffset, 0);
  // Make sure endPeriodId is not reset in mPeriodMap.
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId2].adBreakId, periodId2);

  EXPECT_EQ(mPrivateCDAIObjectMPD->mPlacementObj.curAdIdx, 0);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPlacementObj.pendingAdbrkId, periodId2);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId1].duration, 30000); // in ms
  // periodId2 is not updated in the current logic
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId2].duration, 0); // in ms
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId2].adBreakId, "testPeriodId2");
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId2].offset2Ad[0].adIdx, 0);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId2].offset2Ad[0].adStartOffset, 0);
}

/**
 * @brief Tests the functionality of the PlaceAds method when sourceAdDurationMismatch is true
 * If the duration of ad outside source period is greater than 2sec, its treated as split period
 * This test validates the scenario when duration of ad outside source period is equal to 2sec
 */
TEST_F(AdManagerMPDTests, PlaceAdsTests_5)
{
  // not adding scte35 markers. These are mocked in PrivateObjectMPD instance
  // testPeriodId1 has 0 new fragments added
  // testPeriodId2 has 2 new fragments added
  static const char *manifest =
R"(<?xml version="1.0" encoding="utf-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" availabilityStartTime="2023-01-01T00:00:00Z" maxSegmentDuration="PT2S" minBufferTime="PT4.000S" minimumUpdatePeriod="P100Y" profiles="urn:dvb:dash:profile:dvb-dash:2014,urn:dvb:dash:profile:dvb-dash:isoff-ext-live:2014" publishTime="2023-01-01T00:01:00Z" timeShiftBufferDepth="PT5M" type="dynamic">
  <Period id="testPeriodId0" start="PT0S">
    <AdaptationSet id="0" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p0_init.mp4" media="video_p0_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="0" d="5000" r="14" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
  <Period id="testPeriodId1" start="PT30S">
    <AdaptationSet id="1" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p1_init.mp4" media="video_p1_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="0" d="5000" r="14" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
  <Period id="testPeriodId2" start="PT60S">
    <AdaptationSet id="1" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p1_init.mp4" media="video_p1_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="0" d="5000" r="1" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
</MPD>
)";
  std::string periodId1 = "testPeriodId1";
  std::string periodId2 = "testPeriodId2";
  ProcessSourceMPD(manifest);
  // Set curEndNumber to 15, adNextOffset = (15)*2000
  mPrivateCDAIObjectMPD->mPlacementObj = PlacementObj(periodId1, periodId1, 15, 0, 30000, 0, false);
  mPrivateCDAIObjectMPD->mAdtoInsertInNextBreakVec.emplace_back(periodId2, periodId2, 0, 0, 0, 0, false); // second ad break in vector

  // Add ads to the adBreak
  // testPeriodId1 ad duration is set to 32000 to force mismatch for sourceAdDurationMismatch
  // Also setting brkDuration to 32 sec as well
  mPrivateCDAIObjectMPD->mAdBreaks = {
    {periodId1, AdBreakObject(32000, std::make_shared<std::vector<AdNode>>(), "", 0, 32000)},
    {periodId2, AdBreakObject(30000, std::make_shared<std::vector<AdNode>>(), "", 0, 30000)}
  };
  // 1 - to - 1 mapping of ad and period
  mPrivateCDAIObjectMPD->mAdBreaks[periodId1].ads->emplace_back(false, false, true, "adId1", "url1", 32000, periodId1, 0, nullptr);
  mPrivateCDAIObjectMPD->mAdBreaks[periodId2].ads->emplace_back(false, false, true, "adId2", "url2", 30000, periodId2, 0, nullptr);

  // Add ads to mPeriodMap. mPeriodMap[periodId].adBreakId is non-empty for live at the beginning as per SetAlternateContents
  mPrivateCDAIObjectMPD->mPeriodMap[periodId1] = Period2AdData(false, periodId1, 30000 /*in ms*/,
    {
      std::make_pair (0, AdOnPeriod(0, 0)), // for adId1 idx=0, offset=0s
    });
  mPrivateCDAIObjectMPD->mPeriodMap[periodId2] = Period2AdData(false, periodId2, 0 /*in ms*/,
    {
      std::make_pair (0, AdOnPeriod(0, 0)), // for adId1 idx=0, offset=0s
    });
  mPrivateCDAIObjectMPD->PlaceAds(mAdMPDParseHelper);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].ads->at(0).placed, true);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].endPeriodOffset, 0);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].endPeriodId, "testPeriodId2"); // next period
  EXPECT_TRUE(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].mAdBreakPlaced); // adBreak placed
  EXPECT_FALSE(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].mSplitPeriod); // should not be marked as split period

  EXPECT_EQ(mPrivateCDAIObjectMPD->mPlacementObj.curAdIdx, 0);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPlacementObj.pendingAdbrkId, periodId2);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId1].duration, 30000); // in ms
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId1].adBreakId, "testPeriodId1");
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId1].offset2Ad[0].adIdx, 0);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId1].offset2Ad[0].adStartOffset, 0);

  // periodId2 is not updated in the current logic
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId2].duration, 0); // in ms
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId2].adBreakId, "testPeriodId2");
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId2].offset2Ad[0].adIdx, 0);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId2].offset2Ad[0].adStartOffset, 0);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].ads->at(0).basePeriodId, "testPeriodId1");
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].ads->at(0).basePeriodOffset, 0);
}

/**
 * @brief Tests the functionality of the PlaceAds method when multiple ads are present for a single adbreak
 */
TEST_F(AdManagerMPDTests, PlaceAdsTests_6)
{
  // not adding scte35 markers. These are mocked in PrivateObjectMPD instance
  // testPeriodId1 has 3 new fragments added.
  static const char *manifest1 =
R"(<?xml version="1.0" encoding="utf-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" availabilityStartTime="2023-01-01T00:00:00Z" maxSegmentDuration="PT2S" minBufferTime="PT4.000S" minimumUpdatePeriod="P100Y" profiles="urn:dvb:dash:profile:dvb-dash:2014,urn:dvb:dash:profile:dvb-dash:isoff-ext-live:2014" publishTime="2023-01-01T00:01:00Z" timeShiftBufferDepth="PT5M" type="dynamic">
  <Period id="testPeriodId0" start="PT0S">
    <AdaptationSet id="0" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p0_init.mp4" media="video_p0_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="0" d="5000" r="14" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
  <Period id="testPeriodId1" start="PT30S">
    <AdaptationSet id="1" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p1_init.mp4" media="video_p1_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="0" d="5000" r="16" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
</MPD>
)";

  // Second refresh. testPeriodId1 has remaining fragments added (Duration = 60s). testPeriodId2 added with one fragment
  static const char *manifest2 =
R"(<?xml version="1.0" encoding="utf-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" availabilityStartTime="2023-01-01T00:00:00Z" maxSegmentDuration="PT2S" minBufferTime="PT4.000S" minimumUpdatePeriod="P100Y" profiles="urn:dvb:dash:profile:dvb-dash:2014,urn:dvb:dash:profile:dvb-dash:isoff-ext-live:2014" publishTime="2023-01-01T00:01:00Z" timeShiftBufferDepth="PT5M" type="dynamic">
  <Period id="testPeriodId0" start="PT0S">
    <AdaptationSet id="0" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p0_init.mp4" media="video_p0_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="0" d="5000" r="14" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
  <Period id="testPeriodId1" start="PT30S">
    <AdaptationSet id="1" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p1_init.mp4" media="video_p1_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="0" d="5000" r="29" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
  <Period id="testPeriodId2" start="PT90S">
    <AdaptationSet id="1" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p1_init.mp4" media="video_p1_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="0" d="5000"/>
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
</MPD>
)";
  std::string periodId1 = "testPeriodId1";
  std::string periodId2 = "testPeriodId2";
  ProcessSourceMPD(manifest1);
  // Set curEndNumber to 14, adNextOffset = (14)*2000
  // Currently placing adId1
  mPrivateCDAIObjectMPD->mPlacementObj = PlacementObj(periodId1, periodId1, 14, 0, 28000, 0, false);

  // Add ads to the adBreak
  // testPeriodId1 ad duration is set to 35000 to force mismatch for sourceAdDurationMismatch
  mPrivateCDAIObjectMPD->mAdBreaks = {
    {periodId1, AdBreakObject(60000, std::make_shared<std::vector<AdNode>>(), "", 0, 60000)},
  };
  // 1 - to - 2 mapping of ad (ad1 30s, ad2 30s) and period (periodId1)
  mPrivateCDAIObjectMPD->mAdBreaks[periodId1].ads->emplace_back(false, false, true, "adId1", "url1", 30000, periodId1, 0, nullptr);
  // In FulFillAdObject the second ads, basePeriodID is not populated
  mPrivateCDAIObjectMPD->mAdBreaks[periodId1].ads->emplace_back(false, false, true, "adId2", "url2", 30000, "", -1, nullptr);

  // Add ads to mPeriodMap. mPeriodMap[periodId].adBreakId is non-empty for live at the beginning as per SetAlternateContents
  // Second ads AdOnPeriod is not populated in FulFillAdObject
  mPrivateCDAIObjectMPD->mPeriodMap[periodId1] = Period2AdData(false, periodId1, 28000 /*in ms*/,
    {
      std::make_pair (0, AdOnPeriod(0, 0)), // for adId1 idx=0, offset=0s
    });
  mPrivateCDAIObjectMPD->PlaceAds(mAdMPDParseHelper);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].ads->at(0).placed, true);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].endPeriodOffset, 0);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].endPeriodId, ""); // placement not completed
  EXPECT_FALSE(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].mAdBreakPlaced); // adBreak not placed
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].ads->at(0).basePeriodId, "testPeriodId1");
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].ads->at(0).basePeriodOffset, 0);

  EXPECT_EQ(mPrivateCDAIObjectMPD->mPlacementObj.curAdIdx, 1);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPlacementObj.pendingAdbrkId, periodId1);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId1].duration, 34000); // in ms
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId1].offset2Ad[0].adIdx, 0);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId1].offset2Ad[30000].adIdx, 1);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId2].duration, 0); // in ms
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].ads->at(1).basePeriodId, "testPeriodId1");
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].ads->at(1).basePeriodOffset, 30000); //60 Sec ad break - with 2 -30sec ads

  // next refresh and both ads to be completely placed
  ProcessSourceMPD(manifest2);
  mPrivateCDAIObjectMPD->PlaceAds(mAdMPDParseHelper);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].ads->at(1).placed, true);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].endPeriodOffset, 0);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].endPeriodId, periodId2); // placement not completed

  EXPECT_EQ(mPrivateCDAIObjectMPD->mPlacementObj.curAdIdx, -1);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId1].duration, 60000); // in ms
  EXPECT_TRUE(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].mAdBreakPlaced); // adBreak placed
}

/**
 * @brief Tests the functionality of the PlaceAds method when openPeriodID is filled and new period is added
 * for a spot level replacement. (ie, source period duration > ad duration + 2secs)
 * This is a special case scenario. Only one adbreak is present and that gets filled. In subsequent refresh,
 * new period is updated, but not having ad markers
 *
 */
TEST_F(AdManagerMPDTests, PlaceAdsTests_7)
{
  // not adding scte35 markers. These are mocked in PrivateObjectMPD instance
  static const char *manifest =
R"(<?xml version="1.0" encoding="utf-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" availabilityStartTime="2023-01-01T00:00:00Z" maxSegmentDuration="PT2S" minBufferTime="PT4.000S" minimumUpdatePeriod="P100Y" profiles="urn:dvb:dash:profile:dvb-dash:2014,urn:dvb:dash:profile:dvb-dash:isoff-ext-live:2014" publishTime="2023-01-01T00:01:00Z" timeShiftBufferDepth="PT5M" type="dynamic">
  <Period id="testPeriodId0" start="PT0S">
    <AdaptationSet id="0" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p0_init.mp4" media="video_p0_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="0" d="5000" r="16" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
  <Period id="testPeriodId1" start="PT34S">
    <AdaptationSet id="1" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p1_init.mp4" media="video_p1_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="0" d="5000" r="2" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
</MPD>
)";
  std::string periodId = "testPeriodId0";
  // testPeriodId1 has 3 fragments added in the mock
  ProcessSourceMPD(manifest);
  // Set curEndNumber to 13, adNextOffset = (13)*2000
  mPrivateCDAIObjectMPD->mPlacementObj = PlacementObj(periodId, periodId, 13, 0, 26000, 0, false);
  mPrivateCDAIObjectMPD->mAdtoInsertInNextBreakVec.push_back(mPrivateCDAIObjectMPD->mPlacementObj);

  // Add ads to the adBreak
  mPrivateCDAIObjectMPD->mAdBreaks = {
    {periodId, AdBreakObject(30000, std::make_shared<std::vector<AdNode>>(), "", 0, 30000)}
  };
  mPrivateCDAIObjectMPD->mAdBreaks[periodId].ads->emplace_back(false, false, true, "adId1", "url", 30000, periodId, 0, nullptr);

  // Add ads to mPeriodMap. mPeriodMap[periodId].adBreakId is non-empty for live at the beginning as per SetAlternateContents
  mPrivateCDAIObjectMPD->mPeriodMap[periodId] = Period2AdData(false, periodId, 26000 /*in ms*/,
    {
      std::make_pair (0, AdOnPeriod(0, 0)), // for adId1 idx=0, offset=0s
    });
  mPrivateCDAIObjectMPD->PlaceAds(mAdMPDParseHelper);
  // The current adbreak is filled and placementObj should now be reset
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPlacementObj.curEndNumber, 0);
  EXPECT_NE(mPrivateCDAIObjectMPD->mPlacementObj.pendingAdbrkId, periodId);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPlacementObj.curAdIdx, -1);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId].endPeriodId, periodId); // placement completed and ending in same period ID
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId].endPeriodOffset, 30000); // placement completed and ending in same period ID
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId].duration, 34000); // in ms
  EXPECT_TRUE(mPrivateCDAIObjectMPD->mAdBreaks[periodId].mAdBreakPlaced); // adBreak placed

  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId].ads->at(0).basePeriodId, periodId);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId].ads->at(0).basePeriodOffset, 0);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId].duration, 34000); // in ms
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId].adBreakId, "testPeriodId0");
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId].offset2Ad[0].adIdx, 0);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId].offset2Ad[0].adStartOffset, 0);


  // Update with same mpd again, and the params should not change
  mPrivateCDAIObjectMPD->PlaceAds(mAdMPDParseHelper);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPlacementObj.curEndNumber, 0);
  EXPECT_NE(mPrivateCDAIObjectMPD->mPlacementObj.pendingAdbrkId, periodId);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPlacementObj.curAdIdx, -1);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId].endPeriodId, periodId); // placement completed and ending in same period ID
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId].endPeriodOffset, 30000); // placement completed and ending in same period ID
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId].duration, 34000); // in ms
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId].adBreakId, "testPeriodId0");
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId].offset2Ad[0].adIdx, 0);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId].offset2Ad[0].adStartOffset, 0);
  EXPECT_TRUE(mPrivateCDAIObjectMPD->mAdBreaks[periodId].mAdBreakPlaced); // adBreak placed
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId].ads->at(0).basePeriodId, periodId);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId].ads->at(0).basePeriodOffset, 0);
}

/**
 * @brief Tests the functionality of the PlaceAds method when openPeriodID is currently getting filled
 * and an empty period appears at the upper boundary. This is a special case scenario.
 * Here, the openPeriodID is a spot placement use-case, where the source period duration > ad duration + 2secs
 * and it should be clearly updated as such
 * Extended to this to handle a use-case where an empty period follows the source period
 */
TEST_F(AdManagerMPDTests, PlaceAdsTests_8)
{
  // src period = 30sec, ad for 20 sec
  // not adding scte35 markers. These are mocked in PrivateObjectMPD instance
  static const char *manifest1 =
R"(<?xml version="1.0" encoding="utf-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" availabilityStartTime="2023-01-01T00:00:00Z" maxSegmentDuration="PT2S" minBufferTime="PT4.000S" minimumUpdatePeriod="P100Y" profiles="urn:dvb:dash:profile:dvb-dash:2014,urn:dvb:dash:profile:dvb-dash:isoff-ext-live:2014" publishTime="2023-01-01T00:01:00Z" timeShiftBufferDepth="PT5M" type="dynamic">
  <Period id="testPeriodId0" start="PT0S">
    <AdaptationSet id="0" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p0_init.mp4" media="video_p0_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="0" d="5000" r="9" />
            <S t="50000" d="2500" r="0" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
  <Period id="testPeriodId1" start="PT30S">
   <!-- This is an empty period that can appear in the manifest -->
  </Period>
</MPD>
)";

//segments = (9+1)*2 + (3+1)*1 + (2+1)*2 = 30 Seconds
  static const char *manifest2 =
R"(<?xml version="1.0" encoding="utf-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" availabilityStartTime="2023-01-01T00:00:00Z" maxSegmentDuration="PT2S" minBufferTime="PT4.000S" minimumUpdatePeriod="P100Y" profiles="urn:dvb:dash:profile:dvb-dash:2014,urn:dvb:dash:profile:dvb-dash:isoff-ext-live:2014" publishTime="2023-01-01T00:01:00Z" timeShiftBufferDepth="PT5M" type="dynamic">
  <Period id="testPeriodId0" start="PT0S">
    <AdaptationSet id="0" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p0_init.mp4" media="video_p0_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="0" d="5000" r="9" />
            <S t="50000" d="2500" r="3" />
            <S t="60000" d="5000" r="2" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
  <Period id="testPeriodId1" start="PT30S">
  <!-- This is an empty period that can appear in the manifest -->
  </Period>
    <Period id="testPeriodId2" start="PT30S">
        <AdaptationSet id="0" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p0_init.mp4" media="video_p0_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="75000" d="5000" r="0" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
</MPD>
)";

  std::string periodId = "testPeriodId0";
  // testPeriodId1 has 3 fragments added in the mock
  ProcessSourceMPD(manifest1);
  // Set curEndNumber to 9, adNextOffset = (9)*2000
  mPrivateCDAIObjectMPD->mPlacementObj = PlacementObj(periodId, periodId, 0, 0, 0, 0, false);
  mPrivateCDAIObjectMPD->mAdtoInsertInNextBreakVec.push_back(mPrivateCDAIObjectMPD->mPlacementObj);
  mPrivateCDAIObjectMPD->mPlacementObj.curEndNumber = 9;
  mPrivateCDAIObjectMPD->mPlacementObj.adNextOffset = 18000;
  // Have placed 18.0 Seconds of Ad and we know the period is at least 21.0 seconds long
  // Increase Ad to 20.0 or provide Ad of 20.0 Seconds??
  // From manifest1 we cannot determine when the period ends
  // Expect add insertion to complete and
  // [PlaceAds][612][CDAI] diff [-20000] NOT close to period end, period:testPeriodId0 duration[0]

  // Add ads to the adBreak
  mPrivateCDAIObjectMPD->mAdBreaks = {
    {periodId, AdBreakObject(20000, std::make_shared<std::vector<AdNode>>(), "", 0, 20000)}
  };
  mPrivateCDAIObjectMPD->mAdBreaks[periodId].ads->emplace_back(false, false, true, "adId1", "url", 20000, periodId, 0, nullptr);

  // Add ads to mPeriodMap. mPeriodMap[periodId].adBreakId is non-empty for live at the beginning as per SetAlternateContents
  mPrivateCDAIObjectMPD->mPeriodMap[periodId] = Period2AdData(false, periodId, 18000 /*in ms*/,
    {
      std::make_pair (0, AdOnPeriod(0, 0)), // for adId1 idx=0, offset=0s
    });
  mPrivateCDAIObjectMPD->PlaceAds(mAdMPDParseHelper);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPlacementObj.curEndNumber, 11);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPlacementObj.pendingAdbrkId, periodId);
  // This is because ad is placed, and hence we will increment curAdIdx
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPlacementObj.curAdIdx, 1);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId].duration, 21000); // in ms
  EXPECT_FALSE(mPrivateCDAIObjectMPD->mAdBreaks[periodId].mAdBreakPlaced); // adBreak not placed

  // Update with same mpd again, and the params should not change
  ProcessSourceMPD(manifest2);
  mPrivateCDAIObjectMPD->PlaceAds(mAdMPDParseHelper);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId].endPeriodId, periodId); // placement completed and ending in same period ID
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId].endPeriodOffset, 20000); // placement completed and offset updated
  EXPECT_TRUE(mPrivateCDAIObjectMPD->mAdBreaks[periodId].mAdBreakPlaced); // adBreak placed
  // The current adbreak is filled and placementObj should now be reset
  EXPECT_NE(mPrivateCDAIObjectMPD->mPlacementObj.pendingAdbrkId, periodId);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPlacementObj.curAdIdx, -1);
  // This is because ad is placed in previous iteration with adjustEndPeriodOffset set to true. This needs to be revisited
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId].duration, 21000); // in ms
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId].adBreakId, "testPeriodId0");
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId].offset2Ad[0].adIdx, 0);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId].offset2Ad[0].adStartOffset, 0);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId].ads->at(0).basePeriodId,periodId);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId].ads->at(0).basePeriodOffset, 0);
}

/** @brief Tests the functionality of CheckForAdTerminate method with different params
 */
TEST_F(AdManagerMPDTests, CheckForAdTerminateTests_1)
{
  uint32_t adDuration = 20000;

  // No entry in mCurAds, should return false
  bool ret = mPrivateCDAIObjectMPD->CheckForAdTerminate(0);
  EXPECT_EQ(ret, false);

  // Add an entry in mCurAds, but adIdx is not present in mCurAds, should return false
  mPrivateCDAIObjectMPD->mCurAds = std::make_shared<std::vector<AdNode>>();
  mPrivateCDAIObjectMPD->mCurAds->emplace_back(false, false, true, "adId1", "url", adDuration, "testPeriodId0", 0, nullptr);

  ret = mPrivateCDAIObjectMPD->CheckForAdTerminate(0);
  EXPECT_EQ(ret, false);

  // mCurAdIdx is present in mCurAds, should return false when offset is less than duration
  mPrivateCDAIObjectMPD->mCurAdIdx = 0;
  ret = mPrivateCDAIObjectMPD->CheckForAdTerminate(0);
  EXPECT_EQ(ret, false);

  // mCurAdIdx is present in mCurAds, should return true when offset is greater than duration + OFFSET_ALIGN_FACTOR
  ret = mPrivateCDAIObjectMPD->CheckForAdTerminate(adDuration + OFFSET_ALIGN_FACTOR);
  EXPECT_EQ(ret, true);


  // mCurAdIdx is present in mCurAds, should return false when offset -ve
  ret = mPrivateCDAIObjectMPD->CheckForAdTerminate(-1);
  EXPECT_EQ(ret, false);
}

/**
 * @brief Tests the functionality of the PlaceAds method when openPeriodID is currently getting filled
 * and the duration of an advertised fragment is modified between refreshes.
 * This causes a diff between periodDelta calculated duration
 * and full period duration at any point of time.
 */
TEST_F(AdManagerMPDTests, PlaceAdsTests_9)
{
  // not adding scte35 markers. These are mocked in PrivateObjectMPD instance
  // src period testPeriodId1, having a fragment of 2sec
  // final duration of testPeriodId1 is 10sec
  static const char *manifest1 =
R"(<?xml version="1.0" encoding="utf-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" availabilityStartTime="2023-01-01T00:00:00Z" maxSegmentDuration="PT2S" minBufferTime="PT4.000S" minimumUpdatePeriod="P100Y" profiles="urn:dvb:dash:profile:dvb-dash:2014,urn:dvb:dash:profile:dvb-dash:isoff-ext-live:2014" publishTime="2023-01-01T00:01:00Z" timeShiftBufferDepth="PT5M" type="dynamic">
  <Period id="testPeriodId0" start="PT0S">
    <AdaptationSet id="0" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p0_init.mp4" media="video_p0_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="0" d="5000" r="14" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
  <Period id="testPeriodId1" start="PT30S">
    <AdaptationSet id="0" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p0_init.mp4" media="video_p0_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="75000" d="5000" r="0" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
</MPD>
)";
  // src period testPeriodId1, new fragment of 2sec is advertised, old fragment duration changed to 3sec
  // total duration is now 5sec.
  static const char *manifest2 =
R"(<?xml version="1.0" encoding="utf-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" availabilityStartTime="2023-01-01T00:00:00Z" maxSegmentDuration="PT2S" minBufferTime="PT4.000S" minimumUpdatePeriod="P100Y" profiles="urn:dvb:dash:profile:dvb-dash:2014,urn:dvb:dash:profile:dvb-dash:isoff-ext-live:2014" publishTime="2023-01-01T00:01:00Z" timeShiftBufferDepth="PT5M" type="dynamic">
  <Period id="testPeriodId0" start="PT0S">
    <AdaptationSet id="0" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p0_init.mp4" media="video_p0_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="0" d="5000" r="14" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
  <Period id="testPeriodId1" start="PT30S">
    <AdaptationSet id="0" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p0_init.mp4" media="video_p0_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="75000" d="7500" r="0" />
            <S t="82500" d="5000" r="0" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
</MPD>
)";
  // src period testPeriodId1, finishing up the period, filling to 10sec
  // add new period with duration to place the current ad
  static const char *manifest3 =
R"(<?xml version="1.0" encoding="utf-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" availabilityStartTime="2023-01-01T00:00:00Z" maxSegmentDuration="PT2S" minBufferTime="PT4.000S" minimumUpdatePeriod="P100Y" profiles="urn:dvb:dash:profile:dvb-dash:2014,urn:dvb:dash:profile:dvb-dash:isoff-ext-live:2014" publishTime="2023-01-01T00:01:00Z" timeShiftBufferDepth="PT5M" type="dynamic">
  <Period id="testPeriodId0" start="PT0S">
    <AdaptationSet id="0" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p0_init.mp4" media="video_p0_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="0" d="5000" r="14" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
  <Period id="testPeriodId1" start="PT30S">
    <AdaptationSet id="0" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p0_init.mp4" media="video_p0_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="75000" d="7500" r="0" />
            <S t="82500" d="5000" r="2" />
            <S t="97500" d="2500" r="0" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
  <Period id="testPeriodId2" start="PT40S">
    <AdaptationSet id="0" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p0_init.mp4" media="video_p0_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="100000" d="5000" r="0" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
</MPD>
)";
  std::string periodId = "testPeriodId1";
  // testPeriodId1 has 1 fragment added in the mock
  ProcessSourceMPD(manifest1);
  // Set curEndNumber to 0, adNextOffset = (0)*2000
  mPrivateCDAIObjectMPD->mPlacementObj = PlacementObj(periodId, periodId, 0, 0, 0, 0, false);
  mPrivateCDAIObjectMPD->mAdtoInsertInNextBreakVec.push_back(mPrivateCDAIObjectMPD->mPlacementObj);
  mPrivateCDAIObjectMPD->mPlacementObj.curEndNumber = 0;
  mPrivateCDAIObjectMPD->mPlacementObj.adNextOffset = 0;

  // Add ads to the adBreak, 20sec duration ad
  mPrivateCDAIObjectMPD->mAdBreaks = {
    {periodId, AdBreakObject(10000, std::make_shared<std::vector<AdNode>>(), "", 0, 10000)}
  };
  mPrivateCDAIObjectMPD->mAdBreaks[periodId].ads->emplace_back(false, false, true, "adId1", "url", 10000, periodId, 0, nullptr);

  // Add ads to mPeriodMap. mPeriodMap[periodId].adBreakId is non-empty for live at the beginning as per SetAlternateContents
  // p2AdData.duration is 0 here
  mPrivateCDAIObjectMPD->mPeriodMap[periodId] = Period2AdData(false, periodId, 0 /*in ms*/,
    {
      std::make_pair (0, AdOnPeriod(0, 0)), // for adId1 idx=0, offset=0s
    });
  mPrivateCDAIObjectMPD->PlaceAds(mAdMPDParseHelper);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPlacementObj.pendingAdbrkId, periodId);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPlacementObj.curEndNumber, 1);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPlacementObj.adNextOffset, 2000);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId].duration, 2000); // in ms

  // Update with manifest2, and the duration in mPeriodMap should be updated correctly
  ProcessSourceMPD(manifest2);
  mPrivateCDAIObjectMPD->PlaceAds(mAdMPDParseHelper);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPlacementObj.pendingAdbrkId, periodId);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPlacementObj.curEndNumber, 2);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPlacementObj.adNextOffset, 5000);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId].duration, 5000); // in ms

  // Update with manifest3, and the duration in mPeriodMap should be updated correctly
  // And ad should be placed
  ProcessSourceMPD(manifest3);
  mPrivateCDAIObjectMPD->PlaceAds(mAdMPDParseHelper);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPlacementObj.curAdIdx, -1); // ad is placed
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId].duration, 10000); // in ms
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId].adBreakId, periodId);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId].offset2Ad[0].adIdx, 0);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId].offset2Ad[0].adStartOffset, 0);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId].ads->at(0).placed, true);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId].endPeriodOffset, 0);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId].endPeriodId, "testPeriodId2"); // next period
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId].ads->at(0).basePeriodId, "testPeriodId1");
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId].ads->at(0).basePeriodOffset, 0);
  EXPECT_EQ(mPrivateCDAIObjectMPD->currentAdPeriodClosed, true); // ad is placed, validate this variable
  EXPECT_TRUE(mPrivateCDAIObjectMPD->mAdBreaks[periodId].mAdBreakPlaced); // adBreak placed
}

/**
 * @brief Tests the functionality of the PlaceAds method when openPeriodID is currently getting culled
 */
TEST_F(AdManagerMPDTests, PlaceAdsTests_10)
{
  // not adding scte35 markers. These are mocked in PrivateObjectMPD instance
  // src period testPeriodId1, having a final duration of 20s, culled with a tsbDepth of 10s
  // one fragment is advertised
  static const char *manifest1 =
R"(<?xml version="1.0" encoding="utf-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" availabilityStartTime="2023-01-01T00:00:00Z" maxSegmentDuration="PT2S" minBufferTime="PT4.000S" minimumUpdatePeriod="P100Y" profiles="urn:dvb:dash:profile:dvb-dash:2014,urn:dvb:dash:profile:dvb-dash:isoff-ext-live:2014" publishTime="2023-01-01T00:01:00Z" timeShiftBufferDepth="PT5M" type="dynamic">
  <Period id="testPeriodId0" start="PT0S">
    <AdaptationSet id="0" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p0_init.mp4" media="video_p0_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="55000" d="5000" r="3" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
  <Period id="testPeriodId1" start="PT30S">
    <AdaptationSet id="0" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p0_init.mp4" media="video_p0_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="75000" d="5000" r="0" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
</MPD>
)";

  // testPeriodId1 is now at duration 10sec, culled 0sec
  static const char *manifest2 =
R"(<?xml version="1.0" encoding="utf-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" availabilityStartTime="2023-01-01T00:00:00Z" maxSegmentDuration="PT2S" minBufferTime="PT4.000S" minimumUpdatePeriod="P100Y" profiles="urn:dvb:dash:profile:dvb-dash:2014,urn:dvb:dash:profile:dvb-dash:isoff-ext-live:2014" publishTime="2023-01-01T00:01:00Z" timeShiftBufferDepth="PT5M" type="dynamic">
  <Period id="testPeriodId1" start="PT30S">
    <AdaptationSet id="0" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p0_init.mp4" media="video_p0_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="75000" d="5000" r="4" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
</MPD>
)";

  // testPeriodId1 is now at duration 20sec, culled 10sec
  static const char *manifest3 =
R"(<?xml version="1.0" encoding="utf-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" availabilityStartTime="2023-01-01T00:00:00Z" maxSegmentDuration="PT2S" minBufferTime="PT4.000S" minimumUpdatePeriod="P100Y" profiles="urn:dvb:dash:profile:dvb-dash:2014,urn:dvb:dash:profile:dvb-dash:isoff-ext-live:2014" publishTime="2023-01-01T00:01:00Z" timeShiftBufferDepth="PT5M" type="dynamic">
  <Period id="testPeriodId1" start="PT30S">
    <AdaptationSet id="0" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p0_init.mp4" media="video_p0_$Number$.m4s" startNumber="6">
          <SegmentTimeline>
            <S t="100000" d="5000" r="4" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
</MPD>
)";

  // testPeriodId2 is now culled 18sec
  static const char *manifest4 =
R"(<?xml version="1.0" encoding="utf-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" availabilityStartTime="2023-01-01T00:00:00Z" maxSegmentDuration="PT2S" minBufferTime="PT4.000S" minimumUpdatePeriod="P100Y" profiles="urn:dvb:dash:profile:dvb-dash:2014,urn:dvb:dash:profile:dvb-dash:isoff-ext-live:2014" publishTime="2023-01-01T00:01:00Z" timeShiftBufferDepth="PT5M" type="dynamic">
  <Period id="testPeriodId1" start="PT30S">
    <AdaptationSet id="0" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p0_init.mp4" media="video_p0_$Number$.m4s" startNumber="10">
          <SegmentTimeline>
            <S t="120000" d="5000" r="0" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
  <Period id="testPeriodId2" start="PT50S">
    <AdaptationSet id="0" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p0_init.mp4" media="video_p0_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="125000" d="5000" r="3" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
</MPD>
)";

  std::string periodId = "testPeriodId1";
  // testPeriodId1 has 1 fragment added in the mock
  ProcessSourceMPD(manifest1);
  // Set curEndNumber to 0, adNextOffset = (0)*2000
  mPrivateCDAIObjectMPD->mPlacementObj = PlacementObj(periodId, periodId, 0, 0, 0, 0, false);
  mPrivateCDAIObjectMPD->mAdtoInsertInNextBreakVec.push_back(mPrivateCDAIObjectMPD->mPlacementObj);
  mPrivateCDAIObjectMPD->mPlacementObj.curEndNumber = 0;
  mPrivateCDAIObjectMPD->mPlacementObj.adNextOffset = 0;

  // Add ads to the adBreak, 20sec duration ad
  mPrivateCDAIObjectMPD->mAdBreaks = {
    {periodId, AdBreakObject(20000, std::make_shared<std::vector<AdNode>>(), "", 0, 20000)}
  };
  mPrivateCDAIObjectMPD->mAdBreaks[periodId].ads->emplace_back(false, false, true, "adId1", "url", 20000, periodId, 0, nullptr);

  // Add ads to mPeriodMap. mPeriodMap[periodId].adBreakId is non-empty for live at the beginning as per SetAlternateContents
  // p2AdData.duration is 0 here
  mPrivateCDAIObjectMPD->mPeriodMap[periodId] = Period2AdData(false, periodId, 0 /*in ms*/,
    {
      std::make_pair (0, AdOnPeriod(0, 0)), // for adId1 idx=0, offset=0s
    });
  mPrivateCDAIObjectMPD->PlaceAds(mAdMPDParseHelper);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPlacementObj.pendingAdbrkId, periodId);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPlacementObj.curEndNumber, 1);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPlacementObj.adNextOffset, 2000);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId].duration, 2000); // in ms

  // Update with manifest2, and the duration in mPeriodMap should be updated correctly
  ProcessSourceMPD(manifest2);
  mPrivateCDAIObjectMPD->PlaceAds(mAdMPDParseHelper);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPlacementObj.pendingAdbrkId, periodId);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPlacementObj.curEndNumber, 5);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPlacementObj.adNextOffset, 10000);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId].duration, 10000); // in ms

  // Update with manifest3, and the duration in mPeriodMap should be updated correctly
  ProcessSourceMPD(manifest3);
  mPrivateCDAIObjectMPD->PlaceAds(mAdMPDParseHelper);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId].duration, 20000); // in ms
  EXPECT_EQ(mPrivateCDAIObjectMPD->currentAdPeriodClosed, true); // ad is placed, validate this variable

  // Update with manifest4, and the duration in mPeriodMap should remain the same and ad should be placed
  ProcessSourceMPD(manifest4);
  mPrivateCDAIObjectMPD->PlaceAds(mAdMPDParseHelper);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPlacementObj.curAdIdx, -1); // ad is placed
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId].duration, 20000); // in ms
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId].adBreakId, periodId);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId].offset2Ad[0].adIdx, 0);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId].offset2Ad[0].adStartOffset, 0);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId].ads->at(0).placed, true);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId].ads->at(0).basePeriodId, "testPeriodId1");
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId].ads->at(0).basePeriodOffset, 0);
  // TODO: There is a bug here, actually (int diff = (int)(currPeriodDuration - abObj.endPeriodOffset)) is coming as -ve here
  // Hence this is passing for now.
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId].endPeriodOffset, 0);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId].endPeriodId, "testPeriodId2"); // next period
  EXPECT_TRUE(mPrivateCDAIObjectMPD->mAdBreaks[periodId].mAdBreakPlaced); // adBreak placed
}

/**
 * @brief Tests the functionality of the PlaceAds method when split period case is true
 * Also validates the special case where the second period in split period has a separate adbreak which should be deleted
 * 1 ad of 30 seconds, in 2 source periods of 15 seconds
 * P1(30s), (SCTE 30s)P2(15s), (SCTE15s)P3(15s), P4(30s)
 * .........AD1(30s)                           ........
 */
TEST_F(AdManagerMPDTests, PlaceAdsTests_11)
{
  // not adding scte35 markers. These are mocked in PrivateObjectMPD instance
  // testPeriodId1 has 0 new fragments added, to get periodDelta == 0 case
  // testPeriodId2 has 2 new fragments added
  static const char *manifest1 =
R"(<?xml version="1.0" encoding="utf-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" availabilityStartTime="2023-01-01T00:00:00Z" maxSegmentDuration="PT2S" minBufferTime="PT4.000S" minimumUpdatePeriod="P100Y" profiles="urn:dvb:dash:profile:dvb-dash:2014,urn:dvb:dash:profile:dvb-dash:isoff-ext-live:2014" publishTime="2023-01-01T00:01:00Z" timeShiftBufferDepth="PT5M" type="dynamic">
  <Period id="testPeriodId0" start="PT0S">
    <AdaptationSet id="0" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p0_init.mp4" media="video_p0_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="0" d="5000" r="14" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
  <Period id="testPeriodId1" start="PT30S">
    <AdaptationSet id="1" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p1_init.mp4" media="video_p1_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="75000" d="5000" r="6" />
            <S t="110000" d="2500" r="0" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
  <Period id="testPeriodId2" start="PT45S">
    <AdaptationSet id="1" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p1_init.mp4" media="video_p1_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="112500" d="2500" r="0" />
            <S t="115000" d="5000" r="0" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
</MPD>
)";

  // testPeriodId2 has 6 new fragments added
  // testPeriodId3 has 1 new fragments added
  static const char *manifest2 =
R"(<?xml version="1.0" encoding="utf-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" availabilityStartTime="2023-01-01T00:00:00Z" maxSegmentDuration="PT2S" minBufferTime="PT4.000S" minimumUpdatePeriod="P100Y" profiles="urn:dvb:dash:profile:dvb-dash:2014,urn:dvb:dash:profile:dvb-dash:isoff-ext-live:2014" publishTime="2023-01-01T00:01:00Z" timeShiftBufferDepth="PT5M" type="dynamic">
  <Period id="testPeriodId0" start="PT0S">
    <AdaptationSet id="0" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p0_init.mp4" media="video_p0_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="0" d="5000" r="14" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
  <Period id="testPeriodId1" start="PT30S">
    <AdaptationSet id="1" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p1_init.mp4" media="video_p1_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="75000" d="5000" r="6" />
            <S t="110000" d="2500" r="0" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
  <Period id="testPeriodId2" start="PT45S">
    <AdaptationSet id="1" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p1_init.mp4" media="video_p1_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="112500" d="2500" r="0" />
            <S t="115000" d="5000" r="6" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
  <Period id="testPeriodId3" start="PT60S">
    <AdaptationSet id="1" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p1_init.mp4" media="video_p1_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="150000" d="5000" r="0" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
</MPD>
)";
  std::string periodId1 = "testPeriodId1";
  std::string periodId2 = "testPeriodId2";
  std::string periodId3 = "testPeriodId3";
  ProcessSourceMPD(manifest1);
  // Set curEndNumber to 8, adNextOffset = (7)*2000 + 1*1000
  mPrivateCDAIObjectMPD->mPlacementObj = PlacementObj(periodId1, periodId1, 8, 0, 15000, 0, false);
  // placement object present for periodId2
  mPrivateCDAIObjectMPD->mAdtoInsertInNextBreakVec.emplace_back(periodId2, periodId2, 0, 0, 0, 0, false);

  // Add ads to the adBreak
  mPrivateCDAIObjectMPD->mAdBreaks = {
    {periodId1, AdBreakObject(30000, std::make_shared<std::vector<AdNode>>(), "", 0, 30000)},
    {periodId2, AdBreakObject(15000, std::make_shared<std::vector<AdNode>>(), "", 0, 15000)}
  };
  // 1 - to - 1 mapping of ad and period
  mPrivateCDAIObjectMPD->mAdBreaks[periodId1].ads->emplace_back(false, false, true, "adId1", "url1", 30000, periodId1, 0, nullptr);
  mPrivateCDAIObjectMPD->mAdBreaks[periodId2].ads->emplace_back(false, false, true, "adId2", "url2", 15000, periodId2, 0, nullptr);

  // Add ads to mPeriodMap. mPeriodMap[periodId].adBreakId is non-empty for live at the beginning as per SetAlternateContents
  mPrivateCDAIObjectMPD->mPeriodMap[periodId1] = Period2AdData(false, periodId1, 15000 /*in ms*/,
    {
      std::make_pair (0, AdOnPeriod(0, 0)), // for adId1 idx=0, offset=0s
    });
  mPrivateCDAIObjectMPD->mPeriodMap[periodId2] = Period2AdData(false, periodId2, 0 /*in ms*/,
    {
      std::make_pair (0, AdOnPeriod(0, 0)), // for adId1 idx=0, offset=0s
    });
  mPrivateCDAIObjectMPD->PlaceAds(mAdMPDParseHelper);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].mSplitPeriod, true);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].ads->at(0).placed, false);
  EXPECT_FALSE(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].mAdBreakPlaced); // adBreak not placed
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap.size(), 2); // periodId2 map created
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId1].duration,15000); //periodmap of periodid1 duration
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId1].adBreakId, periodId1);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId1].offset2Ad[0].adIdx, 0);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId1].offset2Ad[0].adStartOffset, 0);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].ads->at(0).basePeriodId, "testPeriodId1");
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPlacementObj.openPeriodId, periodId2); // open period changed to periodId2
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPlacementObj.adNextOffset, 18000); // 3sec from periodId2 also placed
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPlacementObj.adStartOffset, 15000); // ad starts from 15sec in periodId2

  // Check adbreak for periodId2 is deleted
  EXPECT_EQ(mPrivateCDAIObjectMPD->isAdBreakObjectExist(periodId2), false);
  //mPeriodMap[periodId2] is overwritten
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId2].duration, 3000); //periodmap of periodid1 duration
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId2].adBreakId, periodId1); //periodId2 shares the adbreak of periodId1
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId2].offset2Ad[0].adIdx, 0);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId2].offset2Ad[0].adStartOffset, 15000);


  ProcessSourceMPD(manifest2);
  mPrivateCDAIObjectMPD->PlaceAds(mAdMPDParseHelper);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].ads->at(0).placed, true);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].ads->at(0).basePeriodId, "testPeriodId1");
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].ads->at(0).basePeriodOffset, 0);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId2].duration,15000); //periodmap of periodid2 duration
  EXPECT_TRUE(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].mAdBreakPlaced); // adBreak placed
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].endPeriodId, periodId3);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].endPeriodOffset, 0);

}


/**
 * @brief Tests the functionality of the PlaceAds method when split period case is true
 * 2 ads of 20 seconds and 10 seconds, in 2 source periods of 15 seconds
 * P1(30s), (SCTE 30s)P2(15s), P3(15s), P4(30s)
 * .........AD1(20s),AD2(10s)         ........
 */
TEST_F(AdManagerMPDTests, PlaceAdsTests_12)
{
  // not adding scte35 markers. These are mocked in PrivateObjectMPD instance
  // testPeriodId1 has 0 new fragments added, to get periodDelta == 0 case
  // testPeriodId2 has 2 new fragments added
  static const char *manifest1 =
R"(<?xml version="1.0" encoding="utf-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" availabilityStartTime="2023-01-01T00:00:00Z" maxSegmentDuration="PT2S" minBufferTime="PT4.000S" minimumUpdatePeriod="P100Y" profiles="urn:dvb:dash:profile:dvb-dash:2014,urn:dvb:dash:profile:dvb-dash:isoff-ext-live:2014" publishTime="2023-01-01T00:01:00Z" timeShiftBufferDepth="PT5M" type="dynamic">
  <Period id="testPeriodId0" start="PT0S">
    <AdaptationSet id="0" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p0_init.mp4" media="video_p0_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="0" d="5000" r="14" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
  <Period id="testPeriodId1" start="PT30S">
    <AdaptationSet id="1" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p1_init.mp4" media="video_p1_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="75000" d="5000" r="6" />
            <S t="110000" d="2500" r="0" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
  <Period id="testPeriodId2" start="PT45S">
    <AdaptationSet id="1" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p1_init.mp4" media="video_p1_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="112500" d="2500" r="0" />
            <S t="115000" d="5000" r="0" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
</MPD>
)";

  // testPeriodId2 has 6 new fragments added
  // testPeriodId3 has 1 new fragments added
  static const char *manifest2 =
R"(<?xml version="1.0" encoding="utf-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" availabilityStartTime="2023-01-01T00:00:00Z" maxSegmentDuration="PT2S" minBufferTime="PT4.000S" minimumUpdatePeriod="P100Y" profiles="urn:dvb:dash:profile:dvb-dash:2014,urn:dvb:dash:profile:dvb-dash:isoff-ext-live:2014" publishTime="2023-01-01T00:01:00Z" timeShiftBufferDepth="PT5M" type="dynamic">
  <Period id="testPeriodId0" start="PT0S">
    <AdaptationSet id="0" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p0_init.mp4" media="video_p0_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="0" d="5000" r="14" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
  <Period id="testPeriodId1" start="PT30S">
    <AdaptationSet id="1" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p1_init.mp4" media="video_p1_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="75000" d="5000" r="6" />
            <S t="110000" d="2500" r="0" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
  <Period id="testPeriodId2" start="PT45S">
    <AdaptationSet id="1" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p1_init.mp4" media="video_p1_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="112500" d="2500" r="0" />
            <S t="115000" d="5000" r="6" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
  <Period id="testPeriodId3" start="PT60S">
    <AdaptationSet id="1" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p1_init.mp4" media="video_p1_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="150000" d="5000" r="0" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
</MPD>
)";
  std::string periodId1 = "testPeriodId1";
  std::string periodId2 = "testPeriodId2";
  std::string periodId3 = "testPeriodId3";
  ProcessSourceMPD(manifest1);
  // Set curEndNumber to 8, adNextOffset = (7)*2000 + 1*1000
  mPrivateCDAIObjectMPD->mPlacementObj = PlacementObj(periodId1, periodId1, 8, 0, 15000, 0, false);

  // Add ads to the adBreak
  mPrivateCDAIObjectMPD->mAdBreaks = {
    {periodId1, AdBreakObject(30000, std::make_shared<std::vector<AdNode>>(), "", 0, 30000)}
  };
  // 1 - to - 2 mapping of ad and adbreak
  mPrivateCDAIObjectMPD->mAdBreaks[periodId1].ads->emplace_back(false, false, true, "adId1", "url1", 20000, periodId1, 0, nullptr);
  // Second ad doesn't have basePeriodId and basePeriodOffset set
  mPrivateCDAIObjectMPD->mAdBreaks[periodId1].ads->emplace_back(false, false, true, "adId2", "url2", 10000, "", -1, nullptr);

  // Add ads to mPeriodMap. mPeriodMap[periodId].adBreakId is non-empty for live at the beginning as per SetAlternateContents
  mPrivateCDAIObjectMPD->mPeriodMap[periodId1] = Period2AdData(false, periodId1, 15000 /*in ms*/,
    {
      std::make_pair (0, AdOnPeriod(0, 0)), // for adId1 idx=0, offset=0s
    });
  mPrivateCDAIObjectMPD->PlaceAds(mAdMPDParseHelper);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].mSplitPeriod, true);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].ads->at(0).placed, false);
  EXPECT_FALSE(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].mAdBreakPlaced); // adBreak not placed
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap.size(), 2); // periodId2 map created
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId1].duration,15000); //periodmap of periodid1 duration
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId1].adBreakId, periodId1);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId1].offset2Ad[0].adIdx, 0);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId1].offset2Ad[0].adStartOffset, 0);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPlacementObj.openPeriodId, periodId2); // open period changed to periodId2
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPlacementObj.adNextOffset, 18000); // 3sec from periodId2 also placed
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPlacementObj.adStartOffset, 15000); // ad starts from 15sec in periodId2

  ProcessSourceMPD(manifest2);
  mPrivateCDAIObjectMPD->PlaceAds(mAdMPDParseHelper);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].ads->at(0).placed, true);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].ads->at(1).placed, true);
  EXPECT_TRUE(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].mAdBreakPlaced); // adBreak placed
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].endPeriodId, periodId3);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].endPeriodOffset, 0);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].ads->at(0).basePeriodId, "testPeriodId1");
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].ads->at(0).basePeriodOffset, 0);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].ads->at(1).basePeriodId, "testPeriodId2");
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].ads->at(1).basePeriodOffset, 5000); //15 sec period - 20 sec ad
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId2].duration,15000); //periodmap of periodid2 duration
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId2].adBreakId, periodId1); //adbreak share from periodId1
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId2].offset2Ad[0].adIdx, 0);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId2].offset2Ad[0].adStartOffset, 15000);

}

/**
 * @brief Tests the functionality of the PlaceAds method when split period case is true
 * 2 ads of 20 seconds and 10 seconds, in 2 source periods of 15 seconds
 * P1(30s), (SCTE 30s)P2(15s), P3(15s), P4(30s)
 * .........AD1(10s),AD2(20s)         ........
 */
TEST_F(AdManagerMPDTests, PlaceAdsTests_13)
{
  // not adding scte35 markers. These are mocked in PrivateObjectMPD instance
  // testPeriodId1 has 4 new fragments added
  // testPeriodId2 has 2 new fragments added
  static const char *manifest1 =
R"(<?xml version="1.0" encoding="utf-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" availabilityStartTime="2023-01-01T00:00:00Z" maxSegmentDuration="PT2S" minBufferTime="PT4.000S" minimumUpdatePeriod="P100Y" profiles="urn:dvb:dash:profile:dvb-dash:2014,urn:dvb:dash:profile:dvb-dash:isoff-ext-live:2014" publishTime="2023-01-01T00:01:00Z" timeShiftBufferDepth="PT5M" type="dynamic">
  <Period id="testPeriodId0" start="PT0S">
    <AdaptationSet id="0" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p0_init.mp4" media="video_p0_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="0" d="5000" r="14" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
  <Period id="testPeriodId1" start="PT30S">
    <AdaptationSet id="1" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p1_init.mp4" media="video_p1_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="75000" d="5000" r="6" />
            <S t="110000" d="2500" r="0" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
  <Period id="testPeriodId2" start="PT45S">
    <AdaptationSet id="1" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p1_init.mp4" media="video_p1_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="112500" d="2500" r="0" />
            <S t="115000" d="5000" r="0" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
</MPD>
)";

  // testPeriodId2 has 6 new fragments added
  // testPeriodId3 has 1 new fragments added
  static const char *manifest2 =
R"(<?xml version="1.0" encoding="utf-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" availabilityStartTime="2023-01-01T00:00:00Z" maxSegmentDuration="PT2S" minBufferTime="PT4.000S" minimumUpdatePeriod="P100Y" profiles="urn:dvb:dash:profile:dvb-dash:2014,urn:dvb:dash:profile:dvb-dash:isoff-ext-live:2014" publishTime="2023-01-01T00:01:00Z" timeShiftBufferDepth="PT5M" type="dynamic">
  <Period id="testPeriodId0" start="PT0S">
    <AdaptationSet id="0" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p0_init.mp4" media="video_p0_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="0" d="5000" r="14" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
  <Period id="testPeriodId1" start="PT30S">
    <AdaptationSet id="1" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p1_init.mp4" media="video_p1_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="75000" d="5000" r="6" />
            <S t="110000" d="2500" r="0" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
  <Period id="testPeriodId2" start="PT45S">
    <AdaptationSet id="1" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p1_init.mp4" media="video_p1_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="112500" d="2500" r="0" />
            <S t="115000" d="5000" r="6" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
  <Period id="testPeriodId3" start="PT60S">
    <AdaptationSet id="1" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p1_init.mp4" media="video_p1_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="150000" d="5000" r="0" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
</MPD>
)";
  std::string periodId1 = "testPeriodId1";
  std::string periodId2 = "testPeriodId2";
  std::string periodId3 = "testPeriodId3";
  ProcessSourceMPD(manifest1);
  // Set curEndNumber to 4, adNextOffset = (4)*2000
  mPrivateCDAIObjectMPD->mPlacementObj = PlacementObj(periodId1, periodId1, 4, 0, 8000, 0, false);

  // Add ads to the adBreak
  mPrivateCDAIObjectMPD->mAdBreaks = {
    {periodId1, AdBreakObject(30000, std::make_shared<std::vector<AdNode>>(), "", 0, 30000)}
  };
  // 1 - to - 2 mapping of ad and adbreak
  mPrivateCDAIObjectMPD->mAdBreaks[periodId1].ads->emplace_back(false, false, true, "adId1", "url1", 10000, periodId1, 0, nullptr);
  // Second ad doesn't have basePeriodId and basePeriodOffset set
  mPrivateCDAIObjectMPD->mAdBreaks[periodId1].ads->emplace_back(false, false, true, "adId2", "url2", 20000, "", -1, nullptr);

  // Add ads to mPeriodMap. mPeriodMap[periodId].adBreakId is non-empty for live at the beginning as per SetAlternateContents
  mPrivateCDAIObjectMPD->mPeriodMap[periodId1] = Period2AdData(false, periodId1, 8000 /*in ms*/,
    {
      std::make_pair (0, AdOnPeriod(0, 0)), // for adId1 idx=0, offset=0s
    });
  mPrivateCDAIObjectMPD->PlaceAds(mAdMPDParseHelper);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].ads->at(0).placed, true);
  EXPECT_FALSE(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].mAdBreakPlaced); // adBreak not placed
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPlacementObj.curAdIdx, 1); // Moved to next ad
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPlacementObj.openPeriodId, periodId1); // open period not changed
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPlacementObj.adNextOffset, 5000); // 5sec from periodId1 also placed
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPlacementObj.adStartOffset, 0); // ad starts from 0sec in periodId1

  ProcessSourceMPD(manifest2);
  mPrivateCDAIObjectMPD->PlaceAds(mAdMPDParseHelper);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].mSplitPeriod, true); // in the PlaceAds call, periodDelta == 0
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].ads->at(0).placed, true);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].ads->at(1).placed, true);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap.size(), 2); // periodId2 map created

  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId1].duration,15000); //periodmap of periodid1 duration
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId1].adBreakId, periodId1);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId1].offset2Ad[0].adIdx, 0);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId1].offset2Ad[0].adStartOffset, 0);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId1].offset2Ad[10000].adIdx, 1);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId1].offset2Ad[10000].adStartOffset, 0);

  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId2].duration,15000); //periodmap of periodid2 duration
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId2].adBreakId, periodId1);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId2].offset2Ad[0].adIdx, 1);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId2].offset2Ad[0].adStartOffset, 5000);

  EXPECT_TRUE(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].mAdBreakPlaced); // adBreak placed
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].endPeriodId, periodId3);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].endPeriodOffset, 0);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].ads->at(0).basePeriodId, "testPeriodId1");
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].ads->at(0).basePeriodOffset, 0);
}

/**
 * @brief Tests the functionality of the PlaceAds method when split period case is true
 * 2 ads of 15 seconds each, in 2 source periods of 15 seconds
 * P1(30s), (SCTE 30s)P2(15s), P3(15s), P4(30s)
 * .........AD1(15s),AD2(15s)          ........
 */
TEST_F(AdManagerMPDTests, PlaceAdsTests_14)
{
  // not adding scte35 markers. These are mocked in PrivateObjectMPD instance
  // testPeriodId1 has 1 new fragments added, here periodDelta==0 is not expected, because once the ad duration is filled, ad is placed
  // testPeriodId2 has 2 new fragments added
  static const char *manifest1 =
R"(<?xml version="1.0" encoding="utf-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" availabilityStartTime="2023-01-01T00:00:00Z" maxSegmentDuration="PT2S" minBufferTime="PT4.000S" minimumUpdatePeriod="P100Y" profiles="urn:dvb:dash:profile:dvb-dash:2014,urn:dvb:dash:profile:dvb-dash:isoff-ext-live:2014" publishTime="2023-01-01T00:01:00Z" timeShiftBufferDepth="PT5M" type="dynamic">
  <Period id="testPeriodId0" start="PT0S">
    <AdaptationSet id="0" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p0_init.mp4" media="video_p0_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="0" d="5000" r="14" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
  <Period id="testPeriodId1" start="PT30S">
    <AdaptationSet id="1" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p1_init.mp4" media="video_p1_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="75000" d="5000" r="6" />
            <S t="110000" d="2500" r="0" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
  <Period id="testPeriodId2" start="PT45S">
    <AdaptationSet id="1" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p1_init.mp4" media="video_p1_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="112500" d="2500" r="0" />
            <S t="115000" d="5000" r="0" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
</MPD>
)";

  // testPeriodId2 has 6 new fragments added
  // testPeriodId3 has 1 new fragments added
  static const char *manifest2 =
R"(<?xml version="1.0" encoding="utf-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" availabilityStartTime="2023-01-01T00:00:00Z" maxSegmentDuration="PT2S" minBufferTime="PT4.000S" minimumUpdatePeriod="P100Y" profiles="urn:dvb:dash:profile:dvb-dash:2014,urn:dvb:dash:profile:dvb-dash:isoff-ext-live:2014" publishTime="2023-01-01T00:01:00Z" timeShiftBufferDepth="PT5M" type="dynamic">
  <Period id="testPeriodId0" start="PT0S">
    <AdaptationSet id="0" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p0_init.mp4" media="video_p0_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="0" d="5000" r="14" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
  <Period id="testPeriodId1" start="PT30S">
    <AdaptationSet id="1" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p1_init.mp4" media="video_p1_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="75000" d="5000" r="6" />
            <S t="110000" d="2500" r="0" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
  <Period id="testPeriodId2" start="PT45S">
    <AdaptationSet id="1" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p1_init.mp4" media="video_p1_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="112500" d="2500" r="0" />
            <S t="115000" d="5000" r="6" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
  <Period id="testPeriodId3" start="PT60S">
    <AdaptationSet id="1" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p1_init.mp4" media="video_p1_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="150000" d="5000" r="0" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
</MPD>
)";
  std::string periodId1 = "testPeriodId1";
  std::string periodId2 = "testPeriodId2";
  std::string periodId3 = "testPeriodId3";
  ProcessSourceMPD(manifest1);
  // Set curEndNumber to 7, adNextOffset = (7)*2000
  mPrivateCDAIObjectMPD->mPlacementObj = PlacementObj(periodId1, periodId1, 7, 0, 14000, 0, false);

  // Add ads to the adBreak
  mPrivateCDAIObjectMPD->mAdBreaks = {
    {periodId1, AdBreakObject(30000, std::make_shared<std::vector<AdNode>>(), "", 0, 30000)}
  };
  // 1 - to - 2 mapping of ad and adbreak
  mPrivateCDAIObjectMPD->mAdBreaks[periodId1].ads->emplace_back(false, false, true, "adId1", "url1", 15000, periodId1, 0, nullptr);
  // Second ad doesn't have basePeriodId and basePeriodOffset set
  mPrivateCDAIObjectMPD->mAdBreaks[periodId1].ads->emplace_back(false, false, true, "adId2", "url2", 15000, "", -1, nullptr);

  // Add ads to mPeriodMap. mPeriodMap[periodId].adBreakId is non-empty for live at the beginning as per SetAlternateContents
  mPrivateCDAIObjectMPD->mPeriodMap[periodId1] = Period2AdData(false, periodId1, 14000 /*in ms*/,
    {
      std::make_pair (0, AdOnPeriod(0, 0)), // for adId1 idx=0, offset=0s
    });
  mPrivateCDAIObjectMPD->PlaceAds(mAdMPDParseHelper);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].ads->at(0).placed, false); // ad1 is not placed, waiting for periodId1 to have periodDelta==0
  EXPECT_FALSE(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].mAdBreakPlaced); // adBreak not placed
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPlacementObj.openPeriodId, periodId1); // open period not changed
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPlacementObj.adNextOffset, 15000); // ad1 is not placed
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPlacementObj.adStartOffset, 0); // ad1 starts from 0sec in periodId1

  ProcessSourceMPD(manifest2);
  mPrivateCDAIObjectMPD->PlaceAds(mAdMPDParseHelper);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].mSplitPeriod, true); // split period identified
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap.size(), 2); // periodId2 map created

  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId1].duration,15000); //periodmap of periodid2 duration
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId1].adBreakId, periodId1);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId1].offset2Ad[0].adIdx, 0);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId1].offset2Ad[0].adStartOffset, 0);

  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].ads->at(0).placed, true); // ad1 is placed
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].ads->at(1).placed, true); // ad2 is placed
  EXPECT_TRUE(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].mAdBreakPlaced); // adBreak placed
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].endPeriodId, periodId3);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].endPeriodOffset, 0);

  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId2].duration,15000); //periodmap of periodid2 duration
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId2].adBreakId, periodId1);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId2].offset2Ad[0].adIdx, 1);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId2].offset2Ad[0].adStartOffset, 0);

  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].ads->at(0).basePeriodId, "testPeriodId1");
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].ads->at(0).basePeriodOffset, 0);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].ads->at(1).basePeriodId, "testPeriodId2");
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].ads->at(1).basePeriodOffset, 0);
}

/**
 * @brief Tests the functionality of the PlaceAds method when split period case is true
 * 1 ad of 20 seconds, in 2 source periods of 15 seconds
 * P1(30s), (SCTE 30s)P2(15s), P3(15s), P4(30s)
 * .........AD1(20s)             ........
 */
TEST_F(AdManagerMPDTests, PlaceAdsTests_15)
{
  // not adding scte35 markers. These are mocked in PrivateObjectMPD instance
  // testPeriodId1 has 0 new fragments added, to simulate periodDelta==0
  // testPeriodId2 has 2 new fragments added
  static const char *manifest1 =
R"(<?xml version="1.0" encoding="utf-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" availabilityStartTime="2023-01-01T00:00:00Z" maxSegmentDuration="PT2S" minBufferTime="PT4.000S" minimumUpdatePeriod="P100Y" profiles="urn:dvb:dash:profile:dvb-dash:2014,urn:dvb:dash:profile:dvb-dash:isoff-ext-live:2014" publishTime="2023-01-01T00:01:00Z" timeShiftBufferDepth="PT5M" type="dynamic">
  <Period id="testPeriodId0" start="PT0S">
    <AdaptationSet id="0" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p0_init.mp4" media="video_p0_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="0" d="5000" r="14" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
  <Period id="testPeriodId1" start="PT30S">
    <AdaptationSet id="1" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p1_init.mp4" media="video_p1_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="75000" d="5000" r="6" />
            <S t="110000" d="2500" r="0" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
  <Period id="testPeriodId2" start="PT45S">
    <AdaptationSet id="1" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p1_init.mp4" media="video_p1_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="112500" d="2500" r="0" />
            <S t="115000" d="5000" r="0" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
</MPD>
)";

  // testPeriodId2 has 6 new fragments added
  // testPeriodId3 has 1 new fragments added
  static const char *manifest2 =
R"(<?xml version="1.0" encoding="utf-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" availabilityStartTime="2023-01-01T00:00:00Z" maxSegmentDuration="PT2S" minBufferTime="PT4.000S" minimumUpdatePeriod="P100Y" profiles="urn:dvb:dash:profile:dvb-dash:2014,urn:dvb:dash:profile:dvb-dash:isoff-ext-live:2014" publishTime="2023-01-01T00:01:00Z" timeShiftBufferDepth="PT5M" type="dynamic">
  <Period id="testPeriodId0" start="PT0S">
    <AdaptationSet id="0" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p0_init.mp4" media="video_p0_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="0" d="5000" r="14" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
  <Period id="testPeriodId1" start="PT30S">
    <AdaptationSet id="1" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p1_init.mp4" media="video_p1_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="75000" d="5000" r="6" />
            <S t="110000" d="2500" r="0" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
  <Period id="testPeriodId2" start="PT45S">
    <AdaptationSet id="1" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p1_init.mp4" media="video_p1_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="112500" d="2500" r="0" />
            <S t="115000" d="5000" r="6" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
  <Period id="testPeriodId3" start="PT60S">
    <AdaptationSet id="1" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p1_init.mp4" media="video_p1_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="150000" d="5000" r="0" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
</MPD>
)";
  std::string periodId1 = "testPeriodId1";
  std::string periodId2 = "testPeriodId2";
  std::string periodId3 = "testPeriodId3";
  ProcessSourceMPD(manifest1);
  // Set curEndNumber to 8, adNextOffset = (7)*2000 + (1)*1000
  mPrivateCDAIObjectMPD->mPlacementObj = PlacementObj(periodId1, periodId1, 8, 0, 15000, 0, false);

  // Add ads to the adBreak
  // periodId1 has received ads of 20s for 30s break
  mPrivateCDAIObjectMPD->mAdBreaks = {
    {periodId1, AdBreakObject(30000, std::make_shared<std::vector<AdNode>>(), "", 0, 20000)}
  };
  // 1 - to - 2 mapping of ad and adbreak
  mPrivateCDAIObjectMPD->mAdBreaks[periodId1].ads->emplace_back(false, false, true, "adId1", "url1", 20000, periodId1, 0, nullptr);

  // Add ads to mPeriodMap. mPeriodMap[periodId].adBreakId is non-empty for live at the beginning as per SetAlternateContents
  mPrivateCDAIObjectMPD->mPeriodMap[periodId1] = Period2AdData(false, periodId1, 15000 /*in ms*/,
    {
      std::make_pair (0, AdOnPeriod(0, 0)), // for adId1 idx=0, offset=0s
    });
  mPrivateCDAIObjectMPD->PlaceAds(mAdMPDParseHelper);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].mSplitPeriod, true);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].ads->at(0).placed, false);
  EXPECT_FALSE(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].mAdBreakPlaced); // adBreak not placed
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap.size(), 2); // periodId2 map created

  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId1].duration,15000); //periodmap of periodid2 duration
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId1].adBreakId, periodId1);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId1].offset2Ad[0].adIdx, 0);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId1].offset2Ad[0].adStartOffset, 0);

  //periodmap of periodid2 duration
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId2].adBreakId, periodId1);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId2].offset2Ad[0].adIdx, 0);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId2].offset2Ad[0].adStartOffset, 15000);

  EXPECT_EQ(mPrivateCDAIObjectMPD->mPlacementObj.openPeriodId, periodId2); // open period changed to periodId2
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPlacementObj.adNextOffset, 18000); // 3sec from periodId2 also placed
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPlacementObj.adStartOffset, 15000); // ad starts from 15sec in periodId2

  ProcessSourceMPD(manifest2);
  mPrivateCDAIObjectMPD->PlaceAds(mAdMPDParseHelper);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].ads->at(0).placed, true);
  EXPECT_TRUE(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].mAdBreakPlaced); // adBreak placed
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].endPeriodId, periodId2);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].endPeriodOffset, 5000);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].ads->at(0).basePeriodId, "testPeriodId1");
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].ads->at(0).basePeriodOffset, 0);
}

/**
 * @brief Tests the functionality of the PlaceAds method when split period case is true
 * 1 ad of 30 seconds, in 3 source periods of 10 seconds each
 * P1(30s), (SCTE 30s)P2(10s), P3(10s), P4(10s), P5(30s)
 * .........AD1(30s),                          ........
 */
TEST_F(AdManagerMPDTests, PlaceAdsTests_16)
{
  // not adding scte35 markers. These are mocked in PrivateObjectMPD instance
  // testPeriodId1 has 1 new fragments added
  // testPeriodId2 has 2 new fragments added
  static const char *manifest1 =
R"(<?xml version="1.0" encoding="utf-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" availabilityStartTime="2023-01-01T00:00:00Z" maxSegmentDuration="PT2S" minBufferTime="PT4.000S" minimumUpdatePeriod="P100Y" profiles="urn:dvb:dash:profile:dvb-dash:2014,urn:dvb:dash:profile:dvb-dash:isoff-ext-live:2014" publishTime="2023-01-01T00:01:00Z" timeShiftBufferDepth="PT5M" type="dynamic">
  <Period id="testPeriodId0" start="PT0S">
    <AdaptationSet id="0" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p0_init.mp4" media="video_p0_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="0" d="5000" r="14" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
  <Period id="testPeriodId1" start="PT30S">
    <AdaptationSet id="1" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p1_init.mp4" media="video_p1_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="75000" d="5000" r="4" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
  <Period id="testPeriodId2" start="PT40S">
    <AdaptationSet id="1" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p1_init.mp4" media="video_p1_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="100000" d="5000" r="0" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
</MPD>
)";

  // testPeriodId1 has 0 new fragments added
  // testPeriodId2 has 4 new fragments added
  // testPeriodId3 has 1 new fragments added
  static const char *manifest2 =
R"(<?xml version="1.0" encoding="utf-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" availabilityStartTime="2023-01-01T00:00:00Z" maxSegmentDuration="PT2S" minBufferTime="PT4.000S" minimumUpdatePeriod="P100Y" profiles="urn:dvb:dash:profile:dvb-dash:2014,urn:dvb:dash:profile:dvb-dash:isoff-ext-live:2014" publishTime="2023-01-01T00:01:00Z" timeShiftBufferDepth="PT5M" type="dynamic">
  <Period id="testPeriodId0" start="PT0S">
    <AdaptationSet id="0" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p0_init.mp4" media="video_p0_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="0" d="5000" r="14" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
  <Period id="testPeriodId1" start="PT30S">
    <AdaptationSet id="1" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p1_init.mp4" media="video_p1_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="75000" d="5000" r="4" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
  <Period id="testPeriodId2" start="PT40S">
    <AdaptationSet id="1" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p1_init.mp4" media="video_p1_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="100000" d="5000" r="4" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
  <Period id="testPeriodId3" start="PT50S">
    <AdaptationSet id="1" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p1_init.mp4" media="video_p1_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="125000" d="5000" r="0" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
</MPD>
)";

  // testPeriodId1 has 0 new fragments added
  // testPeriodId2 has 0 new fragments added
  // testPeriodId3 has 4 new fragments added
  // testPeriodId4 has 1 new fragments added
  static const char *manifest3 =
R"(<?xml version="1.0" encoding="utf-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" availabilityStartTime="2023-01-01T00:00:00Z" maxSegmentDuration="PT2S" minBufferTime="PT4.000S" minimumUpdatePeriod="P100Y" profiles="urn:dvb:dash:profile:dvb-dash:2014,urn:dvb:dash:profile:dvb-dash:isoff-ext-live:2014" publishTime="2023-01-01T00:01:00Z" timeShiftBufferDepth="PT5M" type="dynamic">
  <Period id="testPeriodId0" start="PT0S">
    <AdaptationSet id="0" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p0_init.mp4" media="video_p0_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="0" d="5000" r="14" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
  <Period id="testPeriodId1" start="PT30S">
    <AdaptationSet id="1" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p1_init.mp4" media="video_p1_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="75000" d="5000" r="4" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
  <Period id="testPeriodId2" start="PT40S">
    <AdaptationSet id="1" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p1_init.mp4" media="video_p1_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="100000" d="5000" r="4" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
  <Period id="testPeriodId3" start="PT50S">
    <AdaptationSet id="1" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p1_init.mp4" media="video_p1_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="125000" d="5000" r="4" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
  <Period id="testPeriodId4" start="PT60S">
    <AdaptationSet id="1" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p1_init.mp4" media="video_p1_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="150000" d="5000" r="0" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
</MPD>
)";

  std::string periodId1 = "testPeriodId1";
  std::string periodId2 = "testPeriodId2";
  std::string periodId3 = "testPeriodId3";
  std::string periodId4 = "testPeriodId4";
  ProcessSourceMPD(manifest1);
  // Set curEndNumber to 4, adNextOffset = (4)*2000
  mPrivateCDAIObjectMPD->mPlacementObj = PlacementObj(periodId1, periodId1, 4, 0, 8000, 0, false);

  // Add ads to the adBreak
  mPrivateCDAIObjectMPD->mAdBreaks = {
    {periodId1, AdBreakObject(30000, std::make_shared<std::vector<AdNode>>(), "", 0, 30000)}
  };
  // 1 - to - 2 mapping of ad and adbreak
  mPrivateCDAIObjectMPD->mAdBreaks[periodId1].ads->emplace_back(false, false, true, "adId1", "url1", 30000, periodId1, 0, nullptr);

  // Add ads to mPeriodMap. mPeriodMap[periodId].adBreakId is non-empty for live at the beginning as per SetAlternateContents
  mPrivateCDAIObjectMPD->mPeriodMap[periodId1] = Period2AdData(false, periodId1, 8000 /*in ms*/,
    {
      std::make_pair (0, AdOnPeriod(0, 0)), // for adId1 idx=0, offset=0s
    });
  mPrivateCDAIObjectMPD->PlaceAds(mAdMPDParseHelper);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].ads->at(0).placed, false);
  EXPECT_FALSE(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].mAdBreakPlaced); // adBreak not placed
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPlacementObj.openPeriodId, periodId1); // open period remains at periodId1
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPlacementObj.adNextOffset, 10000); // 10sec from periodId1 placed
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPlacementObj.adStartOffset, 0); // ad starts from 0sec in periodId1
  // New periodMap is not created as periodDelta != 0.

  ProcessSourceMPD(manifest2);
  mPrivateCDAIObjectMPD->PlaceAds(mAdMPDParseHelper);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].mSplitPeriod, true); // split period is identified only once periodDelta becomes zero
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap.size(), 2); // periodId2 map created

  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId1].duration,10000); //periodmap of periodid1
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId1].adBreakId, periodId1);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId1].offset2Ad[0].adIdx, 0);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId1].offset2Ad[0].adStartOffset, 0);

  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId2].duration,10000); //periodmap of periodid2
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId2].adBreakId, periodId1);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId2].offset2Ad[0].adIdx, 0);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId2].offset2Ad[0].adStartOffset, 10000);

  EXPECT_EQ(mPrivateCDAIObjectMPD->mPlacementObj.openPeriodId, periodId2); // open period changed to periodId2
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPlacementObj.adNextOffset, 20000); // 10sec from periodId2 also placed
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPlacementObj.adStartOffset, 10000); // ad starts from 10sec in periodId2

  ProcessSourceMPD(manifest3);
  mPrivateCDAIObjectMPD->PlaceAds(mAdMPDParseHelper);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap.size(), 3); // periodId3 map created

  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId3].duration,10000); //periodmap of periodid3
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId3].adBreakId, periodId1);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId3].offset2Ad[0].adIdx, 0);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId3].offset2Ad[0].adStartOffset, 20000);

  //mPlacementObj is reset after placing ads, no EXPECT calls for same
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].ads->at(0).placed, true);
  EXPECT_TRUE(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].mAdBreakPlaced); // adBreak placed
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].endPeriodId, periodId4);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].endPeriodOffset, 0);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].ads->at(0).basePeriodId, "testPeriodId1");
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].ads->at(0).basePeriodOffset, 0);

}

/**
 * @brief Tests the functionality of the PlaceAds method when adDuration is less than source period duration by 1sec
 */
TEST_F(AdManagerMPDTests, PlaceAdsTests_17)
{
  // not adding scte35 markers. These are mocked in PrivateObjectMPD instance
  // testPeriodId1 has 2 new fragments added
  // testPeriodId2 has 2 new fragments added
  static const char *manifest =
R"(<?xml version="1.0" encoding="utf-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" availabilityStartTime="2023-01-01T00:00:00Z" maxSegmentDuration="PT2S" minBufferTime="PT4.000S" minimumUpdatePeriod="P100Y" profiles="urn:dvb:dash:profile:dvb-dash:2014,urn:dvb:dash:profile:dvb-dash:isoff-ext-live:2014" publishTime="2023-01-01T00:01:00Z" timeShiftBufferDepth="PT5M" type="dynamic">
  <Period id="testPeriodId0" start="PT0S">
    <AdaptationSet id="0" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p0_init.mp4" media="video_p0_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="0" d="5000" r="14" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
  <Period id="testPeriodId1" start="PT30S">
    <AdaptationSet id="1" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p1_init.mp4" media="video_p1_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="0" d="5000" r="14" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
  <Period id="testPeriodId2" start="PT60S">
    <AdaptationSet id="1" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p1_init.mp4" media="video_p1_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="0" d="5000" r="1" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
</MPD>
)";
  std::string periodId1 = "testPeriodId1";
  std::string periodId2 = "testPeriodId2";
  ProcessSourceMPD(manifest);
  // Set curEndNumber to 13, adNextOffset = (13)*2000
  mPrivateCDAIObjectMPD->mPlacementObj = PlacementObj(periodId1, periodId1, 13, 0, 26000, 0, false);
  mPrivateCDAIObjectMPD->mAdtoInsertInNextBreakVec.emplace_back(periodId2, periodId2, 0, 0, 0, 0, false); // second ad break in vector

  // Add ads to the adBreak
  // testPeriodId1 ad duration is set to 29000 to force mismatch for sourceAdDurationMismatch
  // testPeriodId2 ad duration is set to 30000
  mPrivateCDAIObjectMPD->mAdBreaks = {
    {periodId1, AdBreakObject(29000, std::make_shared<std::vector<AdNode>>(), "", 0, 29000)},
    {periodId2, AdBreakObject(30000, std::make_shared<std::vector<AdNode>>(), "", 0, 30000)}
  };
  // 1 - to - 1 mapping of ad and period
  mPrivateCDAIObjectMPD->mAdBreaks[periodId1].ads->emplace_back(false, false, true, "adId1", "url1", 29000, periodId1, 0, nullptr);
  mPrivateCDAIObjectMPD->mAdBreaks[periodId2].ads->emplace_back(false, false, true, "adId2", "url2", 30000, periodId2, 0, nullptr);

  // Add ads to mPeriodMap. mPeriodMap[periodId].adBreakId is non-empty for live at the beginning as per SetAlternateContents
  mPrivateCDAIObjectMPD->mPeriodMap[periodId1] = Period2AdData(false, periodId1, 26000 /*in ms*/,
    {
      std::make_pair (0, AdOnPeriod(0, 0)), // for adId1 idx=0, offset=0s
    });
  mPrivateCDAIObjectMPD->mPeriodMap[periodId2] = Period2AdData(false, periodId2, 0 /*in ms*/,
    {
      std::make_pair (0, AdOnPeriod(0, 0)), // for adId1 idx=0, offset=0s
    });
  mPrivateCDAIObjectMPD->PlaceAds(mAdMPDParseHelper);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].ads->at(0).placed, true);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].endPeriodOffset, 0); // makes sure we are clamped to period boundary
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].endPeriodId, "testPeriodId2"); // next period
  //periodmap of periodid1 duration
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId1].adBreakId, periodId1);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId1].offset2Ad[0].adIdx,0);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId1].offset2Ad[0].adStartOffset, 0);

  EXPECT_TRUE(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].mAdBreakPlaced); // adBreak placed
  EXPECT_FALSE(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].mSplitPeriod); // should not be marked as split period

  EXPECT_EQ(mPrivateCDAIObjectMPD->mPlacementObj.curAdIdx, 0);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPlacementObj.pendingAdbrkId, periodId2);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId1].duration, 30000); // in ms
  // periodId2 is not updated in the current logic
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId2].duration, 0); // in ms
  //periodmap of periodid2 duration
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId2].adBreakId, periodId2);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId2].offset2Ad[0].adIdx, 0);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId2].offset2Ad[0].adStartOffset, 0);
}

/**
 * @brief Tests the functionality of the PlaceAds method when split period case is true
 * This special case ensures that with AD1 exceeding source duration by 1sec, is treated as a split period scenario
 * 2 ads of 15.5 seconds each, in 2 source periods of 15 seconds
 * P1(30s), (SCTE 30s)P2(15s), P3(15s), P4(30s)
 * .........AD1(15.5s),AD2(15.5s)      ........
 */
TEST_F(AdManagerMPDTests, PlaceAdsTests_18)
{
  // not adding scte35 markers. These are mocked in PrivateObjectMPD instance
  // testPeriodId1 has 0 new fragments added, to simulate periodDelta==0
  // testPeriodId2 has 2 new fragments added
  static const char *manifest1 =
R"(<?xml version="1.0" encoding="utf-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" availabilityStartTime="2023-01-01T00:00:00Z" maxSegmentDuration="PT2S" minBufferTime="PT4.000S" minimumUpdatePeriod="P100Y" profiles="urn:dvb:dash:profile:dvb-dash:2014,urn:dvb:dash:profile:dvb-dash:isoff-ext-live:2014" publishTime="2023-01-01T00:01:00Z" timeShiftBufferDepth="PT5M" type="dynamic">
  <Period id="testPeriodId0" start="PT0S">
    <AdaptationSet id="0" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p0_init.mp4" media="video_p0_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="0" d="5000" r="14" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
  <Period id="testPeriodId1" start="PT30S">
    <AdaptationSet id="1" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p1_init.mp4" media="video_p1_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="75000" d="5000" r="6" />
            <S t="110000" d="2500" r="0" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
  <Period id="testPeriodId2" start="PT45S">
    <AdaptationSet id="1" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p1_init.mp4" media="video_p1_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="112500" d="2500" r="0" />
            <S t="115000" d="5000" r="0" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
</MPD>
)";

  // testPeriodId2 has 6 new fragments added
  // testPeriodId3 has 1 new fragments added
  static const char *manifest2 =
R"(<?xml version="1.0" encoding="utf-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" availabilityStartTime="2023-01-01T00:00:00Z" maxSegmentDuration="PT2S" minBufferTime="PT4.000S" minimumUpdatePeriod="P100Y" profiles="urn:dvb:dash:profile:dvb-dash:2014,urn:dvb:dash:profile:dvb-dash:isoff-ext-live:2014" publishTime="2023-01-01T00:01:00Z" timeShiftBufferDepth="PT5M" type="dynamic">
  <Period id="testPeriodId0" start="PT0S">
    <AdaptationSet id="0" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p0_init.mp4" media="video_p0_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="0" d="5000" r="14" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
  <Period id="testPeriodId1" start="PT30S">
    <AdaptationSet id="1" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p1_init.mp4" media="video_p1_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="75000" d="5000" r="6" />
            <S t="110000" d="2500" r="0" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
  <Period id="testPeriodId2" start="PT45S">
    <AdaptationSet id="1" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p1_init.mp4" media="video_p1_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="112500" d="2500" r="0" />
            <S t="115000" d="5000" r="6" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
  <Period id="testPeriodId3" start="PT60S">
    <AdaptationSet id="1" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p1_init.mp4" media="video_p1_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="150000" d="5000" r="0" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
</MPD>
)";

  // testPeriodId2 has 0 new fragments added
  // testPeriodId3 has 2 new fragments added
  static const char *manifest3 =
R"(<?xml version="1.0" encoding="utf-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" availabilityStartTime="2023-01-01T00:00:00Z" maxSegmentDuration="PT2S" minBufferTime="PT4.000S" minimumUpdatePeriod="P100Y" profiles="urn:dvb:dash:profile:dvb-dash:2014,urn:dvb:dash:profile:dvb-dash:isoff-ext-live:2014" publishTime="2023-01-01T00:01:00Z" timeShiftBufferDepth="PT5M" type="dynamic">
  <Period id="testPeriodId0" start="PT0S">
    <AdaptationSet id="0" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p0_init.mp4" media="video_p0_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="0" d="5000" r="14" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
  <Period id="testPeriodId1" start="PT30S">
    <AdaptationSet id="1" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p1_init.mp4" media="video_p1_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="75000" d="5000" r="6" />
            <S t="110000" d="2500" r="0" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
  <Period id="testPeriodId2" start="PT45S">
    <AdaptationSet id="1" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p1_init.mp4" media="video_p1_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="112500" d="2500" r="0" />
            <S t="115000" d="5000" r="6" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
  <Period id="testPeriodId3" start="PT60S">
    <AdaptationSet id="1" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p1_init.mp4" media="video_p1_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="150000" d="5000" r="2" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
</MPD>
)";
  std::string periodId1 = "testPeriodId1";
  std::string periodId2 = "testPeriodId2";
  std::string periodId3 = "testPeriodId3";
  ProcessSourceMPD(manifest1);
  // Set curEndNumber to 8, adNextOffset = (7)*2000 + (1)*1000
  mPrivateCDAIObjectMPD->mPlacementObj = PlacementObj(periodId1, periodId1, 8, 0, 15000, 0, false);

  // Add ads to the adBreak
  // testPeriodId1 ad duration is set to 26000
  mPrivateCDAIObjectMPD->mAdBreaks = {
    {periodId1, AdBreakObject(30000, std::make_shared<std::vector<AdNode>>(), "", 0, 31000)}
  };
  // 1 - to - 2 mapping of ad and adbreak
  mPrivateCDAIObjectMPD->mAdBreaks[periodId1].ads->emplace_back(false, false, true, "adId1", "url1", 15500, periodId1, 0, nullptr);
  mPrivateCDAIObjectMPD->mAdBreaks[periodId1].ads->emplace_back(false, false, true, "adId2", "url2", 15500, "", -1, nullptr);

  // Add ads to mPeriodMap. mPeriodMap[periodId].adBreakId is non-empty for live at the beginning as per SetAlternateContents
  mPrivateCDAIObjectMPD->mPeriodMap[periodId1] = Period2AdData(false, periodId1, 15000 /*in ms*/,
    {
      std::make_pair (0, AdOnPeriod(0, 0)), // for adId1 idx=0, offset=0s
    });
  mPrivateCDAIObjectMPD->PlaceAds(mAdMPDParseHelper);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].mSplitPeriod, true);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].ads->at(0).placed, true); // since periodDelta==0, it advances to periodId2
  EXPECT_FALSE(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].mAdBreakPlaced); // adBreak not placed
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap.size(), 2); // periodId2 map created

  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId1].duration,15000); //periodmap of periodid1 duration
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId1].adBreakId, periodId1);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId1].offset2Ad[0].adIdx, 0);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId1].offset2Ad[0].adStartOffset, 0);

  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId2].adBreakId, periodId1);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId2].duration, 3000);

  EXPECT_EQ(mPrivateCDAIObjectMPD->mPlacementObj.curAdIdx, 1); // moved to second ad
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPlacementObj.pendingAdbrkId, periodId1);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPlacementObj.openPeriodId, periodId2);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPlacementObj.adStartOffset, 0); // ad starts from 0sec in periodId2
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPlacementObj.adNextOffset, 2500); // 0.5sec from periodId2 placed

  ProcessSourceMPD(manifest2);
  mPrivateCDAIObjectMPD->PlaceAds(mAdMPDParseHelper);
  // periodmap of periodid2 duration
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId2].duration, 15000);
  // The ad2 starts at offset 1000 in periodId2. This is less than OFFSET_ALIGN_FACTOR and hence get adjusted to 0
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId2].offset2Ad[0].adIdx, 1);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId2].offset2Ad[0].adStartOffset, 0);

  EXPECT_EQ(mPrivateCDAIObjectMPD->mPlacementObj.adStartOffset, 0); // ad starts from 0sec in periodId2
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPlacementObj.adNextOffset, 14500); // 0.5sec from periodId2 placed for ad1

  // ad2 gets placed now in periodId2 and ends at periodId3
  ProcessSourceMPD(manifest3);
  mPrivateCDAIObjectMPD->PlaceAds(mAdMPDParseHelper);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].ads->at(1).placed, true);
  EXPECT_TRUE(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].mAdBreakPlaced); // adBreak placed
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].endPeriodId, periodId3);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].endPeriodOffset, 0);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].ads->at(0).basePeriodId, "testPeriodId1");
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].ads->at(0).basePeriodOffset, 0);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].ads->at(1).basePeriodId, "testPeriodId2");
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].ads->at(1).basePeriodOffset, 500);

}


/**
 * @brief Tests the functionality of the PlaceAds method when a tiny period is having ads
 * Tiny periods are ignored in AAMP, so the ads associated with it should be ignored as well
 * 1 ad of 30 seconds in a tiny period, following by 1 ad of 30s in a normal period
 * P1(30s), (SCTE 30s)P2(200ms), (SCTE 30s)P3(30s), P4(30s)
 * .........AD1(30s),............AD2(30s)          ........
 */
TEST_F(AdManagerMPDTests, PlaceAdsTests_19)
{
  // not adding scte35 markers. These are mocked in PrivateObjectMPD instance
  // testPeriodId1 has 1 new fragments added of 200ms
  // testPeriodId2 has 1 new fragments added
  static const char *manifest1 =
R"(<?xml version="1.0" encoding="utf-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" availabilityStartTime="2023-01-01T00:00:00Z" maxSegmentDuration="PT2S" minBufferTime="PT4.000S" minimumUpdatePeriod="P100Y" profiles="urn:dvb:dash:profile:dvb-dash:2014,urn:dvb:dash:profile:dvb-dash:isoff-ext-live:2014" publishTime="2023-01-01T00:01:00Z" timeShiftBufferDepth="PT5M" type="dynamic">
  <Period id="testPeriodId0" start="PT0S">
    <AdaptationSet id="0" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p0_init.mp4" media="video_p0_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="0" d="5000" r="14" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
  <Period id="testPeriodId1" start="PT30S">
    <AdaptationSet id="1" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p1_init.mp4" media="video_p1_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="75000" d="500" r="0" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
  <Period id="testPeriodId2" start="PT30.200S">
    <AdaptationSet id="1" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p1_init.mp4" media="video_p1_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="75500" d="5000" r="0" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
</MPD>
)";

  // testPeriodId2 has 14 new fragments added
  // testPeriodId3 has 1 new fragments added
  static const char *manifest2 =
R"(<?xml version="1.0" encoding="utf-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" availabilityStartTime="2023-01-01T00:00:00Z" maxSegmentDuration="PT2S" minBufferTime="PT4.000S" minimumUpdatePeriod="P100Y" profiles="urn:dvb:dash:profile:dvb-dash:2014,urn:dvb:dash:profile:dvb-dash:isoff-ext-live:2014" publishTime="2023-01-01T00:01:00Z" timeShiftBufferDepth="PT5M" type="dynamic">
  <Period id="testPeriodId0" start="PT0S">
    <AdaptationSet id="0" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p0_init.mp4" media="video_p0_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="0" d="5000" r="14" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
  <Period id="testPeriodId1" start="PT30S">
    <AdaptationSet id="1" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p1_init.mp4" media="video_p1_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="75000" d="500" r="0" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
  <Period id="testPeriodId2" start="PT30.200S">
    <AdaptationSet id="1" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p1_init.mp4" media="video_p1_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="75500" d="5000" r="14" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
  <Period id="testPeriodId3" start="PT60.200S">
    <AdaptationSet id="1" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p1_init.mp4" media="video_p1_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="150500" d="5000" r="0" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
</MPD>
)";
  std::string periodId1 = "testPeriodId1";
  std::string periodId2 = "testPeriodId2";
  std::string periodId3 = "testPeriodId3";
  ProcessSourceMPD(manifest1);
  // Add entries for periodId1 and periodId2 in placement
  mPrivateCDAIObjectMPD->mPlacementObj = PlacementObj(periodId1, periodId1, 0, 0, 0, 0, false);
  mPrivateCDAIObjectMPD->mAdtoInsertInNextBreakVec.emplace_back(periodId2, periodId2, 0, 0, 0, 0, false);

  // Add ads to the adBreak
  // testPeriodId1 ad duration is set to 30000
  // testPeriodId2 ad duration is set to 30000
  mPrivateCDAIObjectMPD->mAdBreaks = {
    {periodId1, AdBreakObject(30000, std::make_shared<std::vector<AdNode>>(), "", 0, 30000)},
    {periodId2, AdBreakObject(30000, std::make_shared<std::vector<AdNode>>(), "", 0, 30000)}
  };
  // 1 - to - 1 mapping of ad and adbreak for testPeriodId1 and testPeriodId2
  mPrivateCDAIObjectMPD->mAdBreaks[periodId1].ads->emplace_back(false, false, true, "adId1", "url1", 20000, periodId1, 0, nullptr);
  mPrivateCDAIObjectMPD->mAdBreaks[periodId1].ads->emplace_back(false, false, true, "adId1", "url1", 10000, "", -1, nullptr);
  mPrivateCDAIObjectMPD->mAdBreaks[periodId2].ads->emplace_back(false, false, true, "adId2", "url2", 30000, periodId2, 0, nullptr);

  // Add entries to mPeriodMap
  mPrivateCDAIObjectMPD->mPeriodMap[periodId1] = Period2AdData(false, periodId1, 0 /*in ms*/,
    {
      std::make_pair (0, AdOnPeriod(0, 0)), // for adId1 idx=0, offset=0s
    });

  mPrivateCDAIObjectMPD->mPeriodMap[periodId1] = Period2AdData(false, periodId2, 0 /*in ms*/,
    {
      std::make_pair (0, AdOnPeriod(0, 0)), // for adId1 idx=0, offset=0s
    });
  mPrivateCDAIObjectMPD->PlaceAds(mAdMPDParseHelper);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPlacementObj.adNextOffset, 200);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId1].duration, 200);

  ProcessSourceMPD(manifest2);
  mPrivateCDAIObjectMPD->PlaceAds(mAdMPDParseHelper); // only in this placeAds call, periodId1 has a periodDelta==0 and places the break
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].mSplitPeriod, false);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].ads->at(0).placed, true); //ad is placed
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].ads->at(0).invalid, true); //invalid since its tiny period
  EXPECT_TRUE(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].mAdBreakPlaced); //adbreak is placed
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].ads->at(0).basePeriodId, "testPeriodId1");
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].ads->at(0).basePeriodOffset, 0);
  // for 2nd ad in break
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].ads->at(1).placed, true); //ad is placed
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].ads->at(1).invalid, true); //invalid since its tiny period
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].ads->at(1).basePeriodId, ""); // second ad in break is not set
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].ads->at(1).basePeriodOffset, -1); // second ad in break is not placed

  // periodmap is reset as endPeriodOffset is less than 2*OFFSET_ALIGN_FACTOR
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId1].duration, 0); //periodmap of periodid2 duration
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId1].adBreakId, ""); // periodmap is reset
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId1].offset2Ad.empty(), true);

  EXPECT_EQ(mPrivateCDAIObjectMPD->mPlacementObj.curAdIdx, 0); // moved to second period
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPlacementObj.pendingAdbrkId, periodId2);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPlacementObj.openPeriodId, periodId2);

  // Call with the same manifest again to place ads in testPeriodId2
  mPrivateCDAIObjectMPD->PlaceAds(mAdMPDParseHelper);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId2].ads->at(0).placed, true); // second ad placed
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId2].ads->at(0).invalid, false); // second ad placed
  EXPECT_TRUE(mPrivateCDAIObjectMPD->mAdBreaks[periodId2].mAdBreakPlaced); // adBreak placed
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId2].endPeriodId, periodId3);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId2].endPeriodOffset, 0);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId2].ads->at(0).basePeriodId, "testPeriodId2");
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId2].ads->at(0).basePeriodOffset, 0);

  //periodmap of periodid2 duration
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId2].adBreakId, periodId2);
  // The ad2 starts at offset 0 in periodId2.
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId2].offset2Ad[0].adIdx, 0);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId2].offset2Ad[0].adStartOffset, 0);
}


/**
 * @brief Tests the functionality of the PlaceAds method when openPeriodID is currently getting filled
 * and an empty period appears at the upper boundary, followed by a valid period. This is a special case scenario.
 * Here, the source period duration < ad duration and it should be clearly updated as such.
 * The endPeriod should be updated to the valid period following empty period
 *  * (SCTE 30s)P1(30s), P2(0), P3(30s)
 * ... AD1(31s)                   ....................
 */
TEST_F(AdManagerMPDTests, PlaceAdsTests_20)
{
  // src period = 30sec, ad for 30 sec
  // not adding scte35 markers. These are mocked in PrivateObjectMPD instance
  static const char *manifest1 =
R"(<?xml version="1.0" encoding="utf-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" availabilityStartTime="2023-01-01T00:00:00Z" maxSegmentDuration="PT2S" minBufferTime="PT4.000S" minimumUpdatePeriod="P100Y" profiles="urn:dvb:dash:profile:dvb-dash:2014,urn:dvb:dash:profile:dvb-dash:isoff-ext-live:2014" publishTime="2023-01-01T00:01:00Z" timeShiftBufferDepth="PT5M" type="dynamic">
  <Period id="testPeriodId0" start="PT0S">
    <AdaptationSet id="0" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p0_init.mp4" media="video_p0_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="0" d="5000" r="14" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
  <Period id="testPeriodId1" start="PT30S">
  </Period>
</MPD>
)";

  static const char *manifest2 =
R"(<?xml version="1.0" encoding="utf-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" availabilityStartTime="2023-01-01T00:00:00Z" maxSegmentDuration="PT2S" minBufferTime="PT4.000S" minimumUpdatePeriod="P100Y" profiles="urn:dvb:dash:profile:dvb-dash:2014,urn:dvb:dash:profile:dvb-dash:isoff-ext-live:2014" publishTime="2023-01-01T00:01:00Z" timeShiftBufferDepth="PT5M" type="dynamic">
  <Period id="testPeriodId0" start="PT0S">
    <AdaptationSet id="0" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p0_init.mp4" media="video_p0_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="0" d="5000" r="14" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
  <Period id="testPeriodId1" start="PT30S">
  </Period>
  <Period id="testPeriodId2" start="PT30S">
    <AdaptationSet id="0" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p0_init.mp4" media="video_p0_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="75000" d="5000" r="9" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
</MPD>
)";

  std::string periodId = "testPeriodId0";
  std::string endPeriodId = "testPeriodId2";
  // testPeriodId1 has 3 fragments added in the mock
  ProcessSourceMPD(manifest1);
  // Set curEndNumber to 12, adNextOffset = (12)*2000
  mPrivateCDAIObjectMPD->mPlacementObj = PlacementObj(periodId, periodId, 0, 0, 0, 0, false);
  mPrivateCDAIObjectMPD->mAdtoInsertInNextBreakVec.push_back(mPrivateCDAIObjectMPD->mPlacementObj);
  mPrivateCDAIObjectMPD->mPlacementObj.curEndNumber = 12;
  mPrivateCDAIObjectMPD->mPlacementObj.adNextOffset = 24000;

  // Add ads to the adBreak
  mPrivateCDAIObjectMPD->mAdBreaks = {
    {periodId, AdBreakObject(31000, std::make_shared<std::vector<AdNode>>(), "", 0, 31000)}
  };
  mPrivateCDAIObjectMPD->mAdBreaks[periodId].ads->emplace_back(false, false, true, "adId1", "url", 31000, periodId, 0, nullptr);

  // Add ads to mPeriodMap. mPeriodMap[periodId].adBreakId is non-empty for live at the beginning as per SetAlternateContents
  mPrivateCDAIObjectMPD->mPeriodMap[periodId] = Period2AdData(false, periodId, 24000 /*in ms*/,
    {
      std::make_pair (0, AdOnPeriod(0, 0)), // for adId1 idx=0, offset=0s
    });
  mPrivateCDAIObjectMPD->PlaceAds(mAdMPDParseHelper);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPlacementObj.curEndNumber, 15);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPlacementObj.pendingAdbrkId, periodId);
  // Ad is not placed in first iteration. Only when next non-zero period is visible ad is placed
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPlacementObj.curAdIdx, 0);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId].duration, 30000); // in ms
  EXPECT_FALSE(mPrivateCDAIObjectMPD->mAdBreaks[periodId].mAdBreakPlaced); // adBreak not placed

  // Update with the next mpd, where periodId1 is empty and periodId2 is valid
  ProcessSourceMPD(manifest2);
  mPrivateCDAIObjectMPD->PlaceAds(mAdMPDParseHelper);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId].endPeriodId, endPeriodId); // placement completed and ending in testPeriodId2
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId].endPeriodOffset, 0); // placement completed and offset updated
  EXPECT_TRUE(mPrivateCDAIObjectMPD->mAdBreaks[periodId].mAdBreakPlaced); // adBreak placed
  // The current adbreak is filled and placementObj should now be reset
  EXPECT_NE(mPrivateCDAIObjectMPD->mPlacementObj.pendingAdbrkId, periodId);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPlacementObj.curAdIdx, -1);
  // This is because ad is placed in previous iteration with adjustEndPeriodOffset set to true. This needs to be revisited
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId].duration, 30000); // in ms
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId].adBreakId, periodId);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId].offset2Ad[0].adIdx, 0);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId].offset2Ad[0].adStartOffset, 0);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId].ads->at(0).basePeriodId,periodId);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId].ads->at(0).basePeriodOffset, 0);
}


/**
 * @brief Tests the functionality of the PlaceAds method when split period case is true
 * 2 ads of 15 seconds each, in 3 source periods with the middle one being an empty period
 * P1(30s), (SCTE 30s)P2(15s), P3(15s), P4(30s)
 * P1(30s), (SCTE 30s)P2(15s), P3(0s), P4(15s), P5(30s)
 * .........AD1(15s),AD2(15s)                   ........
 */
TEST_F(AdManagerMPDTests, PlaceAdsTests_21)
{
  // not adding scte35 markers. These are mocked in PrivateObjectMPD instance
  // testPeriodId1 has 1 new fragments added, here periodDelta==0 is not expected, because once the ad duration is filled, ad is placed
  // testPeriodId2 has 0 fragments (empty period)
  // testPeriodId3 has 2 new fragments added
  static const char *manifest1 =
R"(<?xml version="1.0" encoding="utf-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" availabilityStartTime="2023-01-01T00:00:00Z" maxSegmentDuration="PT2S" minBufferTime="PT4.000S" minimumUpdatePeriod="P100Y" profiles="urn:dvb:dash:profile:dvb-dash:2014,urn:dvb:dash:profile:dvb-dash:isoff-ext-live:2014" publishTime="2023-01-01T00:01:00Z" timeShiftBufferDepth="PT5M" type="dynamic">
  <Period id="testPeriodId0" start="PT0S">
    <AdaptationSet id="0" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p0_init.mp4" media="video_p0_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="0" d="5000" r="14" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
  <Period id="testPeriodId1" start="PT30S">
    <AdaptationSet id="1" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p1_init.mp4" media="video_p1_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="75000" d="5000" r="6" />
            <S t="110000" d="2500" r="0" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
  <Period id="testPeriodId2" start="PT45S">
  </Period>
  <Period id="testPeriodId3" start="PT45S">
    <AdaptationSet id="1" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p1_init.mp4" media="video_p1_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="112500" d="2500" r="0" />
            <S t="115000" d="5000" r="0" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
</MPD>
)";

  // testPeriodId2 has 6 new fragments added
  // testPeriodId3 has 1 new fragments added
  static const char *manifest2 =
R"(<?xml version="1.0" encoding="utf-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" availabilityStartTime="2023-01-01T00:00:00Z" maxSegmentDuration="PT2S" minBufferTime="PT4.000S" minimumUpdatePeriod="P100Y" profiles="urn:dvb:dash:profile:dvb-dash:2014,urn:dvb:dash:profile:dvb-dash:isoff-ext-live:2014" publishTime="2023-01-01T00:01:00Z" timeShiftBufferDepth="PT5M" type="dynamic">
  <Period id="testPeriodId0" start="PT0S">
    <AdaptationSet id="0" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p0_init.mp4" media="video_p0_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="0" d="5000" r="14" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
  <Period id="testPeriodId1" start="PT30S">
    <AdaptationSet id="1" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p1_init.mp4" media="video_p1_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="75000" d="5000" r="6" />
            <S t="110000" d="2500" r="0" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
  <Period id="testPeriodId2" start="PT45S">
  </Period>
  <Period id="testPeriodId3" start="PT45S">
    <AdaptationSet id="1" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p1_init.mp4" media="video_p1_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="112500" d="2500" r="0" />
            <S t="115000" d="5000" r="6" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
  <Period id="testPeriodId4" start="PT60S">
    <AdaptationSet id="1" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p1_init.mp4" media="video_p1_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="150000" d="5000" r="0" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
</MPD>
)";
  std::string periodId1 = "testPeriodId1";
  std::string periodId2 = "testPeriodId2";
  std::string periodId3 = "testPeriodId3";
  std::string periodId4 = "testPeriodId4";
  ProcessSourceMPD(manifest1);
  // Set curEndNumber to 7, adNextOffset = (7)*2000
  mPrivateCDAIObjectMPD->mPlacementObj = PlacementObj(periodId1, periodId1, 7, 0, 14000, 0, false);

  // Add ads to the adBreak
  mPrivateCDAIObjectMPD->mAdBreaks = {
    {periodId1, AdBreakObject(30000, std::make_shared<std::vector<AdNode>>(), "", 0, 30000)}
  };
  // 1 - to - 2 mapping of ad and adbreak
  mPrivateCDAIObjectMPD->mAdBreaks[periodId1].ads->emplace_back(false, false, true, "adId1", "url1", 15000, periodId1, 0, nullptr);
  // Second ad doesn't have basePeriodId and basePeriodOffset set
  mPrivateCDAIObjectMPD->mAdBreaks[periodId1].ads->emplace_back(false, false, true, "adId2", "url2", 15000, "", -1, nullptr);

  // Add ads to mPeriodMap. mPeriodMap[periodId].adBreakId is non-empty for live at the beginning as per SetAlternateContents
  mPrivateCDAIObjectMPD->mPeriodMap[periodId1] = Period2AdData(false, periodId1, 14000 /*in ms*/,
    {
      std::make_pair (0, AdOnPeriod(0, 0)), // for adId1 idx=0, offset=0s
    });
  mPrivateCDAIObjectMPD->PlaceAds(mAdMPDParseHelper);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].ads->at(0).placed, false); // ad1 is not placed, waiting for periodId1 to have periodDelta==0
  EXPECT_FALSE(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].mAdBreakPlaced); // adBreak not placed
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPlacementObj.openPeriodId, periodId1); // open period not changed
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPlacementObj.adNextOffset, 15000); // ad1 is not placed
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPlacementObj.adStartOffset, 0); // ad1 starts from 0sec in periodId1

  ProcessSourceMPD(manifest2);
  mPrivateCDAIObjectMPD->PlaceAds(mAdMPDParseHelper);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].mSplitPeriod, true); // split period identified
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap.size(), 2); // periodId3 map created

  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId1].duration,15000); //periodmap of periodId3 duration
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId1].adBreakId, periodId1);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId1].offset2Ad[0].adIdx, 0);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId1].offset2Ad[0].adStartOffset, 0);

  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].ads->at(0).placed, true); // ad1 is placed
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].ads->at(1).placed, true); // ad2 is placed
  EXPECT_TRUE(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].mAdBreakPlaced); // adBreak placed
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].endPeriodId, periodId4);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].endPeriodOffset, 0);

  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId3].duration,15000); //periodmap of periodId3 duration
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId3].adBreakId, periodId1);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId3].offset2Ad[0].adIdx, 1);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId3].offset2Ad[0].adStartOffset, 0);

  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].ads->at(0).basePeriodId, "testPeriodId1");
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].ads->at(0).basePeriodOffset, 0);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].ads->at(1).basePeriodId, "testPeriodId3");
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].ads->at(1).basePeriodOffset, 0);
}

/**
 * @brief Tests the functionality of the PlaceAds method,periodDurationAvailable where ad of base period duration is greater than the next period ad duration .
 * P1(30s), (SCTE 30s)P2(16s), P3(12s), P4(2s)
 * .........AD1(15s),AD2(15s)         ........*/
TEST_F(AdManagerMPDTests, PlaceAdsTests_22)
{
  // not adding scte35 markers. These are mocked in PrivateObjectMPD instance
  static const char *manifest1 =
R"(<?xml version="1.0" encoding="utf-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" availabilityStartTime="2023-01-01T00:00:00Z" maxSegmentDuration="PT2S" minBufferTime="PT4.000S" minimumUpdatePeriod="P100Y" profiles="urn:dvb:dash:profile:dvb-dash:2014,urn:dvb:dash:profile:dvb-dash:isoff-ext-live:2014" publishTime="2023-01-01T00:01:00Z" timeShiftBufferDepth="PT5M" type="dynamic">
  <Period id="testPeriodId0" start="PT0S">
    <AdaptationSet id="0" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p0_init.mp4" media="video_p0_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="0" d="5000" r="14" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
  <Period id="testPeriodId1" start="PT30S">
    <AdaptationSet id="1" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p1_init.mp4" media="video_p1_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="75000" d="5000" r="7" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>

</MPD>
)";

  static const char *manifest2 =
R"(<?xml version="1.0" encoding="utf-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" availabilityStartTime="2023-01-01T00:00:00Z" maxSegmentDuration="PT2S" minBufferTime="PT4.000S" minimumUpdatePeriod="P100Y" profiles="urn:dvb:dash:profile:dvb-dash:2014,urn:dvb:dash:profile:dvb-dash:isoff-ext-live:2014" publishTime="2023-01-01T00:01:00Z" timeShiftBufferDepth="PT5M" type="dynamic">
  <Period id="testPeriodId0" start="PT0S">
    <AdaptationSet id="0" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p0_init.mp4" media="video_p0_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="0" d="5000" r="14" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
  <Period id="testPeriodId1" start="PT30S">
    <AdaptationSet id="1" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p1_init.mp4" media="video_p1_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="75000" d="5000" r="7" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
  <Period id="testPeriodId2" start="PT46S">
    <AdaptationSet id="1" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p1_init.mp4" media="video_p1_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="110000" d="5000" r="5" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
  <Period id="testPeriodId3" start="PT58S">
    <AdaptationSet id="1" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p1_init.mp4" media="video_p1_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="150000" d="5000" r="0" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
</MPD>
)";
  std::string periodId1 = "testPeriodId1";
  std::string periodId2 = "testPeriodId2";
  std::string periodId3 = "testPeriodId3";

  ProcessSourceMPD(manifest1);
  mPrivateCDAIObjectMPD->mPlacementObj = PlacementObj(periodId1, periodId1, 6, 0, 12000, 0, false);

  // Add ads to the adBreak
  mPrivateCDAIObjectMPD->mAdBreaks = {
    {periodId1, AdBreakObject(30000, std::make_shared<std::vector<AdNode>>(), "", 0, 30000)}
  };
  // 1 - to - 2 mapping of ad and adbreak
  mPrivateCDAIObjectMPD->mAdBreaks[periodId1].ads->emplace_back(false, false, true, "adId1", "url1", 15000, periodId1, 0, nullptr);
  // Second ad doesn't have basePeriodId and basePeriodOffset set
  mPrivateCDAIObjectMPD->mAdBreaks[periodId1].ads->emplace_back(false, false, true, "adId2", "url2", 15000, "", -1, nullptr);

  // Add ads to mPeriodMap. mPeriodMap[periodId].adBreakId is non-empty for live at the beginning as per SetAlternateContents
  mPrivateCDAIObjectMPD->mPeriodMap[periodId1] = Period2AdData(false, periodId1, 12000 /*in ms*/,
    {
      std::make_pair (0, AdOnPeriod(0, 0)), // for adId1 idx=0, offset=0s
    });
  mPrivateCDAIObjectMPD->PlaceAds(mAdMPDParseHelper);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].mSplitPeriod, false);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].ads->at(0).placed, true);
  EXPECT_FALSE(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].mAdBreakPlaced); // adBreak not placed

  ProcessSourceMPD(manifest2);
  mPrivateCDAIObjectMPD->PlaceAds(mAdMPDParseHelper);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap.size(), 2); // periodId2 map created
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId1].duration,16000); //periodmap of periodid1 duration
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId1].adBreakId, periodId1);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId1].offset2Ad[0].adIdx, 0);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId1].offset2Ad[0].adStartOffset, 0);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPlacementObj.openPeriodId, periodId2); // open period changed to periodId2
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPlacementObj.adNextOffset, 13000);

  mPrivateCDAIObjectMPD->PlaceAds(mAdMPDParseHelper);//place ads called again to make periodDelta = 0
  EXPECT_TRUE(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].mAdBreakPlaced); // adBreak placed
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].endPeriodId, periodId3);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].endPeriodOffset, 0);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].ads->at(0).placed, true);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].ads->at(1).placed, true);

  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].ads->at(0).basePeriodId, "testPeriodId1");
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].ads->at(0).basePeriodOffset, 0);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].ads->at(1).basePeriodId, "testPeriodId1");
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].ads->at(1).basePeriodOffset, 15000);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId2].duration,12000);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId2].adBreakId, periodId1);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId2].offset2Ad[0].adIdx, 1);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId2].offset2Ad[0].adStartOffset, 1000);
}

/**
 * @brief Regression test for the "ad short by < 2s" race condition in adjustEndPeriodOffset.
 *
 * Scenario (mirrors AAMP-CDAI-8006):
 *   - 30-second SCTE break (brkDuration = 30000 ms) with a 28.8-second ad
 *     (adsDuration = 28800 ms — this is the cumulative filled-ad duration, NOT
 *     the break duration; adsDuration starts at 0 and is incremented per ad in
 *     SetAlternateContents). The ad is short by 1.2 s.
 *   - After ad placement completes the AdBreakObject state is:
 *       brkDuration = 30000, adsDuration = 28800
 *       endPeriodId = "testPeriodId1", endPeriodOffset = 28800
 *       adjustEndPeriodOffset = true
 *   - On the next MPD refresh the second period ("testPeriodId2") has NOT yet
 *     appeared (currPeriodDuration == 0), but two new 2-second segments have been
 *     appended to "testPeriodId1" after the ad ended (periodDelta = 4000 >= OFFSET_ALIGN_FACTOR).
 *
 * Without the fix the code falls into the ELSE "NOT close to period end" branch and
 * incorrectly marks the adbreak as placed with endPeriodOffset=28800, causing the
 * fetcher to resume from fragment 030 in the not_expected range.
 *
 * With the fix the additional check
 *   (unfilledBreakTimeMs = brkDuration - adsDuration = 30000 - 28800 = 1200 < OFFSET_ALIGN_FACTOR)
 * keeps the code in the WAIT branch until the next period is available.
 */
TEST_F(AdManagerMPDTests, PlaceAdsTests_23_ShortAdRaceCondition)
{
  // Manifest 1: single source period + one ad period.
  // testPeriodId1 has 14 x 2s segments (0..27s), simulating the source period
  // filling up while the 28.8s ad plays. curEndNumber will reach 14 after PlaceAds.
  static const char *manifest1 =
R"(<?xml version="1.0" encoding="utf-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" availabilityStartTime="2023-01-01T00:00:00Z" maxSegmentDuration="PT2S" minBufferTime="PT4.000S" minimumUpdatePeriod="PT6S" profiles="urn:dvb:dash:profile:dvb-dash:2014,urn:dvb:dash:profile:dvb-dash:isoff-ext-live:2014" publishTime="2023-01-01T00:00:00Z" timeShiftBufferDepth="PT5M" type="dynamic">
  <Period id="testPeriodId0" start="PT0S">
    <AdaptationSet id="0" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p0_init.mp4" media="video_p0_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="0" d="5000" r="14" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
  <Period id="testPeriodId1" start="PT30S">
    <AdaptationSet id="0" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p1_init.mp4" media="video_p1_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="75000" d="5000" r="13" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
</MPD>
)";

  // Manifest 2 (race condition): testPeriodId2 has NOT appeared yet, but two new
  // 2s segments (numbers 15 and 16) have been appended to testPeriodId1.
  // GetPeriodNewContentDurationMs will count these and return periodDelta = 4000ms
  // (>= OFFSET_ALIGN_FACTOR). Without the fix this would cause the buggy ELSE path.
  static const char *manifest2_race =
R"(<?xml version="1.0" encoding="utf-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" availabilityStartTime="2023-01-01T00:00:00Z" maxSegmentDuration="PT2S" minBufferTime="PT4.000S" minimumUpdatePeriod="PT6S" profiles="urn:dvb:dash:profile:dvb-dash:2014,urn:dvb:dash:profile:dvb-dash:isoff-ext-live:2014" publishTime="2023-01-01T00:00:06Z" timeShiftBufferDepth="PT5M" type="dynamic">
  <Period id="testPeriodId0" start="PT0S">
    <AdaptationSet id="0" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p0_init.mp4" media="video_p0_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="0" d="5000" r="14" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
  <Period id="testPeriodId1" start="PT30S">
    <AdaptationSet id="0" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p1_init.mp4" media="video_p1_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="75000" d="5000" r="15" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
</MPD>
)";

  // Manifest 3: testPeriodId2 now appears at PT60S, giving testPeriodId1 a
  // duration of 30s. diff = 30000 - 28800 = 1200 < OFFSET_ALIGN_FACTOR, so the
  // adbreak should align to testPeriodId2 with endPeriodOffset = 0.
  static const char *manifest3_resolved =
R"(<?xml version="1.0" encoding="utf-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" availabilityStartTime="2023-01-01T00:00:00Z" maxSegmentDuration="PT2S" minBufferTime="PT4.000S" minimumUpdatePeriod="PT6S" profiles="urn:dvb:dash:profile:dvb-dash:2014,urn:dvb:dash:profile:dvb-dash:isoff-ext-live:2014" publishTime="2023-01-01T00:00:12Z" timeShiftBufferDepth="PT5M" type="dynamic">
  <Period id="testPeriodId0" start="PT0S">
    <AdaptationSet id="0" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p0_init.mp4" media="video_p0_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="0" d="5000" r="14" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
  <Period id="testPeriodId1" start="PT30S">
    <AdaptationSet id="0" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p1_init.mp4" media="video_p1_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="75000" d="5000" r="14" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
  <Period id="testPeriodId2" start="PT60S">
    <AdaptationSet id="0" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p2_init.mp4" media="video_p2_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="150000" d="5000" r="0" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
</MPD>
)";

  const std::string periodId1 = "testPeriodId1";
  const std::string periodId2 = "testPeriodId2";

  // ----- Phase 1: drive PlaceAds until the 28.8s ad is placed -----
  // testPeriodId1 has 14 x 2s segments = 28s of content when we start.
  // adNextOffset starts at 0; after PlaceAds the ad fills 28s, leaving 0.8s
  // of the 28.8s ad still to place, so we need a bit more content.
  // Simplify: pre-set mPlacementObj as if we already advanced through 14 segments
  // and the ad needs just 0.8s more than what is available.
  // Actually, use the full PlaceAds flow:

  ProcessSourceMPD(manifest1);
  // 28.8s ad in a 30s SCTE break
  mPrivateCDAIObjectMPD->mAdBreaks = {
    {periodId1, AdBreakObject(30000, std::make_shared<std::vector<AdNode>>(), "", 0, 28800)}
  };
  mPrivateCDAIObjectMPD->mAdBreaks[periodId1].ads->emplace_back(
    false, false, true, "adId1", "url1", 28800, periodId1, 0, nullptr);

  mPrivateCDAIObjectMPD->mPeriodMap[periodId1] = Period2AdData(false, periodId1, 0 /*duration*/,
    {std::make_pair(0, AdOnPeriod(0, 0))});

  // Start placement from the beginning of testPeriodId1
  mPrivateCDAIObjectMPD->mPlacementObj = PlacementObj(periodId1, periodId1, 0, 0, 0, 0, false);

  // PlaceAds with manifest1: testPeriodId1 has 14 segments (28s).
  // The ad is 28.8s so 28s of content is not enough; placement waits.
  mPrivateCDAIObjectMPD->PlaceAds(mAdMPDParseHelper);
  EXPECT_FALSE(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].mAdBreakPlaced);
  EXPECT_FALSE(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].adjustEndPeriodOffset);

  // Add one extra segment at t=145000 to testPeriodId1 so the ad can
  // fully fit. This appended <S> has d="2000" at timescale 2500, i.e. 0.8s.
  // Reuse manifest1 and append one extra <S>; the original repeated entry
  // remains r="13". For simplicity just update the manifest string directly.
  static const char *manifest1b =
R"(<?xml version="1.0" encoding="utf-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" availabilityStartTime="2023-01-01T00:00:00Z" maxSegmentDuration="PT2S" minBufferTime="PT4.000S" minimumUpdatePeriod="PT6S" profiles="urn:dvb:dash:profile:dvb-dash:2014,urn:dvb:dash:profile:dvb-dash:isoff-ext-live:2014" publishTime="2023-01-01T00:00:02Z" timeShiftBufferDepth="PT5M" type="dynamic">
  <Period id="testPeriodId0" start="PT0S">
    <AdaptationSet id="0" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p0_init.mp4" media="video_p0_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="0" d="5000" r="14" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
  <Period id="testPeriodId1" start="PT30S">
    <AdaptationSet id="0" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p1_init.mp4" media="video_p1_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="75000" d="5000" r="13" />
            <S t="145000" d="2000" r="0" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
</MPD>
)";

  // Process updated manifest with segment 15 (0.8s more, total 28.8s).
  ProcessSourceMPD(manifest1b);
  mPrivateCDAIObjectMPD->PlaceAds(mAdMPDParseHelper);

  // Ad should now be placed and adjustEndPeriodOffset should be set to true.
  // mAdBreakPlaced is still false because offset adjustment is pending.
  EXPECT_TRUE(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].adjustEndPeriodOffset);
  EXPECT_FALSE(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].mAdBreakPlaced);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].endPeriodId, periodId1);
  // endPeriodOffset = p2AdData.duration - periodDelta.
  // p2AdData.duration = 28800 (filled by 14*2000 + 800), periodDelta = 0 (exact fit)
  // so endPeriodOffset should be 28800.
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].endPeriodOffset, 28800u);

  // ----- Phase 2: Race condition — next period not yet in manifest but new
  // segments appeared in testPeriodId1 (periodDelta >= OFFSET_ALIGN_FACTOR).
  // Without the fix this triggers the ELSE path and wrongly marks placed. -----
  ProcessSourceMPD(manifest2_race);
  mPrivateCDAIObjectMPD->PlaceAds(mAdMPDParseHelper);

  // KEY ASSERTION: adbreak must NOT be marked as placed yet.
  // unfilledBreakTimeMs = brkDuration - adsDuration = 30000 - 28800 = 1200 < OFFSET_ALIGN_FACTOR(2000)
  // so the fixed WAIT condition must hold even though periodDelta >= 2000.
  EXPECT_FALSE(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].mAdBreakPlaced)
    << "Regression: adjustEndPeriodOffset ELSE path fired before next period appeared";
  EXPECT_TRUE(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].adjustEndPeriodOffset)
    << "adjustEndPeriodOffset should still be true (still waiting)";

  // ----- Phase 3: Next period now appears — should align to periodId2 -----
  ProcessSourceMPD(manifest3_resolved);
  mPrivateCDAIObjectMPD->PlaceAds(mAdMPDParseHelper);

  EXPECT_TRUE(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].mAdBreakPlaced);
  EXPECT_FALSE(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].adjustEndPeriodOffset);
  // diff = 30000 - 28800 = 1200 < OFFSET_ALIGN_FACTOR -> aligned to next period
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].endPeriodId, periodId2);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].endPeriodOffset, 0u);
}

/**
 * @brief Verifies static-manifest post-roll handling when endPeriodId is the last period.
 *
 * When adjustEndPeriodOffset is pending and the end period is the final period in a
 * static MPD (no next period), PlaceAds should complete placement immediately and mark
 * the adbreak as post-roll.
 */
TEST_F(AdManagerMPDTests, PlaceAdsTests_24_StaticManifestPostRollLastPeriod)
{
  static const char *manifest =
R"(<?xml version="1.0" encoding="utf-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" type="static" mediaPresentationDuration="PT60S" minBufferTime="PT2S">
  <Period id="testPeriodId0" start="PT0S">
    <AdaptationSet id="0" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p0_init.mp4" media="video_p0_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="0" d="5000" r="14" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
  <Period id="testPeriodId1" start="PT30S">
    <AdaptationSet id="1" contentType="video">
      <Representation id="1" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p1_init.mp4" media="video_p1_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="75000" d="5000" r="14" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
</MPD>
)";

  const std::string adBreakId = "testPeriodId1";
  const std::string endPeriodId = "testPeriodId1";

  ProcessSourceMPD(manifest);

  mPrivateCDAIObjectMPD->mAdBreaks = {
    {adBreakId, AdBreakObject(30000, std::make_shared<std::vector<AdNode>>(), "", 0, 30000)}
  };
  mPrivateCDAIObjectMPD->mAdBreaks[adBreakId].ads->emplace_back(
    false, false, true, "adId1", "url1", 30000, adBreakId, 0, nullptr);

  mPrivateCDAIObjectMPD->mPeriodMap[adBreakId] = Period2AdData(false, adBreakId, 0 /*duration*/,
    {std::make_pair(0, AdOnPeriod(0, 0))});

  // Make PlaceAds enter the adbreak processing path.
  mPrivateCDAIObjectMPD->mPlacementObj = PlacementObj(adBreakId, adBreakId, 0, 0, 0, 0, false);

  mPrivateCDAIObjectMPD->PlaceAds(mAdMPDParseHelper);

  EXPECT_FALSE(mPrivateCDAIObjectMPD->mAdBreaks[adBreakId].adjustEndPeriodOffset);
  EXPECT_TRUE(mPrivateCDAIObjectMPD->mAdBreaks[adBreakId].mAdBreakPlaced);
  EXPECT_TRUE(mPrivateCDAIObjectMPD->mAdBreaks[adBreakId].mIsPostRollAdBreak);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[adBreakId].endPeriodId, endPeriodId);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[adBreakId].endPeriodOffset, 30000u);
}

/**
 * @brief Test case for WaitForNextAdResolved with no AdFulfillObj
 */
TEST_F(AdManagerMPDTests, WaitForNextAdResolved_NoAdFulfillObj)
{
  // Nothing set in mAdFulfillObj, should return false immediately
  EXPECT_FALSE(mPrivateCDAIObjectMPD->WaitForNextAdResolved(50));
}

/**
 * @brief Test case for WaitForNextAdResolved with an immediately resolved AdFulfillObj
 */
TEST_F(AdManagerMPDTests, WaitForNextAdResolved_ResolvedImmediately)
{
  std::string periodId = "p1";
  std::string adId = "ad1";

  // Add ads to the adBreak
  mPrivateCDAIObjectMPD->mAdBreaks = {
      {periodId, AdBreakObject(30000, std::make_shared<std::vector<AdNode>>(), "", 0, 30000)}};
  // Create ad break and insert resolved ad
  mPrivateCDAIObjectMPD->mAdBreaks[periodId].ads->emplace_back(false, false, true, adId, "", 0, "", 0, nullptr);
  mPrivateCDAIObjectMPD->mAdFulfillObj = AdFulfillObj(periodId, adId, "dummy");
  EXPECT_TRUE(mPrivateCDAIObjectMPD->WaitForNextAdResolved(100));
}

/**
 * @brief Test case for WaitForNextAdResolved timing out
 */
TEST_F(AdManagerMPDTests, WaitForNextAdResolved_TimesOut)
{
    std::string periodId = "p1";
    std::string adId = "ad1";

      // Add ads to the adBreak
    mPrivateCDAIObjectMPD->mAdBreaks = {
      {periodId, AdBreakObject(30000, std::make_shared<std::vector<AdNode>>(), "", 0, 30000)}};
    mPrivateCDAIObjectMPD->mAdBreaks[periodId].ads->emplace_back(false, false, false, adId, "", 0, "", 0, nullptr);
    mPrivateCDAIObjectMPD->mAdFulfillObj = AdFulfillObj(periodId, adId, "dummy");
    EXPECT_FALSE(mPrivateCDAIObjectMPD->WaitForNextAdResolved(50));
}

/**
 * @brief Test case for WaitForNextAdResolved But the resolution is signalled before timeout
 */
TEST_F(AdManagerMPDTests, WaitForNextAdResolved_SignalledBeforeTimeout)
{
    std::string periodId = "p1";
    std::string adId = "ad1";

      // Add ads to the adBreak
    mPrivateCDAIObjectMPD->mAdBreaks = {
      {periodId, AdBreakObject(30000, std::make_shared<std::vector<AdNode>>(), "", 0, 30000)}};
    mPrivateCDAIObjectMPD->mAdBreaks[periodId].ads->emplace_back(false, false, false, adId, "", 0, "", 0, nullptr);
    mPrivateCDAIObjectMPD->mAdFulfillObj = AdFulfillObj(periodId, adId, "dummy");

    // Run wait in a thread
    bool completed = false;
    std::thread waiter([&]{
        completed = mPrivateCDAIObjectMPD->WaitForNextAdResolved(500);
    });

    // Give some time then resolve and notify
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    {
        std::lock_guard<std::mutex> lock(mPrivateCDAIObjectMPD->mAdPlacementMtx);
        mPrivateCDAIObjectMPD->mAdBreaks[periodId].ads->at(0).resolved = true;
        mPrivateCDAIObjectMPD->mAdPlacementCV.notify_one();
    }

    waiter.join();
    EXPECT_TRUE(completed);
}

/**
 * @brief Test case for WaitForNextAdResolved when the downloads are disabled
 */
TEST_F(AdManagerMPDTests, WaitForNextAdResolved_DisableDownloadsBeforeWait)
{
  std::string periodId = "p1";
  std::string adId = "ad1";

  // Add ads to the adBreak
  mPrivateCDAIObjectMPD->mAdBreaks = {
    {periodId, AdBreakObject(30000, std::make_shared<std::vector<AdNode>>(), "", 0, 30000)}};
  mPrivateCDAIObjectMPD->mAdBreaks[periodId].ads->emplace_back(false, false, false, adId, "", 0, "", 0, nullptr);
  mPrivateCDAIObjectMPD->mAdFulfillObj = AdFulfillObj(periodId, adId, "dummy");

  // Abort before waiting
  EXPECT_CALL(*g_mockPrivateInstanceAAMP ,DownloadsAreEnabled()).WillRepeatedly(Return(false));

  auto start = std::chrono::steady_clock::now();
  bool result = mPrivateCDAIObjectMPD->WaitForNextAdResolved(5000);
  auto end = std::chrono::steady_clock::now();
  auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

  // Should return true as it aborted immediately, downloads are disabled
  EXPECT_TRUE(result);
  // Fail if it actually waited for more than 500ms (indicating it did not abort immediately)
  EXPECT_LT(elapsedMs, 500) << "WaitForNextAdResolved did not abort immediately, waited for " << elapsedMs << " ms";
}

/**
* @brief Test NotifyReservationComplete for empty ad break: should resolve and notify waiting threads.
*/
TEST_F(AdManagerMPDTests, NotifyReservationComplete_EmptyAdBreak_NotifiesAndResolves)
{
    std::string periodId = "testPeriodId";
    mPrivateCDAIObjectMPD->mAdBreaks[periodId] = AdBreakObject(10000, nullptr, "", 0, 0);

    // Start a thread that waits for ad resolution (should be notified by NotifyReservationComplete)
    bool completed = false;
    std::thread waiter([&] {
      completed = mPrivateCDAIObjectMPD->WaitForNextAdResolved(5000, periodId);
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    mPrivateCDAIObjectMPD->NotifyReservationComplete(periodId);

    waiter.join();
    EXPECT_TRUE(mPrivateCDAIObjectMPD->mAdBreaks[periodId].resolved);
    // Empty ad breaks must also be marked invalid so that subsequent
    // SetAlternateContents() calls reject further ads for this break.
    EXPECT_TRUE(mPrivateCDAIObjectMPD->mAdBreaks[periodId].invalid);
    // The waiting thread should have completed (not timed out)
    EXPECT_TRUE(completed);
}

/**
 * @brief CancelReservation should set cancelAtReservationId for active placement.
 */
TEST_F(AdManagerMPDTests, CancelReservation_MatchingPlacement_SetsCancelId)
{
  const std::string activeReservationId = "playingBrk";
  const std::string cancelAtReservationId = "nextBrk";

  mPrivateCDAIObjectMPD->mAdBreaks[activeReservationId] =
    AdBreakObject(30000, nullptr, "", 0, 0);
  mPrivateCDAIObjectMPD->mPlacementObj.pendingAdbrkId = activeReservationId;

  EXPECT_TRUE(mPrivateCDAIObjectMPD->mAdBreaks[activeReservationId].
    cancelAtPeriodId.empty());

  mPrivateCDAIObjectMPD->CancelReservation(cancelAtReservationId);

  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[activeReservationId].
    cancelAtPeriodId, cancelAtReservationId);
}

/**
 * @brief CancelReservation with empty cancel id should not change ad break state.
 */
TEST_F(AdManagerMPDTests, CancelReservation_EmptyCancelId_NoChange)
{
  const std::string periodId = "playingBrk";

  mPrivateCDAIObjectMPD->mAdBreaks[periodId] =
    AdBreakObject(30000, nullptr, "", 0, 0);
  mPrivateCDAIObjectMPD->mPlacementObj.pendingAdbrkId.clear();

  EXPECT_TRUE(mPrivateCDAIObjectMPD->mAdBreaks[periodId].
    cancelAtPeriodId.empty());

  mPrivateCDAIObjectMPD->CancelReservation("");

  EXPECT_TRUE(mPrivateCDAIObjectMPD->mAdBreaks[periodId].
    cancelAtPeriodId.empty());
}

/**
 * @brief CancelReservation with missing placement break should not change state.
 */
TEST_F(AdManagerMPDTests, CancelReservation_PlacementBreakMissing_NoChange)
{
  const std::string existingBreakId = "existingBrk";
  const std::string cancelAtReservationId = "nextBrk";

  mPrivateCDAIObjectMPD->mAdBreaks[existingBreakId] =
    AdBreakObject(30000, nullptr, "", 0, 0);
  mPrivateCDAIObjectMPD->mAdBreaks[existingBreakId].mAdBreakPlaced = true;
  mPrivateCDAIObjectMPD->mPlacementObj.pendingAdbrkId.clear();

  EXPECT_TRUE(mPrivateCDAIObjectMPD->mAdBreaks[existingBreakId].
    cancelAtPeriodId.empty());

  mPrivateCDAIObjectMPD->CancelReservation(cancelAtReservationId);

  EXPECT_TRUE(mPrivateCDAIObjectMPD->mAdBreaks[existingBreakId].
    cancelAtPeriodId.empty());
}

/**
 * @brief RegisterVodAdBreak + CheckVodAdBreakLookahead: opportunity fires exactly
 * once when playhead enters the lookahead window, and mNextVodBreakToCheck
 * advances to max() after the only break is consumed.
 */
TEST_F(AdManagerMPDTests, VodAdBreak_OpportunityFiresOnceInWindow)
{
  VodAdBreakInfo info;
  info.breakId = "brk1";
  info.insertionPointSec = 30.0;
  info.breakDurationSec = 15.0;
  info.breakType = "midroll";
  info.opportunityFired = false;
  info.cancelled = false;
  mPrivateCDAIObjectMPD->RegisterVodAdBreak(info);

  // Position outside lookahead window — should not fire.
  mPrivateCDAIObjectMPD->CheckVodAdBreakLookahead(20.0, 5.0);
  EXPECT_FALSE(mPrivateCDAIObjectMPD->mVodAdBreaks.at("brk1").opportunityFired);

  // Position inside lookahead window (30 - 5 = 25, pos 25 >= 25) — should fire.
  mPrivateCDAIObjectMPD->CheckVodAdBreakLookahead(25.0, 5.0);
  EXPECT_TRUE(mPrivateCDAIObjectMPD->mVodAdBreaks.at("brk1").opportunityFired);

  // Sentinel should now be max() — no remaining unfired breaks.
  EXPECT_EQ(mPrivateCDAIObjectMPD->mNextVodBreakToCheck,
    std::numeric_limits<double>::max());

  // Second call at same position — must not re-fire (opportunityFired stays true).
  mPrivateCDAIObjectMPD->CheckVodAdBreakLookahead(25.0, 5.0);
  EXPECT_TRUE(mPrivateCDAIObjectMPD->mVodAdBreaks.at("brk1").opportunityFired);
}

/**
 * @brief CancelVodAdBreak: cancelled breaks never cause an opportunity event and
 * mNextVodBreakToCheck skips over them to the next active break.
 */
TEST_F(AdManagerMPDTests, VodAdBreak_CancelledBreakNeverFires)
{
  VodAdBreakInfo brk1;
  brk1.breakId = "brkA";
  brk1.insertionPointSec = 20.0;
  brk1.breakDurationSec = 10.0;
  brk1.breakType = "midroll";
  brk1.opportunityFired = false;
  brk1.cancelled = false;

  VodAdBreakInfo brk2;
  brk2.breakId = "brkB";
  brk2.insertionPointSec = 50.0;
  brk2.breakDurationSec = 10.0;
  brk2.breakType = "midroll";
  brk2.opportunityFired = false;
  brk2.cancelled = false;

  mPrivateCDAIObjectMPD->RegisterVodAdBreak(brk1);
  mPrivateCDAIObjectMPD->RegisterVodAdBreak(brk2);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mNextVodBreakToCheck, 20.0);

  // Cancel the earlier break.
  mPrivateCDAIObjectMPD->CancelVodAdBreak("brkA");

  // Sentinel should advance to the next active break (brkB at 50).
  EXPECT_EQ(mPrivateCDAIObjectMPD->mNextVodBreakToCheck, 50.0);

  // Passing through the cancelled break's window — should not fire.
  mPrivateCDAIObjectMPD->CheckVodAdBreakLookahead(19.0, 5.0);
  EXPECT_FALSE(mPrivateCDAIObjectMPD->mVodAdBreaks.at("brkA").opportunityFired);
}

/**
 * @brief mNextVodBreakToCheck sentinel: adding multiple breaks sets it to the
 * earliest; cancelling the earliest recomputes it to the next one; cancelling
 * all breaks sets it to max().
 */
TEST_F(AdManagerMPDTests, VodAdBreak_SentinelTracksEarliestActiveBreak)
{
  VodAdBreakInfo b1;
  b1.breakId = "b1"; b1.insertionPointSec = 60.0;
  b1.breakDurationSec = 5.0; b1.breakType = "midroll";
  b1.opportunityFired = false; b1.cancelled = false;

  VodAdBreakInfo b2;
  b2.breakId = "b2"; b2.insertionPointSec = 30.0;
  b2.breakDurationSec = 5.0; b2.breakType = "midroll";
  b2.opportunityFired = false; b2.cancelled = false;

  VodAdBreakInfo b3;
  b3.breakId = "b3"; b3.insertionPointSec = 90.0;
  b3.breakDurationSec = 5.0; b3.breakType = "midroll";
  b3.opportunityFired = false; b3.cancelled = false;

  mPrivateCDAIObjectMPD->RegisterVodAdBreak(b1);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mNextVodBreakToCheck, 60.0);

  mPrivateCDAIObjectMPD->RegisterVodAdBreak(b2);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mNextVodBreakToCheck, 30.0);  // earlier

  mPrivateCDAIObjectMPD->RegisterVodAdBreak(b3);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mNextVodBreakToCheck, 30.0);  // unchanged

  mPrivateCDAIObjectMPD->CancelVodAdBreak("b2");
  EXPECT_EQ(mPrivateCDAIObjectMPD->mNextVodBreakToCheck, 60.0);  // next earliest

  mPrivateCDAIObjectMPD->CancelVodAdBreak("b1");
  EXPECT_EQ(mPrivateCDAIObjectMPD->mNextVodBreakToCheck, 90.0);

  mPrivateCDAIObjectMPD->CancelVodAdBreak("b3");
  EXPECT_EQ(mPrivateCDAIObjectMPD->mNextVodBreakToCheck,
    std::numeric_limits<double>::max());
}

/**
 * ============================================================================
 * Integration tests: static-manifest CDAI ad placement synchronization
 *
 * These tests cover static-manifest flows where there is no periodic
 * manifest refresh to drive PlaceAds(). Placement is triggered by
 * NotifyReservationComplete once ads are resolved, with a deferred trigger from
 * FulFillAdObject when reservation was completed earlier.
 * ============================================================================
 *
 * Source MPD used by all three tests below:
 * Period 0  : testPeriodId0    - 30 s background content  (start=0)
 * Period 1  : testPeriodId1    - 30 s ad-break period     (start=30s)
 * Period 2  : testPeriodId_nxt - 4 s following content    (start=60s)
 */
static const char *kStaticSourceManifest =
R"(<?xml version="1.0" encoding="utf-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" type="static"
     mediaPresentationDuration="PT1M4S" minBufferTime="PT4S"
     profiles="urn:mpeg:dash:profile:isoff-on-demand:2011">
  <Period id="testPeriodId0" start="PT0S">
    <AdaptationSet id="0" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p0_init.mp4" media="video_p0_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="0" d="5000" r="14" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
  <Period id="testPeriodId1" start="PT30S">
    <AdaptationSet id="1" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p1_init.mp4" media="video_p1_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="0" d="5000" r="14" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
  <Period id="testPeriodId_nxt" start="PT60S">
    <AdaptationSet id="1" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25">
        <SegmentTemplate timescale="2500" initialization="video_p1_init.mp4" media="video_p1_$Number$.m4s" startNumber="1">
          <SegmentTimeline>
            <S t="0" d="5000" r="1" />
          </SegmentTimeline>
        </SegmentTemplate>
      </Representation>
    </AdaptationSet>
  </Period>
</MPD>
)";

/**
 * @brief Integration test: static-manifest placement is triggered on
 * NotifyReservationComplete after ad fulfillment resolves the ad.
 *
 * Verifies that after ad fulfillment and reservation completion, the
 * period-to-ad map is built (mPeriodMap[periodId].duration > 0, ad marked
 * placed) without any manifest refresh.
 */
TEST_F(AdManagerMPDTests, StaticManifest_NotifyComplete_TriggersPlacement)
{
  const std::string periodId = "testPeriodId1";
  const std::string adId     = "testAdId";
  const std::string url      = TEST_AD_MANIFEST_URL;
  constexpr uint64_t startMS   = 0;
  constexpr uint32_t breakdur  = 30000; // matches source period duration (30 s)
  auto adResolvedPromise = std::make_shared<std::promise<void>>();
  auto adResolvedFuture = adResolvedPromise->get_future();

  // Step 1: create the placeholder ad-break entry.
  mPrivateCDAIObjectMPD->SetAlternateContents(periodId, "", "", startMS, breakdur);

  // Step 2: parse the static source MPD and store it as the base helper so
  // NotifyReservationComplete/deferred fulfillment can trigger PlaceAds.
  // Generate mAdMPDParseHelper here so it can be passed via SetBaseMPDParseHelper.
  ProcessSourceMPD(kStaticSourceManifest);
  ASSERT_TRUE(mAdMPDParseHelper != nullptr);
  mPrivateCDAIObjectMPD->SetBaseMPDParseHelper(mAdMPDParseHelper);

  // Step 3: set up the mock to serve the ad manifest on download.
  mManifest = kSharedTenSecondAdManifest;
  EXPECT_CALL(*g_mockPrivateInstanceAAMP,
      GetFile(std::string(TEST_AD_MANIFEST_URL), _, _, _, _, _, _, _, _, _, _, _, _, _, _))
      .WillOnce(WithArgs<0,2,3,4>(Invoke(this, &AdManagerMPDTests::GetManifest)));
  EXPECT_CALL(*g_mockPrivateInstanceAAMP,
      SendAdResolvedEvent(adId, true, startMS, 10000u, eCDAI_ERROR_NONE))
    .Times(1)
    .WillOnce(InvokeWithoutArgs([adResolvedPromise]{ adResolvedPromise->set_value(); }));

  std::string adInitUrl = GetFullURI(TEST_AD_MANIFEST_HOST, "manifest/track-video-repid-LE5-tc--header.mp4");
  EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetFile (adInitUrl, _, _, _, _, _, _, _, _, _, _, _, _, _, _))
            .WillOnce(Return(true));
  adInitUrl = GetFullURI(TEST_AD_MANIFEST_HOST, "manifest-eac3/track-audio-repid-DDen-tc--header.mp4");
  EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetFile (adInitUrl, _, _, _, _, _, _, _, _, _, _, _, _, _, _))
            .WillOnce(Return(true));

  // Step 4: trigger the fulfillment loop.
  mPrivateCDAIObjectMPD->SetAlternateContents(periodId, adId, url, startMS, breakdur);
  // Wait for async FulFillAdObject completion signal from SendAdResolvedEvent.
  ASSERT_EQ(adResolvedFuture.wait_for(std::chrono::milliseconds(500)), std::future_status::ready)
      << "Timed out waiting for SendAdResolvedEvent callback";

  // Step 5: validate that the ad is resolved but placement has not yet occurred.
  EXPECT_FALSE(mPrivateCDAIObjectMPD->mAdBreaks[periodId].mAdBreakPlaced)
      << "SetAlternateContents should not have marked the adbreak as placed yet (NotifyReservationComplete not called)";
  EXPECT_TRUE(mPrivateCDAIObjectMPD->isAdBreakObjectExist(periodId));
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId].ads->size(), 1u);
  EXPECT_TRUE(mPrivateCDAIObjectMPD->mAdBreaks[periodId].ads->at(0).resolved);
  EXPECT_FALSE(mPrivateCDAIObjectMPD->mAdBreaks[periodId].ads->at(0).invalid);
  EXPECT_FALSE(mPrivateCDAIObjectMPD->mAdBreaks[periodId].ads->at(0).placed)
      << "SetAlternateContents should not have marked the ad as placed yet (NotifyReservationComplete not called)";

  // Step 6: all ads resolved. Now mark reservation complete, which triggers PlaceAds for static manifest.
  mPrivateCDAIObjectMPD->NotifyReservationComplete(periodId);

  // Step 7: assert the period-to-ad map was built by PlaceAds (no refresh needed).
  // PlaceAds ran → ad is fully placed and the adbreak is marked complete.
  EXPECT_TRUE(mPrivateCDAIObjectMPD->mAdBreaks[periodId].ads->at(0).placed)
      << "PlaceAds should have marked the ad as placed after NotifyReservationComplete";
  EXPECT_TRUE(mPrivateCDAIObjectMPD->mAdBreaks[periodId].mAdBreakPlaced)
      << "PlaceAds should have marked the adbreak as placed after NotifyReservationComplete";

  // mPeriodMap must be populated: duration is the amount of source-period content
  // consumed while placing the ad (30 s for the 30 s source period).
  EXPECT_GT(mPrivateCDAIObjectMPD->mPeriodMap[periodId].duration, 0u)
      << "PlaceAds should have accumulated period duration into mPeriodMap";
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId].adBreakId, periodId);

  // Step 8: Placement is complete: mPlacementObj should be reset/empty.
  EXPECT_TRUE(mPrivateCDAIObjectMPD->mPlacementObj.pendingAdbrkId.empty());
  EXPECT_TRUE(mPrivateCDAIObjectMPD->mPlacementObj.openPeriodId.empty());
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPlacementObj.curAdIdx, -1);
}

/**
 * @brief Integration test: 20s ad break with two 10s ads fulfilled via
 * SetAlternateContents should complete placement only after
 * NotifyReservationComplete is called.
 */
TEST_F(AdManagerMPDTests, StaticManifest_TwoAds_CompleteTwentySecondBreak)
{
  const std::string periodId = "testPeriodId1";
  const std::string adId1    = "testAdId1";
  const std::string adId2    = "testAdId2";
  const std::string url      = TEST_AD_MANIFEST_URL;
  constexpr uint64_t startMS      = 0;
  constexpr uint32_t breakdur     = 20000;
  constexpr uint32_t adDurationMS = 10000;

  auto adResolvedPromise1 = std::make_shared<std::promise<void>>();
  auto adResolvedFuture1 = adResolvedPromise1->get_future();
  auto adResolvedPromise2 = std::make_shared<std::promise<void>>();
  auto adResolvedFuture2 = adResolvedPromise2->get_future();

  // Step 1: create the 20s placeholder ad-break entry.
  mPrivateCDAIObjectMPD->SetAlternateContents(periodId, "", "", startMS, breakdur);

  // Step 2: parse source MPD and set base helper so static-manifest
  // placement can run when reservation completion is notified.
  ProcessSourceMPD(kStaticSourceManifest);
  ASSERT_TRUE(mAdMPDParseHelper != nullptr);
  mPrivateCDAIObjectMPD->SetBaseMPDParseHelper(mAdMPDParseHelper);

  // Step 3: each ad fulfillment downloads the same 10s ad manifest.
  mManifest = kSharedTenSecondAdManifest;
  EXPECT_CALL(*g_mockPrivateInstanceAAMP,
      GetFile(std::string(TEST_AD_MANIFEST_URL), _, _, _, _, _, _, _, _, _, _, _, _, _, _))
      .Times(2)
      .WillRepeatedly(WithArgs<0,2,3,4>(Invoke(this, &AdManagerMPDTests::GetManifest)));

  {
    ::testing::InSequence seq;
    EXPECT_CALL(*g_mockPrivateInstanceAAMP,
        SendAdResolvedEvent(adId1, true, startMS, adDurationMS, eCDAI_ERROR_NONE))
      .Times(1)
      .WillOnce(InvokeWithoutArgs([adResolvedPromise1]{ adResolvedPromise1->set_value(); }));

    EXPECT_CALL(*g_mockPrivateInstanceAAMP,
        SendAdResolvedEvent(adId2, true, startMS + adDurationMS, adDurationMS, eCDAI_ERROR_NONE))
      .Times(1)
      .WillOnce(InvokeWithoutArgs([adResolvedPromise2]{ adResolvedPromise2->set_value(); }));
  }

  // Expecting the ad init fragment to be downloaded twice (once for each ad fulfillment).
  std::string adInitUrl = GetFullURI(TEST_AD_MANIFEST_HOST, "manifest/track-video-repid-LE5-tc--header.mp4");
  EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetFile (adInitUrl, _, _, _, _, _, _, _, _, _, _, _, _, _, _))
            .WillRepeatedly(Return(true));
  adInitUrl = GetFullURI(TEST_AD_MANIFEST_HOST, "manifest-eac3/track-audio-repid-DDen-tc--header.mp4");
  EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetFile (adInitUrl, _, _, _, _, _, _, _, _, _, _, _, _, _, _))
            .WillRepeatedly(Return(true));

  // Step 4: fulfill ad1 and validate partial placement state.
  mPrivateCDAIObjectMPD->SetAlternateContents(periodId, adId1, url, startMS, adDurationMS);
  ASSERT_EQ(adResolvedFuture1.wait_for(std::chrono::milliseconds(500)), std::future_status::ready)
      << "Timed out waiting for first SendAdResolvedEvent callback";

  ASSERT_TRUE(mPrivateCDAIObjectMPD->isAdBreakObjectExist(periodId));
  ASSERT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId].ads->size(), 1u);
  EXPECT_TRUE(mPrivateCDAIObjectMPD->mAdBreaks[periodId].ads->at(0).resolved);
  EXPECT_FALSE(mPrivateCDAIObjectMPD->mAdBreaks[periodId].ads->at(0).invalid);
  EXPECT_FALSE(mPrivateCDAIObjectMPD->mAdBreaks[periodId].ads->at(0).placed)
      << "Ad1 should not be marked placed until PlaceAds runs (after NotifyReservationComplete)";
  EXPECT_FALSE(mPrivateCDAIObjectMPD->mAdBreaks[periodId].mAdBreakPlaced)
      << "Ad break should remain incomplete until PlaceAds runs";
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId].adsDuration, adDurationMS);

  // Step 5: fulfill ad2 and validate that ad-break placement is complete.
  mPrivateCDAIObjectMPD->SetAlternateContents(periodId, adId2, url, startMS, adDurationMS);
  ASSERT_EQ(adResolvedFuture2.wait_for(std::chrono::milliseconds(500)), std::future_status::ready)
      << "Timed out waiting for second SendAdResolvedEvent callback";

  ASSERT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId].ads->size(), 2u);
  EXPECT_TRUE(mPrivateCDAIObjectMPD->mAdBreaks[periodId].ads->at(1).resolved);
  EXPECT_FALSE(mPrivateCDAIObjectMPD->mAdBreaks[periodId].ads->at(1).invalid);
  EXPECT_FALSE(mPrivateCDAIObjectMPD->mAdBreaks[periodId].ads->at(1).placed)
      << "Ad2 should not be marked placed until PlaceAds runs (after NotifyReservationComplete)";
  // Ad break placement should still be incomplete until NotifyReservationComplete is called
  EXPECT_FALSE(mPrivateCDAIObjectMPD->mAdBreaks[periodId].mAdBreakPlaced)
      << "Ad break should remain incomplete after second ad is fulfilled but before reservation is marked complete";
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId].adsDuration, breakdur)
      << "Ad break should be completely filled with both 10s ads (total 20s)";

  // Step 6: all ads resolved. Now mark reservation complete, which triggers PlaceAds for static manifest.
  mPrivateCDAIObjectMPD->NotifyReservationComplete(periodId);

  // After PlaceAds runs, both ads should be marked as placed
  EXPECT_TRUE(mPrivateCDAIObjectMPD->mAdBreaks[periodId].ads->at(0).placed)
      << "Ad1 should be marked placed after PlaceAds runs";
  EXPECT_TRUE(mPrivateCDAIObjectMPD->mAdBreaks[periodId].ads->at(1).placed)
      << "Ad2 should be marked placed after PlaceAds runs";
  EXPECT_TRUE(mPrivateCDAIObjectMPD->mAdBreaks[periodId].mAdBreakPlaced)
      << "Ad break should be marked placed after NotifyReservationComplete triggers PlaceAds";

  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId].adBreakId, periodId);
  EXPECT_GT(mPrivateCDAIObjectMPD->mPeriodMap[periodId].duration, 0u);

  // Placement should be complete and pending placement queue should be clear.
  EXPECT_TRUE(mPrivateCDAIObjectMPD->mPlacementObj.pendingAdbrkId.empty());
  EXPECT_TRUE(mPrivateCDAIObjectMPD->mPlacementObj.openPeriodId.empty());
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPlacementObj.curAdIdx, -1);
}

/**
 * @brief Integration test: with no base MPD helper, reservation completion
 *        does not trigger placement even after ad fulfillment resolves.
 *
 * Verifies that after NotifyReservationComplete, placement is skipped when
 * mBaseMPDParseHelper is null and mPeriodMap remains unpopulated.
 */
TEST_F(AdManagerMPDTests, StaticManifest_NoBaseHelper_NotifyComplete_DoesNotPlaceAds)
{
  const std::string periodId = "testPeriodId1";
  const std::string adId     = "testAdId";
  const std::string url      = TEST_AD_MANIFEST_URL;
  constexpr uint64_t startMS  = 0;
  constexpr uint32_t breakdur = 30000;
  auto adResolvedPromise = std::make_shared<std::promise<void>>();
  auto adResolvedFuture = adResolvedPromise->get_future();

  // Step 1: placeholder entry — mBaseMPDParseHelper intentionally NOT set.
  mPrivateCDAIObjectMPD->SetAlternateContents(periodId, "", "", startMS, breakdur);

  // Step 2: mock ad manifest download.
  mManifest = kSharedTenSecondAdManifest;
  EXPECT_CALL(*g_mockPrivateInstanceAAMP,
      GetFile(std::string(TEST_AD_MANIFEST_URL), _, _, _, _, _, _, _, _, _, _, _, _, _, _))
      .WillOnce(WithArgs<0,2,3,4>(Invoke(this, &AdManagerMPDTests::GetManifest)));
  EXPECT_CALL(*g_mockPrivateInstanceAAMP,
      SendAdResolvedEvent(adId, true, startMS, 10000u, eCDAI_ERROR_NONE))
    .Times(1)
    .WillOnce(InvokeWithoutArgs([adResolvedPromise]{ adResolvedPromise->set_value(); }));

  std::string adInitUrl = GetFullURI(TEST_AD_MANIFEST_HOST, "manifest/track-video-repid-LE5-tc--header.mp4");
  EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetFile (adInitUrl, _, _, _, _, _, _, _, _, _, _, _, _, _, _))
            .WillOnce(Return(true));
  adInitUrl = GetFullURI(TEST_AD_MANIFEST_HOST, "manifest-eac3/track-audio-repid-DDen-tc--header.mp4");
  EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetFile (adInitUrl, _, _, _, _, _, _, _, _, _, _, _, _, _, _))
            .WillOnce(Return(true));

  // Step 3: trigger fulfillment.
  mPrivateCDAIObjectMPD->SetAlternateContents(periodId, adId, url, startMS, breakdur);
  ASSERT_EQ(adResolvedFuture.wait_for(std::chrono::milliseconds(500)), std::future_status::ready)
      << "Timed out waiting for SendAdResolvedEvent callback";

  // Step 4: ad resolved. Now call NotifyReservationComplete to trigger PlaceAds (which should be skipped).
  mPrivateCDAIObjectMPD->NotifyReservationComplete(periodId);

  // Step 5: verify reservation was marked complete/resolved, but PlaceAds was
  // NOT called because mBaseMPDParseHelper is null.
  EXPECT_TRUE(mPrivateCDAIObjectMPD->mAdBreaks[periodId].reservationComplete);
  EXPECT_TRUE(mPrivateCDAIObjectMPD->mAdBreaks[periodId].resolved);
  EXPECT_TRUE(mPrivateCDAIObjectMPD->mAdBreaks[periodId].ads->at(0).resolved);
  EXPECT_FALSE(mPrivateCDAIObjectMPD->mAdBreaks[periodId].ads->at(0).invalid);
  EXPECT_FALSE(mPrivateCDAIObjectMPD->mAdBreaks[periodId].ads->at(0).placed)
      << "PlaceAds should NOT have been called when mBaseMPDParseHelper is null";
  EXPECT_FALSE(mPrivateCDAIObjectMPD->mAdBreaks[periodId].mAdBreakPlaced)
      << "adbreak should not be marked placed when PlaceAds was not triggered";

  // mPeriodMap duration must still be zero — PlaceAds never ran.
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId].duration, 0u)
      << "mPeriodMap should not be populated when PlaceAds was skipped";

  // curAdIdx stays at 0 (placement queued but not yet executed by PlaceAds).
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPlacementObj.curAdIdx, 0);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPlacementObj.pendingAdbrkId, periodId);
}

/**
 * @brief Integration test: with base helper set, a failed ad download keeps
 *        placement skipped even after NotifyReservationComplete.
 *
 * Verifies that when ad download fails, the ad remains invalid and PlaceAds is
 * not executed, so mPeriodMap stays empty.
 */
TEST_F(AdManagerMPDTests, StaticManifest_AdDownloadFails_NotifyComplete_DoesNotPlaceAds)
{
  const std::string periodId = "testPeriodId1";
  const std::string adId     = "testAdId";
  const std::string url      = TEST_AD_MANIFEST_URL;
  constexpr uint64_t startMS  = 0;
  constexpr uint32_t breakdur = 30000;
  auto adResolvedPromise = std::make_shared<std::promise<void>>();
  auto adResolvedFuture = adResolvedPromise->get_future();

  // Step 1: placeholder entry.
  mPrivateCDAIObjectMPD->SetAlternateContents(periodId, "", "", startMS, breakdur);

  // Step 2: parse and register the base-stream helper (static manifest gate is open).
  ProcessSourceMPD(kStaticSourceManifest);
  ASSERT_TRUE(mAdMPDParseHelper != nullptr);
  mPrivateCDAIObjectMPD->SetBaseMPDParseHelper(mAdMPDParseHelper);

  // Step 3: make the ad manifest download fail with HTTP 404.
  EXPECT_CALL(*g_mockPrivateInstanceAAMP,
      GetFile(std::string(TEST_AD_MANIFEST_URL), _, _, _, _, _, _, _, _, _, _, _, _, _, _))
      .WillOnce(WithArgs<4>(Invoke([](int& err){ err = 404; return false; })));
  EXPECT_CALL(*g_mockPrivateInstanceAAMP,
      SendAdResolvedEvent(adId, false, 0, 0, eCDAI_ERROR_DELIVERY_HTTP_ERROR))
    .Times(1)
    .WillOnce(InvokeWithoutArgs([adResolvedPromise]{ adResolvedPromise->set_value(); }));

  // Step 4: trigger fulfillment.
  mPrivateCDAIObjectMPD->SetAlternateContents(periodId, adId, url, startMS, breakdur);
  ASSERT_EQ(adResolvedFuture.wait_for(std::chrono::milliseconds(500)), std::future_status::ready)
      << "Timed out waiting for SendAdResolvedEvent callback";

  // Step 5: ad resolved (but marked invalid). Now call NotifyReservationComplete to trigger PlaceAds (which should be skipped due to ad failure).
  mPrivateCDAIObjectMPD->NotifyReservationComplete(periodId);

  // Step 6: verify ad marked invalid; PlaceAds must NOT have been called.
  EXPECT_TRUE(mPrivateCDAIObjectMPD->mAdBreaks[periodId].ads->at(0).resolved);
  EXPECT_TRUE(mPrivateCDAIObjectMPD->mAdBreaks[periodId].ads->at(0).invalid);
  EXPECT_FALSE(mPrivateCDAIObjectMPD->mAdBreaks[periodId].ads->at(0).placed)
      << "PlaceAds must not be called when ad download failed";
  EXPECT_FALSE(mPrivateCDAIObjectMPD->mAdBreaks[periodId].mAdBreakPlaced);

  // mPeriodMap duration must be zero — PlaceAds never ran.
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId].duration, 0u)
      << "mPeriodMap should not be populated when ad download failed";

  // No placement queued (adStatus was false).
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPlacementObj.curAdIdx, -1);
  EXPECT_TRUE(mPrivateCDAIObjectMPD->mAdtoInsertInNextBreakVec.empty());
}

// ---------------------------------------------------------------------------
// VOD CDAI stitching path tests (VPAAMP-657)
// These tests exercise PrivateCDAIObjectMPD state changes driven by
// RegisterVodAdBreak + SetAlternateContents without any network access.
// They verify that:
//   (a) ad URLs are stored on VodAdBreakInfo (not pushed to FulfillAdLoop)
//   (b) AreAllVodAdsResolved / mVodAllAdsResolvedCV behave correctly
//   (c) failed / cancelled breaks are handled gracefully
//   (d) linear CDAI (no RegisterVodAdBreak calls) is completely unaffected
// ---------------------------------------------------------------------------

/**
 * @brief RegisterVodAdBreak then SetAlternateContents stores adId/adUrl on
 *        VodAdBreakInfo and creates a pre-resolved AdNode — FulfillAdLoop
 *        queue must remain empty.
 */
TEST_F(AdManagerMPDTests, VodCdai_SetAlternateContents_StoresUrlOnVodBreakInfo)
{
  const std::string breakId = "preroll-1";
  const std::string adId    = "ad-001";
  const std::string adUrl   = "https://cdn.example.com/ads/ad1/manifest.mpd";

  mPrivateCDAIObjectMPD->RegisterVodAdBreak(VodAdBreakInfo{breakId, 0.0, 30.0, "preroll"});
  mPrivateCDAIObjectMPD->SetAlternateContents(breakId, adId, adUrl, 0, 0);

  // adId and adUrl stored directly on VodAdBreakInfo
  const auto &info = mPrivateCDAIObjectMPD->mVodAdBreaks.at(breakId);
  EXPECT_EQ(info.adId,  adId);
  EXPECT_EQ(info.adUrl, adUrl);

  // mAdBreaks entry created with one pre-resolved AdNode
  ASSERT_TRUE(mPrivateCDAIObjectMPD->isAdBreakObjectExist(breakId));
  const auto &abObj = mPrivateCDAIObjectMPD->mAdBreaks.at(breakId);
  ASSERT_EQ(abObj.ads->size(), 1u);
  EXPECT_TRUE(abObj.ads->at(0).resolved);
  EXPECT_EQ(abObj.ads->at(0).adId,  adId);
  EXPECT_EQ(abObj.ads->at(0).url,   adUrl);
  EXPECT_EQ(abObj.ads->at(0).mpd,   nullptr);

  // FulfillAdLoop queue must be untouched
  std::lock_guard<std::mutex> lk(mPrivateCDAIObjectMPD->mAdFulfillMtx);
  EXPECT_TRUE(mPrivateCDAIObjectMPD->mAdFulfillQ.empty())
      << "VOD ad URLs must NOT be pushed to the FulfillAdLoop queue";
}

/**
 * @brief AreAllVodAdsResolved returns false until every registered break has
 *        received a SetAlternateContents call, then returns true.
 */
TEST_F(AdManagerMPDTests, VodCdai_AreAllVodAdsResolved_SignalsAfterAllBreaks)
{
  mPrivateCDAIObjectMPD->RegisterVodAdBreak(VodAdBreakInfo{"brk-1", 0.0,  15.0, "preroll"});
  mPrivateCDAIObjectMPD->RegisterVodAdBreak(VodAdBreakInfo{"brk-2", 30.0, 15.0, "midroll"});

  EXPECT_FALSE(mPrivateCDAIObjectMPD->AreAllVodAdsResolved())
      << "Should be false before any SetAlternateContents";

  mPrivateCDAIObjectMPD->SetAlternateContents("brk-1", "ad-1", "https://cdn.example.com/ad1.mpd", 0, 0);

  EXPECT_FALSE(mPrivateCDAIObjectMPD->AreAllVodAdsResolved())
      << "Should be false while one break still has no URL";

  mPrivateCDAIObjectMPD->SetAlternateContents("brk-2", "ad-2", "https://cdn.example.com/ad2.mpd", 0, 0);

  EXPECT_TRUE(mPrivateCDAIObjectMPD->AreAllVodAdsResolved())
      << "Should be true once every break has a URL";
}

/**
 * @brief mVodAllAdsResolvedCV is signalled after the last SetAlternateContents
 *        so that BuildStitchedVodManifest's wait_for returns immediately.
 */
TEST_F(AdManagerMPDTests, VodCdai_AllAdsResolvedCV_SignalledAfterLastSetAlternateContents)
{
  mPrivateCDAIObjectMPD->RegisterVodAdBreak(VodAdBreakInfo{"brk-alpha", 0.0, 10.0, "preroll"});

  // Arm a background thread waiting on the CV before SetAlternateContents.
  std::atomic<bool> signalled{false};
  auto fut = std::async(std::launch::async, [&]() {
    std::unique_lock<std::mutex> lk(mPrivateCDAIObjectMPD->mVodAllAdsResolvedMtx);
    signalled = mPrivateCDAIObjectMPD->mVodAllAdsResolvedCV.wait_for(
        lk, std::chrono::seconds(2),
        [&]{ return mPrivateCDAIObjectMPD->AreAllVodAdsResolved(); });
  });

  mPrivateCDAIObjectMPD->SetAlternateContents("brk-alpha", "ad-alpha", "https://cdn.example.com/ad-alpha.mpd", 0, 0);

  fut.get();
  EXPECT_TRUE(signalled)
      << "mVodAllAdsResolvedCV must be signalled by SetAlternateContents";
}

/**
 * @brief SetAlternateContents for an unknown break (not registered via
 *        RegisterVodAdBreak and no prior placeholder call) falls into the
 *        linear CDAI else branch, finds no mAdBreaks entry, and rejects the
 *        ad via SendAdResolvedEvent(false). No mAdBreaks entry is created.
 */
TEST_F(AdManagerMPDTests, VodCdai_SetAlternateContents_DropsUnknownBreak)
{
  // No RegisterVodAdBreak and no prior placeholder call — the linear else branch
  // finds no mAdBreaks entry and rejects the ad via SendAdResolvedEvent(false).
  EXPECT_CALL(*g_mockPrivateInstanceAAMP,
              SendAdResolvedEvent(std::string("ad-X"), false, 0, 0, _))
      .Times(1);

  mPrivateCDAIObjectMPD->SetAlternateContents("non-existent-break", "ad-X",
                                               "https://cdn.example.com/adX.mpd", 0, 0);

  EXPECT_FALSE(mPrivateCDAIObjectMPD->isAdBreakObjectExist("non-existent-break"));
  EXPECT_TRUE(mPrivateCDAIObjectMPD->mVodAdBreaks.empty());
}

/**
 * @brief A cancelled VOD break is skipped by AreAllVodAdsResolved — it must
 *        not block the stitcher even if no SetAlternateContents is received.
 */
TEST_F(AdManagerMPDTests, VodCdai_CancelledBreak_DoesNotBlockResolution)
{
  mPrivateCDAIObjectMPD->RegisterVodAdBreak(VodAdBreakInfo{"brk-ok",     0.0,  10.0, "preroll"});
  mPrivateCDAIObjectMPD->RegisterVodAdBreak(VodAdBreakInfo{"brk-cancel", 30.0, 10.0, "midroll"});

  // Cancel the second break before any SetAlternateContents
  mPrivateCDAIObjectMPD->CancelVodAdBreak("brk-cancel");

  // Satisfy only the non-cancelled break
  mPrivateCDAIObjectMPD->SetAlternateContents("brk-ok", "ad-ok", "https://cdn.example.com/ad-ok.mpd", 0, 0);

  EXPECT_TRUE(mPrivateCDAIObjectMPD->AreAllVodAdsResolved())
      << "Cancelled breaks must not block AreAllVodAdsResolved";
}

/**
 * @brief Linear CDAI (no RegisterVodAdBreak calls) must not be affected by
 *        the new VOD path.  SetAlternateContents with an empty adId/url
 *        (linear placeholder) still creates the mAdBreaks entry as before.
 */
TEST_F(AdManagerMPDTests, VodCdai_LinearCdai_UnaffectedByVodPath)
{
  const std::string periodId = "linear-period-1";

  // Linear CDAI placeholder call (empty adId/url) — must still work
  mPrivateCDAIObjectMPD->SetAlternateContents(periodId, "", "", 0, 30000);

  EXPECT_TRUE(mPrivateCDAIObjectMPD->isAdBreakObjectExist(periodId))
      << "Linear CDAI placeholder must still create mAdBreaks entry";

  // mVodAdBreaks must remain empty — no VOD registration happened
  EXPECT_TRUE(mPrivateCDAIObjectMPD->mVodAdBreaks.empty())
      << "Linear CDAI must not touch mVodAdBreaks";

  // AreAllVodAdsResolved must be false (no VOD breaks registered)
  EXPECT_FALSE(mPrivateCDAIObjectMPD->AreAllVodAdsResolved())
      << "AreAllVodAdsResolved must return false when no VOD breaks registered";
}

/**
 * @brief Registration order recorded in mVodAdBreakOrder is preserved
 *        regardless of map key ordering, ensuring FetchAdMPDsParallel
 *        iterates breaks in insertion order.
 */
TEST_F(AdManagerMPDTests, VodCdai_BreakRegistrationOrder_Preserved)
{
  // Register in a non-lexicographic order
  mPrivateCDAIObjectMPD->RegisterVodAdBreak(VodAdBreakInfo{"zzz-post",  120.0, 10.0, "postroll"});
  mPrivateCDAIObjectMPD->RegisterVodAdBreak(VodAdBreakInfo{"aaa-pre",     0.0, 10.0, "preroll"});
  mPrivateCDAIObjectMPD->RegisterVodAdBreak(VodAdBreakInfo{"mmm-mid",    60.0, 10.0, "midroll"});

  const auto &order = mPrivateCDAIObjectMPD->mVodAdBreakOrder;
  ASSERT_EQ(order.size(), 3u);
  EXPECT_EQ(order[0], "zzz-post");
  EXPECT_EQ(order[1], "aaa-pre");
  EXPECT_EQ(order[2], "mmm-mid");
}
