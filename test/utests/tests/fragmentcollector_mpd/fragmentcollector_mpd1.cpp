/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2025 RDK Management
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
#include "priv_aamp.h"
#include "AampConfig.h"
#include "fragmentcollector_mpd.h"
#include "MockPrivateInstanceAAMP.h"
#include "MockAampUtils.h"
#include "MockAampConfig.h"
#include "StreamAbstractionAAMP.h"
#include "MediaStreamContext.h"

#include "libdash/IMPD.h"
#include "libdash/INode.h"
#include "libdash/IDASHManager.h"
#include "libdash/IProducerReferenceTime.h"
#include "libdash/xml/Node.h"
#include "libdash/helpers/Time.h"
#include "libdash/xml/DOMParser.h"
#include <libxml/xmlreader.h>

using ::testing::_;
using ::testing::Field;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::StrEq;
using ::testing::StrictMock;

using namespace dash::xml;
using namespace dash::mpd;

/**
 * @brief MpdTests tests common base class.
 */
class MpdTests : public ::testing::Test
{
protected:
	class TestableStreamAbstractionAAMP_MPD : public StreamAbstractionAAMP_MPD
	{

	public:
		uint64_t ExposeFindPositionInTimeline(MediaStreamContext *ms, std::vector<ITimeline *> &timelines)
		{
			return FindPositionInTimeline(ms, timelines);
		}
		// Constructor to pass parameters to the base class constructor
		TestableStreamAbstractionAAMP_MPD(PrivateInstanceAAMP *aamp)
			: StreamAbstractionAAMP_MPD(aamp, 0, 0)
		{
		}

		void InvokeFindTimedMetadata(MPD *mpd, Node *root, bool init, bool reportBulkMeta)
		{
			FindTimedMetadata(mpd, root, init, reportBulkMeta);
		}

		void SetIsLiveManifest(bool isLive)
		{
			mIsLiveManifest = isLive;
		}
		void InitmCurrentPeriod(MPD *mpd)
		{
			mCurrentPeriod = mpd->GetPeriods().at(0);
		}

		void TsbReader() override
		{
			// This is a stub to allow testing without actual thread execution
			// In real implementation, this would start the TSB reader thread
			for(;;)
			{
				// Simulate thread work
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
				if (abortTsbReader)
				{
					break;
				}
			}
		}

		// Method to explicity shut down running threads
		void ShutdownThreads()
		{
			if (fragmentCollectorThreadID.joinable())
			{
				fragmentCollectorThreadID.join();
			}

			abortTsbReader = true; // Signal the TSB reader to stop
			// Wait for the TSB reader thread to finish
			if (tsbReaderThreadID.joinable())
			{
				tsbReaderThreadID.join();
			}
		}
	};

	PrivateInstanceAAMP *mPrivateInstanceAAMP;
	TestableStreamAbstractionAAMP_MPD *mStreamAbstractionAAMP_MPD;
	const char *mManifest;
	static constexpr const char *TEST_BASE_URL = "http://host/asset/";
	static constexpr const char *TEST_MANIFEST_URL = "http://host/asset/manifest.mpd";
	MPD *mMPD = nullptr;
	Node *mRootNode = nullptr;

	void SetUp()
	{

		gpGlobalConfig = new AampConfig();
		g_mockAampConfig = new NiceMock<MockAampConfig>();

		mPrivateInstanceAAMP = new PrivateInstanceAAMP(gpGlobalConfig);

		g_mockPrivateInstanceAAMP = new StrictMock<MockPrivateInstanceAAMP>();

		mStreamAbstractionAAMP_MPD = nullptr;

		mManifest = nullptr;
		mMPD = nullptr;
		mRootNode = nullptr;
	}

