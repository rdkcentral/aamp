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
#include <chrono>
#include "priv_aamp.h"
#include "AampConfig.h"
#include "AampUtils.h"
#include "AampLogManager.h"
#include "admanager_mpd.h"
#include "fragmentcollector_mpd.h"
#include "MockPrivateInstanceAAMP.h"
#include "MockAampUtils.h"
#include "MockAampMPDParseHelper.h"

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
using ::testing::NiceMock;
using ::testing::Field;
using ::testing::StrEq;

using namespace dash::xml;
using namespace dash::mpd;

AampConfig *gpGlobalConfig{nullptr};

/**
 * @brief FindTimedMetadataTests tests common base class.
 */
class FindTimedMetadataTests : public ::testing::Test
{
protected:

    class TestableStreamAbstractionAAMP_MPD : public StreamAbstractionAAMP_MPD
    {
    public:
        // Constructor to pass parameters to the base class constructor
        TestableStreamAbstractionAAMP_MPD(PrivateInstanceAAMP *aamp)
                : StreamAbstractionAAMP_MPD(aamp, 0, 0)
        {
        }

        void InvokeFindTimedMetadata(MPD* mpd, Node* root, bool init, bool reportBulkMeta)
        {
            FindTimedMetadata(mpd, root, init, reportBulkMeta);
        }

        void SetIsLiveManifest(bool isLive)
        {
            mIsLiveManifest = isLive;
        }

        void SetMPDParseHelper(AampMPDParseHelperPtr helper)
        {
            mMPDParseHelper = helper;
        }

        void SetAvailabilityStartTime(double availabilityStartTime)
        {
            mAvailabilityStartTime = availabilityStartTime;
        }
    };

    PrivateInstanceAAMP *mPrivateInstanceAAMP;
    TestableStreamAbstractionAAMP_MPD *mStreamAbstractionAAMP_MPD;
    CDAIObject *mCdaiObj;
    const char *mManifest;
    static constexpr const char *TEST_BASE_URL = "http://host/asset/";
    static constexpr const char *TEST_MANIFEST_URL = "http://host/asset/manifest.mpd";
    MPD* mMPD = nullptr;
    Node *mRootNode = nullptr;

    void SetUp()
    {
        if(gpGlobalConfig == nullptr)
        {
            gpGlobalConfig =  new AampConfig();
        }

        mPrivateInstanceAAMP = new PrivateInstanceAAMP(gpGlobalConfig);

        g_mockPrivateInstanceAAMP = new StrictMock<MockPrivateInstanceAAMP>();

        g_mockAampUtils = new NiceMock<MockAampUtils>();

        g_mockAampMPDParseHelper = new NiceMock<MockAampMPDParseHelper>();

        mStreamAbstractionAAMP_MPD = nullptr;

        mManifest = nullptr;
        mMPD = nullptr;
        mRootNode = nullptr;
        mCdaiObj = nullptr;
    }

    void TearDown()
    {
        if (mStreamAbstractionAAMP_MPD)
        {
            delete mStreamAbstractionAAMP_MPD;
            mStreamAbstractionAAMP_MPD = nullptr;
        }

        delete mCdaiObj;
        mCdaiObj = nullptr;

        delete mPrivateInstanceAAMP;
        mPrivateInstanceAAMP = nullptr;

        delete gpGlobalConfig;
        gpGlobalConfig = nullptr;

        delete g_mockPrivateInstanceAAMP;
        g_mockPrivateInstanceAAMP = nullptr;

        delete g_mockAampUtils;
        g_mockAampUtils = nullptr;

        delete g_mockAampMPDParseHelper;
        g_mockAampMPDParseHelper = nullptr;

        mManifest = nullptr;
        if (mMPD)
            delete(mMPD);
        if (mRootNode)
            delete(mRootNode);
    }

public:

    /**
     * @brief Get MPD instance from manifest
     */
    void GetMPDFromManifest()
    {
        std::string manifestStr = mManifest;

        xmlTextReaderPtr reader = xmlReaderForMemory( (char *)manifestStr.c_str(), (int) manifestStr.length(), NULL, NULL, 0);
        if (reader != NULL)
        {
            if (xmlTextReaderRead(reader))
            {
                mRootNode = MPDProcessNode(&reader, TEST_MANIFEST_URL);
                if(mRootNode != NULL)
                {
                    mMPD = mRootNode->ToMPD();
                }
            }
        }
        xmlFreeTextReader(reader);
    }

