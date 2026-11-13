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
 * @file AampRialtoVideoSourceTestFixture.h
 * @brief Fixtures and helpers shared by the AampRialtoVideoSource L1 test files.
 */

#ifndef AAMP_RIALTO_VIDEO_SOURCE_TEST_FIXTURE_H
#define AAMP_RIALTO_VIDEO_SOURCE_TEST_FIXTURE_H

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "AampRialtoVideoSource.h"
#include "MockIMediaPipeline.h"
#include "MockDrmBridge.h"
#include "MockMp4Demux.h"

#include <atomic>
#include <chrono>
#include <future>
#include <thread>

using ::testing::_;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

inline MediaCodecInfo MakeH264CodecInfo(
	uint16_t width = 1280, uint16_t height = 720,
	bool naluLengthPrefixed = false)
{
	MediaCodecInfo ci{};
	ci.mCodecFormat           = GST_FORMAT_VIDEO_ES_H264;
	ci.mInfo.video.mWidth     = width;
	ci.mInfo.video.mHeight    = height;
	ci.mCodecData             = {0x01, 0x02, 0x03};
	ci.mNaluLengthPrefixed    = naluLengthPrefixed;
	return ci;
}

inline MediaCodecInfo MakeHevcCodecInfo(
	uint16_t width = 1920, uint16_t height = 1080,
	bool naluLengthPrefixed = false)
{
	MediaCodecInfo ci{};
	ci.mCodecFormat           = GST_FORMAT_VIDEO_ES_HEVC;
	ci.mInfo.video.mWidth     = width;
	ci.mInfo.video.mHeight    = height;
	ci.mCodecData             = {0x01, 0x02};
	ci.mNaluLengthPrefixed    = naluLengthPrefixed;
	return ci;
}

/**
 * @class TestableAampRialtoVideoSourceNullSegment
 * @brief Subclass allowing createSegment() to be forced to fail, for
 *        exercising injectOneSample()'s createSegment()==nullptr path.
 */
class TestableAampRialtoVideoSourceNullSegment : public AampRialtoVideoSource
{
public:
	bool mReturnNullSegment{false};

	std::unique_ptr<firebolt::rialto::IMediaPipeline::MediaSegment>
	createSegment(const AampMediaSample &sample) const override
	{
		if (mReturnNullSegment)
		{
			return nullptr;
		}
		return AampRialtoVideoSource::createSegment(sample);
	}
};




// ---------------------------------------------------------------------------
// Fixture
// ---------------------------------------------------------------------------

/**
 * @class AampRialtoVideoSourceTest
 * @brief Fixture for AampRialtoVideoSource unit tests.
 */
class AampRialtoVideoSourceTest : public ::testing::Test
{
protected:
	void SetUp() override;

	AampRialtoVideoSource m_source;
	std::unique_ptr<MockIMediaPipeline> m_mockPipeline;
	MockIMediaPipeline *m_pipelinePtr{nullptr};
	int32_t m_nextSourceId{10};
};

/// An encrypted sample whose sub-sample table holds @p tableBytes bytes while
/// it declares @p numSubSamples entries (6 bytes each).
inline AampMediaSample MakeEncryptedSample(uint16_t numSubSamples, size_t tableBytes)
{
	static const uint8_t kData[] = {0x01, 0x02, 0x03};
	AampMediaSample sample;
	sample.mData = std::shared_ptr<const uint8_t>(kData, [](const uint8_t *){});
	sample.mDataSize = sizeof(kData);
	sample.mPts = 1.0;
	sample.mDuration = 0.033;
	sample.mDrmMetadata.mIsEncrypted = true;
	sample.mDrmMetadata.mKeyId.assign(16, 0xAA);
	sample.mDrmMetadata.mIV.assign(16, 0xBB);
	sample.mDrmMetadata.mCipher = CIPHER_TYPE_CENC;
	sample.mDrmMetadata.mNumSubSamples = numSubSamples;
	sample.mDrmMetadata.mSubSamples.assign(tableBytes, 0x00);
	return sample;
}

// ===========================================================================
// Fixture: AampRialtoVideoSourceWithDemuxTest
// Extends AampRialtoVideoSourceTest with a fake Mp4Demux backed by
// g_mockMp4Demux so that processInitFragment / processDataFragment /
// injectSingleSample can be exercised.
// ===========================================================================

/**
 * @class AampRialtoVideoSourceWithDemuxTest
 * @brief Fixture for processInitFragment / processDataFragment /
 *        injectSingleSample unit tests.
 */
class AampRialtoVideoSourceWithDemuxTest : public AampRialtoVideoSourceTest
{
protected:
	void SetUp() override;

	void TearDown() override;
};

#endif /* AAMP_RIALTO_VIDEO_SOURCE_TEST_FIXTURE_H */
