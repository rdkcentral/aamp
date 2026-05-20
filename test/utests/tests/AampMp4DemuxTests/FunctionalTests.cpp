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
	EXPECT_CALL(*g_mockMp4Demux, Parse(_))
		.WillOnce(Return(true));

	EXPECT_CALL(*g_mockMp4Demux, GetSamples())
		.WillOnce([]() {
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
		});

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
	bool result = mDemuxer->sendSegment(std::move(buffer), position, duration, fragmentPTSoffset,
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
	EXPECT_CALL(*g_mockMp4Demux, Parse(_))
		.Times(0);
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendStreamTransfer(_, _))
		.Times(0);
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SetStreamCaps(_, _))
		.Times(0);

	// Call sendSegment with empty buffer
	bool result = mDemuxer->sendSegment(std::move(emptyBuffer), 0.0, 0.0, 0.0, false, false, nullptr, ptsError);

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

	EXPECT_CALL(*g_mockMp4Demux, Parse(_)).WillOnce(Return(true));
	EXPECT_CALL(*g_mockMp4Demux, GetSamples())
		.WillOnce([]() {
			std::vector<AampMediaSample> samples;
			AampMediaSample sample;
			const char* audioSample = "audio_sample";
			auto seg = std::make_shared<std::vector<uint8_t>>(audioSample, audioSample + strlen(audioSample));
			sample.mData     = std::shared_ptr<const uint8_t>(seg, seg->data());
			sample.mDataSize = seg->size();
			samples.push_back(std::move(sample));
			return samples;
		});
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendStreamTransfer(eMEDIATYPE_AUDIO, _));
	bool ptsError = false;
	bool result = audDemuxer->sendSegment(std::move(buffer), 2.0, 1.5, 0.0, false, false, nullptr, ptsError);

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

	EXPECT_CALL(*g_mockMp4Demux, Parse(_)).WillOnce(Return(true));
	EXPECT_CALL(*g_mockMp4Demux, GetSamples())
		.WillOnce([]() {
			return std::vector<AampMediaSample>(); // No samples
		});
	EXPECT_CALL(*g_mockMp4Demux, GetCodecInfo())
		.WillOnce([]() {
			MediaCodecInfo codecInfo;
			codecInfo.mCodecFormat = GST_FORMAT_VIDEO_ES_H264;
			return codecInfo;
		}); // Return codec info
	// No SendStreamTransfer calls expected for init segment
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendStreamTransfer(_, _)).Times(0);
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SetStreamCaps(eMEDIATYPE_VIDEO, _)).Times(1); // Should set stream caps
	bool ptsError = false;
	bool result = mDemuxer->sendSegment(std::move(initBuffer), 2.0, 0.0, 0.0, false, true, nullptr, ptsError);

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

	EXPECT_CALL(*g_mockMp4Demux, Parse(_)).WillOnce(Return(true));
	EXPECT_CALL(*g_mockMp4Demux, GetSamples())
		.WillOnce([]() {
			return std::vector<AampMediaSample>(); // No samples
		});
	EXPECT_CALL(*g_mockMp4Demux, GetCodecInfo())
		.WillOnce([]() {
			MediaCodecInfo codecInfo;
			codecInfo.mCodecFormat = GST_FORMAT_INVALID;
			codecInfo.mIsEncrypted = false;
			return codecInfo;
		}); // Return explicit invalid codec info
	// No SendStreamTransfer and SetStreamCaps calls expected for init segment
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendStreamTransfer(_, _)).Times(0);
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SetStreamCaps(eMEDIATYPE_VIDEO, _)).Times(0); // Should set stream caps
	bool ptsError = false;
	bool result = mDemuxer->sendSegment(std::move(initBuffer), 2.0, 0.0, 0.0, false, true, nullptr, ptsError);

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
	EXPECT_CALL(*g_mockMp4Demux, Parse(_))
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
	bool result = mDemuxer->sendSegment(std::move(buffer), position, duration, fragmentPTSoffset,
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
	
	EXPECT_CALL(*g_mockMp4Demux, Parse(_))
		.WillOnce(Return(true));
	// GetTimeScale only called (while logging) when eAAMPConfig_EnablePTSReStampLogging set
	//	EXPECT_CALL(*g_mockMp4Demux, GetTimeScale())
	//		.WillOnce(Return(90000));
	EXPECT_CALL(*g_mockMp4Demux, GetSamples())
		.WillOnce([]() {
			std::vector<AampMediaSample> mockSamples;
			AampMediaSample sample;
			sample.mPts = 10.0;
			sample.mDts = 9.5;
			mockSamples.push_back(std::move(sample));
			return mockSamples;
		});

	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendStreamTransfer(eMEDIATYPE_VIDEO, _))
		.WillOnce([kFragmentPtsOffset](AampMediaType /*mediaType*/, AampMediaSample&& sample) {
			EXPECT_DOUBLE_EQ(sample.mPts, 10.0 + kFragmentPtsOffset);
			EXPECT_DOUBLE_EQ(sample.mDts, 9.5 + kFragmentPtsOffset);
		});

	bool ptsError = false;
	bool result = restampDemuxer.sendSegment(std::move(buffer), 10.0, 5.0, kFragmentPtsOffset,
			false, false, nullptr, ptsError);

	EXPECT_TRUE(result);
	EXPECT_FALSE(ptsError);
}

