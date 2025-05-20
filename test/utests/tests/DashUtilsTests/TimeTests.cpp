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
#include "MPDModel.h"

using ::testing::NiceMock;
using namespace std;

class TimeTests : public ::testing::Test
{
protected:
    std::shared_ptr<DashMPDDocument> mpdDocument;
	void SetUp() override
	{
		mpdDocument = nullptr;
	}

	void TearDown() override
	{
		mpdDocument = nullptr;
	}

public:

};

/**
 * @brief getTimeTest test.
 *
 * Verifies that the getPublishTime, getAvailabilityEndTime, getAvailabilityStartTime return their respective times from manifest
 */
TEST_F(TimeTests, getTimeTest)
{
    const char *manifest1 =
        R"(<?xml version="1.0" encoding="utf-8"?>
            <MPD xmlns="urn:mpeg:dash:schema:mpd:2011" minimumUpdatePeriod="PT0H0M1.920S" availabilityStartTime="2023-02-25T20:00:00.000Z" publishTime="1977-05-25T18:00:00.000Z" availabilityEndTime = "2023-05-25T18:00:00.000Z" type="dynamic" profiles="urn:mpeg:dash:profile:isoff-live:2011,http://dashif.org/guidelines/dash264">
                <Period id="1234">
                    <AdaptationSet id="1" maxWidth="1920" maxHeight="1080" maxFrameRate="25" par="16:9">
                    <Representation id="1" mimeType="video/mp4" codecs="avc1.640028" width="640" height="360" frameRate="25" sar="1:1" bandwidth="1000000">
                        <SegmentTemplate timescale="4" media="video_$Time$.mp4" initialization="video_init.mp4" presentationTimeOffset="0">
                        <SegmentTimeline>
                            <S t="0" d="4" r="3599" />
                        </SegmentTimeline>
                        </SegmentTemplate>
                    </Representation>
                    </AdaptationSet>
                </Period>
            </MPD>
        )";

    mpdDocument = make_shared <DashMPDDocument>(manifest1);
    auto root = mpdDocument->getRoot();

    EXPECT_DOUBLE_EQ(root->getPublishTime(), 233431200.0);
    EXPECT_DOUBLE_EQ(root->getAvailabilityEndTime(), 1685037600.0);
    EXPECT_DOUBLE_EQ(root->getAvailabilityStartTime(), 1677355200.0);
}

/**
 * @brief publishTimeNotAvailable.
 *
 * Verifies that when the publishTime is not present in manifest, default is returned
 */
TEST_F(TimeTests, publishTimeNotAvailable)
{
    const char *manifest1 =
        R"(<?xml version="1.0" encoding="utf-8"?>
            <MPD xmlns="urn:mpeg:dash:schema:mpd:2011" minimumUpdatePeriod="PT0H0M1.920S" availabilityStartTime="2023-02-25T20:00:00.000Z" availabilityEndTime = "2023-05-25T18:00:00.000Z" type="dynamic" profiles="urn:mpeg:dash:profile:isoff-live:2011,http://dashif.org/guidelines/dash264">
                <Period id="1234">
                    <AdaptationSet id="1" maxWidth="1920" maxHeight="1080" maxFrameRate="25" par="16:9">
                    <Representation id="1" mimeType="video/mp4" codecs="avc1.640028" width="640" height="360" frameRate="25" sar="1:1" bandwidth="1000000">
                        <SegmentTemplate timescale="4" media="video_$Time$.mp4" initialization="video_init.mp4" presentationTimeOffset="0">
                        <SegmentTimeline>
                            <S t="0" d="4" r="3599" />
                        </SegmentTimeline>
                        </SegmentTemplate>
                    </Representation>
                    </AdaptationSet>
                </Period>
            </MPD>
        )";

    mpdDocument = make_shared <DashMPDDocument>(manifest1);
    auto root = mpdDocument->getRoot();

    EXPECT_DOUBLE_EQ(root->getPublishTime(), MPD_UNSET_DOUBLE);
}


/**
 * @brief availabilityEndTimeNotAvailable.
 *
 * Verifies that when the availablityEndTime is not present in manifest, default is returned
 */
TEST_F(TimeTests, availabilityEndTimeNotAvailable)
{
    const char *manifest1 =
        R"(<?xml version="1.0" encoding="utf-8"?>
            <MPD xmlns="urn:mpeg:dash:schema:mpd:2011" minimumUpdatePeriod="PT0H0M1.920S" availabilityStartTime="2023-02-25T20:00:00.000Z" publishTime="1977-05-25T18:00:00.000Z" type="dynamic" profiles="urn:mpeg:dash:profile:isoff-live:2011,http://dashif.org/guidelines/dash264">
                <Period id="1234">
                    <AdaptationSet id="1" maxWidth="1920" maxHeight="1080" maxFrameRate="25" par="16:9">
                    <Representation id="1" mimeType="video/mp4" codecs="avc1.640028" width="640" height="360" frameRate="25" sar="1:1" bandwidth="1000000">
                        <SegmentTemplate timescale="4" media="video_$Time$.mp4" initialization="video_init.mp4" presentationTimeOffset="0">
                        <SegmentTimeline>
                            <S t="0" d="4" r="3599" />
                        </SegmentTimeline>
                        </SegmentTemplate>
                    </Representation>
                    </AdaptationSet>
                </Period>
            </MPD>
        )";

    mpdDocument = make_shared <DashMPDDocument>(manifest1);
    auto root = mpdDocument->getRoot();

    EXPECT_DOUBLE_EQ(root->getAvailabilityEndTime(), MPD_UNSET_DOUBLE);
}

/**
 * @brief availabilityStartTimeNotAvailable.
 *
 * Verifies that when the availablityStartTime is not present in manifest, default is returned
 */
TEST_F(TimeTests, availabilityStartTimeNotAvailable)
{
    const char *manifest1 =
        R"(<?xml version="1.0" encoding="utf-8"?>
            <MPD xmlns="urn:mpeg:dash:schema:mpd:2011" minimumUpdatePeriod="PT0H0M1.920S" publishTime="1977-05-25T18:00:00.000Z" availabilityEndTime = "2023-05-25T18:00:00.000Z" type="dynamic" profiles="urn:mpeg:dash:profile:isoff-live:2011,http://dashif.org/guidelines/dash264">
                <Period id="1234">
                    <AdaptationSet id="1" maxWidth="1920" maxHeight="1080" maxFrameRate="25" par="16:9">
                    <Representation id="1" mimeType="video/mp4" codecs="avc1.640028" width="640" height="360" frameRate="25" sar="1:1" bandwidth="1000000">
                        <SegmentTemplate timescale="4" media="video_$Time$.mp4" initialization="video_init.mp4" presentationTimeOffset="0">
                        <SegmentTimeline>
                            <S t="0" d="4" r="3599" />
                        </SegmentTimeline>
                        </SegmentTemplate>
                    </Representation>
                    </AdaptationSet>
                </Period>
            </MPD>
        )";

    mpdDocument = make_shared <DashMPDDocument>(manifest1);
    auto root = mpdDocument->getRoot();

    EXPECT_DOUBLE_EQ(root->getAvailabilityStartTime(), MPD_UNSET_DOUBLE);
}
