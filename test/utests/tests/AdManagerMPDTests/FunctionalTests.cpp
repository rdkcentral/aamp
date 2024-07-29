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
#include <thread>
#include <chrono>
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

using namespace dash::xml;
using namespace dash::mpd;

AampConfig *gpGlobalConfig{nullptr};
AampLogManager *mLogObj{nullptr};

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
  static constexpr const char *TEST_AD_MANIFEST_URL = "http://host/ad/manifest.mpd";
  static constexpr const char *TEST_FOG_AD_MANIFEST_URL = "http://127.0.0.1:9080/adrec?clientId=FOG_AAMP&recordedUrl=http%3A%2F%2Fhost%2Fad%2Fmanifest.mpd";
  static constexpr const char *TEST_FOG_MAIN_MANIFEST_URL = "http://127.0.0.1:9080/recording/manifest.mpd";

  void SetUp()
  {
    if(gpGlobalConfig == nullptr)
    {
      gpGlobalConfig =  new AampConfig();
    }

    mPrivateInstanceAAMP = new PrivateInstanceAAMP(gpGlobalConfig);
    mLogObj = new AampLogManager();

    mCdaiObj = new CDAIObjectMPD(mLogObj, mPrivateInstanceAAMP);
    mPrivateCDAIObjectMPD = mCdaiObj->GetPrivateCDAIObjectMPD();

    g_mockPrivateInstanceAAMP = new StrictMock<MockPrivateInstanceAAMP>();

    mManifest = nullptr;
    mMPD = nullptr;
  }

  void TearDown()
  {
    delete mPrivateInstanceAAMP;
    mPrivateInstanceAAMP = nullptr;

    delete mCdaiObj;
    mCdaiObj = nullptr;
    mPrivateCDAIObjectMPD = nullptr;

    delete mLogObj;
    mLogObj = nullptr;

    delete g_mockPrivateInstanceAAMP;
    g_mockPrivateInstanceAAMP = nullptr;

    delete gpGlobalConfig;
    gpGlobalConfig = nullptr;

    if (mMPD)
    {
      delete mMPD;
      mMPD = nullptr;
    }

    mManifest = nullptr;
  }

public:
  bool GetManifest(std::string remoteUrl, AampGrowableBuffer *buffer, std::string& effectiveUrl, int *httpError)
  {
    /* Setup fake AampGrowableBuffer contents. */
    buffer->Clear();
    buffer->AppendBytes((char *)mManifest, strlen(mManifest));
    effectiveUrl = remoteUrl;
    *httpError = 200;

    return true;
  }

  void InitializeAdMPD(const char *manifest, bool isFOG = false, bool fogDownloadSuccess = true)
  {
    std::string adManifestUrl = TEST_AD_MANIFEST_URL;
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, DownloadsAreEnabled()).WillRepeatedly(Return(true));
    if (manifest)
    {
      mManifest = manifest;
      // remoteUrl, manifest, effectiveUrl
      EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetFile (adManifestUrl, _, _, _, _, _, _, _, _, _, _, _))
              .WillOnce(WithArgs<0,2,3,4>(Invoke(this, &AdManagerMPDTests::GetManifest)));
      if (isFOG)
      {
        // If the ADs are to be recorded by FOG, then the manifest will be downloaded again from FOG
        // remoteUrl, manifest, effectiveUrl, httpError
        std::string adFogManifestUrl = TEST_FOG_AD_MANIFEST_URL;
        if (fogDownloadSuccess)
        {
          EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetFile (adFogManifestUrl, _, _, _, _, _, _, _, _, _, _, _))
              .WillOnce(WithArgs<0,2,3,4>(Invoke(this, &AdManagerMPDTests::GetManifest)));;
        }
        else
        {
          EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetFile (adFogManifestUrl, _, _, _, _, _, _, _, _, _, _, _))
              .WillOnce(Return(false));
        }
      }
    }
    else
    {
      EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetFile (adManifestUrl, _, _, _, _, _, _, _, _, _, _, _))
              .WillOnce(Return(false));
    }
  }

  void InitializeAdMPDObject(const char* manifest)
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
    }
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
 * @brief Tests the functionality of the setPlacementObj function when adBrkId equals endPeriodId.
 * 
 * This test case verifies that the `setPlacementObj` function correctly sets the next placement object
 * when the ad break ID is the same as the end period ID.
 * 
 * @note This test case is part of the AdManagerMPDTests fixture.
 */