    /**
     * @brief Initialize the MPD instance
     *
     * This will:
     *  - Create the MPD and Root node instance from manifest.
     *  - Initialize the StreamAbstractionAAMP_MPD instance.
     *
     * @param[in] manifest Manifest data
     */
    void InitializeMPD(const char *manifest)
    {
        mManifest = manifest;

        /* Create MPD instance. */
        mStreamAbstractionAAMP_MPD = new TestableStreamAbstractionAAMP_MPD(mPrivateInstanceAAMP);
        EXPECT_CALL(*g_mockPrivateInstanceAAMP, DownloadsAreEnabled()).WillRepeatedly(Return(true));
        ResetCDAIAdObject();

        mPrivateInstanceAAMP->SetManifestUrl(TEST_MANIFEST_URL);
        GetMPDFromManifest();
    }

    /**
     * @brief Reset the CDAI Ad object
     */
    void ResetCDAIAdObject()
    {
        if (mCdaiObj)
        {
            delete mCdaiObj;
            mCdaiObj = nullptr;
        }
        mCdaiObj = new CDAIObjectMPD(mPrivateInstanceAAMP);
        mStreamAbstractionAAMP_MPD->SetCDAIObject(mCdaiObj);
    }

};

TEST_F(FindTimedMetadataTests, LinearSCTE35EventsInPeriod)
{
    static const char *manifest =
R"(<?xml version="1.0" encoding="UTF-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" xmlns:scte35="urn:scte:scte35:2014:xml+bin" xmlns:scte214="scte214" xmlns:cenc="urn:mpeg:cenc:2013" xmlns:mspr="mspr" type="dynamic" id="8371500471198371163" profiles="urn:mpeg:dash:profile:isoff-live:2011,http://www.dashif.org/guidelines/low-latency-live-v5" minBufferTime="PT0H0M1.000S" maxSegmentDuration="PT2.34S" minimumUpdatePeriod="PT0H0M1.920S" availabilityStartTime="1970-01-01T00:00:00.000Z" timeShiftBufferDepth="PT0H30M1.044S" publishTime="2024-06-25T11:23:17.130Z">
  <Period id="Period-1" start="PT477586H51M45.467S">
    <EventStream schemeIdUri="urn:scte:scte35:2014:xml+bin" timescale="90000" presentationTimeOffset="79441464098">
      <Event presentationTime="79441464098" duration="2440800">
        <scte35:Signal>
          <scte35:Binary>/DAsAAAQdSsYAP/wBQb+3zKJFQAWAhRDVUVJAAAkQn/AAABOcUAAACIAAJR2FfM=</scte35:Binary>
        </scte35:Signal>
      </Event>
      <Event presentationTime="79455172898" duration="0">
        <scte35:Signal>
          <scte35:Binary>/DAnAAAQdSsYAP/wBQb+34D6VQARAg9DVUVJAAAkQn+AAAAjAAAJmX3z</scte35:Binary>
        </scte35:Signal>
      </Event>
    </EventStream>
    <AdaptationSet id="track-1" contentType="video" mimeType="video/mp4" segmentAlignment="true" startWithSAP="1">
      <Role schemeIdUri="urn:mpeg:dash:role:2011" value="main"/>
      <SegmentTemplate initialization="track-video-periodid-Period-1-repid-$RepresentationID$-tc-0-header.mp4" media="track-video-periodid-Period-1-repid-$RepresentationID$-tc-0-time-$Time$.mp4" timescale="240000" startNumber="168440" presentationTimeOffset="79455172898" availabilityTimeOffset="1.44" availabilityTimeComplete="false">
        <SegmentTimeline>
          <S t="79476480098" d="460800" r="810"/>
          <S t="79850188898" d="364800" r="0"/>
          <S t="79850553698" d="360000" r="0"/>
        </SegmentTimeline>
      </SegmentTemplate>
      <Representation id="track-2" bandwidth="500000" codecs="hvc1.1.6.L63.90" width="640" height="360" frameRate="25"/>
      <Representation id="track-3" bandwidth="1200000" codecs="hvc1.1.6.L93.90" width="960" height="540" frameRate="50"/>
      <Representation id="track-4" bandwidth="1850000" codecs="hvc1.1.6.L93.90" width="960" height="540" frameRate="50"/>
    </AdaptationSet>
  </Period>
</MPD>
)";
    std::string adBreakId = "Period-1";
    EXPECT_CALL(*g_mockAampUtils, parseAndValidateSCTE35(_)).WillRepeatedly(Return(true));

    // LiveManifest=true and init=true
    InitializeMPD(manifest);
    mStreamAbstractionAAMP_MPD->SetIsLiveManifest(true);
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, FoundEventBreak(adBreakId,_,_)).Times(0);
    mStreamAbstractionAAMP_MPD->InvokeFindTimedMetadata(mMPD, mRootNode, true, false);

    // LiveManifest=true and init=false
    ResetCDAIAdObject();
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, FoundEventBreak(adBreakId,_,Field(&EventBreakInfo::duration, 27120))).Times(1);
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, SaveNewTimedMetadata(_,StrEq(adBreakId.c_str()),27120.0)).Times(1);
    mStreamAbstractionAAMP_MPD->InvokeFindTimedMetadata(mMPD, mRootNode, false, false);

    // Duplicate Periods are not processed
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, FoundEventBreak(adBreakId,_,_)).Times(0);
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, SaveNewTimedMetadata(_,_,_)).Times(0);
    mStreamAbstractionAAMP_MPD->InvokeFindTimedMetadata(mMPD, mRootNode, false, false);
}