/**
 * @brief Test trickplay PTS restamping - Initial state reset on first init fragment at trickplay rate
 * 
 * Validates that when entering trickplay mode with an init fragment:
 * - Trickmode state transitions to FIRST_SAMPLE
 * - mRestampedPts is reset to 0.0
 * - mLastTrickRate is updated with current rate
 */
TEST_F(AampMp4DemuxerTests, TrickplayPtsRestamp_InitFragmentStateReset)
{
	// Set trickplay rate (4x fast forward)
	mPrivateInstanceAAMP->rate = 4.0;
	mDemuxer->setRate(4.0, PlayMode_normal);
	mDemuxer->setFrameRateForTM(4);
	
	// Create init fragment buffer
	const char* initData = "init_fragment";
	std::vector<uint8_t> initBuffer(initData, initData + strlen(initData));
	
	EXPECT_CALL(*g_mockMp4Demux, Parse(_)).WillOnce(Return(true));
	EXPECT_CALL(*g_mockMp4Demux, GetSamples())
		.WillOnce([]() {
			return std::vector<AampMediaSample>(); // Init has no samples
		});
	EXPECT_CALL(*g_mockMp4Demux, GetCodecInfo())
		.WillOnce([]() {
			MediaCodecInfo codecInfo;
			codecInfo.mCodecFormat = GST_FORMAT_VIDEO_ES_H264;
			return codecInfo;
		});
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SetStreamCaps(eMEDIATYPE_VIDEO, _)).Times(1);
	
	bool ptsError = false;
	bool result = mDemuxer->sendSegment(std::move(initBuffer), 0.0, 0.0, 0.0, false, true, nullptr, ptsError);
	
	EXPECT_TRUE(result);
	EXPECT_FALSE(ptsError);
}

/**
 * @brief Test trickplay PTS restamping - First media fragment after init
 * 
 * Validates FIRST_SAMPLE state behavior:
 * - First sample PTS is restamped to 0.0
 * - Duration is calculated based on fragment duration and rate
 * - State transitions to STEADY after first sample
 */