TEST_F(AdManagerMPDTests, SetPlacementObjTest)
{
  // Empty mAdtoInsertInNextBreakVec
  // Call the function to test
  PlacementObj nextPlacementObj = mPrivateCDAIObjectMPD->setPlacementObj("testAdBrkId1", "testAdBrkId1");
  // Verify the result
  EXPECT_EQ(nextPlacementObj.pendingAdbrkId, "");

  // Call the function to test
  nextPlacementObj = mPrivateCDAIObjectMPD->setPlacementObj("testAdBrkId1", "testAdBrkId2");
  // Verify the result
  EXPECT_EQ(nextPlacementObj.pendingAdbrkId, "");

  // Initialize mAdtoInsertInNextBreakVec
  mPrivateCDAIObjectMPD->mAdtoInsertInNextBreakVec = {
      {"testAdBrkId1", "testAdBrkId1", 0, 0, 0},
      {"testAdBrkId2", "testAdBrkId2", 0, 0, 0},
      {"testAdBrkId3", "testAdBrkId3", 0, 0, 0}
  };

  // Call the function to test when adBrkId equals endPeriodId
  nextPlacementObj = mPrivateCDAIObjectMPD->setPlacementObj("testAdBrkId1", "testAdBrkId1");
  // Verify the result
  EXPECT_EQ(nextPlacementObj.pendingAdbrkId, "testAdBrkId2");


  // Call the function to test when adBrkId not equals to endPeriodId
  nextPlacementObj = mPrivateCDAIObjectMPD->setPlacementObj("testAdBrkId2", "testAdBrkId3");
  // Verify the result
  EXPECT_EQ(nextPlacementObj.pendingAdbrkId, "testAdBrkId3");

  // Call the function to test endPeriodId not available in mAdtoInsertInNextBreakVec
  nextPlacementObj = mPrivateCDAIObjectMPD->setPlacementObj("testAdBrkId2", "testAdBrkId4");
  // Verify the result
  EXPECT_EQ(nextPlacementObj.pendingAdbrkId, "testAdBrkId3");

  // Call the function to test endPeriodId not available and end of mAdtoInsertInNextBreakVec
  nextPlacementObj = mPrivateCDAIObjectMPD->setPlacementObj("testAdBrkId3", "testAdBrkId4");
  // Verify the result
  EXPECT_EQ(nextPlacementObj.pendingAdbrkId, "");

  // Call the function to test adBrkId equals endPeriodId and end of mAdtoInsertInNextBreakVec
  nextPlacementObj = mPrivateCDAIObjectMPD->setPlacementObj("testAdBrkId3", "testAdBrkId3");
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

  // Call the function to test
  mPrivateCDAIObjectMPD->SetAlternateContents(periodId, adId, url, startMS, breakdur);

  // Verify the result
  EXPECT_TRUE(mPrivateCDAIObjectMPD->isAdBreakObjectExist(periodId));
  EXPECT_FALSE(mPrivateCDAIObjectMPD->mAdObjThreadStarted);

  // New periodId which is not present in mAdBreaks
  periodId = "testPeriodId1";
  adId = "testAdId1";
  url = "testAdUrl1";

  EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendAdResolvedEvent(adId, false, 0, 0)).Times(1);
  // Call the function to test when adbreak object doesn't exist and adId and url not empty
  mPrivateCDAIObjectMPD->SetAlternateContents(periodId, adId, url, startMS, breakdur);

  // Verify the result
  EXPECT_FALSE(mPrivateCDAIObjectMPD->isAdBreakObjectExist(periodId));
  EXPECT_FALSE(mPrivateCDAIObjectMPD->mAdObjThreadStarted);

}