TEST_F(FindTimedMetadataTests, VODSCTE35EventsInPeriod)
{
    static const char *manifest =
R"(<?xml version="1.0" encoding="UTF-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" xmlns:scte35="urn:scte:scte35:2014:xml+bin" xmlns:scte214="scte214" xmlns:cenc="urn:mpeg:cenc:2013" xmlns:mspr="mspr" type="dynamic" id="8371500471198371163" profiles="urn:mpeg:dash:profile:isoff-live:2011,http://www.dashif.org/guidelines/low-latency-live-v5" minBufferTime="PT0H0M1.000S" maxSegmentDuration="PT2.34S" minimumUpdatePeriod="PT0H0M1.920S" availabilityStartTime="1970-01-01T00:00:00.000Z" timeShiftBufferDepth="PT0H30M1.044S" publishTime="2024-06-25T11:23:17.130Z">
  <Period id="Period-1" start="PT0S">
    <EventStream schemeIdUri="urn:scte:scte35:2014:xml+bin" timescale="2500" presentationTimeOffset="0">
      <Event presentationTime="0" duration="75000">
        <scte35:Signal>
          <scte35:Binary>/DAsAAAQdSsYAP/wBQb+3zKJFQAWAhRDVUVJAAAkQn/AAABOcUAAACIAAJR2FfM=</scte35:Binary>
        </scte35:Signal>
      </Event>
      <Event presentationTime="0" duration="0">
        <scte35:Signal>
          <scte35:Binary>/DAnAAAQdSsYAP/wBQb+34D6VQARAg9DVUVJAAAkQn+AAAAjAAAJmX3z</scte35:Binary>
        </scte35:Signal>
      </Event>
    </EventStream>
    <SegmentTemplate timescale="2500" initialization="video_p0_init.mp4" media="video_p0_$Number$.m4s" startNumber="1">
      <SegmentTimeline>
        <S t="0" d="5000" r="14" />
      </SegmentTimeline>
    </SegmentTemplate>
    <AdaptationSet id="0" contentType="video">
      <Representation id="0" mimeType="video/mp4" codecs="avc1.640028" bandwidth="800000" width="640" height="360" frameRate="25"/>
    </AdaptationSet>
  </Period>
</MPD>
)";
    std::string adBreakId = "Period-1";
    EXPECT_CALL(*g_mockAampUtils, parseAndValidateSCTE35(_)).WillRepeatedly(Return(true));

    // LiveManifest=false and init=true
    InitializeMPD(manifest);
    mStreamAbstractionAAMP_MPD->SetIsLiveManifest(false);
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, FoundEventBreak(adBreakId,_,Field(&EventBreakInfo::duration, 30000))).Times(1);
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, FoundEventBreak(adBreakId,_,Field(&EventBreakInfo::duration, 0))).Times(1);
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, SaveNewTimedMetadata(_,StrEq(adBreakId.c_str()),30000.0)).Times(1);
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, SaveNewTimedMetadata(_,StrEq(adBreakId.c_str()),0)).Times(1);
    mStreamAbstractionAAMP_MPD->InvokeFindTimedMetadata(mMPD, mRootNode, true, false);

    // LiveManifest=false and init=false
    ResetCDAIAdObject();
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, FoundEventBreak(adBreakId,_,Field(&EventBreakInfo::duration, 30000))).Times(1);
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, FoundEventBreak(adBreakId,_,Field(&EventBreakInfo::duration, 0))).Times(1);
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, SaveNewTimedMetadata(_,StrEq(adBreakId.c_str()),30000.0)).Times(1);
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, SaveNewTimedMetadata(_,StrEq(adBreakId.c_str()),0)).Times(1);
    mStreamAbstractionAAMP_MPD->InvokeFindTimedMetadata(mMPD, mRootNode, false, false);

    // Duplicate Periods are not processed
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, FoundEventBreak(adBreakId,_,_)).Times(0);
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, SaveNewTimedMetadata(_,_,_)).Times(0);
    mStreamAbstractionAAMP_MPD->InvokeFindTimedMetadata(mMPD, mRootNode, false, false);
}