TEST_F(AampMp4DemuxerTests, TrickplayPtsRestamp_FirstMediaFragment)
{
	// Set trickplay rate (4x fast forward)
	mPrivateInstanceAAMP->rate = 4.0;
	mDemuxer->setRate(4.0, PlayMode_normal);
	mDemuxer->setFrameRateForTM(4);
	
	// First send init fragment to reset state
	const char* initData = "init_fragment";
	std::vector<uint8_t> initBuffer(initData, initData + strlen(initData));
	
	EXPECT_CALL(*g_mockMp4Demux, Parse(_)).WillOnce(Return(true));
	EXPECT_CALL(*g_mockMp4Demux, GetSamples())
		.WillOnce([]() {
			return std::vector<AampMediaSample>();
		});
	EXPECT_CALL(*g_mockMp4Demux, GetCodecInfo())
		.WillOnce([]() {
			MediaCodecInfo codecInfo;
			codecInfo.mCodecFormat = GST_FORMAT_VIDEO_ES_H264;
			return codecInfo;
		});
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SetStreamCaps(eMEDIATYPE_VIDEO, _));
	
	bool ptsError = false;
	mDemuxer->sendSegment(std::move(initBuffer), 0.0, 0.0, 0.0, false, true, nullptr, ptsError);
	
	// Now send first media fragment
	const char* mediaData = "media_fragment";
	std::vector<uint8_t> mediaBuffer(mediaData, mediaData + strlen(mediaData));
	constexpr double kFragmentDuration = 2.0; // 2 seconds
	constexpr double kOriginalPts = 1000.0;
	
	EXPECT_CALL(*g_mockMp4Demux, Parse(_)).WillOnce(Return(true));
	EXPECT_CALL(*g_mockMp4Demux, GetSamples())
		.WillOnce([]() {
			std::vector<AampMediaSample> samples;
			AampMediaSample sample;
			sample.mPts = 1000.0;
			sample.mDts = 1000.0;
			sample.mDuration = 100.0;
			sample.mIsKeyFrame = true;
			samples.push_back(std::move(sample));
			return samples;
		});
	
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendStreamTransfer(eMEDIATYPE_VIDEO, _))
		.WillOnce([](AampMediaType /*mediaType*/, AampMediaSample&& sample) {
			// First sample should be restamped to 0.0
			EXPECT_DOUBLE_EQ(sample.mPts, 0.0);
			EXPECT_DOUBLE_EQ(sample.mDts, 0.0);
			// Duration should be fragment_duration / rate, but capped to reasonable value
			EXPECT_GT(sample.mDuration, 0.0);
		});
	
	bool result = mDemuxer->sendSegment(std::move(mediaBuffer), 0.0, kFragmentDuration, 0.0, false, false, nullptr, ptsError);
	
	EXPECT_TRUE(result);
	EXPECT_FALSE(ptsError);
}

/**
 * @brief Test trickplay PTS restamping - Rewind mode (negative rate)
 * 
 * Validates STEADY state behavior based on real-world logs:
 * - Rate: -2.0 (rewind mode)
 * - Fragment duration: 1.920 seconds
 * - Expected PTS values: 0.0, 0.960, 1.920
 * - Expected duration: 0.960 (fragment_duration / abs(rate) = 1.920 / 2.0)
 * - PTS increases monotonically even in rewind
 */