/**
 * @brief Tests the functionality of the SetAlternateContents method when adId and url are not empty and ad break object exists.
 */
TEST_F(AdManagerMPDTests, SetAlternateContentsTests_2)
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
  mPrivateCDAIObjectMPD->SetAlternateContents(periodId, adId, "", startMS, breakdur);

  url = TEST_AD_MANIFEST_URL;
  InitializeAdMPD(manifest);

  // mIsFogTSB is false, so downloaded from CDN and ad resolved event is sent
  EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendAdResolvedEvent(adId, true, startMS, 10000)).Times(1);
  mPrivateCDAIObjectMPD->SetAlternateContents(periodId, adId, url, startMS, breakdur);
  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  // Verify the result
  // mAdBreak updated and placementObj created
  EXPECT_TRUE(mPrivateCDAIObjectMPD->mAdObjThreadStarted);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPlacementObj.pendingAdbrkId, periodId);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdtoInsertInNextBreakVec.size(), 1);
  EXPECT_EQ((mPrivateCDAIObjectMPD->mAdBreaks[periodId].ads)->size(), 1);

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

  // To create an empty ad break object
  mPrivateCDAIObjectMPD->SetAlternateContents(periodId, adId, "", startMS, breakdur);

  url = TEST_AD_MANIFEST_URL;
  mPrivateCDAIObjectMPD->mIsFogTSB = true;
  mPrivateInstanceAAMP->SetManifestUrl(TEST_FOG_MAIN_MANIFEST_URL);
  InitializeAdMPD(manifest, true);

  // mIsFogTSB is false, so downloaded from CDN and ad resolved event is sent
  EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendAdResolvedEvent(adId, true, startMS, 10000)).Times(1);
  mPrivateCDAIObjectMPD->SetAlternateContents(periodId, adId, url, startMS, breakdur);
  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  // Verify the result
  // mAdBreak updated and placementObj created
  EXPECT_TRUE(mPrivateCDAIObjectMPD->mAdObjThreadStarted);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPlacementObj.pendingAdbrkId, periodId);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdtoInsertInNextBreakVec.size(), 1);
  EXPECT_EQ((mPrivateCDAIObjectMPD->mAdBreaks[periodId].ads)->size(), 1);

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

  // To create an empty ad break object
  mPrivateCDAIObjectMPD->SetAlternateContents(periodId, adId, "", startMS, breakdur);

  url = TEST_AD_MANIFEST_URL;
  InitializeAdMPD(manifest);

  // mIsFogTSB is false, so downloaded from CDN and ad resolved event status should be false
  EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendAdResolvedEvent(adId, false, 0, 0)).Times(1);
  mPrivateCDAIObjectMPD->SetAlternateContents(periodId, adId, url, startMS, breakdur);
  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  // Verify the result
  // mAdBreak updated and placementObj created
  EXPECT_TRUE(mPrivateCDAIObjectMPD->mAdObjThreadStarted);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPlacementObj.pendingAdbrkId, "");
  EXPECT_TRUE(mPrivateCDAIObjectMPD->mAdtoInsertInNextBreakVec.empty());
  EXPECT_TRUE((mPrivateCDAIObjectMPD->mAdBreaks[periodId].ads)->empty());

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
  mPrivateCDAIObjectMPD->SetAlternateContents(periodId, adId, "", startMS, breakdur);

  url = TEST_AD_MANIFEST_URL;
  mPrivateCDAIObjectMPD->mIsFogTSB = true;
  mPrivateInstanceAAMP->SetManifestUrl(TEST_FOG_MAIN_MANIFEST_URL);
  InitializeAdMPD(manifest, true, false);

  // mIsFogTSB is false, so downloaded from CDN and ad resolved event is sent
  EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendAdResolvedEvent(adId, true, startMS, 10000)).Times(1);
  mPrivateCDAIObjectMPD->SetAlternateContents(periodId, adId, url, startMS, breakdur);
  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  // Verify the result
  // mAdBreak updated and placementObj created
  EXPECT_TRUE(mPrivateCDAIObjectMPD->mAdObjThreadStarted);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPlacementObj.pendingAdbrkId, periodId);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdtoInsertInNextBreakVec.size(), 1);
  EXPECT_EQ((mPrivateCDAIObjectMPD->mAdBreaks[periodId].ads)->size(), 1);

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
          std::make_pair (30000, AdOnPeriod(1, 30000)) // for adId2 idx=1, offset=0s
        });
    // Add ads to the adBreak
    mPrivateCDAIObjectMPD->mAdBreaks["testPeriodId"].ads = std::make_shared<std::vector<AdNode>>();
    mPrivateCDAIObjectMPD->mAdBreaks["testPeriodId"].ads->emplace_back(false, false, "adId1", "url", 30000, "testPeriodId", 0, nullptr);
    mPrivateCDAIObjectMPD->mAdBreaks["testPeriodId"].ads->emplace_back(false, false, "adId2", "url", 30000, "testPeriodId", 30000, nullptr);

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
  InitializeAdMPDObject(manifest);
  mPrivateCDAIObjectMPD->PlaceAds(mMPD);
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
  InitializeAdMPDObject(manifest);
  // Set curEndNumber to 0
  mPrivateCDAIObjectMPD->mPlacementObj = PlacementObj(periodId, periodId, 0, 0, 0);

  // Add ads to the adBreak
  mPrivateCDAIObjectMPD->mAdBreaks = {
    {periodId, AdBreakObject(30000, std::make_shared<std::vector<AdNode>>(), "", 0, 30000)}
  };
  mPrivateCDAIObjectMPD->mAdBreaks[periodId].ads->emplace_back(false, false, "adId1", "url", 30000, periodId, 0, nullptr);

  // Add ads to mPeriodMap. mPeriodMap[periodId].adBreakId is non-empty for live at the beginning as per SetAlternateContents
  mPrivateCDAIObjectMPD->mPeriodMap[periodId] = Period2AdData(false, periodId, 0 /*in ms*/,
    {
      std::make_pair (0, AdOnPeriod(0, 0)), // for adId1 idx=0, offset=0s
    });
  mPrivateCDAIObjectMPD->PlaceAds(mMPD);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPlacementObj.curEndNumber, 3);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId].duration, 6000); // in ms

  // Update with same mpd again, and the params should not change
  mPrivateCDAIObjectMPD->PlaceAds(mMPD);
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
  InitializeAdMPDObject(manifest);
  // Set curEndNumber to 13, adNextOffset = (13)*2000
  mPrivateCDAIObjectMPD->mPlacementObj = PlacementObj(periodId, periodId, 13, 0, 26000);

  // Add ads to the adBreak
  mPrivateCDAIObjectMPD->mAdBreaks = {
    {periodId, AdBreakObject(30000, std::make_shared<std::vector<AdNode>>(), "", 0, 30000)}
  };
  mPrivateCDAIObjectMPD->mAdBreaks[periodId].ads->emplace_back(false, false, "adId1", "url", 30000, periodId, 0, nullptr);

  // Add ads to mPeriodMap. mPeriodMap[periodId].adBreakId is non-empty for live at the beginning as per SetAlternateContents
  mPrivateCDAIObjectMPD->mPeriodMap[periodId] = Period2AdData(false, periodId, 26000 /*in ms*/,
    {
      std::make_pair (0, AdOnPeriod(0, 0)), // for adId1 idx=0, offset=0s
    });
  mPrivateCDAIObjectMPD->PlaceAds(mMPD);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId].ads->at(0).placed, true);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId].endPeriodOffset, 0);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId].endPeriodId, "testPeriodId2"); // next period
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPlacementObj.curAdIdx, -1); // since no new placementObj exists
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPlacementObj.curEndNumber, 0);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId].duration, 30000); // in ms
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
  InitializeAdMPDObject(manifest);
  // Set curEndNumber to 13, adNextOffset = (13)*2000
  mPrivateCDAIObjectMPD->mPlacementObj = PlacementObj(periodId1, periodId1, 13, 0, 26000);
  mPrivateCDAIObjectMPD->mAdtoInsertInNextBreakVec.emplace_back(periodId2, periodId2, 0, 0, 0); // second ad break in vector

  // Add ads to the adBreak
  mPrivateCDAIObjectMPD->mAdBreaks = {
    {periodId1, AdBreakObject(30000, std::make_shared<std::vector<AdNode>>(), "", 0, 30000)},
    {periodId2, AdBreakObject(30000, std::make_shared<std::vector<AdNode>>(), "", 0, 30000)}
  };
  mPrivateCDAIObjectMPD->mAdBreaks[periodId1].ads->emplace_back(false, false, "adId1", "url1", 30000, periodId1, 0, nullptr);
  mPrivateCDAIObjectMPD->mAdBreaks[periodId2].ads->emplace_back(false, false, "adId2", "url2", 30000, periodId2, 0, nullptr);

  // Add ads to mPeriodMap. mPeriodMap[periodId].adBreakId is non-empty for live at the beginning as per SetAlternateContents
  mPrivateCDAIObjectMPD->mPeriodMap[periodId1] = Period2AdData(false, periodId1, 26000 /*in ms*/,
    {
      std::make_pair (0, AdOnPeriod(0, 0)), // for adId1 idx=0, offset=0s
    });
  mPrivateCDAIObjectMPD->mPeriodMap[periodId2] = Period2AdData(false, periodId2, 0 /*in ms*/,
    {
      std::make_pair (0, AdOnPeriod(0, 0)), // for adId1 idx=0, offset=0s
    });
  mPrivateCDAIObjectMPD->PlaceAds(mMPD);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].ads->at(0).placed, true);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].endPeriodOffset, 0);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].endPeriodId, "testPeriodId2"); // next period

  EXPECT_EQ(mPrivateCDAIObjectMPD->mPlacementObj.curAdIdx, 0);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPlacementObj.pendingAdbrkId, periodId2);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId1].duration, 30000); // in ms
  // periodId2 is not updated in the current logic
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId2].duration, 0); // in ms
}