	void TearDown()
	{
		if (mStreamAbstractionAAMP_MPD)
		{
			delete mStreamAbstractionAAMP_MPD;
			mStreamAbstractionAAMP_MPD = nullptr;
		}

		delete mPrivateInstanceAAMP;
		mPrivateInstanceAAMP = nullptr;

		delete gpGlobalConfig;
		gpGlobalConfig = nullptr;

		delete g_mockAampConfig;
		g_mockAampConfig = nullptr;

		delete g_mockPrivateInstanceAAMP;
		g_mockPrivateInstanceAAMP = nullptr;

		mManifest = nullptr;
		if (mMPD)
		{
			delete (mMPD);
		}
		if (mRootNode)
		{
			delete (mRootNode);
		}
	}

public:
	/**
	 * @brief Get MPD instance from manifest
	 */
	void GetMPDFromManifest()
	{
		std::string manifestStr = mManifest;

		xmlTextReaderPtr reader = xmlReaderForMemory((char *)manifestStr.c_str(), (int)manifestStr.length(), NULL, NULL, 0);
		if (reader != NULL)
		{
			if (xmlTextReaderRead(reader))
			{
				mRootNode = MPDProcessNode(&reader, TEST_MANIFEST_URL);
				if (mRootNode != NULL)
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
		EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetLLDashChunkMode()).WillRepeatedly(Return(false));
		mPrivateInstanceAAMP->SetManifestUrl(TEST_MANIFEST_URL);
		GetMPDFromManifest();
	}

	MediaStreamContext *RunSetup(const char *manifest, std::vector<ITimeline *> &timelines)
	{
		InitializeMPD(manifest);
		mStreamAbstractionAAMP_MPD->SetIsLiveManifest(true);
		mStreamAbstractionAAMP_MPD->InitmCurrentPeriod(mMPD);
		const IAdaptationSet *adaptationSet = mStreamAbstractionAAMP_MPD->GetAdaptationSetAtIndex(0);
		const ISegmentTemplate *segmentTemplate = adaptationSet->GetSegmentTemplate();
		const ISegmentTimeline *segmentTimeline = segmentTemplate->GetSegmentTimeline();
		timelines = segmentTimeline->GetTimelines();
		return new MediaStreamContext(eTRACK_AUDIO, mStreamAbstractionAAMP_MPD, mPrivateInstanceAAMP, "xxx");
	}

	void InitMs(MediaStreamContext *ms, uint64_t lastSegmentTime, uint64_t lastSegmentDuration)
	{
		ms->lastSegmentTime = lastSegmentTime;
		ms->lastSegmentDuration = lastSegmentDuration;
		ms->timeLineIndex = 0;
		ms->fragmentDescriptor.Number = 1;
		ms->fragmentRepeatCount = 0;
	}
};

/* This test for SegmentTimeline from an ongoing main asset where t !=0 */
TEST_F(MpdTests, FindPositionInTimeline1)
{
	static const char *manifest =
		R"(<?xml version="1.0" encoding="UTF-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" xmlns:scte35="urn:scte:scte35:2014:xml+bin" xmlns:scte214="scte214" xmlns:cenc="urn:mpeg:cenc:2013" xmlns:mspr="mspr" type="dynamic" id="8371500471198371163" profiles="urn:mpeg:dash:profile:isoff-live:2011,http://www.dashif.org/guidelines/low-latency-live-v5" minBufferTime="PT0H0M1.000S" maxSegmentDuration="PT2.34S" minimumUpdatePeriod="PT0H0M1.920S" availabilityStartTime="1970-01-01T00:00:00.000Z" timeShiftBufferDepth="PT0H30M1.044S" publishTime="2024-06-25T11:23:17.130Z">
  <Period id="Period-1" start="PT477586H51M45.467S">
    <AdaptationSet id="track-1" contentType="video" mimeType="video/mp4" segmentAlignment="true" startWithSAP="1">
      <Role schemeIdUri="urn:mpeg:dash:role:2011" value="main"/>
      <SegmentTemplate initialization="track-video-periodid-Period-1-repid-$RepresentationID$-tc-0-header.mp4" media="track-video-periodid-Period-1-repid-$RepresentationID$-tc-0-time-$Time$.mp4" timescale="240000" startNumber="1" presentationTimeOffset="79455172898" availabilityTimeOffset="1.44" availabilityTimeComplete="false">
                    <SegmentTimeline>
                        <S d="54000" t="1115420172133"/>
                        <S d="112000" r="2" t="1115420226133"/>
                        <S d="114000" t="1115420562133"/>
                    </SegmentTimeline>
      </SegmentTemplate>
    </AdaptationSet>
  </Period>
</MPD>
)";
	/* The above SegmentTimeline expands out to the following:
	n, timeLineIndex,   t,             d
	1, 0,               1115420172133, 54000,
	2, 1,               1115420226133, 112000,
	3, 1,               1115420338133, 112000,
	4, 1,               1115420450133, 112000,
	5, 2,               1115420562133, 114000,
	*/

