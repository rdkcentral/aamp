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
#include "AampLogManager.h"
#include "fragmentcollector_hls.h"
#include "MockAampConfig.h"

AampConfig *gpGlobalConfig{nullptr};

// Add this derived class definition
class TestTrackState : public TrackState
{
public:
	TestTrackState(TrackType type, class StreamAbstractionAAMP_HLS *parent,
				   PrivateInstanceAAMP *aamp, const char *name,
				   id3_callback_t id3Handler = nullptr,
				   ptsoffset_update_t ptsUpdate = nullptr)
		: TrackState(type, parent, aamp, name, id3Handler, ptsUpdate) {}

	~TestTrackState()
	{
		// Ensure the thread is stopped when the mock is destroyed
		threadDone = true;
	}

	bool threadDone{false}; /**< Flag to indicate if the thread should exit */

	// Override RunFetchLoop to allow testing
	void RunFetchLoop() override
	{
		while (!threadDone)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}
	};
};
class byteRangeTests : public ::testing::Test
{
protected:
    PrivateInstanceAAMP *mPrivateInstanceAAMP{};
    StreamAbstractionAAMP_HLS *mStreamAbstractionAAMP_HLS{};
    TrackState *trackStateObj{};

    void SetUp() override
    {
        if (gpGlobalConfig == nullptr)
        {
            gpGlobalConfig = new AampConfig();
        }

        mPrivateInstanceAAMP = new PrivateInstanceAAMP(gpGlobalConfig);

        g_mockAampConfig = new MockAampConfig();

        mStreamAbstractionAAMP_HLS = new StreamAbstractionAAMP_HLS(mPrivateInstanceAAMP, 0, 0.0);

        trackStateObj = new TrackState(eTRACK_VIDEO, mStreamAbstractionAAMP_HLS, mPrivateInstanceAAMP, "TestTrack");

    }