/**
 * @brief Tests the functionality of the PlaceAds method when isSrcdurnotequalstoaddur is true
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
  InitializeAdMPDObject(manifest);
  // Set curEndNumber to 15, adNextOffset = (15)*2000
  mPrivateCDAIObjectMPD->mPlacementObj = PlacementObj(periodId1, periodId1, 15, 0, 30000);
  mPrivateCDAIObjectMPD->mAdtoInsertInNextBreakVec.emplace_back(periodId2, periodId2, 0, 0, 0); // second ad break in vector

  // Add ads to the adBreak
  // testPeriodId1 ad duration is set to 35000 to force mismatch for isSrcdurnotequalstoaddur
  mPrivateCDAIObjectMPD->mAdBreaks = {
    {periodId1, AdBreakObject(30000, std::make_shared<std::vector<AdNode>>(), "", 0, 35000)},
    {periodId2, AdBreakObject(30000, std::make_shared<std::vector<AdNode>>(), "", 0, 30000)}
  };
  // 1 - to - 1 mapping of ad and period
  mPrivateCDAIObjectMPD->mAdBreaks[periodId1].ads->emplace_back(false, false, "adId1", "url1", 35000, periodId1, 0, nullptr);
  mPrivateCDAIObjectMPD->mAdBreaks[periodId2].ads->emplace_back(false, false, "adId2", "url2", 30000, periodId2, 0, nullptr);

  // Add ads to mPeriodMap. mPeriodMap[periodId].adBreakId is non-empty for live at the beginning as per SetAlternateContents
  mPrivateCDAIObjectMPD->mPeriodMap[periodId1] = Period2AdData(false, periodId1, 30000 /*in ms*/,
    {
      std::make_pair (0, AdOnPeriod(0, 0)), // for adId1 idx=0, offset=0s
    });
  mPrivateCDAIObjectMPD->mPeriodMap[periodId2] = Period2AdData(false, periodId2, 0 /*in ms*/,
    {
      std::make_pair (0, AdOnPeriod(0, 0)), // for adId1 idx=0, offset=0s
    });
  mPrivateCDAIObjectMPD->PlaceAds(mMPD);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].ads->at(0).placed, true);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].endPeriodOffset, 0);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].endPeriodId, "testPeriodId2"); // next period

  EXPECT_EQ(mPrivateCDAIObjectMPD->mPlacementObj.curAdIdx, 0);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPlacementObj.pendingAdbrkId, periodId2);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId1].duration, 30000); // in ms
  // periodId2 is not updated in the current logic
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId2].duration, 0); // in ms
}