	// LiveManifest=true and init=true
	std::vector<ITimeline *> timelines = {};
	MediaStreamContext *ms = RunSetup(manifest, timelines);

	// Finally some testing
	// Last sent n=1
	InitMs(ms, 1115420172133, 1);
	uint64_t position = mStreamAbstractionAAMP_MPD->ExposeFindPositionInTimeline(ms, timelines);
	EXPECT_EQ(1115420172133, position);
	EXPECT_EQ(1, ms->fragmentDescriptor.Number);
	EXPECT_EQ(0, ms->timeLineIndex);
	EXPECT_EQ(0, ms->fragmentRepeatCount);

	// last sent n=2
	InitMs(ms, 1115420226133, 1);
	position = mStreamAbstractionAAMP_MPD->ExposeFindPositionInTimeline(ms, timelines);
	EXPECT_EQ(1115420226133, position);
	EXPECT_EQ(2, ms->fragmentDescriptor.Number);
	EXPECT_EQ(1, ms->timeLineIndex);
	EXPECT_EQ(0, ms->fragmentRepeatCount);

	// last sent n=3
	InitMs(ms, 1115420338133, 1);
	position = mStreamAbstractionAAMP_MPD->ExposeFindPositionInTimeline(ms, timelines);
	EXPECT_EQ(1115420338133, position);
	EXPECT_EQ(3, ms->fragmentDescriptor.Number);
	EXPECT_EQ(1, ms->timeLineIndex);
	EXPECT_EQ(1, ms->fragmentRepeatCount);

	// last sent n=4
	InitMs(ms, 1115420450133, 1);
	position = mStreamAbstractionAAMP_MPD->ExposeFindPositionInTimeline(ms, timelines);
	EXPECT_EQ(1115420450133, position);
	EXPECT_EQ(4, ms->fragmentDescriptor.Number);
	EXPECT_EQ(1, ms->timeLineIndex);
	EXPECT_EQ(2, ms->fragmentRepeatCount);
}