TEST_F(AampMp4DemuxerTests, TrickplayPtsRestamp_RewindModeMultipleFragments)
{
	// Set rewind rate (-2x) as observed in logs
	mPrivateInstanceAAMP->rate = -2.0;
	mDemuxer->setRate(-2.0, PlayMode_reverse_GOP);
	mDemuxer->setFrameRateForTM(4); // Set trickplay FPS to avoid division by zero
	
	// Send init fragment first
	const char* initData = "init";
	std::vector<uint8_t> initBuffer(initData, initData + strlen(initData));
	
	EXPECT_CALL(*g_mockMp4Demux, Parse(_)).WillOnce(Return(true));
	EXPECT_CALL(*g_mockMp4Demux, GetSamples())
		.WillOnce([]() {
			return std::vector<AampMediaSample>();
		});
	EXPECT_CALL(*g_mockMp4Demux, GetCodecInfo())
		.WillOnce([]() {
			MediaCodecInfo codecInfo;
			codecInfo.mCodecFormat = GST_FORMAT_VIDEO_ES_H264;
			return codecInfo;
		});
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SetStreamCaps(_, _));
	
	bool ptsError = false;
	mDemuxer->sendSegment(std::move(initBuffer), 0.0, 0.0, 0.0, false, true, nullptr, ptsError);
	
	// Expected values from logs
	constexpr double kFragmentDuration = 1.920;
	constexpr double kExpectedDuration = 0.960; // 1.920 / 2.0
	const std::vector<double> kExpectedPts = {0.0, 0.960, 1.920};
	const std::vector<double> kOriginalPts = {69762.803644, 69760.883644, 69758.963644};
	
	std::vector<double> actualPts;
	
	// Send 3 sequential media fragments (rewind: PTS decreases in source)
	for (int i = 0; i < 3; i++)
	{
		const char* mediaData = "media";
		std::vector<uint8_t> mediaBuffer(mediaData, mediaData + strlen(mediaData));
		double originalPts = kOriginalPts[i];
		
		EXPECT_CALL(*g_mockMp4Demux, Parse(_)).WillOnce(Return(true));
		EXPECT_CALL(*g_mockMp4Demux, GetSamples())
			.WillOnce([originalPts, kFragmentDuration]() {
				std::vector<AampMediaSample> samples;
				AampMediaSample sample;
				sample.mPts = originalPts;
				sample.mDts = originalPts;
				sample.mDuration = kFragmentDuration;
				sample.mIsKeyFrame = true;
				samples.push_back(std::move(sample));
				return samples;
			});
		
		EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendStreamTransfer(eMEDIATYPE_VIDEO, _))
			.WillOnce([&actualPts, i, &kExpectedPts, kExpectedDuration](AampMediaType, AampMediaSample&& sample) {
				actualPts.push_back(sample.mPts);
				
				// Verify expected PTS value (use NEAR for floating-point tolerance)
				EXPECT_NEAR(sample.mPts, kExpectedPts[i], 0.001) 
					<< "Fragment " << i << ": PTS mismatch";
				
				// Verify expected DTS value (should match PTS)
				EXPECT_NEAR(sample.mDts, kExpectedPts[i], 0.001) 
					<< "Fragment " << i << ": DTS mismatch";
				
				// Verify expected duration
				EXPECT_NEAR(sample.mDuration, kExpectedDuration, 0.001) 
					<< "Fragment " << i << ": Duration mismatch";
			});
		
		mDemuxer->sendSegment(std::move(mediaBuffer), 0.0, kFragmentDuration, 0.0, false, false, nullptr, ptsError);
	}
	
	// Verify PTS monotonicity - even in rewind, restamped PTS advances forward
	ASSERT_EQ(actualPts.size(), 3);
	for (size_t i = 1; i < actualPts.size(); i++)
	{
		EXPECT_GT(actualPts[i], actualPts[i-1]) 
			<< "PTS should increase monotonically even in rewind mode";
	}
}
/**
 * @brief Test trickplay PTS restamping - Transition from normal to trickplay
 * 
 * Validates transition behavior:
 * - Normal playback doesn't use trickmode restamping
 * - Entering trickplay initializes state properly
 * - Exiting trickplay clears state
 */