    void TearDown() override
    {
        delete trackStateObj;
        trackStateObj = nullptr;

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

TEST_F(byteRangeTests, withoutbyterange) {
	size_t byteRangeLength = 0;
	size_t byteRangeOffset = 0;

	const char *raw = "#EXT-";
	lstring param(raw,strlen(raw));
	bool status = trackStateObj->IsExtXByteRange(param,&byteRangeLength, &byteRangeOffset);
	EXPECT_FALSE(status);
}

TEST_F(byteRangeTests, withoutvalue) {
	GTEST_SKIP(); // for now skip this invalid variation
	size_t byteRangeLength = 0;
	size_t byteRangeOffset = 0;
	const char *raw = "#EXT-X-BYTERANGE:";
	lstring param(raw,strlen(raw));
	bool status = trackStateObj->IsExtXByteRange(param,&byteRangeLength, &byteRangeOffset);
	EXPECT_FALSE(status);
	EXPECT_EQ(byteRangeLength,0);
	EXPECT_EQ(byteRangeOffset,0);
}

TEST_F(byteRangeTests, withbytelength) {
	size_t byteRangeLength = 0;
	size_t byteRangeOffset = 0;
	
	const char *raw = "#EXT-X-BYTERANGE: 1234"; // length-only
	lstring param(raw,strlen(raw));
	bool status = trackStateObj->IsExtXByteRange(param,&byteRangeLength, &byteRangeOffset);
    EXPECT_TRUE(status);
	EXPECT_EQ(byteRangeLength,1234);
	EXPECT_EQ(byteRangeOffset,0);
}

TEST_F(byteRangeTests, withbytevalue) {
	size_t byteRangeLength = 0;
	size_t byteRangeOffset = 0;

	const char *raw = "#EXT-X-BYTERANGE: 1234@4321"; // length & range
	lstring param(raw,strlen(raw));
	bool status = trackStateObj->IsExtXByteRange(param,&byteRangeLength, &byteRangeOffset);
	EXPECT_TRUE(status);
	EXPECT_EQ(byteRangeLength,1234);
	EXPECT_EQ(byteRangeOffset,4321);
}

TEST_F(byteRangeTests, withoutseg) {
	size_t byteRangeLength = 0;
	size_t byteRangeOffset = 0;

	const char *raw = "#EXT-X-BYTERANGE: 1234@4321,"; // length & range
	lstring param(raw,strlen(raw));
	bool status = trackStateObj->IsExtXByteRange(param,&byteRangeLength, &byteRangeOffset);
	EXPECT_TRUE(status);
	EXPECT_EQ(byteRangeLength,1234);
	EXPECT_EQ(byteRangeOffset,4321);
}

TEST_F(byteRangeTests, withsegnum) {
	size_t byteRangeLength = 0;
	size_t byteRangeOffset = 0;

	const char *raw = "#EXT-X-BYTERANGE: 1234@4321,\nseg2.m4s"; // length & range
	lstring param(raw,strlen(raw));
	bool status = trackStateObj->IsExtXByteRange(param,&byteRangeLength, &byteRangeOffset);
	EXPECT_TRUE(status);
	EXPECT_EQ(byteRangeLength,1234);
	EXPECT_EQ(byteRangeOffset,4321);
}

TEST_F(byteRangeTests, withmanifestvalue) {
	size_t byteRangeLength = 0;
	size_t byteRangeOffset = 0;

	const char *raw = "#EXT-X-BYTERANGE:280451@274920";
	lstring param(raw,strlen(raw));
	bool status = trackStateObj->IsExtXByteRange(param,&byteRangeLength, &byteRangeOffset);
	EXPECT_TRUE(status);
	EXPECT_EQ(byteRangeLength,280451);
	EXPECT_EQ(byteRangeOffset,274920);
}

TEST_F(byteRangeTests, testThreadStart) 
{

	// Create a Trackstate object for the test

	// This is necessary to ensure that the StreamAbstractionAAMP_HLS object is properly initialized
	// and can handle the Start and Stop calls correctly.
	TrackState *trackState = new TestTrackState(eTRACK_VIDEO, mStreamAbstractionAAMP_HLS, mPrivateInstanceAAMP, "TestTrack");

	// Call the Start function
	trackState->Start();

	// And a second time - if the thread is already running, it should not cause any issues
	trackState->Start();
}

TEST_F(byteRangeTests, testFormatFromExtension )
{
	static struct
	{
		const char *data;
		StreamOutputFormat format;
	} test_data[] =
	{
        { "#EXTM3U8\r\nx.ts", FORMAT_MPEGTS },
        { "#EXTM3U8\r\n\r\ny.ts", FORMAT_MPEGTS },
		{ "#EXTM3U8\r\n#EXT-X-MEDIA\r\n#EXT-X-MAP foo.mp4", FORMAT_ISO_BMFF },
		{ "a.b.x.aac", FORMAT_AUDIO_ES_AAC },
		{ "x.ac3", FORMAT_AUDIO_ES_AC3 },    /**< AC3 Audio Elementary Stream */
		{ "x.ec3", FORMAT_AUDIO_ES_EC3 },    /**< Dolby Digital Plus Elementary Stream */
		{ "x.webvtt", FORMAT_SUBTITLE_WEBVTT }, /**< WebVTT subtitle Stream */
		{ "x.vtt", FORMAT_SUBTITLE_WEBVTT }, /**< WebVTT subtitle Stream */
		{ "foo", FORMAT_INVALID },
		{ "foo.unk", FORMAT_INVALID },
		{ "a.ts.x", FORMAT_INVALID }
	};
	for( size_t i=0; i<ARRAY_SIZE(test_data); i++ )
	{
        printf( "test#%zu: '%s'\n", i, test_data[i].data );
		AampGrowableBuffer buffer;
		buffer.AppendBytes(test_data[i].data, strlen(test_data[i].data) );
		StreamOutputFormat format = GetFormatFromFragmentExtension( buffer );
		EXPECT_EQ( format, test_data[i].format );
	}
}