TEST_F(FindTimedMetadataTests, LinearWithTwoSCTE35EventsInPeriod)
{
    static const char *manifest =
R"(<?xml version="1.0" encoding="UTF-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" xmlns:scte35="urn:scte:scte35:2014:xml+bin" xmlns:scte214="scte214" xmlns:cenc="urn:mpeg:cenc:2013" xmlns:mspr="mspr" type="dynamic" id="8371500471198371163" profiles="urn:mpeg:dash:profile:isoff-live:2011,http://www.dashif.org/guidelines/low-latency-live-v5" minBufferTime="PT0H0M1.000S" maxSegmentDuration="PT2.34S" minimumUpdatePeriod="PT0H0M1.920S" availabilityStartTime="1970-01-01T00:00:00.000Z" timeShiftBufferDepth="PT0H30M1.044S" publishTime="2024-06-25T11:23:17.130Z">
  <Period id="Period-1" start="PT477586H51M45.467S">
    <EventStream schemeIdUri="urn:scte:scte35:2014:xml+bin" timescale="90000" presentationTimeOffset="79441464098">
      <Event presentationTime="79441464098" duration="2440800">
        <scte35:Signal>
          <scte35:Binary>/DAsAAAQdSsYAP/wBQb+3zKJFQAWAhRDVUVJAAAkQn/AAABOcUAAACIAAJR2FfM=</scte35:Binary>
        </scte35:Signal>
      </Event>
      <Event presentationTime="79455172898" duration="2700000">
        <scte35:Signal>
          <scte35:Binary>/DAnAAAQdSsYAP/wBQb+34D6VQARAg9DVUVJAAAkQn+AAAAjAAAJmX3z</scte35:Binary>
        </scte35:Signal>
      </Event>
    </EventStream>
    <AdaptationSet id="track-1" contentType="video" mimeType="video/mp4" segmentAlignment="true" startWithSAP="1">
      <Role schemeIdUri="urn:mpeg:dash:role:2011" value="main"/>
      <SegmentTemplate initialization="track-video-periodid-Period-1-repid-$RepresentationID$-tc-0-header.mp4" media="track-video-periodid-Period-1-repid-$RepresentationID$-tc-0-time-$Time$.mp4" timescale="240000" startNumber="168440" presentationTimeOffset="79455172898" availabilityTimeOffset="1.44" availabilityTimeComplete="false">
        <SegmentTimeline>
          <S t="79476480098" d="460800" r="810"/>
          <S t="79850188898" d="364800" r="0"/>
          <S t="79850553698" d="360000" r="0"/>
        </SegmentTimeline>
      </SegmentTemplate>
      <Representation id="track-2" bandwidth="500000" codecs="hvc1.1.6.L63.90" width="640" height="360" frameRate="25"/>
      <Representation id="track-3" bandwidth="1200000" codecs="hvc1.1.6.L93.90" width="960" height="540" frameRate="50"/>
      <Representation id="track-4" bandwidth="1850000" codecs="hvc1.1.6.L93.90" width="960" height="540" frameRate="50"/>
    </AdaptationSet>
  </Period>
</MPD>
)";
    std::string adBreakId = "Period-1";
    EXPECT_CALL(*g_mockAampUtils, parseAndValidateSCTE35(_)).WillRepeatedly(Return(true));

    InitializeMPD(manifest);
    mStreamAbstractionAAMP_MPD->SetIsLiveManifest(true);
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, FoundEventBreak(adBreakId,_,Field(&EventBreakInfo::duration, 27120))).Times(1);
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, FoundEventBreak(adBreakId,_,Field(&EventBreakInfo::duration, 30000))).Times(1);
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, SaveNewTimedMetadata(_,StrEq(adBreakId.c_str()),27120.0)).Times(1);
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, SaveNewTimedMetadata(_,StrEq(adBreakId.c_str()),30000.0)).Times(1);
    mStreamAbstractionAAMP_MPD->InvokeFindTimedMetadata(mMPD, mRootNode, false, false);
}