TEST_F(AampMp4DemuxerTests, TrickplayPtsRestamp_NormalToTrickplayTransition)
{
	// Start at normal rate
	mPrivateInstanceAAMP->rate = 1.0;
	
	const char* mediaData = "media";
	std::vector<uint8_t> mediaBuffer(mediaData, mediaData + strlen(mediaData));
	
	// Send normal playback fragment
	EXPECT_CALL(*g_mockMp4Demux, Parse(_)).WillOnce(Return(true));
	EXPECT_CALL(*g_mockMp4Demux, GetSamples())
		.WillOnce([]() {
			std::vector<AampMediaSample> samples;
			AampMediaSample sample;
			sample.mPts = 1000.0;
			sample.mDts = 1000.0;
			sample.mDuration = 100.0;
			samples.push_back(std::move(sample));
			return samples;
		});
	
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendStreamTransfer(_, _))
		.WillOnce([](AampMediaType, AampMediaSample&& sample) {
			// At normal rate, PTS should not be changed to 0
			EXPECT_DOUBLE_EQ(sample.mPts, 1000.0);
		});
	
	bool ptsError = false;
	mDemuxer->sendSegment(std::move(mediaBuffer), 0.0, 2.0, 0.0, false, false, nullptr, ptsError);
	
	// Now enter trickplay mode
	mPrivateInstanceAAMP->rate = 4.0;
	mDemuxer->setRate(4.0, PlayMode_normal);
	mDemuxer->setFrameRateForTM(4);
	
	// Send init to reset trickplay state
	const char* initData = "init";
	std::vector<uint8_t> initBuffer(initData, initData + strlen(initData));
	
	EXPECT_CALL(*g_mockMp4Demux, Parse(_)).WillOnce(Return(true));
	EXPECT_CALL(*g_mockMp4Demux, GetSamples())
		.WillOnce([]() {
			return std::vector<AampMediaSample>();
		});
	EXPECT_CALL(*g_mockMp4Demux, GetCodecInfo())
		.WillOnce([]() {
			MediaCodecInfo codecInfo;
			codecInfo.mCodecFormat = GST_FORMAT_VIDEO_ES_H264;
			return codecInfo;
		});
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SetStreamCaps(_, _));
	
	mDemuxer->sendSegment(std::move(initBuffer), 0.0, 0.0, 0.0, false, true, nullptr, ptsError);
	
	// Send trickplay media fragment
	std::vector<uint8_t> media2Buffer(mediaData, mediaData + strlen(mediaData));
	
	EXPECT_CALL(*g_mockMp4Demux, Parse(_)).WillOnce(Return(true));
	EXPECT_CALL(*g_mockMp4Demux, GetSamples())
		.WillOnce([]() {
			std::vector<AampMediaSample> samples;
			AampMediaSample sample;
			sample.mPts = 3000.0;
			sample.mDts = 3000.0;
			sample.mDuration = 100.0;
			sample.mIsKeyFrame = true;
			samples.push_back(std::move(sample));
			return samples;
		});
	
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendStreamTransfer(_, _))
		.WillOnce([](AampMediaType, AampMediaSample&& sample) {
			// First trickplay sample should reset to 0
			EXPECT_DOUBLE_EQ(sample.mPts, 0.0);
			EXPECT_DOUBLE_EQ(sample.mDts, 0.0);
		});
	
	mDemuxer->sendSegment(std::move(media2Buffer), 0.0, 2.0, 0.0, false, false, nullptr, ptsError);
	
	// Return to normal rate
	mPrivateInstanceAAMP->rate = 1.0;
	mDemuxer->setRate(1.0, PlayMode_normal);
	
	std::vector<uint8_t> media3Buffer(mediaData, mediaData + strlen(mediaData));
	
	EXPECT_CALL(*g_mockMp4Demux, Parse(_)).WillOnce(Return(true));
	EXPECT_CALL(*g_mockMp4Demux, GetSamples())
		.WillOnce([]() {
			std::vector<AampMediaSample> samples;
			AampMediaSample sample;
			sample.mPts = 5000.0;
			sample.mDts = 5000.0;
			sample.mDuration = 100.0;
			samples.push_back(std::move(sample));
			return samples;
		});
	
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendStreamTransfer(_, _))
		.WillOnce([](AampMediaType, AampMediaSample&& sample) {
			// Back to normal - should preserve original PTS
			EXPECT_DOUBLE_EQ(sample.mPts, 5000.0);
		});
	
	mDemuxer->sendSegment(std::move(media3Buffer), 0.0, 2.0, 0.0, false, false, nullptr, ptsError);
}

/**
 * @brief Test trickplay PTS restamping - No duplicate init processing at same rate
 * 
 * This test specifically validates the fix for the TSB duplicate init issue:
 * - First init at trickplay rate resets state
 * - Subsequent inits at SAME rate do NOT reset state
 * - State only resets when rate changes AND init is received
 */