/* This test for SegmentTimeline from a server side inserted Ad where t="0" */
TEST_F(MpdTests, FindPositionInTimeline2)
{
	static const char *manifest =
		R"(<?xml version="1.0" encoding="UTF-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" xmlns:scte35="urn:scte:scte35:2014:xml+bin" xmlns:scte214="scte214" xmlns:cenc="urn:mpeg:cenc:2013" xmlns:mspr="mspr" type="dynamic" id="8371500471198371163" profiles="urn:mpeg:dash:profile:isoff-live:2011,http://www.dashif.org/guidelines/low-latency-live-v5" minBufferTime="PT0H0M1.000S" maxSegmentDuration="PT2.34S" minimumUpdatePeriod="PT0H0M1.920S" availabilityStartTime="1970-01-01T00:00:00.000Z" timeShiftBufferDepth="PT0H30M1.044S" publishTime="2024-06-25T11:23:17.130Z">
  <Period id="Period-1" start="PT477586H51M45.467S">
    <AdaptationSet id="track-1" contentType="video" mimeType="video/mp4" segmentAlignment="true" startWithSAP="1">
      <Role schemeIdUri="urn:mpeg:dash:role:2011" value="main"/>
      <SegmentTemplate initialization="track-video-periodid-Period-1-repid-$RepresentationID$-tc-0-header.mp4" media="track-video-periodid-Period-1-repid-$RepresentationID$-tc-0-time-$Time$.mp4" timescale="240000" startNumber="1" presentationTimeOffset="79455172898" availabilityTimeOffset="1.44" availabilityTimeComplete="false">
                    <SegmentTimeline>
                        <S d="109568" t="0"/>
                        <S d="107520" r="4" t="109568"/>
                        <S d="74752" t="647168"/>
                    </SegmentTimeline>
      </SegmentTemplate>
    </AdaptationSet>
  </Period>
</MPD>
)";
	/* The above SegmentTimeline expands out to the following:
	n, timeLineIndex,   t,       d
	1, 0,               0,      109568,
	2, 1,               109568, 107520,
	3, 1,               217088, 107520,
	4, 1,               324608, 107520,
	5, 1,               432128, 107520,
	6, 1,               539648, 107520,
	7, 2,               647168, 74752,
	*/

	// LiveManifest=true and init=true
	std::vector<ITimeline *> timelines = {};
	MediaStreamContext *ms = RunSetup(manifest, timelines);

	// Finally some testing
	// Last sent n=1
	InitMs(ms, 0, 1);
	uint64_t position = mStreamAbstractionAAMP_MPD->ExposeFindPositionInTimeline(ms, timelines);
	EXPECT_EQ(109568, position);				 // Should be 0 but this vale due to changes
	EXPECT_EQ(2, ms->fragmentDescriptor.Number); // Should be 2 but this vale due to changes
	EXPECT_EQ(1, ms->timeLineIndex);			 // Should be 0 but this vale due to changes
	EXPECT_EQ(0, ms->fragmentRepeatCount);

	// last sent n=2
	InitMs(ms, 109568, 1);
	position = mStreamAbstractionAAMP_MPD->ExposeFindPositionInTimeline(ms, timelines);
	EXPECT_EQ(109568, position);
	EXPECT_EQ(2, ms->fragmentDescriptor.Number);
	EXPECT_EQ(1, ms->timeLineIndex);
	EXPECT_EQ(0, ms->fragmentRepeatCount);

	// last sent n=4
	InitMs(ms, 324608, 1);
	position = mStreamAbstractionAAMP_MPD->ExposeFindPositionInTimeline(ms, timelines);
	EXPECT_EQ(324608, position);
	EXPECT_EQ(4, ms->fragmentDescriptor.Number);
	EXPECT_EQ(1, ms->timeLineIndex);
	EXPECT_EQ(2, ms->fragmentRepeatCount);

	// last sent n=6
	InitMs(ms, 539648, 1);
	position = mStreamAbstractionAAMP_MPD->ExposeFindPositionInTimeline(ms, timelines);
	EXPECT_EQ(539648, position);
	EXPECT_EQ(6, ms->fragmentDescriptor.Number);
	EXPECT_EQ(1, ms->timeLineIndex);
	EXPECT_EQ(4, ms->fragmentRepeatCount);
}

// Test that calling Start() twice in succession does not cause the test to terminate

TEST_F(MpdTests, testRepeatedStartLocalTSB)
{
	// Set local TSB to true
	mStreamAbstractionAAMP_MPD = new TestableStreamAbstractionAAMP_MPD(mPrivateInstanceAAMP);
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, DownloadsAreEnabled()).WillRepeatedly(Return(true));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, IsLocalAAMPTsbInjection()).WillRepeatedly(Return(true));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetLLDashAdjustSpeed()).WillRepeatedly(Return(false));

	mStreamAbstractionAAMP_MPD->Start();

	// Call the Start function again
	mStreamAbstractionAAMP_MPD->Start();
	mStreamAbstractionAAMP_MPD->ShutdownThreads();
}

// Test that calling Start() twice in succession does not cause the test to terminate
// This test is for the case where local TSB is false

TEST_F(MpdTests, testRepeatedStartNotLocalTSB)
{
	// Set local TSB to true
	mStreamAbstractionAAMP_MPD = new TestableStreamAbstractionAAMP_MPD(mPrivateInstanceAAMP);
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, DownloadsAreEnabled()).WillRepeatedly(Return(false));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, IsLocalAAMPTsbInjection()).WillRepeatedly(Return(false));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetLLDashAdjustSpeed()).WillRepeatedly(Return(false));

	mStreamAbstractionAAMP_MPD->Start();

	// Call the Start function again
	mStreamAbstractionAAMP_MPD->Start();
	mStreamAbstractionAAMP_MPD->ShutdownThreads();
}

