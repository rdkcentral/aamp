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

/**
 * @file FunctionalTests.cpp
 * @brief Functional unit tests for AampMp4Demuxer
 */

//Google test dependencies
#include <gtest/gtest.h>
#include <gmock/gmock.h>

// unit under test
#include "AampMp4Demuxer.h"
#include "MockPrivateInstanceAAMP.h"
#include "MockMp4Demux.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::Return;
using ::testing::InSequence;
using ::testing::StrictMock;
using ::testing::NiceMock;
using ::testing::AnyNumber;
using ::testing::Invoke;

AampConfig *gpGlobalConfig{nullptr};

/**
 * @class AampMp4DemuxerBaseTests
 * @brief Test fixture for AampMp4Demuxer functional tests
 */
class AampMp4DemuxerTests : public ::testing::Test
{
protected:
	void SetUp() override
	{
		// Create mock instances
		if(gpGlobalConfig == nullptr)
		{
			gpGlobalConfig =  new AampConfig();
		}
		mPrivateInstanceAAMP = new PrivateInstanceAAMP(gpGlobalConfig);
		g_mockPrivateInstanceAAMP = new NiceMock<MockPrivateInstanceAAMP>();
		g_mockMp4Demux = new NiceMock<MockMp4Demux>();

		// Create the demuxer instance with mocked AAMP
		mDemuxer = new AampMp4Demuxer(mPrivateInstanceAAMP, eMEDIATYPE_VIDEO, false);
	}

	void TearDown() override
	{
		delete mDemuxer;
		mDemuxer = nullptr;
		delete mPrivateInstanceAAMP;
		mPrivateInstanceAAMP = nullptr;
		delete g_mockPrivateInstanceAAMP;
		g_mockPrivateInstanceAAMP = nullptr;
		delete g_mockMp4Demux;
		g_mockMp4Demux = nullptr;
		delete gpGlobalConfig;
		gpGlobalConfig = nullptr;
	}

	AampMp4Demuxer* mDemuxer;
	PrivateInstanceAAMP* mPrivateInstanceAAMP;
};
/**
 * @brief Test AampMp4Demuxer constructor and destructor
 */
TEST_F(AampMp4DemuxerTests, ConstructorDestructor)
{
	// Constructor creates the object successfully
	EXPECT_NE(mDemuxer, nullptr);

	// Test different media types
	AampMp4Demuxer audioDemuxer(mPrivateInstanceAAMP, eMEDIATYPE_AUDIO, false);
	EXPECT_TRUE(true); // Constructor should complete without throwing
}

/**
 * @brief Test sendSegment with valid buffer containing samples
 */
TEST_F(AampMp4DemuxerTests, SendSegmentWithSamples)
{
	// Create test buffer
	const char* videoData = "video_data";
	std::vector<uint8_t> buffer(videoData, videoData + strlen(videoData));

	// Set expectations for Mp4Demux mock
	EXPECT_CALL(*g_mockMp4Demux, Parse(_, _))
		.WillOnce(Return(true));

	EXPECT_CALL(*g_mockMp4Demux, GetSamples())
		.WillOnce(Invoke([]() {
			std::vector<AampMediaSample> mockSamples;
			AampMediaSample sample1, sample2;

			// Set only the timing information, not the buffer data
			sample1.mPts = 1000;
			sample1.mDuration = 100;

			sample2.mPts = 1100;
			sample2.mDuration = 100;

			mockSamples.push_back(std::move(sample1));
			mockSamples.push_back(std::move(sample2));

			return mockSamples;
		}));

	// Set expectations for PrivateInstanceAAMP mock
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendStreamTransfer(eMEDIATYPE_VIDEO, _))
		.Times(2); // Should be called for each sample

	// Test parameters
	double position = 10.0;
	double duration = 5.0;
	double fragmentPTSoffset = 0.0;
	bool discontinuous = false;
	bool isInit = false;
	MediaProcessor::process_fcn_t processor = nullptr;
	bool ptsError = false;

	// Call sendSegment
	bool result = mDemuxer->sendSegment(buffer, position, duration, fragmentPTSoffset,
									   discontinuous, isInit, processor, ptsError);

	// Verify results
	EXPECT_TRUE(result);
	EXPECT_FALSE(ptsError);
}