TEST_F(FindTimedMetadataTests, BulkMetaReporting_CallsSaveTimedMetadata)
{
    static const char *manifest =
R"(<?xml version="1.0" encoding="UTF-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" xmlns:scte35="urn:scte:scte35:2014:xml+bin" type="dynamic" id="test" profiles="urn:mpeg:dash:profile:isoff-live:2011" minBufferTime="PT1S" availabilityStartTime="1970-01-01T00:00:00.000Z" timeShiftBufferDepth="PT30M">
  <Period id="Period-1" start="PT477586H51M45.467S">
    <EventStream schemeIdUri="urn:scte:scte35:2014:xml+bin" timescale="90000" presentationTimeOffset="79441464098">
      <Event presentationTime="79441464098" duration="2440800">
        <scte35:Signal><scte35:Binary>/DAsAAAQdSsYAP/wBQb+3zKJFQAWAhRDVUVJAAAkQn/AAABOcUAAACIAAJR2FfM=</scte35:Binary></scte35:Signal>
      </Event>
      <Event presentationTime="79455172898" duration="2700000">
        <scte35:Signal><scte35:Binary>/DAnAAAQdSsYAP/wBQb+34D6VQARAg9DVUVJAAAkQn+AAAAjAAAJmX3z</scte35:Binary></scte35:Signal>
      </Event>
    </EventStream>
    <AdaptationSet id="track-1" contentType="video" mimeType="video/mp4">
      <SegmentTemplate initialization="init.mp4" media="seg-$Time$.mp4" timescale="240000" presentationTimeOffset="79455172898">
        <SegmentTimeline><S t="79476480098" d="460800" r="10"/></SegmentTimeline>
      </SegmentTemplate>
      <Representation id="track-2" bandwidth="500000" codecs="avc1.640028" width="640" height="360"/>
    </AdaptationSet>
  </Period>
</MPD>
)";
    std::string adBreakId = "Period-1";
    EXPECT_CALL(*g_mockAampUtils, parseAndValidateSCTE35(_)).WillRepeatedly(Return(true));

    InitializeMPD(manifest);
    mStreamAbstractionAAMP_MPD->SetIsLiveManifest(true);

    // FoundEventBreak must be called for both events regardless of reportBulkMeta.
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, FoundEventBreak(adBreakId,_,Field(&EventBreakInfo::duration, 27120))).Times(1);
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, FoundEventBreak(adBreakId,_,Field(&EventBreakInfo::duration, 30000))).Times(1);
    // With reportBulkMeta=true, SaveTimedMetadata is called instead of SaveNewTimedMetadata.
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, SaveTimedMetadata(_,_,StrEq(adBreakId.c_str()),27120.0)).Times(1);
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, SaveTimedMetadata(_,_,StrEq(adBreakId.c_str()),30000.0)).Times(1);
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, SaveNewTimedMetadata(_,_,_)).Times(0);
    mStreamAbstractionAAMP_MPD->InvokeFindTimedMetadata(mMPD, mRootNode, false, true);
}