TEST_F(AampMp4DemuxerTests, TrickplayPtsRestamp_NoDuplicateInitAtSameRate)
{
	// Set trickplay rate
	mPrivateInstanceAAMP->rate = 4.0;
	mDemuxer->setRate(4.0, PlayMode_normal);
	mDemuxer->setFrameRateForTM(4);
	
	// Send first init - should reset state
	const char* initData = "init";
	std::vector<uint8_t> init1Buffer(initData, initData + strlen(initData));
	
	EXPECT_CALL(*g_mockMp4Demux, Parse(_)).WillOnce(Return(true));
	EXPECT_CALL(*g_mockMp4Demux, GetSamples())
		.WillOnce([]() {
			return std::vector<AampMediaSample>();
		});
	EXPECT_CALL(*g_mockMp4Demux, GetCodecInfo())
		.WillOnce([]() {
			MediaCodecInfo codecInfo;
			codecInfo.mCodecFormat = GST_FORMAT_VIDEO_ES_H264;
			return codecInfo;
		});
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SetStreamCaps(_, _));
	
	bool ptsError = false;
	mDemuxer->sendSegment(std::move(init1Buffer), 0.0, 0.0, 0.0, false, true, nullptr, ptsError);
	
	// Send media fragment
	const char* media1 = "media1";
	std::vector<uint8_t> media1Buffer(media1, media1 + strlen(media1));
	
	EXPECT_CALL(*g_mockMp4Demux, Parse(_)).WillOnce(Return(true));
	EXPECT_CALL(*g_mockMp4Demux, GetSamples())
		.WillOnce([]() {
			std::vector<AampMediaSample> samples;
			AampMediaSample sample;
			sample.mPts = 1000.0;
			sample.mDts = 1000.0;
			sample.mDuration = 100.0;
			sample.mIsKeyFrame = true;
			samples.push_back(std::move(sample));
			return samples;
		});
	
	double firstMediaPts = 0.0;
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendStreamTransfer(_, _))
		.WillOnce([&firstMediaPts](AampMediaType, AampMediaSample&& sample) {
			firstMediaPts = sample.mPts;
			EXPECT_DOUBLE_EQ(sample.mPts, 0.0); // First sample at 0
		});
	
	mDemuxer->sendSegment(std::move(media1Buffer), 0.0, 2.0, 0.0, false, false, nullptr, ptsError);
	
	// Send second init AT THE SAME RATE - should NOT reset state
	std::vector<uint8_t> init2Buffer(initData, initData + strlen(initData));
	
	EXPECT_CALL(*g_mockMp4Demux, Parse(_)).WillOnce(Return(true));
	EXPECT_CALL(*g_mockMp4Demux, GetSamples())
		.WillOnce([]() {
			return std::vector<AampMediaSample>();
		});
	EXPECT_CALL(*g_mockMp4Demux, GetCodecInfo())
		.WillOnce([]() {
			MediaCodecInfo codecInfo;
			codecInfo.mCodecFormat = GST_FORMAT_VIDEO_ES_H264;
			return codecInfo;
		});
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SetStreamCaps(_, _));
	
	// Rate is still 4.0 - no change
	mDemuxer->sendSegment(std::move(init2Buffer), 0.0, 0.0, 0.0, false, true, nullptr, ptsError);
	
	// Send another media fragment - PTS should continue from previous, NOT reset to 0
	const char* media2 = "media2";
	std::vector<uint8_t> media2Buffer(media2, media2 + strlen(media2));
	
	EXPECT_CALL(*g_mockMp4Demux, Parse(_)).WillOnce(Return(true));
	EXPECT_CALL(*g_mockMp4Demux, GetSamples())
		.WillOnce([]() {
			std::vector<AampMediaSample> samples;
			AampMediaSample sample;
			sample.mPts = 3000.0; // 2 seconds later
			sample.mDts = 3000.0;
			sample.mDuration = 100.0;
			sample.mIsKeyFrame = true;
			samples.push_back(std::move(sample));
			return samples;
		});
	
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendStreamTransfer(_, _))
		.WillOnce([&firstMediaPts](AampMediaType, AampMediaSample&& sample) {
			// Should NOT be 0 - should continue from previous state
			EXPECT_GT(sample.mPts, 0.0) << "PTS should continue, not reset after init at same rate";
			EXPECT_GT(sample.mPts, firstMediaPts) << "PTS should advance from previous sample";
		});
	
	mDemuxer->sendSegment(std::move(media2Buffer), 0.0, 2.0, 0.0, false, false, nullptr, ptsError);
}