/**
 * @brief Test sendSegment with empty buffer
 */
TEST_F(AampMp4DemuxerTests, SendSegmentWithEmptyBuffer)
{
	std::vector<uint8_t> emptyBuffer;
	bool ptsError = false;

	// Verify no calls were made to the mocked dependencies
	EXPECT_CALL(*g_mockMp4Demux, Parse(_, _))
		.Times(0);
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendStreamTransfer(_, _))
		.Times(0);
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SetStreamCaps(_, _))
		.Times(0);

	// Call sendSegment with empty buffer
	bool result = mDemuxer->sendSegment(emptyBuffer, 0.0, 0.0, 0.0, false, false, nullptr, ptsError);

	// Should return false but not process anything
	EXPECT_FALSE(result);
	EXPECT_FALSE(ptsError);
}

/**
 * @brief Test sendSegment with different media types
 */
TEST_F(AampMp4DemuxerTests, SendSegmentDifferentMediaTypes)
{
	// Test with audio
	AampMp4Demuxer *audDemuxer = new AampMp4Demuxer(mPrivateInstanceAAMP, eMEDIATYPE_AUDIO, false);

	const char* audioData = "audio_data";
	std::vector<uint8_t> buffer(audioData, audioData + strlen(audioData));

	EXPECT_CALL(*g_mockMp4Demux, Parse(_, _)).WillOnce(Return(true));
	EXPECT_CALL(*g_mockMp4Demux, GetSamples())
		.WillOnce(Invoke([]() {
			std::vector<AampMediaSample> samples;
			AampMediaSample sample;
			const char* audioSample = "audio_sample";
			sample.mData.assign(audioSample, audioSample + strlen(audioSample));
			samples.push_back(std::move(sample));
			return samples;
		}));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendStreamTransfer(eMEDIATYPE_AUDIO, _));
	bool ptsError = false;
	bool result = audDemuxer->sendSegment(buffer, 2.0, 1.5, 0.0, false, false, nullptr, ptsError);

	EXPECT_TRUE(result);
	EXPECT_FALSE(ptsError);
	delete audDemuxer;
}

/**
 * @brief Test sendSegment with init segment with valid codec info
 */
TEST_F(AampMp4DemuxerTests, SendInitSegmentWithValidCodecInfo)
{
	// Send an init segment that results in no samples
	const char* initData = "init_data";
	std::vector<uint8_t> initBuffer(initData, initData + strlen(initData));

	EXPECT_CALL(*g_mockMp4Demux, Parse(_, _)).WillOnce(Return(true));
	EXPECT_CALL(*g_mockMp4Demux, GetSamples())
		.WillOnce(Invoke([]() {
			return std::vector<AampMediaSample>(); // No samples
		}));
	EXPECT_CALL(*g_mockMp4Demux, GetCodecInfo())
		.WillOnce(Invoke([]() {
			MediaCodecInfo codecInfo;
			codecInfo.mCodecFormat = GST_FORMAT_VIDEO_ES_H264;
			return codecInfo;
		})); // Return codec info
	// No SendStreamTransfer calls expected for init segment
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendStreamTransfer(_, _)).Times(0);
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SetStreamCaps(eMEDIATYPE_VIDEO, _)).Times(1); // Should set stream caps
	bool ptsError = false;
	bool result = mDemuxer->sendSegment(initBuffer, 2.0, 0.0, 0.0, false, true, nullptr, ptsError);

	EXPECT_TRUE(result);
	EXPECT_FALSE(ptsError);
}

/**
 * @brief Test sendSegment with init segment with invalid codec info
 */