TEST_F(FindTimedMetadataTests, Validate_StartPeriodMS_for_EAP_EventStream)
{
    static const char *manifest =
R"(<?xml version="1.0" encoding="UTF-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011"
     xmlns:scte35="urn:scte:scte35:2014:xml+bin"
     type="dynamic"
     id="hot-cdvr-eap-test"
     profiles="urn:mpeg:dash:profile:isoff-live:2011"
     minBufferTime="PT4S"
     minimumUpdatePeriod="PT4S"
     availabilityStartTime="1970-07-01T10:00:00.000Z"
     timeShiftBufferDepth="PT3H"
     suggestedPresentationDelay="PT10S">
  <!-- Period-1: 10:00 – 10:30, main video, no EventStream -->
  <Period id="Period-1" start="PT0S" duration="PT1800S">
    <AdaptationSet id="1" contentType="video" mimeType="video/mp4" segmentAlignment="true">
      <SegmentTemplate timescale="90000" initialization="video-init.mp4"
                       media="video-$Number$.m4s" startNumber="1">
        <SegmentTimeline>
          <S t="0" d="360000" r="24"/>
        </SegmentTimeline>
      </SegmentTemplate>
      <Representation id="r1" mimeType="video/mp4" codecs="avc1.640028"
                      bandwidth="2000000" width="1280" height="720"/>
    </AdaptationSet>
  </Period>
  <!-- Period-2: 10:30 – 11:00, main video + EventStream (SCTE-35 ad cue) -->
  <Period id="Period-2" start="PT1800S" duration="PT1800S">
    <EventStream schemeIdUri="urn:scte:scte35:2014:xml+bin" timescale="1000">
      <Event presentationTime="0" duration="30000">
        <scte35:Signal>
          <scte35:Binary>/DAsAAAQdSsYAP/wBQb+3zKJFQAWAhRDVUVJAAAkQn/AAABOcUAAACIAAJR2FfM=</scte35:Binary>
        </scte35:Signal>
      </Event>
    </EventStream>
    <AdaptationSet id="2" contentType="video" mimeType="video/mp4" segmentAlignment="true">
      <SegmentTemplate timescale="90000" initialization="video-init.mp4"
                       media="video-$Number$.m4s" startNumber="451">
        <SegmentTimeline>
          <S t="162000000" d="360000" r="24"/>
        </SegmentTimeline>
      </SegmentTemplate>
      <Representation id="r2" mimeType="video/mp4" codecs="avc1.640028"
                      bandwidth="2000000" width="1280" height="720"/>
    </AdaptationSet>
  </Period>
  <!-- Period-3: 11:00 – live edge, main video, NO @duration (EAP follows) -->
  <Period id="Period-3" start="PT3600S">
    <AdaptationSet id="3" contentType="video" mimeType="video/mp4" segmentAlignment="true">
      <SegmentTemplate timescale="90000" initialization="video-init.mp4"
                       media="video-$Number$.m4s" startNumber="901">
        <!-- 5 × 4 s = 20 s = 20000 ms; GetPeriodDuration(index 2) mocked to 20000.0 -->
        <SegmentTimeline>
          <S t="324000000" d="360000" r="4"/>
        </SegmentTimeline>
      </SegmentTemplate>
      <Representation id="r3" mimeType="video/mp4" codecs="avc1.640028"
                      bandwidth="2000000" width="1280" height="720"/>
    </AdaptationSet>
  </Period>
  <!-- Period-4: EAP — no @start, no @duration, no AdaptationSet -->
  <Period id="Period-4">
    <EventStream schemeIdUri="urn:scte:scte35:2014:xml+bin" timescale="1000">
      <Event presentationTime="0" duration="30000">
        <scte35:Signal>
          <scte35:Binary>/DAsAAAQdSsYAP/wBQb+3zKJFQAWAhRDVUVJAAAkQn/AAABOcUAAACIAAJR2FfM=</scte35:Binary>
        </scte35:Signal>
      </Event>
    </EventStream>
  </Period>
</MPD>
)";

    EXPECT_CALL(*g_mockAampUtils, parseAndValidateSCTE35(_)).WillRepeatedly(Return(true));

    InitializeMPD(manifest);
    mStreamAbstractionAAMP_MPD->SetIsLiveManifest(true);
    // availabilityStartTime="1970-07-01T10:00:00.000Z" = 181 days * 86400 + 10 * 3600 = 15674400 seconds
    mStreamAbstractionAAMP_MPD->SetAvailabilityStartTime(15674400.0);
    gpGlobalConfig->SetConfigValue(AAMP_APPLICATION_SETTING,
                                   eAAMPConfig_EnableSCTE35PresentationTime, false);

    auto mpdHelper = std::make_shared<AampMPDParseHelper>();
    mStreamAbstractionAAMP_MPD->SetMPDParseHelper(mpdHelper);

    // Period-4 has no AdaptationSets → EarlyAvailablePeriod returns true.
    EXPECT_CALL(*g_mockAampMPDParseHelper, IsEmptyPeriod(3, _))
        .WillOnce(Return(true));

    EXPECT_CALL(*g_mockAampMPDParseHelper, GetPeriodDuration(2, _, _, _))
        .WillOnce(Return(20000.0));

    // Period-2: effectivePeriodStartMS = 1800000 (period@start) + 15674400000 (availabilityStartTime) = 15676200000 ms.
    EXPECT_CALL(*g_mockPrivateInstanceAAMP,
        FoundEventBreak(std::string("Period-2"), 15676200000ULL, _)).Times(1);
    EXPECT_CALL(*g_mockPrivateInstanceAAMP,
        SaveNewTimedMetadata(15676200000LL, StrEq("Period-2"), 30000.0)).Times(1);

    // Period-4 (EAP): effectivePeriodStartMS = (3600000 + 20000) + 15674400000 = 15678020000 ms.
    EXPECT_CALL(*g_mockPrivateInstanceAAMP,
        FoundEventBreak(std::string("Period-4"), 15678020000ULL, _)).Times(1);
    EXPECT_CALL(*g_mockPrivateInstanceAAMP,
        SaveNewTimedMetadata(15678020000LL, StrEq("Period-4"), 30000.0)).Times(1);

    mStreamAbstractionAAMP_MPD->InvokeFindTimedMetadata(mMPD, mRootNode, false, false);
}