/**
 * @brief Tests the functionality of the PlaceAds method when multiple ads are prsent for a single adbreak
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
  InitializeAdMPDObject(manifest1);
  // Set curEndNumber to 14, adNextOffset = (14)*2000
  // Currently placing adId1
  mPrivateCDAIObjectMPD->mPlacementObj = PlacementObj(periodId1, periodId1, 14, 0, 28000);

  // Add ads to the adBreak
  // testPeriodId1 ad duration is set to 35000 to force mismatch for isSrcdurnotequalstoaddur
  mPrivateCDAIObjectMPD->mAdBreaks = {
    {periodId1, AdBreakObject(60000, std::make_shared<std::vector<AdNode>>(), "", 0, 60000)},
  };
  // 1 - to - 2 mapping of ad (ad1 30s, ad2 30s) and period (periodId1)
  mPrivateCDAIObjectMPD->mAdBreaks[periodId1].ads->emplace_back(false, false, "adId1", "url1", 30000, periodId1, 0, nullptr);
  // In FulFillAdObject the second ads, basePeriodID is not populated
  mPrivateCDAIObjectMPD->mAdBreaks[periodId1].ads->emplace_back(false, false, "adId2", "url2", 30000, "", 0, nullptr);

  // Add ads to mPeriodMap. mPeriodMap[periodId].adBreakId is non-empty for live at the beginning as per SetAlternateContents
  // Second ads AdOnPeriod is not populated in FulFillAdObject
  mPrivateCDAIObjectMPD->mPeriodMap[periodId1] = Period2AdData(false, periodId1, 28000 /*in ms*/,
    {
      std::make_pair (0, AdOnPeriod(0, 0)), // for adId1 idx=0, offset=0s
    });
  mPrivateCDAIObjectMPD->PlaceAds(mMPD);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].ads->at(0).placed, true);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].endPeriodOffset, 0);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].endPeriodId, ""); // placement not completed

  EXPECT_EQ(mPrivateCDAIObjectMPD->mPlacementObj.curAdIdx, 1);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPlacementObj.pendingAdbrkId, periodId1);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId1].duration, 34000); // in ms
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId1].offset2Ad[0].adIdx, 0);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId1].offset2Ad[30000].adIdx, 1);

  // next refresh and both ads to be completely placed
  InitializeAdMPDObject(manifest2);
  mPrivateCDAIObjectMPD->PlaceAds(mMPD);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].ads->at(1).placed, true);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].endPeriodOffset, 0);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mAdBreaks[periodId1].endPeriodId, periodId2); // placement not completed

  EXPECT_EQ(mPrivateCDAIObjectMPD->mPlacementObj.curAdIdx, -1);
  EXPECT_EQ(mPrivateCDAIObjectMPD->mPeriodMap[periodId1].duration, 60000); // in ms
}