TEST_F(AampMp4DemuxerTests, SendInitSegmentWithInvalidCodecInfo)
{
	// Send an init segment that results in no samples
	const char* initData = "init_data";
	std::vector<uint8_t> initBuffer(initData, initData + strlen(initData));

	EXPECT_CALL(*g_mockMp4Demux, Parse(_, _)).WillOnce(Return(true));
	EXPECT_CALL(*g_mockMp4Demux, GetSamples())
		.WillOnce(Invoke([]() {
			return std::vector<AampMediaSample>(); // No samples
		}));
	EXPECT_CALL(*g_mockMp4Demux, GetCodecInfo())
		.WillOnce(Invoke([]() {
			MediaCodecInfo codecInfo;
			codecInfo.mCodecFormat = GST_FORMAT_INVALID;
			codecInfo.mIsEncrypted = false;
			return codecInfo;
		})); // Return explicit invalid codec info
	// No SendStreamTransfer and SetStreamCaps calls expected for init segment
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendStreamTransfer(_, _)).Times(0);
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SetStreamCaps(eMEDIATYPE_VIDEO, _)).Times(0); // Should set stream caps
	bool ptsError = false;
	bool result = mDemuxer->sendSegment(initBuffer, 2.0, 0.0, 0.0, false, true, nullptr, ptsError);

	EXPECT_FALSE(result);
	EXPECT_FALSE(ptsError);
}

/**
 * @brief Test sendSegment with parse failure
 */
TEST_F(AampMp4DemuxerTests, SendSegmentWithParseFailure)
{
	// Create test buffer
	const char* videoData = "video_data";
	std::vector<uint8_t> buffer(videoData, videoData + strlen(videoData));

	// Set expectations for Mp4Demux mock to simulate parse failure
	EXPECT_CALL(*g_mockMp4Demux, Parse(_, _))
		.WillOnce(Return(false)); // Simulate parse failure

	// No calls to GetSamples or SendStreamTransfer should occur
	EXPECT_CALL(*g_mockMp4Demux, GetSamples())
		.Times(0);
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendStreamTransfer(_, _))
		.Times(0);
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SetStreamCaps(_, _))
		.Times(0);

	// Test parameters
	double position = 5.0;
	double duration = 2.0;
	double fragmentPTSoffset = 0.0;
	bool discontinuous = false;
	bool isInit = false;
	MediaProcessor::process_fcn_t processor = nullptr;
	bool ptsError = false;

	// Call sendSegment
	bool result = mDemuxer->sendSegment(buffer, position, duration, fragmentPTSoffset,
									   discontinuous, isInit, processor, ptsError);

	// Verify results
	EXPECT_FALSE(result);
	EXPECT_FALSE(ptsError);
}

/**
 * @brief Test sendSegment with PTS restamping enabled
 */
TEST_F(AampMp4DemuxerTests, SendSegmentWithPtsRestampEnabled)
{
	AampMp4Demuxer restampDemuxer(mPrivateInstanceAAMP, eMEDIATYPE_VIDEO, true);

	const char* videoData = "video_data";
	std::vector<uint8_t> buffer(videoData, videoData + strlen(videoData));

	constexpr double kBasePts{10.0};
	constexpr double kBaseDts{9.5};
	constexpr double kFragmentPtsOffset{2.5};
	
	EXPECT_CALL(*g_mockMp4Demux, Parse(_, _))
		.WillOnce(Return(true));
	// GetTimeScale only called (while logging) when eAAMPConfig_EnablePTSReStampLogging set
	//	EXPECT_CALL(*g_mockMp4Demux, GetTimeScale())
	//		.WillOnce(Return(90000));
	EXPECT_CALL(*g_mockMp4Demux, GetSamples())
		.WillOnce(Invoke([=]() {
			std::vector<AampMediaSample> mockSamples;
			AampMediaSample sample;
			sample.mPts = kBasePts;
			sample.mDts = kBaseDts;
			mockSamples.push_back(std::move(sample));
			return mockSamples;
		}));

	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendStreamTransfer(eMEDIATYPE_VIDEO, _))
		.WillOnce(Invoke([=](AampMediaType /*mediaType*/, AampMediaSample&& sample) {
			EXPECT_DOUBLE_EQ(sample.mPts, kBasePts + kFragmentPtsOffset);
			EXPECT_DOUBLE_EQ(sample.mDts, kBaseDts + kFragmentPtsOffset);
		}));

	bool ptsError = false;
	bool result = restampDemuxer.sendSegment(buffer, 10.0, 5.0, kFragmentPtsOffset,
			false, false, nullptr, ptsError);

	EXPECT_TRUE(result);
	EXPECT_FALSE(ptsError);
}