TEST_F(FindTimedMetadataTests, LiveManifest_PeriodStartMS_IncludesAvailabilityStartTime)
{
    // availabilityStartTime = 1970-01-01T01:00:00Z = 3600 seconds from epoch.
    // Period-1: start="PT1800S" -> periodStartMS = 1800000 ms (relative offset).
    // SetAvailabilityStartTime(3600.0) simulates what Init() sets from the manifest.
    // effectivePeriodStartMS = 1800000 + (3600 * 1000) = 5400000 ms.
    static const char *manifest =
R"(<?xml version="1.0" encoding="UTF-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011"
     xmlns:scte35="urn:scte:scte35:2014:xml+bin"
     type="dynamic"
     profiles="urn:mpeg:dash:profile:isoff-live:2011"
     minBufferTime="PT4S"
     minimumUpdatePeriod="PT4S"
     availabilityStartTime="1970-01-01T01:00:00.000Z"
     timeShiftBufferDepth="PT3H">
  <Period id="Period-1" start="PT1800S" duration="PT1800S">
    <EventStream schemeIdUri="urn:scte:scte35:2014:xml+bin" timescale="1000">
      <Event presentationTime="0" duration="30000">
        <scte35:Signal>
          <scte35:Binary>/DAsAAAQdSsYAP/wBQb+3zKJFQAWAhRDVUVJAAAkQn/AAABOcUAAACIAAJR2FfM=</scte35:Binary>
        </scte35:Signal>
      </Event>
    </EventStream>
    <AdaptationSet id="1" contentType="video" mimeType="video/mp4">
      <Representation id="r1" mimeType="video/mp4" codecs="avc1.640028" bandwidth="2000000" width="1280" height="720"/>
    </AdaptationSet>
  </Period>
</MPD>
)";

    EXPECT_CALL(*g_mockAampUtils, parseAndValidateSCTE35(_)).WillRepeatedly(Return(true));

    InitializeMPD(manifest);
    mStreamAbstractionAAMP_MPD->SetIsLiveManifest(true);
    // Simulate what Init() would populate: availabilityStartTime = 3600 seconds
    mStreamAbstractionAAMP_MPD->SetAvailabilityStartTime(3600.0);

    gpGlobalConfig->SetConfigValue(AAMP_APPLICATION_SETTING,
                                   eAAMPConfig_EnableSCTE35PresentationTime, false);

    // effectivePeriodStartMS = 1800000 (period@start) + 3600000 (availabilityStartTime) = 5400000
    const uint64_t expectedStartMS = 1800000ULL + 3600000ULL;
    EXPECT_CALL(*g_mockPrivateInstanceAAMP,
        FoundEventBreak(std::string("Period-1"), expectedStartMS, _)).Times(1);
    EXPECT_CALL(*g_mockPrivateInstanceAAMP,
        SaveNewTimedMetadata(static_cast<long long>(expectedStartMS), StrEq("Period-1"), 30000.0)).Times(1);

    mStreamAbstractionAAMP_MPD->InvokeFindTimedMetadata(mMPD, mRootNode, false, false);
}